"""Host-compiled tests for the B2500 codec.

The codec only depends on ESPHome for logging and hex formatting, so it can be
compiled with the stubs in ``stubs/`` and exercised without a firmware build.
"""
from pathlib import Path
import shutil
import subprocess

import pytest

HERE = Path(__file__).parent
REPO_ROOT = HERE.parents[1]
COMPONENT_DIR = REPO_ROOT / "components" / "b2500"


@pytest.mark.skipif(shutil.which("g++") is None, reason="g++ is not available")
@pytest.mark.parametrize("source", sorted(HERE.glob("*_test.cpp")), ids=lambda p: p.stem)
def test_codec(source: Path, tmp_path: Path):
    binary = tmp_path / source.stem
    compile_result = subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-Wall",
            f"-I{COMPONENT_DIR}",
            f"-I{HERE / 'stubs'}",
            "-o",
            str(binary),
            str(source),
            str(COMPONENT_DIR / "b2500_codec.cpp"),
        ],
        capture_output=True,
        text=True,
    )
    assert compile_result.returncode == 0, compile_result.stderr

    run_result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert run_result.returncode == 0, run_result.stdout + run_result.stderr
