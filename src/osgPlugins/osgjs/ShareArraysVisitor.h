#pragma once

#include <vector>

#include <osg/ref_ptr>
#include <osg/Geometry>
#include <osg/NodeVisitor>

class ShareArraysVisitor : public osg::NodeVisitor
{
public:
  ShareArraysVisitor();

  void apply(osg::Geometry &geometry);

private:
  osg::ref_ptr<osg::Vec3Array> findEqualVertexArray_(osg::Vec3Array *vertexArray);
  osg::ref_ptr<osg::Vec3Array> findEqualNormalArray_(osg::Vec3Array *normalArray);
  osg::ref_ptr<osg::Vec4Array> findEqualColorArray_(osg::Vec4Array *colorArray);
  osg::ref_ptr<osg::Vec2Array> findEqualTexCoordArray_(osg::Vec2Array *texCoordArray);

  std::vector<osg::ref_ptr<osg::Vec3Array> > vertexArrays_;
  std::vector<osg::ref_ptr<osg::Vec3Array> > normalArrays_;
  std::vector<osg::ref_ptr<osg::Vec4Array> > colorArrays_;
  std::vector<osg::ref_ptr<osg::Vec2Array> > texCoordArrays_;
};
