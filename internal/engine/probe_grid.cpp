#include "probe_grid.hpp"

std::unique_ptr<ProbeGrid> ProbeGridBuilder::build()
{
	const float xProbeSpacing = m_extent.x / static_cast<float>(m_xAxisProbeCount - 1u);
	const float yProbeSpacing = m_extent.y / static_cast<float>(m_yAxisProbeCount - 1u);
	const float zProbeSpacing = m_extent.z / static_cast<float>(m_zAxisProbeCount - 1u);

	for (uint32_t i = 0u; i < m_xAxisProbeCount; i++)
	{
		float y = m_cornerPosition.y + i * yProbeSpacing;
		for (uint32_t j = 0u; j < m_yAxisProbeCount; j++)
		{
			float x = m_cornerPosition.x + j * xProbeSpacing;
			for (uint32_t k = 0u; k < m_zAxisProbeCount; k++)
			{
				float z = m_cornerPosition.z + k * xProbeSpacing;
				m_product->m_probes.push_back(std::make_unique<Probe>(glm::vec3(x, y, z)));
			}
		}
	}

	return std::move(m_product);
}