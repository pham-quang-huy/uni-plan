# uni-plan

## hard_rule_cli_only

**`.Plan.json` files MUST be accessed through the `uni-plan` CLI — never `json.load` / raw JSON parsing.** The CLI is the authoritative interface to the V4 bundle schema; raw reads bypass the typed domain model, validation, and schema-evolution guarantees. If a needed query isn't expressible via existing CLI commands, that is a CLI gap — report it and stop, do not work around with raw file reads. The `uni-plan validate` `summary` section and `uni-plan manifest list` command (both added in v0.71.0) cover the aggregate-query cases that previously tempted circumvention.

## project_overview

uni-plan is a standalone C++17 CLI tool for plan governance — managing, validating, and monitoring `.Plan.json` topic bundles across repositories. Each topic is a single JSON file containing phases, changelogs, verifications, and plan metadata.

- **Language**: C++17
- **Build system**: CMake 3.20+ with Ninja generator
- **Watch mode**: FTXUI terminal UI (conditional via `UPLAN_WATCH` CMake option)
- **Root namespace**: `UniPlan`
- **Build output**: `Build/CMake/uni-plan` on macOS/Linux, `Build/CMakeWin/uni-plan.exe` on Windows
- **Runnable binary**: `~/bin/uni-plan` on macOS/Linux; `Build/CMakeWin/uni-plan.exe` on Windows

## quick_reference

| Item | Value |
|------|-------|
| Language | C++17 |
| Build | CMake + Ninja |
| Namespace | `UniPlan` |
| Version source | `Source/UniPlanCliConstants.h` → `kCliVersion` |
| Build output | `Build/CMake/uni-plan` (macOS/Linux), `Build/CMakeWin/uni-plan.exe` (Windows) |
| Runnable binary | `~/bin/uni-plan` (macOS/Linux), `Build/CMakeWin/uni-plan.exe` (Windows) |
| Watch mode | FTXUI (`UPLAN_WATCH=ON` in shared presets) |

## build_commands

```bash
# macOS/Linux full build + install
./build.sh

# macOS/Linux build + tests
./build.sh --tests

# macOS/Linux manual build through shared presets
cmake --preset dev
cmake --build Build/CMake --parallel

# Verify macOS/Linux install
uni-plan --version
```

```powershell
# Windows build
.\build.ps1

# Windows build + tests
.\build.ps1 -Tests

# Windows manual build through shared presets
cmake --preset dev-win
cmake --build Build\CMakeWin --parallel

# Verify Windows canonical build output
Build\CMakeWin\uni-plan.exe --version
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
```

**No `Co-Authored-By:` trailer** — overrides the Claude Code harness default per `~/.claude/rules/no-claude-coauthor-trailer.md`.

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
├── Test/                      # GoogleTest suite (495 macOS / 494 Windows as of v0.105.4)
├── ThirdParty/                # FTXUI (terminal UI library)
├── Build/                     # CMake output directory
├── .claude/                   # Claude Code system
│   ├── settings.json          # Hook configuration
│   ├── hooks/                 # 3 hook scripts
│   ├── rules/                 # 2 auto-loaded rule files
│   ├── skills/                # 12 skills (upl-* prefix)
│   └── agents/                # 1 agent (upl-agent-*)
├── README.md                  # Root agent-focused entry point
├── CLAUDE.md                  # This file — project manifest
├── AGENTS.md                  # Parallel manifest for non-Claude harnesses (parity with CLAUDE.md)
├── CODING.md                  # Code style and SOLID principles
├── NAMING.md                  # Naming conventions
├── .clang-format              # clang-format configuration
├── CMakePresets.json          # dev/dev-tests → Build/CMake; dev-win/dev-win-tests → Build/CMakeWin
├── CMakeLists.txt             # Build configuration
├── build.sh                   # macOS/Linux build + install script
├── build.ps1                  # Windows PowerShell build script
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

**Version source**: `Source/UniPlanCliConstants.h` → `kCliVersion`
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
   - Same per-phase `EPhaseGapCategory` output as before: `legacy_rich | legacy_rich_matched | legacy_thin | legacy_stub | legacy_absent | v4_only | hollow_both | drift`. Thresholds were recalibrated in **v0.83.0** against V4 schema semantics (the v0.80.0 values over-translated the V3 Playbook.md line convention into V4 chars without accounting for V3 `.md` format overhead). Shared constants: `kPhaseHollowChars = 3000` / `kPhaseRichMinChars = 10000` in `UniPlanTopicTypes.h`.
     - `legacy_rich`: legacy ≥150 LOC AND V4 design_chars < 3000
     - `legacy_rich_matched`: legacy ≥150 LOC AND V4 design_chars ≥ 10000
     - `legacy_thin`: legacy 50–149 LOC
     - `legacy_stub`: legacy <50 LOC
     - `legacy_absent`: no legacy playbook
     - `v4_only`: no legacy AND V4 ≥10000 chars AND ≥3 jobs
     - `hollow_both`: legacy <50 LOC AND completed phase with V4 <3000 chars
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

Siblings exist for every prose flag, including (but not limited to): `--summary-file`, `--goals-file`, `--non-goals-file`, `--problem-statement-file`, `--validation-commands-file`, `--baseline-audit-file`, `--execution-strategy-file`, `--locked-decisions-file`, `--source-references-file`, `--done-file`, `--remaining-file`, `--blockers-file`, `--context-file`, `--scope-file`, `--output-file`, `--investigation-file`, `--code-entity-contract-file`, `--code-snippets-file`, `--best-practices-file`, `--multi-platforming-file`, `--readiness-gate-file`, `--handoff-file`, `--exit-criteria-file`, `--evidence-file`, `--notes-file`, `--change-file`, `--affected-file`, `--check-file`, `--result-file`, `--detail-file`, `--step-file`, `--action-file`, `--expected-file`, `--description-file`, `--reason-file`, `--verification-file`, plus per-entry flags on the v0.89.0+ `risk` / `next-action` / `acceptance-criterion` groups (`--statement-file`, `--mitigation-file`, `--rationale-file`, `--owner-file`, `--measure-file`, `--id-file`), plus per-entry flags on the v0.98.0+ `priority-grouping` / `runbook` / `residual-risk` groups (`--rule-file`, `--name-file`, `--trigger-file`, `--area-file`, `--observation-file`, `--why-deferred-file`, `--target-phase-file`, `--recorded-date-file`, `--closure-sha-file`). `--risks-file` / `--next-actions-file` / `--acceptance-criteria-file` were removed in v0.89.0 along with their parent flags; use the dedicated groups (`uni-plan risk add|set|remove|list`, `uni-plan next-action add|set|remove|list`, `uni-plan acceptance-criterion add|set|remove|list`, `uni-plan priority-grouping add|set|remove|list`, `uni-plan runbook add|set|remove|list`, `uni-plan residual-risk add|set|remove|list`) instead.

Shared implementation:

- `TryReadFileToString` in `Source/UniPlanFileHelpers.h` — opens in binary mode, slurps byte-identically, no trimming, no line processing. Errors map to `UsageError` (exit 2) at parse time.
- `TryConsumeStringOrFileOption` in `Source/UniPlanOptionParsing.cpp` — the one call site used by every parser. Each parser dropped its duplicate `if (Token == "--X") { ... }` branches in favor of `if (TryConsumeStringOrFileOption(..., "--X", "--X-file", Options.mX)) continue;`. Keeps per-parser call sites symmetric and the shell-escape hazard cannot re-enter through ad-hoc branches.
- Help text: `PrintCommandUsage` appends a single shared `kFileFlagFooter` note to commands with prose flags (topic, phase, job, task, changelog, verification). lane / testing / manifest dispatch through per-subcommand handlers rather than `kCommandHelp`; their `-file` flags work but the discoverability note lives in these docs rather than in `--help` output.

Regression tests in `Test/UniPlanTestOptionParsing.cpp` round-trip shell-hostile content (``\`$PATH\` ... $(pwd) ... "quoted"``) through both `--investigation` and `--investigation-file` and assert byte-identical storage. The inline form preserves content only when shell escaping is done correctly by the caller; the file form preserves it unconditionally.

The inline `--<field> <text>` path is unchanged — short, shell-safe values still work via the original form. Both paths are interchangeable when the content is clean; the file form is mandatory only when the content could contain `$`, `` ` ``, `"`, `\`, or newlines.

### v0.89.0 behavior note — `phase cancel` semantic command + `EExecutionStatus::Canceled`

The phase-level execution-status enum gains a 5th value — `Canceled` — covering the "superseded / won't-execute" terminal state for phases that will not run (migration aliases from earlier phase numbering, scope moved to another phase, renumbered). Prior to v0.89.0, `EExecutionStatus` was intentionally closed at 4 values (`NotStarted`, `InProgress`, `Completed`, `Blocked`) and `canceled` was topic-level only. Real-world usage surfaced migration-alias phases that genuinely needed a terminal-but-not-completed state at the phase level — a state where the phase is "done" from the topic's perspective (no pending work, no drift warnings) but was never actually executed (`completed_at` stays empty). The `--status <s>` help text already advertised `canceled` for phases; that text turned out to be stale/wrong (`ExecutionStatusFromString` rejected `"canceled"`), and this release makes it honest.

Along with the enum change:

- **New semantic wrapper `phase cancel`** joins `phase start / complete / block / unblock` as a gated lifecycle transition. Required flag: `--reason <text>` (also accepts `--reason-file <path>`). Gates: the phase must not already be `completed` (completed work cannot be retroactively canceled via the semantic command — use raw `phase set --status canceled` if a historical correction is truly required, accepting the audit trail that implies) and must not already be `canceled` (idempotency-as-error).
- **Effects**: `mStatus` → `Canceled`. Reason text → `mBlockers` (reused as "why it's no longer active"). Auto-changelog records `"Phase N canceled: <reason>"`. Does NOT stamp `completed_at` because the phase never actually finished.
- **Raw `phase set --status canceled`** also works now that the enum accepts the value. The semantic wrapper is preferred because it enforces the gates and mandates a reason.
- **Downstream consumers** that exhaustively match on `EExecutionStatus` were updated: `UniPlanValidation.cpp` (`topic_phase_status_alignment`), `UniPlanCommandValidate.cpp` (status distribution — now emits a `canceled` count), `UniPlanWatchSnapshot.h/.cpp` (new `mPhaseCanceled` field). `phase_status_lane_alignment` and `phase drift` only fire for `NotStarted` / `Completed`, so canceled phases are automatically skipped — the right behavior. `file_manifest_required_for_code_phases` now skips canceled phases (they won't execute, so no manifest is ever coming) and `no_hollow_completed_phase` is unchanged (only fires on `Completed`).
- **Topic alignment**: `topic_phase_status_alignment` now considers a topic "terminal" when every phase is in `{Completed, Canceled}`, not just `Completed`. This matches the semantic expectation: a topic whose every phase is either shipped or superseded has no pending work. Canceled phases still count as "progressed past not_started" for the strict `topic=not_started` invariant.
- **Help-text cleanup**: the stale `dropped` token (never a valid `EExecutionStatus` or `ETopicStatus` value) was removed from every `--status` help blurb it appeared in across `phase set`, `phase list`, `topic list`, `topic set`, `job set`, `task set`, `lane set`. The help-surface now reflects the enum truth.

Use `phase cancel` when:

- A phase entry is a migration alias whose work shipped under a different phase number (renumbering, scope moved).
- A phase will definitively not execute and should be terminal-but-not-completed (e.g. a prerequisite was dropped, the scope was merged into another phase).

Do NOT use `phase cancel` as a shortcut for "I gave up on this phase" when the work was partially done and the progress has value — in that case, prefer `phase complete` (if the delivered scope is itself shippable) or leave the phase `blocked` with a reason pointing at the unblocker.

### v0.94.0 behavior note — `topic add` command + `topic_key_matches_filename` validator

Closes the "cannot create a bundle via CLI" gap. Before v0.94.0, `upl-plan-creation` instructed authors to hand-write the JSON file from a template, which directly contradicted the `hard_rule_cli_only` at the top of this file. The v0.93.0 "CRUD Symmetry" release added `add/remove` to every entity group except topic; this release closes that asymmetry.

- **New command `uni-plan topic add --topic <PascalCase> --title <text> [prose flags]`**. Instantiates `Docs/Plans/<TopicKey>.Plan.json` as an empty-phases shell through the typed serializer, auto-stamps one creation changelog (`target=topic`, `uni-plan-mutation-v1` envelope with new `kTargetTopic` token), and refuses collisions. Every metadata prose field has the v0.76.0 `--<field>-file` sibling (`--summary-file`, `--goals-file`, `--non-goals-file`, `--problem-statement-file`, `--baseline-audit-file`, `--execution-strategy-file`, `--locked-decisions-file`, `--source-references-file`).
- **Topic-key regex enforced at parse time**: `^[A-Z][A-Za-z0-9]*$` → `UsageError` (exit 2). `topic add` is the only command that *chooses* a new key, so it's the correct single enforcement point. Existing bundles keep whatever key they have.
- **Empty-phases shell by design**: the fresh bundle fails `uni-plan validate` with `phases_present` ErrorMajor until the first `phase add` seeds Phase 0 — that's the expected governance signal, not a bug. `topic add` does not auto-seed because CRUD symmetry (`job add` doesn't seed a task, `lane add` doesn't seed a job); each creator adds exactly one entity.
- **Exit codes**: `0` bundle created; `1` runtime error (collision — bundle already discoverable under repo root, or directory-write failure); `2` UsageError (bad key regex, missing `--topic`/`--title`).
- **Mutation target `kTargetTopic = "topic"`**: new constant alongside `kTargetPlan`. Mutation consumers parsing `target` as a structural path should treat `"topic"` as an opaque bundle-creation token, distinct from `"plan"` (plan-level field mutation) or `"phases[N]"` (indexed structural path).
- **New `topic_key_matches_filename` validator (ErrorMinor)**: defense-in-depth for legacy / hand-edited bundles where `"topic": "Bar"` drifted into `Foo.Plan.json`. Parse-time regex prevents new drift; the evaluator catches inherited drift. Structural tier count 15 → 16; total evaluators 34 → 35.
- **Skill rewrite**: `.claude/skills/upl-plan-creation/SKILL.md` Step 2 now calls `uni-plan topic add` instead of prescribing a hand-written JSON template.

Use `uni-plan topic add` when:

- Creating a new plan topic from scratch (replaces every prior skill that asked you to hand-write `Docs/Plans/<Key>.Plan.json`).
- Migrating a topic from a different governance system into a V4 bundle — create the shell first, then layer phases via `phase add`, then layer design material via `phase set`.

The hand-written-JSON path is no longer supported. Any agent flow that still reads a skill telling it to `json.load` or author `.Plan.json` directly is referencing stale documentation — update the skill.

### v0.95.0 behavior note — stable `index` field on list/query output (index-drift fix)

Closes a silent correctness bug where `uni-plan changelog` / `uni-plan verification` sorted or filtered their output before rendering while `changelog set --index N` / `changelog remove --index N` (and the verification analogues) addressed the raw storage index. An agent reading the query and citing `--index N` targeted the wrong row whenever storage order ≠ sort order — which is common, since auto-changelog appends accumulate in insertion order but the query renders `(phase ASC, date DESC)`. Same class-of-bug in the typed-array list commands (`risk list --severity high`, `next-action list --status pending`, `acceptance-criterion list --status met`) where the filter dropped rows and the surviving values lost their pre-filter position.

- **New `index` field** on every emitted entry in:
  - `uni-plan changelog` JSON (`"index": N`) and `--human` output (new `Idx` column).
  - `uni-plan verification` JSON + `--human` (same).
  - `uni-plan risk list` / `uni-plan next-action list` / `uni-plan acceptance-criterion list` JSON — a new `"index"` leads each entry emitted by the shared `EmitRisksJson` / `EmitNextActionsJson` / `EmitAcceptanceCriteriaJson` helpers.
- **Index is the raw storage position** in the bundle's underlying `mChangeLogs` / `mVerifications` / `mRisks` / `mNextActions` / `mAcceptanceCriteria` vector — captured *before* any sort or filter. It is the stable target for `<entity> set --index N` / `<entity> remove --index N` regardless of render order.
- **Emit helpers grew an optional `InOriginalIndices` parameter**. When nullptr (typical for `topic get`, which passes the full unfiltered vector), the loop counter is the storage index. When non-null, the caller (a filtered list runner) remaps per-row to the pre-filter storage position. Default parameter preserves backward compatibility for every existing caller.
- **New constant `kTargetTopic`** from v0.94.0 is unaffected; this release adds no new mutation target tokens.
- **Unchanged**: `job list`, `task list`, `lane list`, `testing list`, `manifest list` — these already emitted `phase_index` / `job_index` / `lane_index` / `testing_index` / `manifest_index`. The sweep verified them clean.
- **kCliVersion bump**: 0.94.0 → 0.95.0. MINOR per SemVer discipline — a new output field is a backward-compatible addition (JSON consumers that don't know about `index` still parse cleanly; consumers that care gain a stable mutation target).
- **Help-text update**: `uni-plan changelog --help`, `uni-plan verification --help`, and the `set` subcommand help for each now document the index contract explicitly ("render order ≠ storage order; cite the emitted `index` field, not the row number").

Regression guards in [Test/UniPlanTestQuery.cpp](Test/UniPlanTestQuery.cpp) cover: the emitted `index` in a sorted changelog round-trips through `changelog set --index N` back to the same row; verification filtered by phase still emits pre-filter indices; risk / next-action / acceptance-criterion list commands with a status/severity filter expose the pre-filter storage position. Suite now 348 → 354 passing (+6 guards).

Historical note: the bug was reported by an AI agent running a repair loop that concluded it was unsafe to keep using the CLI for changelog set/remove and switched to manual bundle edits. Agent was correct; this release closes the gap so the CLI is safe again.

### v0.97.0 behavior note — no-truncation contract on every query surface

Every query surface — JSON and `--human` — now emits byte-identical stored prose. Nothing is clipped, no `...` ellipsis is appended by the renderer. Closes a silent content-loss bug where `topic get`, `phase get --phases`, `phase get --brief`, and every `--human` table previously ran prose through `substr(0, N) + "..."` before emitting, so agents and humans alike received a corrupted tail they had no way to reconstruct.

- **20 truncation sites removed** across 6 source files. JSON paths affected (agent-visible content loss): `ResolvePhaseLabel` in changelog `phase_label`, `topic get` phase_summary scope, `phase get --phases` batch scope, `phase get --brief` done + job_summary scope, `no_duplicate_changelog` / `no_duplicate_phase_field` / `ScanContentPatterns` issue detail previews. `--human` paths affected (operator-visible content loss): changelog (Change, PhaseDisplay, Affected), verification (Check, Detail), timeline (Text), blockers (notes), topic get phases table, phase list scope, validate (Detail, Path), manifest list Description. Watch TUI panel title also emits verbatim; FTXUI's frame layout handles any terminal-width overflow.
- **`TruncateForDisplay` helper removed** from `Source/UniPlanOutputHelpers.h`. The no-truncation contract is the default, not an opt-in.
- **HumanTable auto-sizes columns** to the widest cell. Long prose renders full-width; terminal wrap handles overflow naturally (same as every other long-output Unix tool). If a caller needs a fixed-width preview, they trim on their end.
- **`--brief` semantics**: "compact view" now means **fewer fields**, never **clipped fields**. Every field `--brief` emits carries its full stored content; the compactness comes from omitting design material, jobs/lanes detail, file_manifest, testing, etc. Callers concerned with token budget can combine `--brief` with explicit field filtering (or post-process themselves).
- **Validator `issues[].detail`** now embeds the full match context. `no_duplicate_changelog`/`no_duplicate_phase_field`/`ScanContentPatterns` previously wrote `'...preview...'` capped at 60 chars; a caller reading a duplicate-detection issue couldn't recover which exact duplicate pair fired. Now the full text is there.
- **kCliVersion bump**: 0.96.0 → 0.97.0. MINOR per SemVer discipline — the output-contract change is strictly additive (callers that previously saw a trimmed string now see the full string; the suffix `...` is no longer synthetic and any `...` present in output comes from the underlying bundle content). Consumers that were relying on the clip to limit their own output size must add their own trim.
- **Guard tests** (Test/UniPlanTestQuery.cpp) seed scope / done / change / detail with a 2000-byte payload + unique sentinel, then assert byte-identical survival through: `topic get` JSON phase_summary, `phase get --phases` batch JSON, `phase get --brief` JSON done, changelog JSON change + phase_label, verification JSON check + detail, validate JSON no_duplicate_changelog issue detail, `topic get --human`, `changelog --human`. Suite 373 → 381 passing (+8 guards).

Historical note: reported as "many get commands return trimmed string (with ...), so the content is lost and human and AI Agents can't get real content". The report was correct. Pre-v0.97.0 the CLI silently broke the `hard_rule_cli_only` contract at the top of this file — agents asking for `.Plan.json` content through the CLI couldn't recover what they asked for past the per-command truncation threshold.

### v0.99.0 behavior note — guarded bundle write flow (concurrent-mutation race fix)

Every mutation write now routes through `GuardedWriteBundle` (new [Source/UniPlanBundleWriteGuard.h](Source/UniPlanBundleWriteGuard.h)), which: acquires an exclusive advisory lock on the target bundle, verifies the file has not changed since this process read it, serializes to a sibling `<bundle>.tmp.<pid>.<tid>`, fsyncs + closes + `std::filesystem::rename(tmp, final)`. Closes a concurrent-mutation race where two `uni-plan` instances operating on the same bundle could clobber each other's changes between read and write (the previous code did a direct truncate-write with no lock, no stale-check, no atomic rename).

- **Lock primitive**: `flock(LOCK_EX)` on POSIX, `LockFileEx(LOCKFILE_EXCLUSIVE_LOCK)` on Windows. Kernel-owned advisory locks — release automatically on process death, no stale-lock sidecar/cleanup dance. 5 s acquisition timeout as defense-in-depth.
- **Stale-check**: every `TryLoadBundleByTopic` and `LoadAllBundles` now stamps `OutBundle.mReadSession` with `(file_size, mtime_nanos, FNV-1a-64 of file bytes)`. On write-back, the guard re-stats and re-hashes under the lock; any mismatch fails exit 1 with `"bundle changed during mutation; re-read and retry"`. The `(size, mtime)` tuple is the fast path; the FNV-1a-64 hash closes HFS+'s 1-second mtime ambiguity window.
- **Atomic rename**: write to sibling tmp in the same directory, `fsync` (or `FlushFileBuffers` on Windows), release lock handle, then `fs::rename` into place. POSIX rename is atomic same-filesystem; Windows `MOVEFILE_REPLACE_EXISTING` semantics via `fs::rename`. Cross-filesystem rename (EXDEV) surfaces as an error directing operators not to symlink `Docs/Plans` across filesystems — no non-atomic copy-fallback.
- **No bypass flag**: conflict is hard-stop exit 1. There is no `--force`. The callers' recovery protocol is re-read and retry.
- **Raw `TryWriteTopicBundle` is unchanged** — it remains the serialize-and-write primitive used by test fixtures and by the fresh-in-memory path inside the guard. The guard wraps it.
- **`FTopicBundle.mReadSession`**: new runtime-only field alongside `mBundlePath`, not serialized. `mbValid=false` for bundles built in memory (topic add, tests, migration rewrite that already holds the only reader of record) — those skip the stale-check and rely on lock-only protection. All 40+ test call sites using `TryWriteTopicBundle` or building bundles in memory continue to work unchanged.
- **FNV-1a helpers promoted** to new [Source/UniPlanHashHelpers.h](Source/UniPlanHashHelpers.h) as inline functions, so `UniPlanCache.cpp` and `UniPlanBundleWriteGuard.cpp` share one implementation (no algorithm duplication). The historic UniPlanCache seed value (`1469598103934665603ull`, two digits shorter than the canonical offset basis) is preserved as-is — changing it would invalidate in-flight cache entries; any correction must update both consumers together.
- **Fault injection**: setting the `UPLAN_FAULT_PRE_RENAME` environment variable causes the guard to abort after writing the tmp file but before renaming, returning the same exit 1 error contract. Test-only; zero-cost one `getenv` call on the normal path. No `#ifdef` scaffolding in the production binary.
- **kCliVersion bump**: 0.98.0 → 0.99.0. MINOR per SemVer discipline — the new conflict exit path is an observable behavior change for callers that previously assumed the last write always wins.
- **Tests**: new [Test/UniPlanTestBundleWriteGuard.cpp](Test/UniPlanTestBundleWriteGuard.cpp) covers seven invariants — rename atomicity (no tmp detritus), stale-check conflict detection on out-of-band peer write, concurrent `std::thread` racers (one winner one loser), concurrent `fork()`-ed processes (POSIX only), pre-rename fault leaves original bit-identical, external `FBundleFileLock` holder blocks a peer until released, raw-primitive / `mbValid=false` skip path for fresh-in-memory bundles.

Historical context: reported as an open program of work on bundle concurrency safety. Prior to 0.99.0 there was no mention of locks / races / atomic writes anywhere in the codebase — `TryWriteTopicBundle` did `ios::out | ios::trunc`, and `WriteBundleBack` called it directly. Two parallel `uni-plan phase set` invocations on the same topic could silently lose one side's mutation.

### v0.100.0 behavior note — JSON-file setters for typed-array inputs

Three new flags close the shell-hostility gap in the last pipe-delimited input grammars. The `--validation-commands` / `--validation-add` / `--dependency-add` pipe grammars remain supported as legacy authoring input, but the JSON-file siblings are now the documented preferred form for any content that might be shell-hostile (contain literal `|`, `$`, backticks, double quotes, or newlines). JSON in a file is read as raw bytes by the CLI — bash never touches the content.

- **`--validation-commands-json-file <path>`** (on `topic set` and `phase set`) — REPLACE semantics: clears existing `validation_commands` and fills from the JSON array. Parallels `--validation-commands`. File shape: `[{"platform": "any|macos|windows|linux", "command": "<required>", "description": "<optional>"}, ...]`.
- **`--validation-add-json-file <path>`** (on `topic set` and `phase set`) — APPEND semantics: no clear, just adds each entry. Parallels `--validation-add`. Same JSON shape.
- **`--dependency-add-json-file <path>`** (on `topic set` and `phase set`) — APPEND semantics. Parallels `--dependency-add`. File shape: `[{"kind": "bundle|phase|governance|external", "topic": "<required for bundle/phase>", "phase": <int, optional>, "path": "<required for governance/external>", "note": "<optional>"}, ...]`.

Parse errors (malformed JSON, missing required fields, wrong top-level type, invalid enum values) surface as `UsageError` (exit 2) at parse time with `<flag> '<path>' [<index>].<field>: <reason>` diagnostics.

Why this matters: the pipe grammars `<platform>|<command>|<description>` and `<kind>|<topic>|<phase>|<path>|<note>` cannot carry a literal `|` without silent mangling. Commands containing bash pipelines (`grep foo bar.log | wc -l`), URLs with query strings (`https://example.com/?a=1|2`), and free-form notes were all hazardous via the pipe path. JSON is inert to these metachars.

Semantics are otherwise identical to the pipe counterparts — `mbValidationClear` is set by `--validation-commands-json-file` (REPLACE) and NOT set by `--validation-add-json-file` (APPEND), and dependency kind/topic/path validation matches `--dependency-add` exactly (kind=bundle|phase requires `topic`; kind=governance|external requires `path`).

**Tests** (`Test/UniPlanTestOptionParsing.cpp`): 9 new guards covering pipe-in-command round-trip, shell-hostile content (backticks/$/$(..)/quotes) round-trip, REPLACE vs APPEND semantics, every dependency kind including notes with literal `|`, and every malformed-input path (JSON parse error, empty command, missing required topic, top-level-not-array).

**kCliVersion bump**: 0.99.1 → 0.100.0. MINOR per pre-1.0 SemVer — new flags added to the command surface. Zero behavior change for callers using only the legacy pipe forms.

**Note on version numbering**: uni-plan goes 0.99.x → 0.100.0, NOT 1.0.0. v1.0 is reserved for an explicit surface lock and is not ratified yet.

### v0.101.0 behavior note — Phase-complete execution-descendant gate + `lane complete` semantic command + `completed_jobs_have_completed_tasks` validator

Promotes the post-hoc `phase_status_lane_alignment` Warning to parse-time refusals at the mutation surface, closes the jobs-vs-tasks alignment gap, and adds a matching lane-level semantic command so the authoring flow has a gated path at every level.

**1. Phase-complete descendant gate**:
- `uni-plan phase complete` now refuses when any lane, job, or task in the target phase is not terminal. Terminal = `Completed` or `Canceled`. NotStarted / InProgress / Blocked all fail.
- Error message enumerates every incomplete descendant by index with its current status, and points the operator at `job set --status`, `task set --status`, or `lane set --status` for remediation.
- The gate sits at `Source/UniPlanCommandLifecycle.cpp` inside `RunPhaseCompleteCommand`, after the v0.88.0 file-manifest gate and before any state mutation — so a refusal is atomic (no partial writes, phase stays in_progress).

**2. `uni-plan lane complete` semantic command** (new):
- Flags: `--topic <T> --phase <N> --lane <L>`. Symmetric with `phase complete / block / unblock / cancel`.
- Gate: every job on the lane (identified by `job.mLane == InLaneIndex`) must be terminal. Same error taxonomy as the phase-complete gate.
- Effects: flips lane status to `Completed`, appends an auto-changelog entry, emits standard `uni-plan-mutation-v1` JSON.
- Raw `lane set --status completed` remains available for manual repair but bypasses this gate — prefer the semantic command for authoring discipline. The skill guidance and docs should recommend `lane complete` as the default path.

**3. `completed_jobs_have_completed_tasks` validator** (new, ErrorMinor):
- Fires when a job has `status=completed` but carries at least one task that is not terminal (not Completed and not Canceled).
- Detail message: "N of M task(s) not terminal but job status is completed".
- Path format: `phases[P].jobs[J]`.
- Closes the jobs→tasks gap that the v0.84.0 `phase_status_lane_alignment` Warning covers at phases→lanes but never reached into task granularity.

**Exit codes**: phase-complete / lane-complete gate failures → exit 1. `ErrorMinor` from the new validator → `valid=false` under `--strict`; a plain `validate` run still reports the issue but `valid=false` is only driven by ErrorMajor.

**kCliVersion bump**: 0.100.0 → 0.101.0. MINOR per pre-1.0 SemVer — adds a new command (`lane complete`), a new validator ID, and a new refusal path on `phase complete`. Consumers relying on the old "phase complete accepts incomplete descendants" behavior must either complete or cancel those descendants first, or use raw `phase set --status completed` (which skips this gate, carrying the audit trail implied by the raw setter).

**Tests** (`Test/UniPlanTestSemantic.cpp` + `Test/UniPlanTestValidationContent.cpp`): 5 new guards covering phase-complete refusal on incomplete lane, phase-complete refusal on incomplete task, phase-complete allows canceled descendants, lane-complete happy path, lane-complete refuses incomplete job, validator fires for completed job with in-progress task, validator clean when every task is Completed or Canceled.

### v0.102.0 behavior note — `phase sync-execution` reconciliation command

Addresses the post-v0.101.0 ergonomic: after a batch of leaf-level `task set --status completed` / `task set --status canceled` updates, an agent used to have to step `job set --status completed` on every job, `lane complete` on every lane, then `phase complete`. `phase sync-execution` propagates terminal status upward in one call — child → parent only, non-destructive, idempotent.

- **New command**: `uni-plan phase sync-execution --topic <T> --phase <N> [--dry-run]`. Output schema `uni-plan-sync-execution-v1`.
- **Strict child → parent only**. Never flips a child from a parent's status. Never downgrades a parent already in a terminal state (`Completed` or `Canceled`). **Never touches phase status** — that remains `phase complete` / `phase cancel` territory with their own gates.
- **Rollup rules** (symmetric for jobs ← tasks and lanes ← jobs-on-that-lane):
  - Zero children → skip (nothing to roll up).
  - Parent already terminal → skip.
  - Every child terminal AND ≥1 Completed → parent → `Completed`.
  - Every child terminal AND every child Canceled → parent → `Canceled`.
  - Any child not terminal → skip (the parent is genuinely in-flight).
- **Two-pass order**: jobs first (from tasks), then lanes (from the newly-updated job state). So a phase whose every task is complete propagates all the way up to the lane in one invocation.
- **`--dry-run`** emits the same JSON envelope with `dry_run: true` and `changes: [...]` populated, but does not touch disk. Useful for previewing before commit.
- **Idempotent**: re-running after a successful pass emits `jobs_flipped: 0, lanes_flipped: 0, changes: []`. The second invocation sees every parent already terminal and skips cleanly.
- **Auto-changelog**: one entry per flipped entity (`"lanes[1] synced in_progress → completed (all jobs terminal; ≥1 completed)"`). Routes through `GuardedWriteBundle` so the v0.99.x lock + atomic-rename + stale-check guarantees apply.
- **Not a data-repair command**: if a parent is `Completed` but a child is `InProgress` (inconsistent state), `sync-execution` leaves the parent alone — the `completed_jobs_have_completed_tasks` validator (v0.101.0) surfaces that inconsistency separately. Downgrading a parent in response to a dirty child is a different decision that belongs to an explicit `job set --status` / `lane set --status` invocation with audit-trail awareness.

**kCliVersion bump**: 0.101.0 → 0.102.0. MINOR per pre-1.0 SemVer — adds a new subcommand and a new output schema. No behavior change for callers not using it.

**Tests** (`Test/UniPlanTestSemantic.cpp`): 7 new guards covering happy path (task → job → lane chain), `--dry-run` leaves on-disk bytes byte-identical, mixed-statuses skip the job, all-canceled tasks roll up as Canceled, mixed Completed + Canceled tasks roll up as Completed (real work happened), already-terminal parent is skipped (non-destructive), idempotency on re-run.

### v0.102.1 behavior note — End-to-end regression fixtures + dry-run pass-2 fix

PATCH. Two changes — one bugfix, one test-coverage addition. Both discovered while adding the regression fixtures.

1. **Bug fix: `phase sync-execution --dry-run` now previews the FULL cascade.** Previously dry-run's pass-1 (job rollup) skipped the in-memory mutation of `job.mStatus`, so pass-2 (lane rollup) saw the *original* in-flight job state and reported `lanes_flipped: 0` even when the non-dry-run would have rolled them up. The fix makes the in-memory mutation unconditional — only the disk write (`WriteBundleBack`) remains gated by `--dry-run`. Dry-run now emits the complete set of flips both passes would apply. The real (non-dry-run) behavior is unchanged. Fixes `Source/UniPlanCommandLifecycle.cpp` `RunPhaseSyncExecutionCommand`.

2. **Test-coverage addition: `Test/UniPlanTestEndToEnd.cpp`** — 3 new regression fixtures covering the full CLI command chain from `topic add` through `phase complete`. Complements the per-command unit tests in `UniPlanTestMutation` / `UniPlanTestSemantic` / `UniPlanTestEntity` / `UniPlanTestValidationContent` / `UniPlanTestBundleWriteGuard` by proving the whole chain composes correctly across v0.94.0 – v0.102.0 surfaces:
   - `EndToEndDocOnlyPhaseLifecycle` — doc-only phase: topic add → phase add → design → `--no-file-manifest=true` → lane/job/task add → testing (human + ai) → start → bulk `task set --status completed` → `sync-execution --dry-run` (verify jobs_flipped + lanes_flipped both reported, bytes unchanged) → `sync-execution` → `phase complete` → validate corpus clean.
   - `EndToEndCodeBearingPhaseLifecycle` — code-bearing phase: same chain but with `code_entity_contract` + `manifest add` path, exercising the v0.88.0 file-manifest gate end-to-end.
   - `EndToEndShellHostileValidationCommandViaJsonFile` — carries a literal `grep foo build.log | wc -l` pipe-containing command through `--validation-commands-json-file`, runs the whole lifecycle, asserts the `|` character survives byte-identically from file-read through lock+stale-check writes through final `ReloadBundle`.

**kCliVersion bump**: 0.102.0 → 0.102.1. PATCH per pre-1.0 SemVer — dry-run now reports a previously-hidden set of flips (strictly additive; no caller is worse off), plus test-only additions. No command-surface change.

Suite size after: 440 passing (was 437).

### v0.103.0 behavior note — runtime phase metrics

Adds a read-only audit surface for plan detail and intensity without changing the `.Plan.json` schema.

- **New command**: `uni-plan phase metric --topic <T> [--phase <N>|--phases <csv>] [--status <filter>] [--human]`.
- **Output schema**: `uni-plan-phase-metric-v1`. This is a command-output contract only, not a persisted bundle schema.
- **Runtime-only metrics**: `design_chars`, `solid_words`, `recursive_words`, `field_coverage_percent`, `work_items`, `testing_records`, `file_manifest_entries`, and `evidence_items`.
- **Watch mode**: PHASE DETAIL now has a `d` toggle for a metrics view. Every metric cell renders as a gauge bar using the same visual language as the existing Design column.
- **No migration**: existing plans are unchanged. The command computes from existing phase, child, changelog, and verification data at query time.

**kCliVersion bump**: 0.102.1 → 0.103.0. MINOR per pre-1.0 SemVer — adds a new subcommand, output schema, and watch UI surface.

### v0.104.0 behavior note — incremental watch snapshots

`uni-plan watch` now uses an in-memory snapshot cache for long-running watch
sessions. It fingerprints `.Plan.json` bundles separately from lint-relevant
`.md` files, reloads only changed bundles, reuses unchanged phase summaries and
runtime metrics, and reruns markdown lint only when markdown files change.

The cache is process-local and is not the persisted `uni-plan cache` store.
It does not add `.Plan.json` fields, does not change command JSON schemas, and
does not persist runtime metrics. The watch footer now exposes operation-count
telemetry (`reload/reuse`, metric recompute count, validation/lint run vs
reuse) so performance can be audited without wall-clock-only guesses.

**kCliVersion bump**: 0.103.0 → 0.104.0. MINOR per pre-1.0 SemVer — changes
watch runtime behavior and visible watch status text.

### v0.104.1 behavior note — adaptive watch metric gauges

The PHASE DETAIL metrics view now separates audit status from comparison
intensity. Gauge color still uses the fixed hollow/rich thresholds, but gauge
fill switches to an adaptive per-plan scale when every phase in that metric
column is already rich. Dense plans therefore keep visible row-to-row
differences instead of rendering every already-rich metric as a full bar. The
metrics toggle keeps the `Scope` column visible so phase intent remains in
view after the `Evidence` gauge column. `Fields` reaches full only at 100%
coverage, and `Work` uses 40 items as the rich threshold so common 20-item
phases render as mid-range rather than saturated. `Tests` uses 8 records as the
rich threshold so three test records no longer saturate the column.

**kCliVersion bump**: 0.104.0 → 0.104.1. PATCH per pre-1.0 SemVer — fixes
watch metric display semantics only; no command/schema/persisted-state change.
`phase metric` threshold values are recalibrated under the existing output
schema.

### v0.105.0 behavior note — `--ack-only` compact mutation response

Closes the largest observed token-waste surface on AI-agent dense-plan
authoring sessions. Every mutation command that emits the generic
`uni-plan-mutation-v1` envelope (`topic set` / `phase set` / `lane add` /
`job add` / `task add` / `changelog add` / `verification add` /
`manifest add` / `risk add` / `next-action add` /
`acceptance-criterion add` / `priority-grouping add` / `runbook add` /
`residual-risk add` and every `*set` / `*remove` sibling plus the
lifecycle transitions in `UniPlanCommandLifecycle.cpp`: `phase start`,
`phase complete`, `phase block`, `phase unblock`, `phase cancel`,
`lane complete`, `topic start`, `topic complete`, `topic block`) now
accepts an opt-in `--ack-only` flag.

**Not affected**: `phase sync-execution` emits its own
`uni-plan-sync-execution-v1` schema (per-entity `changes[]` + summary
counts), not the generic mutation envelope. Its response is already
compact by schema design; `--ack-only` is silently ignored on that
command. Query commands (`validate`, `topic get`, `phase get`, `phase
metric`, `phase readiness`, etc.) also ignore the flag — their own
schemas stay unchanged.

Under `--ack-only`:

- The response schema is [`kMutationAckSchema`](Source/UniPlanCliConstants.h) =
  `"uni-plan-mutation-ack-v1"` (new constant), not the default
  `"uni-plan-mutation-v1"`.
- The `"changes":[{"field":"…","old":"…","new":"…"}]` array is replaced
  with a flat `"changed_fields":["…","…"]` list of field names. No
  prior/new value echo.
- Every other envelope field (`ok`, `topic`, `target`, `auto_changelog`)
  is unchanged.

Without `--ack-only` the response shape is byte-identical to v0.104.1.
The on-disk bundle, the auto-changelog stamp, the `GuardedWriteBundle`
v0.99.0+ lock-and-stale-check flow, the exit codes, and the validator
surface are all **independent of the flag**. The flag only controls the
stdout response envelope.

**Implementation shape**:

- [`BaseOptions`](Source/UniPlanOptionTypes.h) gains `bool mbAckOnly =
  false`. Every Options struct inherits through `BaseOptions`, so the
  flag is parsed once in `ConsumeCommonOptions`
  ([Source/UniPlanOptionParsing.cpp](Source/UniPlanOptionParsing.cpp))
  and threaded through to every handler.
- [`EmitMutationJson`](Source/UniPlanCommandMutationCommon.cpp) grows a
  5th `bool InAckOnly` parameter (header default `false` for migration
  safety; every one of the 20 existing call sites in
  `UniPlanCommandMutation.cpp` and `UniPlanCommandLifecycle.cpp`
  explicitly passes `Options.mbAckOnly`).
- Every mutation command's `--help` gains a `--ack-only` line under
  `Common options` via
  [`PrintSubcommandBlock`](Source/UniPlanCommandHelp.cpp) — gated by the
  `uni-plan-mutation-` schema prefix so query-only commands do not
  advertise the flag.

**Tests** (`Test/UniPlanTestMutation.cpp`): 4 new fixtures covering
`AckOnlyEmitsChangedFieldsNotOldNew`,
`DefaultEnvelopeUnchangedWithoutAckOnly`,
`AckOnlyDoesNotChangeDiskBytes`
(the load-bearing byte-identical-bundle invariant), and
`AckOnlyAutoChangelogStillStamped`. Suite size 469 → 473 passing.

**kCliVersion**: lands at `0.105.0` at the phases[3] close-out of the
[CliAgentErgonomics](Docs/Plans/CliAgentErgonomics.Plan.json) topic
alongside the other v0.105.0 additions (`--all-phases` batch sugar,
`task set --description` with gate, `--<field>-append-file`). Until
that final phase completes, kCliVersion stays at `0.104.1`; the
`--ack-only` surface is observable in the shipped binary after
phases[0] builds green.

### v0.105.0 behavior note — `--all-phases` batch sweep on query commands

Closes the per-phase fork cost when an AI agent wants readiness /
metric / get across every phase of a topic. Prior to v0.105.0,
`uni-plan phase readiness` was single-phase only and callers had to
loop:

```bash
for N in 0 1 2 3 4 5 6 7 8 9 10; do
  uni-plan phase readiness --topic MegaScan --phase $N
done
```

Each iteration forked a process (~30-50ms cold), totaling hundreds of
ms per sweep. `phase get` and `phase metric` already accepted
`--phases 0,1,...,N-1` but that required the caller to count phases
first.

**New surface**:

- `uni-plan phase readiness --topic <T> --all-phases` — sweeps every
  phase and emits a wrapped envelope with schema
  [`kPhaseReadinessBatchSchema`](Source/UniPlanCliConstants.h) =
  `"uni-plan-phase-readiness-batch-v1"`. Each element of `phases[]`
  carries the same payload shape as the single-phase v1 emission, so
  consumers that parsed v1 can parse each batch element unchanged.
- `uni-plan phase get --topic <T> --all-phases` — sugar for the
  existing v0.84.0 `--phases 0,1,...,N-1` path. Response schema is
  the existing `uni-plan-phase-get-v2` batch envelope.
- `uni-plan phase metric --topic <T> --all-phases` — explicit form of
  the existing "no index selector = every phase" default. Mutual
  exclusion with `--phase` / `--phases` is now enforced at parse time.

**Mutual exclusion** (all three commands, parse-time
[`UsageError`](Source/UniPlanOptionTypes.h) exit 2):

- `--phase <N>` + `--phases <csv>` — mutually exclusive (pre-existing).
- `--phase <N>` + `--all-phases` — mutually exclusive (new).
- `--phases <csv>` + `--all-phases` — mutually exclusive (new).

**Handler implementation**: `RunPhaseReadinessCommand`
([Source/UniPlanCommandSemanticQuery.cpp](Source/UniPlanCommandSemanticQuery.cpp))
extracts per-phase payload rendering into lambdas
(`EvaluateGates`, `EmitPerPhaseJsonBody`, `EmitPerPhaseHuman`) so the
batch path and the single-phase path share identical content. `phase
get` and `phase metric` detect `mbAllPhases` on their options struct
and expand to `mPhaseIndices` (populated with `0..N-1`) after the
bundle loads, reusing the existing batch emitters. `phase wave-status`
shares the `FPhaseQueryOptions` parser but explicitly rejects
`--all-phases` at handler time — wave tables are per-phase by design.

**Empty-phase topics**: `--all-phases` on a freshly-created topic with
zero phases emits an empty `phases[]` array and exits 0. Callers that
want to treat empty as error must check the array length themselves.

**Tests** (`Test/UniPlanTestQuery.cpp`): 6 new fixtures covering the
batch schema, full-index coverage, parse-time mutual exclusion on
`phase readiness` and `phase get`, the wave-status rejection, and the
`phase get --all-phases` → v2 batch envelope equivalence. Suite size
473 → 479 passing.

**kCliVersion**: still `0.104.1` until phases[3] closes the topic.
The `--all-phases` surface is observable in the shipped binary after
phases[1] builds green.

### v0.106.0 behavior note — query schemas and topic lookup

Read-only semantic queries no longer reuse the mutation envelope.
`topic status`, `phase next`, single-phase `phase readiness`, and
`phase wave-status` emit dedicated query schemas with `generated_utc`
and normalized `repo_root` fields. Typed-array list commands now honor
an explicit `--repo-root` in their JSON envelope even when the caller's
working directory is different. Single-topic lookup first checks the
canonical `Docs/Plans/<Topic>.Plan.json` path, then falls back to the
recursive scan for project-scoped bundles.

### v0.105.0 behavior note — `task set --description` with safety gate

Closes a documented CLI asymmetry from
`square-bot/.claude/rules/square-bot-cli-gap-discipline.md`: `task add`
accepted `--description` but `task set` did not, forcing authors who
wanted to correct a typo in a task description to `task remove` +
`task add` and destroy the task's audit history (`evidence`, `notes`,
status transitions in the changelog).

**New surface**:

- `uni-plan task set --description <text>` and the paired
  `--description-file <path>` (v0.76.0 shell-safety helper).
- Safety gate: a description change is allowed freely when the task
  is in status `not_started`. On any other status the mutation is
  refused (parse-handler
  [`UsageError`](Source/UniPlanOptionTypes.h) exit 2) unless the
  caller supplies BOTH `--force` and a non-empty `--reason <text>`.
  An all-whitespace reason is equivalent to no reason.
- The forced path embeds the before/after description text and the
  trimmed reason in the auto-changelog entry's `change` field, so
  the reason for the overwrite is captured in the audit trail.

**Implementation**:

- [`FTaskSetOptions`](Source/UniPlanOptionTypes.h) gains
  `mDescription`, `mbDescriptionSet`, `mbForce`, and `mReason`.
  `mbDescriptionSet` distinguishes the "caller passed the flag" state
  from the "caller omitted it" state, because `""` is a valid
  description value (a legitimate clear-the-description request).
- [`ParseTaskSetOptions`](Source/UniPlanOptionParsing.cpp) consumes
  `--description` / `--description-file` (via the shared
  `TryConsumeStringOrFileOption` helper), `--force`, and `--reason`
  / `--reason-file`.
- [`RunTaskSetCommand`](Source/UniPlanCommandMutation.cpp) runs the
  gate before the auto-changelog stamp, and — on the forced path —
  extends the `AppendAutoChangelog` change text with
  `description forced-update: "<old>" -> "<new>" (reason: <trimmed>)`.
- `task set --help` grows four new lines under `Options` documenting
  `--description` / `--description-file` / `--force` / `--reason`
  and the gate semantic.

**Tests** (`Test/UniPlanTestMutation.cpp`): 5 new fixtures —
`TaskSetDescriptionAllowedOnNotStartedTask`,
`TaskSetDescriptionRefusedOnInProgressWithoutForce`,
`TaskSetDescriptionAllowedWithForceAndReason` (asserts the change
text contains both the new description and the reason),
`TaskSetDescriptionForceAloneStillRefuses` (covers empty-reason and
whitespace-only-reason variants),
`TaskSetDescriptionFilePreservesShellHostileContent`. Suite size
479 → 484 passing.

**kCliVersion**: still `0.104.1` until phases[3] closes the topic.
The `task set --description` surface is observable in the shipped
binary after phases[2] builds green.

### v0.105.0 behavior note — `--<field>-append-file` and topic seal

Final phase of the [CliAgentErgonomics](Docs/Plans/CliAgentErgonomics.Plan.json)
topic. Seals the v0.105.0 MINOR bump and closes the last accepted
proposal surface (P5 `--<field>-append-file`, append-only variant).

**New surface** (phase set only in this initial wave):

Every one of the 7 phase-design prose fields gains a
`--<field>-append-file <path>` sibling next to the existing
`--<field> <text>` and `--<field>-file <path>` replace flags:

- `--investigation-append-file`
- `--code-entity-contract-append-file`
- `--code-snippets-append-file`
- `--best-practices-append-file`
- `--multi-platforming-append-file`
- `--readiness-gate-append-file`
- `--handoff-append-file`

**Semantics** (shared helper
[`ComputeAppendOrReplace`](Source/UniPlanStringHelpers.h)):

- When the existing stored value is empty: append is equivalent to
  replace — no leading `\n\n` seam.
- When the existing value is non-empty: result is
  `<existing> + "\n\n" + <file content>`. The seam is exactly one
  blank line (double-newline) regardless of trailing whitespace on
  the existing value or leading whitespace on the file content.
- Closes the pull-concat-push round-trip pattern authors used to hit
  to grow a prose field by a small delta (e.g. 1k addition onto a
  9k investigation no longer requires 9k pulled + 10k pushed — just
  the delta pushed).

**Infrastructure**:

- [`BaseOptions`](Source/UniPlanOptionTypes.h) gains
  `std::set<std::string> mAppendFields`. Every Options struct
  inherits it.
- [`TryConsumeStringOrFileOrAppendFileOption`](Source/UniPlanOptionParsing.cpp)
  is a new parser helper that extends the v0.76.0
  `TryConsumeStringOrFileOption` with a third companion flag
  `--<field>-append-file <path>`. When the append branch matches, the
  file bytes land in `OutValue` AND the field key is inserted into
  `OutAppendFields`. The append-file branch is checked BEFORE the
  file branch because the two flags share a `--<field>` prefix; the
  longer match must win.
- [`ComputeAppendOrReplace`](Source/UniPlanStringHelpers.h) is a pure
  inline helper: `InAppend` selects between pass-through (replace)
  and seam-concat semantics.
- `RunPhaseSetCommand`'s `ApplyPhase` lambda routes through
  `ComputeAppendOrReplace` with `Options.mAppendFields.count(<key>) > 0`
  as the append toggle. Every one of the 7 design fields uses the
  same key string that matches what the parser wrote.

**Rollout scope**: phases[3] wires append-file into `phase set`'s 7
design-prose fields only. The infrastructure (`BaseOptions.mAppendFields`,
the parser helper, `ComputeAppendOrReplace`) is ready for future
waves to extend append-file to `topic set` / `phase log` /
`verification add/set` / `testing add/set` / `lane set` / `manifest
set` / typed-array groups. Each future wave is a separate MINOR bump
with its own tests.

**Tests** (`Test/UniPlanTestMutation.cpp`): 4 new fixtures —
`PhaseSetInvestigationAppendFileConcatsWithSeam`,
`PhaseSetAppendFileOnEmptyFieldActsAsReplace`,
`PhaseSetAppendFilePreservesShellHostileBytes`,
`PhaseSetAppendFileCrossFieldIndependence`. Suite size 484 → 488
passing.

**kCliVersion bump**: `0.104.1` → `0.105.0`
([Source/UniPlanCliConstants.h](Source/UniPlanCliConstants.h)). MINOR
per pre-1.0 SemVer — the topic adds 4 new observable surfaces
(`--ack-only`, `--all-phases`, `task set --description` with gate,
`--<field>-append-file`), zero deprecations, no bundle format change,
no evaluator-surface change, no concurrency-model change. Consumers
that don't use any new flag see identical behavior to v0.104.1.

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

The 10 schema files in `Schemas/` are V3 legacy artifacts used only by `uni-plan lint` for markdown filename pattern checking. V4 bundle validation uses `ValidateAllBundles()` with 35 evaluator functions against `FTopicBundle` data — it does not read Schema.md files.

| Schema | Purpose (lint only) |
|--------|---------------------|
| `Plan.Schema.md` | Plan .md section ordering |
| `Playbook.Schema.md` | Playbook .md section ordering |
| `Implementation.Schema.md` | Implementation .md section ordering |
| `*ChangeLog.Schema.md` (3) | ChangeLog .md sidecar structure |
| `*Verification.Schema.md` (3) | Verification .md sidecar structure |
| `Doc.Schema.md` | Base .md document structure |

## validation_checks

`uni-plan validate [--topic <T>] [--strict] [--human]` runs 35 evaluator functions against every `.Plan.json` bundle. Checks are split into three tiers:

### Structural checks (ErrorMajor + ErrorMinor) — 16 checks

Required fields, index references, enum values, timestamp format, and referential integrity. `ErrorMajor` always flips `valid=false`; `ErrorMinor` only does so under `--strict`.

| Check ID | Severity | Scope |
|---|---|---|
| `required_fields` | ErrorMajor | topic key + title |
| `phases_present` | ErrorMajor | ≥1 phase |
| `topic_key_matches_filename` | ErrorMinor | disk filename stem equals topic key (v0.94.0+) |
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
| `no_hollow_completed_phase` | Warning | A phase with `status=completed`, empty `jobs[]` / `testing[]` / `file_manifest[]`, AND `ComputePhaseDesignChars < kPhaseHollowChars` (3000 as of v0.83.0, ≈ 5-7 design fields populated with 1-3 sentences each). Catches governance lies where `completed` is claimed without substance. v0.82.0 tightened this from the prior binary-emptiness check on `code_snippets`/`investigation` to the unified chars threshold so trivial filler (e.g. `"TBD"`) no longer escapes detection; v0.83.0 recalibrated the threshold from 4000 to 3000 against V4 schema semantics. |
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

**`--help` is the authoritative per-command reference (v0.85.0+).** This
section lists the grammar so agents can discover the command surface;
flag-level detail lives on `uni-plan <cmd> [<sub>] --help` — every leaf
prints usage, required flags, options, mutually exclusive modes, the
output schema name, exit codes, and examples. `--help` always exits 0
to stdout, unknown subcommands still exit 2 with a `UsageError`.

### Command index

```
uni-plan --help                                  # global overview
uni-plan <cmd> --help                            # group help (subcommand index)
uni-plan <cmd> <sub> --help                      # per-subcommand detail
```

| Group | Subcommands | Purpose |
|---|---|---|
| `topic` | list, get, add, set, normalize, start, complete, block, status | Topic-level queries + lifecycle (v0.94.0+ adds `add` — create a new bundle) |
| `phase` | list, get, set, add, remove, normalize, start, complete, block, unblock, progress, complete-jobs, log, verify, next, readiness, wave-status, drift | Phase queries + lifecycle |
| `job` | set | Mutate a job |
| `task` | set | Mutate a task |
| `changelog` | (query), add, set, remove | Changelog queries + mutations |
| `verification` | (query), add, set | Verification queries + mutations |
| `lane` | set, add | Lane mutations |
| `testing` | add, set | Testing record mutations |
| `manifest` | add, remove, list, set | file_manifest mutations + disk-drift audit |
| `cache` | info, clear, config | Inventory cache management |
| `timeline` | — | Changelog+verification chronology for a topic |
| `blockers` | — | Phases with blocked status or blockers prose |
| `validate` | — | Run the 34-check validator surface |
| `legacy-gap` | — | V3↔V4 parity audit (stateless, 8 categories) |
| `watch` | — | FTXUI dashboard (built with `-DUPLAN_WATCH=1`) |

### Key output-shape flags

- `--human` — formatted ANSI output (every query supports it)
- `--phases <csv>` on `phase get` — batch mode, emits wrapped v2 schema `uni-plan-phase-get-v2` (single-phase continues v1)
- `--sections <csv>` on `topic get` — filter to named top-level prose sections (14 enumerated names)
- `--brief` / `--design` / `--execution` on `phase get` — mutually exclusive view modes; default is the full view including testing + file_manifest
- `--stale-plan` on `manifest list` — drift filter (stale_create / stale_delete / dangling_modify), AND-intersects with `--missing-only`
- `--strict` on `validate` — promotes ErrorMinor + Warning to fail (`valid=false`, exit 1)

### Default JSON output shape

Every query command emits JSON with:
- Top-level schema envelope: `schema`, `generated_utc`, `repo_root`
- `issues[]` — one entry per failing check (id, severity, topic, path, line, detail) for validator commands
- `summary` — aggregate stats for `validate`: `topic_count`, `topics[].phase_count`, `topics[].status_distribution`, and per-phase `scope_chars`, `output_chars`, `design_chars` (sum of investigation + code_entity_contract + code_snippets + best_practices + handoff + readiness_gate + multi_platforming), `jobs_count`, `testing_count`, `file_manifest_count`, `file_manifest_missing`. Added in v0.71.0 so agents can audit the full corpus through a single CLI invocation. For richer runtime-only phase depth/intensity metrics, use `uni-plan phase metric` (v0.103.0); it does not alter the plan schema.

### File-based prose input

Every `--<field> <text>` mutation flag has a sibling `--<field>-file <path>` (v0.76.0+). The file form reads raw bytes and bypasses shell expansion — prefer it for any content containing `$`, backticks, double quotes, or newlines. Documented on every prose-bearing leaf's `--help` via the shared `kFileFlagFooter`.
