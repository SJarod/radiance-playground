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
    result.reserve(cascadeCount);

    for (int i = 0; i < cascadeCount; ++i)
    {
        result.push_back(createCascade(desc0));
        // P1 = P0/4
        desc0.p /= 4;
        // Q1 = 2Q0
        desc0.q *= 2;
        // DW1 = 2DW0
        desc0.dw *= 2.0;
    }

    return result;
}
void RadianceCascades::init(void *userData)
{
    std::cout << "Radiance Cascades Init" << std::endl;

    auto data = (init_data *)userData;
    m_device = data->device;

    {
        BufferDirector bd;
        BufferBuilder bb;
        bd.configureUniformBufferBuilder(bb);
        bb.setDevice(m_device);
        bb.setSize(sizeof(parameters));

        m_radianceCascadesParametersBuffer = bb.build();
        parameters params = {
            .maxCascadeCount = m_maxCascadeCount,
            .maxProbeCount = m_maxProbeCount,
            .minDiscreteValueCount = m_minDiscreteValueCount,
            .minRadianceintervalLength = m_minRadianceintervalLength,
            .lightIntensity = m_lightIntensity,
            .maxRayIterationCount = m_maxRayIterationCount,
        };
        m_radianceCascadesParametersBuffer->copyDataToMemory(&params);
    }

    // create probes in a cascade

    cascade_desc cd0 = cascade_desc(m_maxProbeCount, m_minDiscreteValueCount, m_minRadianceintervalLength);

    // create cascades
    auto cascades = createCascades(cd0, m_maxCascadeCount);

    // buffer 1 is for the descriptors of each cascade used in the fragment shader
    // all cascades descriptors
    {
        std::vector<cascade_desc> descs;
        descs.reserve(cascades.size());
        for (int i = 0; i < cascades.size(); ++i)
        {
            descs.push_back(cascades[i].desc);
        }

        BufferDirector bd;
        BufferBuilder bb;
        bd.configureStorageBufferBuilder(bb);
        bb.setDevice(m_device);
        bb.setSize(sizeof(cascade_desc) * cascades.size());

        m_cascadesDescBuffer = bb.build();
        m_cascadesDescBuffer->copyDataToMemory(descs.data());
    }

    // buffer 2 is for all the actual probe data (positions)
    // all cascades probes positions
    {
        // total number of probes combining all the cascades
        int probeCount = 0;
        std::vector<glm::vec2> positions;
        for (int i = 0; i < cascades.size(); ++i)
        {
            probeCount += cascades[i].desc.p;

            for (const probe &p : cascades[i].probes)
            {
                positions.push_back(p.position);
            }
        }

        BufferDirector bd;
        BufferBuilder bb;
        bd.configureStorageBufferBuilder(bb);
        bb.setDevice(m_device);
        bb.setSize(sizeof(glm::vec2) * probeCount);

        m_probePositionBuffer = bb.build();
        m_probePositionBuffer->copyDataToMemory(positions.data());
    }

    // buffer 3 is a storage buffer for the radiance gathering
    // write : writting the radiance intervals in the gather phase
    // read : fragment shader read the radiance intervals and merge and apply for the indirect lighting computation
    {
        for (int i = 0; i < data->frameInFlightCount; ++i)
        {
            BufferDirector bd;
            BufferBuilder bb;
            bd.configureStorageBufferBuilder(bb);
            bb.setDevice(m_device);

            // total number of interval combining all the cascades
            int intervalCount = 0;
            for (int i = 0; i < cascades.size(); ++i)
            {
                intervalCount += cascades[i].m;
            }

            bb.setSize(sizeof(glm::vec4) * intervalCount);

            m_radianceIntervalsStorageBufferRW.push_back(bb.build());
        }
    }
}

void RadianceCascades::begin()
{
    std::cout << "Radiance Cascades Begin" << std::endl;
}
void RadianceCascades::update(float deltaTime)
{
}
