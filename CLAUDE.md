# uni-plan

## project_overview

uni-plan is a standalone C++17 CLI tool for plan governance — validating, linting, and monitoring markdown-based plan/playbook/implementation document bundles across repositories.

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
│   ├── UniPlanValidation.cpp # 28 validation checks
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

### Document naming

| Doc Type | Pattern | Placement |
|----------|---------|-----------|
| Plan | `<TopicPascalCase>.Plan.md` | `Docs/Plans/` |
| Implementation | `<TopicPascalCase>.Impl.md` | `Docs/Implementation/` |
| Playbook | `<TopicPascalCase>.<PhaseKey>.Playbook.md` | `Docs/Playbooks/` |
| Plan ChangeLog | `<Topic>.Plan.ChangeLog.md` | `Docs/Plans/` |
| Plan Verification | `<Topic>.Plan.Verification.md` | `Docs/Plans/` |
| Impl ChangeLog | `<Topic>.Impl.ChangeLog.md` | `Docs/Implementation/` |
| Impl Verification | `<Topic>.Impl.Verification.md` | `Docs/Implementation/` |
| Playbook ChangeLog | `<Topic>.<Phase>.Playbook.ChangeLog.md` | `Docs/Playbooks/` |
| Playbook Verification | `<Topic>.<Phase>.Playbook.Verification.md` | `Docs/Playbooks/` |

### Pairing rules

- Every plan must have a paired implementation tracker
- Every active phase must have a dedicated playbook
- Every plan/playbook/impl must have ChangeLog + Verification sidecars

## schema_files

The 10 canonical schema files in `Schemas/` define document structure contracts:

| Schema | Purpose |
|--------|---------|
| `Doc.Schema.md` | Base document structure (H1 + section_menu) |
| `Plan.Schema.md` | Plan required/optional sections and ordering |
| `Playbook.Schema.md` | Playbook required/optional sections (most detailed) |
| `Implementation.Schema.md` | Implementation tracker sections |
| `PlanChangeLog.Schema.md` | Plan change log sidecar structure |
| `PlanVerification.Schema.md` | Plan verification sidecar structure |
| `PlaybookChangeLog.Schema.md` | Playbook change log sidecar structure |
| `PlaybookVerification.Schema.md` | Playbook verification sidecar structure |
| `ImplChangeLog.Schema.md` | Implementation change log sidecar structure |
| `ImplVerification.Schema.md` | Implementation verification sidecar structure |

Schemas are parsed at runtime by `BuildSectionSchemaEntries()` in `Source/UniPlanParsing.cpp`. Schema resolution order: repo-local → bundled (next to binary) → fallback.

## cli_commands

```bash
uni-plan list [--type plan|impl|playbook|pair] [--status <filter>]
uni-plan phase [--topic <topic>]
uni-plan lint
uni-plan validate
uni-plan diagnose drift
uni-plan blockers
uni-plan artifacts --topic <topic>
uni-plan section list --doc <path>
uni-plan section content --doc <path> --id <section>
uni-plan section schema --type <plan|playbook|implementation>
uni-plan schema [--type <doctype>]
uni-plan inventory
uni-plan orphan-check
uni-plan watch [--repo <path>]
```
