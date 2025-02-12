#include <iostream>

#include "ShareArraysVisitor.h"

ShareArraysVisitor::ShareArraysVisitor() : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
{
}

void ShareArraysVisitor::apply(osg::Geometry &geometry)
{
  osg::Array *array = geometry.getVertexArray();
  if (array) {
    osg::Vec3Array *vertexArray = dynamic_cast<osg::Vec3Array *>(array);
    osg::ref_ptr<osg::Vec3Array> equalVertexArray = findEqualVertexArray_(vertexArray);
    if (equalVertexArray.valid())
      geometry.setVertexArray(equalVertexArray);
    else
      vertexArrays_.push_back(vertexArray);
  }

  array = geometry.getNormalArray();
  if (array) {
    osg::Vec3Array *normalArray = dynamic_cast<osg::Vec3Array *>(array);
    osg::ref_ptr<osg::Vec3Array> equalNormalArray = findEqualNormalArray_(normalArray);
    if (equalNormalArray.valid())
      geometry.setNormalArray(equalNormalArray);
    else
      normalArrays_.push_back(normalArray);
  }

  array = geometry.getColorArray();
  if (array) {
    osg::Vec4Array *colorArray = dynamic_cast<osg::Vec4Array *>(array);
    osg::ref_ptr<osg::Vec4Array> equalColorArray = findEqualColorArray_(colorArray);
    if (equalColorArray.valid())
      geometry.setColorArray(equalColorArray);
    else
      colorArrays_.push_back(colorArray);
  }

  for (unsigned int i = 0; i < 8; ++i) {
    array = geometry.getTexCoordArray(i);
    if (array) {
      osg::Vec2Array *texCoordArray = dynamic_cast<osg::Vec2Array *>(array);
      osg::ref_ptr<osg::Vec2Array> equalTexCoordArray = findEqualTexCoordArray_(texCoordArray);
      if (equalTexCoordArray.valid())
        geometry.setTexCoordArray(i, equalTexCoordArray);
      else
        texCoordArrays_.push_back(texCoordArray);
    }
  }
}

osg::ref_ptr<osg::Vec3Array> ShareArraysVisitor::findEqualVertexArray_(osg::Vec3Array *vertexArray)
{
  for (std::vector<osg::ref_ptr<osg::Vec3Array> >::iterator it = vertexArrays_.begin(); it != vertexArrays_.end(); ++it) {
    if (*it->get() == *vertexArray) {
      return *it;
    }
  }
  return osg::ref_ptr<osg::Vec3Array>();
}

osg::ref_ptr<osg::Vec3Array> ShareArraysVisitor::findEqualNormalArray_(osg::Vec3Array *normalArray)
{
  for (std::vector<osg::ref_ptr<osg::Vec3Array> >::iterator it = normalArrays_.begin(); it != normalArrays_.end(); ++it) {
    if (*it->get() == *normalArray)
      return *it;
  }
  return osg::ref_ptr<osg::Vec3Array>();
}

osg::ref_ptr<osg::Vec4Array> ShareArraysVisitor::findEqualColorArray_(osg::Vec4Array *colorArray)
{
  for (std::vector<osg::ref_ptr<osg::Vec4Array> >::iterator it = colorArrays_.begin(); it != colorArrays_.end(); ++it) {
    if (*it->get() == *colorArray)
      return *it;
  }
  return osg::ref_ptr<osg::Vec4Array>();
}

osg::ref_ptr<osg::Vec2Array> ShareArraysVisitor::findEqualTexCoordArray_(osg::Vec2Array *texCoordArray)
{
  for (std::vector<osg::ref_ptr<osg::Vec2Array> >::iterator it = texCoordArrays_.begin(); it != texCoordArrays_.end(); ++it) {
    if (*it->get() == *texCoordArray)
      return *it;
  }
  return osg::ref_ptr<osg::Vec2Array>();
}
