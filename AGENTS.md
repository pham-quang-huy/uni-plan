# uni-plan

## hard_rule_cli_only

**`.Plan.json` files MUST be accessed through the `uni-plan` CLI — never `json.load` / raw JSON parsing.** The CLI is the authoritative interface to the V4 bundle schema; raw reads bypass the typed domain model, validation, and schema-evolution guarantees. If a needed query isn't expressible via existing CLI commands, that is a CLI gap — report it and stop, do not work around with raw file reads. The `uni-plan validate` `summary` section and `uni-plan manifest list` command (both added in v0.71.0) cover the aggregate-query cases that previously tempted circumvention.

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

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
```

**Types**: `feat`, `fix`, `chore`, `docs`, `refactor`, `test`, `perf`

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
├── Docs/                      # uni-plan's own development corpus
│   ├── INDEX.md               # Plan discovery index
│   ├── Plans/                 # Active .Plan.json bundles
│   ├── Implementation/        # Legacy markdown fixtures / historical references
│   └── Playbooks/             # Legacy markdown fixtures / historical references
├── Test/                      # GoogleTest suite (187 tests as of v0.72.0)
├── ThirdParty/                # FTXUI (terminal UI library)
├── Build/                     # CMake output directory
├── .claude/                   # Claude Code system
│   ├── settings.json          # Hook configuration
│   ├── hooks/                 # 3 hook scripts
│   ├── rules/                 # 2 auto-loaded rule files
│   ├── skills/                # 12 skills (upl-* prefix)
│   └── agents/                # 1 agent (upl-agent-*)
├── README.md                  # Root agent-focused entry point
├── AGENTS.md                  # This file — project manifest
├── CLAUDE.md                  # Parallel manifest for Claude Code (parity with AGENTS.md)
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

uni-plan is still **pre-1.0** (currently `0.76.0`) and under active
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

### v0.72.0 behavior note — parse-time enum validation

Invalid option enum values (`--status`, `--type`, `--actor`, `--action`, any `topic/phase/job/task/lane` status flag) now fail at **parse time** with **exit code 2** and a `UsageError`, instead of the previous deferred mutation-time exit 1. Valid values are enumerated in CLI `--help` output and in `Source/UniPlanEnums.h`. Option structs now store enum fields as `std::optional<E*>` — a missing flag is a real `std::nullopt`, not a sentinel string. Scripts that relied on exit 1 for invalid enum inputs must be updated to check exit 2.

### v0.73.1 behavior note — `phase set` timestamp overrides and completed-phase gate

Two new flags on `phase set`, intended for migration/repair passes that need to backfill historical timestamps instead of stamping "now":

- `--started-at <iso>` — explicit `mStartedAt` override. ISO-8601 format validated at parse time; invalid values fail with `UsageError` (exit 2).
- `--completed-at <iso>` — explicit `mCompletedAt` override (same parse-time validation).

Transitioning a phase to `status=completed` when `started_at` is empty now **requires** `--started-at <iso>` to be supplied explicitly; otherwise the command fails with `UsageError` (exit 2). This enforces the Data Fix Gate — the CLI will not fabricate a historical start time from `completed_at` or "now". The normal execution path (`phase start` / `phase set --status in_progress` then `phase complete` / `phase set --status completed`) already stamps `started_at` at the in_progress transition, so this gate only fires when callers skip straight from `not_started` to `completed`. The new `completed_phase_timestamp_required` structural-warning flags any persisted phase that already violates the invariant — `completed` phases need both timestamps, `in_progress`/`blocked` phases need at least `started_at`.

### v0.75.0 behavior note — legacy-gap stateless + `phases[].origin` stamp

The v0.74.0 `legacy_sources[]` schema field (both topic-level and per-phase) has been **removed**. Storing repo-relative paths to transient V3 `.md` files is the wrong durability class: once the legacy markdown corpus is deleted, every stored path becomes dangling. The replacement splits that concern in two:

1. **Semantic provenance — `phases[].origin`** (new optional field):
   - Enum `EPhaseOrigin { NativeV4, V3Migration }` serialized as `"native_v4"` / `"v3_migration"`.
   - Durable once stamped; filesystem-independent. Records *whether* the phase was migrated from V3, not *what file* it came from.
   - Absence on read maps to `NativeV4` for backward compatibility with pre-0.75.0 bundles. Serialization always emits the value so downstream tools see one canonical shape.
   - Stamp via `uni-plan phase set --topic <T> --phase <N>` once a phase-level `--origin <value>` flag lands; for now, migrations can hand-stamp via bundle mutation during initial creation.

2. **Filesystem-driven parity audit — `uni-plan legacy-gap` is now stateless**:
   - Discovers V3 `.md` artifacts by filename convention (`<Topic>.Plan.md`, `<Topic>.Impl.md`, `<Topic>.<PhaseKey>.Playbook.md`, and their `.ChangeLog.md` / `.Verification.md` sidecars) **at invoke time**, not from a stored bundle index.
   - Same per-phase `EPhaseGapCategory` output as before: `legacy_rich | legacy_rich_matched | legacy_thin | legacy_stub | legacy_absent | v4_only | hollow_both | drift`. Thresholds are unchanged (versioned constants in `UniPlanCommandLegacyGap.cpp`):
     - `legacy_rich`: legacy ≥150 LOC AND V4 design_chars < 500
     - `legacy_rich_matched`: legacy ≥150 LOC AND V4 design_chars ≥ 2000
     - `legacy_thin`: legacy 50–149 LOC
     - `legacy_stub`: legacy <50 LOC
     - `legacy_absent`: no legacy playbook
     - `v4_only`: no legacy AND V4 ≥2000 chars AND ≥3 jobs
     - `hollow_both`: legacy <50 LOC AND completed phase with V4 <500 chars
     - `drift`: reserved for future semantic-overlap detection
   - After the legacy corpus is deleted, every row falls into `legacy_absent` / `v4_only` — the correct steady state.
   - Relies on `LegacyMdContentLineCount` in `UniPlanFileHelpers.h`, which strips the 10-line V3 archival banner (`> **ARCHIVAL — V3 legacy markdown artifact.**`) from each file before counting.

**Removed in 0.75.0**:
- `uni-plan legacy-scan` subcommand (both the writer and the dry-run variant)
- `legacy_source_path_resolves` validator
- Topic-level `legacy_sources[]` and per-phase `phases[N].legacy_sources[]` schema fields
- `$defs/legacy_md_source` schema definition (the `{kind, path}` record)
- `ELegacyMdKind` enum and `FLegacyMdSource` type
- `FLegacyScanOptions` / `FLegacyScanReport` / `FLegacyScanHit`
- `ParseLegacyScanOptions` parser and `RunLegacyScanCommand` handler
- `ValidateAllBundles` second parameter (`const fs::path &InRepoRoot`) — signature reverts to `ValidateAllBundles(const std::vector<FTopicBundle>&)`

Breaking for callers: any bundle serialized by 0.74.0 that carries `legacy_sources[]` arrays will have those silently dropped on the next 0.75.0 round-trip (deserializer does not recognize the key). Any CI that depended on `legacy-scan` or `legacy_source_path_resolves` must migrate.

Kept unchanged: `uni-plan legacy-gap`, `EPhaseGapCategory`, `FPhaseGapRow`, `FLegacyGapReport`, `FLegacyGapOptions`, `ParseLegacyGapOptions`, `RunLegacyGapCommand`, `LegacyMdContentLineCount` helper.

### v0.76.0 behavior note — `--<field>-file <path>` input path for every prose setter

Every prose-setting flag on every mutation command now has a `-file` sibling that reads the field value as raw bytes from disk. This closes a real correctness hazard: the `--investigation "$(cat /tmp/inv.txt)"` idiom expands `$VAR`, `$(...)`, and backticks inside the shell's double quotes *before* the CLI sees the content. A field containing legitimate prose like ``use `$PATH`, `$(pwd)`, or "quoted" text`` silently corrupts on that path.

The file-based form sidesteps bash entirely:

```bash
uni-plan phase set --topic A --phase N --investigation-file /tmp/inv.txt
uni-plan topic set --topic A --summary-file /tmp/summary.txt
uni-plan phase log --topic A --phase N --change-file /tmp/change.txt --type feat
uni-plan verification add --topic A --phase N --check-file /tmp/check.txt
```

Siblings exist for every prose flag, including (but not limited to): `--summary-file`, `--goals-file`, `--non-goals-file`, `--risks-file`, `--acceptance-criteria-file`, `--problem-statement-file`, `--validation-commands-file`, `--baseline-audit-file`, `--execution-strategy-file`, `--locked-decisions-file`, `--source-references-file`, `--next-actions-file`, `--done-file`, `--remaining-file`, `--blockers-file`, `--context-file`, `--scope-file`, `--output-file`, `--investigation-file`, `--code-entity-contract-file`, `--code-snippets-file`, `--best-practices-file`, `--multi-platforming-file`, `--readiness-gate-file`, `--handoff-file`, `--exit-criteria-file`, `--evidence-file`, `--notes-file`, `--change-file`, `--affected-file`, `--check-file`, `--result-file`, `--detail-file`, `--step-file`, `--action-file`, `--expected-file`, `--description-file`, `--reason-file`, `--verification-file`.

Shared implementation:

- `TryReadFileToString` in `Source/UniPlanFileHelpers.h` — opens in binary mode, slurps byte-identically, no trimming, no line processing. Errors map to `UsageError` (exit 2) at parse time.
- `TryConsumeStringOrFileOption` in `Source/UniPlanOptionParsing.cpp` — the one call site used by every parser. Each parser dropped its duplicate `if (Token == "--X") { ... }` branches in favor of `if (TryConsumeStringOrFileOption(..., "--X", "--X-file", Options.mX)) continue;`. Keeps per-parser call sites symmetric and the shell-escape hazard cannot re-enter through ad-hoc branches.
- Help text: `PrintCommandUsage` appends a single shared `kFileFlagFooter` note to commands with prose flags (topic, phase, job, task, changelog, verification). lane / testing / manifest dispatch through per-subcommand handlers rather than `kCommandHelp`; their `-file` flags work but the discoverability note lives in these docs rather than in `--help` output.

Regression tests in `Test/UniPlanTestOptionParsing.cpp` round-trip shell-hostile content (``\`$PATH\` ... $(pwd) ... "quoted"``) through both `--investigation` and `--investigation-file` and assert byte-identical storage. The inline form preserves content only when shell escaping is done correctly by the caller; the file form preserves it unconditionally.

The inline `--<field> <text>` path is unchanged — short, shell-safe values still work via the original form. Both paths are interchangeable when the content is clean; the file form is mandatory only when the content could contain `$`, `` ` ``, `"`, `\`, or newlines.

## documentation_rules

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

The 10 schema files in `Schemas/` are V3 legacy artifacts used only by `uni-plan lint` for markdown filename pattern checking. V4 bundle validation uses `ValidateAllBundles()` with 34 evaluator functions against `FTopicBundle` data — it does not read Schema.md files.

| Schema | Purpose (lint only) |
|--------|---------------------|
| `Plan.Schema.md` | Plan .md section ordering |
| `Playbook.Schema.md` | Playbook .md section ordering |
| `Implementation.Schema.md` | Implementation .md section ordering |
| `*ChangeLog.Schema.md` (3) | ChangeLog .md sidecar structure |
| `*Verification.Schema.md` (3) | Verification .md sidecar structure |
| `Doc.Schema.md` | Base .md document structure |

## validation_checks

`uni-plan validate [--topic <T>] [--strict] [--human]` runs 34 evaluator functions against every `.Plan.json` bundle. Checks are split into three tiers:

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

### Structural warnings (Warning) — 5 checks

| Check ID | Scope |
|---|---|
| `phase_tracking` | phase has populated `done`/`remaining` |
| `testing_actor_coverage` | phase has human + ai records |
| `canonical_entity_ref` | `changelogs[*].affected` path format |
| `topic_phase_status_alignment` | topic status is consistent with phase statuses — topic=completed ⇔ every phase completed; topic=not_started ⇔ no phase started; topic=in_progress ⇒ ≥1 phase started (added v0.73.0) |
| `completed_phase_timestamp_required` | `completed` phase has both `started_at` and `completed_at`; `in_progress`/`blocked` phase has non-empty `started_at` (added v0.73.0) |

### Content-hygiene checks (ErrorMinor + Warning) — 14 checks

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
| `topic_fields_not_identical` | Warning | Two topic-level prose fields are byte-identical non-empty strings (≥20 chars) — topic-level parallel of `no_duplicate_phase_field`; catches migration-stamp artifacts that reuse one template across `summary`/`goals`/`non_goals`/etc. (added v0.73.0) |
| `no_degenerate_dependency_entry` | ErrorMinor | Dependency row has all three of `topic`, `path`, `note` empty, OR `bundle`/`phase` kind is missing its required `topic` key, OR `governance`/`external` kind is missing its required `path`. Flags rows that survived a mutation but carry no information. (added Warning in v0.73.0, promoted to ErrorMinor in v0.73.1) |

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

# Legacy V3 ↔ V4 parity (v0.75.0: stateless — discovers .md files at invoke time)
uni-plan legacy-gap  [--topic <T>] [--category <c>] [--human]    # per-phase parity report, 8 categories
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
uni-plan topic set --topic <T> [--status <s>] [--next-actions <text>] [--summary <t>] [--goals <t>] [--non-goals <t>] [--risks <t>] [--acceptance-criteria <t>] [--problem-statement <t>] [--validation-commands <t>] [--baseline-audit <t>] [--execution-strategy <t>] [--locked-decisions <t>] [--source-references <t>] [--dependency-clear] [--dependency-add '<kind>|<topic>|<phase>|<path>|<note>']
uni-plan phase set --topic <T> --phase <N> [--status <s>] [--done <t>] [--remaining <t>] [--blockers <t>] [--context <t>] [--scope <t>] [--output <t>] [--investigation <t>] [--code-entity-contract <t>] [--code-snippets <t>] [--best-practices <t>] [--multi-platforming <t>] [--readiness-gate <t>] [--handoff <t>] [--validation-commands <t>] [--dependency-clear] [--dependency-add '<kind>|<topic>|<phase>|<path>|<note>']
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
