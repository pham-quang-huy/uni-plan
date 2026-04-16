#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanTypes.h"

#include <gtest/gtest.h>

// ===================================================================
// phase log
// ===================================================================

TEST_F(FBundleTestFixture, PhaseLogAppendsChangelog)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t CountBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunPhaseLogCommand(
        {"--topic", "SampleTopic", "--phase", "0", "--change", "Test log entry",
         "--type", "feat", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json["ok"].get<bool>());

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mChangeLogs.size(), CountBefore + 1);
    EXPECT_EQ(After.mChangeLogs.back().mChange, "Test log entry");
    EXPECT_EQ(After.mChangeLogs.back().mPhase, 0);
}

TEST_F(FBundleTestFixture, PhaseLogRejectsOutOfRange)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunPhaseLogCommand(
        {"--topic", "SampleTopic", "--phase", "99", "--change", "Test",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// phase verify
// ===================================================================

TEST_F(FBundleTestFixture, PhaseVerifyAppendsVerification)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t CountBefore = Before.mVerifications.size();

    StartCapture();
    const int Code = UniPlan::RunPhaseVerifyCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--check", "Smoke test",
         "--result", "pass", "--detail", "All green", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mVerifications.size(), CountBefore + 1);
    EXPECT_EQ(After.mVerifications.back().mCheck, "Smoke test");
    EXPECT_EQ(After.mVerifications.back().mResult, "pass");
    EXPECT_EQ(After.mVerifications.back().mPhase, 1);
}

TEST_F(FBundleTestFixture, PhaseVerifyRejectsOutOfRange)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunPhaseVerifyCommand(
        {"--topic", "SampleTopic", "--phase", "99", "--check", "Test",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}
