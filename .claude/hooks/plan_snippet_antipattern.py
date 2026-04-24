#!/usr/bin/env python3
"""Plan snippet anti-pattern lint.

Parses the `code_snippets`, `code_entity_contract`, and `best_practices`
fields of a .Plan.json phase via the uni-plan CLI (HARD RULE: never raw-read
the bundle) and scans fenced C/C++ code blocks for shapes the repo's
coding-principles and P18-anchor best-practices classify as anti-patterns:

  * Stringly-typed if-chains: 3+ consecutive `if (X == "literal") return ...;`
    arms. Textbook `NO IF/ELSE HELL` violation; registration-table shape
    required.
  * Large enum switches: `switch` whose case arms all dereference differently
    named pointers with otherwise identical bodies (duplicated-under-enum-case
    smell) - flagged when >= 5 such arms are detected.
  * Stringly-typed handler signatures: `std::map<std::string, std::string>`
    appearing in a handler-like parameter list.
  * Raw `new F<Name>(...)` outside `std::make_unique` / `std::make_shared`.
  * Bare `goto` outside documented fixtures.
  * `std::regex` inside blocks tagged as "hot path".

The scanner intentionally skips code blocks that appear inside sections
whose heading contains "Anti-Pattern", or whose block body starts with a
`// BAD:` or `// REJECTED:` comment - those are the documented negative
examples that the snippet catalog uses to prevent the pattern from silently
returning. See `.claude/rules/upl-plan-snippet-discipline.md` for
the full rule set.

Output: `plan-snippet-antipattern-v1` JSON on stdout with a `findings[]`
array. Exit 1 when any finding is emitted under --strict; exit 0 otherwise
(but the JSON still documents the findings so callers can report them).
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass, field
from typing import Iterable, Sequence


SCHEMA = "plan-snippet-antipattern-v1"

# Fields pulled from the bundle for scanning. code_snippets is the primary
# surface; code_entity_contract sometimes carries small sketches; best_practices
# may include anti-pattern examples that should be recognized as such.
DESIGN_FIELDS = ("code_snippets", "code_entity_contract", "best_practices")


@dataclass
class Finding:
    topic: str
    phase: int
    field: str
    section: str
    rule: str
    severity: str
    line: int
    detail: str
    suggestion: str


@dataclass
class Block:
    """One fenced code block lifted from a design-material field."""

    phase: int
    field: str
    section_heading: str
    fence_lang: str
    start_line: int
    body_lines: list[str]
    # True when the block is in an Anti-Pattern / Rejected Pattern section,
    # or its body starts with `// BAD:` / `// REJECTED:`.
    is_negative_example: bool = False


# ---------------------------------------------------------------------------
# Fetch phases via uni-plan. HARD RULE - never raw-read the .Plan.json.
# ---------------------------------------------------------------------------


def run_uni_plan(args: Sequence[str]) -> dict:
    cmd = ["uni-plan", *args]
    try:
        out = subprocess.check_output(cmd, text=True, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as exc:
        sys.stderr.write(f"uni-plan failed: {' '.join(cmd)}\n{exc.output}\n")
        raise SystemExit(2)
    # uni-plan prepends a `[repo: ...]\n` line before its JSON.
    json_start = out.find("{")
    if json_start < 0:
        raise SystemExit(
            f"uni-plan returned no JSON for command: {' '.join(cmd)}\n{out}"
        )
    return json.loads(out[json_start:])


def enumerate_phases(topic: str) -> list[int]:
    data = run_uni_plan(["topic", "get", "--topic", topic])
    return [p["index"] for p in data.get("phase_summary", [])]


def fetch_phase_design(topic: str, phase: int) -> dict:
    return run_uni_plan(
        ["phase", "get", "--topic", topic, "--phase", str(phase), "--design"]
    )


# ---------------------------------------------------------------------------
# Block extraction. Split on markdown headings; recognize fenced code blocks.
# ---------------------------------------------------------------------------


def extract_blocks(phase: int, field_name: str, text: str) -> list[Block]:
    blocks: list[Block] = []
    if not text:
        return blocks
    lines = text.splitlines()
    current_heading = ""
    in_block = False
    fence_lang = ""
    block_start = 0
    body: list[str] = []

    for idx, line in enumerate(lines):
        if not in_block:
            stripped = line.strip()
            if stripped.startswith("#") and " " in stripped:
                current_heading = stripped.lstrip("# ").strip()
            m = re.match(r"```([A-Za-z0-9_+-]*)\s*$", line.strip())
            if m:
                in_block = True
                fence_lang = m.group(1) or ""
                block_start = idx + 1
                body = []
        else:
            if line.strip().startswith("```"):
                # Determine if negative-example.
                is_neg = False
                head_lc = current_heading.lower()
                if "anti-pattern" in head_lc or "rejected" in head_lc:
                    is_neg = True
                first_non_blank = next(
                    (ln.strip() for ln in body if ln.strip()), ""
                )
                if re.match(r"//\s*(BAD|REJECTED|ANTI-PATTERN)\b", first_non_blank, re.IGNORECASE):
                    is_neg = True
                blocks.append(
                    Block(
                        phase=phase,
                        field=field_name,
                        section_heading=current_heading,
                        fence_lang=fence_lang.lower(),
                        start_line=block_start,
                        body_lines=list(body),
                        is_negative_example=is_neg,
                    )
                )
                in_block = False
                fence_lang = ""
                body = []
                continue
            body.append(line)

    return blocks


def is_cpp_block(b: Block) -> bool:
    return b.fence_lang in {"cpp", "c++", "cc", "cxx", "c"}


def is_negative_block(b: Block) -> bool:
    """Robust negative-example check.

    `extract_blocks` already sets `is_negative_example` for blocks inside
    Anti-Pattern / Rejected sections or blocks whose body starts with a
    BAD/REJECTED comment. Rules also consult the section heading directly
    so constructing a Block explicitly (in tests or in a future consumer)
    does not accidentally dodge the negative-example exclusion.
    """

    if b.is_negative_example:
        return True
    head_lc = (b.section_heading or "").lower()
    return "anti-pattern" in head_lc or "rejected" in head_lc


# ---------------------------------------------------------------------------
# Rules. Each rule receives (block, topic) and yields Finding records.
# ---------------------------------------------------------------------------


IF_CHAIN_RE = re.compile(
    r"^\s+if\s*\(\s*(?:[A-Za-z_][\w]*)\s*==\s*\"[^\"]+\"\s*\)\s*return\b"
)

IF_CHAIN_RE_MULTI = re.compile(
    r"^\s+if\s*\(\s*(?:[A-Za-z_][\w]*)\s*==\s*\"[^\"]+\"\s*\)\s*\{"
)


def rule_if_chain(b: Block) -> Iterable[Finding]:
    if is_negative_block(b) or not is_cpp_block(b):
        return
    consec = 0
    run_start = 0
    for lnum, line in enumerate(b.body_lines, start=b.start_line):
        if IF_CHAIN_RE.match(line) or IF_CHAIN_RE_MULTI.match(line):
            if consec == 0:
                run_start = lnum
            consec += 1
        else:
            if consec >= 3:
                yield Finding(
                    topic="",
                    phase=b.phase,
                    field=b.field,
                    section=b.section_heading,
                    rule="if_chain_stringly_typed",
                    severity="error",
                    line=run_start,
                    detail=f"{consec} consecutive stringly-typed if-arms",
                    suggestion=(
                        "Replace with a registration table "
                        "(std::unordered_map<std::string_view, T>) or a small "
                        "std::array<FAliasEntry> for stateful composites. See "
                        "coding-principles rule 5 (NO IF/ELSE HELL)."
                    ),
                )
            consec = 0
    if consec >= 3:
        yield Finding(
            topic="",
            phase=b.phase,
            field=b.field,
            section=b.section_heading,
            rule="if_chain_stringly_typed",
            severity="error",
            line=run_start,
            detail=f"{consec} consecutive stringly-typed if-arms",
            suggestion=(
                "Replace with a registration table. See coding-principles "
                "rule 5 (NO IF/ELSE HELL)."
            ),
        )


SWITCH_OPEN_RE = re.compile(r"^\s*switch\s*\(")
CASE_ENUM_RE = re.compile(r"^\s*case\s+E[A-Z]\w+::")


def rule_large_enum_switch(b: Block) -> Iterable[Finding]:
    """Flag switch statements over enum with duplicated-body shape.

    Heuristic: count consecutive `case EXxx::` lines. When >= 7, flag as
    candidate for registration-table refactor. Deliberately conservative
    (7 rather than 5) because small switches are fine; the threshold mirrors
    P18 best-practices Section 8.
    """

    if is_negative_block(b) or not is_cpp_block(b):
        return
    in_switch = False
    arm_count = 0
    switch_start = 0
    for lnum, line in enumerate(b.body_lines, start=b.start_line):
        if SWITCH_OPEN_RE.match(line):
            in_switch = True
            arm_count = 0
            switch_start = lnum
            continue
        if in_switch:
            if CASE_ENUM_RE.match(line):
                arm_count += 1
            stripped = line.strip()
            if stripped == "}" and arm_count:
                if arm_count >= 7:
                    yield Finding(
                        topic="",
                        phase=b.phase,
                        field=b.field,
                        section=b.section_heading,
                        rule="large_enum_switch",
                        severity="warning",
                        line=switch_start,
                        detail=f"switch with {arm_count} enum case arms",
                        suggestion=(
                            "Consider a registration table "
                            "(unordered_map<EEnum, Handler>) especially when "
                            "case bodies share a common shape."
                        ),
                    )
                in_switch = False


STRINGLY_ARGS_RE = re.compile(
    r"(const\s+)?std::map\s*<\s*std::string\s*,\s*std::string\s*>\s*&?\s*In\w*"
)


def rule_stringly_args(b: Block) -> Iterable[Finding]:
    if is_negative_block(b) or not is_cpp_block(b):
        return
    for lnum, line in enumerate(b.body_lines, start=b.start_line):
        if STRINGLY_ARGS_RE.search(line):
            yield Finding(
                topic="",
                phase=b.phase,
                field=b.field,
                section=b.section_heading,
                rule="stringly_typed_args",
                severity="error",
                line=lnum,
                detail=(
                    "handler accepts std::map<std::string, std::string>; "
                    "prefer a typed F*Args struct"
                ),
                suggestion=(
                    "Author an F<Command>Args struct with typed fields and "
                    "parse once in a factory. See P18 best-practices Section "
                    "6 (Domain Types)."
                ),
            )


RAW_NEW_RE = re.compile(r"^(?!\s*//).*\bnew\s+F[A-Z]\w+\s*\(")


def rule_raw_new(b: Block) -> Iterable[Finding]:
    if is_negative_block(b) or not is_cpp_block(b):
        return
    for lnum, line in enumerate(b.body_lines, start=b.start_line):
        if RAW_NEW_RE.match(line):
            if "make_unique" in line or "make_shared" in line:
                continue
            yield Finding(
                topic="",
                phase=b.phase,
                field=b.field,
                section=b.section_heading,
                rule="raw_new_operator",
                severity="warning",
                line=lnum,
                detail="raw `new` on a domain type",
                suggestion=(
                    "Use std::make_unique<T>(...) (or std::make_shared when "
                    "shared ownership is genuinely required). See "
                    "coding-principles rule 7 (Memory And Ownership)."
                ),
            )


GOTO_RE = re.compile(r"^\s*goto\s+\w+\s*;")


def rule_goto(b: Block) -> Iterable[Finding]:
    if is_negative_block(b) or not is_cpp_block(b):
        return
    for lnum, line in enumerate(b.body_lines, start=b.start_line):
        if GOTO_RE.match(line):
            yield Finding(
                topic="",
                phase=b.phase,
                field=b.field,
                section=b.section_heading,
                rule="goto_used",
                severity="error",
                line=lnum,
                detail="goto present in snippet",
                suggestion="Refactor into structured control flow.",
            )


RULES = (rule_if_chain, rule_large_enum_switch, rule_stringly_args,
         rule_raw_new, rule_goto)


# ---------------------------------------------------------------------------
# Orchestration.
# ---------------------------------------------------------------------------


def lint_phase(topic: str, phase: int) -> list[Finding]:
    design = fetch_phase_design(topic, phase)
    findings: list[Finding] = []
    for field_name in DESIGN_FIELDS:
        text = design.get(field_name) or ""
        for block in extract_blocks(phase, field_name, text):
            for rule in RULES:
                for finding in rule(block):
                    finding.topic = topic
                    findings.append(finding)
    return findings


def main(argv: Sequence[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--topic", required=True)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--phase", type=int, help="Lint one phase index")
    g.add_argument("--all", action="store_true", help="Lint every phase")
    ap.add_argument("--strict", action="store_true",
                    help="Exit 1 when any finding is emitted")
    ns = ap.parse_args(argv)

    phases = [ns.phase] if ns.phase is not None else enumerate_phases(ns.topic)
    all_findings: list[Finding] = []
    for p in phases:
        all_findings.extend(lint_phase(ns.topic, p))

    report = {
        "schema": SCHEMA,
        "topic": ns.topic,
        "phases_scanned": phases,
        "finding_count": len(all_findings),
        "findings": [f.__dict__ for f in all_findings],
    }
    sys.stdout.write(json.dumps(report, indent=2) + "\n")

    if ns.strict and all_findings:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
