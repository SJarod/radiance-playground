#include "debug_camera.hpp"

void DebugCamera::init(void *userData)
{
}
void DebugCamera::begin()
{
    Transform t;
    t.position = glm::vec3(-2.468218, -0.038910, -0.676446);
    t.rotation = glm::quat(glm::vec3(0.215371, -1.329704, -0.209325));
    t.scale = glm::vec3(1.000000, 1.000000, 1.000000);
    mainCamera->setTransform(t);

}
void DebugCamera::update(float deltaTime)
{
    if (InputManager::GetKeyUp(Keycode::F1))
    {
        Transform t = mainCamera->getTransform();
        auto e = glm::eulerAngles(t.rotation);
        printf("-------------- dump camera position ------------------------\n");
        printf("position : %f, %f, %f\n", t.position.x, t.position.y, t.position.z);
        printf("rotation : %f, %f, %f\n", e.x, e.y, e.z);
        printf("scale : %f, %f, %f\n", t.scale.x, t.scale.y, t.scale.z);
        break;
    }

}
