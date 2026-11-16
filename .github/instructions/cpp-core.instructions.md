---
applyTo: "include/cpy6107/core/**,src/core/**"
---
# Core Library Rules (`core/`)
- Files in `core/` execute identically on both the SystemCore ARM64 runtime and Windows 11 / Ubuntu / macOS simulation hosts.
- Do not include WPILib HAL headers, `SimDevice` headers, or OS-specific headers (`<windows.h>`, `<unistd.h>`, etc.).
- Mark pure functions `constexpr` and `[[nodiscard]]` wherever possible.
- Avoid heap allocations inside periodic update methods.
