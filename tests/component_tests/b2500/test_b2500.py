from pathlib import Path
import re
import tempfile
import pytest

from tests.component_tests.conftest import generate_main


@pytest.mark.parametrize(
    "yaml_file, class_name",
    [
        ("test_b2500.yaml", "B2500ComponentV1"),
        ("test_b2500_v2.yaml", "B2500ComponentV2"),
    ],
)
def test_b2500_component_generation(generate_main, yaml_file: str, class_name: str):
    yaml_path = Path(__file__).with_name(yaml_file)
    contents = yaml_path.read_text()
    component_dir = (Path(__file__).parents[3] / "components").resolve()
    contents = contents.replace("COMPONENT_PATH", str(component_dir))

    with tempfile.NamedTemporaryFile("w", suffix=".yaml", delete=False) as f:
        f.write(contents)
        temp_path = f.name

    main_cpp = generate_main(temp_path)
    # ESPHome 2025.x renders as `new b2500::ClassName()`, while 2026.4+
    # uses placement new: `new(<id>) b2500::ClassName()`.
    pattern = rf"new(?:\([^)]*\))?\s+b2500::{class_name}\(\)"
    assert re.search(pattern, main_cpp), main_cpp


def test_b2500_set_timer_generation_uses_integer_templates(generate_main):
    yaml_path = Path(__file__).with_name("test_b2500_v2_actions.yaml")
    contents = yaml_path.read_text()
    component_dir = (Path(__file__).parents[3] / "components").resolve()
    contents = contents.replace("COMPONENT_PATH", str(component_dir))

    with tempfile.NamedTemporaryFile("w", suffix=".yaml", delete=False) as f:
        f.write(contents)
        temp_path = f.name

    main_cpp = generate_main(temp_path)

    # Older ESPHome versions emit `set_timer(1)` for literal int values, while
    # 2026.4+ wraps every templatable in a lambda (`set_timer([]() -> int { return 1; })`).
    # Both forms prove the int-typed `templatable` overload is used (not bool/float).
    int_lambda_re = re.compile(r"set_timer\(\[\]\(\) -> int \{[^}]*return\s+1;\s*\}\)")
    assert "set_timer(1)" in main_cpp or int_lambda_re.search(main_cpp), main_cpp

    lambda_set_timer = "set_timer([]() -> int {"
    assert lambda_set_timer in main_cpp
    # The literal-int lambda (return 1) may appear before the user lambda
    # (return 2). Find the occurrence that corresponds to `timer: !lambda return 2;`.
    search_start = 0
    while True:
        idx = main_cpp.find(lambda_set_timer, search_start)
        if idx == -1:
            pytest.fail("Did not find set_timer lambda returning 2")
        if "return 2;" in main_cpp[idx: idx + 200]:
            break
        search_start = idx + len(lambda_set_timer)

    # ESPHome 2026.4 introduced TemplatableFn (4 bytes, function-pointer-only),
    # which static_asserts when fed a raw value instead of a callable. All
    # TEMPLATABLE_VALUE setters must therefore be passed via cg.templatable so
    # constants are wrapped in stateless lambdas. On 2025.x raw values are
    # still accepted, but the codegen path must go through cg.templatable.
    from esphome.const import __version__ as esphome_version

    major, minor = (int(p) for p in esphome_version.split(".", 2)[:2])
    requires_lambda_wrap = (major, minor) >= (2026, 4)

    # set_mqtt's `ssl` defaults to False (not exposed in YAML).
    assert "set_ssl(" in main_cpp
    # set_datetime with a struct literal.
    assert "set_datetime(" in main_cpp
    if requires_lambda_wrap:
        # TemplatableFn rejects raw constants — both must be wrapped via
        # cg.templatable into stateless lambdas.
        assert "set_ssl(false)" not in main_cpp
        assert "set_ssl(true)" not in main_cpp
        assert "set_datetime(ESPTime" not in main_cpp
