---
name: upl-code-refactor
description: Structural decomposition and cleanup specialist. Use this skill when decomposing god structs, splitting monolith files, introducing domain types, extracting enums, or enforcing SOLID principles. Preserves behavior exactly — structural changes only.
implicit_invocation: true
---

# UPL Refactor

Use this skill for structural cleanup that preserves behavior exactly. Refactoring changes how code is organized, not what it does.

## SOLID Principles

### S — Single Responsibility

One change-driver per struct/class/file. If a type mixes unrelated concerns, decompose it.

| Smell | Detection | Refactoring |
|-------|-----------|-------------|
| God struct mixing unrelated state | Struct has >50 fields, or fields span multiple domains | Decompose into focused `F`-prefix sub-structs |
| Monolith file (>1000 lines) | Multiple responsibilities in one `.cpp`/`.h` | Split by domain into separate files in `Source/` |
| Mixed anonymous namespace (>20 functions) | Unrelated helpers lumped together | Extract into focused helper files by domain |

### O — Open/Closed

Extend behavior through registration, composition, or polymorphism — not by growing central `if`/`else`/`switch` chains.

| Pattern | When to use |
|---------|-------------|
| `std::unordered_map<EKey, Handler>` | Command dispatch, any surface expected to grow (>5 entries) |
| `std::variant<A, B, C>` + `std::visit` | Closed set of types known at compile time |
| Virtual dispatch | Open-ended extension points, cold paths |

**When switch/if-else IS fine**: 2-3 branches on a stable enum, small fixed state machines.

### L — Liskov Substitution

Every override must preserve the base contract's observable behavior.

### I — Interface Segregation

Keep interfaces narrow and role-specific.

### D — Dependency Inversion

Use domain types (`F`-prefix structs, `E`-prefix enums) instead of raw primitives.

## Detection Patterns

### RS-01: God Struct (>50 fields) — **S**

Split into focused `F`-prefix sub-structs grouped by domain purpose.

### RS-02: If/Else Hell (>3 branches on same key) — **O**

Replace with `enum class` + dispatch, `std::variant` + `std::visit`, or lookup table.

### RS-03: Untyped State (`bHasX + mX` pairs) — **D**

Replace boolean+value pairs with `std::optional<T>`.

### RS-04: String Enums (string comparisons >3) — **O, D**

Replace with `enum class E<Name>`.

### RS-05: Raw Primitives as Domain Data — **D**

Replace `int`, `float`, `std::string`, `bool` representing domain concepts with `F`-prefix structs and `E`-prefix enums.

### RS-06: Mixed Concerns (anonymous namespace >20 functions) — **S**

Extract into focused helper files by domain.

### RS-07: Monolith File (>1000 lines) — **S**

Split by responsibility. Each new file goes in `Source/` with `UniPlan` prefix.

### RS-10: Code Duplication — **S**

Identical or near-identical logic in 2+ places. Extract shared function/method.

### RS-11: File-Scope Static Functions with Reuse Potential — **S, D**

`static` free functions that perform reusable logic. Move to `UniPlanHelpers.h` or a dedicated utility header.

## Workflow

### 1. Scan

Run detection patterns on target files. Categorize by pattern ID, SOLID principle, and severity.

### 2. Plan Batches

Group changes by cohesion:
- **Batch 1**: Type extractions (new `F`-prefix structs, `E`-prefix enums)
- **Batch 2**: File splits (move functions to new files in `Source/`)
- **Batch 3**: Caller updates (update includes and usage)

### 3. Execute One Batch

- Structural changes only — behavior must be identical
- Follow NAMING.md for all new types
- Follow CODING.md for memory ownership
- Update `CMakeLists.txt` if new source files added

### 4. Build Verify

```bash
./build.sh
```

Must compile clean before proceeding.

### 5. Repeat

Execute next batch, build verify, until all changes applied.

## Safety Rules

- **Never change behavior** — refactoring is structural only
- **Never introduce workarounds** — fix the design
- **Build after every batch** — compilation is the primary safety net
- **Preserve all callers** — every call site must still compile and behave identically
- **One pattern at a time** — don't mix RS-01 and RS-04 in the same batch
- **Extract, don't duplicate** — if a refactoring creates shared logic, put it in UniPlanHelpers.h

## Output Format

| ID | SOLID | Pattern | File | Severity | Count |
|----|-------|---------|------|----------|-------|
| RS-01 | S | God struct | path:line | HIGH | fields |
| RS-02 | O | If/else hell | path:line | MEDIUM | branches |

| Batch | Files Modified | Files Created | Build |
|-------|---------------|---------------|-------|
| 1 | 3 | 2 | pass |
