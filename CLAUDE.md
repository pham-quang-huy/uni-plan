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

### V4 query commands

```bash
uni-plan topic list [--status <filter>] [--json|--human]
uni-plan topic get --topic <topic> [--json|--human]
uni-plan phase list --topic <topic> [--status <filter>] [--json|--human]
uni-plan phase get --topic <topic> --phase <N> [--brief|--execution|--reference] [--json|--human]
uni-plan changelog --topic <topic> [--phase <N>] [--json|--human]
uni-plan verification --topic <topic> [--phase <N>] [--json|--human]
uni-plan timeline --topic <topic> [--since <yyyy-mm-dd>] [--json|--human]
uni-plan blockers [--topic <topic>] [--json|--human]
uni-plan validate [--topic <topic>] [--strict] [--json|--human]
```

### V4 mutation commands

```bash
uni-plan topic set --topic <topic> [--status <status>] [--next-actions <text>]
uni-plan phase set --topic <topic> --phase <N> [--status <s>] [--done <t>] [--remaining <t>] [--blockers <t>] [--context <t>]
uni-plan job set --topic <topic> --phase <N> --job <N> --status <status>
uni-plan task set --topic <topic> --phase <N> --job <N> --task <N> [--status <s>] [--evidence <t>] [--notes <t>]
uni-plan changelog add --topic <topic> [--phase <N>] --change <text> [--type <feat|fix|refactor|chore>]
uni-plan verification add --topic <topic> [--phase <N>] --check <text> [--result <text>] [--detail <text>]
```

### Utility commands

```bash
uni-plan cache info|clear|config [--dir <path>] [--json|--human]
uni-plan watch [--repo <path>]
uni-plan lint [--json|--human]  # legacy .md filename check only
```
