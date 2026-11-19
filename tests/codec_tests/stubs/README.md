# ESPHome stub headers

`b2500_codec.cpp` only needs `format_hex_pretty_to()` and the `ESP_LOG*` macros
from ESPHome, so the codec can be compiled and exercised on the host with these
minimal stand-ins instead of a full ESPHome/PlatformIO toolchain.

The helpers stub tracks what the current ESPHome `helpers.h` declares, not an
older version: a function ESPHome has moved out of it must not be stubbed, or
these tests pass on code that no longer builds as firmware.
