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
| Content depth | Runtime metrics show enough plan detail for a future AI agent or junior developer: design chars, SOLID language, recursive words, field coverage, work items, tests, files, and evidence | `uni-plan phase metric --topic <T> --phase <N>` |
| Testing | For testable phases: testing records exist | `uni-plan phase get --topic <T> --phase <N> --execution` |
| Validation clean | No ErrorMajor issues for this topic | `uni-plan validate --topic <T>` |
| Snippet anti-pattern lint clean | No rejected code shapes in `code_snippets`, `code_entity_contract`, or `best_practices` | `python3 .agents/hooks/plan_snippet_antipattern.py --topic <T> --phase <N> --strict` |

If any gate is not met, populate the phase design material first. The
snippet lint scans for stringly-typed if-chains (3+ arms), enum switches
with 7+ case arms, stringly-typed handler args, raw `new F<Name>` without
a smart-pointer factory, and `goto`. Negative examples under an
`## Anti-Pattern ...` heading or prefixed with `// BAD:` are skipped. See
`.agents/rules/upl-plan-snippet-discipline.md`.

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

## Execution ergonomics (v0.105.0+)

On mutation-heavy phases, prefer these opt-in flags:

- `--ack-only` on every mutation emitting `uni-plan-mutation-v1`
  (topic set / phase set / task set / lane add / job add / changelog
  add / verification add / manifest add / phase complete / lane
  complete / phase cancel / every other mutation command). Switches
  the response envelope to `uni-plan-mutation-ack-v1` (flat
  `changed_fields[]` list), no `old`/`new` echo. On a dense phase
  execution the session transcript stays ~30-40% smaller vs. the
  default shape. Disk state, auto-changelog, and exit codes are
  unaffected. Not affected: `phase sync-execution` emits its own
  `uni-plan-sync-execution-v1` schema and silently ignores `--ack-only`.

- `--<field>-append-file <path>` on `phase set`'s 7 design-prose
  fields (`investigation`, `code_entity_contract`, `code_snippets`,
  `best_practices`, `multi_platforming`, `readiness_gate`, `handoff`).
  Appends to the existing stored value with a `\n\n` seam instead of
  replacing. Useful when extending design material during execution
  (e.g. appending a new finding to `investigation`) without pulling
  and concatenating locally.

- `task set --description` (with `--force --reason <text>` gate on
  non-NotStarted tasks). Use this - not `task remove` + `task add` -
  when a task description needs correction mid-execution. The forced
  path embeds the reason in the auto-changelog for audit.

- `uni-plan phase readiness --all-phases` replaces the old shell
  `for`-loop that forked one `uni-plan` process per phase. Single
  invocation emits the `uni-plan-phase-readiness-batch-v1` envelope
  with every phase's gate evaluation. Use it before the post-phase
  re-audit when you want a topic-wide readiness sweep in one call.

Dogfood pattern from the CliAgentErgonomics execution session: every
`task set --status completed` and `phase complete` invocation used
`--ack-only`, keeping the phase-close-out transcript roughly 40%
lighter than v0.104.1. (`phase sync-execution` was in the same chain
but emits its own compact `uni-plan-sync-execution-v1` schema, so
the flag is a no-op there.)

## Rules

- Never skip the post-phase re-audit
- Never advance a phase without meeting all entry gates
- Build-verify all code changes before marking phase complete
- Record evidence via `changelog add` and `verification add`
