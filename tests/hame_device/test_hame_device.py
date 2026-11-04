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


def test_template_keys():
    """Codegen keeps exactly the keys render_template looks up."""
    pytest.importorskip("esphome")
    import importlib.util

    spec = importlib.util.spec_from_file_location("hame_device_init", COMPONENT_DIR / "__init__.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    keys = mod.template_keys
    assert keys("cd=20,md=0[,a1={d1},b1={e1|0:0}],aN={value}") == ["d1", "e1"]
    assert keys("cd=1") == []
    assert keys("{{lit}} {x}{x} [[y]] {z|a|b}") == ["x", "z"]
    assert keys("{open") == []
