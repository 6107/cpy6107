---
applyTo: "include/**/*.hpp,src/**/*.cpp,src/**/*.hpp"
---
# C++23 Core Implementation Rules
* Use `#pragma once` at the top of every header file.
* Enclose all declarations inside `namespace cpy6107` (or a sub-namespace such as `cpy6107::framework`).
* Never use `using namespace` or namespace-wide `using` declarations in headers; spell out `std::chrono::`, `wpi::units::` etc. In `.cpp` files, prefer namespace aliases (`namespace chrono = std::chrono;`) or specific `using wpi::units::second_t;` declarations at file scope. `using namespace std::chrono_literals;` is the one accepted exception.
* Use `std::expected<T, ErrorEnum>` for recoverable runtime errors instead of throwing exceptions across performance-critical loops.
* Annotate pure mathematical and WPILib kinematics helpers with `constexpr` and `[[nodiscard]]` wherever supported by WPILib headers.
* Use `std::span<const double>` instead of `const std::vector<double>&` for read-only array parameters.
* Ensure code conforms to `.clang-format` and passes `.clang-tidy` checks cleanly.
