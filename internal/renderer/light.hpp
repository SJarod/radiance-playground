#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "engine/vertex.hpp"
#include "graphics/buffer.hpp"

class Light
{
	public:
		virtual ~Light() = default;

		glm::vec3 diffuseColor;
		float diffusePower;
		glm::vec3 specularColor;
		float specularPower;
};

class PointLight : public Light
{
	public:
		glm::vec3 position;
		glm::vec3 attenuation = glm::vec3(0.f, 0.f, 1.f);
};

class DirectionalLight : public Light
{
public:
	glm::vec3 direction;
};
