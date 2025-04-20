#include "probe_grid.hpp"

std::unique_ptr<ProbeGrid> ProbeGridBuilder::build()
{
	const glm::vec3 probeSpacing = m_product->m_extent / static_cast<glm::vec3>(m_product->m_dimensions - glm::uvec3(1u));

	for (uint32_t i = 0u; i < m_product->m_dimensions.y; i++)
	{
		const float y = m_product->m_cornerPosition.y + i * probeSpacing.y;
		for (uint32_t j = 0u; j < m_product->m_dimensions.x; j++)
		{
			const float x = m_product->m_cornerPosition.x + j * probeSpacing.x;
			for (uint32_t k = 0u; k < m_product->m_dimensions.z; k++)
			{
				const float z = m_product->m_cornerPosition.z + k * probeSpacing.z;
				m_product->m_probes.push_back(std::make_unique<Probe>(glm::vec3(x, y, z)));
			}
		}
	}

	return std::move(m_product);
}