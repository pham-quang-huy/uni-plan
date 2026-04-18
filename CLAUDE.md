# uni-plan

## project_overview

uni-plan is a standalone C++17 CLI tool for plan governance — managing, validating, and monitoring `.Plan.json` topic bundles across repositories. Each topic is a single JSON file containing phases, changelogs, verifications, and plan metadata.

- **Language**: C++17
- **Build system**: CMake 3.20+ with Ninja generator
- **Watch mode**: FTXUI terminal UI (conditional via `UPLAN_WATCH` CMake option)
- **Root namespace**: `UniPlan`
- **Binary**: `~/bin/uni-plan` (symlinked by `build.sh`)

## quick_reference

| Item | Value |
|------|-------|
| Language | C++17 |
| Build | CMake + Ninja |
| Namespace | `UniPlan` |
| Version source | `Source/UniPlanTypes.h` → `kCliVersion` |
| Binary | `~/bin/uni-plan` |
| Watch mode | FTXUI (optional, `UPLAN_WATCH=1`) |

## build_commands

```bash
# Full build + install
./build.sh

# Manual build
cmake -S . -B Build/CMake -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build Build/CMake -j "$(sysctl -n hw.logicalcpu)"

# With watch mode
cmake -S . -B Build/CMake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUPLAN_WATCH=1
cmake --build Build/CMake -j "$(sysctl -n hw.logicalcpu)"

# Verify
uni-plan --version
```

## key_conventions

Read these before modifying code:
- `CODING.md` — formatting, SOLID principles, memory ownership, error handling
- `NAMING.md` — type prefixes (F, E), member prefixes (m, mb, k), parameter prefixes (In, Out), pointer prefixes (rp)

## naming_quick_reference

| Prefix | Kind | Example |
|--------|------|---------|
| `F` | Struct (value type) | `FDocWatchSnapshot`, `FWatchPlanSummary` |
| `E` | Enum class | `EDocType`, `EPhaseStatus` |
| `m` | Member | `mPlanCount`, `mTopicKey` |
| `mb` | Boolean member | `mbOk`, `mbRequired` |
| `k` | Constant | `kCliVersion`, `kColorReset` |
| `In` | Input parameter | `InRepoRoot`, `InPlan` |
| `Out` | Output parameter | `OutLines`, `OutError` |
| `rp` | Raw pointer | `rpSidecar` |

Functions: PascalCase (`BuildWatchSnapshot`). Locals: PascalCase (`Stream`, `Index`). Acronyms: ALL CAPS (`ID`, `JSON`, `UTC`).

## coding_style

- 4-space indent, Allman braces, 80 column limit
- `#pragma once` in all headers
- `namespace UniPlan` — never `using namespace`
- Include order: own header → project headers → standard library
- All source files in `Source/` (flat directory)
- `std::cerr` for diagnostic output

## commit_message_format

```
type: Subject in Title Case
- Bullet point describing key change
- Another bullet point

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
```

**Types**: `feat`, `fix`, `chore`, `docs`, `refactor`, `test`, `perf`

## project_structure

```
uni-plan/
├── Source/                    # 19 C++ source files
│   ├── Main.cpp              # Entry point
│   ├── UniPlanTypes.h        # Types, version, JSON schema constants
│   ├── UniPlanHelpers.h      # String/JSON/file utilities
│   ├── UniPlanRuntime.cpp/h  # Runtime engine
│   ├── UniPlanParsing.cpp    # Document parsing
│   ├── UniPlanValidation.cpp # 18 V4 bundle evaluators + lint
│   ├── UniPlanCache.cpp      # Caching system
│   ├── UniPlanAnalysis.cpp   # Analysis operations
│   ├── UniPlanOptionParsing.cpp # CLI option parsing
│   ├── UniPlanOutputJson.cpp   # JSON output formatter
│   ├── UniPlanOutputText.cpp   # Text output formatter
│   ├── UniPlanOutputHuman.cpp  # ANSI human output formatter
│   ├── UniPlanWatchApp.cpp/h   # Watch mode TUI (FTXUI)
│   ├── UniPlanWatchPanels.cpp/h # Watch UI panels
│   ├── UniPlanWatchSnapshot.cpp/h # Watch data builder
│   └── UniPlanForwardDecls.h  # Forward declarations
├── Schemas/                   # 10 canonical schema files
│   ├── Doc.Schema.md          # Base document structure
│   ├── Plan.Schema.md         # Plan document schema
│   ├── Playbook.Schema.md     # Playbook schema
│   ├── Implementation.Schema.md # Implementation tracker schema
│   └── *ChangeLog.Schema.md / *Verification.Schema.md  # 6 sidecar schemas
├── Docs/                      # uni-plan's own development corpus
│   ├── INDEX.md               # Plan discovery index
│   ├── Plans/                 # Active .Plan.json bundles
│   ├── Implementation/        # Legacy markdown fixtures / historical references
│   └── Playbooks/             # Legacy markdown fixtures / historical references
├── ThirdParty/                # FTXUI (terminal UI library)
├── Build/                     # CMake output directory
├── .claude/                   # Claude Code system
│   ├── settings.json          # Hook configuration
│   ├── hooks/                 # 3 hook scripts
│   ├── rules/                 # 2 auto-loaded rule files
│   ├── skills/                # 12 skills (upl-* prefix)
│   └── agents/                # 1 agent (upl-agent-*)
├── CLAUDE.md                  # This file — project manifest
├── CODING.md                  # Code style and SOLID principles
├── NAMING.md                  # Naming conventions
├── .clang-format              # clang-format configuration
├── CMakeLists.txt             # Build configuration
├── build.sh                   # Build + install script
└── uni-plan.ini               # Runtime config (cache settings)
```

## claude_system_overview

| Component | Count | Location |
|-----------|-------|----------|
| Rules | 2 | `.claude/rules/` — auto-loaded coding and naming enforcement |
| Hooks | 3 | `.claude/hooks/` — using-namespace guard, pragma-once guard, auto-format |
| Skills | 12 | `.claude/skills/upl-*/` — see skill table below |
| Agents | 1 | `.claude/agents/upl-agent-senior-tester.md` — test coverage auditor |

## project_skills

| Skill | Path | When to use |
|-------|------|-------------|
| `upl-commit` | `.claude/skills/upl-commit/SKILL.md` | User requests a commit — validates format, SemVer gate |
| `upl-code-fix` | `.claude/skills/upl-code-fix/SKILL.md` | Fixing bugs — workaround gate, SOLID assessment |
| `upl-code-refactor` | `.claude/skills/upl-code-refactor/SKILL.md` | Structural cleanup — SOLID enforcement, detection patterns |
| `upl-schema-audit` | `.claude/skills/upl-schema-audit/SKILL.md` | Audit Schemas/*.Schema.md for consistency |
| `upl-validation-creation` | `.claude/skills/upl-validation-creation/SKILL.md` | Scaffold a new validation check — scaffold + wire |
| `upl-watch-panel` | `.claude/skills/upl-watch-panel/SKILL.md` | Add/modify watch mode TUI panels |
| `upl-claude-audit` | `.claude/skills/upl-claude-audit/SKILL.md` | Audit .claude/ system integrity |
| `upl-claude-learn` | `.claude/skills/upl-claude-learn/SKILL.md` | Learn from sessions, propose .claude/ updates |
| `upl-plan-creation` | `.claude/skills/upl-plan-creation/SKILL.md` | Create governed plan bundles |
| `upl-plan-execution` | `.claude/skills/upl-plan-execution/SKILL.md` | Execute plan phases with governance gates |
| `upl-plan-audit` | `.claude/skills/upl-plan-audit/SKILL.md` | Audit plan topics via uni-plan CLI |
| `upl-unit-test` | `.claude/skills/upl-unit-test/SKILL.md` | Build and run unit tests for all CLI commands |

## cli_semver_discipline

uni-plan is still **pre-1.0** (currently `0.50.0`) and under active
development. The command surface, mutation target path format,
validator output schema, and auto-changelog `affected` contract are all
subject to change. There is no stability commitment until we explicitly
ship v1.0.

While in the 0.x range:

| Bump | When |
|------|------|
| MINOR (0.x.0 → 0.(x+1).0) | New features or **any** breaking change: new commands, new flags, new validation checks, new output fields, command renames, removed options, schema format changes. All breaking + new-feature work shares MINOR while pre-1.0. |
| PATCH (0.x.y → 0.x.(y+1)) | Bug fixes, documentation, internal refactoring, performance improvements that do not change observable output |

**MAJOR will be reserved for v1.0 and later** once the CLI surface is
locked. Do not bump MAJOR while in the 0.x range.

**Version source**: `Source/UniPlanTypes.h` → `kCliVersion`
**Trigger files**: All files in `Source/`

Before committing any `Source/` changes, verify `kCliVersion` was bumped
appropriately for the kind of change introduced.

## documentation_rules

### V4 bundle access — CLI-only

**`.Plan.json` files MUST be accessed through the `uni-plan` CLI — never `json.load` / raw JSON parsing.** The CLI is the authoritative interface to the V4 bundle schema; raw reads bypass the typed domain model, validation, and schema-evolution guarantees. If a needed query isn't expressible via existing CLI commands, that is a CLI gap — report it and stop, do not work around with raw file reads. The `uni-plan validate` `summary` section and `uni-plan manifest list` command (both added in v0.71.0) cover the aggregate-query cases that previously tempted circumvention.

### V4 bundle model

Each active topic is a single `.Plan.json` file in `Docs/Plans/`. The bundle contains all plan metadata, phases (with lifecycle + design material), changelogs, and verifications. Legacy markdown implementation/playbook documents may still exist in this repo for regression coverage, fixture data, or historical reference, but they are not the active source of truth for bundle-governed topics.

| Item | Location |
|------|----------|
| Topic bundle | `Docs/Plans/<TopicPascalCase>.Plan.json` |
| Phases | Inline array in the bundle (`mPhases`) |
| Changelogs | Inline array in the bundle (`mChangeLogs`) |
| Verifications | Inline array in the bundle (`mVerifications`) |

### Legacy .md naming (lint only)

The `.md` naming patterns below apply to non-plan documentation plus legacy historical fixtures. Active plan execution uses `.Plan.json` bundles.

Bundle entity references should use `phases[n]`, `lanes[n]`, `waves[n]`, `jobs[n]`, and `tasks[n]` inside errors, changelogs, and docs. Legacy phase keys such as `P1` should only appear when quoting an actual historical filename.

| Doc Type | Pattern | Placement |
|----------|---------|-----------|
| Plan (legacy) | `<TopicPascalCase>.Plan.md` | `Docs/Plans/` |
| Implementation (legacy) | `<TopicPascalCase>.Impl.md` | `Docs/Implementation/` |
| Playbook (legacy) | `<TopicPascalCase>.<PhaseKey>.Playbook.md` | `Docs/Playbooks/` |

## schema_files

The 10 schema files in `Schemas/` are V3 legacy artifacts used only by `uni-plan lint` for markdown filename pattern checking. V4 bundle validation uses `ValidateAllBundles()` with 30 evaluator functions against `FTopicBundle` data — it does not read Schema.md files.

| Schema | Purpose (lint only) |
|--------|---------------------|
| `Plan.Schema.md` | Plan .md section ordering |
| `Playbook.Schema.md` | Playbook .md section ordering |
| `Implementation.Schema.md` | Implementation .md section ordering |
| `*ChangeLog.Schema.md` (3) | ChangeLog .md sidecar structure |
| `*Verification.Schema.md` (3) | Verification .md sidecar structure |
| `Doc.Schema.md` | Base .md document structure |

## validation_checks

`uni-plan validate [--topic <T>] [--strict] [--human]` runs 28 evaluator functions against every `.Plan.json` bundle. Checks are split into three tiers:

### Structural checks (ErrorMajor + ErrorMinor) — 15 checks

Required fields, index references, enum values, timestamp format, and referential integrity. `ErrorMajor` always flips `valid=false`; `ErrorMinor` only does so under `--strict`.

| Check ID | Severity | Scope |
|---|---|---|
| `required_fields` | ErrorMajor | topic key + title |
| `phases_present` | ErrorMajor | ≥1 phase |
| `phase_scope` | ErrorMinor | per-phase scope non-empty |
| `job_required_fields` / `task_required_fields` / `lane_required_fields` | ErrorMinor | required child fields |
| `job_lane_ref` | ErrorMinor | job→lane index valid |
| `changelog_phase_ref` / `verification_phase_ref` | ErrorMinor | phase index valid |
| `changelog_required_fields` / `verification_required_fields` | ErrorMinor | required sidecar fields |
| `testing_record_fields` / `file_manifest_fields` | ErrorMinor | array entry fields |
| `timestamp_format` | ErrorMinor | ISO 8601 format |

### Structural warnings (Warning) — 3 checks

| Check ID | Scope |
|---|---|
| `phase_tracking` | phase has populated `done`/`remaining` |
| `testing_actor_coverage` | phase has human + ai records |
| `canonical_entity_ref` | `changelogs[*].affected` path format |

### Content-hygiene checks (ErrorMinor + Warning) — 11 checks

Detect agent-safety hazards, format inconsistencies, and reference integrity in prose fields. All flip `valid=false` under `--strict`.

Per-phase scan scope is structurally partitioned into three field classes, each independently status-filtered via `EPhaseEvidenceScope { AllPhases, NotCompleted, CompletedOnly }`:

| Field class | Fields | Rationale |
|---|---|---|
| Prescriptive | `scope`, `output`, all `design.*`, `dependencies[].*`, `validation_commands[].*`, `lanes[].*`, `jobs[].{scope, output, exit_criteria}`, `jobs[].tasks[].description`, `testing[].{step, action, expected}` | Forward-looking contract — format hygiene applies always. |
| Evidence (inside design) | `jobs[].tasks[].{evidence, notes}`, `testing[].evidence` | Execution proof. |
| Lifecycle | `lifecycle.{done, remaining, blockers}` | Historical on completed phases. |

Per-check scope:

| Check | Prescriptive | Evidence | Lifecycle |
|---|---|---|---|
| `no_dev_absolute_path`, `no_hardcoded_endpoint`, `no_smart_quotes`, `no_html_in_prose` | All phases | AllPhases | — |
| `no_unresolved_marker` | All phases | CompletedOnly | CompletedOnly |
| `no_empty_placeholder_literal` | — | — | AllPhases |

V3-era vocabulary/filename/CLI drift checks (`v3_terminology_free`, `legacy_cli_free`, `stale_plan_md_reference`, `canonical_phase_ref_prose`) were removed in v0.63.0. Pattern-enumeration against free-text prose is an open-ended treadmill that can never be complete; once V3 artifacts are gone from the corpus, drift prevention belongs in `AGENTS.md` / `CLAUDE.md` authoring discipline and human review, not in validator regex.

| Check ID | Severity | What it catches |
|---|---|---|
| `no_dev_absolute_path` | ErrorMinor | `/Users/<name>/`, `/home/<name>/`, `C:\Users\<name>\` in prose |
| `topic_ref_integrity` | ErrorMinor | `<X>.Plan.json` references where X is not a real topic |
| `path_resolves` | ErrorMinor | Impossible path refs (`Docs/Implementation/X.Plan.json` — V4 bundles live at `Docs/Plans/`, not `Docs/Implementation/` or `Docs/Playbooks/`) |
| `no_hardcoded_endpoint` | Warning | `localhost:N`, `127.0.0.1`, `192.168.*.*`, `10.*.*.*` in prose |
| `validation_command_fields` | ErrorMinor / Warning | Each `FValidationCommand` record must have a non-empty `command` (ErrorMinor) and a non-empty `description` (Warning). |
| `validation_command_platform_consistency` | Warning | A validation command with Windows-specific backslash path segments (`\Windows-x64\`, `\Debug\`, `\Tools\`) must set `platform: windows`. |
| `no_smart_quotes` | Warning | Unicode curly quotes `" " ' '` and en/em-dashes |
| `no_html_in_prose` | Warning | `<br>`, `<div>`, `<span>`, `<p>`, `<hN>` tags |
| `no_empty_placeholder_literal` | Warning | `"None"`/`"N/A"`/`"TBD"`/`"-"` literal strings (use empty) |
| `no_unresolved_marker` | Warning | `TODO`/`FIXME`/`XXX`/`HACK`/`???` in prescriptive prose and completed-phase evidence/lifecycle |
| `no_duplicate_changelog` | Warning | Same `(phase, change)` tuple recorded ≥2 times |
| `no_duplicate_phase_field` | Warning | Two phases of the same bundle share byte-identical non-empty content (≥20 chars) in a prescriptive or lifecycle field (`scope`, `output`, `done`, `remaining`, `handoff`, `readiness_gate`, `investigation`, `code_entity_contract`, `code_snippets`, `best_practices`) — signature of a migration script that stamped the same template across many phases |
| `no_hollow_completed_phase` | Warning | A phase with `status=completed` but no execution evidence: empty `jobs[]`, empty `testing[]`, empty `file_manifest[]`, and both `code_snippets` and `investigation` empty. Catches governance lies where `completed` is claimed without substance. |

### `--strict` flag

Without `--strict`, only `ErrorMajor` flips `valid=false` (and exit code). With `--strict`, any `ErrorMinor` or `Warning` also flips `valid=false`. Use `--strict` in CI gates and governance audits.

### `line` field on each issue

Every issue in `issues[]` carries a `line` field (1-based, or `null` when unresolvable) pointing to the JSON line of the offending path. Human output renders a `Line` column. Resolved via a one-pass scanner over each bundle's raw text, built once per topic.

Example JSON issue:

```json
{
  "id": "no_smart_quotes",
  "severity": "warning",
  "topic": "AudioSFXSystem",
  "path": "changelogs[4].change",
  "line": 892,
  "detail": "unicode smart char: \u201C"
}
```

## cli_commands

### Query commands

```bash
uni-plan topic list [--status <filter>] [--human]
uni-plan topic get --topic <T> [--human]
uni-plan topic status [--human]                              # overview: counts + active phases
uni-plan phase list --topic <T> [--status <filter>] [--human]
uni-plan phase get --topic <T> --phase <N> [--brief|--execution|--reference] [--human]
uni-plan phase next --topic <T> [--human]                    # find next not_started phase + readiness
uni-plan phase readiness --topic <T> --phase <N> [--human]   # gate-by-gate status
uni-plan phase wave-status --topic <T> --phase <N> [--human] # per-wave job completion
uni-plan changelog --topic <T> [--phase <N>] [--human]
uni-plan verification --topic <T> [--phase <N>] [--human]
uni-plan timeline --topic <T> [--phase <N>] [--since <date>] [--human]
uni-plan blockers [--topic <T>] [--human]
uni-plan validate [--topic <T>] [--strict] [--human]
```

The default (non-`--human`) output is JSON with two top-level sections:
- `issues[]` — one entry per failing check (id, severity, topic, path, line, detail)
- `summary` — aggregate stats for cross-topic queries without raw JSON reads: `topic_count`, `topics[].phase_count`, `topics[].status_distribution`, and per-phase `scope_chars`, `output_chars`, `design_chars` (sum of investigation + code_entity_contract + code_snippets + best_practices + handoff + readiness_gate + multi_platforming), `jobs_count`, `testing_count`, `file_manifest_count`, `file_manifest_missing` (count of `file_manifest[*].file_path` entries that don't resolve on disk). Added in v0.71.0 so agents can audit the full corpus through a single CLI invocation.

### Semantic lifecycle commands

Enforce gates with hard errors. Prefer these over raw `set` commands.

```bash
# Topic lifecycle
uni-plan topic start --topic <T>                              # gate: not_started
uni-plan topic complete --topic <T> [--verification <text>]   # gate: all phases completed
uni-plan topic block --topic <T> --reason <text>              # gate: in_progress

# Phase lifecycle
uni-plan phase start --topic <T> --phase <N> [--context <t>]  # gate: not_started + design material
uni-plan phase complete --topic <T> --phase <N> --done <text> [--verification <text>]  # gate: in_progress
uni-plan phase block --topic <T> --phase <N> --reason <text>  # gate: in_progress
uni-plan phase unblock --topic <T> --phase <N>                # gate: blocked
uni-plan phase progress --topic <T> --phase <N> --done <text> --remaining <text>       # gate: in_progress
uni-plan phase complete-jobs --topic <T> --phase <N>          # bulk-complete all jobs

# Evidence shortcuts (phase-scoped with bounds check)
uni-plan phase log --topic <T> --phase <N> --change <text> [--type <type>] [--affected <text>]
uni-plan phase verify --topic <T> --phase <N> --check <text> [--result <text>] [--detail <text>]
```

### Raw mutation commands

Low-level field setters. Use semantic commands above when possible.

```bash
uni-plan topic set --topic <T> [--status <s>] [--next-actions <text>] [--summary <t>] [--goals <t>] [--non-goals <t>] [--risks <t>] [--acceptance-criteria <t>] [--problem-statement <t>] [--validation-commands <t>] [--baseline-audit <t>] [--execution-strategy <t>] [--locked-decisions <t>] [--source-references <t>] [--dependencies <t>]
uni-plan phase set --topic <T> --phase <N> [--status <s>] [--done <t>] [--remaining <t>] [--blockers <t>] [--context <t>] [--scope <t>] [--output <t>] [--investigation <t>] [--code-entity-contract <t>] [--code-snippets <t>] [--best-practices <t>] [--multi-platforming <t>] [--readiness-gate <t>] [--handoff <t>] [--validation-commands <t>] [--phase-dependencies <t>]
uni-plan phase add --topic <T> [--scope <t>] [--output <t>] [--status <s>]  # append trailing phase; default status=not_started
uni-plan phase remove --topic <T> --phase <N>  # trailing-only; requires not_started + no changelog/verification refs
uni-plan phase normalize --topic <T> --phase <N> [--dry-run]  # replace em/en/figure dashes -> hyphen, smart quotes -> straight, NBSP -> space across all phase prose fields
uni-plan job set --topic <T> --phase <N> --job <N> [--status <s>] [--scope <t>] [--output <t>] [--exit-criteria <t>] [--lane <N>] [--wave <N>]
uni-plan task set --topic <T> --phase <N> --job <N> --task <N> [--status <s>] [--evidence <t>] [--notes <t>]
uni-plan changelog add --topic <T> [--phase <N>] --change <text> [--type <feat|fix|refactor|chore>] [--affected <t>]
uni-plan changelog set --topic <T> --index <N> [--phase <N|topic>] [--date <YYYY-MM-DD>] [--change <t>] [--type <t>] [--affected <t>]
uni-plan verification add --topic <T> [--phase <N>] --check <text> [--result <text>] [--detail <text>]
uni-plan verification set --topic <T> --index <N> [--check <t>] [--result <t>] [--detail <t>]
uni-plan lane set --topic <T> --phase <N> --lane <N> [--status <s>] [--scope <t>] [--exit-criteria <t>]
uni-plan lane add --topic <T> --phase <N> [--status <s>] [--scope <t>] [--exit-criteria <t>]
uni-plan testing add --topic <T> --phase <N> --session <text> --step <text> --action <text> --expected <text> [--actor <human|ai|automated>] [--evidence <t>]
uni-plan testing set --topic <T> --phase <N> --index <N> [--session <t>] [--actor <t>] [--step <t>] [--action <t>] [--expected <t>] [--evidence <t>]
uni-plan manifest add --topic <T> --phase <N> --file <path> --action <create|modify|delete> --description <text>
uni-plan manifest remove --topic <T> --phase <N> --index <N>
uni-plan manifest list [--topic <T>] [--phase <N>] [--missing-only]  # enumerate file_manifest entries; --missing-only filters to paths that don't resolve on disk
uni-plan manifest set --topic <T> --phase <N> --index <N> [--file <t>] [--action <t>] [--description <t>]
```

### Utility commands

```bash
uni-plan cache info|clear|config [--dir <path>] [--human]
uni-plan watch [--repo-root <path>]
uni-plan lint [--human]  # legacy .md filename check only
```
