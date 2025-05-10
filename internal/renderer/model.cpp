
#include <filesystem>
#include <iostream>
#include <unordered_map>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>

#include "mesh.hpp"
#include "model.hpp"
#include "texture.hpp"

Model::~Model()
{
    std::cout << "Destroying model " << m_name << std::endl;
    for (int i = 0; i < m_meshes.size(); ++i)
        std::cout << "\t" << m_meshes[i]->getName() << std::endl;
}

void ModelBuilder::setMesh(const std::shared_ptr<Mesh> &mesh, uint32_t meshIndex)
{
    m_meshes.resize(meshIndex + 1u);
    m_meshes[meshIndex] = mesh;
}

void ModelBuilder::setName(const std::string &name)
{
    m_product->setName(name);
}

std::unique_ptr<Model> ModelBuilder::build()
{
    if (m_bLoadFromFile)
    {
        std::filesystem::path scenePath = m_modelFilename;
        scenePath.remove_filename();

        Assimp::Importer importer;
        const aiScene *pScene = importer.ReadFile(m_modelFilename, m_importerFlags);
        if (!pScene)
        {
            std::cerr << "Failed to load model : " << m_modelFilename << std::endl;
            return nullptr;
        }

        TextureDirector textureDirector;

        std::unordered_map<std::filesystem::path, std::shared_ptr<Texture>> loadedTextures;

        m_product->m_meshes.reserve(pScene->mNumMeshes);
        for (uint32_t i = 0u; i < pScene->mNumMeshes; i++)
        {
            const aiMesh *pMesh = pScene->mMeshes[i];

            MeshBuilder meshBuilder;
            meshBuilder.setDevice(m_device);
            meshBuilder.setVerticesFromAiMesh(pMesh);
            meshBuilder.setIndicesFromAiMesh(pMesh);

            std::shared_ptr<Mesh> mesh = meshBuilder.buildAndRestart();

            const uint32_t matIndex = pMesh->mMaterialIndex;
            const aiMaterial *pMaterial = pScene->mMaterials[matIndex];

            aiString sTexture;
            if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &sTexture) == aiReturn_SUCCESS)
            {
                std::shared_ptr<Texture> meshTexture;

                const std::filesystem::path texturePath = scenePath / sTexture.C_Str();

                auto loadedTextureIt = loadedTextures.find(texturePath);
                if (loadedTextureIt != loadedTextures.end())
                {
                    meshTexture = loadedTextureIt->second;
                }
                else
                {
                    TextureBuilder textureBuilder;
                    textureDirector.configureSRGBTextureBuilder(textureBuilder);
                    textureBuilder.setDevice(m_device);
                    textureBuilder.setTextureFilename(texturePath.string());

                    meshTexture = textureBuilder.buildAndRestart();
                    loadedTextures.insert({texturePath, meshTexture});
                }

                mesh->setTexture(meshTexture);
            }

            m_meshes.push_back(mesh);
        }
    }

    assert(m_meshes.size() > 0u);
    m_product->m_meshes = m_meshes;
    return std::move(m_product);
}