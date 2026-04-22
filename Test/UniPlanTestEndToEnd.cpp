// End-to-end lifecycle fixtures (v0.104.0).
//
// Each test exercises the full command chain from `topic add` through
// `phase complete`, verifying the new v0.99–v0.102 gates and commands
// compose correctly:
//   - `topic add`                          (v0.94.0)
//   - `phase add`                          (v0.93.0)
//   - `phase set`                          (design material + optional
//                                           no-file-manifest opt-out)
//   - `lane add` / `job add` / `task add`  (v0.93.0)
//   - `testing add`                        (human + ai coverage)
//   - `manifest add`                       (code-bearing only)
//   - `phase start`                        (gates on design, not_started)
//   - `task set --status completed`        (leaf-level)
//   - `phase sync-execution --dry-run`     (v0.102.0)
//   - `phase sync-execution`               (rolls up jobs + lanes)
//   - `phase complete`                     (v0.101.0 descendant gate)
//   - `validate`                           (corpus clean afterward)
//
// These tests are regression guards for the whole command chain, not
// for any single command in isolation — those live in
// UniPlanTestMutation / UniPlanTestSemantic / UniPlanTestEntity /
// UniPlanTestValidationContent / UniPlanTestBundleWriteGuard.

#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanJSONIO.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <fstream>
#include <string>

#include <gtest/gtest.h>

namespace
{

// Helper: write a temp file with the given content and return the path.
std::string WriteTempFile(const std::string &InFilename,
                          const std::string &InContent)
{
    const fs::path Dir = fs::temp_directory_path() / "uni-plan-e2e-scratch";
    fs::create_directories(Dir);
    const fs::path Path = Dir / InFilename;
    std::ofstream Out(Path, std::ios::binary);
    Out << InContent;
    Out.close();
    return Path.string();
}

// Helper: count validate issues for a given topic at a given severity.
// When InSeverity is empty, counts all severities.
int CountIssues(const nlohmann::json &InJson, const std::string &InTopic,
                const std::string &InSeverity = "")
{
    int Count = 0;
    for (const auto &Issue : InJson["issues"])
    {
        if (Issue["topic"].get<std::string>() != InTopic)
            continue;
        if (!InSeverity.empty() &&
            Issue["severity"].get<std::string>() != InSeverity)
            continue;
        ++Count;
    }
    return Count;
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Doc-only phase lifecycle — no code, no file_manifest, full lifecycle.
// Exercises: topic add → phase add → design populate → taxonomy → start →
// bulk task close → sync-execution → phase complete → validate clean.
// ---------------------------------------------------------------------------
TEST_F(FBundleTestFixture, EndToEndDocOnlyPhaseLifecycle)
{
    const std::string Topic = "EndToEndDoc";

    // --- topic add ---------------------------------------------------------
    StartCapture();
    int Code = UniPlan::RunTopicAddCommand(
        {"--topic", Topic, "--title", "E2E doc-only lifecycle", "--summary",
         "Exercises the full doc-phase chain.", "--goals",
         "Verify the full command chain composes cleanly.", "--non-goals",
         "Nothing that touches code.", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    // --- phase add ---------------------------------------------------------
    StartCapture();
    Code = UniPlan::RunPhaseAddCommand(
        {"--topic", Topic, "--scope", "Author a doc-only phase", "--output",
         "Phase 0 complete with no file manifest", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    // --- design material (investigation) -----------------------------------
    const std::string InvPath = WriteTempFile(
        Topic + "-inv.txt",
        "Research note: this is a doc-only phase; no code changes.\n"
        "Verified via `uni-plan phase get --design` that the phase can "
        "start.\n");
    StartCapture();
    Code = UniPlan::RunPhaseSetCommand({"--topic", Topic, "--phase", "0",
                                        "--investigation-file", InvPath,
                                        "--repo-root", mRepoRoot.string()},
                                       mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    // --- declare no-code stance --------------------------------------------
    StartCapture();
    Code = UniPlan::RunPhaseSetCommand(
        {"--topic", Topic, "--phase", "0", "--no-file-manifest", "true",
         "--no-file-manifest-reason",
         "Doc-only phase: no production code touched.", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    // --- lane add ----------------------------------------------------------
    StartCapture();
    Code = UniPlan::RunLaneAddCommand(
        {"--topic", Topic, "--phase", "0", "--scope", "Doc writing lane",
         "--exit-criteria", "All doc tasks completed", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    // --- job add -----------------------------------------------------------
    StartCapture();
    Code = UniPlan::RunJobAddCommand(
        {"--topic", Topic, "--phase", "0", "--wave", "1", "--lane", "0",
         "--scope", "Draft and review doc set", "--output",
         "Published doc bundle", "--exit-criteria", "Reviewer approves",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    // --- task add (two tasks under the one job) ----------------------------
    for (int I = 0; I < 2; ++I)
    {
        const std::string Desc =
            "Draft section " + std::to_string(I + 1) + " of the doc";
        StartCapture();
        Code = UniPlan::RunTaskAddCommand({"--topic", Topic, "--phase", "0",
                                           "--job", "0", "--description", Desc,
                                           "--repo-root", mRepoRoot.string()},
                                          mRepoRoot.string());
        StopCapture();
        ASSERT_EQ(Code, 0) << mCapturedStderr;
    }

    // --- testing rows (human + ai for actor coverage) ---------------------
    StartCapture();
    Code = UniPlan::RunTestingAddCommand(
        {"--topic", Topic, "--phase", "0", "--session", "1", "--actor", "human",
         "--step", "Proofread each section", "--action",
         "Human reviewer reads the doc end-to-end", "--expected",
         "No factual errors", "--evidence", "Reviewer sign-off recorded",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    StartCapture();
    Code = UniPlan::RunTestingAddCommand(
        {"--topic", Topic, "--phase", "0", "--session", "1", "--actor", "ai",
         "--step", "Automated lint", "--action",
         "uni-plan validate --topic <T>", "--expected", "No ErrorMajor issues",
         "--evidence", "Validate passes with clean summary", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    // --- phase start -------------------------------------------------------
    StartCapture();
    Code = UniPlan::RunPhaseStartCommand(
        {"--topic", Topic, "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    // --- bulk leaf completion ---------------------------------------------
    for (int T = 0; T < 2; ++T)
    {
        StartCapture();
        Code = UniPlan::RunTaskSetCommand(
            {"--topic", Topic, "--phase", "0", "--job", "0", "--task",
             std::to_string(T), "--status", "completed", "--evidence",
             "Section drafted + reviewed", "--repo-root", mRepoRoot.string()},
            mRepoRoot.string());
        StopCapture();
        ASSERT_EQ(Code, 0) << mCapturedStderr;
    }

    // --- sync-execution --dry-run should report 2 flips without writing ---
    const std::string BeforeBytes = ReadBundleFile(Topic);
    StartCapture();
    Code = UniPlan::RunPhaseSyncExecutionCommand(
        {"--topic", Topic, "--phase", "0", "--dry-run", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;
    const auto DryJson = ParseCapturedJSON();
    EXPECT_TRUE(DryJson["dry_run"].get<bool>());
    EXPECT_EQ(DryJson["summary"]["jobs_flipped"].get<int>(), 1);
    EXPECT_EQ(DryJson["summary"]["lanes_flipped"].get<int>(), 1);
    EXPECT_EQ(ReadBundleFile(Topic), BeforeBytes)
        << "--dry-run must leave on-disk bytes byte-identical";

    // --- sync-execution (real) --------------------------------------------
    StartCapture();
    Code = UniPlan::RunPhaseSyncExecutionCommand(
        {"--topic", Topic, "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    UniPlan::FTopicBundle AfterSync;
    ASSERT_TRUE(ReloadBundle(Topic, AfterSync));
    EXPECT_EQ(AfterSync.mPhases[0].mJobs[0].mStatus,
              UniPlan::EExecutionStatus::Completed);
    EXPECT_EQ(AfterSync.mPhases[0].mLanes[0].mStatus,
              UniPlan::EExecutionStatus::Completed);
    // Phase is NEVER flipped by sync-execution.
    EXPECT_EQ(AfterSync.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::InProgress);

    // --- phase complete (v0.101.0 descendant gate must pass now) ----------
    StartCapture();
    Code = UniPlan::RunPhaseCompleteCommand(
        {"--topic", Topic, "--phase", "0", "--done",
         "Doc phase delivered; reviewer approved.", "--verification",
         "All doc sections present", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    UniPlan::FTopicBundle Final;
    ASSERT_TRUE(ReloadBundle(Topic, Final));
    EXPECT_EQ(Final.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::Completed);
    EXPECT_EQ(Final.mStatus, UniPlan::ETopicStatus::Completed)
        << "Topic auto-cascade should have closed the topic since every "
           "phase is now terminal AND at least one is Completed.";

    // --- validate: no ErrorMajor for this topic ---------------------------
    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", Topic, "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto ValidateJson = ParseCapturedJSON();
    EXPECT_EQ(CountIssues(ValidateJson, Topic, "error_major"), 0)
        << "End-to-end doc-only lifecycle produced ErrorMajor findings: "
        << ValidateJson.dump();
}

// ---------------------------------------------------------------------------
// 2. Code-bearing phase lifecycle — code_entity_contract populated,
// file_manifest entries required, full lifecycle.
// ---------------------------------------------------------------------------
TEST_F(FBundleTestFixture, EndToEndCodeBearingPhaseLifecycle)
{
    const std::string Topic = "EndToEndCode";

    // topic + phase
    StartCapture();
    ASSERT_EQ(
        UniPlan::RunTopicAddCommand(
            {"--topic", Topic, "--title", "E2E code-bearing lifecycle",
             "--summary", "Exercises code-bearing full chain.", "--goals",
             "Ship a code feature end-to-end.", "--non-goals",
             "Documentation-only work.", "--repo-root", mRepoRoot.string()},
            mRepoRoot.string()),
        0)
        << mCapturedStderr;
    StopCapture();
    StartCapture();
    ASSERT_EQ(UniPlan::RunPhaseAddCommand(
                  {"--topic", Topic, "--scope", "Implement the foo feature",
                   "--output", "Foo module shipping with tests", "--repo-root",
                   mRepoRoot.string()},
                  mRepoRoot.string()),
              0)
        << mCapturedStderr;
    StopCapture();

    // Populate design — code_entity_contract makes this a code-bearing phase,
    // which triggers the v0.88.0 file_manifest gate on phase complete.
    const std::string ContractPath =
        WriteTempFile(Topic + "-contract.txt",
                      "Kind: struct | Name: FFoo | Target: Source/Foo.h\n"
                      "Responsibility: Value-typed foo domain record.\n");
    const std::string SnippetsPath = WriteTempFile(
        Topic + "-snippets.txt",
        "Before:\n"
        "  no foo type existed; callers used std::map<std::string, int>.\n"
        "After:\n"
        "  FFoo value type with typed fields (mBar, mBaz).\n");
    const std::string InvPath = WriteTempFile(
        Topic + "-inv.txt",
        "Research: the existing map-based foo state is weak. A value-type\n"
        "struct closes the domain-model gap per CODING.md S3.\n");
    StartCapture();
    ASSERT_EQ(UniPlan::RunPhaseSetCommand(
                  {"--topic", Topic, "--phase", "0", "--investigation-file",
                   InvPath, "--code-entity-contract-file", ContractPath,
                   "--code-snippets-file", SnippetsPath, "--repo-root",
                   mRepoRoot.string()},
                  mRepoRoot.string()),
              0)
        << mCapturedStderr;
    StopCapture();

    // Taxonomy (lane + job + tasks)
    StartCapture();
    ASSERT_EQ(UniPlan::RunLaneAddCommand(
                  {"--topic", Topic, "--phase", "0", "--scope",
                   "Implementation lane", "--exit-criteria",
                   "Code + tests land", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0)
        << mCapturedStderr;
    StopCapture();
    StartCapture();
    ASSERT_EQ(UniPlan::RunJobAddCommand(
                  {"--topic", Topic, "--phase", "0", "--wave", "1", "--lane",
                   "0", "--scope", "Build the FFoo type", "--output",
                   "FFoo struct + tests in Source/Foo.h", "--exit-criteria",
                   "Tests pass", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0)
        << mCapturedStderr;
    StopCapture();
    for (int I = 0; I < 2; ++I)
    {
        const std::string Desc = "Implement FFoo step " + std::to_string(I + 1);
        StartCapture();
        ASSERT_EQ(
            UniPlan::RunTaskAddCommand({"--topic", Topic, "--phase", "0",
                                        "--job", "0", "--description", Desc,
                                        "--repo-root", mRepoRoot.string()},
                                       mRepoRoot.string()),
            0)
            << mCapturedStderr;
        StopCapture();
    }

    // Testing rows (human + ai)
    StartCapture();
    ASSERT_EQ(UniPlan::RunTestingAddCommand(
                  {"--topic", Topic, "--phase", "0", "--session", "1",
                   "--actor", "human", "--step", "Manual verify", "--action",
                   "Read the header and confirm signatures", "--expected",
                   "FFoo compiles", "--evidence", "Local build green",
                   "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0)
        << mCapturedStderr;
    StopCapture();
    StartCapture();
    ASSERT_EQ(UniPlan::RunTestingAddCommand(
                  {"--topic", Topic, "--phase", "0", "--session", "1",
                   "--actor", "ai", "--step", "Automated test", "--action",
                   "ctest --output-on-failure", "--expected", "All pass",
                   "--evidence", "CI green", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0)
        << mCapturedStderr;
    StopCapture();

    // file_manifest entry (required for code-bearing phase complete).
    StartCapture();
    ASSERT_EQ(UniPlan::RunManifestAddCommand(
                  {"--topic", Topic, "--phase", "0", "--file", "Source/Foo.h",
                   "--action", "create", "--description", "New FFoo type",
                   "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0)
        << mCapturedStderr;
    StopCapture();

    // Start, complete leaves, sync, complete.
    StartCapture();
    ASSERT_EQ(UniPlan::RunPhaseStartCommand({"--topic", Topic, "--phase", "0",
                                             "--repo-root", mRepoRoot.string()},
                                            mRepoRoot.string()),
              0)
        << mCapturedStderr;
    StopCapture();
    for (int T = 0; T < 2; ++T)
    {
        StartCapture();
        ASSERT_EQ(
            UniPlan::RunTaskSetCommand(
                {"--topic", Topic, "--phase", "0", "--job", "0", "--task",
                 std::to_string(T), "--status", "completed", "--evidence",
                 "Implemented + covered", "--repo-root", mRepoRoot.string()},
                mRepoRoot.string()),
            0)
            << mCapturedStderr;
        StopCapture();
    }
    StartCapture();
    ASSERT_EQ(UniPlan::RunPhaseSyncExecutionCommand(
                  {"--topic", Topic, "--phase", "0", "--repo-root",
                   mRepoRoot.string()},
                  mRepoRoot.string()),
              0)
        << mCapturedStderr;
    StopCapture();
    StartCapture();
    ASSERT_EQ(UniPlan::RunPhaseCompleteCommand(
                  {"--topic", Topic, "--phase", "0", "--done",
                   "FFoo shipped with tests", "--verification",
                   "All tests green", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0)
        << mCapturedStderr;
    StopCapture();

    UniPlan::FTopicBundle Final;
    ASSERT_TRUE(ReloadBundle(Topic, Final));
    EXPECT_EQ(Final.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::Completed);
    EXPECT_EQ(Final.mPhases[0].mJobs[0].mStatus,
              UniPlan::EExecutionStatus::Completed);
    EXPECT_EQ(Final.mPhases[0].mLanes[0].mStatus,
              UniPlan::EExecutionStatus::Completed);
    EXPECT_FALSE(Final.mPhases[0].mFileManifest.empty())
        << "code-bearing phase must carry manifest entries";

    // Validate: no ErrorMajor findings.
    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", Topic, "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto ValidateJson = ParseCapturedJSON();
    EXPECT_EQ(CountIssues(ValidateJson, Topic, "error_major"), 0)
        << "End-to-end code-bearing lifecycle produced ErrorMajor findings: "
        << ValidateJson.dump();
}

// ---------------------------------------------------------------------------
// 3. Shell-hostile content via JSON-file setter — prove the v0.100.0
// path survives the full lifecycle. A validation command containing a
// literal `|` would be mangled through the pipe grammar.
// ---------------------------------------------------------------------------
TEST_F(FBundleTestFixture, EndToEndShellHostileValidationCommandViaJsonFile)
{
    const std::string Topic = "EndToEndShellHostile";
    const std::string HazardousCommand = "grep foo build.log | wc -l";

    // topic + phase
    StartCapture();
    ASSERT_EQ(UniPlan::RunTopicAddCommand(
                  {"--topic", Topic, "--title",
                   "E2E shell-hostile validation command", "--summary",
                   "Proves JSON-file setters round-trip `|` safely.", "--goals",
                   "Carry literal pipe through the whole chain.", "--non-goals",
                   "Test any other metacharacter.", "--repo-root",
                   mRepoRoot.string()},
                  mRepoRoot.string()),
              0)
        << mCapturedStderr;
    StopCapture();
    StartCapture();
    ASSERT_EQ(UniPlan::RunPhaseAddCommand(
                  {"--topic", Topic, "--scope", "Exercise validation command",
                   "--output", "Phase with inert JSON validation command",
                   "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0)
        << mCapturedStderr;
    StopCapture();

    // Write the JSON-file with a command containing a literal `|`.
    const std::string JsonBody =
        std::string("[{\"platform\":\"any\",\"command\":\"") +
        HazardousCommand +
        "\",\"description\":\"Count foo lines in build log.\"}]";
    const std::string JsonPath = WriteTempFile(Topic + "-vc.json", JsonBody);

    // Populate design + validation commands via JSON-file form. The pipe
    // grammar would split `grep foo build.log | wc -l` into two fields.
    const std::string InvPath = WriteTempFile(
        Topic + "-inv.txt",
        "This phase carries a bash-pipeline validation command to prove\n"
        "the JSON-file setter preserves literal `|`.\n");
    StartCapture();
    ASSERT_EQ(
        UniPlan::RunPhaseSetCommand(
            {"--topic", Topic, "--phase", "0", "--investigation-file", InvPath,
             "--validation-commands-json-file", JsonPath, "--no-file-manifest",
             "true", "--no-file-manifest-reason", "Doc-only governance phase.",
             "--repo-root", mRepoRoot.string()},
            mRepoRoot.string()),
        0)
        << mCapturedStderr;
    StopCapture();

    // Verify byte-identical round-trip BEFORE starting — the JSON-file
    // path must not mangle the `|`.
    UniPlan::FTopicBundle Mid;
    ASSERT_TRUE(ReloadBundle(Topic, Mid));
    ASSERT_EQ(Mid.mPhases[0].mDesign.mValidationCommands.size(), 1u);
    EXPECT_EQ(Mid.mPhases[0].mDesign.mValidationCommands[0].mCommand,
              HazardousCommand)
        << "Pipe character must survive the JSON-file path unchanged";

    // Minimal taxonomy: one lane, one job, one task.
    StartCapture();
    ASSERT_EQ(UniPlan::RunLaneAddCommand({"--topic", Topic, "--phase", "0",
                                          "--scope", "Validation lane",
                                          "--exit-criteria", "Validation runs",
                                          "--repo-root", mRepoRoot.string()},
                                         mRepoRoot.string()),
              0)
        << mCapturedStderr;
    StopCapture();
    StartCapture();
    ASSERT_EQ(UniPlan::RunJobAddCommand(
                  {"--topic", Topic, "--phase", "0", "--wave", "1", "--lane",
                   "0", "--scope", "Run the validation command", "--output",
                   "Validation log", "--exit-criteria", "Nonzero foo count",
                   "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0)
        << mCapturedStderr;
    StopCapture();
    StartCapture();
    {
        const std::string TaskDesc =
            "Execute the pipeline and record the count";
        ASSERT_EQ(
            UniPlan::RunTaskAddCommand({"--topic", Topic, "--phase", "0",
                                        "--job", "0", "--description", TaskDesc,
                                        "--repo-root", mRepoRoot.string()},
                                       mRepoRoot.string()),
            0)
            << mCapturedStderr;
        StopCapture();
        for (const char *Actor : {"human", "ai"})
        {
            StartCapture();
            ASSERT_EQ(UniPlan::RunTestingAddCommand(
                          {"--topic", Topic, "--phase", "0", "--session", "1",
                           "--actor", Actor, "--step", "Run validation",
                           "--action", "grep foo build.log | wc -l",
                           "--expected", "Nonzero integer", "--evidence",
                           "Ran manually, saw count 42", "--repo-root",
                           mRepoRoot.string()},
                          mRepoRoot.string()),
                      0)
                << mCapturedStderr;
            StopCapture();
        }

        // Start → complete task → sync → complete.
        StartCapture();
        ASSERT_EQ(
            UniPlan::RunPhaseStartCommand({"--topic", Topic, "--phase", "0",
                                           "--repo-root", mRepoRoot.string()},
                                          mRepoRoot.string()),
            0)
            << mCapturedStderr;
        StopCapture();
        StartCapture();
        ASSERT_EQ(UniPlan::RunTaskSetCommand(
                      {"--topic", Topic, "--phase", "0", "--job", "0", "--task",
                       "0", "--status", "completed", "--evidence", "Count = 42",
                       "--repo-root", mRepoRoot.string()},
                      mRepoRoot.string()),
                  0)
            << mCapturedStderr;
        StopCapture();
        StartCapture();
        ASSERT_EQ(UniPlan::RunPhaseSyncExecutionCommand(
                      {"--topic", Topic, "--phase", "0", "--repo-root",
                       mRepoRoot.string()},
                      mRepoRoot.string()),
                  0)
            << mCapturedStderr;
        StopCapture();
        StartCapture();
        ASSERT_EQ(
            UniPlan::RunPhaseCompleteCommand(
                {"--topic", Topic, "--phase", "0", "--done",
                 "Validation ran; count recorded.", "--verification",
                 "Command output captured", "--repo-root", mRepoRoot.string()},
                mRepoRoot.string()),
            0)
            << mCapturedStderr;
        StopCapture();

        // Final verification: the literal `|` still survives after every
        // mutation downstream.
        UniPlan::FTopicBundle Final;
        ASSERT_TRUE(ReloadBundle(Topic, Final));
        ASSERT_EQ(Final.mPhases[0].mDesign.mValidationCommands.size(), 1u);
        EXPECT_EQ(Final.mPhases[0].mDesign.mValidationCommands[0].mCommand,
                  HazardousCommand)
            << "Pipe character must survive the entire lifecycle intact";
    }
}
