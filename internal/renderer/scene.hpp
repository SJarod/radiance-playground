#pragma once

#include <memory>
#include <vector>

class Mesh;
class Light;

class Scene
{
  private:
    std::vector<std::shared_ptr<Mesh>> m_objects;
    std::vector<std::shared_ptr<Light>> m_lights;

  public:
    Scene(const std::weak_ptr<Device> device);

  public:
    [[nodiscard]] const std::vector<std::shared_ptr<Mesh>> &getObjects() const
    {
        return m_objects;
    }

    [[nodiscard]] const std::vector<std::shared_ptr<Light>> &getLights() const
    {
        return m_lights;
    }
};