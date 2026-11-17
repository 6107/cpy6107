// ------------------------------------------------------------------------
//      o-o      o                o
//     /         |                |
//    O     o  o O-o  o-o o-o     |  oo o--o o-o o-o
//     \    |  | |  | |-' |   \   o | | |  |  /   /
//      o-o o--O o-o  o-o o    o-o  o-o-o--O o-o o-o
//             |                           |
//          o--o                        o--o
//                        o--o      o         o
//                        |   |     |         |  o
//                        O-Oo  o-o O-o  o-o -o-    o-o o-o
//                        |  \  | | |  | | |  |  | |     |
//                        o   o o-o o-o  o-o  o  |  o-o o-o
//
//    Jemison High School - Huntsville Alabama
// ------------------------------------------------------------------------

#pragma once

#include <string_view>

namespace cpy6107::core {

// TODO: Hook into same version string as pyproject.toml
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
