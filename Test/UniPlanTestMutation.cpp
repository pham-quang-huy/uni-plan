#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanJSONIO.h"
#include "UniPlanTypes.h"

#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

// ===================================================================
// topic set
// ===================================================================

TEST_F(FBundleTestFixture, TopicSetStatusChangesBundle)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTopicSetCommand(
        {"--topic", "SampleTopic", "--status", "blocked", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json["ok"].get<bool>());

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    EXPECT_EQ(Bundle.mStatus, UniPlan::ETopicStatus::Blocked);
    EXPECT_FALSE(Bundle.mChangeLogs.empty());
}

TEST_F(FBundleTestFixture, TopicSetInvalidStatusFails)
{
    CopyFixture("SampleTopic");
    EXPECT_THROW(UniPlan::RunTopicSetCommand(
                     {"--topic", "SampleTopic", "--status", "garbage",
                      "--repo-root", mRepoRoot.string()},
                     mRepoRoot.string()),
                 UniPlan::UsageError);
}

// v0.89.0: --next-actions and --next-actions-file removed from `topic set`;
// the field is a typed array and is mutated via the `next-action` group.
// Regression test: the removed flag surfaces a UsageError with a migration
// pointer rather than being silently accepted.
TEST_F(FBundleTestFixture, TopicSetNextActionsFlagRemovedInV0_89_0)
{
    CopyFixture("SampleTopic");
    EXPECT_THROW(UniPlan::RunTopicSetCommand(
                     {"--topic", "SampleTopic", "--next-actions", "Do stuff",
                      "--repo-root", mRepoRoot.string()},
                     mRepoRoot.string()),
                 UniPlan::UsageError);
    EXPECT_THROW(UniPlan::RunTopicSetCommand(
                     {"--topic", "SampleTopic", "--next-actions-file", "/tmp/x",
                      "--repo-root", mRepoRoot.string()},
                     mRepoRoot.string()),
                 UniPlan::UsageError);
}

TEST_F(FBundleTestFixture, TopicSetMetadataSummary)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunTopicSetCommand(
        {"--topic", "SampleTopic", "--summary", "New summary text",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mMetadata.mSummary, "New summary text");
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
}

TEST_F(FBundleTestFixture, TopicSetMetadataMultipleFields)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTopicSetCommand(
        {"--topic", "SampleTopic", "--goals", "G1\nG2", "--dependency-add",
         "bundle|ClientServer|||needs client API", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mMetadata.mGoals, "G1\nG2");
    ASSERT_EQ(After.mMetadata.mDependencies.size(), 1u);
    EXPECT_EQ(After.mMetadata.mDependencies[0].mKind,
              UniPlan::EDependencyKind::Bundle);
    EXPECT_EQ(After.mMetadata.mDependencies[0].mTopic, "ClientServer");
    EXPECT_EQ(After.mMetadata.mDependencies[0].mNote, "needs client API");
}

// v0.89.0: --risks and --acceptance-criteria (and their -file variants)
// removed from `topic set`. Regression test: the removed flags surface a
// UsageError with a migration pointer.
TEST_F(FBundleTestFixture,
       TopicSetRisksAndAcceptanceCriteriaFlagsRemovedInV0_89_0)
{
    CopyFixture("SampleTopic");
    EXPECT_THROW(
        UniPlan::RunTopicSetCommand({"--topic", "SampleTopic", "--risks", "R1",
                                     "--repo-root", mRepoRoot.string()},
                                    mRepoRoot.string()),
        UniPlan::UsageError);
    EXPECT_THROW(UniPlan::RunTopicSetCommand(
                     {"--topic", "SampleTopic", "--acceptance-criteria",
                      "completed", "--repo-root", mRepoRoot.string()},
                     mRepoRoot.string()),
                 UniPlan::UsageError);
}

TEST_F(FBundleTestFixture, TopicSetNoFieldsFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTopicSetCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// phase set
// ===================================================================

TEST_F(FBundleTestFixture, PhaseSetDoneAndRemaining)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--done", "W1 done",
         "--remaining", "W2-W3", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["target"], "phases[1]");
    AssertNoLegacyPhasePath(Json["target"].get<std::string>());

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    EXPECT_EQ(Bundle.mPhases[1].mLifecycle.mDone, "W1 done");
    EXPECT_EQ(Bundle.mPhases[1].mLifecycle.mRemaining, "W2-W3");
    EXPECT_GT(Bundle.mChangeLogs.size(), ChangelogsBefore);
    ASSERT_FALSE(Bundle.mChangeLogs.empty());
    EXPECT_EQ(Bundle.mChangeLogs.back().mAffected, "phases[1]");
    AssertNoLegacyPhasePath(Bundle.mChangeLogs.back().mAffected);
}

TEST_F(FBundleTestFixture, PhaseSetOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "SampleTopic", "--phase", "99", "--status", "blocked",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, PhaseAddAppendsTrailingPhase)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, false);
    StartCapture();
    const int Code = UniPlan::RunPhaseAddCommand(
        {"--topic", "T", "--scope", "New scope for phase 2", "--output",
         "New output", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("T", After));
    EXPECT_EQ(After.mPhases.size(), 3u);
    EXPECT_EQ(After.mPhases[2].mScope, "New scope for phase 2");
    EXPECT_EQ(After.mPhases[2].mOutput, "New output");
    EXPECT_EQ(After.mPhases[2].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::NotStarted);
    EXPECT_GT(After.mChangeLogs.size(), 0u);
    EXPECT_EQ(After.mChangeLogs.back().mPhase, -1);
    EXPECT_NE(After.mChangeLogs.back().mChange.find("Added phases[2]"),
              std::string::npos);
}

TEST_F(FBundleTestFixture, PhaseAddDefaultsToNotStarted)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    StartCapture();
    const int Code = UniPlan::RunPhaseAddCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("T", After));
    EXPECT_EQ(After.mPhases.size(), 2u);
    EXPECT_EQ(After.mPhases[1].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::NotStarted);
    EXPECT_TRUE(After.mPhases[1].mScope.empty());
}

TEST_F(FBundleTestFixture, PhaseNormalizeReplacesSmartChars)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mDesign.mInvestigation =
        "Baseline \xE2\x80\x94 D3D12 reference uses \xE2\x80\x9Csmart\xE2\x80"
        "\x9D quotes.";
    Bundle.mPhases[0].mDesign.mReadinessGate = "Phase 6 \xE2\x80\x93 complete";
    {
        const fs::path BundlePath =
            mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
        std::string WriteError;
        ASSERT_TRUE(
            UniPlan::TryWriteTopicBundle(Bundle, BundlePath, WriteError))
            << WriteError;
    }

    StartCapture();
    const int Code = UniPlan::RunPhaseNormalizeCommand(
        {"--topic", "T", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("T", After));
    EXPECT_EQ(After.mPhases[0].mDesign.mInvestigation,
              "Baseline - D3D12 reference uses \"smart\" quotes.");
    EXPECT_EQ(After.mPhases[0].mDesign.mReadinessGate, "Phase 6 - complete");
    EXPECT_GT(After.mChangeLogs.size(), 0u);
    EXPECT_NE(After.mChangeLogs.back().mChange.find("Normalized phases[0]"),
              std::string::npos);
}

TEST_F(FBundleTestFixture, PhaseNormalizeDryRunDoesNotMutate)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    const std::string Original = "Dash\xE2\x80\x94here";
    Bundle.mPhases[0].mDesign.mHandoff = Original;
    {
        const fs::path BundlePath =
            mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
        std::string WriteError;
        ASSERT_TRUE(
            UniPlan::TryWriteTopicBundle(Bundle, BundlePath, WriteError))
            << WriteError;
    }
    const size_t ChangelogBefore = Bundle.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunPhaseNormalizeCommand(
        {"--topic", "T", "--phase", "0", "--dry-run", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("T", After));
    EXPECT_EQ(After.mPhases[0].mDesign.mHandoff, Original);
    EXPECT_EQ(After.mChangeLogs.size(), ChangelogBefore);
}

TEST_F(FBundleTestFixture, PhaseNormalizeCleanPhaseIsNoOp)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mDesign.mHandoff = "Clean prose with no smart chars.";
    {
        const fs::path BundlePath =
            mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
        std::string WriteError;
        ASSERT_TRUE(
            UniPlan::TryWriteTopicBundle(Bundle, BundlePath, WriteError))
            << WriteError;
    }
    const size_t ChangelogBefore = Bundle.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunPhaseNormalizeCommand(
        {"--topic", "T", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("T", After));
    EXPECT_EQ(After.mChangeLogs.size(), ChangelogBefore);
}

TEST_F(FBundleTestFixture, PhaseNormalizeOutOfRangePhaseFails)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    StartCapture();
    const int Code = UniPlan::RunPhaseNormalizeCommand(
        {"--topic", "T", "--phase", "5", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
    EXPECT_NE(mCapturedStderr.find("out of range"), std::string::npos);
}

TEST_F(FBundleTestFixture, PhaseAddRejectsInvalidStatus)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    EXPECT_THROW(
        UniPlan::RunPhaseAddCommand({"--topic", "T", "--status", "bogus",
                                     "--repo-root", mRepoRoot.string()},
                                    mRepoRoot.string()),
        UniPlan::UsageError);
    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("T", After));
    EXPECT_EQ(After.mPhases.size(), 1u);
}

TEST_F(FBundleTestFixture, PhaseRemoveTrailingNotStartedPhase)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 3,
                         UniPlan::EExecutionStatus::NotStarted, false);
    StartCapture();
    const int Code = UniPlan::RunPhaseRemoveCommand(
        {"--topic", "T", "--phase", "2", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("T", After));
    EXPECT_EQ(After.mPhases.size(), 2u);
    EXPECT_GT(After.mChangeLogs.size(), 0u);
    // Removal changelog is filed topic-scoped (mPhase=-1, mAffected="plan")
    // because the phase it references no longer exists — leaving it
    // phase-scoped would dangle and trip changelog_phase_ref.
    EXPECT_EQ(After.mChangeLogs.back().mPhase, -1);
    EXPECT_NE(After.mChangeLogs.back().mChange.find("Removed phases[2]"),
              std::string::npos);
}

TEST_F(FBundleTestFixture, PhaseRemoveRefusesNonTrailingPhase)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 3,
                         UniPlan::EExecutionStatus::NotStarted, false);
    StartCapture();
    const int Code = UniPlan::RunPhaseRemoveCommand(
        {"--topic", "T", "--phase", "1", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
    EXPECT_NE(mCapturedStderr.find("trailing phase"), std::string::npos);
    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("T", After));
    EXPECT_EQ(After.mPhases.size(), 3u);
}

TEST_F(FBundleTestFixture, PhaseRemoveRefusesInProgressPhase)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::InProgress, false);
    StartCapture();
    const int Code = UniPlan::RunPhaseRemoveCommand(
        {"--topic", "T", "--phase", "1", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
    EXPECT_NE(mCapturedStderr.find("not_started"), std::string::npos);
    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("T", After));
    EXPECT_EQ(After.mPhases.size(), 2u);
}

TEST_F(FBundleTestFixture, PhaseRemoveRefusesWhenChangelogReferences)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, false);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FChangeLogEntry C;
    C.mDate = "2026-04-18";
    C.mChange = "Note referencing the trailing phase";
    C.mPhase = 1;
    Bundle.mChangeLogs.push_back(C);
    const fs::path BundlePath = mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
    std::string WriteError;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, BundlePath, WriteError))
        << WriteError;

    StartCapture();
    const int Code = UniPlan::RunPhaseRemoveCommand(
        {"--topic", "T", "--phase", "1", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
    EXPECT_NE(mCapturedStderr.find("changelogs[]"), std::string::npos);
    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("T", After));
    EXPECT_EQ(After.mPhases.size(), 2u);
}

TEST_F(FBundleTestFixture, PhaseSetDesignMaterial)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--scope",
         "Implement core rendering", "--investigation",
         "Checked Metal API docs", "--best-practices",
         "Use RAII for GPU resources", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mScope, "Implement core rendering");
    EXPECT_EQ(After.mPhases[1].mDesign.mInvestigation,
              "Checked Metal API docs");
    EXPECT_EQ(After.mPhases[1].mDesign.mBestPractices,
              "Use RAII for GPU resources");
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
}

TEST_F(FBundleTestFixture, PhaseSetOutputAndHandoff)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--output",
         "Compiled shader pipeline", "--handoff", "Ready for testing phase",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mOutput, "Compiled shader pipeline");
    EXPECT_EQ(After.mPhases[1].mDesign.mHandoff, "Ready for testing phase");
}

// ===================================================================
// job set
// ===================================================================

TEST_F(FBundleTestFixture, JobSetStatusCompleted)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunJobSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--job", "0", "--status",
         "completed", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    EXPECT_EQ(Bundle.mPhases[1].mJobs[0].mStatus,
              UniPlan::EExecutionStatus::Completed);
    EXPECT_GT(Bundle.mChangeLogs.size(), ChangelogsBefore);
}

TEST_F(FBundleTestFixture, JobSetOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunJobSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--job", "99", "--status",
         "completed", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, JobSetScopeAndOutput)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunJobSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--job", "0", "--scope",
         "Build shader compiler", "--output", "Compiled SPIR-V",
         "--exit-criteria", "All shaders compile", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mJobs[0].mScope, "Build shader compiler");
    EXPECT_EQ(After.mPhases[1].mJobs[0].mOutput, "Compiled SPIR-V");
    EXPECT_EQ(After.mPhases[1].mJobs[0].mExitCriteria, "All shaders compile");
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
}

TEST_F(FBundleTestFixture, JobSetNoFieldsFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunJobSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--job", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// task set
// ===================================================================

TEST_F(FBundleTestFixture, TaskSetEvidenceAndNotes)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunTaskSetCommand(
        {"--topic", "SampleTopic", "--phase", "0", "--job", "0", "--task", "0",
         "--evidence", "proof", "--notes", "looks good", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    EXPECT_EQ(Bundle.mPhases[0].mJobs[0].mTasks[0].mEvidence, "proof");
    EXPECT_GT(Bundle.mChangeLogs.size(), ChangelogsBefore);
}

TEST_F(FBundleTestFixture, TaskSetOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTaskSetCommand(
        {"--topic", "SampleTopic", "--phase", "0", "--job", "0", "--task", "99",
         "--status", "completed", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// changelog add
// ===================================================================

TEST_F(FBundleTestFixture, ChangelogAddAppendsEntry)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t CountBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunChangelogAddCommand(
        {"--topic", "SampleTopic", "--phase", "0", "--change", "Test entry",
         "--type", "feat", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mChangeLogs.size(), CountBefore + 1);
    EXPECT_EQ(After.mChangeLogs.back().mChange, "Test entry");
}

TEST_F(FBundleTestFixture, ChangelogAddMissingTopicFails)
{
    StartCapture();
    EXPECT_THROW(UniPlan::RunChangelogAddCommand(
                     {"--change", "test", "--repo-root", mRepoRoot.string()},
                     mRepoRoot.string()),
                 UniPlan::UsageError);
    StopCapture();
}

// ===================================================================
// verification add
// ===================================================================

TEST_F(FBundleTestFixture, VerificationAddAppendsEntry)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t CountBefore = Before.mVerifications.size();

    StartCapture();
    const int Code = UniPlan::RunVerificationAddCommand(
        {"--topic", "SampleTopic", "--phase", "0", "--check", "Build clean",
         "--result", "pass", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mVerifications.size(), CountBefore + 1);
    EXPECT_EQ(After.mVerifications.back().mCheck, "Build clean");
}

TEST_F(FBundleTestFixture, VerificationAddMissingTopicFails)
{
    StartCapture();
    EXPECT_THROW(UniPlan::RunVerificationAddCommand(
                     {"--check", "test", "--repo-root", mRepoRoot.string()},
                     mRepoRoot.string()),
                 UniPlan::UsageError);
    StopCapture();
}

TEST_F(FBundleTestFixture, ChangelogSetUpdatesEntry)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunChangelogSetCommand(
        {"--topic", "SampleTopic", "--index", "0", "--phase", "2", "--change",
         "Retargeted entry", "--type", "fix", "--affected", "phases[2].jobs[0]",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["target"], "changelogs[0]");

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_FALSE(After.mChangeLogs.empty());
    EXPECT_EQ(After.mChangeLogs[0].mPhase, 2);
    EXPECT_EQ(After.mChangeLogs[0].mChange, "Retargeted entry");
    EXPECT_EQ(After.mChangeLogs[0].mAffected, "phases[2].jobs[0]");
    AssertNoLegacyPhasePath(After.mChangeLogs[0].mAffected);
    EXPECT_EQ(After.mChangeLogs[0].mType, UniPlan::EChangeType::Fix);
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
}

TEST_F(FBundleTestFixture, ChangelogSetOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunChangelogSetCommand(
        {"--topic", "SampleTopic", "--index", "99", "--change", "X",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// lane set
// ===================================================================

TEST_F(FBundleTestFixture, LaneSetChangesStatus)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunLaneSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--lane", "0", "--status",
         "completed", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    EXPECT_EQ(Bundle.mPhases[1].mLanes[0].mStatus,
              UniPlan::EExecutionStatus::Completed);
    EXPECT_GT(Bundle.mChangeLogs.size(), ChangelogsBefore);
}

TEST_F(FBundleTestFixture, LaneSetOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunLaneSetCommand(
        {"--topic", "SampleTopic", "--phase", "0", "--lane", "99", "--status",
         "completed", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, LaneSetScopeAndExitCriteria)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunLaneSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--lane", "0", "--scope",
         "GPU pipeline lane", "--exit-criteria", "All passes render",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mLanes[0].mScope, "GPU pipeline lane");
    EXPECT_EQ(After.mPhases[1].mLanes[0].mExitCriteria, "All passes render");
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
}

TEST_F(FBundleTestFixture, LaneSetNoFieldsFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunLaneSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--lane", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// phase set — --started-at / --completed-at timestamp overrides
// (v0.73.0)
//
// These flags exist to support migration / repair scenarios where a
// historical completion date must be backfilled instead of stamping
// "now". Without the override, `phase set --status completed` always
// writes GetUtcNow() which buries real history.
// ===================================================================

TEST_F(FBundleTestFixture, PhaseSetStartedAtOverrideWinsOverAutoStamp)
{
    // Phase 1 of SampleTopic starts as not_started. Moving it to
    // in_progress WITHOUT the override stamps "now"; WITH the override
    // the explicit value must win.
    CopyFixture("SampleTopic");
    const std::string kHistorical = "2025-06-15T09:00:00Z";

    StartCapture();
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--status", "in_progress",
         "--started-at", kHistorical, "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    EXPECT_EQ(Bundle.mPhases[1].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::InProgress);
    EXPECT_EQ(Bundle.mPhases[1].mLifecycle.mStartedAt, kHistorical);
}

TEST_F(FBundleTestFixture, PhaseSetCompletedAtOverrideWinsForStatusCompleted)
{
    // Explicit --completed-at must beat the auto-stamp for a completion
    // transition. The fixture phase 1 already carries a started_at, so
    // only completed_at is written; the pre-existing started_at must be
    // preserved (NOT clobbered by the override or auto-stamp).
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const std::string PriorStart = Before.mPhases[1].mLifecycle.mStartedAt;
    ASSERT_FALSE(PriorStart.empty());
    const std::string kHistorical = "2024-11-01T18:00:00Z";

    StartCapture();
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--status", "completed",
         "--completed-at", kHistorical, "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    EXPECT_EQ(Bundle.mPhases[1].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::Completed);
    EXPECT_EQ(Bundle.mPhases[1].mLifecycle.mCompletedAt, kHistorical);
    // started_at must be left alone when it was already populated.
    EXPECT_EQ(Bundle.mPhases[1].mLifecycle.mStartedAt, PriorStart);
}

TEST_F(FBundleTestFixture, PhaseSetStartedAtInvalidFormatThrowsUsageError)
{
    // Parse-time validation must reject malformed ISO values before any
    // bundle mutation. The option parser throws UsageError; the CLI
    // dispatcher catches and emits exit 1 in production. The test
    // exercises the parser contract directly.
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const UniPlan::EExecutionStatus PriorStatus =
        Before.mPhases[1].mLifecycle.mStatus;
    const std::string PriorStart = Before.mPhases[1].mLifecycle.mStartedAt;
    const size_t PriorChangelogs = Before.mChangeLogs.size();

    StartCapture();
    EXPECT_THROW(UniPlan::RunPhaseSetCommand(
                     {"--topic", "SampleTopic", "--phase", "1", "--status",
                      "completed", "--started-at", "not-a-timestamp",
                      "--repo-root", mRepoRoot.string()},
                     mRepoRoot.string()),
                 UniPlan::UsageError);
    StopCapture();

    // Bundle must not have been mutated — status unchanged, timestamps
    // unchanged, no changelog entry appended.
    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mLifecycle.mStatus, PriorStatus);
    EXPECT_EQ(After.mPhases[1].mLifecycle.mStartedAt, PriorStart);
    EXPECT_EQ(After.mChangeLogs.size(), PriorChangelogs);
}

TEST_F(FBundleTestFixture, PhaseSetStatusCompletedRequiresStartedAtWhenMissing)
{
    // Data Fix Gate contract: when a not_started phase is flipped
    // straight to completed, the CLI must refuse to fabricate a
    // started_at from "now" or from completed_at. The caller must
    // supply --started-at <iso> so the recorded start time reflects
    // real history, not invented data. Phase 2 of the fixture is
    // not_started with empty started_at — the exact shape this gate
    // protects.
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    ASSERT_EQ(Before.mPhases[2].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::NotStarted);
    ASSERT_TRUE(Before.mPhases[2].mLifecycle.mStartedAt.empty());
    const size_t PriorChangelogs = Before.mChangeLogs.size();

    StartCapture();
    EXPECT_THROW(UniPlan::RunPhaseSetCommand(
                     {"--topic", "SampleTopic", "--phase", "2", "--status",
                      "completed", "--repo-root", mRepoRoot.string()},
                     mRepoRoot.string()),
                 UniPlan::UsageError);
    StopCapture();

    // Bundle must not have been mutated — status stays not_started,
    // timestamps still empty, no changelog entry appended.
    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[2].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::NotStarted);
    EXPECT_TRUE(After.mPhases[2].mLifecycle.mStartedAt.empty());
    EXPECT_TRUE(After.mPhases[2].mLifecycle.mCompletedAt.empty());
    EXPECT_EQ(After.mChangeLogs.size(), PriorChangelogs);
}

TEST_F(FBundleTestFixture, PhaseSetStatusCompletedWithStartedAtSucceeds)
{
    // Positive counterpart to the gate: supplying --started-at unblocks
    // the not_started → completed transition. completed_at defaults to
    // "now" (the transition is happening now, truthfully), while
    // started_at is taken verbatim from the override.
    CopyFixture("SampleTopic");
    const std::string kHistoricalStart = "2025-12-01T14:00:00Z";

    StartCapture();
    const int ExitCode = UniPlan::RunPhaseSetCommand(
        {"--topic", "SampleTopic", "--phase", "2", "--status", "completed",
         "--started-at", kHistoricalStart, "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(ExitCode, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[2].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::Completed);
    EXPECT_EQ(After.mPhases[2].mLifecycle.mStartedAt, kHistoricalStart);
    EXPECT_FALSE(After.mPhases[2].mLifecycle.mCompletedAt.empty());
}

// ===================================================================
// v0.89.0 typed-array CLI groups (risk / next-action / acceptance-
// criterion). Each group gets add + set + remove + list round-trip
// coverage — the same four subcommands the `changelog` precedent uses
// at UniPlanCommandMutation.cpp:992 + UniPlanCommandEntity.cpp:859,947.
//
// The SampleTopic fixture seeds legacy string-form values in risks,
// next_actions, and acceptance_criteria, which dual-read auto-promotes
// to 1-entry arrays on load. RESET_TYPED_ARRAYS wipes them so tests
// can assert exact sizes without carrying the seed overhead. Macro
// form (not a helper function) because ReloadBundle and mRepoRoot are
// protected members of FBundleTestFixture, reachable only from derived
// class bodies like the TEST_F blocks themselves.
// ===================================================================

#define RESET_TYPED_ARRAYS(TOPIC)                                              \
    do                                                                         \
    {                                                                          \
        UniPlan::FTopicBundle _ResetBundle;                                    \
        ASSERT_TRUE(ReloadBundle((TOPIC), _ResetBundle));                      \
        _ResetBundle.mMetadata.mRisks.clear();                                 \
        _ResetBundle.mMetadata.mAcceptanceCriteria.clear();                    \
        _ResetBundle.mNextActions.clear();                                     \
        const fs::path _ResetPath =                                            \
            mRepoRoot / "Docs" / "Plans" / ((TOPIC) + ".Plan.json");           \
        std::string _ResetError;                                               \
        ASSERT_TRUE(UniPlan::TryWriteTopicBundle(_ResetBundle, _ResetPath,     \
                                                 _ResetError))                 \
            << _ResetError;                                                    \
    } while (0)

// --- risk group -----------------------------------------------------

TEST_F(FBundleTestFixture, RiskAddAppendsTypedEntryAndWritesArrayForm)
{
    CopyFixture("SampleTopic");
    RESET_TYPED_ARRAYS(std::string("SampleTopic"));
    StartCapture();
    const int Code = UniPlan::RunRiskAddCommand(
        {"--topic", "SampleTopic", "--statement", "Listener drift",
         "--mitigation", "Pin endpoint", "--severity", "high", "--id", "R1",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json["ok"].get<bool>());
    EXPECT_EQ(Json["target"].get<std::string>(), "risks[0]");

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mMetadata.mRisks.size(), 1u);
    EXPECT_EQ(After.mMetadata.mRisks[0].mId, "R1");
    EXPECT_EQ(After.mMetadata.mRisks[0].mStatement, "Listener drift");
    EXPECT_EQ(After.mMetadata.mRisks[0].mMitigation, "Pin endpoint");
    EXPECT_EQ(After.mMetadata.mRisks[0].mSeverity,
              UniPlan::ERiskSeverity::High);
    EXPECT_EQ(After.mMetadata.mRisks[0].mStatus, UniPlan::ERiskStatus::Open);
}

TEST_F(FBundleTestFixture, RiskSetMutatesOnlyPassedFields)
{
    CopyFixture("SampleTopic");
    RESET_TYPED_ARRAYS(std::string("SampleTopic"));
    // Seed two risks so we can verify only index 0 changes.
    UniPlan::RunRiskAddCommand({"--topic", "SampleTopic", "--statement",
                                "First", "--severity", "medium", "--repo-root",
                                mRepoRoot.string()},
                               mRepoRoot.string());
    UniPlan::RunRiskAddCommand({"--topic", "SampleTopic", "--statement",
                                "Second", "--severity", "low", "--repo-root",
                                mRepoRoot.string()},
                               mRepoRoot.string());

    StartCapture();
    const int Code = UniPlan::RunRiskSetCommand(
        {"--topic", "SampleTopic", "--index", "0", "--status", "mitigated",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mMetadata.mRisks.size(), 2u);
    EXPECT_EQ(After.mMetadata.mRisks[0].mStatus,
              UniPlan::ERiskStatus::Mitigated);
    EXPECT_EQ(After.mMetadata.mRisks[0].mStatement, "First"); // unchanged
    EXPECT_EQ(After.mMetadata.mRisks[1].mStatus, UniPlan::ERiskStatus::Open);
}

TEST_F(FBundleTestFixture, RiskRemoveShiftsSubsequentIndicesDown)
{
    CopyFixture("SampleTopic");
    RESET_TYPED_ARRAYS(std::string("SampleTopic"));
    for (const char *Stmt : {"A", "B", "C"})
    {
        UniPlan::RunRiskAddCommand({"--topic", "SampleTopic", "--statement",
                                    Stmt, "--repo-root", mRepoRoot.string()},
                                   mRepoRoot.string());
    }

    StartCapture();
    const int Code =
        UniPlan::RunRiskRemoveCommand({"--topic", "SampleTopic", "--index", "1",
                                       "--repo-root", mRepoRoot.string()},
                                      mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mMetadata.mRisks.size(), 2u);
    EXPECT_EQ(After.mMetadata.mRisks[0].mStatement, "A");
    EXPECT_EQ(After.mMetadata.mRisks[1].mStatement, "C"); // B removed
}

TEST_F(FBundleTestFixture, RiskListFiltersBySeverity)
{
    CopyFixture("SampleTopic");
    RESET_TYPED_ARRAYS(std::string("SampleTopic"));
    UniPlan::RunRiskAddCommand({"--topic", "SampleTopic", "--statement", "L",
                                "--severity", "low", "--repo-root",
                                mRepoRoot.string()},
                               mRepoRoot.string());
    UniPlan::RunRiskAddCommand({"--topic", "SampleTopic", "--statement", "H",
                                "--severity", "high", "--repo-root",
                                mRepoRoot.string()},
                               mRepoRoot.string());

    StartCapture();
    const int Code =
        UniPlan::RunRiskListCommand({"--topic", "SampleTopic", "--severity",
                                     "high", "--repo-root", mRepoRoot.string()},
                                    mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["count"].get<int>(), 1);
    EXPECT_EQ(Json["risks"][0]["severity"].get<std::string>(), "high");
}

// --- next-action group ----------------------------------------------

TEST_F(FBundleTestFixture, NextActionAddAutoAssignsOrderAndWritesArray)
{
    CopyFixture("SampleTopic");
    RESET_TYPED_ARRAYS(std::string("SampleTopic"));
    // First add: auto-order should be 1 (empty array + 1).
    StartCapture();
    const int Code = UniPlan::RunNextActionAddCommand(
        {"--topic", "SampleTopic", "--statement", "Ship v0.89.0", "--rationale",
         "Closes VoGame blind spot", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mNextActions.size(), 1u);
    EXPECT_EQ(After.mNextActions[0].mOrder, 1);
    EXPECT_EQ(After.mNextActions[0].mStatement, "Ship v0.89.0");
    EXPECT_EQ(After.mNextActions[0].mStatus, UniPlan::EActionStatus::Pending);
}

TEST_F(FBundleTestFixture, NextActionSetUpdatesStatusAndOrderByIndex)
{
    CopyFixture("SampleTopic");
    RESET_TYPED_ARRAYS(std::string("SampleTopic"));
    UniPlan::RunNextActionAddCommand({"--topic", "SampleTopic", "--statement",
                                      "Do X", "--repo-root",
                                      mRepoRoot.string()},
                                     mRepoRoot.string());

    StartCapture();
    const int Code = UniPlan::RunNextActionSetCommand(
        {"--topic", "SampleTopic", "--index", "0", "--status", "completed",
         "--order", "7", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mNextActions[0].mOrder, 7);
    EXPECT_EQ(After.mNextActions[0].mStatus, UniPlan::EActionStatus::Completed);
    EXPECT_EQ(After.mNextActions[0].mStatement, "Do X"); // unchanged
}

TEST_F(FBundleTestFixture, NextActionRemoveByIndex)
{
    CopyFixture("SampleTopic");
    RESET_TYPED_ARRAYS(std::string("SampleTopic"));
    UniPlan::RunNextActionAddCommand({"--topic", "SampleTopic", "--statement",
                                      "Alpha", "--repo-root",
                                      mRepoRoot.string()},
                                     mRepoRoot.string());
    UniPlan::RunNextActionAddCommand({"--topic", "SampleTopic", "--statement",
                                      "Beta", "--repo-root",
                                      mRepoRoot.string()},
                                     mRepoRoot.string());

    StartCapture();
    const int Code = UniPlan::RunNextActionRemoveCommand(
        {"--topic", "SampleTopic", "--index", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mNextActions.size(), 1u);
    EXPECT_EQ(After.mNextActions[0].mStatement, "Beta");
}

TEST_F(FBundleTestFixture, NextActionListFiltersByStatus)
{
    CopyFixture("SampleTopic");
    RESET_TYPED_ARRAYS(std::string("SampleTopic"));
    UniPlan::RunNextActionAddCommand({"--topic", "SampleTopic", "--statement",
                                      "P", "--status", "pending", "--repo-root",
                                      mRepoRoot.string()},
                                     mRepoRoot.string());
    UniPlan::RunNextActionAddCommand({"--topic", "SampleTopic", "--statement",
                                      "D", "--status", "completed",
                                      "--repo-root", mRepoRoot.string()},
                                     mRepoRoot.string());

    StartCapture();
    const int Code = UniPlan::RunNextActionListCommand(
        {"--topic", "SampleTopic", "--status", "pending", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["count"].get<int>(), 1);
    EXPECT_EQ(Json["next_actions"][0]["status"].get<std::string>(), "pending");
}

// --- acceptance-criterion group -------------------------------------

TEST_F(FBundleTestFixture, AcceptanceCriterionAddWritesTypedEntry)
{
    CopyFixture("SampleTopic");
    RESET_TYPED_ARRAYS(std::string("SampleTopic"));
    StartCapture();
    const int Code = UniPlan::RunAcceptanceCriterionAddCommand(
        {"--topic", "SampleTopic", "--id", "AC1", "--statement",
         "Validate clean under --strict", "--measure",
         "uni-plan validate --topic SampleTopic --strict", "--status", "met",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mMetadata.mAcceptanceCriteria.size(), 1u);
    EXPECT_EQ(After.mMetadata.mAcceptanceCriteria[0].mId, "AC1");
    EXPECT_EQ(After.mMetadata.mAcceptanceCriteria[0].mStatus,
              UniPlan::ECriterionStatus::Met);
}

TEST_F(FBundleTestFixture, AcceptanceCriterionSetPromotesFromNotMetToMet)
{
    CopyFixture("SampleTopic");
    RESET_TYPED_ARRAYS(std::string("SampleTopic"));
    UniPlan::RunAcceptanceCriterionAddCommand(
        {"--topic", "SampleTopic", "--statement", "Feature shipped",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());

    StartCapture();
    const int Code = UniPlan::RunAcceptanceCriterionSetCommand(
        {"--topic", "SampleTopic", "--index", "0", "--status", "met",
         "--evidence", "commit deadbeef", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mMetadata.mAcceptanceCriteria[0].mStatus,
              UniPlan::ECriterionStatus::Met);
    EXPECT_EQ(After.mMetadata.mAcceptanceCriteria[0].mEvidence,
              "commit deadbeef");
}

TEST_F(FBundleTestFixture, AcceptanceCriterionRemoveDropsEntry)
{
    CopyFixture("SampleTopic");
    RESET_TYPED_ARRAYS(std::string("SampleTopic"));
    UniPlan::RunAcceptanceCriterionAddCommand(
        {"--topic", "SampleTopic", "--statement", "A", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    UniPlan::RunAcceptanceCriterionAddCommand(
        {"--topic", "SampleTopic", "--statement", "B", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());

    StartCapture();
    const int Code = UniPlan::RunAcceptanceCriterionRemoveCommand(
        {"--topic", "SampleTopic", "--index", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mMetadata.mAcceptanceCriteria.size(), 1u);
    EXPECT_EQ(After.mMetadata.mAcceptanceCriteria[0].mStatement, "B");
}

TEST_F(FBundleTestFixture, AcceptanceCriterionListFiltersByStatus)
{
    CopyFixture("SampleTopic");
    RESET_TYPED_ARRAYS(std::string("SampleTopic"));
    UniPlan::RunAcceptanceCriterionAddCommand(
        {"--topic", "SampleTopic", "--statement", "M", "--status", "met",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    UniPlan::RunAcceptanceCriterionAddCommand(
        {"--topic", "SampleTopic", "--statement", "N", "--status", "not_met",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());

    StartCapture();
    const int Code = UniPlan::RunAcceptanceCriterionListCommand(
        {"--topic", "SampleTopic", "--status", "met", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["count"].get<int>(), 1);
    EXPECT_EQ(Json["acceptance_criteria"][0]["statement"].get<std::string>(),
              "M");
}

// ===================================================================
// Dual-read: legacy pipe-delimited string form is auto-promoted to
// typed arrays on bundle load without touching on-disk form. Writer
// always emits array, so first mutation normalizes the file.
// ===================================================================

TEST_F(FBundleTestFixture, DualReadLegacyRisksStringParsesToArray)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    // Write the bundle to disk (canonical array form).
    Bundle.mMetadata.mRisks.clear();
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "SampleTopic.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;
    // Overwrite the risks field on disk with legacy string form by editing
    // the JSON text directly (one-shot setup; reader path is what's under
    // test, not the writer).
    std::ifstream In(Path);
    std::string Raw((std::istreambuf_iterator<char>(In)),
                    std::istreambuf_iterator<char>());
    In.close();
    const std::string Needle = "\"risks\": []";
    const size_t Pos = Raw.find(Needle);
    ASSERT_NE(Pos, std::string::npos);
    Raw.replace(
        Pos, Needle.size(),
        "\"risks\": \"`R1` | Legacy statement | Legacy mitigation | Legacy "
        "notes\\nLegacy-only statement\"");
    std::ofstream Out(Path);
    Out << Raw;
    Out.close();

    UniPlan::FTopicBundle Reloaded;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Reloaded));
    ASSERT_EQ(Reloaded.mMetadata.mRisks.size(), 2u);
    EXPECT_EQ(Reloaded.mMetadata.mRisks[0].mId, "R1");
    EXPECT_EQ(Reloaded.mMetadata.mRisks[0].mStatement, "Legacy statement");
    EXPECT_EQ(Reloaded.mMetadata.mRisks[0].mMitigation, "Legacy mitigation");
    EXPECT_EQ(Reloaded.mMetadata.mRisks[0].mNotes, "Legacy notes");
    // Second row: no id segment, whole prose ends up in statement.
    EXPECT_EQ(Reloaded.mMetadata.mRisks[1].mStatement, "Legacy-only statement");
}

TEST_F(FBundleTestFixture, DualReadLegacyNextActionsStringParsesToArray)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    Bundle.mNextActions.clear();
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "SampleTopic.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;
    std::ifstream In(Path);
    std::string Raw((std::istreambuf_iterator<char>(In)),
                    std::istreambuf_iterator<char>());
    In.close();
    const std::string Needle = "\"next_actions\": []";
    const size_t Pos = Raw.find(Needle);
    ASSERT_NE(Pos, std::string::npos);
    Raw.replace(Pos, Needle.size(),
                "\"next_actions\": \"`1` | Do first | Why first\\n`2` | Do "
                "second | Why second\"");
    std::ofstream Out(Path);
    Out << Raw;
    Out.close();

    UniPlan::FTopicBundle Reloaded;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Reloaded));
    ASSERT_EQ(Reloaded.mNextActions.size(), 2u);
    EXPECT_EQ(Reloaded.mNextActions[0].mOrder, 1);
    EXPECT_EQ(Reloaded.mNextActions[0].mStatement, "Do first");
    EXPECT_EQ(Reloaded.mNextActions[0].mRationale, "Why first");
    EXPECT_EQ(Reloaded.mNextActions[1].mOrder, 2);
    EXPECT_EQ(Reloaded.mNextActions[1].mStatement, "Do second");
}

TEST_F(FBundleTestFixture, DualReadLegacyAcceptanceCriteriaMapsCompletedToMet)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    Bundle.mMetadata.mAcceptanceCriteria.clear();
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "SampleTopic.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;
    std::ifstream In(Path);
    std::string Raw((std::istreambuf_iterator<char>(In)),
                    std::istreambuf_iterator<char>());
    In.close();
    const std::string Needle = "\"acceptance_criteria\": []";
    const size_t Pos = Raw.find(Needle);
    ASSERT_NE(Pos, std::string::npos);
    Raw.replace(Pos, Needle.size(),
                "\"acceptance_criteria\": \"`AC1` | Done thing | "
                "completed\\n`AC2` | Pending thing | pending\"");
    std::ofstream Out(Path);
    Out << Raw;
    Out.close();

    UniPlan::FTopicBundle Reloaded;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Reloaded));
    ASSERT_EQ(Reloaded.mMetadata.mAcceptanceCriteria.size(), 2u);
    EXPECT_EQ(Reloaded.mMetadata.mAcceptanceCriteria[0].mId, "AC1");
    EXPECT_EQ(Reloaded.mMetadata.mAcceptanceCriteria[0].mStatus,
              UniPlan::ECriterionStatus::Met); // "completed" → Met
    EXPECT_EQ(Reloaded.mMetadata.mAcceptanceCriteria[1].mId, "AC2");
    EXPECT_EQ(Reloaded.mMetadata.mAcceptanceCriteria[1].mStatus,
              UniPlan::ECriterionStatus::NotMet); // "pending" → NotMet
}

// ===================================================================
// topic add (v0.94.0) — create a brand-new .Plan.json bundle
// ===================================================================

TEST_F(FBundleTestFixture, TopicAddCreatesFreshBundleWithEmptyPhases)
{
    // Note: no CreateMinimalFixture — the command must cope with a fresh
    // repo where Docs/Plans/ does not yet exist.
    StartCapture();
    const int Code = UniPlan::RunTopicAddCommand(
        {"--topic", "NewThing", "--title", "A new topic", "--summary",
         "Plan summary prose", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "NewThing.Plan.json";
    ASSERT_TRUE(fs::exists(Path));

    UniPlan::FTopicBundle Reloaded;
    ASSERT_TRUE(ReloadBundle("NewThing", Reloaded));
    EXPECT_EQ(Reloaded.mTopicKey, "NewThing");
    EXPECT_EQ(Reloaded.mMetadata.mTitle, "A new topic");
    EXPECT_EQ(Reloaded.mMetadata.mSummary, "Plan summary prose");
    EXPECT_EQ(Reloaded.mStatus, UniPlan::ETopicStatus::NotStarted);
    EXPECT_TRUE(Reloaded.mPhases.empty());
    ASSERT_EQ(Reloaded.mChangeLogs.size(), 1u);
    EXPECT_EQ(Reloaded.mChangeLogs[0].mPhase, -1); // topic-scoped
    EXPECT_NE(Reloaded.mChangeLogs[0].mChange.find("created"),
              std::string::npos);

    // Mutation envelope target is the new kTargetTopic token.
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json["ok"].get<bool>());
    EXPECT_EQ(Json["target"].get<std::string>(), "topic");
    EXPECT_EQ(Json["topic"].get<std::string>(), "NewThing");
}

TEST_F(FBundleTestFixture, TopicAddRejectsCollision)
{
    // First add succeeds.
    const int First =
        UniPlan::RunTopicAddCommand({"--topic", "Dup", "--title", "first",
                                     "--repo-root", mRepoRoot.string()},
                                    mRepoRoot.string());
    ASSERT_EQ(First, 0);

    StartCapture();
    const int Second =
        UniPlan::RunTopicAddCommand({"--topic", "Dup", "--title", "second",
                                     "--repo-root", mRepoRoot.string()},
                                    mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Second, 1);
    EXPECT_NE(mCapturedStderr.find("already exists"), std::string::npos);
}

TEST_F(FBundleTestFixture, TopicAddRejectsNonPascalCaseKey)
{
    // lowercase-first
    EXPECT_THROW(
        UniPlan::RunTopicAddCommand({"--topic", "lower", "--title", "x",
                                     "--repo-root", mRepoRoot.string()},
                                    mRepoRoot.string()),
        UniPlan::UsageError);
    // digit-first
    EXPECT_THROW(
        UniPlan::RunTopicAddCommand({"--topic", "123Foo", "--title", "x",
                                     "--repo-root", mRepoRoot.string()},
                                    mRepoRoot.string()),
        UniPlan::UsageError);
    // hyphen
    EXPECT_THROW(
        UniPlan::RunTopicAddCommand({"--topic", "foo-bar", "--title", "x",
                                     "--repo-root", mRepoRoot.string()},
                                    mRepoRoot.string()),
        UniPlan::UsageError);
    // dot
    EXPECT_THROW(
        UniPlan::RunTopicAddCommand({"--topic", "A.B", "--title", "x",
                                     "--repo-root", mRepoRoot.string()},
                                    mRepoRoot.string()),
        UniPlan::UsageError);
}

TEST_F(FBundleTestFixture, TopicAddAcceptsValidPascalCaseKeys)
{
    EXPECT_EQ(UniPlan::RunTopicAddCommand({"--topic", "Simple", "--title", "x",
                                           "--repo-root", mRepoRoot.string()},
                                          mRepoRoot.string()),
              0);
    EXPECT_EQ(
        UniPlan::RunTopicAddCommand({"--topic", "WithNumbers42", "--title", "x",
                                     "--repo-root", mRepoRoot.string()},
                                    mRepoRoot.string()),
        0);
    EXPECT_EQ(UniPlan::RunTopicAddCommand({"--topic", "LongerPascalCaseName",
                                           "--title", "x", "--repo-root",
                                           mRepoRoot.string()},
                                          mRepoRoot.string()),
              0);
}

TEST_F(FBundleTestFixture, TopicAddRequiresTitle)
{
    EXPECT_THROW(UniPlan::RunTopicAddCommand(
                     {"--topic", "Foo", "--repo-root", mRepoRoot.string()},
                     mRepoRoot.string()),
                 UniPlan::UsageError);
}

TEST_F(FBundleTestFixture, TopicAddRequiresTopic)
{
    EXPECT_THROW(UniPlan::RunTopicAddCommand(
                     {"--title", "x", "--repo-root", mRepoRoot.string()},
                     mRepoRoot.string()),
                 UniPlan::UsageError);
}

TEST_F(FBundleTestFixture, TopicAddProseFileFlagRoundTripsShellHostileContent)
{
    // Mirrors the v0.76.0 regression pattern: content containing $VAR,
    // $(cmd), backticks, and quotes must survive byte-identical through
    // the --X-file path.
    const std::string Hostile =
        "`$PATH`, $(pwd), and \"quoted\" text with em-dash \xE2\x80\x94";
    const fs::path SummaryPath = mRepoRoot / "summary.txt";
    {
        std::ofstream Out(SummaryPath, std::ios::binary);
        Out << Hostile;
    }

    const int Code = UniPlan::RunTopicAddCommand(
        {"--topic", "Hostile", "--title", "x", "--summary-file",
         SummaryPath.string(), "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    ASSERT_EQ(Code, 0);

    UniPlan::FTopicBundle Reloaded;
    ASSERT_TRUE(ReloadBundle("Hostile", Reloaded));
    EXPECT_EQ(Reloaded.mMetadata.mSummary, Hostile);
}

TEST_F(FBundleTestFixture, TopicAddFreshBundleFailsPhasesPresentValidator)
{
    // Documents the expected governance signal: a just-created bundle has
    // no phases and must fail `phases_present` ErrorMajor until `phase
    // add` seeds Phase 0. This is by design — see plan decision 1.
    ASSERT_EQ(
        UniPlan::RunTopicAddCommand({"--topic", "NoPhases", "--title", "x",
                                     "--repo-root", mRepoRoot.string()},
                                    mRepoRoot.string()),
        0);
    UniPlan::FTopicBundle Reloaded;
    ASSERT_TRUE(ReloadBundle("NoPhases", Reloaded));
    EXPECT_TRUE(Reloaded.mPhases.empty());
}

// ===================================================================
// v0.98.0 — priority-grouping / runbook / residual-risk CLI groups.
// 12 round-trip tests: 4 per group × 3 groups. Plus graph command
// tests live in UniPlanTestGraph.cpp.
// ===================================================================

// --- priority-grouping group ---------------------------------------

TEST_F(FBundleTestFixture, PriorityGroupingAddAppendsTypedEntry)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunPriorityGroupingAddCommand(
        {"--topic", "SampleTopic", "--id", "O1", "--phase-indices", "0,1",
         "--rule", "Foundation phases", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json["ok"].get<bool>());
    EXPECT_EQ(Json["target"].get<std::string>(), "priority_groupings[0]");

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mMetadata.mPriorityGroupings.size(), 1u);
    EXPECT_EQ(After.mMetadata.mPriorityGroupings[0].mID, "O1");
    EXPECT_EQ(After.mMetadata.mPriorityGroupings[0].mPhaseIndices.size(), 2u);
    EXPECT_EQ(After.mMetadata.mPriorityGroupings[0].mPhaseIndices[0], 0);
    EXPECT_EQ(After.mMetadata.mPriorityGroupings[0].mPhaseIndices[1], 1);
    EXPECT_EQ(After.mMetadata.mPriorityGroupings[0].mRule, "Foundation phases");
}

TEST_F(FBundleTestFixture, PriorityGroupingSetReplacesPhaseIndices)
{
    CopyFixture("SampleTopic");
    UniPlan::RunPriorityGroupingAddCommand(
        {"--topic", "SampleTopic", "--id", "O1", "--phase-index", "0", "--rule",
         "Old rule", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());

    StartCapture();
    const int Code = UniPlan::RunPriorityGroupingSetCommand(
        {"--topic", "SampleTopic", "--index", "0", "--phase-indices", "1,2",
         "--rule", "New rule", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mMetadata.mPriorityGroupings[0].mRule, "New rule");
    ASSERT_EQ(After.mMetadata.mPriorityGroupings[0].mPhaseIndices.size(), 2u);
    EXPECT_EQ(After.mMetadata.mPriorityGroupings[0].mPhaseIndices[0], 1);
    EXPECT_EQ(After.mMetadata.mPriorityGroupings[0].mPhaseIndices[1], 2);
}

TEST_F(FBundleTestFixture, PriorityGroupingRemoveShiftsIndices)
{
    CopyFixture("SampleTopic");
    for (const char *Id : {"O1", "O2", "O3"})
    {
        UniPlan::RunPriorityGroupingAddCommand(
            {"--topic", "SampleTopic", "--id", Id, "--phase-index", "0",
             "--rule", "r", "--repo-root", mRepoRoot.string()},
            mRepoRoot.string());
    }
    StartCapture();
    const int Code = UniPlan::RunPriorityGroupingRemoveCommand(
        {"--topic", "SampleTopic", "--index", "1", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mMetadata.mPriorityGroupings.size(), 2u);
    EXPECT_EQ(After.mMetadata.mPriorityGroupings[0].mID, "O1");
    EXPECT_EQ(After.mMetadata.mPriorityGroupings[1].mID, "O3"); // O2 removed
}

TEST_F(FBundleTestFixture, PriorityGroupingListEmitsEntries)
{
    CopyFixture("SampleTopic");
    UniPlan::RunPriorityGroupingAddCommand(
        {"--topic", "SampleTopic", "--id", "O1", "--phase-index", "0", "--rule",
         "r1", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    UniPlan::RunPriorityGroupingAddCommand(
        {"--topic", "SampleTopic", "--id", "O2", "--phase-indices", "1,2",
         "--rule", "r2", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());

    StartCapture();
    const int Code = UniPlan::RunPriorityGroupingListCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["count"].get<int>(), 2);
    EXPECT_EQ(Json["priority_groupings"][0]["id"].get<std::string>(), "O1");
    EXPECT_EQ(Json["priority_groupings"][1]["id"].get<std::string>(), "O2");
}

// --- runbook group -------------------------------------------------

TEST_F(FBundleTestFixture, RunbookAddWritesOrderedCommands)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunRunbookAddCommand(
        {"--topic", "SampleTopic", "--name", "Baseline-Alignment", "--trigger",
         "On validator drift", "--command", "uni-plan validate --strict",
         "--command", "uni-plan manifest list --missing-only", "--description",
         "Sweep baseline violations.", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mMetadata.mRunbooks.size(), 1u);
    EXPECT_EQ(After.mMetadata.mRunbooks[0].mName, "Baseline-Alignment");
    ASSERT_EQ(After.mMetadata.mRunbooks[0].mCommands.size(), 2u);
    EXPECT_EQ(After.mMetadata.mRunbooks[0].mCommands[0],
              "uni-plan validate --strict");
    EXPECT_EQ(After.mMetadata.mRunbooks[0].mCommands[1],
              "uni-plan manifest list --missing-only");
}

TEST_F(FBundleTestFixture, RunbookSetReplacesCommandsList)
{
    CopyFixture("SampleTopic");
    UniPlan::RunRunbookAddCommand({"--topic", "SampleTopic", "--name", "n",
                                   "--trigger", "t", "--command", "old-cmd",
                                   "--repo-root", mRepoRoot.string()},
                                  mRepoRoot.string());

    StartCapture();
    const int Code = UniPlan::RunRunbookSetCommand(
        {"--topic", "SampleTopic", "--index", "0", "--command", "new-1",
         "--command", "new-2", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mMetadata.mRunbooks[0].mCommands.size(), 2u);
    EXPECT_EQ(After.mMetadata.mRunbooks[0].mCommands[0], "new-1");
    EXPECT_EQ(After.mMetadata.mRunbooks[0].mCommands[1], "new-2");
}

TEST_F(FBundleTestFixture, RunbookRemoveDropsEntry)
{
    CopyFixture("SampleTopic");
    for (const char *Name : {"A", "B"})
    {
        UniPlan::RunRunbookAddCommand(
            {"--topic", "SampleTopic", "--name", Name, "--trigger", "t",
             "--command", "c", "--repo-root", mRepoRoot.string()},
            mRepoRoot.string());
    }

    StartCapture();
    const int Code = UniPlan::RunRunbookRemoveCommand(
        {"--topic", "SampleTopic", "--index", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mMetadata.mRunbooks.size(), 1u);
    EXPECT_EQ(After.mMetadata.mRunbooks[0].mName, "B");
}

TEST_F(FBundleTestFixture, RunbookListEmitsEntries)
{
    CopyFixture("SampleTopic");
    UniPlan::RunRunbookAddCommand(
        {"--topic", "SampleTopic", "--name", "N", "--trigger", "t", "--command",
         "c", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());

    StartCapture();
    const int Code = UniPlan::RunRunbookListCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["count"].get<int>(), 1);
    EXPECT_EQ(Json["runbooks"][0]["name"].get<std::string>(), "N");
}

// --- residual-risk group -------------------------------------------

TEST_F(FBundleTestFixture, ResidualRiskAddWritesAllFields)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunResidualRiskAddCommand(
        {"--topic", "SampleTopic", "--area", "Rendering", "--observation",
         "Screenshot parity unverified", "--why-deferred",
         "Windows host rebuild needed", "--target-phase", "phases[9]",
         "--recorded-date", "2026-04-21", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mMetadata.mResidualRisks.size(), 1u);
    EXPECT_EQ(After.mMetadata.mResidualRisks[0].mArea, "Rendering");
    EXPECT_EQ(After.mMetadata.mResidualRisks[0].mObservation,
              "Screenshot parity unverified");
    EXPECT_EQ(After.mMetadata.mResidualRisks[0].mTargetPhase, "phases[9]");
    EXPECT_EQ(After.mMetadata.mResidualRisks[0].mClosureSha, "");
}

TEST_F(FBundleTestFixture, ResidualRiskSetFillsClosureSha)
{
    CopyFixture("SampleTopic");
    UniPlan::RunResidualRiskAddCommand(
        {"--topic", "SampleTopic", "--area", "A", "--observation", "O",
         "--why-deferred", "W", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());

    StartCapture();
    const int Code = UniPlan::RunResidualRiskSetCommand(
        {"--topic", "SampleTopic", "--index", "0", "--closure-sha",
         "deadbeefcafe1234", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mMetadata.mResidualRisks[0].mClosureSha,
              "deadbeefcafe1234");
    EXPECT_EQ(After.mMetadata.mResidualRisks[0].mArea, "A"); // unchanged
}

TEST_F(FBundleTestFixture, ResidualRiskRemoveShiftsIndices)
{
    CopyFixture("SampleTopic");
    for (const char *Area : {"A", "B", "C"})
    {
        UniPlan::RunResidualRiskAddCommand(
            {"--topic", "SampleTopic", "--area", Area, "--observation", "O",
             "--why-deferred", "W", "--repo-root", mRepoRoot.string()},
            mRepoRoot.string());
    }

    StartCapture();
    const int Code = UniPlan::RunResidualRiskRemoveCommand(
        {"--topic", "SampleTopic", "--index", "1", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mMetadata.mResidualRisks.size(), 2u);
    EXPECT_EQ(After.mMetadata.mResidualRisks[0].mArea, "A");
    EXPECT_EQ(After.mMetadata.mResidualRisks[1].mArea, "C"); // B removed
}

TEST_F(FBundleTestFixture, ResidualRiskListEmitsEntries)
{
    CopyFixture("SampleTopic");
    UniPlan::RunResidualRiskAddCommand(
        {"--topic", "SampleTopic", "--area", "A", "--observation", "O",
         "--why-deferred", "W", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());

    StartCapture();
    const int Code = UniPlan::RunResidualRiskListCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["count"].get<int>(), 1);
    EXPECT_EQ(Json["residual_risks"][0]["area"].get<std::string>(), "A");
}
