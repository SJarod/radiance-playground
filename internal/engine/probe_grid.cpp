
#include "probe_grid.hpp"

std::unique_ptr<ProbeGrid> ProbeGridBuilder::build()
{
	for (float currentLayerHeight = m_gridBaseHeight; currentLayerHeight < m_gridMaxHeight; currentLayerHeight += m_probeSpacing)
	{
		for (float x = 0; x < m_gridMaxHeight; x += m_probeSpacing) 
		{
			for (float z = 0; z < m_gridMaxHeight; z += m_probeSpacing)
			{
				m_product->m_probes.push_back({ glm::vec3(x, currentLayerHeight, z)	});
			}
		}
	}

	return std::move(m_product);
}