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

#include "cpy6107/core/version.hpp"

namespace cpy6107::core {

std::string_view GetBuildBanner() noexcept {
    // TODO: Hook into same version string as pyproject.toml

    if constexpr (IsSimulationBuild()) {
        return "cpy6107 0.1.0 (FRC 2027 - Host Simulation)";
    } else {
        return "cpy6107 0.1.0 (FRC 2027 - SystemCore Runtime)";
    }
}

} // namespace cpy6107::core
