#include "BindPerVertexVisitor"


void BindPerVertexVisitor::process(osg::Geometry& geometry) {
    unsigned int numVertices = (geometry.getVertexArray() ? geometry.getVertexArray()->getNumElements() : 0);

    if (geometry.getNormalArray() && geometry.getNormalBinding() != osg::Geometry::BIND_PER_VERTEX) {
        bindPerVertex(geometry.getNormalArray(),
                      geometry.getNormalBinding(),
                      geometry.getPrimitiveSetList(),
                      numVertices);
        if (geometry.getNormalArray()->getNumElements() == 0)
          geometry.setNormalArray(NULL);
        geometry.setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
    }

    if (geometry.getColorArray() && geometry.getColorBinding() != osg::Geometry::BIND_PER_VERTEX) {
        bindPerVertex(geometry.getColorArray(),
                      geometry.getColorBinding(),
                      geometry.getPrimitiveSetList(),
                      numVertices);
        if (geometry.getColorArray()->getNumElements() == 0)
          geometry.setColorArray(NULL);
        geometry.setColorBinding(osg::Geometry::BIND_PER_VERTEX);
    }

    if (geometry.getSecondaryColorArray() && geometry.getSecondaryColorBinding() != osg::Geometry::BIND_PER_VERTEX) {
        bindPerVertex(geometry.getSecondaryColorArray(),
                      geometry.getSecondaryColorBinding(),
                      geometry.getPrimitiveSetList(),
                      numVertices);
        if (geometry.getSecondaryColorArray()->getNumElements() == 0)
          geometry.setSecondaryColorArray(NULL);
        geometry.setSecondaryColorBinding(osg::Geometry::BIND_PER_VERTEX);
    }

    if (geometry.getFogCoordArray() && geometry.getFogCoordBinding() != osg::Geometry::BIND_PER_VERTEX) {
        bindPerVertex(geometry.getFogCoordArray(),
                      geometry.getFogCoordBinding(),
                      geometry.getPrimitiveSetList(),
                      numVertices);
        if (geometry.getFogCoordArray()->getNumElements() == 0)
          geometry.setFogCoordArray(NULL);
        geometry.setFogCoordBinding(osg::Geometry::BIND_PER_VERTEX);
    }
}


void BindPerVertexVisitor::bindPerVertex(osg::Array* src,
                                         osg::Geometry::AttributeBinding fromBinding,
                                         osg::Geometry::PrimitiveSetList& primitives,
                                         const unsigned int& numVertices) {
  if (doConvert<osg::ByteArray>(src, fromBinding, primitives, numVertices))
        return;
  if (doConvert<osg::ShortArray>(src, fromBinding, primitives, numVertices))
        return;
  if (doConvert<osg::IntArray>(src, fromBinding, primitives, numVertices))
        return;
  if (doConvert<osg::UByteArray>(src, fromBinding, primitives, numVertices))
        return;
  if (doConvert<osg::UShortArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::UIntArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::FloatArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::DoubleArray>(src, fromBinding, primitives, numVertices))
        return;

    if (doConvert<osg::Vec2Array>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec3Array>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec4Array>(src, fromBinding, primitives, numVertices))
        return;

    if (doConvert<osg::Vec2bArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec3bArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec4bArray>(src, fromBinding, primitives, numVertices))
        return;

    if (doConvert<osg::Vec2sArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec3sArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec4sArray>(src, fromBinding, primitives, numVertices))
        return;

    if (doConvert<osg::Vec2iArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec3iArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec4iArray>(src, fromBinding, primitives, numVertices))
        return;

    if (doConvert<osg::Vec2dArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec3dArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec4dArray>(src, fromBinding, primitives, numVertices))
        return;

    if (doConvert<osg::Vec2ubArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec3ubArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec4ubArray>(src, fromBinding, primitives, numVertices))
        return;

    if (doConvert<osg::Vec2usArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec3usArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec4usArray>(src, fromBinding, primitives, numVertices))
        return;

    if (doConvert<osg::Vec2uiArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec3uiArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::Vec4uiArray>(src, fromBinding, primitives, numVertices))
        return;

    if (doConvert<osg::MatrixfArray>(src, fromBinding, primitives, numVertices))
        return;
    if (doConvert<osg::MatrixdArray>(src, fromBinding, primitives, numVertices))
        return;
}


template <class T>
bool BindPerVertexVisitor::doConvert(osg::Array* src,
                                     osg::Geometry::AttributeBinding fromBinding,
                                     osg::Geometry::PrimitiveSetList& primitives,
                                     const unsigned int& numVertices) {
    T* array= dynamic_cast<T*>(src);
    if (array) {
        convert(*array, fromBinding, primitives, numVertices);
        return true;
    }
    return false;
}


template <class T>
void BindPerVertexVisitor::convert(T& array,
                                   osg::Geometry::AttributeBinding fromBinding,
                                   osg::Geometry::PrimitiveSetList& primitives,
                                   const unsigned int& numVertices) {
    osg::ref_ptr<T> result = new T();
    for (unsigned int p = 0; p < primitives.size(); p++) {
        switch ( primitives[p]->getMode() ) {
        case osg::PrimitiveSet::POINTS:
            osg::notify(osg::WARN) << "ConvertToBindPerVertex not supported for POINTS" << std::endl;
            break;

        case osg::PrimitiveSet::LINE_STRIP:
            switch(fromBinding) {
            case osg::Geometry::BIND_OFF:
            case osg::Geometry::BIND_PER_VERTEX:
                break;
            case osg::Geometry::BIND_OVERALL:
            {
                unsigned int nb = std::min(numVertices, primitives[p]->getNumIndices());
                for (unsigned int i = 0; i < nb; i++)
                    result->push_back(array[0]);
            }
            break;
            case osg::Geometry::BIND_PER_PRIMITIVE_SET:
            {
                unsigned int nb = std::min(numVertices, primitives[p]->getNumIndices());
                for (unsigned int i = 0; i < nb; i++)
                    result->push_back(array[p]);
            }
            break;
            }
            break;

        case osg::PrimitiveSet::LINES:
            switch(fromBinding) {
            case osg::Geometry::BIND_OFF:
            case osg::Geometry::BIND_PER_VERTEX:
                break;
            case osg::Geometry::BIND_OVERALL:
            {
                unsigned int nb = std::min(numVertices, primitives[p]->getNumIndices());
                for (unsigned int i = 0; i < nb; i++)
                    result->push_back(array[0]);
            }
            break;
            case osg::Geometry::BIND_PER_PRIMITIVE_SET:
            {
                unsigned int nb = std::min(numVertices, primitives[p]->getNumIndices());
                for (unsigned int i = 0; i < nb; i++)
                    result->push_back(array[p]);
            }
            break;
            }
            break;

        case osg::PrimitiveSet::TRIANGLES:
            switch(fromBinding) {
            case osg::Geometry::BIND_OFF:
            case osg::Geometry::BIND_PER_VERTEX:
                break;
            case osg::Geometry::BIND_OVERALL:
            {
                unsigned int nb = std::min(numVertices, primitives[p]->getNumIndices());
                for (unsigned int i = 0; i < nb; i++)
                    result->push_back(array[0]);
            }
            break;
            case osg::Geometry::BIND_PER_PRIMITIVE_SET:
            {
                unsigned int nb = std::min(numVertices, primitives[p]->getNumIndices());
                for (unsigned int i = 0; i < nb; i++)
                    result->push_back(array[p]);
            }
            break;
            }
            break;

        case osg::PrimitiveSet::TRIANGLE_STRIP:
            switch(fromBinding) {
            case osg::Geometry::BIND_OFF:
            case osg::Geometry::BIND_PER_VERTEX:
                break;
            case osg::Geometry::BIND_OVERALL:
            {
                unsigned int nb = std::min(numVertices, primitives[p]->getNumIndices());
                for (unsigned int i = 0; i < nb; i++)
                    result->push_back(array[0]);
            }
            break;
            case osg::Geometry::BIND_PER_PRIMITIVE_SET:
            {
                osg::notify(osg::FATAL) << "Can't convert Array from BIND_PER_PRIMITIVE_SET to BIND_PER_VERTEX, for TRIANGLE_STRIP" << std::endl;
            }
            break;
            }
            break;

        case osg::PrimitiveSet::TRIANGLE_FAN:
            switch(fromBinding) {
            case osg::Geometry::BIND_OFF:
            case osg::Geometry::BIND_PER_VERTEX:
                break;
            case osg::Geometry::BIND_OVERALL:
            {
              unsigned int nb = std::min(numVertices, primitives[p]->getNumIndices());
              for (unsigned int i = 0; i < nb; i++)
                    result->push_back(array[0]);
            }
            break;
            case osg::Geometry::BIND_PER_PRIMITIVE_SET:
            {
                osg::notify(osg::FATAL) << "Can't convert Array from BIND_PER_PRIMITIVE_SET to BIND_PER_VERTEX, for TRIANGLE_FAN" << std::endl;
            }
            break;
            }
            break;

        case osg::PrimitiveSet::QUADS:
            switch(fromBinding) {
            case osg::Geometry::BIND_OFF:
            case osg::Geometry::BIND_PER_VERTEX:
                break;
            case osg::Geometry::BIND_OVERALL:
            {
              unsigned int nb = std::min(numVertices, primitives[p]->getNumIndices());
              for (unsigned int i = 0; i < nb; i++)
                    result->push_back(array[0]);
            }
            break;
            case osg::Geometry::BIND_PER_PRIMITIVE_SET:
            {
                osg::notify(osg::FATAL) << "Can't convert Array from BIND_PER_PRIMITIVE_SET to BIND_PER_VERTEX, for QUADS" << std::endl;
            }
            break;
            }
            break;

        case osg::PrimitiveSet::QUAD_STRIP:
            switch(fromBinding) {
            case osg::Geometry::BIND_OFF:
            case osg::Geometry::BIND_PER_VERTEX:
                break;
            case osg::Geometry::BIND_OVERALL:
            {
                unsigned int nb = std::min(numVertices, primitives[p]->getNumIndices());
                for (unsigned int i = 0; i < nb; i++)
                    result->push_back(array[0]);
            }
            break;
            case osg::Geometry::BIND_PER_PRIMITIVE_SET:
            {
                osg::notify(osg::FATAL) << "Can't convert Array from BIND_PER_PRIMITIVE_SET to BIND_PER_VERTEX, for QUAD_STRIP" << std::endl;
            }
            break;
            }
            break;
        }
    }
    array = *result;
}
