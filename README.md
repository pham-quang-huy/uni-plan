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
| Version source | [Source/UniPlanTypes.h](Source/UniPlanTypes.h) → `kCliVersion` (currently `0.71.1`) |
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
├── Source/                    # C++17 implementation (flat directory)
│   ├── Main.cpp               # Entry point
│   ├── UniPlanTypes.h         # kCliVersion, JSON schema constants, core types
│   ├── UniPlanForwardDecls.h  # Run*Command declarations
│   ├── UniPlanRuntime.*       # Runtime engine
│   ├── UniPlanCommand*.cpp    # Command dispatch (Bundle, Plan, Evidence, Search, Dispatch)
│   ├── UniPlanMutation.*      # Mutation pipeline + typed mutation operations
│   ├── UniPlanParsing.cpp     # V4 bundle parsing into FTopicBundle
│   ├── UniPlanValidation.cpp  # V4 evaluator functions (ValidateAllBundles)
│   ├── UniPlanSchemaValidation.* # Schema-driven structural validation
│   ├── UniPlanCache.cpp       # Caching layer (uni-plan.ini)
│   ├── UniPlanOutput*.cpp     # JSON / Text / ANSI-Human formatters
│   ├── UniPlanWatch*.{cpp,h}  # FTXUI watch-mode TUI (App, Panels, Snapshot)
│   └── UniPlan*Helpers.h      # String / JSON / file / markdown / status / inventory utilities
├── Schemas/                   # Legacy V3 .md schemas (used only by `uni-plan lint`)
├── Docs/                      # Plan corpus — active .Plan.json bundles live in Docs/Plans/
├── Test/                      # GoogleTest suite
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
