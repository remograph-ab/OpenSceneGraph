#ifndef OSG_SHAPE_H
#define OSG_SHAPE_H

#include <stdio.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <osg/Referenced>

#include "ESRIType.h"

namespace ESRIShape {


enum ByteOrder {
    LittleEndian,
    BigEndian
};

enum PartType{
    TriangleStrip   = 0,
    TriangleFan     = 1,
    OuterRing       = 2,
    InnerRing       = 3,
    FirstRing       = 4,
    Ring            = 5
};

enum ShapeType {
    ShapeTypeNullShape   = 0,
    ShapeTypePoint       = 1,
    ShapeTypePolyLine    = 3,
    ShapeTypePolygon     = 5,
    ShapeTypeMultiPoint  = 8,
    ShapeTypePointZ      = 11,
    ShapeTypePolyLineZ   = 13,
    ShapeTypePolygonZ    = 15,
    ShapeTypeMultiPointZ = 18,
    ShapeTypePointM      = 21,
    ShapeTypePolyLineM   = 23,
    ShapeTypePolygonM    = 25,
    ShapeTypeMultiPointM = 28,
    ShapeTypeMultiPatch  = 31
};


struct BoundingBox
{
    Double Xmin;
    Double Ymin;
    Double Xmax;
    Double Ymax;
    Double Zmin;
    Double Zmax;
    Double Mmin;
    Double Mmax;

    bool read( int fd );
    bool write( int fd );

    void print();
};

///////////////

struct ShapeHeader
{
    Integer fileCode;
    Byte _unused_0[20];
    Integer fileLength;
    Integer version;
    Integer shapeType;
    BoundingBox bbox;

    bool read(int fd);
    bool write( int fd );

    void print();
};

struct RecordHeader
{
    Integer recordNumber;
    Integer contentLength;

    RecordHeader();

    bool read( int fd );
    bool write( int fd );

    void print();
};


struct NullRecord
{
    Integer shapeType;
    NullRecord();

    bool read( int fd );
    bool write( int fd );
};

//////////////////////////////////////////////////////////////////////

struct Box
{
    Double Xmin, Ymin, Xmax, Ymax;

    Box();
    Box(const Box &b );
    bool read( int fd );
    bool write( int fd );
};

struct Range {
    Double min, max;
    Range();
    Range( const Range &r );

    bool read( int fd );
    bool write( int fd );
};

struct ShapeObject : public osg::Referenced
{
    ShapeType shapeType;
    Integer contentLength;
    ShapeObject(ShapeType s);
    virtual ~ShapeObject();

    virtual ShapeObject *clone() = 0;
    virtual Integer getContentLength() = 0;
};


struct Point : public ShapeObject
{
    Double x, y;

    Point();
    Point(const Point &p);
    virtual ~Point();

    bool read( int fd );
    bool write( int fd );
    void print();

    virtual ShapeObject *clone()
    {
      Point *rec = new Point();
      rec->x = x;
      rec->y = y;
      return rec;
    }

    virtual Integer getContentLength()
    {
      return 2 + 16/2;
    }
};

struct PointRecord
{
    Point point;
    bool read( int fd );
    bool write( int fd );
};

struct MultiPoint: public ShapeObject
{
    Box     bbox;
    Integer numPoints;
    struct Point   *points;

    MultiPoint();

    MultiPoint( const struct MultiPoint &mpoint );

    virtual ~MultiPoint();

    bool read( int fd );
    bool write( int fd );

    void print();
    virtual ShapeObject *clone()
    {
      MultiPoint *rec = new MultiPoint();
      rec->bbox = bbox;
      rec->numPoints = numPoints;
      rec->points = new Point[numPoints];
      for (int i=0; i<numPoints; ++i) {
        rec->points[i].x = points[i].x;
        rec->points[i].y = points[i].y;
      }
      return rec;
    }

    virtual Integer getContentLength()
    {
      return 2 + (36 + numPoints*16)/2;
    }
};

struct PolyLine: public ShapeObject
{
    Box             bbox;
    Integer         numParts;
    Integer         numPoints;
    Integer         *parts;
    struct Point    *points;

    PolyLine();

    PolyLine( const PolyLine &p );

    virtual ~PolyLine();

    bool read( int fd );
    bool write( int fd );

    virtual ShapeObject *clone()
    {
      PolyLine *rec = new PolyLine();
      rec->bbox = bbox;
      rec->numParts = numParts;
      rec->numPoints = numPoints;
      rec->parts = new Integer[numParts];
      for (int i=0; i<numParts; ++i)
        rec->parts[i] = parts[i];
      rec->points = new Point[numPoints];
      for (int i=0; i<numPoints; ++i) {
        rec->points[i].x = points[i].x;
        rec->points[i].y = points[i].y;
      }
      return rec;
    }

    virtual Integer getContentLength()
    {
      return 2 + (40 + numParts*4 + numPoints*16)/2;
    }

};



struct Polygon : public ShapeObject
{
    Box             bbox;
    Integer         numParts;
    Integer         numPoints;
    Integer         *parts;
    Point           *points;

    Polygon();

    Polygon( const Polygon &p );

    virtual ~Polygon();


    bool read( int fd );
    bool write( int fd );

    virtual ShapeObject *clone()
    {
      Polygon *rec = new Polygon();
      rec->bbox = bbox;
      rec->numParts = numParts;
      rec->numPoints = numPoints;
      rec->parts = new Integer[numParts];
      for (int i=0; i<numParts; ++i)
        rec->parts[i] = parts[i];
      rec->points = new Point[numPoints];
      for (int i=0; i<numPoints; ++i) {
        rec->points[i].x = points[i].x;
        rec->points[i].y = points[i].y;
      }
      return rec;
    }

    virtual Integer getContentLength()
    {
      return 2 + (40 + numParts*4 + numPoints*16)/2;
    }
};

//////////////////////////////////////////////////////////////////////
struct PointM : public ShapeObject
{
    Double x, y, m;

    PointM();

    PointM(const PointM &p);

    virtual ~PointM();

    bool read( int fd );
    bool write( int fd );

    void print();

    virtual ShapeObject *clone()
    {
      PointM *rec = new PointM();
      rec->x = x;
      rec->y = y;
      rec->m = m;
      return rec;
    }

    virtual Integer getContentLength()
    {
      return 2 + 24/2;
    }
};

struct PointMRecord
{
    PointM pointM;

    bool read( int fd );
    bool write( int fd );
};


struct MultiPointM: public ShapeObject
{
    Box             bbox;
    Integer         numPoints;
    struct Point    *points;
    struct Range    mRange;
    Double          *mArray;

    MultiPointM();

    MultiPointM( const struct MultiPointM &mpointm );

    virtual ~MultiPointM();

    bool read( int fd );
    bool write( int fd );

    void print();

    virtual ShapeObject *clone()
    {
      MultiPointM *rec = new MultiPointM();
      rec->bbox = bbox;
      rec->numPoints = numPoints;
      rec->points = new Point[numPoints];
      for (int i=0; i<numPoints; ++i) {
        rec->points[i].x = points[i].x;
        rec->points[i].y = points[i].y;
      }
      rec->mRange = mRange;
      for (int i=0; i<numPoints; ++i)
        rec->mArray[i] = mArray[i];
      return rec;
    }

    virtual Integer getContentLength()
    {
      return 2 + (52 + numPoints*(16 + 8))/2;
    }
};


struct PolyLineM: public ShapeObject
{
    Box             bbox;
    Integer         numParts;
    Integer         numPoints;
    Integer         *parts;
    struct Point    *points;
    struct Range    mRange;
    Double          *mArray;

    PolyLineM();

    PolyLineM(const PolyLineM &p);

    virtual ~PolyLineM();

    bool read( int fd );
    bool write( int fd );

    virtual ShapeObject *clone()
    {
      PolyLineM *rec = new PolyLineM();
      rec->bbox = bbox;
      rec->numParts = numParts;
      rec->numPoints = numPoints;
      rec->parts = new Integer[numParts];
      for (int i=0; i<numParts; ++i)
        rec->parts[i] = parts[i];
      rec->points = new Point[numPoints];
      for (int i=0; i<numPoints; ++i) {
        rec->points[i].x = points[i].x;
        rec->points[i].y = points[i].y;
      }
      rec->mRange = mRange;
      for (int i=0; i<numPoints; ++i)
        rec->mArray[i] = mArray[i];
      return rec;
    }

    virtual Integer getContentLength()
    {
      return 2 + (56 + numParts*4 + numPoints*(16 + 8))/2;
    }
};


struct PolygonM : public ShapeObject
{
    Box             bbox;
    Integer         numParts;
    Integer         numPoints;
    Integer         *parts;
    Point           *points;
    struct Range    mRange;
    Double          *mArray;

    PolygonM();

    PolygonM(const PolygonM &p);

    virtual ~PolygonM();

    bool read( int fd );
    bool write( int fd );

    virtual ShapeObject *clone()
    {
      PolygonM *rec = new PolygonM();
      rec->bbox = bbox;
      rec->numParts = numParts;
      rec->numPoints = numPoints;
      rec->parts = new Integer[numParts];
      for (int i=0; i<numParts; ++i)
        rec->parts[i] = parts[i];
      rec->points = new Point[numPoints];
      for (int i=0; i<numPoints; ++i) {
        rec->points[i].x = points[i].x;
        rec->points[i].y = points[i].y;
      }
      rec->mRange = mRange;
      for (int i=0; i<numPoints; ++i)
        rec->mArray[i] = mArray[i];
      return rec;
    }

    virtual Integer getContentLength()
    {
      return 2 + (56 + numParts*4 + numPoints*(16 + 8))/2;
    }
};




//////////////////////////////////////////////////////////////////////




struct PointZ : public ShapeObject
{
    Double x, y, z, m;

    PointZ();
    PointZ(const PointZ &p);
    virtual ~PointZ();

    bool read( int fd );
    bool write( int fd );

    void print();

    virtual ShapeObject *clone()
    {
      PointZ *rec = new PointZ();
      rec->x = x;
      rec->y = y;
      rec->z = z;
      rec->m = m;
      return rec;
    }

    virtual Integer getContentLength()
    {
      return 2 + 32/2;
    }
};

struct MultiPointZ: public ShapeObject
{
    Box             bbox;
    Integer         numPoints;
    struct Point    *points;
    struct Range    zRange;
    Double          *zArray;
    struct Range    mRange;
    Double          *mArray;

    MultiPointZ();

    MultiPointZ( const struct MultiPointZ &);

    virtual ~MultiPointZ();

    bool read( int fd );
    bool write( int fd );

    void print();

    virtual ShapeObject *clone()
    {
      MultiPointZ *rec = new MultiPointZ();
      rec->bbox = bbox;
      rec->numPoints = numPoints;
      rec->points = new Point[numPoints];
      for (int i=0; i<numPoints; ++i) {
        rec->points[i].x = points[i].x;
        rec->points[i].y = points[i].y;
      }
      rec->zRange = zRange;
      for (int i=0; i<numPoints; ++i)
        rec->zArray[i] = zArray[i];
      rec->mRange = mRange;
      for (int i=0; i<numPoints; ++i)
        rec->mArray[i] = mArray[i];
      return rec;
    }

    virtual Integer getContentLength()
    {
      return 2 + (68 + numPoints*(16 + 16))/2;
    }
};



struct PolyLineZ: public ShapeObject
{
    Box             bbox;
    Integer         numParts;
    Integer         numPoints;
    Integer         *parts;
    struct Point    *points;
    struct Range    zRange;
    Double          *zArray;
    struct Range    mRange;
    Double          *mArray;

    PolyLineZ();

    PolyLineZ( const PolyLineZ &p );

    virtual ~PolyLineZ();

    bool read( int fd );
    bool write( int fd );

    virtual ShapeObject *clone()
    {
      PolyLineZ *rec = new PolyLineZ();
      rec->bbox = bbox;
      rec->numParts = numParts;
      rec->numPoints = numPoints;
      rec->parts = new Integer[numParts];
      for (int i=0; i<numParts; ++i)
        rec->parts[i] = parts[i];
      rec->points = new Point[numPoints];
      for (int i=0; i<numPoints; ++i) {
        rec->points[i].x = points[i].x;
        rec->points[i].y = points[i].y;
      }
      rec->zRange = zRange;
      for (int i=0; i<numPoints; ++i)
        rec->zArray[i] = zArray[i];
      rec->mRange = mRange;
      for (int i=0; i<numPoints; ++i)
        rec->mArray[i] = mArray[i];
      return rec;
    }

    virtual Integer getContentLength()
    {
      return 2 + (72 + numParts*4 + numPoints*(16 + 16))/2;
    }
};


struct PolygonZ : public ShapeObject
{
    Box             bbox;
    Integer         numParts;
    Integer         numPoints;
    Integer         *parts;
    Point           *points;
    struct Range    zRange;
    Double          *zArray;
    struct Range    mRange;
    Double          *mArray;

    PolygonZ();

    PolygonZ( const PolygonZ &p );

    virtual ~PolygonZ();


    bool read( int fd );
    bool write( int fd );

    virtual ShapeObject *clone()
    {
      PolygonZ *rec = new PolygonZ();
      rec->bbox = bbox;
      rec->numParts = numParts;
      rec->numPoints = numPoints;
      rec->parts = new Integer[numParts];
      for (int i=0; i<numParts; ++i)
        rec->parts[i] = parts[i];
      rec->points = new Point[numPoints];
      for (int i=0; i<numPoints; ++i) {
        rec->points[i].x = points[i].x;
        rec->points[i].y = points[i].y;
      }
      rec->zRange = zRange;
      for (int i=0; i<numPoints; ++i)
        rec->zArray[i] = zArray[i];
      rec->mRange = mRange;
      for (int i=0; i<numPoints; ++i)
        rec->mArray[i] = mArray[i];
      return rec;
    }

    virtual Integer getContentLength()
    {
      return 2 + (72 + numParts*4 + numPoints*(16 + 16))/2;
    }
};


//////////////////////////////////////////////////////////////////////


struct MultiPatch
{
    Box             bbox;
    Integer         numParts;
    Integer         numPoints;
    Integer         *parts;
    Integer         *partTypes;
    struct Point    *points;
    Range           zRange;
    Double          *zArray;
    Range           mRange;
    Double          *mArray;

    MultiPatch();
    MultiPatch( const MultiPatch &);
    virtual ~MultiPatch();
    bool read( int );
    bool write( int );

    //virtual ShapeObject *clone()
    //{
    //  MultiPatch *rec = new MultiPatch();
    //  rec->bbox = bbox;
    //  rec->numParts = numParts;
    //  rec->numPoints = numPoints;
    //  rec->parts = new Integer[numParts];
    //  for (int i=0; i<numParts; ++i)
    //    rec->parts[i] = parts[i];
    //  rec->partTypes = new Integer[numParts];
    //  for (int i=0; i<numParts; ++i)
    //    rec->partTypes[i] = partTypes[i];
    //  rec->points = new Point[numPoints];
    //  for (int i=0; i<numPoints; ++i) {
    //    rec->points[i].x = points[i].x;
    //    rec->points[i].y = points[i].y;
    //  }
    //  rec->zRange = zRange;
    //  for (int i=0; i<numPoints; ++i)
    //    rec->zArray[i] = zArray[i];
    //  rec->mRange = mRange;
    //  for (int i=0; i<numPoints; ++i)
    //    rec->mArray[i] = mArray[i];
    //  return rec;
    //}
};

}

#endif
