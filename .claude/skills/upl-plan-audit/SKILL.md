---
name: upl-plan-audit
description: Audit a plan topic through the uni-plan CLI. Use this skill whenever auditing a topic's .Plan.json bundle, detecting validation issues, checking phase readiness, or verifying a topic's health before starting a new phase. Also use when someone asks about plan status or document quality.
implicit_invocation: true
---

# UPL Plan Audit

Use this skill for CLI-first topic audits. uni-plan IS the audit tool — use it directly.

Treat `phases[n]`, `lanes[n]`, `waves[n]`, `jobs[n]`, and `tasks[n]` as the canonical bundle entity references. Legacy phase keys should be reported only when they are being used as live bundle references, not when they appear inside a real historical filename.

## Workflow

### 1. Run CLI Audit Commands

```bash
# Topic overview (status, phases, metadata)
uni-plan topic get --topic <topic> --human

# Phase breakdown with status
uni-plan phase list --topic <topic> --human

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
| Content depth | Phase design material has substantive content, not empty strings |
| Testing fields | Testable active phases have testing records with actor and step fields, including both manual and automation-capable coverage (`human` + `ai`/`automated`) |
| Validation clean | `uni-plan validate --topic <topic>` reports no ErrorMajor issues; under `--strict`, no Warning or ErrorMinor issues either |
| Content hygiene | No smart quotes (`no_smart_quotes`), no HTML tags (`no_html_in_prose`), no dev-machine absolute paths (`no_dev_absolute_path`), no hardcoded endpoints (`no_hardcoded_endpoint`), no placeholder literals like `"None"`/`"TBD"` (`no_empty_placeholder_literal`), no unresolved markers in prescriptive prose or completed-phase evidence (`no_unresolved_marker`), no duplicate changelogs (`no_duplicate_changelog`), no duplicate phase-field content across `scope`/`output`/`done`/`remaining`/`handoff`/`readiness_gate`/`investigation`/`code_entity_contract`/`code_snippets`/`best_practices` (`no_duplicate_phase_field`), no hollow completed phases (`no_hollow_completed_phase` — `completed` status with zero jobs, zero testing, zero file manifest, and empty `code_snippets` + `investigation`), no broken topic refs (`topic_ref_integrity`), no impossible path refs (`path_resolves`), typed validation commands are well-formed (`validation_command_fields`, `validation_command_platform_consistency`) |

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
- **NEEDS REMEDIATION**: Major findings — fix before proceeding
- **BLOCKED**: Critical findings — must fix immediately

## Rules

- Read-only by default — do not edit repo files unless the user explicitly asks
- Use `uni-plan` CLI commands as the primary audit tool
- Keep human-facing summaries table-first
- Prefer `uni-plan` CLI output over manual file scanning
- Report in table format
