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
├── Docs/                      # uni-plan's own development plans
│   ├── INDEX.md               # Plan discovery index
│   ├── Plans/                 # Plan documents
│   ├── Implementation/        # Implementation trackers
│   └── Playbooks/             # Phase playbooks
├── ThirdParty/                # FTXUI (terminal UI library)
├── Build/                     # CMake output directory
├── .claude/                   # Claude Code system
│   ├── settings.json          # Hook configuration
│   ├── hooks/                 # 3 hook scripts
│   ├── rules/                 # 2 auto-loaded rule files
│   └── skills/                # 11 skills (upl-* prefix)
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
| Skills | 11 | `.claude/skills/upl-*/` — see skill table below |

## project_skills

| Skill | Path | When to use |
|-------|------|-------------|
| `upl-commit` | `.claude/skills/upl-commit/SKILL.md` | User requests a commit — validates format, SemVer gate |
| `upl-code-fix` | `.claude/skills/upl-code-fix/SKILL.md` | Fixing bugs — workaround gate, SOLID assessment |
| `upl-code-refactor` | `.claude/skills/upl-code-refactor/SKILL.md` | Structural cleanup — SOLID enforcement, detection patterns |
| `upl-schema-audit` | `.claude/skills/upl-schema-audit/SKILL.md` | Audit Schemas/*.Schema.md for consistency |
| `upl-validation-new` | `.claude/skills/upl-validation-new/SKILL.md` | Add a new validation check — scaffold + wire |
| `upl-watch-panel` | `.claude/skills/upl-watch-panel/SKILL.md` | Add/modify watch mode TUI panels |
| `upl-claude-audit` | `.claude/skills/upl-claude-audit/SKILL.md` | Audit .claude/ system integrity |
| `upl-claude-learn` | `.claude/skills/upl-claude-learn/SKILL.md` | Learn from sessions, propose .claude/ updates |
| `upl-plan-creation` | `.claude/skills/upl-plan-creation/SKILL.md` | Create governed plan bundles |
| `upl-plan-execution` | `.claude/skills/upl-plan-execution/SKILL.md` | Execute plan phases with governance gates |
| `upl-plan-audit` | `.claude/skills/upl-plan-audit/SKILL.md` | Audit plan topics via uni-plan CLI |

## cli_semver_discipline

| Bump | When |
|------|------|
| MAJOR | Breaking changes: command renames, removed options, schema format changes |
| MINOR | New features: new commands, new validation checks, new output fields |
| PATCH | Bug fixes, documentation, internal refactoring |

**Version source**: `Source/UniPlanTypes.h` → `kCliVersion`
**Trigger files**: All files in `Source/`

Before committing any `Source/` changes, verify `kCliVersion` was bumped appropriately.

## documentation_rules

### V4 bundle model

Each topic is a single `.Plan.json` file in `Docs/Plans/`. The bundle contains all plan metadata, phases (with lifecycle + design material), changelogs, and verifications. There are no separate implementation trackers, playbook files, or sidecar files.

| Item | Location |
|------|----------|
| Topic bundle | `Docs/Plans/<TopicPascalCase>.Plan.json` |
| Phases | Inline array in the bundle (`mPhases`) |
| Changelogs | Inline array in the bundle (`mChangeLogs`) |
| Verifications | Inline array in the bundle (`mVerifications`) |

### Legacy .md naming (lint only)

The `.md` naming patterns below apply to non-plan documentation (specs, ADRs, references). Plan/implementation/playbook topics are `.Plan.json` bundles.

| Doc Type | Pattern | Placement |
|----------|---------|-----------|
| Plan (legacy) | `<TopicPascalCase>.Plan.md` | `Docs/Plans/` |
| Implementation (legacy) | `<TopicPascalCase>.Impl.md` | `Docs/Implementation/` |
| Playbook (legacy) | `<TopicPascalCase>.<PhaseKey>.Playbook.md` | `Docs/Playbooks/` |

## schema_files

The 10 schema files in `Schemas/` are V3 legacy artifacts used only by `uni-plan lint` for markdown filename pattern checking. V4 bundle validation uses `ValidateAllBundles()` with 18 evaluator functions against `FTopicBundle` data — it does not read Schema.md files.

| Schema | Purpose (lint only) |
|--------|---------------------|
| `Plan.Schema.md` | Plan .md section ordering |
| `Playbook.Schema.md` | Playbook .md section ordering |
| `Implementation.Schema.md` | Implementation .md section ordering |
| `*ChangeLog.Schema.md` (3) | ChangeLog .md sidecar structure |
| `*Verification.Schema.md` (3) | Verification .md sidecar structure |
| `Doc.Schema.md` | Base .md document structure |

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
uni-plan topic set --topic <T> [--status <s>] [--next-actions <text>]
uni-plan phase set --topic <T> --phase <N> [--status <s>] [--done <t>] [--remaining <t>] [--blockers <t>] [--context <t>]
uni-plan job set --topic <T> --phase <N> --job <N> --status <s>
uni-plan task set --topic <T> --phase <N> --job <N> --task <N> [--status <s>] [--evidence <t>] [--notes <t>]
uni-plan changelog add --topic <T> [--phase <N>] --change <text> [--type <feat|fix|refactor|chore>] [--affected <t>]
uni-plan verification add --topic <T> [--phase <N>] --check <text> [--result <text>] [--detail <text>]
uni-plan lane set --topic <T> --phase <N> --lane <N> --status <s>
uni-plan testing add --topic <T> --phase <N> --step <text> --action <text> --expected <text> [--actor <human|ai>] [--session <s>] [--evidence <t>]
uni-plan manifest add --topic <T> --phase <N> --file <path> --action <create|modify|delete> --description <text>
```

### Utility commands

```bash
uni-plan cache info|clear|config [--dir <path>] [--human]
uni-plan watch [--repo-root <path>]
uni-plan lint [--human]  # legacy .md filename check only
```
