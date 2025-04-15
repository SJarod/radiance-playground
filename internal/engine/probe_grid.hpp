
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

	float m_probeSpacing = 1.f;
	float m_gridBaseHeight = 0.f;
	float m_gridMaxHeight = 2.f;

	void restart() 
	{
		m_product = std::make_unique<ProbeGrid>();
	}

public:
	ProbeGridBuilder() 
	{
		restart();
	}

	void setProbeSpacing(float spacing)
	{
		m_probeSpacing = spacing;
	}

	void setGridBaseHeight(float baseHeight)
	{
		m_gridBaseHeight = baseHeight;
	}

	void setGridMaxHeight(float maxHeight)
	{
		m_gridMaxHeight = maxHeight;
	}

	std::unique_ptr<ProbeGrid> build();
};