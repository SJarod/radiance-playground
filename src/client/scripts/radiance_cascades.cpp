#include <iostream>

#include "graphics/device.hpp"

#include "radiance_cascades.hpp"

int RadianceCascades::getTotalProbeCount(std::vector<cascade> cascades) const
{
    int probeCount = 0;
    for (int i = 0; i < cascades.size(); ++i)
    {
        probeCount += cascades[i].probes.size();
    }
    return probeCount;
}

void RadianceCascades::init(void *userData)
{
    m_device = (*(std::weak_ptr<Device> *)(userData));
}
RadianceCascades::cascade RadianceCascades::createCascade(cascade_desc cd) const
{
    cascade result(cd.p);
    result.desc = cd;

    // probe count per dimension
    float px = sqrt(float(cd.p));
    float py = px;
    float ipx = 1.0 / px;
    float ipy = 1.0 / py;

    // center the probes
    glm::vec2 offset = glm::vec2(ipx * -0.5, ipy * -0.5);

    // probes within  the cascade
    for (float i = 0.0; i < px; ++i)
    {
        for (float j = 0.0; j < py; ++j)
        {
            // initialize probe posititon
            int probeIndex = int(px * i + j);
            result.probes[probeIndex].position.x = ipx * float(i) - offset.x;
            result.probes[probeIndex].position.y = ipy * float(j) - offset.y;
        }
    }

    // radiance interval count
    result.m = cd.p * cd.q;

    return result;
}
std::vector<RadianceCascades::cascade> RadianceCascades::createCascades(cascade_desc desc0, int cascadeCount) const
{
    std::vector<cascade> result;
    result.resize(cascadeCount);

    for (int i = 0; i < cascadeCount; ++i)
    {
        result[i] = createCascade(desc0);
        // P1 = P0/4
        desc0.p /= 4;
        // Q1 = 2Q0
        desc0.q *= 2;
        // DW1 = 2DW0
        desc0.dw *= 2.0;
    }

    return result;
}
void RadianceCascades::begin()
{
    std::cout << "Radiance Cascades Begin" << std::endl;

    // create probes in a cascade

    cascade_desc cd0 = cascade_desc(m_maxProbeCount, m_minDiscreteValueCount, m_minRadianceintervalLength);

    // create cascades
    auto cascades = createCascades(cd0, m_maxCascadeCount);

    BufferDirector bd;
    BufferBuilder bb;
    bd.configureUniformBufferBuilder(bb);
    bb.setDevice(m_device);
    int probeCount = getTotalProbeCount(cascades);
    bb.setSize(sizeof(RadianceCascades::probe) * probeCount);
    m_probePositionBuffer = bb.build();
    m_probePositionBuffer->copyDataToMemory
}
void RadianceCascades::update(float deltaTime)
{
}
