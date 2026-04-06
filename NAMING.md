# naming_conventions

When writing or modifying C++ code in this repository, enforce these naming conventions.

## type_prefixes

| Prefix | Kind | Example |
|--------|------|---------|
| `F` | Structs (value types) | `FDocWatchSnapshot`, `FWatchPlanSummary`, `FWatchPhaseTaxonomy` |
| `E` | Enum classes | `EDocType`, `EPhaseStatus` |
| (none) | Classes | `InventoryPanel`, `ValidationPanel` |

## member_prefixes

| Prefix | Kind | Example |
|--------|------|---------|
| `m` | Members | `mPlanCount`, `mTopicKey`, `mPhaseCount` |
| `mb` | Boolean members | `mbOk`, `mbRequired`, `mbPresent`, `mbCanonical` |
| `k` | Constants | `kCliVersion`, `kColorReset`, `kSidecarChangeLog` |

## parameter_prefixes

| Prefix | Kind | Example |
|--------|------|---------|
| `In` | Input parameters | `InRepoRoot`, `InPlan`, `InSelectedIndex` |
| `Out` | Output parameters | `OutLines`, `OutError`, `OutWarnings` |
| `InOut` | In/out parameters | `InOutWarnings` |

## pointer_prefixes

| Prefix | Kind | Example |
|--------|------|---------|
| `rp` | Raw pointer (non-owning) | `rpSidecar`, `rpDevice` |
| `up` | Unique pointer | `upParser` |
| `sp` | Shared pointer | `spConfig` |
| `op` | Optional | `opContext` |

## function_naming

- PascalCase: `BuildWatchSnapshot()`, `ToLower()`, `TryReadFileLines()`
- Prefixes for intent: `Try` (may fail), `Get` (accessor), `Build` (construction), `Is`/`Has` (boolean query)

## local_variables

- PascalCase: `Stream`, `Index`, `Value`, `Cells`, `Working`
- Short-lived loop counters may use lowercase: `i`, `j`

## acronyms

Always ALL CAPS regardless of position:

- `ID`, `JSON`, `URL`, `UTC`, `ANSI`, `CLI`, `TUI`
- Correct: `mFrameID`, `GetCycleID()`, `InCycleID`, `kSchemaSchema`
- Wrong: `mFrameId`, `GetCycleId()`, `InCycleId`

## namespace

- Root namespace: `UniPlan`
- Never `using namespace` — always fully qualified paths
- Exception: `namespace fs = std::filesystem` alias (in `UniPlanTypes.h` only)

## file_naming

- PascalCase with `UniPlan` prefix: `UniPlanValidation.cpp`, `UniPlanWatchPanels.h`
- Header + source pairs: `UniPlanFoo.h` + `UniPlanFoo.cpp`
