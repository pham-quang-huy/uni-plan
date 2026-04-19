---
name: upl-code-refactor
description: Structural decomposition and cleanup specialist. Use this skill when decomposing god structs, splitting monolith files, introducing domain types, extracting enums, or enforcing SOLID principles. Preserves behavior exactly — structural changes only.
implicit_invocation: true
---

# UPL Refactor

Use this skill for structural cleanup that preserves behavior exactly. Refactoring changes how code is organized, not what it does.

## CLI-Aware SOLID

uni-plan is a CLI governance tool (C++17, no ECS, no real-time constraints). SOLID applies fully with no hot-path exemptions. All dispatch surfaces (commands, validation checks, output formats) are cold paths where abstraction overhead is negligible.

| Code path | Abstraction posture | Rationale |
|-----------|-------------------|-----------|
| Command dispatch (~20 commands) | Map/table lookup or `if`-chain | Growing surface — prefer registry pattern |
| Validation checks (~28 checks) | Vector of check functions | Open-ended, new checks added regularly |
| Output format dispatch (JSON/text/human) | 3-way branch on options struct | Stable, small N, switch is fine |
| Markdown/JSON parsing | Free functions in `UniPlan::` namespace | Value-oriented, no polymorphism needed |
| Watch mode panels (~18 panels) | Function dispatch per panel type | Moderate N, extend via new panel functions |

## SOLID Principles

### S — Single Responsibility

One change-driver per struct/class/file. If a type mixes unrelated concerns, decompose it.

| Smell | Detection | Refactoring |
|-------|-----------|-------------|
| God struct mixing unrelated state | Struct has >50 fields, or fields span multiple domains (e.g., inventory + validation + watch in one type) | Decompose into focused `F`-prefix sub-structs grouped by domain (`FDocumentIdentity`, `FPhaseTaxonomy`, `FValidateCheck`) |
| Monolith file (>1000 lines) | Multiple responsibilities in one `.cpp`/`.h` | Split by domain into separate files in `Source/` with `UniPlan` prefix |
| Mixed anonymous namespace (>20 functions) | Unrelated helpers lumped together | Extract into focused helper files by domain |
| Types header spanning multiple domains | Historical example: `UniPlanTypes.h` (pre-v0.72.0) mixed CLI constants + option types + inventory types + result types | Split into focused domain headers. In v0.72.0, `UniPlanTypes.h` became an IWYU umbrella re-exporting `UniPlanCliConstants.h`, `UniPlanOptionTypes.h`, `UniPlanInventoryTypes.h`, `UniPlanResultTypes.h`. |

**When NOT to over-apply**: Do not create one-field structs or one-function files for purity. If fields always change together, they belong in one struct. Test: "Do these fields change for the same reason at the same time?"

### O — Open/Closed

Extend behavior through registration, composition, or polymorphism — not by growing central `if`/`else`/`switch` chains.

**C++17 dispatch alternatives** (choose by context):

| Pattern | When to use | When NOT to use |
|---------|-------------|-----------------|
| `std::unordered_map<EKey, Handler>` | Command dispatch, validation check registration, any surface expected to grow (>5 entries) | Trivial 2-3 case dispatch where a switch is clearer |
| `std::variant<A, B, C>` + `std::visit` | Closed set of types known at compile time, value semantics required | Open-ended type sets, large size disparity between alternatives |
| Virtual dispatch | Open-ended extension points | uni-plan has no virtual dispatch currently — avoid introducing it unless genuinely needed |
| Registry/table pattern | Command surfaces, output format selection | One-off branching that will never grow |

**`std::variant` closed dispatch example**:

```cpp
// Closed set: all document types known at compile time
using DocumentVariant = std::variant<FPlanDoc, FPlaybookDoc, FImplDoc>;

struct FDocumentProcessor
{
    void operator()(FPlanDoc& InDoc)     { /* plan logic */ }
    void operator()(FPlaybookDoc& InDoc) { /* playbook logic */ }
    void operator()(FImplDoc& InDoc)     { /* impl logic */ }
};

// Compiler enforces exhaustive handling
for (auto& Doc : Documents)
    std::visit(FDocumentProcessor{}, Doc);
```

**When switch/if-else IS fine**:

- 2-3 branches on a stable `enum class` unlikely to grow
- Output format dispatch (`mbJson` / `mbHuman` / default text)
- Small, fixed state machines where the switch reads clearly

**When switch/if-else IS a smell**:

- Every new command/check adds a case to the same chain (shotgun surgery)
- Same enum switched on in multiple files
- String comparisons `if (name == "X")` — extract to `enum class`
- >3 branches on the same key set growing over time

**Sibling consistency rule**: When operations in the same family use different architectural patterns, that is a refactoring target even if the inconsistent member currently works. Example: if `list` uses cached inventory but `phase` rebuilds it from scratch, the inconsistency is the bug waiting to happen.

### L — Liskov Substitution

Every override must preserve the base contract's observable behavior and lifecycle intent.

| Smell | Detection | Refactoring |
|-------|-----------|-------------|
| No-op override | Override does nothing or returns dummy value | Base interface too broad — split it (see **I**) |
| Contract violation | Function returns different error semantics depending on document format | Align to shared contract; document format-specific behavior in the interface |

**Practical test**: Can you swap the JSON path for the markdown path and have all downstream code still function correctly? If not, the abstraction is leaking.

### I — Interface Segregation

Keep interfaces narrow and role-specific. Prefer many focused free-function groups over one fat utility header.

| Smell | Detection | Refactoring |
|-------|-----------|-------------|
| Fat utility header | `UniPlanHelpers.h` with JSON helpers + string helpers + file helpers + status helpers | Split into `UniPlanJsonHelpers.h`, `UniPlanStringHelpers.h`, `UniPlanFileHelpers.h` |
| Unused functions in shared header | Callers use <50% of functions in an included header | Extract the used subset into a narrower header |

**When splitting HURTS**: Do NOT create one header per function. Group by client role, not by granularity. If all callers use all functions, the header is correctly cohesive.

### D — Dependency Inversion

Use domain types (`F`-prefix structs, `E`-prefix enums) instead of raw primitives.

| Smell | Detection | Refactoring |
|-------|-----------|-------------|
| Raw primitives as domain data | `int`, `float`, `std::string`, `bool` representing domain concepts (status, severity, pair state) | Introduce `F`-prefix domain structs and `E`-prefix enums |
| Untyped maps as configuration | `std::map<std::string, std::string>` for structured config | Replace with `F`-prefix typed config struct with named fields |
| String-typed status values | `std::string mStatus` compared against string literals | Use `EPhaseStatus` enum class with `ToString()`/`FromString()` |

## Domain Type Patterns

Strong typing eliminates entire categories of bugs at compile time.

| Pattern | C++17 Implementation | uni-plan Convention | When to use |
|---------|----------------------|---------------------|-------------|
| Domain struct | `struct FDocumentIdentity { ... };` | `F` prefix | Any structured domain data |
| Enum class | `enum class EPhaseStatus : uint8_t { NotStarted, InProgress };` | `E` prefix | Any fixed set of named values; replace >3 string comparisons |
| Type-safe ID | `enum class TopicID : uint32_t {};` | Use `enum class` for IDs | Prevent mixup of same-underlying-type values |
| Optional state | `std::optional<FSectionContent> opSection;` | `op` prefix | Replace `bHasX + mX` pairs. No heap allocation |
| Sum type | `using ParseResult = std::variant<FDocument, FParseError>;` | Named `using` alias | Closed-set result types; replace error code integers |

**When raw primitives are acceptable**: local loop counters, array indices within a single function, intermediate string building. Do not wrap `int LineNumber` in a struct if it never leaves the function.

## Composition vs. Inheritance

| Prefer Composition | Prefer Inheritance |
|--------------------|-------------------|
| Behavior varies independently along multiple axes | Genuine "is-a" with a stable contract |
| Runtime flexibility to add/remove capabilities | Shallow hierarchy (1-2 levels) |
| Multiple unrelated types need same capability | Never in uni-plan (no class hierarchies) |
| Want value semantics (copyable, no heap) | — |

**In uni-plan**: The codebase uses composition exclusively (F-prefix structs, free functions, namespace). There are no class hierarchies and no virtual dispatch. Keep it that way unless a genuinely open-ended extension point emerges.

## Detection Patterns

Structural anti-patterns mapped to SOLID principles:

### RS-01: God Struct (>50 fields) — **S**

Split into focused `F`-prefix sub-structs grouped by domain purpose.

```bash
grep -c "^\s*\(std::\|F\|E\|bool\|int\|float\|size_t\|uint\)" Source/*.h
```

### RS-02: If/Else Hell (>3 branches on same key) — **O**

Replace with `enum class` + `switch`, `std::variant` + `std::visit`, or `std::unordered_map<Key, Handler>`.

```bash
grep -c "else if\|} else {" Source/*.cpp
```

### RS-03: Untyped State (`bHasX + mX` pairs >5) — **D**

Replace boolean+value pairs with `std::optional<T>` using `op` prefix.

```bash
grep -c "bHas\|bIs\|bCan\|bShould" Source/*.h
```

### RS-04: String Enums (string comparisons >3) — **O, D**

Replace with `enum class E<Name> : uint8_t` with `ToString()`/`FromString()`.

```bash
grep -c '==\"\|!=\"\|\.compare(' Source/*.cpp
```

### RS-05: Raw Primitives as Domain Data — **D**

Replace `int`, `float`, `std::string`, `bool` representing domain concepts with `F`-prefix structs and `E`-prefix enums.

```bash
grep -n "std::string mStatus\|std::string mKind\|std::string mSeverity" Source/*.h
```

### RS-06: Mixed Concerns (anonymous namespace >20 functions) — **S**

Extract into focused helper files in `Source/`.

### RS-07: Monolith File (>1000 lines) — **S**

Split by responsibility. Each new file goes in `Source/` with `UniPlan` prefix.

```bash
wc -l Source/*.cpp Source/*.h | sort -rn | head -10
```

### RS-07b: Monolith Runtime File — **S**

Files that dump all command logic into one place (e.g., `UniPlanRuntime.cpp` at ~3000 lines). Split into domain-specific command handler files named by the command group they own (e.g., `UniPlanCommandDispatch.cpp`, `UniPlanCommandRead.cpp`, `UniPlanCommandPlan.cpp`).

```bash
wc -l Source/UniPlanRuntime.cpp
```

### RS-08: Fat Utility Header — **I**

A single header (e.g., `UniPlanHelpers.h`) mixing JSON helpers + string helpers + file I/O + status logic. Split into focused headers by client role.

### RS-09: Missing Abstraction Layer — **D, I**

Multiple functions share the same data access pattern but lack a common abstraction. Example: CLI commands and watch mode both call `BuildInventory()` separately but could share a `DocumentStore` layer.

### RS-10: Code Duplication — **S**

Identical or near-identical logic in 2+ places. Extract shared function in the owning module.

```bash
# Look for suspiciously similar blocks across files
grep -rn "BuildInventory\|ParseHeadingRecords\|ParseMarkdownTables" Source/
```

### RS-11: File-Scope Static Functions with Reuse Potential — **S, D**

`static` free functions in `.cpp` files that perform reusable logic. Move to `UniPlanHelpers.h` or a dedicated utility header.

```bash
grep -n "^static " Source/*.cpp | wc -l
```

Exception: truly file-local one-off helpers with no reuse potential may remain `static`.

### RS-12: Watch Type Duplication — **S, D**

Watch-specific types (`FWatchLaneItem`, `FWatchJobItem`, `FWatchTaskItem`) that duplicate domain types already present or needed in the CLI path. Unify into shared types in a domain header.

### RS-13: Missing Enum for String Status — **D**

Status values stored as `std::string` and compared via `==` against string literals (`"not_started"`, `"completed"`, etc.). Replace with `enum class` + `ToString()`/`FromString()` dispatch.

```bash
grep -n '"not_started"\|"in_progress"\|"completed"\|"closed"\|"blocked"' Source/*.cpp | wc -l
```

### RS-14: Lowercase Acronym in Name — **D**

Acronyms must be ALL CAPS in all contexts per NAMING.md: `ID`, `JSON`, `URL`, `UTC`, `CSV`. Not `Id`, `Json`, `Url`.

```bash
grep -rn "mId\b\|mUrl\b\|mJson\b\|mUtc\b" Source/*.h Source/*.cpp
```

### RS-15: Shortened Word in Name — **D**

Abbreviated or truncated words in identifiers. Use the full word per NAMING.md.

| Shortened | Full |
|-----------|------|
| `Impl` | `Implementation` (exception: file extension `.Impl.md` is canonical) |
| `Config` | `Configuration` |
| `Doc` | `Document` |
| `Info` | `Information` |

### RS-16: Backward-Compat Shim — **S**

Code marked `// Retained for backward compatibility`, unused typedef aliases, re-exported types, `_var` renaming of unused parameters. Delete completely — if unused, it should not exist.

## Workflow

### 1. Scan

Run detection patterns on target files. Categorize by pattern ID, SOLID principle, and severity.

### 2. Plan Batches

Group changes by cohesion:
- **Batch 1**: Type extractions (new `F`-prefix structs, `E`-prefix enums)
- **Batch 2**: File splits (move functions to new files in `Source/`)
- **Batch 3**: Caller updates (update includes, dispatch sites, and usage)

### 3. Execute One Batch

- Structural changes only — behavior must be identical
- Follow NAMING.md for all new types
- Follow CODING.md for memory ownership
- All new files go in `Source/` (flat directory, `UniPlan` prefix)
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
- **Never add backward-compat shims** — if unused, delete completely
- **Build after every batch** — compilation is the primary safety net
- **Preserve all callers** — every call site must still compile and behave identically
- **One pattern at a time** — don't mix RS-01 and RS-04 in the same batch
- **Extract, don't duplicate** — if a refactoring creates shared logic, put it in a dedicated utility header, not a file-scope `static` function
- **Verify watch mode** — after structural changes to shared types, confirm `uni-plan watch` still renders correctly
- **Verify CLI output** — after type changes, confirm `uni-plan validate --strict` still passes against a real repo

## Output Format

| ID | SOLID | Pattern | File | Severity | Count |
|----|-------|---------|------|----------|-------|
| RS-01 | S | God struct | path:line | HIGH | fields |
| RS-02 | O | If/else hell | path:line | MEDIUM | branches |

| Batch | Files Modified | Files Created | Build |
|-------|---------------|---------------|-------|
| 1 | 3 | 2 | pass |
