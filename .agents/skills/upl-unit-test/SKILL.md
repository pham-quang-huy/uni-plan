---
name: upl-unit-test
description: Build and run unit tests for uni-plan CLI commands and watch-mode behavior. Use this skill to verify command correctness, run the full test suite, add tests, or debug failures. Covers option parsing, query commands, mutations, semantic lifecycle, evidence, entity commands, watch panels, watch interaction state, and watch performance caches.
implicit_invocation: true
---

# UPL Unit Test

Build and run the uni-plan test suite, add new tests, or debug failures.

## Mandatory: Coverage Audit After Every Run

After building and running tests, you MUST perform a coverage audit. Use the
coverage audit agent only when the current runtime explicitly allows
subagents; otherwise perform the audit manually before reporting results.

Audit the surface that changed:

- CLI command changes: build the command inventory from
  `Source/UniPlanCommandCatalog.cpp` and `Source/UniPlanCommandHelp.cpp`, using
  `Source/UniPlanForwardDecls.h` only to map registered leaf commands to
  implementation runners.
- Watch-mode changes: audit `Source/UniPlanWatchApp.cpp`,
  `Source/UniPlanWatchInteraction.*`, `Source/UniPlanWatchPanels.*`,
  `Source/UniPlanWatchScroll.*`, and `Source/UniPlanWatchSnapshot.*` against
  `Test/UniPlanTestWatchPerformance.cpp`.

Many CLI leaves are tested through dispatch wrappers (`RunTopicCommand`,
`RunBundlePhaseCommand`, `RunBundleChangelogCommand`, etc.) rather than by
direct `Run*Command` calls; count those wrapper tests for the routed leaf they
exercise.

When subagents are explicitly allowed, delegate this bounded audit to an
explorer:

```text
Read .claude/agents/upl-agent-senior-tester.md for your full instructions.
Then execute the audit workflow: (1) Read Source/UniPlanCommandCatalog.cpp,
Source/UniPlanCommandHelp.cpp, and Source/UniPlanForwardDecls.h to build the
complete registered leaf-command inventory. (2) Read all
Test/UniPlanTest*.cpp files to build the test inventory, mapping wrapper calls
like RunTopicCommand({"status", ...}) to the routed leaf. (3) Produce a
coverage matrix with columns: Command, Type, Happy, Negative, Bundle,
Changelog, Gate Msg — marking Y or N for each. (4) Flag any N as a gap with
the test file and test name that should be added.
```

Do NOT report test results to the user until the coverage audit is complete and you have reviewed its matrix. If the matrix shows gaps, fix them before reporting.

## Quick Run

```bash
# macOS/Linux: configure, build, install, and run all tests
./build.sh --tests

# Windows PowerShell from a VS 18 Developer Command Prompt
.\build.ps1 -Tests

# Windows from a plain PowerShell: invoke VS 18 DevCmd inline first
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Tests'

# macOS/Linux manual configure with tests enabled
cmake --preset dev-tests

# macOS/Linux manual build and run all tests
cmake --build Build/CMake --parallel && ./Build/CMake/uni-plan-tests

# Windows manual configure, build, and run all tests from VS 18 DevCmd
cmake --preset dev-win-tests
cmake --build Build\CMakeWin --parallel
Build\CMakeWin\uni-plan-tests.exe

# Run specific test suite
./Build/CMake/uni-plan-tests --gtest_filter="OptionParsing.*"
./Build/CMake/uni-plan-tests --gtest_filter="FBundleTestFixture.PhaseStart*"
Build\CMakeWin\uni-plan-tests.exe --gtest_filter="OptionParsing.*"
Build\CMakeWin\uni-plan-tests.exe --gtest_filter="FBundleTestFixture.PhaseStart*"

# Run via CTest
cd Build/CMake && ctest --output-on-failure
cd Build\CMakeWin && ctest --output-on-failure
```

## Test Architecture

| Component | File | Dependencies |
|-----------|------|-------------|
| **Fixture** | `Test/UniPlanTestFixture.h/.cpp` | Temp dir, stdout/stderr capture, JSON parse |
| **Option parsing** | `Test/UniPlanTestOptionParsing.cpp` | Pure functions, no fixture |
| **Query commands** | `Test/UniPlanTestQuery.cpp` | SampleTopic fixture |
| **Runtime phase metrics** | `Test/UniPlanTestPhaseMetrics.cpp` | In-memory bundle metrics + phase metric command guards |
| **Raw mutations** | `Test/UniPlanTestMutation.cpp` | SampleTopic fixture |
| **Semantic lifecycle** | `Test/UniPlanTestSemantic.cpp` | Minimal fixtures via CreateMinimalFixture |
| **Evidence shortcuts** | `Test/UniPlanTestEvidence.cpp` | SampleTopic fixture |
| **Entity coverage** | `Test/UniPlanTestEntity.cpp` | SampleTopic fixture |
| **Content-hygiene validation** | `Test/UniPlanTestValidationContent.cpp` | Minimal fixtures, regex-pattern injection, 13 content-hygiene checks + `--strict` gate |
| **Watch mode** | `Test/UniPlanTestWatchPerformance.cpp` | Snapshot projection, panels, interaction state, scroll, wrapping, cache, viewport rendering |

## Coverage Requirements (MANDATORY)

**"All tests pass" is NOT the same as "all commands are covered."** After writing tests, verify this checklist for EVERY registered CLI leaf command in the command catalog/help registry. Do not mark a leaf missing just because there is no direct call to its implementation runner; wrapper-dispatch tests count when they route to that leaf.

### Per-command minimum coverage

| Command type | Required tests |
|---|---|
| **Option parser** | Happy path (fields populated) + missing required field (`EXPECT_THROW` UsageError) |
| **Query command** | Happy path (exit 0, JSON field names verified) + negative (missing topic OR out-of-range phase → exit 1) |
| **Raw mutation** | Happy path (exit 0, `ReloadBundle` verifies mutation, `mChangeLogs.size()` grew) + negative (out-of-range OR invalid value → exit 1) |
| **Semantic lifecycle** | Happy path (exit 0, mutation + timestamp + auto-cascade verified) + gate rejection (wrong status → exit 1, `mCapturedStderr` contains reason) + design gate if applicable |
| **Evidence command** | Happy path (entry appended, phase index correct) + bounds check (out-of-range → exit 1) |
| **Entity command** | Happy path (record appended, changelog appended) + invalid enum (exit 1) + bounds check (exit 1) |

### Watch-mode minimum coverage

When changing `uni-plan watch`, cover the behavior at the smallest stable
boundary:

| Watch surface | Required tests |
|---|---|
| **Snapshot projection** | Typed fields copied from `FTopicBundle` / `FPhaseRecord`; no raw `.Plan.json` parsing |
| **Panel rendering** | Title, empty state, line gutters, colors/backgrounds, scroll indicators, narrow-width wrapping |
| **Interaction state** | Key-equivalent operations for selection, side-pane toggles, scroll direction, and reset scope |
| **Scroll behavior** | One visual line at a time; edge-scroll only when selected row leaves the viewport |
| **Cache/virtualization** | Layout cache reuse on scroll/render-only passes; cache invalidation on generation/topic/phase/width changes |
| **Cross-platform drawing** | Screen draw helpers sanitize control glyphs and respect stencil clipping |

### Coverage audit checklist

Run this BEFORE declaring tests complete:

```
For EVERY registered CLI leaf command:
[ ] At least 1 TEST_F with exit code 0 (happy path)
[ ] At least 1 TEST_F with exit code 1 (error path)
[ ] For mutations: ReloadBundle verifies field changed on disk
[ ] For mutations: ReloadBundle verifies mChangeLogs.size() grew
[ ] For gates: mCapturedStderr contains expected error phrase
[ ] For queries: JSON field names match actual command output
```

For EVERY changed watch surface:

```
[ ] Public behavior is tested through a stable panel/helper boundary
[ ] Scroll offsets are tested for both forward and backward movement
[ ] Reset scope is tested so unrelated scroll state is preserved
[ ] Cache reuse and invalidation are tested when layout caching changes
[ ] Rendering tests assert visible text plus relevant color/background pixels
[ ] Windows and macOS/Linux behavior stays in shared C++ code paths
```

### Post-test report format

After writing or modifying tests, report a **coverage matrix** — not just a pass count:

```
| Command | Happy | Negative | Bundle | Changelog | Gate Msg |
|---------|-------|----------|--------|-----------|----------|
| topic start | Y | Y | Y | Y | N/A |
| phase start | Y | Y (x2) | Y | Y | Y |
...
```

For watch-mode changes, add a second matrix:

```
| Surface | Behavior | Render | Scroll | Cache | Reset |
|---------|----------|--------|--------|-------|-------|
| side pane interaction | Y | N/A | Y | N/A | Y |
| phase list | Y | Y | Y | Y | Y |
...
```

This is the deliverable. "92 tests, 0 failures" alone is insufficient.

## Build System

- Object library `uni-plan-lib` compiles all Source/*.cpp except Main.cpp
- Both `uni-plan` and `uni-plan-tests` link against it (no double compilation)
- Google Test fetched via CMake FetchContent (v1.15.2)
- Shared CMake presets use `Build/CMake` on macOS/Linux and
  `Build/CMakeWin` on Windows
- Option `UPLAN_TESTS=OFF` by default; use `./build.sh --tests`,
  `.\build.ps1 -Tests` from VS 18 DevCmd, or `cmake --preset dev-tests`
  to enable tests

## Fixture: FBundleTestFixture

Provides per-test isolation via temp directories.

```cpp
class FBundleTestFixture : public ::testing::Test
{
protected:
    fs::path mRepoRoot;          // temp dir with Docs/Plans/
    std::string mCapturedStdout; // after StopCapture()
    std::string mCapturedStderr;

    void CopyFixture(const std::string &InName);  // copy from Example/
    void CreateMinimalFixture(                      // build in memory
        const std::string &InTopicKey,
        UniPlan::ETopicStatus InStatus, int InPhaseCount,
        UniPlan::EExecutionStatus InPhaseStatus, bool InDesign);
    void StartCapture();                            // redirect cout/cerr
    void StopCapture();                             // restore + capture
    nlohmann::json ParseCapturedJSON();             // parse stdout
    bool ReloadBundle(const std::string &InKey,
                      UniPlan::FTopicBundle &Out);  // re-read from disk
};
```

## Adding a New Test

### For a new command

1. Decide which test file (by tier):
   - Query → `UniPlanTestQuery.cpp`
   - Raw mutation → `UniPlanTestMutation.cpp`
   - Semantic lifecycle → `UniPlanTestSemantic.cpp`
   - Evidence → `UniPlanTestEvidence.cpp`
   - Entity → `UniPlanTestEntity.cpp`

2. Add a `TEST_F(FBundleTestFixture, ...)` with this pattern:

```cpp
TEST_F(FBundleTestFixture, CommandNameHappyPath)
{
    CopyFixture("SampleTopic");  // or CreateMinimalFixture(...)
    StartCapture();
    const int Code = UniPlan::RunCommandName(
        {"--topic", "SampleTopic", "--phase", "0",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json["ok"].get<bool>());

    // Verify file mutation persisted
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::InProgress);
}
```

3. Build and run: `./build.sh --tests`

### For a new option parser

Add a `TEST(OptionParsing, ...)` in `UniPlanTestOptionParsing.cpp`:

```cpp
TEST(OptionParsing, NewParserRequiresTopic)
{
    EXPECT_THROW(UniPlan::ParseNewOptions({}), UniPlan::UsageError);
}
```

## Key Conventions

- **Never `using namespace`** — fully qualify `UniPlan::` (hook enforced)
- **Always pass `--repo-root`** in test args — `ConsumeCommonOptions` defaults to cwd
- **Use `CreateMinimalFixture`** for gate tests — avoids coupling to SampleTopic structure
- **Use `CopyFixture("SampleTopic")`** for data-rich tests (jobs, lanes, tasks, etc.)
- **Timestamps are non-deterministic** — use `AssertISOTimestamp()` / `AssertISODate()` helpers
- **Stderr is captured but not asserted structurally** — only check for key error phrases

## Fixture File

`Example/Docs/Plans/SampleTopic.Plan.json` — complete V4 bundle with:
- 3 phases: completed (0), in_progress (1), not_started (2)
- Jobs with tasks, lanes, testing records, file manifest
- 4 changelog entries, 2 verification entries
- Full metadata (title, summary, goals, risks, acceptance criteria)

## Troubleshooting

| Issue | Fix |
|-------|-----|
| `gtest/gtest.h not found` | Run `cmake --preset dev-tests` or the script test mode |
| Fixture not found | Verify `Example/Docs/Plans/SampleTopic.Plan.json` exists |
| Schema not found in validate | Post-build copies Schemas/ — rebuild the test target |
| Static `bPrinted` noise in stderr | Expected — `NormalizeRepoRootPath` prints once per process |
| Linker duplicate library warning | Harmless — GTest main links gtest twice |
