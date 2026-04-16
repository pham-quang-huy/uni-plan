---
name: upl-plan-execution
description: Execute multi-phase plans with governance checkpoints. Use this skill when advancing a plan phase from not_started to in_progress to completed, or progressing through a plan's implementation. Automatically re-audits via upl-plan-audit after every phase completion.
implicit_invocation: true
---

# UPL Plan Execution

Use this skill to execute plan phases with proper governance gates and automatic re-auditing.

Treat the bundle itself as the execution source of truth. Use `phase[n]`-style references in bundle text and evidence, and only mention legacy phase keys when quoting a real historical filename. Prefer CLI mutations, but honor explicit user requests for direct manual bundle repair and re-validate immediately after.

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
| Design material | Phase has non-empty investigation and design fields | `uni-plan phase get --topic <T> --phase <N> --reference` |
| Content depth | Design material is substantive, not empty stubs | Review `--reference` output |
| Testing | For testable active phases: testing records exist with both manual and automation-capable coverage | `uni-plan phase get --topic <T> --phase <N> --execution` |
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
not_started → in_progress → completed → closed (optional)
                    ↓
                 blocked (with explicit blocker reason)
```

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
