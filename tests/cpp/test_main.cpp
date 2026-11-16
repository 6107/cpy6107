#include "cpy6107/framework/wpilib_compat.hpp"

#include <wpi/hal/HAL.h>

#include "test_harness.hpp"

int main() {
    // The simulated HAL runs headlessly. a6 takes (timeout_ms, mode); a8 takes no arguments.
#if CPY6107_WPILIB_ALPHA6
    const bool ok = HAL_Initialize(500, 0) != 0;
#else
    const bool ok = HAL_Initialize() != 0;
#endif
    if (!ok) {
        std::fputs("HAL_Initialize failed\n", stderr);
        return 2;
    }
    return cpy6107::testing::RunAllTests() == 0 ? 0 : 1;
}
