from pathlib import Path
import tempfile
import pytest

from tests.component_tests.conftest import generate_main


@pytest.mark.parametrize(
    "yaml_file, expected",
    [
        ("test_b2500.yaml", "new b2500::B2500ComponentV1()"),
        ("test_b2500_v2.yaml", "new b2500::B2500ComponentV2()"),
    ],
)
def test_b2500_component_generation(generate_main, yaml_file: str, expected: str):
    yaml_path = Path(__file__).with_name(yaml_file)
    contents = yaml_path.read_text()
    component_dir = (Path(__file__).parents[3] / "components").resolve()
    contents = contents.replace("COMPONENT_PATH", str(component_dir))

    with tempfile.NamedTemporaryFile("w", suffix=".yaml", delete=False) as f:
        f.write(contents)
        temp_path = f.name

    main_cpp = generate_main(temp_path)
    assert expected in main_cpp


def test_b2500_set_timer_templated_index_guard(generate_main):
    yaml_path = Path(__file__).with_name("test_b2500_timer_template_guard.yaml")
    contents = yaml_path.read_text()
    component_dir = (Path(__file__).parents[3] / "components").resolve()
    contents = contents.replace("COMPONENT_PATH", str(component_dir))

    with tempfile.NamedTemporaryFile("w", suffix=".yaml", delete=False) as f:
        f.write(contents)
        temp_path = f.name

    main_cpp = generate_main(temp_path)
    assert "return 99;" in main_cpp

    automation_h = (Path(__file__).parents[3] / "components" / "b2500" / "automation.h").read_text()
    assert "if (timer_idx < 0 || timer_idx >= 5)" in automation_h
    assert "Invalid timer index %d in set_timer action" in automation_h
