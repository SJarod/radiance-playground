
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

	glm::uvec3 m_dimensions = glm::uvec3(2u);
	glm::vec3 m_cornerPosition = { 0.f, 0.f, 0.f };
	glm::vec3 m_extent = { 2.f, 2.f, 2.f };

public:
	inline const std::vector<std::unique_ptr<Probe>>& getProbes() const
	{
		return m_probes;
	}

	inline const Probe* getProbeAtIndex(const uint32_t index) const
	{
		return m_probes[index].get();
	}

	inline const glm::vec3& getDimensions() const
	{
		return m_dimensions;
	}

	inline const glm::vec3& getExtent() const
	{
		return m_extent;
	}

	inline const glm::vec3& getCornerPosition() const
	{
		return m_cornerPosition;
	}
};

class ProbeGridBuilder 
{
private:
	std::unique_ptr<ProbeGrid> m_product;

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
		m_product->m_dimensions.x = probeCount;
	}

	void setYAxisProbeCount(uint32_t probeCount)
	{
		m_product->m_dimensions.y = probeCount;
	}

	void setZAxisProbeCount(uint32_t probeCount)
	{
		m_product->m_dimensions.z = probeCount;
	}

	void setCornerPosition(const glm::vec3& cornerPosition)
	{
		m_product->m_cornerPosition = cornerPosition;
	}

	void setExtent(const glm::vec3& extent)
	{
		m_product->m_extent = extent;
	}

	std::unique_ptr<ProbeGrid> build();
};