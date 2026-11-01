"""Host-compiled tests for the hame_at AT engine and FC41D emulator.

Both are plain C++17 with no ESPHome dependency, so each *_test.cpp here is
compiled with g++ together with the sources and run.
"""
from pathlib import Path
import shutil
import subprocess

import pytest

HERE = Path(__file__).parent
REPO_ROOT = HERE.parents[1]
COMPONENT_DIR = REPO_ROOT / "components" / "hame_at"
SOURCES = ["at_engine.cpp", "fc41d_emulator.cpp"]


@pytest.mark.skipif(shutil.which("g++") is None, reason="g++ is not available")
@pytest.mark.parametrize("source", sorted(HERE.glob("*_test.cpp")), ids=lambda p: p.stem)
def test_hame_at(source: Path, tmp_path: Path):
    binary = tmp_path / source.stem
    compile_result = subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            f"-I{COMPONENT_DIR}",
            "-o",
            str(binary),
            str(source),
            *[str(COMPONENT_DIR / s) for s in SOURCES],
        ],
        capture_output=True,
        text=True,
    )
    assert compile_result.returncode == 0, compile_result.stderr

    run_result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert run_result.returncode == 0, run_result.stdout + run_result.stderr
