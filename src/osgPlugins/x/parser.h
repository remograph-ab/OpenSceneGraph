// -*-c++-*-

/*
 * Loader for DirectX .x files.
 *
 * Generic .x file parser: header, MSZIP decompression ('tzip'/'bzip'),
 * text ('txt ') and binary ('bin ') tokenizers, and a format independent
 * parser that turns the token stream into a tree of data objects.
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

#ifndef _DX_PARSER_H_
#define _DX_PARSER_H_

#include <string>
#include <vector>

namespace DX {

    /// Case-insensitive string comparison.
    bool equalsNoCase(const std::string& a, const char* b);

    /**
     * Generic data object (an instance of a template) in a .x file.
     *
     * All numeric values of the object (integers and floats, in file order)
     * are collected in 'numbers', all strings in 'strings'. Nested data
     * objects and references to other data objects ("{ name }") are stored,
     * in file order, in 'children'.
     */
    class DataObject {
    public:
        DataObject() : isReference(false) {}
        ~DataObject();

        /// Template name, e.g. "Mesh" (empty for references).
        std::string type;

        /// Optional instance name, or the referenced name if isReference.
        std::string name;

        /// True if this is a reference to another data object.
        bool isReference;

        std::vector<double> numbers;
        std::vector<std::string> strings;
        std::vector<DataObject*> children;

        bool isType(const char* t) const { return !isReference && equalsNoCase(type, t); }

    private:
        DataObject(const DataObject&);
        DataObject& operator=(const DataObject&);
    };

    /**
     * Parse a complete .x file (including the 'xof ' header) into a list of
     * top-level data objects. Templates are skipped.
     * The caller takes ownership of the returned objects.
     * @return false on fatal error (message in 'error'); 'warning' may be set
     * even on success if the file was only partially readable.
     */
    bool parseXFile(const std::string& data, std::vector<DataObject*>& objects,
                    std::string& error, std::string& warning);

} // namespace

#endif
