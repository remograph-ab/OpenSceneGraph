// -*-c++-*-

/*
 * Loader for DirectX .x files.
 *
 * Generic .x file parser, see parser.h.
 *
 * File layout:
 *   16 byte header "xof MMmmffffssss" where MMmm is the version, ffff the
 *   format ("txt ", "bin ", "tzip" or "bzip") and ssss the float size
 *   ("0032" or "0064").
 *
 * Compressed formats ("tzip"/"bzip") use MSZIP after the header:
 *   DWORD total uncompressed size (including the 16 byte header), then
 *   chunks of: WORD uncompressed size, WORD compressed size (including the
 *   following signature), "CK", raw deflate data. Each chunk may refer back
 *   into the previous chunk's output (deflate dictionary).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#include "parser.h"

#include <osg/Math>

#include <string.h>
#include <ctype.h>
#include <sstream>

#ifdef USE_ZLIB
#include <zlib.h>
#endif

namespace DX {

bool equalsNoCase(const std::string& a, const char* b)
{
    size_t n = strlen(b);
    if (a.size() != n)
        return false;
    for (size_t i = 0; i < n; ++i) {
        if (tolower((unsigned char) a[i]) != tolower((unsigned char) b[i]))
            return false;
    }
    return true;
}

DataObject::~DataObject()
{
    for (size_t i = 0; i < children.size(); ++i)
        delete children[i];
}

namespace {

    /**********************************************************************
     *
     * Tokens
     *
     **********************************************************************/

    struct Token {
        enum Type {
            END,        // end of input
            ERROR_TOKEN,// unreadable input
            NAME,       // identifier
            STRING,     // quoted string
            NUMBER,     // integer or float
            GUID,       // <...>
            OBRACE,     // {
            CBRACE,     // }
            TEMPLATE,   // keyword 'template'
            OTHER       // anything only relevant inside templates
        };

        Type type;
        std::string text;
        double value;

        Token() : type(END), value(0.0) {}
    };

    class Lexer {
    public:
        virtual ~Lexer() {}
        virtual void next(Token& tok) = 0;
        virtual std::string position() const = 0;
    };

    /**********************************************************************
     *
     * Text tokenizer
     *
     **********************************************************************/

    class TextLexer : public Lexer {
    public:
        TextLexer(const char* begin, const char* end) : _p(begin), _end(end), _line(1) {}

        virtual std::string position() const
        {
            std::ostringstream ostr;
            ostr << "line " << _line;
            return ostr.str();
        }

        virtual void next(Token& tok)
        {
            tok.text.clear();
            tok.value = 0.0;

            for (;;) {
                // Skip white space and separators
                while (_p < _end && (isspace((unsigned char) *_p) || *_p == ',' || *_p == ';')) {
                    if (*_p == '\n')
                        ++_line;
                    ++_p;
                }
                if (_p >= _end) {
                    tok.type = Token::END;
                    return;
                }

                // Comments: '//' or '#' to end of line
                if (*_p == '#' || (*_p == '/' && _p + 1 < _end && _p[1] == '/')) {
                    while (_p < _end && *_p != '\n')
                        ++_p;
                    continue;
                }
                break;
            }

            char c = *_p;
            if (c == '{') {
                ++_p;
                tok.type = Token::OBRACE;
                return;
            }
            if (c == '}') {
                ++_p;
                tok.type = Token::CBRACE;
                return;
            }
            if (c == '"') {
                // String: everything up to the closing quote
                const char* start = ++_p;
                while (_p < _end && *_p != '"') {
                    if (*_p == '\n')
                        ++_line;
                    ++_p;
                }
                tok.text.assign(start, _p - start);
                if (_p < _end)
                    ++_p;
                tok.type = Token::STRING;
                return;
            }
            if (c == '<') {
                // GUID
                const char* start = ++_p;
                while (_p < _end && *_p != '>' && *_p != '\n')
                    ++_p;
                tok.text.assign(start, _p - start);
                if (_p < _end && *_p == '>')
                    ++_p;
                tok.type = Token::GUID;
                return;
            }
            if (c == '[' || c == ']' || c == '(' || c == ')' || c == '>') {
                ++_p;
                tok.type = Token::OTHER;
                return;
            }

            // Word: a name or a number
            const char* start = _p;
            while (_p < _end) {
                char d = *_p;
                if (isspace((unsigned char) d) || d == ',' || d == ';' || d == '{' || d == '}' ||
                    d == '"' || d == '<' || d == '>' || d == '[' || d == ']' || d == '(' || d == ')')
                    break;
                if (d == '/' && _p + 1 < _end && _p[1] == '/')
                    break;
                ++_p;
            }
            tok.text.assign(start, _p - start);

            if (isNumber(tok.text, tok.value))
                tok.type = Token::NUMBER;
            else if (tok.text == "template")
                tok.type = Token::TEMPLATE;
            else
                tok.type = Token::NAME;
        }

    private:
        const char* _p;
        const char* _end;
        unsigned int _line;

        /// Does the word look like a number? Values such as "1.#QNAN0" count as numbers (0).
        static bool isNumber(const std::string& s, double& value)
        {
            if (s.empty())
                return false;
            char c = s[0];
            if (!(isdigit((unsigned char) c) || c == '-' || c == '+' || c == '.'))
                return false;

            size_t i = 0;
            bool digits = false;
            while (i < s.size()) {
                char d = s[i];
                if (isdigit((unsigned char) d))
                    digits = true;
                else if (!(d == '-' || d == '+' || d == '.' || d == 'e' || d == 'E'))
                    break;
                ++i;
            }
            if (!digits)
                return false;
            if (i < s.size()) {
                // Trailing junk, e.g. "1.#INF" - accept as number only for MS special values
                if (s.find('#') == std::string::npos)
                    return false;
                value = 0.0;
                return true;
            }
            value = osg::asciiToDouble(s.c_str());
            return true;
        }
    };

    /**********************************************************************
     *
     * Binary tokenizer
     *
     **********************************************************************/

    class BinaryLexer : public Lexer {
    public:
        BinaryLexer(const unsigned char* begin, const unsigned char* end, bool doubles)
            : _begin(begin), _p(begin), _end(end), _doubles(doubles), _listRemaining(0), _listIsFloat(false) {}

        virtual std::string position() const
        {
            std::ostringstream ostr;
            ostr << "byte offset " << (16 + (_p - _begin)) << " (uncompressed)";
            return ostr.str();
        }

        virtual void next(Token& tok)
        {
            tok.text.clear();
            tok.value = 0.0;

            if (_listRemaining > 0) {
                readListValue(tok);
                return;
            }

            for (;;) {
                if (_end - _p < 2) {
                    tok.type = Token::END;
                    return;
                }
                unsigned int code = readWord();

                switch (code) {
                    case 1: // TOKEN_NAME: DWORD count, chars
                    case 2: // TOKEN_STRING: DWORD count, chars (separator token follows)
                    {
                        unsigned int n;
                        if (!readDWord(n) || (size_t)(_end - _p) < n) {
                            tok.type = Token::ERROR_TOKEN;
                            return;
                        }
                        tok.text.assign((const char*) _p, n);
                        _p += n;
                        // Strip trailing NUL characters some exporters include
                        while (!tok.text.empty() && tok.text[tok.text.size() - 1] == '\0')
                            tok.text.erase(tok.text.size() - 1);
                        tok.type = (code == 1 ? Token::NAME : Token::STRING);
                        return;
                    }
                    case 3: // TOKEN_INTEGER: DWORD
                    {
                        unsigned int v;
                        if (!readDWord(v)) {
                            tok.type = Token::ERROR_TOKEN;
                            return;
                        }
                        tok.value = (double) v;
                        tok.type = Token::NUMBER;
                        return;
                    }
                    case 5: // TOKEN_GUID: 16 bytes
                        if (_end - _p < 16) {
                            tok.type = Token::ERROR_TOKEN;
                            return;
                        }
                        _p += 16;
                        tok.type = Token::GUID;
                        return;
                    case 6: // TOKEN_INTEGER_LIST: DWORD count, DWORD[count]
                    case 7: // TOKEN_FLOAT_LIST: DWORD count, float/double[count]
                    {
                        unsigned int n;
                        if (!readDWord(n)) {
                            tok.type = Token::ERROR_TOKEN;
                            return;
                        }
                        _listIsFloat = (code == 7);
                        size_t elemSize = (_listIsFloat && _doubles) ? 8 : 4;
                        if ((size_t)(_end - _p) / elemSize < n) {
                            tok.type = Token::ERROR_TOKEN;
                            return;
                        }
                        if (n == 0)
                            continue;
                        _listRemaining = n;
                        readListValue(tok);
                        return;
                    }
                    case 10: // TOKEN_OBRACE
                        tok.type = Token::OBRACE;
                        return;
                    case 11: // TOKEN_CBRACE
                        tok.type = Token::CBRACE;
                        return;
                    case 0:  // padding (tolerated)
                    case 19: // TOKEN_COMMA
                    case 20: // TOKEN_SEMICOLON
                        continue;
                    case 31: // TOKEN_TEMPLATE
                        tok.type = Token::TEMPLATE;
                        return;
                    default:
                        if ((code >= 12 && code <= 18) || (code >= 40 && code <= 52)) {
                            // ( ) [ ] < > . and primitive type keywords: only used in templates
                            tok.type = Token::OTHER;
                            return;
                        }
                        _p -= 2;
                        tok.type = Token::ERROR_TOKEN;
                        return;
                }
            }
        }

    private:
        const unsigned char* _begin;
        const unsigned char* _p;
        const unsigned char* _end;
        bool _doubles;
        unsigned int _listRemaining;
        bool _listIsFloat;

        unsigned int readWord()
        {
            unsigned int v = _p[0] | (_p[1] << 8);
            _p += 2;
            return v;
        }

        bool readDWord(unsigned int& v)
        {
            if (_end - _p < 4)
                return false;
            v = (unsigned int) _p[0] | ((unsigned int) _p[1] << 8) |
                ((unsigned int) _p[2] << 16) | ((unsigned int) _p[3] << 24);
            _p += 4;
            return true;
        }

        void readListValue(Token& tok)
        {
            // Bounds were checked when the list was started
            if (_listIsFloat) {
                if (_doubles) {
                    double d;
                    memcpy(&d, _p, 8);
                    _p += 8;
                    tok.value = d;
                }
                else {
                    float f;
                    memcpy(&f, _p, 4);
                    _p += 4;
                    tok.value = f;
                }
            }
            else {
                unsigned int v;
                readDWord(v);
                tok.value = (double) v;
            }
            --_listRemaining;
            tok.type = Token::NUMBER;
        }
    };

    /**********************************************************************
     *
     * Format independent parser
     *
     **********************************************************************/

    class Parser {
    public:
        Parser(Lexer& lexer) : _lexer(lexer), _depth(0) {}

        /// Parse all top-level objects. Returns false if input was malformed (objects read so far are kept).
        bool parseFile(std::vector<DataObject*>& objects)
        {
            for (;;) {
                Token tok;
                next(tok);
                switch (tok.type) {
                    case Token::END:
                        return true;
                    case Token::ERROR_TOKEN:
                        return fail("unreadable data");
                    case Token::TEMPLATE:
                        if (!skipTemplate())
                            return false;
                        break;
                    case Token::NAME:
                    {
                        DataObject* obj = new DataObject;
                        obj->type = tok.text;
                        int r = parseObject(*obj);
                        if (r == 0) {
                            delete obj; // stray identifier, ignored
                            break;
                        }
                        objects.push_back(obj);
                        if (r < 0)
                            return false;
                        break;
                    }
                    case Token::OBRACE:
                    {
                        // Top-level reference or anonymous block: ignore
                        DataObject dummy;
                        if (!parseBody(dummy))
                            return false;
                        break;
                    }
                    default:
                        // Stray token at top level: ignore
                        break;
                }
            }
        }

        const std::string& error() const { return _error; }

    private:
        Lexer& _lexer;
        unsigned int _depth;
        std::string _error;
        std::vector<Token> _pushback;

        void next(Token& tok)
        {
            if (!_pushback.empty()) {
                tok = _pushback.back();
                _pushback.pop_back();
            }
            else
                _lexer.next(tok);
        }

        void pushBack(const Token& tok) { _pushback.push_back(tok); }

        bool fail(const std::string& msg)
        {
            if (_error.empty())
                _error = msg + " at " + _lexer.position();
            return false;
        }

        /// Skip 'template' declaration (keyword already read).
        bool skipTemplate()
        {
            Token tok;
            // Name, then '{'
            do {
                next(tok);
                if (tok.type == Token::END || tok.type == Token::ERROR_TOKEN)
                    return fail("unexpected end of template");
            } while (tok.type != Token::OBRACE);

            int depth = 1;
            while (depth > 0) {
                next(tok);
                if (tok.type == Token::END || tok.type == Token::ERROR_TOKEN)
                    return fail("unexpected end of template");
                if (tok.type == Token::OBRACE)
                    ++depth;
                else if (tok.type == Token::CBRACE)
                    --depth;
            }
            return true;
        }

        /**
         * Parse data object header "[name] [GUID] {" and body; the template
         * name has already been read into obj.type.
         * @return 1 if an object was parsed, 0 if the template name turned out
         * not to start an object (following tokens are pushed back), -1 on error.
         */
        int parseObject(DataObject& obj)
        {
            Token t1;
            next(t1);
            if (t1.type == Token::OBRACE)
                return parseBody(obj) ? 1 : -1;

            if (t1.type == Token::GUID) {
                Token t2;
                next(t2);
                if (t2.type == Token::OBRACE)
                    return parseBody(obj) ? 1 : -1;
                pushBack(t2);
                return 0;
            }

            if (t1.type == Token::NAME || t1.type == Token::NUMBER) {
                Token t2;
                next(t2);
                if (t2.type == Token::OBRACE) {
                    obj.name = t1.text;
                    return parseBody(obj) ? 1 : -1;
                }
                if (t2.type == Token::GUID) {
                    Token t3;
                    next(t3);
                    if (t3.type == Token::OBRACE) {
                        obj.name = t1.text;
                        return parseBody(obj) ? 1 : -1;
                    }
                    pushBack(t3);
                }
                pushBack(t2);
            }
            pushBack(t1);
            return 0;
        }

        /// Parse data object body up to and including the closing '}'.
        bool parseBody(DataObject& obj)
        {
            if (++_depth > 1000) {
                --_depth;
                return fail("nesting too deep");
            }

            bool result = true;
            Token tok;
            for (;;) {
                next(tok);
                if (tok.type == Token::CBRACE)
                    break;

                if (tok.type == Token::END) {
                    result = fail("unexpected end of file in '" + obj.type + "'");
                    break;
                }
                if (tok.type == Token::ERROR_TOKEN) {
                    result = fail("unreadable data in '" + obj.type + "'");
                    break;
                }

                if (tok.type == Token::NUMBER) {
                    obj.numbers.push_back(tok.value);
                }
                else if (tok.type == Token::STRING) {
                    obj.strings.push_back(tok.text);
                }
                else if (tok.type == Token::NAME) {
                    // Nested data object (or an unquoted identifier value, which is ignored)
                    DataObject* child = new DataObject;
                    child->type = tok.text;
                    int r = parseObject(*child);
                    if (r == 0) {
                        delete child;
                        continue;
                    }
                    obj.children.push_back(child);
                    if (r < 0) {
                        result = false;
                        break;
                    }
                }
                else if (tok.type == Token::OBRACE) {
                    // Reference: '{' [name] [GUID] '}'
                    if (!parseReference(obj)) {
                        result = false;
                        break;
                    }
                }
                else if (tok.type == Token::TEMPLATE) {
                    if (!skipTemplate()) {
                        result = false;
                        break;
                    }
                }
                // GUID and OTHER tokens inside data are ignored
            }

            --_depth;
            return result;
        }

        bool parseReference(DataObject& parent)
        {
            std::string name;
            Token tok;
            for (;;) {
                next(tok);
                if (tok.type == Token::CBRACE)
                    break;
                if (tok.type == Token::END || tok.type == Token::ERROR_TOKEN)
                    return fail("unexpected end of reference");
                if ((tok.type == Token::NAME || tok.type == Token::NUMBER || tok.type == Token::STRING) && name.empty())
                    name = tok.text;
                else if (tok.type == Token::OBRACE)
                    return fail("unexpected '{' in reference");
            }
            if (!name.empty()) {
                DataObject* ref = new DataObject;
                ref->isReference = true;
                ref->name = name;
                parent.children.push_back(ref);
            }
            return true;
        }
    };

#ifdef USE_ZLIB
    /// Decompress MSZIP data following the header.
    bool decompressMSZip(const std::string& data, std::string& out, std::string& error)
    {
        const unsigned char* p = (const unsigned char*) data.data() + 16;
        const unsigned char* end = (const unsigned char*) data.data() + data.size();

        if (end - p < 4) {
            error = "truncated compressed file";
            return false;
        }
        unsigned int total = p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int) p[3] << 24);
        p += 4;
        if (total > 16 && total - 16 < 512u * 1024u * 1024u)
            out.reserve(total - 16);

        z_stream stream;
        memset(&stream, 0, sizeof(stream));
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            error = "failed to initialize zlib";
            return false;
        }

        const size_t windowSize = 32768;
        bool ok = true;
        while (end - p >= 4) {
            unsigned int uncompSize = p[0] | (p[1] << 8);
            unsigned int compSize = p[2] | (p[3] << 8);
            p += 4;

            if (compSize < 2 || (size_t)(end - p) < compSize || p[0] != 'C' || p[1] != 'K') {
                error = "corrupt MSZIP block";
                ok = false;
                break;
            }

            size_t start = out.size();
            out.resize(start + uncompSize);

            stream.next_in = (Bytef*) (p + 2);
            stream.avail_in = compSize - 2;
            stream.next_out = (Bytef*) &out[start];
            stream.avail_out = uncompSize;

            int ret = inflate(&stream, Z_FINISH);
            if (ret != Z_STREAM_END && ret != Z_OK && ret != Z_BUF_ERROR) {
                error = std::string("decompression failed: ") + (stream.msg ? stream.msg : "zlib error");
                ok = false;
                break;
            }
            out.resize(start + (uncompSize - stream.avail_out));
            p += compSize;

            // Next block may refer back into the data decompressed so far
            inflateReset(&stream);
            size_t dictSize = out.size() < windowSize ? out.size() : windowSize;
            if (dictSize > 0)
                inflateSetDictionary(&stream, (const Bytef*) out.data() + out.size() - dictSize, (uInt) dictSize);
        }

        inflateEnd(&stream);
        return ok;
    }
#endif

} // anonymous namespace


bool parseXFile(const std::string& data, std::vector<DataObject*>& objects,
                std::string& error, std::string& warning)
{
    // Header
    if (data.size() < 16 || data.compare(0, 4, "xof ") != 0) {
        error = "not a DirectX .x file (missing 'xof ' header)";
        return false;
    }

    std::string format = data.substr(8, 4);
    for (size_t i = 0; i < format.size(); ++i)
        format[i] = (char) tolower((unsigned char) format[i]);
    std::string floatSize = data.substr(12, 4);

    bool binary, compressed;
    if (format == "txt ") {
        binary = false;
        compressed = false;
    }
    else if (format == "bin ") {
        binary = true;
        compressed = false;
    }
    else if (format == "tzip") {
        binary = false;
        compressed = true;
    }
    else if (format == "bzip") {
        binary = true;
        compressed = true;
    }
    else {
        error = "unsupported .x format '" + format + "'";
        return false;
    }

    bool doubles = (floatSize == "0064");
    if (binary && !doubles && floatSize != "0032") {
        error = "unsupported .x float size '" + floatSize + "'";
        return false;
    }

    // Body, decompressed if needed
    std::string decompressed;
    const char* bodyBegin = data.data() + 16;
    const char* bodyEnd = data.data() + data.size();

    if (compressed) {
#ifdef USE_ZLIB
        std::string zipError;
        if (!decompressMSZip(data, decompressed, zipError)) {
            if (decompressed.empty()) {
                error = zipError;
                return false;
            }
            warning = zipError;
        }
        bodyBegin = decompressed.data();
        bodyEnd = decompressed.data() + decompressed.size();
#else
        error = "compressed .x files are not supported (plugin built without zlib)";
        return false;
#endif
    }

    // Tokenize and parse
    TextLexer textLexer(bodyBegin, bodyEnd);
    BinaryLexer binaryLexer((const unsigned char*) bodyBegin, (const unsigned char*) bodyEnd, doubles);
    Lexer& lexer = binary ? (Lexer&) binaryLexer : (Lexer&) textLexer;

    Parser parser(lexer);
    if (!parser.parseFile(objects)) {
        if (objects.empty()) {
            error = parser.error();
            return false;
        }
        // Keep what could be read
        if (!warning.empty())
            warning += "; ";
        warning += parser.error();
    }

    return true;
}

} // namespace DX
