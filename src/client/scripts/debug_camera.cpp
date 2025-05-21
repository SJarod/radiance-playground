#include <iostream>

#include "engine/camera.hpp"

#include "input_manager.hpp"

#include "debug_camera.hpp"

static std::vector<Transform> stamps;
static std::vector<Transform> stamps2;
static float tt = 0.f;

void DebugCamera::init(void *userData)
{
    m_mainCamera = &((UserDataT *)userData)->camera;
}
void DebugCamera::begin()
{
    // Transform t;
    // t.position = glm::vec3(-2.468218, -0.038910, -0.676446);
    // t.rotation = glm::quat(glm::vec3(0.215371, -1.329704, -0.209325));
    // t.scale = glm::vec3(1.000000, 1.000000, 1.000000);
    // m_mainCamera->setTransform(t);

    stamps.push_back({});
    stamps.back().position = glm::vec3(31.617231, -4.756382, -4.277650);
    stamps.back().rotation = glm::quat(glm::vec3(2.956373, 0.957362, 2.989147));
    stamps.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps.push_back({});
    stamps.back().position = glm::vec3(27.734350, -4.261373, 0.794268);
    stamps.back().rotation = glm::quat(glm::vec3(0.647657, 1.527912, 0.646779));
    stamps.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps.push_back({});
    stamps.back().position = glm::vec3(8.207860, -6.325625, 0.577195);
    stamps.back().rotation = glm::quat(glm::vec3(1.435162, 1.384061, 1.434272));
    stamps.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps.push_back({});
    stamps.back().position = glm::vec3(-18.918198, -5.491159, -1.953288);
    stamps.back().rotation = glm::quat(glm::vec3(0.260427, 0.997265, 0.220109));
    stamps.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps.push_back({});
    stamps.back().position = glm::vec3(-32.017586, -4.258978, 2.326329);
    stamps.back().rotation = glm::quat(glm::vec3(0.074250, -1.139608, -0.067457));
    stamps.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps.push_back({});
    stamps.back().position = glm::vec3(-31.379313, -7.132545, 12.921016);
    stamps.back().rotation = glm::quat(glm::vec3(2.253082, -1.245067, -2.279636));
    stamps.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps.push_back({});
    stamps.back().position = glm::vec3(13.424729, -2.869305, 12.923481);
    stamps.back().rotation = glm::quat(glm::vec3(-2.934986, -1.509651, 2.956618));
    stamps.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps.push_back({});
    stamps.back().position = glm::vec3(35.414165, -6.276485, 15.152065);
    stamps.back().rotation = glm::quat(glm::vec3(2.924095, 0.799213, 3.011589));
    stamps.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps.push_back({});
    stamps.back().position = glm::vec3(27.473539, -2.556665, 0.807204);
    stamps.back().rotation = glm::quat(glm::vec3(-1.545888, 1.081754, -1.542559));
    stamps.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps.push_back({});
    stamps.back().position = glm::vec3(1.347844, 0.212840, 5.305541);
    stamps.back().rotation = glm::quat(glm::vec3(-2.230593, -0.331115, 2.744861));
    stamps.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps.push_back({});
    stamps.back().position = glm::vec3(-10.418525, -11.959794, 1.441303);
    stamps.back().rotation = glm::quat(glm::vec3(-2.203079, -1.133249, 2.250952));
    stamps.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps2.push_back({});
    stamps2.back().position = glm::vec3(-35.699215, -45.304783, -0.069762);
    stamps2.back().rotation = glm::quat(glm::vec3(-0.213547, -0.343759, 0.073212));
    stamps2.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps2.push_back({});
    stamps2.back().position = glm::vec3(1.753815, -25.288198, -0.037544);
    stamps2.back().rotation = glm::quat(glm::vec3(1.590034, -1.071702, -1.592418));
    stamps2.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps2.push_back({});
    stamps2.back().position = glm::vec3(15.233271, -15.667577, -0.127971);
    stamps2.back().rotation = glm::quat(glm::vec3(-0.722714, -1.466266, 0.719906));
    stamps2.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps2.push_back({});
    stamps2.back().position = glm::vec3(33.628437, -14.381637, -3.174222);
    stamps2.back().rotation = glm::quat(glm::vec3(0.018410, 0.919407, 0.014701));
    stamps2.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps2.push_back({});
    stamps2.back().position = glm::vec3(32.027431, -14.366851, 14.297418);
    stamps2.back().rotation = glm::quat(glm::vec3(3.124270, 1.207161, 3.125463));
    stamps2.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps2.push_back({});
    stamps2.back().position = glm::vec3(1.261750, -14.896058, 15.019718);
    stamps2.back().rotation = glm::quat(glm::vec3(-3.074445, 0.756039, -3.095432));
    stamps2.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);

    stamps2.push_back({});
    stamps2.back().position = glm::vec3(-30.764833, -15.735815, 11.658937);
    stamps2.back().rotation = glm::quat(glm::vec3(3.075465, -0.991933, -3.086156));
    stamps2.back().scale = glm::vec3(1.000000, 1.000000, 1.000000);
}

void DebugCamera::update(float deltaTime)
{
    if (InputManager::GetKeyUp(Keycode::F1))
    {
        Transform t = m_mainCamera->getTransform();
        auto e = glm::eulerAngles(t.rotation);
        printf("-------------- dump camera position ------------------------\n");
        printf("position : %f, %f, %f\n", t.position.x, t.position.y, t.position.z);
        printf("rotation : %f, %f, %f\n", e.x, e.y, e.z);
        printf("scale : %f, %f, %f\n", t.scale.x, t.scale.y, t.scale.z);
    }

    if (InputManager::GetKey(Keycode::NUM_1))
    {
        tt += deltaTime * 0.025f;
        tt = std::min(tt, 1.f);

        int i = (int)((float)stamps.size() * tt);
        int ii = std::min(i + 1, (int)stamps.size() - 1);

        Transform t;
        t.position = glm::mix(stamps[i].position, stamps[ii].position, tt * stamps.size() - i);
        t.rotation = glm::slerp(stamps[i].rotation, stamps[ii].rotation, tt * stamps.size() - i);
        t.scale = glm::vec3(1.000000, 1.000000, 1.000000);
        m_mainCamera->setTransform(t);
    }
    else if (InputManager::GetKey(Keycode::NUM_2))
    {
        tt += deltaTime * 0.03f;
        tt = std::min(tt, 1.f);

        int i = (int)((float)stamps2.size() * tt);
        int ii = std::min(i + 1, (int)stamps2.size() - 1);

        Transform t;
        t.position = glm::mix(stamps2[i].position, stamps2[ii].position, tt * stamps2.size() - i);
        t.rotation = glm::slerp(stamps2[i].rotation, stamps2[ii].rotation, tt * stamps2.size() - i);
        t.scale = glm::vec3(1.000000, 1.000000, 1.000000);
        m_mainCamera->setTransform(t);
    }
    else
    {
        tt = 0.f;
    }
}
