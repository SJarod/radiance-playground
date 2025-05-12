#include <tracy/Tracy.hpp>

#include "engine/camera.hpp"
#include "engine/scriptable.hpp"

#include "graphics/buffer.hpp"

#include "renderer/model.hpp"
#include "renderer/render_state.hpp"
#include "renderer/texture.hpp"

#include "scene.hpp"

SceneABC::~SceneABC()
{
    for (int i = 0; i < m_objects.size(); ++i)
    {
        std::cout << m_objects[0]->getName() << " Model use count : " << m_objects[0].use_count() << std::endl;
    }

    if (ModelRenderState::s_defaultDiffuseTexture)
        ModelRenderState::s_defaultDiffuseTexture.reset();
}

void SceneABC::beginSimulation()
{
    for (auto &script : m_scripts)
    {
        script->begin();
    }
}

void SceneABC::updateSimulation(float deltaTime)
{
    ZoneScoped;

    for (auto &script : m_scripts)
    {
        script->update(deltaTime);
    }
}