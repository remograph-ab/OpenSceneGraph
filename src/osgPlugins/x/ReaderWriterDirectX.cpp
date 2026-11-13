// -*-c++-*-

/*
 * $Id$
 *
 * DirectX file converter for OpenSceneGraph.
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

#include "directx.h"

#include <osg/TexEnv>
#include <osg/CullFace>

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Material>
#include <osg/MatrixTransform>
#include <osg/Image>
#include <osg/Texture2D>

#include <osg/Notify>
#include <osgDB/Registry>
#include <osgDB/ReadFile>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/fstream>

#include <map>
#include <set>
#include <iostream>


/**
 * OpenSceneGraph plugin wrapper/converter.
 *
 * Supports text ('txt '), binary ('bin ') and MSZIP compressed ('tzip',
 * 'bzip') .x files. The frame hierarchy is converted to osg::MatrixTransform
 * nodes (osg::Group for frames without a transform), meshes to osg::Geode.
 */
class ReaderWriterDirectX : public osgDB::ReaderWriter
{
public:
    ReaderWriterDirectX()
    {
        supportsExtension("x","DirectX scene format");
        supportsOption("flipTexture", "flip texture upside-down");
        // made hand switching an option - .x models from XSI's export are right-handed already
        supportsOption("rightHanded", "prevents reader from switching handedness for right handed files");
        supportsOption("leftHanded", "reader switches handedness for left handed files");
    }

    virtual const char* className() const {
        return "DirectX Reader";
    }

    virtual ReadResult readObject(const std::string& fileName, const osgDB::ReaderWriter::Options* options) const
    {
        return readNode(fileName, options);
    }

    virtual ReadResult readNode(const std::string& fileName, const osgDB::ReaderWriter::Options* options) const;

    virtual ReadResult readObject(std::istream& fin, const Options* options) const
    {
        return readNode(fin,options);
    }

    virtual ReadResult readNode(std::istream& fin, const osgDB::ReaderWriter::Options* options) const;

private:
    struct ConvertContext {
        bool switchToLeftHanded;
        bool flipTexture;
        float creaseAngle;
        const osgDB::ReaderWriter::Options* options;

        /// Geodes of meshes already converted (meshes may be shared by several frames).
        std::map<const DX::Mesh*, osg::ref_ptr<osg::Geode> > geodes;

        /// Textures shared between all meshes.
        std::map<std::string, osg::ref_ptr<osg::Texture2D> > textures;

        /// Texture images that could not be read (not retried).
        std::set<std::string> missingTextures;

        unsigned int numDrawables;
    };

    osg::Group * convertFromDX(DX::Object & obj, ConvertContext& context) const;

    void convertFrame(const DX::Frame & frame, osg::Group & group, ConvertContext& context) const;

    osg::Geode * convertFromDX(DX::Mesh & mesh, ConvertContext& context) const;

    osg::StateSet * convertMaterial(const DX::Material & mtl, ConvertContext& context) const;
};

// Register with Registry to instantiate the above reader/writer.
REGISTER_OSGPLUGIN(x, ReaderWriterDirectX)


// Read node
osgDB::ReaderWriter::ReadResult ReaderWriterDirectX::readNode(const std::string& file, const osgDB::ReaderWriter::Options* options) const
{
    std::string ext = osgDB::getLowerCaseFileExtension(file);
    if (!acceptsExtension(ext)) return ReadResult::FILE_NOT_HANDLED;

    std::string fileName = osgDB::findDataFile( file, options );
    if (fileName.empty()) return ReadResult::FILE_NOT_FOUND;

    OSG_INFO << "ReaderWriterDirectX::readNode(" << fileName << ")\n";

    // Binary mode: binary and compressed .x files must not be subject to newline translation
    osgDB::ifstream fin(fileName.c_str(), std::ios::in | std::ios::binary);
    if (!fin) {
        OSG_WARN << "ReaderWriterDirectX failed to read '" << fileName.c_str() << "'\n";
        return ReadResult::ERROR_IN_READING_FILE;
    }

    // code for setting up the database path so that internally referenced file are searched for on relative paths.
    osg::ref_ptr<Options> local_opt = options ? static_cast<Options*>(options->clone(osg::CopyOp::SHALLOW_COPY)) : new Options;
    local_opt->setDatabasePath(osgDB::getFilePath(fileName));

    return readNode(fin, local_opt.get());
}

osgDB::ReaderWriter::ReadResult ReaderWriterDirectX::readNode(std::istream& fin, const osgDB::ReaderWriter::Options* options) const
{
    DX::Object obj;
    if (obj.load(fin) == false) {
        std::string msg = "DirectX loader: " + obj.getError();
        OSG_WARN << msg << std::endl;
        return ReadResult(msg);
    }
    if (!obj.getWarning().empty()) {
        OSG_WARN << "DirectX loader: " << obj.getWarning() << std::endl;
    }

    // Options?
    ConvertContext context;
    context.flipTexture = true;
    context.switchToLeftHanded = true; // when true: swap y and z for incoming files
    context.creaseAngle = 80.0f;
    context.options = options;
    context.numDrawables = 0;
    if (options) {
        const std::string option = options->getOptionString();
        if (option.find("rightHanded") != std::string::npos) {
            context.switchToLeftHanded = false;
        }
        if (option.find("leftHanded") != std::string::npos) {
            context.switchToLeftHanded = true;
        }
        if (option.find("flipTexture") != std::string::npos) {
            context.flipTexture = false;
        }
        if (option.find("creaseAngle") != std::string::npos) {
            // TODO
        }
    }

    // Convert to osg::Group
    osg::ref_ptr<osg::Group> group = convertFromDX(obj, context);
    if (!group || context.numDrawables == 0) {
        std::string msg = "DirectX loader: No geometry found";
        if (!obj.getWarning().empty())
            msg += " (" + obj.getWarning() + ")";
        OSG_WARN << msg << std::endl;
        return ReadResult(msg);
    }

    return group.release();
}


// Convert DirectX object
osg::Group * ReaderWriterDirectX::convertFromDX(DX::Object & obj, ConvertContext& context) const
{
    osg::ref_ptr<osg::Group> group = new osg::Group;
    convertFrame(obj.getRootFrame(), *group, context);
    return group.release();
}

// Convert frame contents (meshes and child frames) into children of 'group'
void ReaderWriterDirectX::convertFrame(const DX::Frame & frame, osg::Group & group, ConvertContext& context) const
{
    for (unsigned int i = 0; i < frame.meshes.size(); ++i) {
        DX::Mesh* mesh = frame.meshes[i];
        osg::ref_ptr<osg::Geode>& geode = context.geodes[mesh];
        if (!geode.valid()) {
            geode = convertFromDX(*mesh, context);
            if (!geode.valid())
                continue;
        }
        group.addChild(geode.get());
    }

    for (unsigned int i = 0; i < frame.frames.size(); ++i) {
        const DX::Frame& child = *frame.frames[i];

        osg::ref_ptr<osg::Group> childGroup;
        if (child.hasTransform) {
            // DirectX matrices are row-major for row vectors, as osg::Matrix
            osg::Matrixd m(child.transform.m);
            if (context.switchToLeftHanded) {
                // Same y/z swap as for vertices: M' = P * M * P
                osg::Matrixd swapped;
                const int p[4] = { 0, 2, 1, 3 };
                for (int r = 0; r < 4; ++r)
                    for (int c = 0; c < 4; ++c)
                        swapped(r, c) = m(p[r], p[c]);
                m = swapped;
            }
            childGroup = new osg::MatrixTransform(m);
        }
        else {
            childGroup = new osg::Group;
        }
        childGroup->setName(child.name);

        convertFrame(child, *childGroup, context);

        // Skip frames without content (e.g. bones or attach points)
        if (childGroup->getNumChildren() > 0)
            group.addChild(childGroup.get());
    }
}

// Convert DirectX material to state set
osg::StateSet * ReaderWriterDirectX::convertMaterial(const DX::Material & mtl, ConvertContext& context) const
{
    osg::StateSet* state = new osg::StateSet;

    // Material
    osg::Material* material = new osg::Material;
    material->setName(mtl.name);
    state->setAttributeAndModes(material);

    float alpha = mtl.faceColor.alpha;
    osg::Vec4 ambient(mtl.faceColor.red,
                      mtl.faceColor.green,
                      mtl.faceColor.blue,
                      alpha);
    material->setAmbient(osg::Material::FRONT, ambient);
    material->setDiffuse(osg::Material::FRONT, ambient);

    material->setShininess(osg::Material::FRONT, osg::clampBetween(mtl.power, 0.0f, 128.0f));

    osg::Vec4 specular(mtl.specularColor.red,
                       mtl.specularColor.green,
                       mtl.specularColor.blue, alpha);
    material->setSpecular(osg::Material::FRONT, specular);

    osg::Vec4 emissive(mtl.emissiveColor.red,
                       mtl.emissiveColor.green,
                       mtl.emissiveColor.blue, alpha);
    material->setEmission(osg::Material::FRONT, emissive);

    // Transparency? Set render hint & blending
    if (alpha < 1.0f) {
        state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        state->setMode(GL_BLEND, osg::StateAttribute::ON);
    }
    else
        state->setMode(GL_BLEND, osg::StateAttribute::OFF);

    unsigned int textureCount = mtl.texture.size();
    for (unsigned int j = 0; j < textureCount; j++) {

        // Share image/texture pairs
        if (context.missingTextures.count(mtl.texture[j]))
            continue;
        osg::ref_ptr<osg::Texture2D>& texture = context.textures[mtl.texture[j]];
        if (!texture.valid()) {
            osg::ref_ptr<osg::Image> image = osgDB::readRefImageFile(mtl.texture[j], context.options);
            if (!image) {
                // osgDB reports the failure itself
                context.missingTextures.insert(mtl.texture[j]);
                continue;
            }

            // Texture
            texture = new osg::Texture2D;
            texture->setImage(image.get());
            texture->setWrap(osg::Texture2D::WRAP_S, osg::Texture2D::REPEAT);
            texture->setWrap(osg::Texture2D::WRAP_T, osg::Texture2D::REPEAT);
        }
        state->setTextureAttributeAndModes(j, texture.get());
    }

    return state;
}

// Convert DirectX mesh to osg::Geode
osg::Geode* ReaderWriterDirectX::convertFromDX(DX::Mesh & mesh, ConvertContext& context) const
{
    const std::vector<DX::MeshFace> & faces = mesh.getFaces();
    const std::vector<DX::Vector> & vertices = mesh.getVertices();
    if (faces.empty() || vertices.empty())
        return NULL;

    const DX::MeshNormals* meshNormals = mesh.getMeshNormals();
    if (!meshNormals) {
        mesh.generateNormals(context.creaseAngle);
        meshNormals = mesh.getMeshNormals();
    }
    if (!meshNormals)
        return NULL;

    const DX::MeshTextureCoords* meshTexCoords = mesh.getMeshTextureCoords();

    /*
     * - MeshMaterialList contains a list of Material and a per-face
     *   information with Material is to be applied to which face.
     * - Mesh contains a list of Vertices and a per-face information
     *   which vertices (three or four) belong to this face.
     * - MeshNormals contains a list of Normals and a per-face information
     *   which normal is used by which vertex.
     * - MeshTextureCoords contains a list of per-vertex texture coordinates.
     *
     * - Uses left-hand CS with Y-up, Z-into
     *   obj_x -> osg_x
     *   obj_y -> osg_z
     *   obj_z -> osg_y
     *
     * - aa: Changed always change left to right hand to an option that allows
     *   us to read right-handed models as-is.  Our modeler is using XSI, which
     *   exports to right-handed system.
     *
     * - Polys are CW oriented
     */

    // Materials; meshes without material list (or with invalid indices) use a default material
    std::vector<DX::Material> materials;
    std::vector<unsigned int> faceMaterials(faces.size(), 0);
    const DX::MeshMaterialList* meshMaterial = mesh.getMeshMaterialList();
    if (meshMaterial)
        materials = meshMaterial->material;
    unsigned int defaultMaterialIndex = materials.size();
    bool needDefaultMaterial = false;
    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned int mi = (meshMaterial && i < meshMaterial->faceIndices.size()) ? meshMaterial->faceIndices[i] : defaultMaterialIndex;
        if (mi >= defaultMaterialIndex) {
            mi = defaultMaterialIndex;
            needDefaultMaterial = true;
        }
        faceMaterials[i] = mi;
    }
    if (needDefaultMaterial) {
        DX::Material mtl;
        DX::initDefaultMaterial(mtl);
        materials.push_back(mtl);
    }

    std::vector<osg::Geometry*> geomList;
    for (unsigned int i = 0; i < materials.size(); i++) {
        const DX::Material& mtl = materials[i];

        // Geometry
        osg::Geometry* geom = new osg::Geometry;
        geomList.push_back(geom);

        geom->setStateSet(convertMaterial(mtl, context));

        // Arrays to hold vertices, normals, and texcoords.
        geom->setVertexArray(new osg::Vec3Array);
        geom->setNormalArray(new osg::Vec3Array, osg::Array::BIND_PER_VERTEX);
        if (meshTexCoords) {
            // All texture units share the same array
            osg::Vec2Array* texCoords = new osg::Vec2Array;
            unsigned int textureCount = mtl.texture.size() > 0 ? mtl.texture.size() : 1;
            for (unsigned int j = 0; j < textureCount; j++)
                geom->setTexCoordArray(j, texCoords);
        }

        geom->addPrimitiveSet(new osg::DrawArrayLengths(osg::PrimitiveSet::POLYGON));
    }

    const unsigned int numVertices = vertices.size();
    const unsigned int numNormals = meshNormals->normals.size();
    unsigned int skippedFaces = 0;

    // Add faces to Geometry
    for (unsigned int i = 0; i < faces.size(); i++) {

        const DX::MeshFace& face = faces[i];
        const DX::MeshFace* faceNormals = (i < meshNormals->faceNormals.size() ? &meshNormals->faceNormals[i] : NULL);

        // #pts of this face
        unsigned int np = face.size();

        // Skip degenerate faces and faces with invalid indices
        bool valid = (np >= 3);
        for (unsigned int j = 0; valid && j < np; j++) {
            if (face[j] >= numVertices)
                valid = false;
        }
        if (!valid) {
            ++skippedFaces;
            continue;
        }
        bool normalsValid = (faceNormals && faceNormals->size() == np);
        for (unsigned int j = 0; normalsValid && j < np; j++) {
            if ((*faceNormals)[j] >= numNormals)
                normalsValid = false;
        }

        // Geometry for Material
        osg::Geometry* geom = geomList[faceMaterials[i]];
        ((osg::DrawArrayLengths*) geom->getPrimitiveSet(0))->push_back(np);

        osg::Vec3Array* vertexArray = (osg::Vec3Array*) geom->getVertexArray();
        osg::Vec3Array* normalArray = (osg::Vec3Array*) geom->getNormalArray();
        osg::Vec2Array* texCoordArray=NULL; // only make them if the original has them
        if(meshTexCoords) texCoordArray = (osg::Vec2Array*) geom->getTexCoordArray(0);

        // Add vertices, normals, texcoords
        for (unsigned int j = 0; j < np; j++) {

            // Convert CW to CCW order
            unsigned int jj = (j > 0 ? np - j : j);
            if(!context.switchToLeftHanded) jj=j;

            // Vertices
            unsigned int vi = face[jj];
            const DX::Vector & v = vertices[vi];
            if(context.switchToLeftHanded)// Transform Xleft/Yup/Zinto to Xleft/Yinto/Zup
                vertexArray->push_back(osg::Vec3(v.x,v.z,v.y));
            else
                vertexArray->push_back(osg::Vec3(v.x,v.y,v.z));

            // Normals
            if (normalsValid) {
                const DX::Vector& n = meshNormals->normals[(*faceNormals)[jj]];
                if(context.switchToLeftHanded)// Transform Xleft/Yup/Zinto to Xleft/Yinto/Zup
                    normalArray->push_back(osg::Vec3(n.x,n.z,n.y));
                else
                    normalArray->push_back(osg::Vec3(n.x,n.y,n.z));
            }
            else {
                normalArray->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
            }

            // TexCoords
            if (texCoordArray) {
                DX::Coords2d tc = { 0.0f, 0.0f };
                if (vi < meshTexCoords->size())
                    tc = (*meshTexCoords)[vi];
                osg::Vec2 uv;
                if (context.flipTexture){
                    if(context.switchToLeftHanded)
                        uv.set(tc.u, 1.0f - tc.v); // Image is upside down
                    else
                        uv.set(1.0f - tc.u, 1.0f - tc.v); // Image is 180 degrees
                }
                else
                    uv.set(tc.u, tc.v);
                texCoordArray->push_back(uv);
            }
        }
    }

    if (skippedFaces > 0) {
        OSG_WARN << "DirectX loader: Skipped " << skippedFaces << " invalid faces in mesh '" << mesh.getName() << "'" << std::endl;
    }

    // Add non-empty nodes to Geode
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->setName(mesh.getName());
    for (unsigned int i = 0; i < geomList.size(); i++) {
        osg::ref_ptr<osg::Geometry> geom = geomList[i];
        if (((osg::Vec3Array*) geom->getVertexArray())->size())
            geode->addDrawable(geom.get());
    }
    if (geode->getNumDrawables() == 0)
        return NULL;
    context.numDrawables += geode->getNumDrawables();

    // Back-face culling
    osg::StateSet* state = new osg::StateSet;
    geode->setStateSet(state);

    osg::CullFace* cullFace = new osg::CullFace;
    cullFace->setMode(osg::CullFace::BACK);
    state->setAttributeAndModes(cullFace);

    return geode.release();
}
