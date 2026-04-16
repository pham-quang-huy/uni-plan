---
name: upl-unit-test
description: Build and run unit tests for all uni-plan CLI commands. Use this skill to verify command correctness, run the full test suite, add new tests, or debug test failures. Covers option parsing, query commands, mutations, semantic lifecycle, evidence, and entity commands.
implicit_invocation: true
---

# UPL Unit Test

Build and run the uni-plan test suite, add new tests, or debug failures.

## Mandatory: Coverage Audit After Every Run

After building and running tests, you MUST spawn a coverage audit agent. This is not optional.

```
Agent({
  description: "Audit test coverage",
  subagent_type: "Explore",
  prompt: "Read .claude/agents/upl-agent-senior-tester.md for your full instructions. Then execute the audit workflow: (1) Read Source/UniPlanForwardDecls.h to build the complete Run*Command inventory. (2) Read all Test/UniPlanTest*.cpp files to build the test inventory. (3) Produce a coverage matrix with columns: Command, Type, Happy, Negative, Bundle, Changelog, Gate Msg — marking Y or N for each. (4) Flag any N as a gap with the test file and test name that should be added."
})
```

Do NOT report test results to the user until the agent has completed and you have reviewed its coverage matrix. If the matrix shows gaps, fix them before reporting.

## Quick Run

```bash
# Configure with tests enabled (first time only, or after CMakeLists.txt changes)
cmake -S . -B Build/CMake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUPLAN_TESTS=ON -DUPLAN_WATCH=ON

# Build and run all tests
cmake --build Build/CMake -j "$(sysctl -n hw.logicalcpu)" && ./Build/CMake/uni-plan-tests

# Run specific test suite
./Build/CMake/uni-plan-tests --gtest_filter="OptionParsing.*"
./Build/CMake/uni-plan-tests --gtest_filter="FBundleTestFixture.PhaseStart*"

# Run via CTest
cd Build/CMake && ctest --output-on-failure
```

## Test Architecture

| Component | File | Dependencies |
|-----------|------|-------------|
| **Fixture** | `Test/UniPlanTestFixture.h/.cpp` | Temp dir, stdout/stderr capture, JSON parse |
| **Option parsing** | `Test/UniPlanTestOptionParsing.cpp` | Pure functions, no fixture |
| **Query commands** | `Test/UniPlanTestQuery.cpp` | SampleTopic fixture |
| **Raw mutations** | `Test/UniPlanTestMutation.cpp` | SampleTopic fixture |
| **Semantic lifecycle** | `Test/UniPlanTestSemantic.cpp` | Minimal fixtures via CreateMinimalFixture |
| **Evidence shortcuts** | `Test/UniPlanTestEvidence.cpp` | SampleTopic fixture |
| **Entity coverage** | `Test/UniPlanTestEntity.cpp` | SampleTopic fixture |

## Coverage Requirements (MANDATORY)

**"All tests pass" is NOT the same as "all commands are covered."** After writing tests, verify this checklist for EVERY `Run*Command` in `Source/UniPlanForwardDecls.h`:

### Per-command minimum coverage

| Command type | Required tests |
|---|---|
| **Option parser** | Happy path (fields populated) + missing required field (`EXPECT_THROW` UsageError) |
| **Query command** | Happy path (exit 0, JSON field names verified) + negative (missing topic OR out-of-range phase → exit 1) |
| **Raw mutation** | Happy path (exit 0, `ReloadBundle` verifies mutation, `mChangeLogs.size()` grew) + negative (out-of-range OR invalid value → exit 1) |
| **Semantic lifecycle** | Happy path (exit 0, mutation + timestamp + auto-cascade verified) + gate rejection (wrong status → exit 1, `mCapturedStderr` contains reason) + design gate if applicable |
| **Evidence command** | Happy path (entry appended, phase index correct) + bounds check (out-of-range → exit 1) |
| **Entity command** | Happy path (record appended, changelog appended) + invalid enum (exit 1) + bounds check (exit 1) |

### Coverage audit checklist

Run this BEFORE declaring tests complete:

```
For EVERY Run*Command in UniPlanForwardDecls.h:
[ ] At least 1 TEST_F with exit code 0 (happy path)
[ ] At least 1 TEST_F with exit code 1 (error path)
[ ] For mutations: ReloadBundle verifies field changed on disk
[ ] For mutations: ReloadBundle verifies mChangeLogs.size() grew
[ ] For gates: mCapturedStderr contains expected error phrase
[ ] For queries: JSON field names match actual command output
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

This is the deliverable. "92 tests, 0 failures" alone is insufficient.

## Build System

- Object library `uni-plan-lib` compiles all Source/*.cpp except Main.cpp
- Both `uni-plan` and `uni-plan-tests` link against it (no double compilation)
- Google Test fetched via CMake FetchContent (v1.15.2)
- Option `UPLAN_TESTS=OFF` by default — `build.sh` is unaffected

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

3. Build and run: `cmake --build Build/CMake && ./Build/CMake/uni-plan-tests`

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
| `gtest/gtest.h not found` | Run cmake configure with `-DUPLAN_TESTS=ON` |
| Fixture not found | Verify `Example/Docs/Plans/SampleTopic.Plan.json` exists |
| Schema not found in validate | Post-build copies Schemas/ — rebuild the test target |
| Static `bPrinted` noise in stderr | Expected — `NormalizeRepoRootPath` prints once per process |
| Linker duplicate library warning | Harmless — GTest main links gtest twice |
