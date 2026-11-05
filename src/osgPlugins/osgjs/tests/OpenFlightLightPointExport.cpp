/* OpenSceneGraph - see LICENSE.txt for license details. */
// Binary layouts and actual-size units: OGC OpenFlight 19-065, records 111/128/129/130/131.
#include <osg/NodeVisitor>
#include <osgSim/LightPointNode>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/Registry>
#include <osgDB/Options>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace
{
struct Record
{
    std::vector<unsigned char> bytes;
    void integer(unsigned int value, unsigned int size = 4)
    {
        for (unsigned int i = size; i; --i) bytes.push_back((value >> ((i-1)*8)) & 255);
    }
    void real(float value)
    {
        unsigned int bits;
        std::memcpy(&bits, &value, 4);
        integer(bits);
    }
    void real64(double value)
    {
        unsigned long long bits;
        std::memcpy(&bits, &value, 8);
        integer(static_cast<unsigned int>(bits >> 32));
        integer(static_cast<unsigned int>(bits));
    }
    void text(const std::string& value, unsigned int size)
    {
        for (unsigned int i = 0; i != size; ++i) bytes.push_back(i < value.size() ? value[i] : 0);
    }
    void write(std::ostream& out, unsigned int opcode)
    {
        Record header;
        header.integer(opcode, 2);
        header.integer(static_cast<unsigned int>(bytes.size())+4, 2);
        out.write(reinterpret_cast<const char*>(&header.bytes[0]), 4);
        if (!bytes.empty()) out.write(reinterpret_cast<const char*>(&bytes[0]), bytes.size());
    }
};

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void attributes(Record& r)
{
    r.integer(0); // Display mode.
    r.real(1); r.real(0.5f);
    r.real(0); r.real(0);
    for (unsigned int i = 0; i != 4; ++i) r.integer(0);
    r.real(2); r.real(64); r.real(6);
    for (unsigned int i = 0; i != 7; ++i) r.real(0);
    r.integer(2); // Bidirectional.
    r.real(90); r.real(60); r.real(15);
    r.real(0); r.real(0);
}

void vertex(std::ostream& out)
{
    Record r;
    r.integer(0, 2); r.integer(0x1000, 2); // Packed vertex color.
    r.real64(10); r.real64(20); r.real64(30);
    r.real(0); r.real(1); r.real(0);
    r.integer(0xff004080); // ABGR: red=128, green=64, blue=0.
    r.integer(0);
    r.write(out, 69);
}

void inlineLight(std::ostream& out, const char* name, float period, float enabled, bool flashing)
{
    Record r;
    r.text(name, 8);
    r.integer(0, 2); r.integer(0, 2); r.integer(0);
    attributes(r);
    r.real(period); r.real(0.25f); r.real(enabled);
    r.real(0); r.integer(0); r.integer(flashing ? 0x00400000 : 0);
    r.real(0); r.real(0); r.real(1);
    r.write(out, 111);
    Record().write(out, 10); vertex(out); Record().write(out, 11);
}

void appearance(std::ostream& out)
{
    Record r;
    r.integer(0); r.text("Appearance", 256); r.integer(1);
    r.integer(0, 2); r.integer(0, 2); r.integer(0);
    attributes(r);
    r.real(0); r.integer(0);
    for (unsigned int i = 0; i != 6; ++i) r.real(0);
    r.integer(0xffff, 2); r.integer(0, 2);
    r.write(out, 128);
}

void animation(std::ostream& out, unsigned int index, int type, float period, float enabled, bool colors)
{
    Record r;
    r.integer(0); r.text("Animation", 256); r.integer(index);
    r.real(period); r.real(0.25f); r.real(enabled);
    r.real(0); r.real(0); r.real(1);
    r.integer(0x80000000); r.integer(type);
    r.integer(0); r.integer(0); r.integer(0); r.text("", 1024);
    r.integer(colors ? 5 : 0);
    if (colors)
    {
        r.integer(0); r.real(0.5f); r.integer(0xffffffff); // ON.
        r.integer(1); r.real(0.25f); r.integer(0); // OFF.
        r.integer(2); r.real(0.75f); r.integer(0xff00ff00); // Green replaces red.
        r.integer(0); r.real(0); r.integer(0);
        r.integer(0); r.real(-1); r.integer(0);
    }
    r.write(out, 129);
}

void indexed(std::ostream& out, const char* name, int animationIndex)
{
    Record r;
    r.text(name, 8); r.integer(1); r.integer(animationIndex); r.integer(0);
    r.write(out, 130);
    Record().write(out, 10); vertex(out); Record().write(out, 11);
}

void generateFlight()
{
    std::ofstream out("lightpoints-openflight.flt", std::ios::binary);
    Record header;
    header.text("LPTest", 8); header.integer(1600); header.integer(0); header.text("", 32);
    header.integer(0); header.integer(0); header.integer(1, 2);
    header.integer(4, 1); header.integer(0, 1); // Feet, converted to meters.
    header.bytes.resize(320, 0);
    header.write(out, 1);
    appearance(out);
    animation(out, 1, 2, 2, 0.5f, false);
    animation(out, 2, 0, 0, 0, true);
    animation(out, 3, 0, 0, 0, false);
    animation(out, 4, 2, 0, 0, false);
    animation(out, 5, 2, 1, 3, false);
    animation(out, 6, 2, 1, -1, false);
    Record().write(out, 10);
    Record system;
    system.text("System", 8); system.real(0.75f); system.integer(1); system.integer(0x80000000);
    system.write(out, 131);
    Record matrix;
    for (unsigned int i = 0; i != 16; ++i) matrix.real(i == 12 ? 10.0f : (i % 5 == 0 ? 1.0f : 0.0f));
    matrix.write(out, 49);
    Record().write(out, 10);
    Record group;
    group.text("Nested", 8); group.bytes.resize(40, 0); group.write(out, 2);
    Record().write(out, 10);
    inlineLight(out, "Inline", 2, 0.5f, true);
    inlineLight(out, "Steady", 0, 0, false);
    inlineLight(out, "BadTime", -1, 3, true);
    indexed(out, "Strobe", 1);
    indexed(out, "Colors", 2);
    indexed(out, "Empty", 3);
    indexed(out, "ZeroTime", 4);
    indexed(out, "AlwaysOn", 5);
    indexed(out, "Off", 6);
    indexed(out, "NoAnim", -1);
    Record().write(out, 11);
    Record().write(out, 11);
    Record().write(out, 11);
    check(out.good(), "Writing FLT fixture");
}

struct Collect : osg::NodeVisitor
{
    Collect(): osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
    std::map<std::string, osgSim::LightPointNode*> nodes;
    virtual void apply(osg::Node& node)
    {
        osgSim::LightPointNode* lights = dynamic_cast<osgSim::LightPointNode*>(&node);
        if (lights) nodes[node.getName()] = lights;
        traverse(node);
    }
};

void validate(osg::Node& root, double unitScale)
{
    Collect collect;
    root.accept(collect);
    check(collect.nodes.size() == 10, "All FLT light nodes");
    osgSim::LightPointSystem* system = NULL;
    for (std::map<std::string, osgSim::LightPointNode*>::iterator i = collect.nodes.begin(); i != collect.nodes.end(); ++i)
    {
        osgSim::LightPointNode* lights = i->second;
        check(lights->getNumLightPoints() == 2, "Bidirectional light count");
        check(lights->getMinPixelSize() == 2 && lights->getMaxPixelSize() == 64, "Pixel limits unchanged");
        check(lights->getLightPointSystem() != NULL, "System inherited through nested group");
        if (!system) system = lights->getLightPointSystem();
        check(system == lights->getLightPointSystem(), "Shared inherited system");
        const osgSim::LightPoint& front = lights->getLightPoint(0);
        const osgSim::LightPoint& back = lights->getLightPoint(1);
        check(std::abs(front._position.x()-10*unitScale) < 0.0001f, "Vertex unit conversion");
        check(std::abs(front._radius-3*unitScale) < 0.0001f, "Database-unit diameter converted to model radius");
        check(front._intensity == 1 && back._intensity == 0.5f, "Front/back intensity");
        check(front._sector.valid() && back._sector.valid(), "Directional sectors");
        check((*front._sector)(osg::Vec3(0,1,0)) > 0.99f &&
              (*front._sector)(osg::Vec3(0,-1,0)) == 0 &&
              (*back._sector)(osg::Vec3(0,-1,0)) > 0.99f, "Front/back directions");
        if (front._blinkSequence.valid())
        {
            check(front._blinkSequence->getPulsePeriod() > 0, "Positive blink period");
            for (unsigned int p = 0; p != front._blinkSequence->getNumPulses(); ++p)
            {
                double duration; osg::Vec4 color;
                front._blinkSequence->getPulse(p, duration, color);
                check(duration > 0 && std::isfinite(duration), "Safe pulse duration");
            }
        }
    }
    check(system->getIntensity() == 0.75f &&
          system->getAnimationState() == osgSim::LightPointSystem::ANIMATION_ON, "System settings");
    const char* steady[] = {"Steady", "BadTime", "Empty", "ZeroTime", "NoAnim"};
    for (unsigned int i = 0; i != 5; ++i)
        check(!collect.nodes[steady[i]]->getLightPoint(0)._blinkSequence.valid(), "Steady fallback");
    const char* flashing[] = {"Inline", "Strobe", "AlwaysOn"};
    for (unsigned int i = 0; i != 3; ++i)
    {
        const osgSim::LightPoint& point = collect.nodes[flashing[i]]->getLightPoint(0);
        double duration; osg::Vec4 color;
        point._blinkSequence->getPulse(point._blinkSequence->getNumPulses()-1, duration, color);
        check(color == osg::Vec4(1,1,1,1), "ON uses neutral RGBA multiplier");
    }
    osgSim::LightPointNode* colors = collect.nodes["Colors"];
    check(colors->getLightPoint(0)._color == osg::Vec4(1,1,1,1), "Color-change neutral base");
    double duration; osg::Vec4 color;
    colors->getLightPoint(0)._blinkSequence->getPulse(0, duration, color);
    check(std::abs(color.r()-128.0f/255) < 0.0001f && color.b() == 0, "ON restores original front color");
    colors->getLightPoint(1)._blinkSequence->getPulse(0, duration, color);
    check(color == osg::Vec4(1,1,1,1), "ON restores original back color");
    colors->getLightPoint(0)._blinkSequence->getPulse(2, duration, color);
    check(color == osg::Vec4(0,1,0,1), "Absolute COLOR_CHANGE including zero channel");
}
}

int main(int argc, char** argv)
{
    try
    {
        check(argc == 4, "Expected OpenFlight, osgjs and GLES plugin paths");
        for (int i = 1; i != argc; ++i)
            check(osgDB::Registry::instance()->loadLibrary(argv[i]) == osgDB::Registry::LOADED, "Load plugin");
        generateFlight();
        osg::ref_ptr<osg::Node> root = osgDB::readNodeFile("lightpoints-openflight.flt");
        check(root.valid(), "Read actual binary FLT");
        validate(*root, 0.3048);
        check(osgDB::writeNodeFile(*root, "lightpoints-openflight.osgjs"), "FLT to OSGJS");
        check(osgDB::writeNodeFile(*root, "lightpoints-openflight-gles.osgjs.gles"), "FLT to OSGJS.GLES");
        osg::ref_ptr<osgDB::Options> options = new osgDB::Options("noUnitsConversion");
        // Stream reading bypasses OpenFlight's filename-only local cache.
        std::ifstream input("lightpoints-openflight.flt", std::ios::binary);
        osg::ref_ptr<osg::Node> unscaled = osgDB::Registry::instance()->
            getReaderWriterForExtension("flt")->readNode(input, options).getNode();
        check(unscaled.valid(), "Read FLT without unit conversion");
        validate(*unscaled, 1.0);
        std::cout << "OpenFlight binary fixture, colors, timing, sectors and inherited systems passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
