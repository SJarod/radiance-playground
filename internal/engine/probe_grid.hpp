
#include <memory>
#include <vector>

class ProbeGridBuilder;

struct Probe 
{
	float x;
	float y;
	float z;
};

class ProbeGrid 
{
	friend ProbeGridBuilder;

private:
	std::vector<Probe> m_probes;

};

class ProbeGridBuilder 
{
private:
	std::unique_ptr<ProbeGrid> m_product;

	float m_probeSpacing = 1.f;
	float m_gridBaseHeight = 0.f;
	float m_gridMaxHeight = 10.f;

	void restart() 
	{
		m_product = std::make_unique<ProbeGrid>();
	}

public:
	ProbeGridBuilder() 
	{
		restart();
	}

	std::unique_ptr<ProbeGrid> build();
};