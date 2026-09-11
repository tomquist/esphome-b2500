from pathlib import Path
import shutil
import subprocess
import tempfile


def test_cell_info_parser_handles_malformed_tokens_gracefully():
    repo_root = Path(__file__).parents[2]
    codec_cpp = repo_root / "components" / "b2500" / "b2500_codec.cpp"

    gpp = shutil.which("g++")
    assert gpp is not None, "g++ is required to run parser robustness test"

    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp = Path(tmp_dir)
        include_dir = tmp / "esphome" / "core"
        include_dir.mkdir(parents=True)

        (include_dir / "helpers.h").write_text(
            """
#pragma once
#include <cstdint>
#include <string>
#include <vector>

inline std::string format_hex_pretty(const uint8_t *, size_t) { return \"\"; }
""".strip()
        )
        (include_dir / "log.h").write_text(
            """
#pragma once
#define ESP_LOGW(tag, fmt, ...) do {} while (0)
#define ESP_LOGD(tag, fmt, ...) do {} while (0)
""".strip()
        )

        harness = tmp / "cell_info_parser_harness.cpp"
        harness.write_text(
            f"""
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include \"{(repo_root / 'components' / 'b2500' / 'b2500_codec.h').as_posix()}\"

using esphome::b2500::B2500Codec;
using esphome::b2500::CellInfoPacket;

bool parse(B2500Codec &codec, const std::string &payload) {{
  CellInfoPacket packet{{}};
  auto *data = reinterpret_cast<uint8_t *>(const_cast<char *>(payload.data()));
  return codec.parse_cell_info(data, static_cast<uint16_t>(payload.size()), packet);
}}

int main() {{
  B2500Codec codec;

  const std::string valid = "10_24_25_3162_3161_3156_3156_3152_3162_3156_3156_3151_3153_3153_3153_3147_3155";
  if (!parse(codec, valid)) {{
    std::cerr << "expected valid payload to parse" << std::endl;
    return 1;
  }}

  const std::vector<std::string> invalid = {{
      "10_24_25_3162_3161_3156_3156_3152_3162_3156_3156_3151_3153_3153_3153_3147_bad",
      "10_24_25_3162_3161_3156_3156_3152_3162_3156_3156_3151_3153_3153_3153_3147_",
      "10_24_25_3162_3161_3156_3156_3152_3162_3156_3156_3151_3153_3153_3153_3147_31 55",
  }};

  for (const auto &token : invalid) {{
    if (parse(codec, token)) {{
      std::cerr << "expected malformed payload to fail: " << token << std::endl;
      return 2;
    }}
  }}

  return 0;
}}
""".strip()
        )

        output = tmp / "cell_info_parser_harness"
        compile_cmd = [
            gpp,
            "-std=c++17",
            "-I",
            tmp.as_posix(),
            "-I",
            repo_root.as_posix(),
            harness.as_posix(),
            codec_cpp.as_posix(),
            "-o",
            output.as_posix(),
        ]
        subprocess.run(compile_cmd, check=True, capture_output=True, text=True)

        subprocess.run([output.as_posix()], check=True, capture_output=True, text=True)
