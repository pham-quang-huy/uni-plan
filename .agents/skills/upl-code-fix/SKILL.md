---
name: upl-code-fix
description: Fix bugs and issues with root-cause solutions. Use this skill whenever fixing a bug, resolving an error, or correcting incorrect behavior. Enforces a strict no-workaround policy with a 5-point workaround gate. Always fix the design, never patch the caller.
implicit_invocation: true
---

# UPL Fix

Use this skill to fix bugs and issues with long-term, root-cause solutions. Never use workarounds, quick hacks, or backward-compatibility shims.

## Workflow

### Step 1: Diagnose

1. **Reproduce**: Confirm the bug exists with a concrete test case
2. **Root cause**: Identify the actual design flaw, not just the symptom
3. **Scope**: List all files affected by the root cause (not just the crash site)

### Step 2: Workaround Gate

Before writing any fix code, answer ALL five questions. If any answer is "yes", you are about to write a workaround — stop and rethink.

| # | Question | If "yes" |
|---|----------|----------|
| 1 | Does this fix add a special case for one caller? | Fix the API, not the caller |
| 2 | Does this preserve a broken interface while routing around it? | Fix the interface |
| 3 | Does this add a `// TODO` or `// HACK` comment? | Fix it now |
| 4 | Does this add backward-compat code for old behavior? | Migrate callers, delete old code |
| 5 | Would you be embarrassed to explain this fix in a code review? | Find the real fix |

### Step 3: Structural Assessment

Before fixing, check if the surrounding code has SOLID violations that caused or masked the bug:

| SOLID | Threshold | Action |
|-------|-----------|--------|
| **S** | God struct (>50 fields mixing unrelated domains) | Decompose into `F`-prefix sub-structs before fixing |
| **S** | Monolith file (>1000 lines) | Split relevant section before fixing |
| **O** | If/else chain (>3 branches on same key) | Refactor to dispatch table, `std::variant` + `std::visit`, or polymorphic handler |
| **O** | String enums (>3 comparisons) | Extract `enum class E<Name>` first |
| **D** | Raw primitives as domain data | Introduce `F`-prefix structs and `E`-prefix enums |

If structural issues exist, invoke `upl-code-refactor` first, then fix the bug in the cleaned-up code.

### Step 4: Fix with SOLID Design

Fix the design, not the symptom. Apply the appropriate SOLID principle:

- **S**: If the bug exists because one type does too many things, split it
- **O**: If the bug exists because a new case was missed in a switch/if-else chain, replace with extensible dispatch
- **L**: If the bug exists because an override diverges from the base contract, fix the override
- **I**: If the bug exists because a no-op method was called, split the interface
- **D**: If the bug exists because policy depends on a concrete detail, introduce an abstraction boundary

**Additional fix conventions**:
- Follow NAMING.md conventions (all prefixes: `m`, `mb`, `k`, `In`, `Out`, `rp`)
- Use `std::optional<T>` instead of `bHasX + mX` pairs
- Use `std::make_unique<T>()` instead of raw `new`/`delete`
- Use `enum class` instead of string comparisons or magic integers
- **No duplication**: If the fix duplicates existing logic, extract a shared function/method
- **No file-scope static functions**: Place reusable helpers in UniPlanHelpers.h or a dedicated utility header

### Step 5: Verify

1. **Build**: `./build.sh`
2. **Test**: Confirm the original bug no longer reproduces
3. **Regression**: Grep for callers of modified APIs to confirm they still work
4. **Scope check**: Verify no unintended behavioral changes

## Anti-Pattern Table

| Anti-Pattern | SOLID | Replace With |
|-------------|-------|-------------|
| `catch (...) {}` | — | `std::cerr` with context |
| `// TODO: fix later` | — | Fix now |
| Caller-side null check for broken API | — | Fix the API to never return null |
| God struct mixing unrelated state | **S** | Decompose into focused `F`-prefix sub-structs |
| `if (type == "X") ... else if (type == "Y") ...` >3 branches | **O** | `enum class` + dispatch table or `std::variant` + `std::visit` |
| `if (oldFormat) ... else if (newFormat)` | **O** | Pick canonical format, migrate all callers |
| `bHasX + mX` boolean-value pairs | **D** | `std::optional<T>` |
| Raw `new`/`delete` | **D** | `std::make_unique<T>()` or value types |
| String comparison for state | **O, D** | `enum class` with `E` prefix |
| Duplicated logic across files | **S** | Extract shared function/method |

## Rules

- Never mix fixing with unrelated refactoring
- Build-verify after every significant change
- If the fix reveals a deeper design issue, fix the design
- If the fix is in a shared utility (`UniPlanHelpers.h`), verify all callers
