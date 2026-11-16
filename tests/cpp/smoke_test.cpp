#include "cpy6107/core/version.hpp"

#include <cassert>

int main() {
    static_assert(cpy6107::core::IsSimulationBuild());
    assert(!cpy6107::core::GetBuildBanner().empty());
    return 0;
}
