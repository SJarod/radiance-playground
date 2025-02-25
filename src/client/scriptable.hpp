#pragma once

class ScriptableABC
{
  public:
    virtual void begin() = 0;
    virtual void update() = 0;
};