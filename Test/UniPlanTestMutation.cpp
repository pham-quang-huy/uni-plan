#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanTypes.h"

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
    StartCapture();
    const int Code = UniPlan::RunTopicSetCommand(
        {"--topic", "SampleTopic", "--status", "garbage", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, TopicSetNextActions)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTopicSetCommand(
        {"--topic", "SampleTopic", "--next-actions", "Do stuff", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    EXPECT_EQ(Bundle.mNextActions, "Do stuff");
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
        {"--topic", "SampleTopic", "--goals", "G1\nG2", "--risks", "R1",
         "--dependencies", "Dep1", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mMetadata.mGoals, "G1\nG2");
    EXPECT_EQ(After.mMetadata.mRisks, "R1");
    EXPECT_EQ(After.mMetadata.mDependencies, "Dep1");
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
        {"--topic", "SampleTopic", "--index", "0", "--phase", "2",
         "--change", "Retargeted entry", "--type", "fix", "--affected",
         "phases[2].jobs[0]", "--repo-root", mRepoRoot.string()},
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
