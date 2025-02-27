#pragma once

#include <memory>

class RenderGraph;
class SwapChain;
class Device;

/**
 * @brief manages the swapchain
 *
 */
class Renderer
{
  private:
  
  // TODO : rename, frames in flight
  int m_bufferingType = 2;
  
  public:
  std::weak_ptr<Device> m_device;
  const SwapChain *m_swapchain;
    std::unique_ptr<RenderGraph> m_renderGraph;

    uint32_t acquireNextSwapChainImage();
    void presentBackBuffer(uint32_t imageIndex);
};