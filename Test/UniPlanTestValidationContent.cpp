#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanJSONIO.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <gtest/gtest.h>

// ===================================================================
// Content-hygiene validation tests
//
// Each new evaluator has:
//   * One "positive" test — inject offending content, expect the check
//     to fire with the documented severity.
//   * One "negative" test — clean bundle, expect no matching issue.
//
// Plus one wiring test for --strict gate behavior.
// ===================================================================

namespace
{

// Count issues with the given id in a validate response.
int CountIssuesWithId(const nlohmann::json &InJson, const std::string &InId)
{
    int Count = 0;
    for (const auto &Issue : InJson["issues"])
    {
        if (Issue["id"].get<std::string>() == InId)
            ++Count;
    }
    return Count;
}

// Return the first issue with the given id, or an empty object.
nlohmann::json FirstIssueWithId(const nlohmann::json &InJson,
                                const std::string &InId)
{
    for (const auto &Issue : InJson["issues"])
    {
        if (Issue["id"].get<std::string>() == InId)
            return Issue;
    }
    return nlohmann::json::object();
}

// Write Bundle back to the temp repo.
void WriteBundle(const fs::path &InRepoRoot, const std::string &InTopicKey,
                 const UniPlan::FTopicBundle &InBundle)
{
    const fs::path Path =
        InRepoRoot / "Docs" / "Plans" / (InTopicKey + ".Plan.json");
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(InBundle, Path, Error)) << Error;
}

// Build a single-entry validation_commands vector for tests that inject
// shell-string content into the typed field. Many content-hygiene
// checks scan the `command` string, so the one-argument overload is
// the common case.
UniPlan::FValidationCommand
MakeVC(const std::string &InCommand, const std::string &InDescription = "test",
       UniPlan::EPlatformScope InPlatform = UniPlan::EPlatformScope::Any)
{
    UniPlan::FValidationCommand C;
    C.mPlatform = InPlatform;
    C.mCommand = InCommand;
    C.mDescription = InDescription;
    return C;
}

} // namespace

// -------------------------------------------------------------------
// no_dev_absolute_path — dev-machine paths
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, NoDevAbsolutePathFlagsUsersPath)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mMetadata.mValidationCommands = {
        MakeVC("cd /Users/alice/code/project")};
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "no_dev_absolute_path");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "error_minor");
}

TEST_F(FBundleTestFixture, NoDevAbsolutePathCleanBundlePasses)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mMetadata.mValidationCommands = {MakeVC("cd ~/code/project")};
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_dev_absolute_path"), 0);
}

// -------------------------------------------------------------------
// no_hardcoded_endpoint — localhost / LAN IPs
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, NoHardcodedEndpointFlagsLocalhost)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mMetadata.mValidationCommands = {
        MakeVC("curl localhost:8080/health")};
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "no_hardcoded_endpoint");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
}

TEST_F(FBundleTestFixture, NoHardcodedEndpointCleanBundlePasses)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mMetadata.mValidationCommands = {MakeVC("curl $HEALTHCHECK_URL")};
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_hardcoded_endpoint"), 0);
}

// -------------------------------------------------------------------
// validation_command_platform_consistency — Windows `\` paths in a
// non-Windows command row (replaces the former platform_path_sep_free
// workaround). The typed FValidationCommand.mPlatform field is now the
// source of truth; backslash paths are only acceptable when mPlatform
// == Windows.
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture,
       ValidationCommandPlatformConsistencyFlagsMistaggedWindows)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // mPlatform == Any but command contains Windows-only backslash path.
    Bundle.mMetadata.mValidationCommands = {
        MakeVC("Build\\Windows-x64\\Debug\\test.exe", "builds the exe",
               UniPlan::EPlatformScope::Any)};
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue =
        FirstIssueWithId(Json, "validation_command_platform_consistency");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
}

TEST_F(FBundleTestFixture,
       ValidationCommandPlatformConsistencyAcceptsExplicitWindows)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Same command, but mPlatform == Windows — structurally correct.
    Bundle.mMetadata.mValidationCommands = {
        MakeVC("Build\\Windows-x64\\Debug\\test.exe", "builds the exe",
               UniPlan::EPlatformScope::Windows)};
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(
        CountIssuesWithId(Json, "validation_command_platform_consistency"), 0);
}

// -------------------------------------------------------------------
// no_smart_quotes — curly quotes/en-dashes
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, NoSmartQuotesFlagsCurlyQuotes)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // U+201C LEFT DOUBLE QUOTATION MARK, U+201D RIGHT DOUBLE QUOTATION MARK
    Bundle.mMetadata.mSummary = "Run \xE2\x80\x9Ctest\xE2\x80\x9D";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "no_smart_quotes");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
}

TEST_F(FBundleTestFixture, NoSmartQuotesCleanBundlePasses)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mMetadata.mSummary = "Run \"test\"";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_smart_quotes"), 0);
}

// -------------------------------------------------------------------
// no_html_in_prose — HTML tags
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, NoHtmlInProseFlagsBrTag)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mDesign.mCodeEntityContract = "line 1<br>line 2";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "no_html_in_prose");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
}

TEST_F(FBundleTestFixture, NoHtmlInProseCleanBundlePasses)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mDesign.mCodeEntityContract = "line 1\nline 2";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_html_in_prose"), 0);
}

// -------------------------------------------------------------------
// no_empty_placeholder_literal — "None"/"N/A"/"TBD" strings
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, NoEmptyPlaceholderLiteralFlagsNoneString)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mLifecycle.mBlockers = "None";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "no_empty_placeholder_literal");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
}

TEST_F(FBundleTestFixture, NoEmptyPlaceholderLiteralEmptyStringPasses)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mLifecycle.mBlockers = "";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_empty_placeholder_literal"), 0);
}

// -------------------------------------------------------------------
// no_unresolved_marker — TODO/FIXME/TBD/XXX markers
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, NoUnresolvedMarkerFlagsTodoInCompletedPhase)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::Completed, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mLifecycle.mDone = "Shipped. TODO: retest edge cases.";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "no_unresolved_marker");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
}

TEST_F(FBundleTestFixture, NoUnresolvedMarkerCleanBundlePasses)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::Completed, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mLifecycle.mDone = "Shipped. Retest complete.";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_unresolved_marker"), 0);
}

// -------------------------------------------------------------------
// no_duplicate_phase_field — byte-identical prescriptive field across phases
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, NoDuplicatePhaseFieldFlagsIdenticalHandoff)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    const std::string Stamp =
        "L0 | L1 | Phase-key binding proof.\nL1 | L2 | Synchronized bundle "
        "state across dependent phases with current status wording.";
    Bundle.mPhases[0].mDesign.mHandoff = Stamp;
    Bundle.mPhases[1].mDesign.mHandoff = Stamp;
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "no_duplicate_phase_field");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
    EXPECT_EQ(Issue["path"], "phases[1].handoff");
    EXPECT_NE(Issue["detail"].get<std::string>().find("phases[0].handoff"),
              std::string::npos);
}

TEST_F(FBundleTestFixture, NoDuplicatePhaseFieldIgnoresShortStubs)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mDesign.mHandoff = "N/A";
    Bundle.mPhases[1].mDesign.mHandoff = "N/A";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_duplicate_phase_field"), 0);
}

TEST_F(FBundleTestFixture, NoDuplicatePhaseFieldCleanBundlePasses)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mDesign.mHandoff =
        "Lane-by-lane handoff for phase zero baseline work and owners.";
    Bundle.mPhases[1].mDesign.mHandoff =
        "Lane-by-lane handoff for phase one execution and follow-up owners.";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_duplicate_phase_field"), 0);
}

// -------------------------------------------------------------------
// no_hollow_completed_phase — completed phase with no execution evidence
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, NoHollowCompletedPhaseFlagsEmptyCompleted)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::Completed, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mJobs.clear();
    Bundle.mPhases[0].mTesting.clear();
    Bundle.mPhases[0].mFileManifest.clear();
    Bundle.mPhases[0].mDesign.mCodeSnippets.clear();
    Bundle.mPhases[0].mDesign.mInvestigation.clear();
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "no_hollow_completed_phase");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
    EXPECT_EQ(Issue["path"], "phases[0]");
}

TEST_F(FBundleTestFixture, NoHollowCompletedPhaseIgnoresPhaseWithJobs)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::Completed, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mTesting.clear();
    Bundle.mPhases[0].mFileManifest.clear();
    Bundle.mPhases[0].mDesign.mCodeSnippets.clear();
    Bundle.mPhases[0].mDesign.mInvestigation.clear();
    UniPlan::FJobRecord Job;
    Job.mScope = "Real execution work";
    Job.mOutput = "Shipping code";
    Job.mExitCriteria = "Build passes";
    Bundle.mPhases[0].mJobs = {Job};
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_hollow_completed_phase"), 0);
}

TEST_F(FBundleTestFixture, NoHollowCompletedPhaseIgnoresNotStartedPhase)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::NotStarted, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mJobs.clear();
    Bundle.mPhases[0].mTesting.clear();
    Bundle.mPhases[0].mFileManifest.clear();
    Bundle.mPhases[0].mDesign.mCodeSnippets.clear();
    Bundle.mPhases[0].mDesign.mInvestigation.clear();
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_hollow_completed_phase"), 0);
}

// Regression guard for the v0.82.0 threshold tightening: a completed
// phase with trivial filler in a design field (e.g. "TBD" in
// code_snippets) used to pass under the old binary-emptiness check
// because `code_snippets` was non-empty. The chars-threshold form
// correctly flags it as hollow since total design chars < 4000.
TEST_F(FBundleTestFixture, NoHollowCompletedPhaseFlagsTrivialFillerDesign)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::Completed, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mJobs.clear();
    Bundle.mPhases[0].mTesting.clear();
    Bundle.mPhases[0].mFileManifest.clear();
    // Trivial filler in design — non-empty, but well below kPhaseHollowChars.
    Bundle.mPhases[0].mDesign.mCodeSnippets = "TBD";
    Bundle.mPhases[0].mDesign.mInvestigation.clear();
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "no_hollow_completed_phase");
    ASSERT_FALSE(Issue.empty())
        << "Trivial-filler completed phase should flag as hollow under the "
           "chars-threshold check";
    EXPECT_EQ(Issue["severity"], "warning");
    EXPECT_EQ(Issue["path"], "phases[0]");
}

// Conversely: a completed phase with substantial design prose
// (>= kPhaseHollowChars) but still no jobs/testing/manifest should pass,
// because the design prose alone demonstrates authored content.
TEST_F(FBundleTestFixture, NoHollowCompletedPhasePassesOnRichDesignOnly)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::Completed, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mJobs.clear();
    Bundle.mPhases[0].mTesting.clear();
    Bundle.mPhases[0].mFileManifest.clear();
    // Pad investigation so design_chars >= kPhaseHollowChars (4000).
    // scope/output already carry some content from the fixture; pad
    // investigation past the threshold to be safe.
    Bundle.mPhases[0].mDesign.mInvestigation = std::string(5000, 'x');
    Bundle.mPhases[0].mDesign.mCodeSnippets.clear();
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_hollow_completed_phase"), 0)
        << "Rich-design completed phase (>= 4000 design chars) should not "
           "flag as hollow even with empty jobs/testing/manifest";
}

// -------------------------------------------------------------------
// no_duplicate_lane_scope — clone-and-forget lane detection (v0.84.0)
// Conservative: exact-normalized match only. Lowercase + whitespace-
// collapse + edge-punctuation strip. Fires Warning on the later lane;
// detail names the earlier one.
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, NoDuplicateLaneScopeFlagsIdenticalLanes)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FLaneRecord L0;
    L0.mScope = "Restructure Main.cpp subcommand dispatch.";
    L0.mStatus = UniPlan::EExecutionStatus::Completed;
    UniPlan::FLaneRecord L1;
    L1.mScope = "Restructure Main.cpp subcommand dispatch.";
    L1.mStatus = UniPlan::EExecutionStatus::NotStarted;
    Bundle.mPhases[0].mLanes = {L0, L1};
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "no_duplicate_lane_scope");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
    EXPECT_EQ(Issue["path"].get<std::string>(), "phases[0].lanes[1]");
    EXPECT_NE(Issue["detail"].get<std::string>().find("phases[0].lanes[0]"),
              std::string::npos);
}

TEST_F(FBundleTestFixture, NoDuplicateLaneScopeNormalizesCaseAndWhitespace)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FLaneRecord L0;
    L0.mScope = "Implement FeatureQuery.h/.cpp and FeatureQueryOutput.h/.cpp.";
    UniPlan::FLaneRecord L1;
    // Same prose; different case, extra whitespace, trailing punct stripped.
    L1.mScope = "  IMPLEMENT   FeatureQuery.h/.cpp    and "
                "FeatureQueryOutput.h/.cpp!  ";
    Bundle.mPhases[0].mLanes = {L0, L1};
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_duplicate_lane_scope"), 1)
        << "case/whitespace/edge-punct should normalize to same scope";
}

TEST_F(FBundleTestFixture, NoDuplicateLaneScopePassesOnDistinctLanes)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FLaneRecord L0;
    L0.mScope = "Restructure Main.cpp subcommand dispatch.";
    UniPlan::FLaneRecord L1;
    L1.mScope = "Implement FeatureQuery.h/.cpp and FeatureQueryOutput.h/.cpp.";
    UniPlan::FLaneRecord L2;
    L2.mScope = "Rename CMake target to see. Create build_see.sh/.bat.";
    Bundle.mPhases[0].mLanes = {L0, L1, L2};
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_duplicate_lane_scope"), 0)
        << "semantically-related-but-textually-distinct lanes must pass";
}

// -------------------------------------------------------------------
// file_manifest_required_for_code_phases — authoring-discipline gap
// (v0.86.0). Fires Warning when a phase has populated
// code_entity_contract or code_snippets (the "code-bearing" predicate)
// but empty file_manifest, unless the explicit opt-out
// (no_file_manifest=true + reason) is set.
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, FileManifestRequiredFiresOnCodeBearingEmpty)
{
    // Minimal fixture with InPopulateDesign=true sets
    // code_entity_contract — code-bearing predicate true; manifest empty.
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(
        Json, "file_manifest_required_for_code_phases");
    ASSERT_FALSE(Issue.empty());
    // v0.87.0: severity promoted from Warning → ErrorMinor now that
    // `manifest suggest` (v0.86.0) provides the migration path.
    EXPECT_EQ(Issue["severity"], "error_minor");
    EXPECT_EQ(Issue["path"].get<std::string>(),
              "phases[0].file_manifest");
}

TEST_F(FBundleTestFixture, FileManifestRequiredSkippedOnNonCodeBearing)
{
    // No design fields populated → not code-bearing → no fire.
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, false);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(
                  Json, "file_manifest_required_for_code_phases"),
              0);
}

TEST_F(FBundleTestFixture, FileManifestRequiredSkippedWhenManifestPresent)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FFileManifestItem Item;
    Item.mFilePath = "Engine/Foo.cpp";
    Item.mAction = UniPlan::EFileAction::Modify;
    Item.mDescription = "covered";
    Bundle.mPhases[0].mFileManifest.push_back(std::move(Item));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(
                  Json, "file_manifest_required_for_code_phases"),
              0);
}

TEST_F(FBundleTestFixture, FileManifestRequiredSkippedByExplicitOptOut)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mbNoFileManifest = true;
    Bundle.mPhases[0].mFileManifestSkipReason =
        "Doc-only phase, no code touched";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(
                  Json, "file_manifest_required_for_code_phases"),
              0);
}

// v0.87.0: stale_mislabeled_modify smoke tests.
// The evaluator spawns `git log` against the repo root; the test
// fixture's mRepoRoot is a fresh tmpdir without git, so the evaluator
// silently no-ops. These tests cover the no-op contract; a real-corpus
// integration test against ~/code/fie validates the positive path.
TEST_F(FBundleTestFixture, StaleMislabeledModifySilentInNonGitRepo)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FFileManifestItem Item;
    Item.mFilePath = "Engine/Foo.cpp";
    Item.mAction = UniPlan::EFileAction::Modify;
    Item.mDescription = "covered";
    Bundle.mPhases[0].mFileManifest.push_back(std::move(Item));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "stale_mislabeled_modify"), 0)
        << "non-git repo must produce zero stale_mislabeled_modify issues";
}

TEST_F(FBundleTestFixture, StaleMislabeledModifySkippedWhenNoModifyEntries)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FFileManifestItem Item;
    Item.mFilePath = "Engine/Foo.cpp";
    Item.mAction = UniPlan::EFileAction::Create; // not modify, not in scope
    Item.mDescription = "covered";
    Bundle.mPhases[0].mFileManifest.push_back(std::move(Item));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "stale_mislabeled_modify"), 0);
}

// -------------------------------------------------------------------
// phase_status_lane_alignment — drift evaluator (v0.84.0)
// Mirrors the phase drift command but via validate --strict, so CI can
// gate on phase status vs lane-evidence disagreement.
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, PhaseStatusLaneAlignmentFlagsStatusLagLane)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FLaneRecord L;
    L.mStatus = UniPlan::EExecutionStatus::Completed;
    L.mScope = "Implemented";
    Bundle.mPhases[0].mLanes.push_back(L);
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "phase_status_lane_alignment");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
    EXPECT_NE(Issue["detail"].get<std::string>().find("status_lag_lane"),
              std::string::npos);
}

TEST_F(FBundleTestFixture, PhaseStatusLaneAlignmentPassesOnAlignedStatus)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Phase = in_progress, lane = in_progress → aligned.
    UniPlan::FLaneRecord L;
    L.mStatus = UniPlan::EExecutionStatus::InProgress;
    L.mScope = "WIP";
    Bundle.mPhases[0].mLanes.push_back(L);
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "phase_status_lane_alignment"), 0);
}

TEST_F(FBundleTestFixture, NoDuplicateLaneScopeIgnoresEmptyScopes)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Two empty-scope lanes must not flag — EvalLaneRequiredFields owns that.
    UniPlan::FLaneRecord L0;
    L0.mScope = "";
    UniPlan::FLaneRecord L1;
    L1.mScope = "";
    Bundle.mPhases[0].mLanes = {L0, L1};
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_duplicate_lane_scope"), 0)
        << "empty-scope duplicates belong to EvalLaneRequiredFields";
}

// -------------------------------------------------------------------
// topic_ref_integrity — `<X>.Plan.json` must resolve
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, TopicRefIntegrityFlagsUnknownTopic)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FBundleReference R;
    R.mKind = UniPlan::EDependencyKind::Bundle;
    R.mTopic = "SomeMissingTopic";
    Bundle.mMetadata.mDependencies = {R};
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "topic_ref_integrity");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "error_minor");
    EXPECT_NE(Issue["detail"].get<std::string>().find("SomeMissingTopic"),
              std::string::npos);
}

TEST_F(FBundleTestFixture, TopicRefIntegritySelfReferencePasses)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FBundleReference R;
    R.mKind = UniPlan::EDependencyKind::Bundle;
    R.mTopic = "T";
    Bundle.mMetadata.mDependencies = {R};
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "topic_ref_integrity"), 0);
}

// Regression guard (v0.73.3): validate --topic <T> must resolve cross-topic
// `kind=bundle` refs against the full repo-wide topic registry, not just
// the scoped bundle. Prior to v0.73.3, the validate command loaded only
// the scoped bundle, producing false-positive topic_ref_integrity errors
// for any kind=bundle/kind=phase reference to another real topic.
TEST_F(FBundleTestFixture, TopicRefIntegrityScopedRunResolvesCrossTopicRef)
{
    // Two real bundles in the repo: A and B.
    CreateMinimalFixture("A", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    CreateMinimalFixture("B", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);

    // A depends on B via kind=bundle.
    UniPlan::FTopicBundle BundleA;
    ASSERT_TRUE(ReloadBundle("A", BundleA));
    UniPlan::FBundleReference R;
    R.mKind = UniPlan::EDependencyKind::Bundle;
    R.mTopic = "B";
    BundleA.mMetadata.mDependencies = {R};
    WriteBundle(mRepoRoot, "A", BundleA);

    // Scoped run: only --topic A in the output, but the evaluator
    // registry must still see B so the kind=bundle ref resolves.
    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "A", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "topic_ref_integrity"), 0);
    // Output scope: bundle_count reports the scoped view (1), not the
    // full loaded set.
    EXPECT_EQ(Json["bundle_count"].get<int>(), 1);
    EXPECT_EQ(Json["summary"]["topic_count"].get<int>(), 1);
}

// Regression guard (v0.73.3): --topic <T> pointing at a missing topic
// emits a load_failure issue and exit 1, unchanged by the scoped-load
// refactor.
TEST_F(FBundleTestFixture, ValidateMissingTopicEmitsLoadFailure)
{
    CreateMinimalFixture("Real", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "Missing", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "load_failure");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "error_major");
    EXPECT_FALSE(Json["valid"].get<bool>());
}

// -------------------------------------------------------------------
// no_duplicate_changelog — same (phase, change) recorded twice
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, NoDuplicateChangelogFlagsRepeatedEntry)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FChangeLogEntry E;
    E.mDate = "2026-04-01";
    E.mChange = "Locked decision X";
    E.mPhase = -1;
    Bundle.mChangeLogs.push_back(E);
    Bundle.mChangeLogs.push_back(E);
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "no_duplicate_changelog");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
}

TEST_F(FBundleTestFixture, NoDuplicateChangelogUniqueEntriesPass)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FChangeLogEntry E1;
    E1.mDate = "2026-04-01";
    E1.mChange = "Entry A";
    E1.mPhase = -1;
    UniPlan::FChangeLogEntry E2 = E1;
    E2.mChange = "Entry B";
    Bundle.mChangeLogs.push_back(E1);
    Bundle.mChangeLogs.push_back(E2);
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_duplicate_changelog"), 0);
}

// -------------------------------------------------------------------
// --strict gate — Warning/ErrorMinor flip bValid and return 1
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, StrictPromotesWarningIntoValidGate)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Trigger a Warning-severity check.
    Bundle.mMetadata.mSummary = "\xE2\x80\x9Csmart quotes\xE2\x80\x9D";
    WriteBundle(mRepoRoot, "T", Bundle);

    // Without --strict — bValid=true, exit 0.
    StartCapture();
    int CodeA = UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(CodeA, 0);
    const auto JsonA = ParseCapturedJSON();
    EXPECT_TRUE(JsonA["valid"].get<bool>());
    EXPECT_GT(CountIssuesWithId(JsonA, "no_smart_quotes"), 0);

    // With --strict — bValid=false, exit 1.
    StartCapture();
    int CodeB = UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--strict", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(CodeB, 1);
    const auto JsonB = ParseCapturedJSON();
    EXPECT_FALSE(JsonB["valid"].get<bool>());
}

TEST_F(FBundleTestFixture, StrictOnCleanBundleStillExitsZero)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Populate everything a minimal completed fixture needs to avoid
    // unrelated warnings firing.
    Bundle.mPhases[0].mScope = "clean scope";
    Bundle.mMetadata.mSummary = "clean summary";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    int Code = UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--strict", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    // bValid must be true only if no warnings OR errors at all — assert
    // exit 0 implies valid=true.
    if (Code == 0)
        EXPECT_TRUE(Json["valid"].get<bool>());
}

// -------------------------------------------------------------------
// line field — issues carry a 1-based line number for the JSON path
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, IssueLineFieldResolvesKeyLine)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Inject a smart quote in summary → triggers no_smart_quotes at the
    // `summary` key line.
    Bundle.mMetadata.mSummary = "\xE2\x80\x9Ctest\xE2\x80\x9D";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "no_smart_quotes");
    ASSERT_FALSE(Issue.empty());
    // `line` field must be present and be a positive integer.
    ASSERT_TRUE(Issue.contains("line"));
    ASSERT_FALSE(Issue["line"].is_null());
    EXPECT_GT(Issue["line"].get<int>(), 0);
}

TEST_F(FBundleTestFixture, IssueLineFieldForPhaseKey)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Inject a smart quote in phases[1].scope — line must be distinct
    // from phases[0].scope.
    Bundle.mPhases[1].mScope = "scope with \xE2\x80\x9Cquotes\xE2\x80\x9D";
    Bundle.mPhases[0].mScope = "clean scope";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    // Find the no_smart_quotes issue that targets phases[1].scope.
    int Line = -1;
    for (const auto &Issue : Json["issues"])
    {
        if (Issue["id"] == "no_smart_quotes" &&
            Issue["path"].get<std::string>() == "phases[1].scope")
        {
            Line = Issue["line"].get<int>();
            break;
        }
    }
    EXPECT_GT(Line, 0);
}

// -------------------------------------------------------------------
// no_empty_placeholder_literal — status-word extension (v0.73.0)
// Catches migration column-shift where V3 Status column leaked into
// V4 scope/output/done. See UniPlanValidationContent.cpp EvalNoEmpty-
// PlaceholderLiteral comment for the rationale.
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, NoEmptyPlaceholderLiteralFlagsStatusWordInScope)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Classic column-shift defect: status-word landed in scope.
    Bundle.mPhases[0].mScope = "Completed";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    // Confirm the check fires on phases[0].scope specifically.
    bool bFoundScope = false;
    for (const auto &Issue : Json["issues"])
    {
        if (Issue["id"] == "no_empty_placeholder_literal" &&
            Issue["path"].get<std::string>() == "phases[0].scope")
        {
            EXPECT_EQ(Issue["severity"], "warning");
            bFoundScope = true;
            break;
        }
    }
    EXPECT_TRUE(bFoundScope);
}

TEST_F(FBundleTestFixture, NoEmptyPlaceholderLiteralAllowsProseMentioningDone)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Real prose that happens to contain status words must pass. The
    // check is anchored to whole-field matches, not substring matches.
    Bundle.mPhases[0].mScope = "Completed the foo and started the bar";
    Bundle.mPhases[0].mLifecycle.mDone = "done wiring X; in_progress on Y";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_empty_placeholder_literal"), 0);
}

// -------------------------------------------------------------------
// topic_phase_status_alignment — topic.status must match phase.status
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture,
       TopicPhaseStatusAlignmentFlagsCompletedTopicWithNotStartedPhase)
{
    // Topic says completed, but phases are all not_started — the
    // migration defect we saw on 30 topics in mm-factory.
    CreateMinimalFixture("T", UniPlan::ETopicStatus::Completed, 3,
                         UniPlan::EExecutionStatus::NotStarted, true);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "topic_phase_status_alignment");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
    EXPECT_EQ(Issue["path"], "status");
}

TEST_F(FBundleTestFixture, TopicPhaseStatusAlignmentPassesWhenAligned)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::Completed, 2,
                         UniPlan::EExecutionStatus::Completed, true);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "topic_phase_status_alignment"), 0);
}

// -------------------------------------------------------------------
// completed_phase_timestamp_required — completed phases must carry
// both started_at and completed_at
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, CompletedPhaseTimestampRequiredFlagsMissingStartedAt)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::Completed, 1,
                         UniPlan::EExecutionStatus::Completed, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Fixture pre-populates timestamps when Completed — blank them so
    // the check has something to fire on.
    Bundle.mPhases[0].mLifecycle.mStartedAt = "";
    Bundle.mPhases[0].mLifecycle.mCompletedAt = "";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    // Expect two failures: one for started_at, one for completed_at.
    int Count = CountIssuesWithId(Json, "completed_phase_timestamp_required");
    EXPECT_EQ(Count, 2);
}

TEST_F(FBundleTestFixture,
       CompletedPhaseTimestampRequiredPassesWithBothTimestamps)
{
    // Fixture pre-stamps both timestamps on Completed phases.
    CreateMinimalFixture("T", UniPlan::ETopicStatus::Completed, 1,
                         UniPlan::EExecutionStatus::Completed, true);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "completed_phase_timestamp_required"), 0);
}

// -------------------------------------------------------------------
// topic_fields_not_identical — byte-identical summary/goals signals a
// migration copy-paste bug
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, TopicFieldsNotIdenticalFlagsSummaryEqualsGoals)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // 50-char string — above the 40-char threshold.
    const std::string Prose =
        "Freeze command semantics and provider mapping before code.";
    Bundle.mMetadata.mSummary = Prose;
    Bundle.mMetadata.mGoals = Prose;
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "topic_fields_not_identical");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
    // Path encodes which pair collided.
    EXPECT_EQ(Issue["path"].get<std::string>(), "summary==goals");
}

TEST_F(FBundleTestFixture, TopicFieldsNotIdenticalPassesWhenDistinct)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mMetadata.mSummary =
        "Freeze command semantics and provider mapping before code.";
    Bundle.mMetadata.mGoals =
        "Ship one working end-to-end image.create happy path on Freepik.";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "topic_fields_not_identical"), 0);
}

// -------------------------------------------------------------------
// no_degenerate_dependency_entry — typed deps must carry real fields
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, NoDegenerateDependencyEntryFlagsBundleWithoutTopic)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Shell entry: kind=Bundle (the default) with empty topic — the
    // exact migration defect seen in 25 mm-factory bundles.
    UniPlan::FBundleReference Ref;
    Ref.mKind = UniPlan::EDependencyKind::Bundle;
    Ref.mNote = "Upstream depends on CiloLaunch";
    Bundle.mMetadata.mDependencies.push_back(std::move(Ref));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "no_degenerate_dependency_entry");
    ASSERT_FALSE(Issue.empty());
    // Promoted from Warning to ErrorMinor in v0.73.1 once mm-factory
    // corpus hit zero residuals on this check.
    EXPECT_EQ(Issue["severity"], "error_minor");
    EXPECT_EQ(Issue["path"].get<std::string>(), "dependencies[0]");
}

TEST_F(FBundleTestFixture, NoDegenerateDependencyEntryPassesWithRealTopic)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FBundleReference Ref;
    Ref.mKind = UniPlan::EDependencyKind::Bundle;
    Ref.mTopic = "CiloLaunch";
    Ref.mNote = "Upstream token launchpad";
    Bundle.mMetadata.mDependencies.push_back(std::move(Ref));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "no_degenerate_dependency_entry"), 0);
}

// ===================================================================
// v0.89.0 typed-array evaluators
// ===================================================================

TEST_F(FBundleTestFixture,
       ScopeAndNonScopePopulatedFlagsInProgressTopicWithEmptyFields)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Blank both — the exact VoGame shape: in_progress topic with summary
    // populated but goals/non_goals empty.
    Bundle.mMetadata.mGoals = "";
    Bundle.mMetadata.mNonGoals = "";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "scope_and_non_scope_populated");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
    EXPECT_EQ(Issue["path"].get<std::string>(), "plan");
}

TEST_F(FBundleTestFixture, ScopeAndNonScopePopulatedPassesWhenBothAreSet)
{
    // CreateMinimalFixture seeds goals + non_goals for active/completed
    // topics since v0.89.0, so the default state is clean.
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "scope_and_non_scope_populated"), 0);
}

TEST_F(FBundleTestFixture,
       ScopeAndNonScopePopulatedSkipsNotStartedTopic)
{
    // Not-started topics are still being authored; don't fire on them.
    CreateMinimalFixture("T", UniPlan::ETopicStatus::NotStarted, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mMetadata.mGoals = "";
    Bundle.mMetadata.mNonGoals = "";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "scope_and_non_scope_populated"), 0);
}

TEST_F(FBundleTestFixture, RiskEntryWellformedFlagsEmptyStatement)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FRiskEntry Risk;
    // mStatement intentionally left empty
    Risk.mMitigation = "Some mitigation";
    Bundle.mMetadata.mRisks.push_back(std::move(Risk));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "risk_entry_wellformed");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "error_minor");
    EXPECT_EQ(Issue["path"].get<std::string>(), "risks[0].statement");
}

TEST_F(FBundleTestFixture,
       RiskSeverityPopulatedForHighImpactFlagsHighWithoutMitigation)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FRiskEntry Risk;
    Risk.mStatement = "Critical regression if builder fails";
    Risk.mSeverity = UniPlan::ERiskSeverity::High;
    Risk.mStatus = UniPlan::ERiskStatus::Open; // not accepted/closed
    // mMitigation intentionally empty
    Bundle.mMetadata.mRisks.push_back(std::move(Risk));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue =
        FirstIssueWithId(Json, "risk_severity_populated_for_high_impact");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
}

TEST_F(FBundleTestFixture, NextActionOrderUniqueFlagsDuplicates)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Clear the seeded action so we control the test input exactly.
    Bundle.mNextActions.clear();
    for (int I = 0; I < 2; ++I)
    {
        UniPlan::FNextActionEntry NA;
        NA.mOrder = 3; // both same order
        NA.mStatement = "Action " + std::to_string(I);
        Bundle.mNextActions.push_back(std::move(NA));
    }
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "next_action_order_unique");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "error_minor");
}

TEST_F(FBundleTestFixture,
       AcceptanceCriteriaHasEntriesFlagsCompletedTopicWithEmptyArray)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::Completed, 1,
                         UniPlan::EExecutionStatus::Completed, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Clear the seeded criterion — the fixture added one for completed
    // topics since v0.89.0, but this test wants the empty state.
    Bundle.mMetadata.mAcceptanceCriteria.clear();
    Bundle.mPhases[0].mbNoFileManifest = true;
    Bundle.mPhases[0].mFileManifestSkipReason = "Test scoped to AC evaluator";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue =
        FirstIssueWithId(Json, "acceptance_criteria_has_entries");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "error_minor");
}

TEST_F(FBundleTestFixture,
       CompletedTopicCriteriaAllMetFlagsUnmetCriterionOnCompletedTopic)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::Completed, 1,
                         UniPlan::EExecutionStatus::Completed, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Replace the seeded Met criterion with one still NotMet.
    Bundle.mMetadata.mAcceptanceCriteria.clear();
    UniPlan::FAcceptanceCriterionEntry AC;
    AC.mStatement = "Still open";
    AC.mStatus = UniPlan::ECriterionStatus::NotMet;
    Bundle.mMetadata.mAcceptanceCriteria.push_back(std::move(AC));
    Bundle.mPhases[0].mbNoFileManifest = true;
    Bundle.mPhases[0].mFileManifestSkipReason = "Test scoped to AC evaluator";
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue =
        FirstIssueWithId(Json, "completed_topic_criteria_all_met");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
}

// -------------------------------------------------------------------
// v0.98.0 typed-array evaluators: priority_groupings / runbooks /
// residual_risks. 7 validators — 6 ErrorMinor + 1 Warning.
// -------------------------------------------------------------------

TEST_F(FBundleTestFixture, PriorityGroupingWellformedFlagsEmptyRule)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FPriorityGrouping G;
    G.mID = "O1";
    G.mPhaseIndices = {0};
    G.mRule = ""; // violation
    Bundle.mMetadata.mPriorityGroupings.push_back(std::move(G));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "priority_grouping_wellformed");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "error_minor");
}

TEST_F(FBundleTestFixture, PriorityGroupingPhaseIndexValidFlagsOutOfRange)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FPriorityGrouping G;
    G.mID = "O1";
    G.mPhaseIndices = {5}; // topic has 2 phases, so 5 is out of range
    G.mRule = "rule";
    Bundle.mMetadata.mPriorityGroupings.push_back(std::move(G));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue =
        FirstIssueWithId(Json, "priority_grouping_phase_index_valid");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "error_minor");
}

TEST_F(FBundleTestFixture, PriorityGroupingIdUniqueFlagsDuplicate)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FPriorityGrouping G1;
    G1.mID = "O1";
    G1.mPhaseIndices = {0};
    G1.mRule = "r1";
    UniPlan::FPriorityGrouping G2;
    G2.mID = "O1"; // duplicate
    G2.mPhaseIndices = {1};
    G2.mRule = "r2";
    Bundle.mMetadata.mPriorityGroupings.push_back(std::move(G1));
    Bundle.mMetadata.mPriorityGroupings.push_back(std::move(G2));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "priority_grouping_id_unique");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "error_minor");
}

TEST_F(FBundleTestFixture, RunbookWellformedFlagsEmptyCommands)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FRunbookProcedure R;
    R.mName = "n";
    R.mTrigger = "t";
    // R.mCommands left empty — violation
    Bundle.mMetadata.mRunbooks.push_back(std::move(R));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "runbook_wellformed");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "error_minor");
}

TEST_F(FBundleTestFixture, RunbookNameUniqueFlagsDuplicate)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FRunbookProcedure R1;
    R1.mName = "Baseline";
    R1.mTrigger = "t";
    R1.mCommands = {"c"};
    UniPlan::FRunbookProcedure R2;
    R2.mName = "Baseline"; // duplicate
    R2.mTrigger = "t";
    R2.mCommands = {"c"};
    Bundle.mMetadata.mRunbooks.push_back(std::move(R1));
    Bundle.mMetadata.mRunbooks.push_back(std::move(R2));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "runbook_name_unique");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "error_minor");
}

TEST_F(FBundleTestFixture, ResidualRiskWellformedFlagsEmptyWhyDeferred)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FResidualRiskEntry R;
    R.mArea = "A";
    R.mObservation = "O";
    R.mWhyDeferred = ""; // violation
    Bundle.mMetadata.mResidualRisks.push_back(std::move(R));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue = FirstIssueWithId(Json, "residual_risk_wellformed");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "error_minor");
}

TEST_F(FBundleTestFixture, ResidualRiskClosureShaFormatFlagsNonHex)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FResidualRiskEntry R;
    R.mArea = "A";
    R.mObservation = "O";
    R.mWhyDeferred = "W";
    R.mClosureSha = "NOT-A-SHA"; // violation — not hex + has uppercase
    Bundle.mMetadata.mResidualRisks.push_back(std::move(R));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    const auto Issue =
        FirstIssueWithId(Json, "residual_risk_closure_sha_format");
    ASSERT_FALSE(Issue.empty());
    EXPECT_EQ(Issue["severity"], "warning");
}

TEST_F(FBundleTestFixture, ResidualRiskClosureShaFormatAcceptsValidHex)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FResidualRiskEntry R;
    R.mArea = "A";
    R.mObservation = "O";
    R.mWhyDeferred = "W";
    R.mClosureSha = "deadbeef";
    Bundle.mMetadata.mResidualRisks.push_back(std::move(R));
    WriteBundle(mRepoRoot, "T", Bundle);

    StartCapture();
    UniPlan::RunBundleValidateCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(CountIssuesWithId(Json, "residual_risk_closure_sha_format"), 0);
}
