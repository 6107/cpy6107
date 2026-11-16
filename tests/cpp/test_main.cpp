#include <wpi/hal/HAL.h>

#include "test_harness.hpp"

int main() {
    // The simulated HAL runs headlessly; 500 ms is the HAL initialization timeout.
    if (HAL_Initialize(500, 0) == 0) {
        std::fputs("HAL_Initialize failed\n", stderr);
        return 2;
    }
    return cpy6107::testing::RunAllTests() == 0 ? 0 : 1;
}
