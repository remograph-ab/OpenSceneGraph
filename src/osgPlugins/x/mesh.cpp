// -*-c++-*-

/*
 * $Id$
 *
 * Loader for DirectX .x files.
 * Copyright (c)2002-2006 Ulrich Hertlein <u.hertlein@sandbox.de>
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
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if defined(_MSC_VER) && (_MSC_VER <= 1200)
#pragma warning (disable : 4786)
#endif

#include "mesh.h"
#include "directx.h"
#include "parser.h"

#include <iostream>
#include <string.h>
#include <stdlib.h>

#include <osg/Notify>

using namespace DX;
using namespace std;


namespace {

    /// Sequential reader for the numeric values of a data object, with bounds checking.
    class NumberReader {
    public:
        NumberReader(const std::vector<double>& v) : _v(v), _p(0) {}

        size_t remaining() const { return _v.size() - _p; }

        double next()
        {
            if (_p >= _v.size())
                return 0.0;
            return _v[_p++];
        }

        /// Read a count or index (negative or out-of-range values give 0 / UINT_MAX).
        unsigned int nextUInt()
        {
            double d = next();
            if (!(d >= 0.0))
                return 0;
            if (d >= 4294967295.0)
                return 0xffffffffu;
            return (unsigned int) d;
        }

    private:
        const std::vector<double>& _v;
        size_t _p;
    };

    /// Read 'count' Vectors.
    void readVectors(NumberReader& r, unsigned int count, std::vector<Vector>& v)
    {
        size_t n = count;
        if (n > r.remaining() / 3)
            n = r.remaining() / 3;
        v.resize(n);
        for (size_t i = 0; i < n; ++i) {
            v[i].x = (float) r.next();
            v[i].y = (float) r.next();
            v[i].z = (float) r.next();
        }
    }

    /// Read 'count' MeshFaces.
    void readFaces(NumberReader& r, unsigned int count, std::vector<MeshFace>& faces)
    {
        faces.clear();
        if (count > r.remaining())
            count = (unsigned int) r.remaining();
        faces.reserve(count);
        for (unsigned int i = 0; i < count && r.remaining() > 0; ++i) {
            unsigned int n = r.nextUInt();
            if (n > r.remaining())
                break;
            MeshFace face(n);
            for (unsigned int j = 0; j < n; ++j)
                face[j] = r.nextUInt();
            faces.push_back(face);
        }
    }

} // anonymous namespace


/**********************************************************************
 *
 * DirectX material.
 *
 **********************************************************************/

namespace DX {

    void initDefaultMaterial(Material& material)
    {
        material.name.clear();
        material.faceColor.red = material.faceColor.green = material.faceColor.blue = material.faceColor.alpha = 1.0f;
        material.power = 0.0f;
        material.specularColor.red = material.specularColor.green = material.specularColor.blue = 0.0f;
        material.emissiveColor.red = material.emissiveColor.green = material.emissiveColor.blue = 0.0f;
        material.texture.clear();
    }

    void parseMaterial(const DataObject& obj, Material& material)
    {
        initDefaultMaterial(material);
        material.name = obj.name;

        NumberReader r(obj.numbers);
        if (r.remaining() >= 11) {
            material.faceColor.red = (float) r.next();
            material.faceColor.green = (float) r.next();
            material.faceColor.blue = (float) r.next();
            material.faceColor.alpha = (float) r.next();
            material.power = (float) r.next();
            material.specularColor.red = (float) r.next();
            material.specularColor.green = (float) r.next();
            material.specularColor.blue = (float) r.next();
            material.emissiveColor.red = (float) r.next();
            material.emissiveColor.green = (float) r.next();
            material.emissiveColor.blue = (float) r.next();
        }
        else {
            OSG_WARN << "DirectX loader: Incomplete material '" << obj.name << "'" << endl;
        }

        // Textures (template name is case-insensitive: 'TextureFilename' and 'TextureFileName' both occur)
        for (size_t i = 0; i < obj.children.size(); ++i) {
            const DataObject& child = *obj.children[i];
            if (child.isType("TextureFilename") && !child.strings.empty() && !child.strings[0].empty())
                material.texture.push_back(child.strings[0]);
        }
    }

} // namespace DX


/**********************************************************************
 *
 * DirectX mesh.
 *
 **********************************************************************/

Mesh::Mesh(Object * obj)
{
    _obj = obj;
    _normals = 0;
    _textureCoords = 0;
    _materialList = 0;
}

void Mesh::clear()
{
    if (_normals) {
        delete _normals;
        _normals = 0;
    }

    if (_textureCoords) {
        delete _textureCoords;
        _textureCoords = 0;
    }

    if (_materialList) {
        delete _materialList;
        _materialList = 0;
    }
}

bool Mesh::generateNormals(float /*creaseAngle*/)
{
    // Forget old normals
    if (_normals) {
        delete _normals;
        _normals = 0;
    }

    /*
     * Calculate per-vertex normals as the normalized sum of the normals of
     * all faces that share the vertex. The index of the vertex normal is
     * identical to the vertex index.
     */
    _normals = new MeshNormals;
    Vector zero = { 0.0f, 0.0f, 0.0f };
    _normals->normals.assign(_vertices.size(), zero);

    const unsigned int numVertices = (unsigned int) _vertices.size();
    for (size_t fi = 0; fi < _faces.size(); fi++) {
        const MeshFace& face = _faces[fi];
        size_t n = face.size();
        if (n < 3 || face[0] >= numVertices || face[1] >= numVertices || face[2] >= numVertices)
            continue;

        const Vector& p0 = _vertices[face[0]];
        const Vector& p1 = _vertices[face[1]];
        const Vector& p2 = _vertices[face[2]];

        // Edge vectors
        Vector e0 = { p1.x - p0.x, p1.y - p0.y, p1.z - p0.z };
        Vector e1 = { p2.x - p0.x, p2.y - p0.y, p2.z - p0.z };

        // Cross-product of e0,e1
        Vector normal;
        normal.x = e0.y * e1.z - e0.z * e1.y;
        normal.y = e0.z * e1.x - e0.x * e1.z;
        normal.z = e0.x * e1.y - e0.y * e1.x;
        normal.normalize();

        for (size_t i = 0; i < n; i++) {
            unsigned int vi = face[i];
            if (vi < numVertices) {
                Vector& vn = _normals->normals[vi];
                vn.x += normal.x;
                vn.y += normal.y;
                vn.z += normal.z;
            }
        }
    }

    for (size_t vi = 0; vi < _normals->normals.size(); vi++)
        _normals->normals[vi].normalize();

    // Copy face mesh to normals mesh
    _normals->faceNormals = _faces;

    return true;
}

// Build mesh from 'Mesh' data object:
//   DWORD nVertices; Vector vertices[nVertices]; DWORD nFaces; MeshFace faces[nFaces]; [...]
void Mesh::parseMesh(const DataObject& obj)
{
    clear();
    _vertices.clear();
    _faces.clear();
    _name = obj.name;

    NumberReader r(obj.numbers);

    unsigned int nVertices = r.nextUInt();
    readVectors(r, nVertices, _vertices);
    if (nVertices != _vertices.size())
        OSG_WARN << "DirectX loader: Error reading vertices of mesh '" << _name << "'; " << _vertices.size() << " instead of " << nVertices << endl;

    unsigned int nFaces = r.nextUInt();
    readFaces(r, nFaces, _faces);
    if (nFaces != _faces.size())
        OSG_WARN << "DirectX loader: Error reading faces of mesh '" << _name << "'; " << _faces.size() << " instead of " << nFaces << endl;

    for (size_t i = 0; i < obj.children.size(); ++i) {
        const DataObject& child = *obj.children[i];
        if (child.isType("MeshNormals"))
            parseMeshNormals(child);
        else if (child.isType("MeshTextureCoords"))
            parseMeshTexCoords(child);
        else if (child.isType("MeshMaterialList"))
            parseMeshMaterialList(child);
        else
            OSG_DEBUG << "DirectX loader: Mesh '" << _name << "': ignoring " << (child.isReference ? "reference" : child.type) << endl;
    }
}

// Parse 'MeshMaterialList':
//   DWORD nMaterials; DWORD nFaceIndexes; DWORD faceIndexes[nFaceIndexes]; [Material]
void Mesh::parseMeshMaterialList(const DataObject& obj)
{
    if (_materialList)
        delete _materialList;
    _materialList = new MeshMaterialList;

    NumberReader r(obj.numbers);
    unsigned int nMaterials = r.nextUInt();
    unsigned int nFaceIndices = r.nextUInt();
    size_t n = nFaceIndices;
    if (n > r.remaining())
        n = r.remaining();
    _materialList->faceIndices.resize(n);
    for (size_t i = 0; i < n; ++i)
        _materialList->faceIndices[i] = r.nextUInt();

    // Materials, inline or referenced by name ("{ name }")
    for (size_t i = 0; i < obj.children.size(); ++i) {
        const DataObject& child = *obj.children[i];
        if (child.isReference) {
            Material mm;
            const Material* found = _obj->findMaterial(child.name);
            if (found)
                mm = *found;
            else {
                OSG_WARN << "DirectX loader: Material '" << child.name << "' not found" << endl;
                initDefaultMaterial(mm);
                mm.name = child.name;
            }
            _materialList->material.push_back(mm);
        }
        else if (child.isType("Material")) {
            Material mm;
            parseMaterial(child, mm);
            _materialList->material.push_back(mm);
        }
    }

    if (nMaterials != _materialList->material.size())
        OSG_WARN << "DirectX loader: Error reading material list of mesh '" << _name << "'; " << _materialList->material.size() << " instead of " << nMaterials << endl;

    // A shorter list (often a single index) applies its last entry to the remaining faces
    if (_materialList->faceIndices.size() != _faces.size()) {
        if (_materialList->faceIndices.size() != 1)
            OSG_INFO << "DirectX loader: Mesh '" << _name << "' has " << _materialList->faceIndices.size() << " material indices for " << _faces.size() << " faces" << endl;
        unsigned int last = _materialList->faceIndices.empty() ? 0 : _materialList->faceIndices.back();
        _materialList->faceIndices.resize(_faces.size(), last);
    }
}

// Parse 'MeshNormals':
//   DWORD nNormals; Vector normals[nNormals]; DWORD nFaceNormals; MeshFace faceNormals[nFaceNormals];
void Mesh::parseMeshNormals(const DataObject& obj)
{
    if (_normals)
        delete _normals;
    _normals = new MeshNormals;

    NumberReader r(obj.numbers);
    unsigned int nNormals = r.nextUInt();
    readVectors(r, nNormals, _normals->normals);
    if (nNormals != _normals->normals.size())
        OSG_WARN << "DirectX loader: Error reading normals of mesh '" << _name << "'; " << _normals->normals.size() << " instead of " << nNormals << endl;

    for (size_t i = 0; i < _normals->normals.size(); i++)
        _normals->normals[i].normalize();

    unsigned int nFaceNormals = r.nextUInt();
    readFaces(r, nFaceNormals, _normals->faceNormals);
    if (nFaceNormals != _normals->faceNormals.size())
        OSG_WARN << "DirectX loader: Error reading face normals of mesh '" << _name << "'; " << _normals->faceNormals.size() << " instead of " << nFaceNormals << endl;

    // Validate against the faces; discard (and later regenerate) unusable normals
    bool valid = (_normals->faceNormals.size() == _faces.size());
    const size_t numNormals = _normals->normals.size();
    for (size_t fi = 0; valid && fi < _faces.size(); ++fi) {
        const MeshFace& fn = _normals->faceNormals[fi];
        if (fn.size() != _faces[fi].size()) {
            valid = false;
            break;
        }
        for (size_t j = 0; j < fn.size(); ++j) {
            if (fn[j] >= numNormals) {
                valid = false;
                break;
            }
        }
    }
    if (!valid) {
        OSG_WARN << "DirectX loader: Normals of mesh '" << _name << "' don't match its faces; generating normals" << endl;
        delete _normals;
        _normals = 0;
    }
}

// Parse 'MeshTextureCoords':
//   DWORD nTextureCoords; Coords2d textureCoords[nTextureCoords];
void Mesh::parseMeshTexCoords(const DataObject& obj)
{
    if (_textureCoords)
        delete _textureCoords;
    _textureCoords = new MeshTextureCoords;

    NumberReader r(obj.numbers);
    unsigned int nTextureCoords = r.nextUInt();
    size_t n = nTextureCoords;
    if (n > r.remaining() / 2)
        n = r.remaining() / 2;
    _textureCoords->resize(n);
    for (size_t i = 0; i < n; ++i) {
        (*_textureCoords)[i].u = (float) r.next();
        (*_textureCoords)[i].v = (float) r.next();
    }

    if (nTextureCoords != _textureCoords->size())
        OSG_WARN << "DirectX loader: Error reading texcoords of mesh '" << _name << "'; " << _textureCoords->size() << " instead of " << nTextureCoords << endl;

    if (_textureCoords->empty()) {
        delete _textureCoords;
        _textureCoords = 0;
    }
}
