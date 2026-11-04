"""Host-compiled tests for hame_device's message parser and command templates
(plain C++17, no ESPHome dependency)."""
from pathlib import Path
import shutil
import subprocess

import pytest

HERE = Path(__file__).parent
COMPONENT_DIR = HERE.parents[1] / "components" / "hame_device"
SOURCES = ["kv_message.cpp"]


@pytest.mark.skipif(shutil.which("g++") is None, reason="g++ is not available")
@pytest.mark.parametrize("source", sorted(HERE.glob("*_test.cpp")), ids=lambda p: p.stem)
def test_hame_device(source: Path, tmp_path: Path):
    binary = tmp_path / source.stem
    result = subprocess.run(
        ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", f"-I{COMPONENT_DIR}", "-o", str(binary), str(source),
         *[str(COMPONENT_DIR / s) for s in SOURCES]],
        capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stderr
    run = subprocess.run([str(binary)], capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr
