---
name: upl-plan-execution
description: Execute multi-phase plans with governance checkpoints. Use this skill when advancing a plan phase from not_started to in_progress to completed, executing playbook steps, or progressing through a plan's implementation. Automatically re-audits via upl-plan-audit after every phase completion.
implicit_invocation: true
---

# UPL Plan Execution

Use this skill to execute plan phases with proper governance gates and automatic re-auditing.

## Required Context

Before executing, read:
1. The plan document (`<Topic>.Plan.md`)
2. The implementation tracker (`<Topic>.Impl.md`)
3. The active phase playbook (`<Topic>.<PhaseKey>.Playbook.md`)
4. `CODING.md` and `NAMING.md` (for code-bearing phases)

## Workflow

### 1. Normalize

Verify the plan topic is in good shape:
```bash
uni-plan artifacts --topic <topic>
uni-plan phase list --topic <topic>
```

### 2. Determine Next Phase

Read the implementation tracker's `phase_tracking` table. Find the first phase with status `not_started` or `in_progress`.

### 3. Phase Entry Gate

Before marking a phase `in_progress`, verify ALL gates:

| Gate | Requirement | Check |
|------|-------------|-------|
| Playbook exists | `<Topic>.<PhaseKey>.Playbook.md` | File exists |
| Schema compliance | All required sections present per playbook schema | `uni-plan section list` vs `uni-plan section schema --type playbook` |
| Content depth | Active-phase playbook should be substantive | Not just section stubs |
| Testing procedures | For testable phases: `testing` section | Rows with expected results |

If any gate is not met, prepare the playbook first before proceeding.

### 4. Execute

Follow the playbook's execution lanes:
- Read `execution_lanes` for scope and exit criteria
- Execute tasks per lane
- For code-bearing jobs, enforce SOLID and naming conventions:
  - Each new type/file owns one responsibility
  - Use dispatch tables or polymorphism for extensible surfaces
  - Use `F`/`E`-prefix domain types — no raw primitives for domain concepts
  - No duplication — extract shared logic into reusable functions
- Build-verify after code changes: `./build.sh`
- Record evidence in the implementation tracker

### 5. Phase Completion

When all jobs in the phase are done:

1. **SOLID spot-check** (code-bearing phases):
   - No god structs (>50 fields mixing domains)
   - No if/else chains >3 branches on same key
   - No raw primitives representing domain concepts
   - No duplicated logic
   - If violations found, invoke `upl-code-refactor` before marking complete
2. **Update playbook**: Mark all `execution_lanes` as `completed`
3. **Update implementation tracker**: Record done/remaining in `phase_tracking`
4. **Record evidence**: Add entries to detached sidecars (ChangeLog + Verification)

### 6. Post-Phase Re-Audit (MANDATORY)

After completing any phase, invoke `upl-plan-audit`:

```bash
uni-plan artifacts --topic <topic>
uni-plan validate
uni-plan lint
```

This ensures:
- Playbook lane statuses are consistent with phase completion
- No governance drift was introduced
- Sidecars are up to date

### 7. Decide Next

After audit:
- If more phases remain: evaluate whether to continue or pause
- If all phases complete: mark plan as `completed` in both plan and tracker
- If audit found drift: fix drift before proceeding

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

## Doc Validation

After any document changes:
```bash
uni-plan lint
```

## Rules

- Never skip the post-phase re-audit
- Never advance a phase without meeting all entry gates
- Always synchronize plan and implementation tracker status
- Build-verify all code changes before marking phase complete
- Record evidence in detached sidecars, not inline in the plan
