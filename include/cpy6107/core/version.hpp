#pragma once

#include <string_view>

namespace cpy6107::core {

constexpr std::string_view kVersion = "0.1.0-frc2027";

[[nodiscard]] constexpr bool IsSimulationBuild() noexcept {
#ifdef CPY6107_SIMULATION
    return true;
#else
    return false;
#endif
}

[[nodiscard]] std::string_view GetBuildBanner() noexcept;

} // namespace cpy6107::core
