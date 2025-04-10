#pragma once

#include <memory>
#include <type_traits>
#include <typeinfo>
#include <vector>

class Model;
class CameraABC;
class Light;
class Skybox;

#include "engine/scriptable.hpp"

class SceneABC
{
  protected:
    CameraABC *m_mainCamera;
    std::vector<std::unique_ptr<CameraABC>> m_cameras;

    std::vector<std::unique_ptr<ScriptableABC>> m_scripts;
    std::vector<std::shared_ptr<Model>> m_objects;
    std::vector<std::shared_ptr<Light>> m_lights;
    std::shared_ptr<Skybox> m_skybox;

  public:
    SceneABC() = default;
    virtual ~SceneABC() = default;

    SceneABC(const SceneABC &) = delete;
    SceneABC &operator=(const SceneABC &) = delete;
    SceneABC(SceneABC &&) = delete;
    SceneABC &operator=(SceneABC &&) = delete;

  public:
    void beginSimulation();
    void updateSimulation(float deltaTime);

  public:
    [[nodiscard]] const std::vector<std::shared_ptr<Model>> &getObjects() const
    {
        return m_objects;
    }

    [[nodiscard]] const std::vector<std::shared_ptr<Light>> &getLights() const
    {
        return m_lights;
    }

    [[nodiscard]] CameraABC *getMainCamera() const
    {
        return m_mainCamera;
    }

    [[nodiscard]] const std::shared_ptr<Skybox> getSkybox() const
    {
        return m_skybox;
    }

    template <typename TType> [[nodiscard]] std::vector<TType *> getReadOnlyInstancedComponents() const
    {
        std::vector<TType *> foundComponents;

        if (std::is_base_of<CameraABC, TType>::value)
        {
            for (int i = 0; i < m_cameras.size(); ++i)
            {
                if (typeid(*m_cameras[i].get()) == typeid(TType))
                {
                    foundComponents.push_back(dynamic_cast<TType *>(m_cameras[i].get()));
                }
            }
        }
        else if (std::is_base_of<ScriptableABC, TType>::value)
        {
            for (int i = 0; i < m_scripts.size(); ++i)
            {
                if (typeid(*m_scripts[i].get()) == typeid(TType))
                {
                    foundComponents.push_back(dynamic_cast<TType *>(m_scripts[i].get()));
                }
            }
        }
        else if (typeid(Model) == typeid(TType))
        {
            for (int i = 0; i < m_objects.size(); ++i)
            {
                foundComponents.push_back(reinterpret_cast<TType *>(m_objects[i].get()));
            }
        }
        else if (typeid(Light) == typeid(TType))
        {
            for (int i = 0; i < m_lights.size(); ++i)
            {
                foundComponents.push_back(reinterpret_cast<TType *>(m_lights[i].get()));
            }
        }

        return foundComponents;
    }
};