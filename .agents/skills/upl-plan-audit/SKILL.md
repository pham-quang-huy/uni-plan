---
name: upl-plan-audit
description: Audit a plan topic through the uni-plan CLI. Use this skill whenever auditing a topic's .Plan.json bundle, detecting validation issues, checking phase readiness, or verifying a topic's health before starting a new phase. Also use when someone asks about plan status or document quality.
implicit_invocation: true
---

# UPL Plan Audit

Use this skill for CLI-first topic audits. uni-plan IS the audit tool — use it directly.

**HARD RULE - CLI-only access to `.Plan.json`.** Never `json.load` / raw JSON parsing on bundle files. Use `uni-plan topic get`, `phase list`, `phase get`, `phase metric`, `validate`, `blockers`, `changelog`, `verification`, and `manifest list` for audit evidence. If a needed query is not expressible through the CLI, report a CLI gap instead of raw-reading the bundle.

Treat `phases[n]`, `lanes[n]`, `waves[n]`, `jobs[n]`, and `tasks[n]` as the canonical bundle entity references. Legacy phase keys should be reported only when they are being used as live bundle references, not when they appear inside a real historical filename.

## Agentic Plan Audit Standard

Audit the bundle as if a future AI agent or junior developer must implement the
next phase using only the plan plus repo docs. A plan that is technically valid
but vague is still an audit finding when it omits target files, owned modules,
domain types, sequencing, acceptance evidence, or explicit non-goals.

Use the uni-plan refactor baseline as the code-quality lens for code-bearing
phases:

- Codex/project agents: `/Users/huypham/code/uni-plan/.agents/skills/upl-code-refactor/SKILL.md`
- Claude agents: `/Users/huypham/code/uni-plan/.claude/skills/upl-code-refactor/SKILL.md`

Flag phase material that fails to carry that baseline into executable guidance:
god structs, monolith files, string-keyed domain state, raw primitive domain
concepts, duplicated logic, workaround debt, missing `F`/`E`/`I` contracts, or
structural refactors mixed invisibly with behavior changes. Recommended fixes
must name the owning plan skill and the exact bundle fields to deepen.

## Workflow

### 1. Run CLI Audit Commands

```bash
# Topic overview (status, phases, metadata)
uni-plan topic get --topic <topic> --human

# Phase breakdown with status
uni-plan phase list --topic <topic> --human

# Runtime-only phase depth/intensity metrics
uni-plan phase metric --topic <topic> --human
uni-plan phase metric --topic <topic> --phase <N>
uni-plan phase metric --topic <topic> --all-phases --human   # v0.105.0+

# Specific phase detail (jobs, lanes, design material)
uni-plan phase get --topic <topic> --phase <N> --human

# V4 bundle validation (28 evaluators across structural + content-hygiene tiers)
uni-plan validate --topic <topic> --human
uni-plan validate --topic <topic> --strict --human   # gate on Warning + ErrorMinor too

# Blockers across all topics
uni-plan blockers --human

# Evidence history
uni-plan changelog --topic <topic> --human
uni-plan verification --topic <topic> --human
```

### 2. Phase Governance Gates

Flag violations when a phase advances to `in_progress` without satisfying these:

| Gate | Requirement |
|------|-------------|
| Design material | Phase has populated investigation, code entity contract, and testing fields |
| Content depth | `uni-plan phase metric` reports substantive `design_chars`, `solid_words`, recursive words, field coverage, work items, tests, files, and evidence |
| Testing fields | Testable active phases have testing records with actor and step fields, including both manual and automation-capable coverage (`human` + `ai`/`automated`) |
| Validation clean | `uni-plan validate --topic <topic>` reports no ErrorMajor issues; under `--strict`, no Warning or ErrorMinor issues either |
| Content hygiene | No V3 terminology drift (`v3_terminology_free`), no legacy `doc` CLI refs (`legacy_cli_free`), no smart quotes (`no_smart_quotes`), no placeholder literals like `"None"`/`"TBD"` (`no_empty_placeholder_literal`), no duplicate changelogs (`no_duplicate_changelog`), no stale `.Plan.md` refs (`stale_plan_md_reference`), no broken topic refs (`topic_ref_integrity`) |

### 2b. Snippet anti-pattern lint

Runtime depth and hygiene gates do not inspect code shapes inside
`code_snippets`, `code_entity_contract`, or `best_practices`. Run the
snippet lint whenever a phase carries code-bearing design material:

```bash
python3 .agents/hooks/plan_snippet_antipattern.py --topic <topic> --all --strict
```

The lint reports stringly-typed if-chains (3+ arms), enum switches with
7+ case arms, stringly-typed handler args, raw `new F<Name>` without a
smart-pointer factory, and `goto`. Negative examples under an
`## Anti-Pattern ...` heading or prefixed with `// BAD:` are skipped. See
`.agents/rules/upl-plan-snippet-discipline.md`.

### 3. Report Findings

Present structured findings in **table format**:

| # | Severity | Topic | Path | Finding | Recommended Fix |
|---|----------|-------|------|---------|----------------|
| 1 | critical | TopicName | phases[0].scope | empty scope field | Populate phase scope |

## Finding Taxonomy

Every finding must carry:

| Field | Content |
|-------|---------|
| severity | `critical`, `major`, or `minor` |
| kind | Validation or governance category |
| topic | Owning topic key |
| evidence | Concrete observed proof |
| recommended_fix | Actionable remediation |

## Audit Scope

### Single Topic
```bash
uni-plan topic get --topic <topic> --human
uni-plan phase list --topic <topic> --human
uni-plan validate --topic <topic> --human
```

### All Topics
```bash
uni-plan topic list --human
uni-plan validate --human
uni-plan blockers --human
```

## Verdict

- **PASS**: No critical or major findings
- **NEEDS REMEDIATION**: Major findings - fix before proceeding
- **BLOCKED**: Critical findings - must fix immediately

## Audit ergonomics (v0.105.0+)

During heavy audit sessions, prefer these opt-in shapes:

- `uni-plan phase readiness --topic <T> --all-phases` - one fork,
  one JSON envelope (`uni-plan-phase-readiness-batch-v1`) with every
  phase's gate evaluation inline. Replaces the shell `for`-loop
  pattern that forked one process per phase. `phase get` and
  `phase metric` also accept `--all-phases` as sugar for the v0.84.0
  batch path.
- `--ack-only` on every mutation command you touch during an
  audit-driven repair (e.g. `phase set --no-file-manifest=true` +
  reason, or a `phase cancel` on a superseded phase). Keeps the
  repair-session transcript lean without changing disk state or the
  audit trail.

These are read-path and response-shape improvements only - no
evaluator surface change. The 28+ evaluators emit the same findings
whether you sweep with `--all-phases` or iterate per-phase.

## Rules

- Read-only by default — do not edit repo files unless the user explicitly asks
- Use `uni-plan` CLI commands as the primary audit tool
- Keep human-facing summaries table-first
- Prefer `uni-plan` CLI output over manual file scanning
- Report in table format
