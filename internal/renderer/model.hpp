#pragma once

#include <memory>
#include <string>

#include "transform.hpp"

class Mesh;

class Model 
{
private:
	std::shared_ptr<Mesh> m_mesh;
	Transform m_transform;
    std::string m_name;

public:
    [[nodiscard]] const Transform& getTransform() const
    {
        return m_transform;
    }
    [[nodiscard]] const std::string& getName() const
    {
        return m_name;
    }
    [[nodiscard]] const & getMesh() const
    {
        return m_mesh;
    }

public:
    void Model::setTransform(const Transform& transform)
    {
        m_transform = transform;
    }

    void Model::setName(const std::string& name)
    {
        m_name = name;
    }

    void Model::setMesh(const std::shared_ptr<Mesh>)
    {
        m_transform = transform;
    }
};