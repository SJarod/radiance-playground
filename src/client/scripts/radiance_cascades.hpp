#pragma once

#include <cmath>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "graphics/buffer.hpp"

#include "engine/scriptable.hpp"

class Device;

class RadianceCascades : public ScriptableABC
{
  public:
    struct init_data
    {
        std::weak_ptr<Device> device;
        uint32_t frameInFlightCount;
    };

  private:
    const int m_maxCascadeCount = 3;
    // p = probe count is a square number
    const int m_maxProbeCount = std::pow(2.f, 8.f);
    // q = discrete value count will be doubled every cascade
    // number of radiance intervals for the probes from first cascade
    const int m_minDiscreteValueCount = 8;
    // dw = radiance interval length will be doubled every cascade
    const float m_minRadianceintervalLength = 20.f / float(m_maxProbeCount);

    // intensity of every lights (when applying irradiance)
    const float m_lightIntensity = 1.f;

    // iteration along the ray for the raycasting function
    // it is for now uniformly distributed (it may not be precise)
    const int m_maxRayIterationCount = 16;

    struct parameters
    {
        int maxCascadeCount;
        int maxProbeCount;
        int minDiscreteValueCount;
        float minRadianceintervalLength;
        float lightIntensity;
        int maxRayIterationCount;
    };
    std::unique_ptr<Buffer> m_radianceCascadesParametersBuffer;

    struct probe
    {
        glm::vec2 position;
    };

    struct cascade_desc
    {
        // number of probes p
        int p;
        // number of discrete values per probes q
        int q;

        // interval length
        float dw;
    };

    struct cascade
    {
        cascade_desc desc;

        std::vector<probe> probes;

        // total of radiance intervals in the cascade
        // m = pq
        int m;

        cascade() = delete;
        explicit cascade(int probeCount)
        {
            probes.resize(probeCount);
        }
    };

    std::weak_ptr<Device> m_device;

    /**
     * @brief buffer containing the cascades description
     *
     */
    std::unique_ptr<Buffer> m_cascadesDescBuffer;
    /**
     * @brief buffer containing the probe information from the cascade
     *
     */
    std::unique_ptr<Buffer> m_probePositionBuffer;
    /**
     * @brief buffer containing the probe radiance intervals information for write and read
     * It is an array because there are as many as frames in flight.
     *
     */
    std::vector<std::unique_ptr<Buffer>> m_radianceIntervalsStorageBufferRW;

    cascade createCascade(cascade_desc cd) const;
    std::vector<cascade> createCascades(cascade_desc desc0, int cascadeCount) const;

    int getTotalProbeCount(std::vector<cascade> cascades) const;

  public:
    virtual void init(void *userData) override;
    virtual void begin() override;
    virtual void update(float deltaTime) override;

  public:
    [[nodisacrd]] inline const int getCascadeCount() const
    {
        return m_maxCascadeCount;
    }
    [[nodiscard]] inline const Buffer *getParametersBufferHandle() const
    {
        return m_radianceCascadesParametersBuffer.get();
    }
    [[nodiscard]] inline const Buffer *getCascadesDescBufferHandle() const
    {
        return m_cascadesDescBuffer.get();
    }
    [[nodiscard]] inline const Buffer *getProbePositionsBufferHandle() const
    {
        return m_probePositionBuffer.get();
    }
    [[nodiscard]] inline const Buffer *getRadianceIntervalsStorageBufferHandle(uint32_t inFlightCount) const
    {
        return m_radianceIntervalsStorageBufferRW[inFlightCount].get();
    }
};