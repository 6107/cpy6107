#include "cpy6107/core/version.hpp"

#include "test_harness.hpp"

TEST(CoreVersionBanner) {
    CHECK(!cpy6107::core::kVersion.empty());
    CHECK(cpy6107::core::GetBuildBanner().contains("cpy6107"));
    CHECK(cpy6107::core::IsSimulationBuild() ==
          cpy6107::core::GetBuildBanner().contains("Host Simulation"));
}
