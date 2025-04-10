#pragma once

#include <utility>

#include "engine/scriptable.hpp"

class DebugCamera : public ScriptableABC
{
  private:
  public:
    void init(void *userData) override;
    void begin() override;
    void update(float deltaTime) override;
};