
#include <memory>
#include <vector>
#include <glm/glm.hpp>

class ProbeGridBuilder;

class Probe 
{
public:
	glm::vec3 position;

	Probe(const glm::vec3& position)
		: position(position) { }
	virtual ~Probe() = default;
};

class ProbeGrid 
{
	friend ProbeGridBuilder;

private:
	std::vector<std::unique_ptr<Probe>> m_probes;

public:
	const std::vector<std::unique_ptr<Probe>>& getProbes() const
	{
		return m_probes;
	}
};

class ProbeGridBuilder 
{
private:
	std::unique_ptr<ProbeGrid> m_product;

	uint32_t m_xAxisProbeCount = 2u;
	uint32_t m_yAxisProbeCount = 2u;
	uint32_t m_zAxisProbeCount = 2u;

	glm::vec3 m_cornerPosition = { 0.f, 0.f, 0.f };
	glm::vec3 m_extent = { 2.f, 2.f, 2.f };

	void restart() 
	{
		m_product = std::make_unique<ProbeGrid>();
	}

public:
	ProbeGridBuilder() 
	{
		restart();
	}

	void setXAxisProbeCount(uint32_t probeCount)
	{
		m_xAxisProbeCount = probeCount;
	}

	void setYAxisProbeCount(uint32_t probeCount)
	{
		m_yAxisProbeCount = probeCount;
	}

	void setZAxisProbeCount(uint32_t probeCount)
	{
		m_zAxisProbeCount = probeCount;
	}

	void setCornerPosition(const glm::vec3& cornerPosition)
	{
		m_cornerPosition = cornerPosition;
	}

	void setExtent(const glm::vec3& extent)
	{
		m_extent = extent;
	}

	std::unique_ptr<ProbeGrid> build();
};