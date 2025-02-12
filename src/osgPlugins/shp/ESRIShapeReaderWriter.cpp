#include <fstream>
#include <vector>
#include <algorithm>

#include <fcntl.h>

#if defined(_MSC_VER)
    #include <io.h>
    #include <stdio.h>
#endif 

#include <osg/NodeVisitor>
#include <osg/Geode>
#include <osg/Geometry>

#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/fstream>
#include <osgDB/Registry>

#include <osgTerrain/Locator>

#include "ESRIType.h"

#include "ESRIShape.h"
#include "ESRIShapeParser.h"

#include "XBaseParser.h"


class CreateShpVisitor : public osg::NodeVisitor
{
public:

  CreateShpVisitor(const std::string &filename, const osgDB::ReaderWriter::Options *options = 0): osg::NodeVisitor( osg::NodeVisitor::TRAVERSE_ACTIVE_CHILDREN ), filename_(filename), options_(options)
  {
  }

  ~CreateShpVisitor()
  {
    if (!filename_.empty() && !polylines_.empty()) {
      int fd = 0;
#ifdef WIN32
      if ((fd = open(filename_.c_str(), O_WRONLY | O_CREAT | O_BINARY, 0644)) <= 0)
#else
      if ((fd = open(filename_.c_str(), O_WRONLY | O_CREAT, 0644)) <= 0)
#endif
      {
        if (fd)
          close(fd);
        std::cerr << "Failed opening " << filename_ << " for writing." << std::endl;
        return;
      }

      ESRIShape::ShapeHeader header;
      header.fileCode = 9994;
      for (int i=0; i<20; ++i)
        header._unused_0[i] = (ESRIShape::Byte)0;
      header.fileLength = 50; // Increase below
      header.version = 1000;
      header.shapeType = ESRIShape::ShapeTypePolyLine;
      header.bbox.Xmin = header.bbox.Ymin = FLT_MAX;
      header.bbox.Xmax = header.bbox.Ymax = -FLT_MAX;
      for (std::vector<ESRIShape::PolyLine *>::iterator iter = polylines_.begin(); iter != polylines_.end(); ++iter) {
        header.fileLength += (4 + (*iter)->getContentLength());
        header.bbox.Xmin = std::min(header.bbox.Xmin, (*iter)->bbox.Xmin);
        header.bbox.Ymin = std::min(header.bbox.Ymin, (*iter)->bbox.Ymin);
        header.bbox.Xmax = std::max(header.bbox.Xmax, (*iter)->bbox.Xmax);
        header.bbox.Ymax = std::max(header.bbox.Ymax, (*iter)->bbox.Ymax);
        header.bbox.Mmin = header.bbox.Mmax = header.bbox.Zmin = header.bbox.Zmax = 0.0;
      }
      if (!header.write(fd)) {
        std::cerr << "Failed writing header to " << filename_ << std::endl;
        return;
      }

      int i=1;
      for (std::vector<ESRIShape::PolyLine *>::iterator iter = polylines_.begin(); iter != polylines_.end(); ++iter) {
        if (!(*iter)->write(fd)) {
          std::cerr << "Failed writing polyline " << i << "/" << polylines_.size() << " to " << filename_ << std::endl;
          return;
        }
        ++i;
      }

      close(fd);
    }
  }

  const std::string &getErrorString() const { return errorString_; }

  virtual void apply(osg::Geode& geode)
  {
    osg::Matrix mat = osg::computeLocalToWorld(getNodePath());

    for (unsigned int i=0; i<geode.getNumDrawables(); ++i) {
      osg::Drawable *drawable = geode.getDrawable(i);
      if (!drawable)
        continue;

      osg::Geometry *geometry = drawable->asGeometry();
      if (geometry) {
        // Retrieve the data arrays from the geometry
        osg::Array *vertexArray = geometry->getVertexArray();

        // Retrieve index arrays if coordinates are indexed
        //osg::IndexArray *vertexIndices = geometry->getVertexIndices();

        // Traverse the primitive sets
        ESRIShape::Point *firstVertex = NULL, *prevVertex1 = NULL, *prevVertex2 = NULL, *prevVertex3 = NULL;
        for (unsigned int j=0; j<geometry->getNumPrimitiveSets(); ++j) {
          const osg::PrimitiveSet *primitiveSet = geometry->getPrimitiveSet(j);
          if (!primitiveSet)
            continue;

          // Specify number of vertices per primitive
          int numVerticesPerPrim = 0;
          const osg::DrawArrayLengths *drawArrayLengths = dynamic_cast<const osg::DrawArrayLengths *>(primitiveSet);
          osg::DrawArrayLengths::const_iterator arrayLengthsIter;
          if (drawArrayLengths)
            arrayLengthsIter = drawArrayLengths->begin();

          bool useDrawArrayLength = false;
          switch (primitiveSet->getMode())
          {
          //case osg::PrimitiveSet::POINTS:
          //  numVerticesPerPrim = 1;
          //  break;

          case osg::PrimitiveSet::LINES:
            numVerticesPerPrim = 2;
            break;

          case osg::PrimitiveSet::LINE_STRIP:
          case osg::PrimitiveSet::LINE_LOOP:
          case osg::PrimitiveSet::TRIANGLE_STRIP:
          case osg::PrimitiveSet::TRIANGLE_FAN:
          case osg::PrimitiveSet::QUAD_STRIP:
          case osg::PrimitiveSet::POLYGON:
            if (drawArrayLengths) {
              numVerticesPerPrim = *arrayLengthsIter;
              useDrawArrayLength = true;
            }
            else
              numVerticesPerPrim = primitiveSet->getNumIndices();
            break;

          case osg::PrimitiveSet::TRIANGLES:
            numVerticesPerPrim = 3;
            break;

          case osg::PrimitiveSet::QUADS:
            numVerticesPerPrim = 4;
            break;

          default:
            break;
          }

          // Create shape object
          ESRIShape::PolyLine *polyline = new ESRIShape::PolyLine();

          // Traverse the primitives
          bool stripOrFanFinished = false;
          int primitiveNumber = -1;
          int primitiveVertexNumber = 0;
          std::vector<ESRIShape::Point *> vertices;
          for (unsigned int k=0; k<primitiveSet->getNumIndices(); ++k) {
            if (numVerticesPerPrim <= 0 || (useDrawArrayLength && arrayLengthsIter == drawArrayLengths->end()))
              break;

            if (stripOrFanFinished || primitiveVertexNumber == 0) {
              ++primitiveNumber;
              stripOrFanFinished = false;            

              // Start with a new shape record
              vertices.clear();
            }

            unsigned int index = primitiveSet->index(k);
            unsigned int coordIndex = 0;
            //if (vertexIndices) {
            //  if (vertexIndices->getNumElements() <= index)
            //    continue;            
            //  coordIndex = vertexIndices->index(index);
            //}
            //else
              coordIndex = index;

            if (!vertexArray || vertexArray->getNumElements() <= coordIndex)
              continue;

            // Retrieve vertex position
            osg::Vec3 position;
            switch (vertexArray->getType())
            {
            case osg::Array::Vec3ArrayType:
              position = reinterpret_cast<const osg::Vec3 *>(vertexArray->getDataPointer())[coordIndex];
              break;
            case osg::Array::Vec3sArrayType:
              {
                osg::Vec3s sPosition = reinterpret_cast<const osg::Vec3s *>(static_cast<const osg::Vec3sArray *>(vertexArray)->getDataPointer())[coordIndex];
                position.set(sPosition.x(), sPosition.y(), sPosition.z());
              }
              break;
            case osg::Array::Vec3bArrayType:
              {
                osg::Vec3b bPosition = reinterpret_cast<const osg::Vec3b *>(static_cast<const osg::Vec3bArray *>(vertexArray)->getDataPointer())[coordIndex];
                position.set(bPosition.x(), bPosition.y(), bPosition.z());
              }
              break;
            default:
              continue;
              break;
            }

            // Set vertex coordinate
            ESRIShape::Point *vertex = new ESRIShape::Point();
            vertex->x = position.x();
            vertex->y = position.y();

            switch (primitiveSet->getMode())
            {
            case osg::PrimitiveSet::POINTS:
            case osg::PrimitiveSet::LINES:
            case osg::PrimitiveSet::LINE_STRIP:
            case osg::PrimitiveSet::LINE_LOOP:
            case osg::PrimitiveSet::TRIANGLES:
            case osg::PrimitiveSet::QUADS:
            case osg::PrimitiveSet::POLYGON:
              vertices.push_back(vertex);
              break;

            case osg::PrimitiveSet::TRIANGLE_STRIP:
              if (primitiveVertexNumber < 3) {
                vertices.push_back(vertex);
                if (primitiveVertexNumber == 2)
                  stripOrFanFinished = true;
              }
              else if (prevVertex1 != NULL && prevVertex2 != NULL) {
                vertices.push_back(prevVertex2);
                vertices.push_back(prevVertex1);
                vertices.push_back(vertex);
                stripOrFanFinished = true;

                // Every other triangle is upside down
                if (primitiveVertexNumber % 2 != 0) {
                  std::reverse(vertices.begin(), vertices.end());
                }
              }
              break;

            case osg::PrimitiveSet::TRIANGLE_FAN:
              if (primitiveVertexNumber < 3) {
                vertices.push_back(vertex);
                if (primitiveVertexNumber == 2)
                  stripOrFanFinished = true;
              }
              else if (firstVertex != NULL && prevVertex1 != NULL) {
                vertices.push_back(firstVertex);
                vertices.push_back(prevVertex1);
                vertices.push_back(vertex);
                stripOrFanFinished = true;
              }
              break;

            case osg::PrimitiveSet::QUAD_STRIP:
              if (primitiveVertexNumber < 4) {
                vertices.push_back(vertex);
                if (primitiveVertexNumber == 3) {
                  stripOrFanFinished = true;

                  // Since we can't control the order in which the first quad's vertices are added we re-order them now
                  ESRIShape::Point *tempPoint = vertices[0];
                  vertices[0] = vertices[1];
                  vertices[1] = tempPoint;
                }
              }
              else if (prevVertex1 != NULL && prevVertex2 != NULL && prevVertex3 != NULL) {
                vertices.push_back(prevVertex2);
                vertices.push_back(prevVertex3);
                vertices.push_back(prevVertex1);
                vertices.push_back(vertex);
                stripOrFanFinished = true;
              }
              break;

            default:
              break;
            }

            // Finish shape and write to disk
            if (stripOrFanFinished || primitiveVertexNumber == numVerticesPerPrim - 1) {
              // Close if line loop
              if (!vertices.empty()) {
                switch (primitiveSet->getMode())
                {
                case osg::PrimitiveSet::LINE_LOOP:
                case osg::PrimitiveSet::TRIANGLES:
                case osg::PrimitiveSet::QUADS:
                case osg::PrimitiveSet::POLYGON:
                case osg::PrimitiveSet::TRIANGLE_STRIP:
                case osg::PrimitiveSet::TRIANGLE_FAN:
                case osg::PrimitiveSet::QUAD_STRIP:
                  {
                    ESRIShape::Point *closingPoint = new ESRIShape::Point();
                    closingPoint->x = vertices[0]->x; 
                    closingPoint->y = vertices[0]->y;
                    vertices.push_back(closingPoint);
                  }
                  break;

                default:
                  break;
                }

                // Reverse vertex order (CCW in OSG, CW in SHP)
                std::reverse(vertices.begin(), vertices.end());

                // Calculate bounding box
                ESRIShape::Box bbox;
                bbox.Xmin = bbox.Ymin = FLT_MAX;
                bbox.Xmax = bbox.Ymax = -FLT_MAX;
                for (std::vector<ESRIShape::Point *>::iterator iter = vertices.begin(); iter != vertices.end(); ++iter) {
                  bbox.Xmin = std::min(bbox.Xmin, (*iter)->x);
                  bbox.Ymin = std::min(bbox.Ymin, (*iter)->y);
                  bbox.Xmax = std::max(bbox.Xmax, (*iter)->x);
                  bbox.Ymax = std::max(bbox.Ymax, (*iter)->y);
                }

                polyline->bbox = bbox;
                polyline->numParts = 1;
                polyline->numPoints = vertices.size();
                polyline->parts = new ESRIShape::Integer[1];
                polyline->parts[0] = 0;
                polyline->points = new ESRIShape::Point[vertices.size()];
                int index = 0;
                for (std::vector<ESRIShape::Point *>::iterator iter = vertices.begin(); iter != vertices.end(); ++iter) {
                  polyline->points[index].x = (*iter)->x;
                  polyline->points[index].y = (*iter)->y;
                  ++index;
                }
                polylines_.push_back(polyline);
              }
            }

            if (primitiveVertexNumber == numVerticesPerPrim - 1) {
              // We just created the last vertex in this primitive, 
              // reset the primitive vertex number and the three latest vertices
              primitiveVertexNumber = 0;
              prevVertex3 = NULL;
              prevVertex2 = NULL;
              prevVertex1 = NULL;
              firstVertex = NULL;

              // Also move to the next draw array lengths element
              if (useDrawArrayLength) {
                ++arrayLengthsIter;
                if (arrayLengthsIter == drawArrayLengths->end())
                  break;

                switch (primitiveSet->getMode())
                {
                case osg::PrimitiveSet::LINE_STRIP:
                case osg::PrimitiveSet::LINE_LOOP:
                case osg::PrimitiveSet::TRIANGLE_STRIP:
                case osg::PrimitiveSet::TRIANGLE_FAN:
                case osg::PrimitiveSet::QUAD_STRIP:
                case osg::PrimitiveSet::POLYGON:
                  numVerticesPerPrim = *arrayLengthsIter;
                  break;
                default:
                  break;
                }
              }
            }
            else {
              // Increase the primitive vertex number and
              // remember the three latest vertices for creation of strips, fans and loops
              ++primitiveVertexNumber;
              prevVertex3 = prevVertex2;
              prevVertex2 = prevVertex1;
              prevVertex1 = vertex;
            }
          }
        }

        continue;
      }

      // TODO: ShapeDrawable? ParticleSystem? ImpostorSprite? Text?
    }
  }

private:
  osgDB::ReaderWriter::Options const * options_;
  int fd_;
  std::string filename_;
  std::string errorString_;
  std::vector<ESRIShape::PolyLine *> polylines_;
};




class ESRIShapeReaderWriter : public osgDB::ReaderWriter
{
    public:
        ESRIShapeReaderWriter()
        {
            supportsExtension("shp","Geospatial Shape file format");
            supportsOption("double","Read x,y,z data as double an stored as geometry in osg::Vec3dArray's.");
            supportsOption("keepSeparatePoints", "Avoid combining point features into multi-point.");
        }

        virtual const char* className() const { return "ESRI Shape ReaderWriter"; }

        virtual bool acceptsExtension(const std::string& extension) const
        {
            return osgDB::equalCaseInsensitive(extension,"shp");
        }

        virtual ReadResult readObject(const std::string& fileName, const Options* opt) const
        { return readNode(fileName,opt); }

        virtual ReadResult readNode(const std::string& file, const Options* options) const
        {
            std::string ext = osgDB::getFileExtension(file);
            if (!acceptsExtension(ext))
                return ReadResult::FILE_NOT_HANDLED;

            std::string fileName = osgDB::findDataFile(file, options);
            if (fileName.empty()) return ReadResult::FILE_NOT_FOUND;

            bool useDouble = false;
            if (options && options->getOptionString().find("double")!=std::string::npos)
            {
                useDouble = true;
            }

            bool keepSeparatePoints = false;
            if (options && options->getOptionString().find("keepSeparatePoints") != std::string::npos)
            {
              keepSeparatePoints = true;
            }


            ESRIShape::ESRIShapeParser sp(fileName, useDouble, keepSeparatePoints);


            std::string xbaseFileName(osgDB::getNameLessExtension(fileName) + ".dbf");
            ESRIShape::XBaseParser xbp(xbaseFileName);


            if (sp.getGeode() && (xbp.getAttributeList().empty() == false))
            {
                if (sp.getGeode()->getNumDrawables() != xbp.getAttributeList().size())
                {
                    OSG_WARN << "ESRIShape loader : .dbf file contains a different number of records (" << xbp.getAttributeList().size()
                      << ") than .shp file (" << sp.getGeode()->getNumDrawables() << ")." << std::endl
                      << "                   .dbf record skipped." << std::endl;
                }
                else
                {
                    osg::Geode * geode = sp.getGeode();
                    unsigned int i = 0;

                    ESRIShape::XBaseParser::ShapeAttributeListList::const_iterator it, end = xbp.getAttributeList().end();
                    for (it = xbp.getAttributeList().begin(); it != end; ++it, ++i)
                    {
                        geode->getDrawable(i)->setUserData(it->get());
                    }
                }
            }

            if (sp.getGeode())
            {

                std::string projFileName(osgDB::getNameLessExtension(fileName) + ".prj");
                if (osgDB::fileExists(projFileName))
                {
                    osgDB::ifstream fin(projFileName.c_str());
                    if (fin)
                    {
                        std::string projstring;
                        while(!fin.eof())
                        {
                            char readline[4096];
                            *readline = 0;
                            fin.getline(readline, sizeof(readline));
                            if (!projstring.empty() && !fin.eof())
                            {
                                projstring += '\n';
                            }
                            projstring += readline;

                        }

                        if (!projstring.empty())
                        {
                            osgTerrain::Locator* locator = new osgTerrain::Locator;
                            sp.getGeode()->setUserData(locator);

                            if (projstring.compare(0,6,"GEOCCS")==0)
                            {
                                locator->setCoordinateSystemType(osgTerrain::Locator::GEOCENTRIC);
                            }
                            else if (projstring.compare(0,6,"PROJCS")==0)
                            {
                                locator->setCoordinateSystemType(osgTerrain::Locator::PROJECTED);
                            }
                            else if (projstring.compare(0,6,"GEOGCS")==0)
                            {
                                locator->setCoordinateSystemType(osgTerrain::Locator::GEOGRAPHIC);
                            }

                            locator->setFormat("WKT");
                            locator->setCoordinateSystem(projstring);
                            locator->setDefinedInFile(false);
                        }
                    }

                }


            }
            return sp.getGeode();
        }

        virtual WriteResult writeNode(const osg::Node& node, const std::string& fileName, const Options *opts = NULL) const
        {
            if (fileName.empty()) return WriteResult::FILE_NOT_HANDLED;

            std::string ext = osgDB::getLowerCaseFileExtension(fileName);
            if (ext != "shp" )
              return WriteResult::FILE_NOT_HANDLED;

            CreateShpVisitor createShpVisitor( fileName, opts );
            const_cast<osg::Node&>(node).accept( createShpVisitor );

            if (createShpVisitor.getErrorString().empty())
            {
                return WriteResult::FILE_SAVED;
            }
            else
            {
                OSG_NOTICE<<"Error: "<<createShpVisitor.getErrorString()<<std::endl;
                return WriteResult::ERROR_IN_WRITING_FILE;
            }

        }
};

REGISTER_OSGPLUGIN(shp, ESRIShapeReaderWriter)
