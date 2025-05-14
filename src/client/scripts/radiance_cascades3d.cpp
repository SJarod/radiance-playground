#include <iostream>

#include "graphics/device.hpp"

#include "radiance_cascades3d.hpp"

int RadianceCascades3D::getTotalProbeCount(std::vector<cascade> cascades) const
{
    int probeCount = 0;
    for (int i = 0; i < cascades.size(); ++i)
    {
        probeCount += cascades[i].probes.size();
    }
    return probeCount;
}

RadianceCascades3D::cascade RadianceCascades3D::createCascade(cascade_desc cd) const
{
    cascade result(cd.p);
    result.desc = cd;

    // probe count per dimension
    float px = std::cbrt(float(cd.p));
    float py = px;
    float pz = px;
    float ipx = 1.0 / px;
    float ipy = 1.0 / py;
    float ipz = 1.0 / pz;

    // center the probes
    glm::vec3 offset = glm::vec3(ipx * -0.5, ipy * -0.5, ipz * -0.5);

    // probes within  the cascade
    for (int i = 0; i < (int)px; ++i)
    {
        for (int j = 0; j < (int)py; ++j)
        {
            for (int k = 0; k < (int)pz; ++k)
            {
                // initialize probe posititon
                int probeIndex = int(px * i + j + px * py * k);
                result.probes[probeIndex].position.x = ipx * m_range.x * float(i) - offset.x;
                result.probes[probeIndex].position.y = ipy * m_range.y * float(j) - offset.y;
                result.probes[probeIndex].position.z = ipz * m_range.z * float(k) - offset.z;
            }
        }
    }

    // radiance interval count
    result.m = cd.p * cd.q;

    return result;
}
std::vector<RadianceCascades3D::cascade> RadianceCascades3D::createCascades(cascade_desc desc0, int cascadeCount) const
{
    std::vector<cascade> result;
    result.reserve(cascadeCount);

    for (int i = 0; i < cascadeCount; ++i)
    {
        result.push_back(createCascade(desc0));
        // P1 = P0/4 in 2d P0/8 in 3d
        desc0.p /= 8;
        // Q1 = 2Q0 in 3d 4Q0 in 3d
        desc0.q *= 4;
        // DW1 = 2DW0
        desc0.dw *= 2.0;
    }

    return result;
}
void RadianceCascades3D::init(void *userData)
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
        bb.setName("Radiance Cascades Parameters Buffer");

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
        bb.setName("Radiance Cascades Descriptors Buffer");

        m_cascadesDescBuffer = bb.build();
        m_cascadesDescBuffer->copyDataToMemory(descs.data());
    }

    // buffer 2 is for all the actual probe data (positions)
    // all cascades probes positions
    {
        // total number of probes combining all the cascades
        int probeCount = 0;
        std::vector<probe> positions;
        for (int i = 0; i < cascades.size(); ++i)
        {
            probeCount += cascades[i].desc.p;
            probePositions.push_back({});

            for (const probe &p : cascades[i].probes)
            {
                positions.push_back(p);
                probePositions[i].push_back(p.position);
            }
        }

        BufferDirector bd;
        BufferBuilder bb;
        bd.configureStorageBufferBuilder(bb);
        bb.setDevice(m_device);
        bb.setSize(sizeof(probe) * probeCount);
        bb.setName("Radiance Cascades Positions Buffer");

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
            bb.setName("Radiance Cascades Radiance Intervals Buffer");

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

void RadianceCascades3D::begin()
{
    std::cout << "Radiance Cascades Begin" << std::endl;
}
void RadianceCascades3D::update(float deltaTime)
{
}
