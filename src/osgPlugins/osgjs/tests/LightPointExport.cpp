/* OpenSceneGraph - see LICENSE.txt for license details. */
#include <osg/MatrixTransform>
#include <osg/Sequence>
#include <osgAnimation/Bone>
#include <osgSim/LightPointNode>
#include <osgSim/MultiSwitch>
#include <osgDB/WriteFile>
#include <osgDB/Registry>
#include <osgUtil/Optimizer>
#include <iostream>

class CustomSector : public osgSim::Sector
{
public:
    CustomSector() {}
    CustomSector(const CustomSector& other, const osg::CopyOp& op = osg::CopyOp::SHALLOW_COPY):
        osgSim::Sector(other, op) {}
    META_Object(test, CustomSector);
    float operator()(const osg::Vec3&) const { return 0.5f; }
};

int main(int argc, char** argv)
{
    if (argc > 1) osgDB::Registry::instance()->loadLibrary(argv[1]);
    osg::ref_ptr<osg::MatrixTransform> root = new osg::MatrixTransform;
    root->setName("LightPointTransform");
    root->setMatrix(osg::Matrix::translate(10.0, 20.0, 30.0));
    osg::ref_ptr<osgAnimation::Bone> bone = new osgAnimation::Bone;
    bone->setName("LightPointBone");
    bone->setMatrix(osg::Matrix::translate(1.0, 2.0, 3.0));
    root->addChild(bone);
    osg::ref_ptr<osg::Sequence> sequence = new osg::Sequence;
    bone->addChild(sequence);
    osg::ref_ptr<osgSim::MultiSwitch> multi = new osgSim::MultiSwitch;
    sequence->addChild(multi);
    sequence->setTime(0, 2.0);
    sequence->setMode(osg::Sequence::STOP);
    osg::ref_ptr<osgSim::LightPointNode> lights = new osgSim::LightPointNode;
    lights->setName("BatchedLights");
    lights->setMinPixelSize(2.0f);
    lights->setMaxPixelSize(64.0f);
    lights->setMaxVisibleDistance2(40000.0f);
    lights->setPointSprite(true);
    osg::ref_ptr<osgSim::LightPointSystem> system = new osgSim::LightPointSystem;
    system->setIntensity(0.75f);
    system->setAnimationState(osgSim::LightPointSystem::ANIMATION_RANDOM);
    lights->setLightPointSystem(system);
    osg::ref_ptr<osgSim::SequenceGroup> group = new osgSim::SequenceGroup(12.5);
    osg::ref_ptr<osgSim::BlinkSequence> blink = new osgSim::BlinkSequence;
    blink->setSequenceGroup(group);
    blink->setPhaseShift(0.25);
    blink->addPulse(0.2, osg::Vec4(1, 0, 0, 1));
    blink->addPulse(0.8, osg::Vec4(0, 0, 1, 0.5));
    osg::ref_ptr<osgSim::Sector> sectors[] = {
        new osgSim::AzimSector(-1, 1, 0.1f),
        new osgSim::ElevationSector(-1.0f, 1.0f, 1.5f),
        new osgSim::AzimElevationSector(-1, 1, -0.5f, 0.5f, 0.1f),
        new osgSim::ConeSector(osg::Vec3(0, 1, 0), 0.5f, 0.2f),
        new osgSim::DirectionalSector(osg::Vec3(1, 2, 3), 5.5f, 0.5f, 0.25f, 1.0f)
    };
    for (unsigned int i = 0; i != 5; ++i) {
        osgSim::LightPoint point(i != 1, osg::Vec3(i, 2, 3), osg::Vec4(1, 0.5, 0.25, 0.8),
            2.0f, 0.25f, sectors[i], blink,
            i % 2 ? osgSim::LightPoint::ADDITIVE : osgSim::LightPoint::BLENDED);
        lights->addLightPoint(point);
    }
    // Sharing must retain identity; disabled switch children must still be exported.
    multi->addChild(lights);
    multi->addChild(lights);
    multi->setValue(0, 0, false);
    osg::ref_ptr<osgSim::LightPointNode> second = new osgSim::LightPointNode;
    second->setName("SharedSystem");
    second->setLightPointSystem(system);
    osgSim::LightPoint point(osg::Vec3(0, 0, 1), osg::Vec4(1, 1, 1, 1));
    osg::ref_ptr<osgSim::BlinkSequence> secondBlink = new osgSim::BlinkSequence;
    secondBlink->setSequenceGroup(group);
    secondBlink->addPulse(1, osg::Vec4(1, 1, 1, 1));
    point._blinkSequence = secondBlink;
    second->addLightPoint(point);
    multi->addChild(second);
    osg::ref_ptr<osgSim::LightPointNode> empty = new osgSim::LightPointNode;
    empty->setName("EmptyLights");
    multi->addChild(empty);

    osgUtil::Optimizer optimizer;
    optimizer.optimize(root);
    if (root->getMatrix().getTrans() != osg::Vec3d(10, 20, 30)) return 1;
    if (!osgDB::writeNodeFile(*root, "lightpoints-direct.osgjs")) return 2;
    if (!osgDB::writeNodeFile(*root, "lightpoints-gles.osgjs.gles")) return 3;
    osg::ref_ptr<osgSim::LightPointNode> invalid = new osgSim::LightPointNode;
    point._sector = new CustomSector;
    invalid->addLightPoint(point);
    if (osgDB::writeNodeFile(*invalid, "lightpoints-unsupported.osgjs")) return 4;
    std::cout << "LightPoint export fixtures and unsupported-sector rejection passed\n";
    return 0;
}
