#pragma once

#include <memory>
#include <string>

#include "engine/transform.hpp"

class Mesh;
class MeshBuilder;

class Model 
{
    friend class ModelBuilder;
private:
	std::shared_ptr<Mesh> m_mesh;
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
    [[nodiscard]] const std::shared_ptr<Mesh>& getMesh() const 
    {
        return m_mesh;
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
    std::shared_ptr<Mesh> m_mesh;
    
    void restart()
    {
        m_product = std::unique_ptr<Model>(new Model);
    }

public:
    ModelBuilder() 
    { 
        restart();
    }

    void setMesh(const std::shared_ptr<Mesh> &mesh);
    void setName(const std::string& name);
    std::unique_ptr<Model> build();
};