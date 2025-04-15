
#include <memory>
#include <vector>
#include <glm/glm.hpp>

class ProbeGridBuilder;

struct Probe 
{
	glm::vec3 position;
};

class ProbeGrid 
{
	friend ProbeGridBuilder;

private:
	std::vector<Probe> m_probes;

public:
	const std::vector<Probe>& getProbes() const
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