#include "RegionManager.h"
#include <cassert>
#include <limits>
int main() {
    level::RegionManager m;
    assert(m.add(-1.0, 1.0, level::RegionType::Breath) == 0);
    assert(m.add(1.0, 1.0, level::RegionType::Breath) == 0);
    assert(m.add(0.0, 1.0, level::RegionType::Breath, 1.5f) == 0);
    const auto breath = m.add(2.0, 3.0, level::RegionType::Breath);
    const auto pop = m.add(0.5, 1.0, level::RegionType::Plosive);
    assert(breath != 0 && pop != 0);
    assert(m.all().size() == 2 && m.all()[0].id == pop);
    assert(m.resize(breath, 0.2, 0.4));
    assert(m.all()[0].id == breath);
    assert(!m.resize(breath, 0.8, 0.7));
    assert(m.setGain(breath, -12.0f));
    assert(!m.setGain(breath, -500.0f));
    assert(m.setEnabled(breath, false) && !m.all()[0].enabled);
    assert(m.erase(pop) && !m.erase(pop));
    m.clear();
    assert(m.all().empty());
    return 0;
}
