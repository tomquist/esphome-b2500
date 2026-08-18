# ESPHome stub headers

`b2500_codec.cpp` only needs `format_hex_pretty()` and the `ESP_LOG*` macros
from ESPHome, so the codec can be compiled and exercised on the host with these
minimal stand-ins instead of a full ESPHome/PlatformIO toolchain.
