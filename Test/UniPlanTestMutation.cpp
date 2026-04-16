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

// ===================================================================
// phase set
// ===================================================================

TEST_F(FBundleTestFixture, PhaseSetDoneAndRemaining)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--done", "W1 done",
         "--remaining", "W2-W3", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    EXPECT_EQ(Bundle.mPhases[1].mLifecycle.mDone, "W1 done");
    EXPECT_EQ(Bundle.mPhases[1].mLifecycle.mRemaining, "W2-W3");
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

// ===================================================================
// job set
// ===================================================================

TEST_F(FBundleTestFixture, JobSetStatusCompleted)
{
    CopyFixture("SampleTopic");
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
}

// ===================================================================
// task set
// ===================================================================

TEST_F(FBundleTestFixture, TaskSetEvidenceAndNotes)
{
    CopyFixture("SampleTopic");
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

// ===================================================================
// lane set
// ===================================================================

TEST_F(FBundleTestFixture, LaneSetChangesStatus)
{
    CopyFixture("SampleTopic");
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
