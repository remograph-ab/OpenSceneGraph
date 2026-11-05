/* -*-c++-*- OpenSceneGraph
 * See LICENSE.txt for the OpenSceneGraph Public License.
 */

#include "WriteVisitor"
#include <stdexcept>
#include <typeinfo>

namespace
{
void writeAzimuth(JSONObject& json, const osgSim::AzimRange& range)
{
    float minimum, maximum, fade;
    range.getAzimuthRange(minimum, maximum, fade);
    json.getMaps()["MinAzimuth"] = new JSONValue<float>(minimum);
    json.getMaps()["MaxAzimuth"] = new JSONValue<float>(maximum);
    json.getMaps()["AzimuthFadeAngle"] = new JSONValue<float>(fade);
}

void writeElevation(JSONObject& json, const osgSim::ElevationRange& range)
{
    json.getMaps()["MinElevation"] = new JSONValue<float>(range.getMinElevation());
    json.getMaps()["MaxElevation"] = new JSONValue<float>(range.getMaxElevation());
    json.getMaps()["ElevationFadeAngle"] = new JSONValue<float>(range.getFadeAngle());
    json.getMaps()["MinFadeElevation"] = new JSONValue<float>(range.getMinFadeElevation());
    json.getMaps()["MaxFadeElevation"] = new JSONValue<float>(range.getMaxFadeElevation());
}

void unsupported(const osg::Object& object)
{
    const std::string message = std::string("osgjs: unsupported light-point object ") +
        object.libraryName() + "::" + object.className();
    osg::notify(osg::WARN) << message << std::endl;
    throw std::runtime_error(message);
}
}

JSONObject* WriteVisitor::createJSONSector(osgSim::Sector* sector)
{
    osg::ref_ptr<JSONObject> json = new JSONObject;
    // Exact type checks avoid silently discarding custom sector behavior.
    if (typeid(*sector) == typeid(osgSim::AzimSector)) {
        writeAzimuth(*json, *static_cast<osgSim::AzimSector*>(sector));
    }
    else if (typeid(*sector) == typeid(osgSim::ElevationSector)) {
        writeElevation(*json, *static_cast<osgSim::ElevationSector*>(sector));
    }
    else if (typeid(*sector) == typeid(osgSim::AzimElevationSector)) {
        osgSim::AzimElevationSector& range = *static_cast<osgSim::AzimElevationSector*>(sector);
        writeAzimuth(*json, range);
        writeElevation(*json, range);
    }
    else if (typeid(*sector) == typeid(osgSim::ConeSector)) {
        osgSim::ConeSector& cone = *static_cast<osgSim::ConeSector*>(sector);
        json->getMaps()["Axis"] = new JSONVec3Array(cone.getAxis());
        json->getMaps()["Angle"] = new JSONValue<float>(cone.getAngle());
        json->getMaps()["FadeAngle"] = new JSONValue<float>(cone.getFadeAngle());
    }
    else if (typeid(*sector) == typeid(osgSim::DirectionalSector)) {
        osgSim::DirectionalSector& direction = *static_cast<osgSim::DirectionalSector*>(sector);
        json->getMaps()["Direction"] = new JSONVec3Array(direction.getDirection());
        json->getMaps()["HorizLobeAngle"] = new JSONValue<float>(direction.getHorizLobeAngle());
        json->getMaps()["VertLobeAngle"] = new JSONValue<float>(direction.getVertLobeAngle());
        json->getMaps()["LobeRollAngle"] = new JSONValue<float>(direction.getLobeRollAngle());
        json->getMaps()["FadeAngle"] = new JSONValue<float>(direction.getFadeAngle());
        json->getMaps()["HorizFadeAngle"] = new JSONValue<float>(direction.getHorizFadeAngle());
        json->getMaps()["VertFadeAngle"] = new JSONValue<float>(direction.getVertFadeAngle());
    }
    else unsupported(*sector);
    json->getMaps()["Type"] = new JSONValue<std::string>(sector->className());
    return json.release();
}

JSONObject* WriteVisitor::createJSONBlinkSequence(osgSim::BlinkSequence* sequence)
{
    if (typeid(*sequence) != typeid(osgSim::BlinkSequence)) unsupported(*sequence);
    if (JSONObject* reference = getJSON(sequence)) return reference;
    osg::ref_ptr<JSONObject> json = new JSONObjectWithUniqueID;
    setJSON(sequence, json);
    json->getMaps()["PhaseShift"] = new JSONValue<double>(sequence->getPhaseShift());
    // Preserve the stored period: setPulse() does not recalculate it in native OSG.
    json->getMaps()["PulsePeriod"] = new JSONValue<double>(sequence->getPulsePeriod());
    osg::ref_ptr<JSONArray> pulses = new JSONArray;
    for (int i = 0; i < sequence->getNumPulses(); ++i) {
        double duration;
        osg::Vec4 color;
        sequence->getPulse(i, duration, color);
        osg::ref_ptr<JSONObject> pulse = new JSONObject;
        pulse->getMaps()["Duration"] = new JSONValue<double>(duration);
        pulse->getMaps()["Color"] = new JSONVec4Array(color);
        pulses->getArray().push_back(pulse);
    }
    json->getMaps()["Pulses"] = pulses;
    if (osgSim::SequenceGroup* group = sequence->getSequenceGroup()) {
        if (typeid(*group) != typeid(osgSim::SequenceGroup)) unsupported(*group);
        osg::ref_ptr<JSONObject> jsonGroup = getJSON(group);
        if (!jsonGroup) {
            jsonGroup = new JSONObjectWithUniqueID;
            setJSON(group, jsonGroup);
            jsonGroup->getMaps()["BaseTime"] = new JSONValue<double>(group->getBaseTime());
        }
        json->getMaps()["SequenceGroup"] = jsonGroup;
    }
    return json.release();
}

JSONObject* WriteVisitor::createJSONLightPointSystem(osgSim::LightPointSystem* system)
{
    if (typeid(*system) != typeid(osgSim::LightPointSystem)) unsupported(*system);
    if (JSONObject* reference = getJSON(system)) return reference;
    osg::ref_ptr<JSONObject> json = new JSONObjectWithUniqueID;
    setJSON(system, json);
    json->getMaps()["Intensity"] = new JSONValue<float>(system->getIntensity());
    std::string state;
    switch (system->getAnimationState()) {
        case osgSim::LightPointSystem::ANIMATION_ON: state = "ANIMATION_ON"; break;
        case osgSim::LightPointSystem::ANIMATION_OFF: state = "ANIMATION_OFF"; break;
        case osgSim::LightPointSystem::ANIMATION_RANDOM: state = "ANIMATION_RANDOM"; break;
        default: throw std::runtime_error("osgjs: invalid LightPointSystem animation state");
    }
    json->getMaps()["AnimationState"] = new JSONValue<std::string>(state);
    return json.release();
}

JSONObject* WriteVisitor::createJSONLightPointNode(osgSim::LightPointNode* node)
{
    osg::ref_ptr<JSONObject> json = new JSONNode;
    setJSON(node, json);
    json->getMaps()["MinPixelSize"] = new JSONValue<float>(node->getMinPixelSize());
    json->getMaps()["MaxPixelSize"] = new JSONValue<float>(node->getMaxPixelSize());
    json->getMaps()["MaxVisibleDistance2"] = new JSONValue<float>(node->getMaxVisibleDistance2());
    json->getMaps()["PointSprite"] = new JSONValue<bool>(node->getPointSprite());
    if (node->getLightPointSystem())
        json->getMaps()["LightPointSystem"] = createJSONLightPointSystem(node->getLightPointSystem());
    osg::ref_ptr<JSONArray> points = new JSONArray;
    const osgSim::LightPointNode::LightPointList& list = node->getLightPointList();
    for (osgSim::LightPointNode::LightPointList::const_iterator i = list.begin(); i != list.end(); ++i) {
        osg::ref_ptr<JSONObject> point = new JSONObject;
        point->getMaps()["On"] = new JSONValue<bool>(i->_on);
        point->getMaps()["Position"] = new JSONVec3Array(i->_position);
        point->getMaps()["Color"] = new JSONVec4Array(i->_color);
        point->getMaps()["Intensity"] = new JSONValue<float>(i->_intensity);
        point->getMaps()["Radius"] = new JSONValue<float>(i->_radius);
        if (i->_blendingMode != osgSim::LightPoint::ADDITIVE && i->_blendingMode != osgSim::LightPoint::BLENDED)
            throw std::runtime_error("osgjs: invalid light-point blending mode");
        point->getMaps()["BlendingMode"] = new JSONValue<std::string>(
            i->_blendingMode == osgSim::LightPoint::ADDITIVE ? "ADDITIVE" : "BLENDED");
        if (i->_sector) point->getMaps()["Sector"] = createJSONSector(i->_sector.get());
        if (i->_blinkSequence) point->getMaps()["BlinkSequence"] = createJSONBlinkSequence(i->_blinkSequence.get());
        points->getArray().push_back(point);
    }
    json->getMaps()["LightPoints"] = points;
    return json.release();
}
