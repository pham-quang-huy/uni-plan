# uni-plan

uni-plan is a standalone C++17 CLI tool for plan governance — managing, validating, and monitoring `.Plan.json` topic bundles across repositories. Each topic lives in a single JSON file containing phases (with lifecycle + design material), changelogs, verifications, and topic metadata. This README is the entry point for AI agents working in this repo.

## hard_rule_cli_only

> Target: `.Plan.json` files are only ever touched through the `uni-plan` CLI. Raw JSON reads are prohibited.

**`.Plan.json` files MUST be read and mutated through the `uni-plan` CLI. Never `json.load`, never raw JSON parsing, never a manual editor pass for programmatic tasks.** The CLI is the authoritative interface to the V4 bundle schema; raw reads bypass the typed domain model, validation, and schema-evolution guarantees. If a needed query isn't expressible via an existing CLI command, that is a CLI gap — report it and stop.

| Aggregate-query need | Command added in `v0.71.0` |
| --- | --- |
| Corpus-wide topic/phase counts, char sizes, manifest stats | `uni-plan validate` → `summary` block |
| Enumerate every `file_manifest[]` entry (optionally missing-only) | `uni-plan manifest list [--topic <T>] [--phase <N>] [--missing-only]` |

This rule is repeated in [CLAUDE.md](CLAUDE.md), [AGENTS.md](AGENTS.md), and every `upl-plan-*` skill header.

## project_overview

| Item | Value |
| --- | --- |
| Language | C++17 |
| Build | CMake `3.20+` with Ninja generator |
| Root namespace | `UniPlan` |
| Version source | [Source/UniPlanTypes.h](Source/UniPlanTypes.h) → `kCliVersion` (currently `0.73.2`) |
| Binary | `~/bin/uni-plan` (symlinked by [build.sh](build.sh)) |
| Watch mode | FTXUI terminal UI (optional, `-DUPLAN_WATCH=1`) |
| Tests | GoogleTest, `./Build/CMake/uni-plan-tests` |

## non_negotiable_rules

| Rule ID | Rule |
| --- | --- |
| `R1` | `.Plan.json` access is CLI-only. Never `json.load`. If the CLI can't express a query, report the gap and stop. |
| `R2` | Fix root causes, not symptoms. No workarounds, no content-sniffing fallbacks, no backward-compat shims. |
| `R3` | Every `Source/` change must bump `kCliVersion` — MINOR for features/breaking, PATCH for fixes/docs/refactors. MAJOR is reserved for `v1.0`. |
| `R4` | Follow [CODING.md](CODING.md) and [NAMING.md](NAMING.md). Using `using namespace`, missing `#pragma once` in new headers, and format drift are blocked or auto-corrected by hooks. |
| `R5` | Only commit when the user explicitly asks. Use the prescribed format. Never `git commit --no-verify`. |

## data_fix_gate

> Target: never ship content-sniffing or fallback logic in a consumer when a producer can give a guarantee.

When data doesn't match expectations, STOP before writing any code and answer:

| Step | Question |
| --- | --- |
| 1 | What is the semantic contract this field must uphold? |
| 2 | Which layer is producing the wrong value? (source doc, schema, extraction, serializer) |
| 3 | What should that layer guarantee instead? |

Only after identifying the responsible layer, fix that layer. **Workaround smell**: any `if (value.find("..."))` that detects content to decide behavior. Rewrite as a producer-side guarantee.

## semver_discipline

| Bump | When |
| --- | --- |
| `MINOR` (`0.x.0` → `0.(x+1).0`) | New features *or* breaking changes: new commands, new flags, new validation checks, new output fields, renames, removals, schema format changes |
| `PATCH` (`0.x.y` → `0.x.(y+1)`) | Bug fixes, docs, refactoring with no observable output change |
| `MAJOR` | Reserved for `v1.0`. Do not bump while pre-1.0. |

Trigger files: every file under `Source/`. Version source: [Source/UniPlanTypes.h](Source/UniPlanTypes.h) → `kCliVersion`. The [upl-commit](.claude/skills/upl-commit/SKILL.md) skill gates this.

## commit_message_format

```
type: Subject in Title Case
- Bullet point describing key change
- Another bullet point

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
```

| Type | Usage |
| --- | --- |
| `feat` | New feature or breaking change |
| `fix` | Bug fix |
| `refactor` | Behavior-preserving structural change |
| `chore` | Build, tooling, or dependency bookkeeping |
| `docs` | Documentation only |
| `test` | Test-only change |
| `perf` | Performance improvement with no observable output change |

## claude_system_map

The repo ships a self-contained Claude Code operating system under `.claude/`: `2` auto-loaded rules, `3` tool-use hooks, `12` skills, `1` agent — all `upl-*` prefixed.

### rules_auto_loaded

| File | Enforces |
| --- | --- |
| [.claude/rules/upl-coding-principles.md](.claude/rules/upl-coding-principles.md) | SOLID, domain types, no if/else hell, memory ownership, no workarounds (with the Data Fix Gate) |
| [.claude/rules/upl-naming-enforcement.md](.claude/rules/upl-naming-enforcement.md) | Type / member / param / pointer prefixes, PascalCase, ALL-CAPS acronyms, `UniPlan::` namespace |

### hooks_preposttooluse

Configured in [.claude/settings.json](.claude/settings.json). Match on `Edit|Write` for `*.cpp|*.h|*.hpp|*.cc|*.cxx|*.mm`:

| Script | Phase | Behavior |
| --- | --- | --- |
| [upl-guard-using-namespace.sh](.claude/hooks/upl-guard-using-namespace.sh) | PreToolUse (Edit, Write) | Blocks `using namespace` in any edit/write. Exit 2 = hard reject. |
| [upl-guard-pragma-once.sh](.claude/hooks/upl-guard-pragma-once.sh) | PreToolUse (Write only) | Blocks new headers missing `#pragma once`. Excludes `Build/`, `ThirdParty/`. |
| [upl-auto-format.sh](.claude/hooks/upl-auto-format.sh) | PostToolUse (Edit, Write) | Runs `clang-format -i` on the touched file using the repo's [.clang-format](.clang-format). |

If a hook blocks your write, fix the root cause in your content — a hook exit-2 means the content violated policy, not that the hook is wrong. Do not bypass with `--no-verify` or similar.

### skills_invocable_via_slash

All skills live under `.claude/skills/` with `implicit_invocation: true`, so the harness surfaces them automatically when their description matches the current task.

| Skill | When to use |
| --- | --- |
| [upl-commit](.claude/skills/upl-commit/SKILL.md) | User requests a commit — validates format, blocks sensitive files, enforces SemVer |
| [upl-code-fix](.claude/skills/upl-code-fix/SKILL.md) | Fixing bugs — runs the 5-point workaround gate, SOLID assessment |
| [upl-code-refactor](.claude/skills/upl-code-refactor/SKILL.md) | Structural cleanup — SOLID enforcement, god-struct decomposition, behavior-preserving moves |
| [upl-plan-creation](.claude/skills/upl-plan-creation/SKILL.md) | Create a new governed `.Plan.json` topic bundle |
| [upl-plan-execution](.claude/skills/upl-plan-execution/SKILL.md) | Advance a phase through `not_started → in_progress → completed` with governance gates; re-audits via `upl-plan-audit` after each completion |
| [upl-plan-audit](.claude/skills/upl-plan-audit/SKILL.md) | Audit a topic's health — validation, phase readiness, hollow-phase detection — all via the CLI |
| [upl-validation-creation](.claude/skills/upl-validation-creation/SKILL.md) | Scaffold a new validation check — ID, Evaluate function, wiring, version bump |
| [upl-schema-audit](.claude/skills/upl-schema-audit/SKILL.md) | Audit legacy `Schemas/*.Schema.md` files (used only by `uni-plan lint`) |
| [upl-watch-panel](.claude/skills/upl-watch-panel/SKILL.md) | Add or modify FTXUI panels in `uni-plan watch` |
| [upl-unit-test](.claude/skills/upl-unit-test/SKILL.md) | Build and run unit tests; add new tests for a command |
| [upl-claude-audit](.claude/skills/upl-claude-audit/SKILL.md) | Audit the `.claude/` system itself for integrity and cross-file consistency |
| [upl-claude-learn](.claude/skills/upl-claude-learn/SKILL.md) | Propose `.claude/` updates based on patterns learned in the current session |

### agents_delegated_via_tool

| Agent | Role |
| --- | --- |
| [upl-agent-senior-tester](.claude/agents/upl-agent-senior-tester.md) | Read-only test coverage auditor. Builds a command × test-type matrix across all `Run*Command` functions, flags missing tests, JSON field mismatches, coverage gaps. Does **not** write test code. |

## cli_surface_summary

Full grammar (options, flags, every subcommand) lives in [CLAUDE.md](CLAUDE.md) and [AGENTS.md](AGENTS.md). Orientation table:

| Family | Purpose | Representative commands |
| --- | --- | --- |
| `query` | Read-only inspection; JSON by default, `--human` for ANSI tables | `topic list / get / status`, `phase list / get / next / readiness / wave-status`, `changelog`, `verification`, `timeline`, `blockers`, `validate` |
| `semantic_lifecycle` | Gated state transitions — prefer these over raw `set` | `topic start / complete / block`, `phase start / complete / block / unblock / progress / complete-jobs`, `phase log`, `phase verify` |
| `raw_mutation` | Low-level field setters; use only when a semantic command doesn't fit | `topic set`, `phase set / add / remove / normalize`, `job set`, `task set`, `changelog add / set`, `verification add / set`, `lane set / add`, `testing add / set`, `manifest add / remove / list / set` |
| `utility` | Operational commands | `cache info / clear / config`, `watch`, `lint` (legacy `.md` filename check) |

Default output is JSON with two top-level sections — `issues[]` and `summary` — so agents can consume any command without raw file reads.

## primary_use_cases

> Target: canonical agent recipes for the most common plan-governance actions. Every recipe is CLI-only — no raw JSON edits, no file opens.

### dependencies_field_model

> Target: understand what `dependencies` means before mutating it.

Typed list of `FBundleReference` entries, present at **both** plan-level ([FPlanMetadata.mDependencies](Source/UniPlanTopicTypes.h)) and phase-level ([FPhaseDesignMaterial.mDependencies](Source/UniPlanTopicTypes.h)). Each reference has a kind, topic key, phase index, path, and free-text note. Kinds:

| Kind | Purpose | Required segments |
| --- | --- | --- |
| `bundle` | Depend on another topic's whole plan | `<topic>` |
| `phase` | Depend on a specific phase in another topic | `<topic>` + `<phase>` |
| `governance` | Reference a repo governance doc (e.g. `CLAUDE.md`, `AGENTS.md`) | `<path>` |
| `external` | Reference a third-party doc or URL | `<path>` |

Validators enforce integrity: `topic_ref_integrity` (the target topic must exist), `path_resolves` (the path must resolve on disk for repo-local refs), `canonical_entity_ref` (changelog `affected` paths must match the `phases[N]` / `jobs[N]` / `lanes[N]` shape).

### blockers_field_model

> Target: understand the difference between a *dependency* (structural) and a *blocker* (narrative + status flip).

| Field | Scope | Type | Set by |
| --- | --- | --- | --- |
| Phase `mBlockers` | Phase-level only | Free-form `std::string` on `FPhaseLifecycle` | `phase block --reason <text>` (semantic, also flips status to `blocked`) or `phase set --blockers <text>` (raw) |
| Topic blockage | Topic-level | No dedicated string field — captured via `mStatus=Blocked` + auto-changelog entry | `topic block --reason <text>` |

A blocker text describes *why* work is paused right now. A dependency is a durable typed link that survives across blockages and informs readiness checks.

### record_a_topic_level_dependency

> Target: express that Topic A's plan depends on another topic or doc.

```bash
# Depend on Topic B's whole plan
uni-plan topic set --topic A \
  --dependency-add 'bundle|B||Docs/Plans/B.Plan.json|B must ship first'

# Depend on a specific phase of Topic B
uni-plan topic set --topic A \
  --dependency-add 'phase|B|3||Waiting on B phase 3 output'

# Clear all existing topic dependencies before adding fresh ones
uni-plan topic set --topic A --dependency-clear \
  --dependency-add 'phase|B|3||Refreshed dep after replan'
```

Grammar is pipe-delimited: `<kind>|<topic>|<phase>|<path>|<note>`. Empty segments are allowed; only the kind-specific required segments must be non-empty.

### record_a_phase_level_dependency

> Target: express that Phase N of Topic A depends on a specific phase of Topic B.

```bash
uni-plan phase set --topic A --phase N \
  --dependency-add 'phase|B|3||Phase 3 of B produces the schema that phase N consumes'
```

Same grammar as `topic set`. The dependency lives in `FPhaseDesignMaterial.mDependencies` and surfaces in `uni-plan phase get --execution`.

### plan_blocked_by_an_external_plans_phase

> Target: record that Topic A's *entire plan* is paused because Topic B phase 3 hasn't shipped.

```bash
# 1) Record the typed structural dependency (durable — stays past the blockage)
uni-plan topic set --topic A \
  --dependency-add 'phase|B|3||Waiting on B phase 3 output'

# 2) Flip topic status to blocked and log the narrative reason
uni-plan topic block --topic A --reason "Blocked by B[3]: schema not merged"
```

`topic block` gates on `status=in_progress`, flips status to `blocked`, and auto-appends a changelog entry. There is no semantic `topic unblock` — to resume, use `uni-plan topic set --topic A --status in_progress` once the dependency clears.

### phase_blocked_by_an_external_plans_phase

> Target: record that Phase N of Topic A is paused because Topic B phase 3 hasn't shipped.

```bash
# 1) Record the typed phase-to-phase dependency (once)
uni-plan phase set --topic A --phase N \
  --dependency-add 'phase|B|3||Requires B[3] telemetry schema'

# 2) Flip the phase to blocked; the reason text persists in mBlockers
uni-plan phase block --topic A --phase N \
  --reason "Blocked by B[3]: telemetry schema not merged"

# 3) When the external phase clears
uni-plan phase unblock --topic A --phase N
```

`phase block --reason` **replaces** `mBlockers`; it does not append. `phase unblock` flips `blocked → in_progress`, clears `mBlockers`, and auto-logs the transition. Typed dependencies are not touched by block/unblock — they remain as historical record.

### backfill_missing_phase_timestamps

> Target: a phase's `started_at` or `completed_at` is missing (e.g. migrated from legacy data, or completed by a caller that skipped the `phase start` step). Repair them with explicit ISO-8601 overrides.

```bash
# Normal completion (started_at was stamped by an earlier phase start / phase set --status in_progress)
uni-plan phase complete --topic A --phase N --done "<summary>"

# Backfill completion — phase jumps straight from not_started to completed.
# --started-at is REQUIRED because the CLI refuses to fabricate a historical
# start time. --completed-at is optional (defaults to "now", the truthful
# moment the transition is being recorded).
uni-plan phase set --topic A --phase N --status completed \
  --started-at 2025-12-01T14:00:00Z --completed-at 2025-12-05T18:30:00Z

# Repair timestamps on a phase that is already in the right status
uni-plan phase set --topic A --phase N \
  --started-at 2025-12-01T14:00:00Z --completed-at 2025-12-05T18:30:00Z
```

Both flags validate ISO-8601 at parse time (`YYYY-MM-DD` or `YYYY-MM-DDThh:mm:ssZ`); invalid strings fail with `UsageError` (exit 2). In v0.73.1, `phase set --status completed` **rejects** any call where the phase has empty `started_at` and no `--started-at` override — this prevents silent fabrication of historical data and is what makes the `completed_phase_timestamp_required` structural-warning honest rather than cosmetic.

### patch_one_word_of_a_long_prose_field

> Target: change a single word (or insert a paragraph) in a long `investigation` / `scope` / `code_snippets` / `handoff` field. **The CLI does not support partial-field patching** — every mutation is a whole-field replace.

Read → edit in memory → write back:

```bash
# 1) Read the current field via a query command (JSON output by default)
uni-plan phase get --topic A --phase N > /tmp/phase.json
# Extract the field: use jq, your agent's JSON parser, or any tool

# 2) Edit the extracted string locally (sed, agent buffer, editor)

# 3) Write the whole new string back
uni-plan phase set --topic A --phase N --investigation "$(cat /tmp/new_investigation.txt)"
```

This pattern applies to every long text field (`scope`, `output`, `investigation`, `code_snippets`, `code_entity_contract`, `best_practices`, `handoff`, `readiness_gate`, `multi_platforming`, `done`, `remaining`). Never edit the `.Plan.json` file directly — raw writes bypass validation, timestamp updates, auto-changelog emission, and the typed domain model (violates R1).

### reference_governance_or_external_docs

> Target: mark reliance on a non-bundle doc — repo governance (`CLAUDE.md`, `AGENTS.md`), an RFC, or an external URL.

```bash
# Governance (repo-local)
uni-plan phase set --topic A --phase 0 \
  --dependency-add 'governance|||CLAUDE.md|Naming rules enforced by hooks'

# External (third-party spec or URL)
uni-plan topic set --topic A \
  --dependency-add 'external|||https://example.org/spec.pdf|Reference spec v2'
```

For `governance` and `external`, the `<topic>` and `<phase>` segments are empty; `<path>` is required.

### enumerate_currently_blocked_work

> Target: list every phase currently in `blocked` status with its reason text.

```bash
uni-plan blockers                # every topic
uni-plan blockers --topic A      # scope to one topic
uni-plan blockers --human        # ANSI table rendering
```

### find_the_next_phase_to_execute

> Target: ask the CLI which phase is ready to start and what gates still need to pass.

```bash
uni-plan phase next --topic A               # find next not_started phase + readiness summary
uni-plan phase readiness --topic A --phase 3  # gate-by-gate status for a specific phase
```

`phase next` surfaces both the candidate phase and its readiness report, so an agent can decide whether to `phase start` or finish remaining prerequisites first.

### audit_the_entire_corpus_through_one_command

> Target: get aggregate stats across every `.Plan.json` bundle without raw JSON reads.

```bash
uni-plan validate                # issues[] + summary block (topic_count, per-phase char sizes, manifest stats)
uni-plan validate --strict       # ErrorMinor + Warning also flip valid=false
uni-plan manifest list --missing-only  # every file_manifest entry that doesn't resolve on disk
```

These three commands (added in `v0.71.0`) replaced the previous temptation to `json.load` each bundle for cross-topic statistics. If your analytical need isn't expressible here, report the CLI gap per R1 rather than falling back to raw reads.

## validation_surface

> Target: every `.Plan.json` bundle passes three severity tiers covering structural integrity, structural warnings, and content hygiene.

### three_tier_evaluator_set

| Tier | Severity | Covers |
| --- | --- | --- |
| Structural | `ErrorMajor` + `ErrorMinor` | Required fields, index references, enum values, timestamp format, referential integrity |
| Structural warnings | `Warning` | `phase_tracking`, `testing_actor_coverage`, `canonical_entity_ref` |
| Content hygiene | `ErrorMinor` + `Warning` | Dev absolute paths, unknown topic refs, impossible path refs, hardcoded endpoints, smart quotes, HTML in prose, empty-placeholder literals, unresolved markers (`TODO`/`FIXME`/`XXX`/`HACK`/`???`), duplicate changelog entries, duplicate migration-stamp phase fields, hollow completed phases, validation-command hygiene |

### strict_flag_semantics

| Flag | Severities that flip `valid=false` | Exit on failure | Use |
| --- | --- | --- | --- |
| (default) | `ErrorMajor` only | `1` | Day-to-day inspection |
| `--strict` | `ErrorMajor` + `ErrorMinor` + `Warning` | `1` | CI gates, governance audits |

### issue_line_field

Every entry in `issues[]` carries a `line` field (1-based, or `null` if unresolvable) pointing to the JSON line of the offending path. Human output renders a `Line` column between `Topic` and `Path`.

V3-era vocabulary/filename/CLI drift checks were removed in `v0.63.0`. Pattern enumeration against free-text prose is intrinsically incomplete — drift prevention belongs in authoring discipline, not validator regex.

## project_structure

```
uni-plan/
├── Source/                    # C++17 implementation (flat directory, ~52 files)
│   ├── Main.cpp               # Entry point
│   ├── UniPlanTypes.h         # IWYU umbrella — re-exports the 4 domain headers below
│   ├── UniPlanCliConstants.h  # Schemas / colors / sidecar extensions / mutation targets
│   ├── UniPlanOptionTypes.h   # BaseOptions + every F*Options + UsageError (enum fields: std::optional<E*>)
│   ├── UniPlanInventoryTypes.h # V3 markdown inventory types (lint only)
│   ├── UniPlanResultTypes.h   # Per-command result / data structs
│   ├── UniPlanTopicTypes.h    # FTopicBundle, FPhaseRecord, FPhaseLifecycle, FPlanMetadata
│   ├── UniPlanTaxonomyTypes.h # FBundleReference, FValidationCommand, FPhaseTaxonomy
│   ├── UniPlanEnums.h         # EExecutionStatus, EDependencyKind, EChangeType, ETopicStatus, …
│   ├── UniPlanForwardDecls.h  # Run*Command declarations
│   ├── UniPlanCommand*.cpp    # Per-group command implementations (post-v0.72.0 split):
│   │                          #   Dispatch, Bundle, Topic, Phase, Validate, History,
│   │                          #   Lifecycle, Mutation, Entity, SemanticQuery, MutationCommon
│   ├── UniPlanValidation.cpp       # Structural + structural-warning evaluators + lint
│   ├── UniPlanValidationContent.cpp # Content-hygiene evaluators (split from Validation.cpp in v0.72.0)
│   ├── UniPlanSchemaValidation.cpp/h # Schema-driven structural validation
│   ├── UniPlanJSON.h / UniPlanJSONIO.cpp/h / UniPlanJSONLineIndex.cpp/h # nlohmann adaptor + bundle serialize + line index
│   ├── UniPlanCache.cpp       # Caching layer (uni-plan.ini)
│   ├── UniPlanOutputJSON.cpp / UniPlanOutputText.cpp / UniPlanOutputHuman.cpp # Output formatters
│   ├── UniPlanWatchApp.cpp/h / UniPlanWatchPanels.cpp/h / UniPlanWatchSnapshot.cpp/h # FTXUI watch TUI
│   ├── UniPlanOptionParsing.cpp # CLI option parsing (enum fields validated at parse time)
│   ├── UniPlanParsing.cpp     # V4 bundle / markdown parsing
│   ├── UniPlanRuntime.cpp/h   # Runtime engine
│   └── UniPlan*Helpers.h      # Domain helpers (String, File, JSON, Inventory, Markdown, Status, Output)
├── Schemas/                   # 10 canonical V3 schema files (used only by `uni-plan lint`)
├── Docs/                      # Plan corpus — active .Plan.json bundles live in Docs/Plans/
├── Test/                      # GoogleTest suite (187 tests as of v0.72.0)
├── ThirdParty/                # FTXUI
├── Build/                     # CMake output (ignored)
├── .claude/                   # Rules, hooks, skills, agents
├── CLAUDE.md                  # Project manifest for Claude Code
├── AGENTS.md                  # Parallel manifest for other agent harnesses (parity with CLAUDE.md)
├── CODING.md                  # Code style and SOLID principles
├── NAMING.md                  # Naming conventions
├── .clang-format              # Formatter config (consumed by the auto-format hook)
├── CMakeLists.txt             # Build configuration
├── build.sh                   # Build + install driver
└── uni-plan.ini               # Runtime cache config
```

# development

## build_commands

| Step | Command |
| --- | --- |
| Full build + install | `./build.sh` |
| Verify install | `uni-plan --version` |
| Run unit tests | `./Build/CMake/uni-plan-tests` |
| Manual configure (debug) | `cmake -S . -B Build/CMake -G Ninja -DCMAKE_BUILD_TYPE=Debug` |
| Manual configure (with watch TUI) | `cmake -S . -B Build/CMake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUPLAN_WATCH=1` |
| Manual build | `cmake --build Build/CMake -j "$(sysctl -n hw.logicalcpu)"` |

## documentation_map

| If you need… | Read |
| --- | --- |
| Full CLI grammar with every flag (Claude Code harness) | [CLAUDE.md](CLAUDE.md) → `cli_commands` |
| Same content, harness-neutral manifest | [AGENTS.md](AGENTS.md) |
| Code style rules (enforced) | [CODING.md](CODING.md) |
| Naming rules (enforced) | [NAMING.md](NAMING.md) |
| A specific skill's workflow | `.claude/skills/upl-<name>/SKILL.md` |
| Hook source | `.claude/hooks/*.sh` |
| Test coverage rubric | [.claude/agents/upl-agent-senior-tester.md](.claude/agents/upl-agent-senior-tester.md) |

## editor_setup

| Item | Guidance |
| --- | --- |
| Formatter | `clang-format` via the repo's [.clang-format](.clang-format); the [upl-auto-format.sh](.claude/hooks/upl-auto-format.sh) hook runs it on every Edit/Write |
| Indent | 4 spaces, no tabs |
| Braces | Allman style (opening brace on its own line) |
| Column limit | 80 characters |
| Headers | `#pragma once` required; [upl-guard-pragma-once.sh](.claude/hooks/upl-guard-pragma-once.sh) blocks new headers without it |
| Namespace discipline | `UniPlan::` fully qualified paths; `using namespace` is blocked by [upl-guard-using-namespace.sh](.claude/hooks/upl-guard-using-namespace.sh) |
| Diagnostic output | `std::cerr` — no custom log macros |
