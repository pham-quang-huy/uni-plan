---
name: upl-plan-audit
description: Audit a plan topic through the uni-plan CLI. Use this skill whenever auditing a topic's plan/playbook/implementation bundle, detecting governance drift or missing artifacts, checking phase readiness gates, or verifying a topic's health before starting a new phase. Also use when someone asks about plan status or document quality.
implicit_invocation: true
---

# UPL Plan Audit

Use this skill for CLI-first topic audits. uni-plan IS the audit tool — use it directly.

## Workflow

### 1. Run CLI Audit Commands

```bash
# Topic artifact bundle
uni-plan artifacts --topic <topic>

# Plan phase table
uni-plan phase --topic <topic>

# Repo-wide validation
uni-plan validate

# Repo-wide drift detection
uni-plan diagnose drift

# Repo blockers
uni-plan blockers

# Lint all documents
uni-plan lint

# Section discovery for a specific doc
uni-plan section list --doc <path>

# Compare against schema
uni-plan section schema --type <plan|playbook|implementation>
```

### 2. Phase Governance Gates

Flag violations when a phase advances to `in_progress` without satisfying these:

| Gate | Requirement |
|------|-------------|
| Playbook-first | Phase needs a dedicated `<Topic>.<Phase>.Playbook.md` before starting |
| Schema compliance | All required sections present per schema |
| Content depth | Playbook has substantive content, not just section stubs |
| Testing procedures | Testable phases carry testing procedures in the playbook |

### 3. Report Findings

Present structured findings in **table format**:

| # | Severity | Artifact | Finding | Recommended Fix |
|---|----------|----------|---------|----------------|
| 1 | critical | TopicName.Plan.md | Missing `execution_strategy` section | Add section per Plan.Schema.md |

## Finding Taxonomy

Every finding must carry:

| Field | Content |
|-------|---------|
| severity | `critical`, `major`, or `minor` |
| kind | Drift or governance category |
| artifact | Owning document |
| evidence | Concrete observed proof |
| recommended_fix | Actionable remediation |

## Audit Scope

### Single Topic
```bash
uni-plan artifacts --topic <topic>
uni-plan phase --topic <topic>
```

### All Topics
```bash
uni-plan list --type pair
uni-plan validate
uni-plan diagnose drift
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
