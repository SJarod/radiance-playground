#include "engine/scriptable.hpp"

#include "scene.hpp"

void SceneABC::beginSimulation()
{
    for (auto &script : m_scripts)
    {
        script->begin();
    }
}

void SceneABC::updateSimulation(float deltaTime)
{
    for (auto &script : m_scripts)
    {
        script->update(deltaTime);
    }
}