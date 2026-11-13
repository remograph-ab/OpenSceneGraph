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

#include "directx.h"
#include "parser.h"

#include <iostream>
#include <iterator>
#include <string.h>

#include <osg/Notify>

using namespace DX;
using namespace std;


/**********************************************************************
 *
 * DirectX object.
 *
 **********************************************************************/

Object::Object()
{
}

void Object::clear()
{
    for (unsigned int i = 0; i < _meshes.size(); ++i) {
        delete _meshes[i];
    }
    _meshes.clear();
    _globalMaterials.clear();

    for (unsigned int i = 0; i < _root.frames.size(); ++i)
        delete _root.frames[i];
    _root.frames.clear();
    _root.meshes.clear();

    _error.clear();
    _warning.clear();
}

bool Object::load(std::istream& fin)
{
    std::string data((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
    return load(data);
}

bool Object::load(const std::string& data)
{
    clear();

    std::vector<DataObject*> objects;
    if (!parseXFile(data, objects, _error, _warning)) {
        for (size_t i = 0; i < objects.size(); ++i)
            delete objects[i];
        return false;
    }

    // Index named objects and references to frames/meshes from frames
    for (size_t i = 0; i < objects.size(); ++i)
        collectNames(*objects[i], false);

    // Named materials (may be referenced by mesh material lists)
    for (std::multimap<std::string, const DataObject*>::const_iterator itr = _named.begin(); itr != _named.end(); ++itr) {
        if (itr->second->isType("Material")) {
            Material mm;
            parseMaterial(*itr->second, mm);
            _globalMaterials.push_back(mm);
        }
    }

    // Build frame hierarchy. Top-level meshes and frames that are referenced
    // from within a frame are only placed where they are referenced.
    for (size_t i = 0; i < objects.size(); ++i) {
        const DataObject& obj = *objects[i];
        if ((obj.isType("Frame") || obj.isType("Mesh")) &&
            !obj.name.empty() && _frameReferences.count(obj.name))
            continue;
        addToFrame(_root, obj, 0);
    }

    _named.clear();
    _frameReferences.clear();
    _meshForObject.clear();
    for (size_t i = 0; i < objects.size(); ++i)
        delete objects[i];

    return true;
}

void Object::collectNames(const DataObject& obj, bool insideFrame)
{
    if (obj.isReference) {
        if (insideFrame)
            _frameReferences.insert(obj.name);
        return;
    }

    if (!obj.name.empty())
        _named.insert(std::make_pair(obj.name, &obj));

    bool isFrame = obj.isType("Frame");
    for (size_t i = 0; i < obj.children.size(); ++i)
        collectNames(*obj.children[i], isFrame);
}

const DataObject* Object::findNamed(const std::string& name, const char* type) const
{
    std::pair<std::multimap<std::string, const DataObject*>::const_iterator,
              std::multimap<std::string, const DataObject*>::const_iterator> range = _named.equal_range(name);
    for (std::multimap<std::string, const DataObject*>::const_iterator itr = range.first; itr != range.second; ++itr) {
        if (itr->second->isType(type))
            return itr->second;
    }
    return 0;
}

Mesh* Object::getMesh(const DataObject& obj)
{
    // Meshes referenced from several frames are shared
    std::map<const DataObject*, Mesh*>::iterator itr = _meshForObject.find(&obj);
    if (itr != _meshForObject.end())
        return itr->second;

    Mesh* mesh = new Mesh(this);
    mesh->parseMesh(obj);
    _meshes.push_back(mesh);
    _meshForObject[&obj] = mesh;
    OSG_INFO << "DirectX loader: Mesh '" << obj.name << "' " << mesh->getVertices().size() << " vertices, " << mesh->getFaces().size() << " faces" << endl;
    return mesh;
}

void Object::addToFrame(Frame& frame, const DataObject& obj, unsigned int depth)
{
    if (depth > 256) {
        OSG_WARN << "DirectX loader: Frame hierarchy too deep (recursive references?)" << endl;
        return;
    }

    if (obj.isReference) {
        // Reference to a mesh or frame
        const DataObject* target = findNamed(obj.name, "Mesh");
        if (!target)
            target = findNamed(obj.name, "Frame");
        if (target)
            addToFrame(frame, *target, depth + 1);
        else
            OSG_INFO << "DirectX loader: Ignoring reference to '" << obj.name << "'" << endl;
    }
    else if (obj.isType("Mesh")) {
        frame.meshes.push_back(getMesh(obj));
    }
    else if (obj.isType("Frame")) {
        Frame* child = new Frame;
        frame.frames.push_back(child);
        buildFrame(*child, obj, depth + 1);
    }
    else if (obj.isType("FrameTransformMatrix")) {
        if (obj.numbers.size() >= 16 && !frame.hasTransform) {
            frame.hasTransform = true;
            for (unsigned int i = 0; i < 16; ++i)
                frame.transform.m[i] = obj.numbers[i];
        }
    }
    else if (obj.isType("Material") || obj.isType("AnimationSet") || obj.isType("Animation") ||
             obj.isType("AnimTicksPerSecond") || obj.isType("Header")) {
        // Not part of the scene graph (materials are referenced by name)
    }
    else {
        // Unknown container: look for meshes and frames inside it (references inside are ignored)
        for (size_t i = 0; i < obj.children.size(); ++i) {
            if (!obj.children[i]->isReference)
                addToFrame(frame, *obj.children[i], depth + 1);
        }
    }
}

void Object::buildFrame(Frame& frame, const DataObject& obj, unsigned int depth)
{
    frame.name = obj.name;
    for (size_t i = 0; i < obj.children.size(); ++i)
        addToFrame(frame, *obj.children[i], depth);
}

bool Object::generateNormals(float creaseAngle)
{
    bool result = true;
    for (unsigned int i = 0; i < _meshes.size(); ++i) {
        result &= _meshes[i]->generateNormals(creaseAngle);
    }

    return result;
}

const Material * Object::findMaterial(const std::string & name) const
{
    for (std::vector<Material>::const_iterator itr = _globalMaterials.begin(); itr != _globalMaterials.end(); ++itr) {
        if ((*itr).name == name) {
            return &*itr;
        }
    }

    return 0;
}
