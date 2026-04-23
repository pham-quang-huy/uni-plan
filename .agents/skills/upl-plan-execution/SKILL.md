---
name: upl-plan-execution
description: Execute multi-phase plans with governance checkpoints. Use this skill when advancing a plan phase from not_started to in_progress to completed, or progressing through a plan's implementation. Automatically re-audits via upl-plan-audit after every phase completion.
implicit_invocation: true
---

# UPL Plan Execution

**HARD RULE - CLI-only access to `.Plan.json`.** Never `json.load` / raw JSON parsing on bundle files. All reads and mutations go through the `uni-plan` CLI. See `AGENTS.md` for the full rule.

**`--help` is the authoritative per-command reference (v0.85.0+).** Run `uni-plan <cmd> [<sub>] --help` for usage, required/optional flags, mutually exclusive modes, output schema, exit codes, and examples. Examples below use the most common modes; when a flag in this skill disagrees with what `--help` emits, trust `--help`.

Use this skill to execute plan phases with proper governance gates and automatic re-auditing.

## Agentic Execution Standard

Treat every phase as a work order written for a future AI agent or junior
developer. If the phase scope, lanes, jobs, tasks, target files, contracts, or
validation commands are too vague for that executor to proceed without
guessing, stop coding and deepen the bundle through `upl-plan-creation`
before `phase start`.

For code-bearing phases, apply the uni-plan refactor baseline while executing
and when repairing deficient plan material:

- Codex/project agents: `/Users/huypham/code/uni-plan/.agents/skills/upl-code-refactor/SKILL.md`
- Claude agents: `/Users/huypham/code/uni-plan/.claude/skills/upl-code-refactor/SKILL.md`

Preserve the phase boundary and the coding quality contract: structural cleanup
precedes behavior changes, domain concepts get typed `F`/`E`/`I` contracts,
monolith/god-struct/string-enum debt is not carried forward, and every
completed task has concrete evidence a junior developer could verify.

## Required Context

Before executing, read the topic bundle:
```bash
uni-plan topic get --topic <topic> --human
uni-plan phase get --topic <topic> --phase <N> --human
```

For code-bearing phases, also read `CODING.md` and `NAMING.md`.

## Workflow

### 1. Normalize

Verify the plan topic is in good shape:
```bash
uni-plan topic get --topic <topic> --human
uni-plan phase list --topic <topic> --human
uni-plan validate --topic <topic> --human
```

### 2. Determine Next Phase

Find the first phase with status `not_started` and check readiness:
```bash
uni-plan phase next --topic <topic> --human
uni-plan phase readiness --topic <topic> --phase <N> --human
```

### 3. Phase Entry Gate

Before marking a phase `in_progress`, verify ALL gates:

| Gate | Requirement | Check |
|------|-------------|-------|
| Design material | Phase has non-empty investigation and design fields | `uni-plan phase get --topic <T> --phase <N> --design` (renamed from `--reference` in v0.83.0) |
| Content depth | `design_chars ≥ 3000` (`kPhaseHollowChars`) clears `no_hollow_completed_phase`; `≥ 10000` (`kPhaseRichMinChars`) is the aspirational "rich" bar | Inspect `design_chars` header from `--design` output |
| Testing | For testable phases: testing records exist | `uni-plan phase get --topic <T> --phase <N> --execution` |
| Validation clean | No ErrorMajor issues for this topic | `uni-plan validate --topic <T>` |

If any gate is not met, populate the phase design material first.

### 4. Claim the Phase

```bash
uni-plan phase start --topic <topic> --phase <N> [--context "agent continuation prompt"]
```

This enforces: phase must be `not_started` and design material must be non-empty. Auto-sets `started_at`, auto-starts topic if `not_started`, and appends changelog.

### 5. Execute

Follow the phase's jobs and lanes:
```bash
uni-plan phase get --topic <topic> --phase <N> --execution --human
```

- Execute tasks per job
- For code-bearing jobs, enforce SOLID and naming conventions
- Build-verify after code changes: `./build.sh`
- Track progress as you go:

```bash
uni-plan phase progress --topic <topic> --phase <N> \
  --done "What was completed" \
  --remaining "What's left"
```

- Check wave-by-wave progress:

```bash
uni-plan phase wave-status --topic <topic> --phase <N> --human
```

### 6. Phase Completion

When all jobs in the phase are done:

1. **SOLID spot-check** (code-bearing phases):
   - No god structs (>50 fields mixing domains)
   - No if/else chains >3 branches on same key
   - No raw primitives representing domain concepts
   - If violations found, invoke `upl-code-refactor` before marking complete
2. **Close the phase** (enforces in_progress gate, auto-sets completed_at, auto-completes topic if all phases done):
   ```bash
   uni-plan phase complete --topic <topic> --phase <N> \
     --done "Final summary of delivered work" \
     --verification "Build passes, all exit criteria met"
   ```
3. **Record additional evidence** (if needed beyond the --verification above):
   ```bash
   uni-plan phase log --topic <topic> --phase <N> --change "Details..." --type feat
   uni-plan phase verify --topic <topic> --phase <N> --check "Visual parity" --result pass
   ```

### 7. Post-Phase Re-Audit (MANDATORY)

After completing any phase, validate:

```bash
uni-plan validate --topic <topic> --human
uni-plan blockers --topic <topic> --human
```

This ensures no governance drift was introduced.

### 8. Decide Next

After audit:
- If more phases remain: evaluate whether to continue or pause
- If all phases complete: `uni-plan topic complete --topic <topic>` (enforces all-phases-completed gate)
- If audit found issues: fix before proceeding

## Status Transitions

```
not_started ─┬─ in_progress ─┬─ completed → closed (optional)
             │               │
             │               └─ blocked ─┬─ in_progress (unblock)
             │                           │
             │                           └─ canceled (v0.89.0+)
             │
             └───────────────────────────── canceled (v0.89.0+)
```

`canceled` covers superseded phases (migration aliases, renumbered scopes, work shipped elsewhere) — terminal-but-not-completed. Unlike `completed`, no `completed_at` is stamped. Use `uni-plan phase cancel --topic <T> --phase <N> --reason <text>`; gates reject `completed` and `canceled` as starting states.

## Build Verification

For code-bearing phases, always verify:
```bash
./build.sh
```

## Rules

- Never skip the post-phase re-audit
- Never advance a phase without meeting all entry gates
- Build-verify all code changes before marking phase complete
- Record evidence via `changelog add` and `verification add`
