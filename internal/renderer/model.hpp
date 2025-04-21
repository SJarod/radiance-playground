#pragma once

#include <memory>
#include <string>

#include "engine/transform.hpp"

class Mesh;
class MeshBuilder;
class Device;

class Model 
{
    friend class ModelBuilder;
private:
	std::vector<std::shared_ptr<Mesh>> m_meshes;
	Transform m_transform;
    std::string m_name = "default";

public:
    [[nodiscard]] const Transform& getTransform() const
    {
        return m_transform;
    }
    [[nodiscard]] const std::string& getName() const
    {
        return m_name;
    }
    [[nodiscard]] const std::shared_ptr<Mesh> getMesh(uint32_t meshIndex = 0u) const 
    {
        if (meshIndex > m_meshes.size())
            return nullptr;

        return m_meshes[meshIndex];
    }

    [[nodiscard]] const std::vector<std::shared_ptr<Mesh>>& getMeshes() const
    {
        return m_meshes;
    }

public:
    void setTransform(const Transform& transform)
    {
        m_transform = transform;
    }

    void setName(const std::string& name)
    {
        m_name = name;
    }
};

class ModelBuilder 
{
private:
    std::unique_ptr<Model> m_product;
    std::vector<std::shared_ptr<Mesh>> m_meshes;
    std::weak_ptr<Device> m_device;

    void restart()
    {
        m_product = std::unique_ptr<Model>(new Model);
    }

    std::string m_modelFilename;
    bool m_bLoadFromFile = false;

    unsigned int m_importerFlags = 0x00000000;

public:
    ModelBuilder() 
    { 
        restart();
    }
    void setDevice(std::weak_ptr<Device> device)
    {
        m_device = device;
    }
    void setModelFilename(const std::string& filename)
    {
        m_modelFilename = filename;
        m_bLoadFromFile = true;
    }
    void setModelImporterFlags(unsigned int flags)
    {
        m_importerFlags = flags;
    }
    void setMesh(const std::shared_ptr<Mesh> &mesh, uint32_t meshIndex = 0u);
    void setName(const std::string& name);
    std::unique_ptr<Model> build();
};