When writing or modifying C++ code in this repository, enforce these naming conventions from NAMING.md:

1. **TYPE PREFIXES**:
   - `F` — Structs (value types): `FDocWatchSnapshot`, `FWatchPlanSummary`
   - `E` — Enums (enum class): `EDocType`, `EPhaseStatus`
   - Classes have no prefix: `InventoryPanel`, `ValidationPanel`

2. **MEMBER PREFIXES**:
   - `m` — Members: `mPlanCount`, `mTopicKey`
   - `mb` — Boolean members: `mbOk`, `mbRequired`, `mbPresent`
   - `k` — Constants: `kCliVersion`, `kColorReset`

3. **PARAMETER PREFIXES**:
   - `In` — Input parameters: `InRepoRoot`, `InPlan`, `InSelectedIndex`
   - `Out` — Output parameters: `OutLines`, `OutError`

4. **POINTER PREFIXES**:
   - `rp` — Raw pointer (non-owning): `rpSidecar`
   - `up` — Unique pointer: `upParser`
   - `sp` — Shared pointer: `spConfig`
   - `op` — Optional: `opContext`

5. **FUNCTION NAMING**: PascalCase — `BuildWatchSnapshot()`, `ToLower()`
6. **LOCAL VARIABLES**: PascalCase — `Stream`, `Index`, `Value`
7. **ACRONYMS**: Always ALL CAPS regardless of position — `ID`, `JSON`, `URL`, `UTC`
   - Correct: `mFrameID`, `GetCycleID()`, `InCycleID`
   - Wrong: `mFrameId`, `GetCycleId()`, `InCycleId`
8. **NAMESPACE**: Root `UniPlan::`, never `using namespace`
