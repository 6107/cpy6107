#include "cpy6107/core/version.hpp"

namespace cpy6107::core {

std::string_view GetBuildBanner() noexcept {
    if constexpr (IsSimulationBuild()) {
        return "cpy6107 0.1.0 (FRC 2027 - Host Simulation)";
    } else {
        return "cpy6107 0.1.0 (FRC 2027 - SystemCore Runtime)";
    }
}

} // namespace cpy6107::core
