# coding_conventions

When writing or modifying C++ code in this repository, follow these conventions.

## formatting

- **Indent**: 4 spaces, no tabs
- **Braces**: Allman style (opening brace on its own line)
- **Column limit**: 80 characters
- **One statement per line**
- **Access specifiers**: at column 0

## headers

- `#pragma once` in every header
- Include order: own header first, then project headers, then standard library headers
- Never `using namespace` — use fully qualified `UniPlan::` paths
  - Exception: `namespace fs = std::filesystem` alias in `UniPlanTypes.h`

## source_placement

All source files live in `Source/` (flat directory, no Public/Private split).

## solid_principles

1. **S — Single Responsibility**: One change-driver per struct/class/file. If a type mixes unrelated concerns, decompose it into focused `F`-prefix sub-structs.
2. **O — Open/Closed**: Extend behavior through registration, composition, or `std::variant` + `std::visit` — not by growing central `if`/`else`/`switch` chains beyond 3 branches.
3. **L — Liskov Substitution**: Every override must preserve the base contract's observable behavior.
4. **I — Interface Segregation**: Keep interfaces narrow and role-specific.
5. **D — Dependency Inversion**: Use domain types (`F`-prefix structs, `E`-prefix enums) instead of raw primitives. High-level policy depends on abstractions, not concrete details.

## domain_types

Use typed structs and enums instead of raw primitives:

| Pattern | Implementation | When to use |
|---------|---------------|-------------|
| Domain struct | `struct FPassDescriptor { ... };` | Any structured domain data |
| Enum class | `enum class EPassType : uint8_t { ... };` | Any fixed set of named values; replace >3 string comparisons |
| Optional state | `std::optional<T>` | Replace `bHasX + mX` boolean-value pairs |

## memory_ownership

Preferred order: **value > unique_ptr > optional > shared_ptr > raw pointer**

- No raw `new`/`delete` — use `std::make_unique<T>()` or value types
- Use `rp` prefix for non-owning raw pointers

## no_if_else_hell

Replace long if/else chains (>3 branches on the same key) with:
- `enum class` + `switch` for state/type dispatch
- `std::unordered_map<Key, Handler>` for open-ended surfaces
- `std::variant<A, B, C>` + `std::visit` for closed type sets

## error_output

Use `std::cerr` for CLI diagnostic output (no custom log macros — project scope doesn't warrant them).

## long_term_fixes_only

- Never use workarounds, quick hacks, or backward-compatibility shims
- Fix the root cause — if a bug is caused by a design flaw, fix the design
- No `// TODO: fix later` or `// HACK` comments — fix it now
