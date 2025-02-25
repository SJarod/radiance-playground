#pragma once

class ScriptableABC
{
  public:
    ScriptableABC() = default;
    virtual ~ScriptableABC() = default;

    ScriptableABC(const ScriptableABC &) = delete;
    ScriptableABC &operator=(const ScriptableABC &) = delete;
    ScriptableABC(ScriptableABC &&) = delete;
    ScriptableABC &operator=(ScriptableABC &&) = delete;

    virtual void init(void *userData) = 0;
    virtual void begin() = 0;
    virtual void update(float deltaTime) = 0;
};