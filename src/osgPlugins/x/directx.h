// -*-c++-*-

/*
 * $Id$
 *
 * Loader for DirectX .x files.
 * Copyright (c)2002 Ulrich Hertlein <u.hertlein@sandbox.de>
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

#ifndef _DIRECTX_H_
#define _DIRECTX_H_

#include <string>
#include <vector>
#include <map>
#include <set>
#include <istream>

#include <osg/Export>
#include <osg/Math>

#include "types.h"
#include "mesh.h"

namespace DX {

    class DataObject;

    /// Initialize material to opaque white without textures.
    void initDefaultMaterial(Material& material);

    /// Build material from a 'Material' data object.
    void parseMaterial(const DataObject& obj, Material& material);

    /**
     * Frame (node of the transform hierarchy).
     */
    class Frame {
    public:
        Frame() : hasTransform(false) {}
        ~Frame() {
            for (unsigned int i = 0; i < frames.size(); ++i)
                delete frames[i];
        }

        std::string name;

        /// True if the frame has a 'FrameTransformMatrix'.
        bool hasTransform;
        Matrix4x4 transform;

        /// Meshes in this frame (owned by Object, may be shared).
        std::vector<Mesh*> meshes;

        /// Child frames (owned).
        std::vector<Frame*> frames;

    private:
        Frame(const Frame&);
        Frame& operator=(const Frame&);
    };

    /**
     * DirectX object.
     */
    class Object {
    public:
        /// Constructor.
        Object();

        /// Destructor.
        virtual ~Object() {
            clear();
        }

        /**
         * Load model from stream (text, binary or compressed .x file).
         * Discards old data. The stream should be opened in binary mode.
         * @return false if the model could not be loaded, else true.
         */
        bool load(std::istream& fin);

        /// Load model from the complete file contents.
        bool load(const std::string& data);

        /// Error message if load() failed.
        const std::string& getError() const {
            return _error;
        }

        /// Non-fatal problems encountered by load().
        const std::string& getWarning() const {
            return _warning;
        }

        /**
         * Generate per-vertex normals for the entire model.
         * Discards any previously loaded or generated normals.
         * @param creaseAngle TODO: The angle above which two adjacent faces are no
         * longer considered to belong to a common surface.
         * @return false if an error occurred, else true.
         */
        bool generateNormals(float creaseAngle = 80.0f);

        /// Get number of meshes.
        inline unsigned int getNumMeshes() const {
            return _meshes.size();
        }

        /// Get Mesh.
        inline Mesh* getMesh(unsigned int i) {
            return _meshes[i];
        }
        inline const Mesh* getMesh(unsigned int i) const {
            return _meshes[i];
        }

        /**
         * Get root of the frame hierarchy. Holds top-level meshes and frames;
         * the root itself has no name and no transform.
         */
        const Frame& getRootFrame() const {
            return _root;
        }

        /// Find named material.
        const Material * findMaterial(const std::string & name) const;

    private:
        /// All named materials in the file.
        std::vector<Material> _globalMaterials;

        /// Meshes (owned).
        std::vector<Mesh*> _meshes;

        /// Frame hierarchy.
        Frame _root;

        std::string _error;
        std::string _warning;

        // Used while building
        std::multimap<std::string, const DataObject*> _named;
        std::set<std::string> _frameReferences;
        std::map<const DataObject*, Mesh*> _meshForObject;

        /// Clear object.
        void clear();

        void collectNames(const DataObject& obj, bool insideFrame);
        const DataObject* findNamed(const std::string& name, const char* type) const;
        Mesh* getMesh(const DataObject& obj);
        void addToFrame(Frame& frame, const DataObject& obj, unsigned int depth);
        void buildFrame(Frame& frame, const DataObject& obj, unsigned int depth);
    };
} // namespace

#endif
