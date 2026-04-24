#!/usr/bin/env python3
"""Unit tests for plan_snippet_antipattern.py.

Run standalone: `python3 .claude/hooks/tests/test_plan_snippet_antipattern.py`.
Uses only stdlib. Exercises each rule against synthetic block content so the
hook keeps its regression protection even when no real bundle is available.
"""

from __future__ import annotations

import importlib.util
import pathlib
import sys


HOOK_PATH = (
    pathlib.Path(__file__).resolve().parents[1] / "plan_snippet_antipattern.py"
)
spec = importlib.util.spec_from_file_location("plan_snippet_antipattern", HOOK_PATH)
mod = importlib.util.module_from_spec(spec)
assert spec.loader is not None
# Register before exec so Python 3.9 dataclass resolution can find it.
sys.modules["plan_snippet_antipattern"] = mod
spec.loader.exec_module(mod)


def run_rules(block):
    findings = []
    for rule in mod.RULES:
        findings.extend(list(rule(block)))
    return findings


def make_block(body: str, *, section="## 3. Resolver", lang="cpp",
               negative=False):
    lines = body.splitlines()
    return mod.Block(
        phase=19, field="code_snippets",
        section_heading=section, fence_lang=lang,
        start_line=1, body_lines=lines,
        is_negative_example=negative,
    )


def test_if_chain_fires_on_14_arms():
    body = "\n".join(
        f'    if (InAlias == "alias_{i}") return EScreen::Screen{i};'
        for i in range(14)
    )
    findings = run_rules(make_block(body))
    assert any(f.rule == "if_chain_stringly_typed" for f in findings), findings
    got = next(f for f in findings if f.rule == "if_chain_stringly_typed")
    assert "14 consecutive" in got.detail, got.detail


def test_if_chain_silent_on_2_arms():
    body = (
        '    if (InKey == "id") IdValue = Val;\n'
        '    if (InKey == "force") ForceValue = Val;\n'
    )
    findings = run_rules(make_block(body, section="## 4. Args"))
    assert not any(f.rule == "if_chain_stringly_typed" for f in findings)


def test_if_chain_silent_in_anti_pattern_section():
    body = "\n".join(
        f'    if (InAlias == "x_{i}") return EScreen::A;'
        for i in range(20)
    )
    findings = run_rules(make_block(body, section="## 8. Anti-Pattern: If-Chain"))
    assert not findings, findings


def test_if_chain_silent_with_bad_comment():
    body = (
        "    // BAD: 20 consecutive equality-to-literal arms.\n"
        + "\n".join(
            f'    if (InAlias == "x_{i}") return EScreen::A;'
            for i in range(20)
        )
    )
    findings = run_rules(make_block(body, negative=True))
    assert not findings, findings


def test_large_enum_switch_fires():
    arms = "\n".join(
        f"        case EScreen::Screen{i}:\n"
        f"            DoSomething();\n"
        f"            break;"
        for i in range(8)
    )
    body = (
        "    switch (mCurrentScreen) {\n"
        f"{arms}\n"
        "        default: break;\n"
        "    }\n"
    )
    findings = run_rules(make_block(body, section="## 5. Switch"))
    assert any(f.rule == "large_enum_switch" for f in findings), findings


def test_large_enum_switch_silent_on_3_arms():
    body = (
        "    switch (Mode) {\n"
        "        case EMode::A: break;\n"
        "        case EMode::B: break;\n"
        "        case EMode::C: break;\n"
        "    }\n"
    )
    findings = run_rules(make_block(body))
    assert not any(f.rule == "large_enum_switch" for f in findings)


def test_stringly_args_fires():
    body = (
        "FRuntimeCommandResponse HandleFoo(\n"
        "    const std::map<std::string, std::string>& InArgs);\n"
    )
    findings = run_rules(make_block(body))
    assert any(f.rule == "stringly_typed_args" for f in findings), findings


def test_stringly_args_silent_in_negative_section():
    body = (
        "FRuntimeCommandResponse HandleFoo(\n"
        "    const std::map<std::string, std::string>& InArgs);\n"
    )
    findings = run_rules(make_block(body, section="## 7. Anti-Pattern: Untyped Args"))
    assert not findings, findings


def test_raw_new_fires():
    body = "    auto* X = new FInputSuggestionEngineLocal();"
    findings = run_rules(make_block(body))
    assert any(f.rule == "raw_new_operator" for f in findings), findings


def test_raw_new_silent_with_make_unique():
    body = "    auto X = std::make_unique<FInputSuggestionEngineLocal>();"
    findings = run_rules(make_block(body))
    assert not findings, findings


def test_goto_fires():
    body = "    goto cleanup;"
    findings = run_rules(make_block(body))
    assert any(f.rule == "goto_used" for f in findings), findings


def test_non_cpp_block_silent():
    body = "    if (InAlias == \"x\") return A\n" * 5
    findings = run_rules(make_block(body, lang="python"))
    assert not findings, findings


def main():
    tests = [name for name in globals() if name.startswith("test_")]
    failed = []
    for name in tests:
        try:
            globals()[name]()
            print(f"  PASS  {name}")
        except AssertionError as e:
            failed.append(name)
            print(f"  FAIL  {name}: {e}")
        except Exception as e:
            failed.append(name)
            print(f"  ERROR {name}: {type(e).__name__}: {e}")
    print()
    print(f"{len(tests) - len(failed)}/{len(tests)} passed")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
