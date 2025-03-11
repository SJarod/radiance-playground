
#include <iostream>

#include "model.hpp"
#include "mesh.hpp"

void ModelBuilder::setMesh(const std::shared_ptr<Mesh> &mesh) 
{
	m_mesh = mesh;
}

void ModelBuilder::setName(const std::string& name)
{
	m_product->setName(name);
}

std::unique_ptr<Model> ModelBuilder::build() 
{
	assert(m_mesh);
	m_product->m_mesh = m_mesh;
	return std::move(m_product);
}