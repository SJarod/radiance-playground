#pragma once

#include <memory>
#include <vector>

class Mesh;
class CameraABC;
class Light;

class SceneABC
{
  protected:
    CameraABC *m_mainCamera;
    std::vector<std::unique_ptr<CameraABC>> m_cameras;

    std::vector<std::shared_ptr<Mesh>> m_objects;
    std::vector<std::shared_ptr<Light>> m_lights;

  public:
    SceneABC() = default;
    virtual ~SceneABC() = default;

    SceneABC(const SceneABC &) = delete;
    SceneABC &operator=(const SceneABC &) = delete;
    SceneABC(SceneABC &&) = delete;
    SceneABC &operator=(SceneABC &&) = delete;

  public:
    [[nodiscard]] const std::vector<std::shared_ptr<Mesh>> &getObjects() const
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
};