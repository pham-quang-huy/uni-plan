#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanTypes.h"

#include <gtest/gtest.h>

// ===================================================================
// topic list
// ===================================================================

TEST_F(FBundleTestFixture, TopicListJsonReturnsTopics)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTopicCommand(
        {"list", "--repo-root", mRepoRoot.string()}, mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["schema"], "uni-plan-topic-list-v1");
    EXPECT_EQ(Json["count"], 1);
    EXPECT_EQ(Json["topics"][0]["topic"], "SampleTopic");
}

TEST_F(FBundleTestFixture, TopicListEmptyRepo)
{
    StartCapture();
    const int Code = UniPlan::RunTopicCommand(
        {"list", "--repo-root", mRepoRoot.string()}, mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["count"], 0);
}

// ===================================================================
// topic get
// ===================================================================

TEST_F(FBundleTestFixture, TopicGetJsonHasFields)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTopicCommand(
        {"get", "--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["schema"], "uni-plan-topic-get-v1");
    EXPECT_EQ(Json["topic"], "SampleTopic");
    EXPECT_EQ(Json["status"], "in_progress");
    EXPECT_TRUE(Json.contains("phase_summary"));
    EXPECT_EQ(Json["phase_count"], 3);
    EXPECT_EQ(Json["title"], "Sample Topic for Testing and Reference");
}

TEST_F(FBundleTestFixture, TopicGetMissingTopicFails)
{
    StartCapture();
    const int Code = UniPlan::RunTopicCommand(
        {"get", "--topic", "NoSuchTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// topic status
// ===================================================================

TEST_F(FBundleTestFixture, TopicStatusJsonHasCounts)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTopicCommand(
        {"status", "--repo-root", mRepoRoot.string()}, mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["total"], 1);
    EXPECT_TRUE(Json.contains("counts"));
    EXPECT_EQ(Json["counts"]["in_progress"], 1);
}

// ===================================================================
// phase list
// ===================================================================

TEST_F(FBundleTestFixture, PhaseListJsonReturnsPhases)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"list", "--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["count"], 3);
    EXPECT_TRUE(Json.contains("phases"));
}

// ===================================================================
// phase get
// ===================================================================

TEST_F(FBundleTestFixture, PhaseGetJsonHasFields)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"get", "--topic", "SampleTopic", "--phase", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["phase_index"], 0);
    EXPECT_EQ(Json["status"], "completed");
}

TEST_F(FBundleTestFixture, PhaseGetOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"get", "--topic", "SampleTopic", "--phase", "99", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// phase next
// ===================================================================

TEST_F(FBundleTestFixture, PhaseNextFindsNotStarted)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"next", "--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["phase_index"], 2);
}

TEST_F(FBundleTestFixture, PhaseNextAllStartedReturnsNegative)
{
    CreateMinimalFixture("AllStarted", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::InProgress);
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"next", "--topic", "AllStarted", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["phase_index"], -1);
}

// ===================================================================
// phase readiness
// ===================================================================

TEST_F(FBundleTestFixture, PhaseReadinessReportsGates)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"readiness", "--topic", "SampleTopic", "--phase", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json.contains("gates"));
    EXPECT_FALSE(Json["gates"].empty());
}

// ===================================================================
// phase wave-status
// ===================================================================

TEST_F(FBundleTestFixture, PhaseWaveStatusReportsWaves)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"wave-status", "--topic", "SampleTopic", "--phase", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json.contains("waves"));
    EXPECT_TRUE(Json.contains("current_wave"));
}

TEST_F(FBundleTestFixture, PhaseWaveStatusOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"wave-status", "--topic", "SampleTopic", "--phase", "99",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, PhaseReadinessOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundlePhaseCommand(
        {"readiness", "--topic", "SampleTopic", "--phase", "99", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// changelog
// ===================================================================

TEST_F(FBundleTestFixture, ChangelogJsonReturnsEntries)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundleChangelogCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_GT(Json["count"].get<int>(), 0);
}

// ===================================================================
// verification
// ===================================================================

TEST_F(FBundleTestFixture, VerificationJsonReturnsEntries)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundleVerificationCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_GT(Json["count"].get<int>(), 0);
}

// ===================================================================
// timeline
// ===================================================================

TEST_F(FBundleTestFixture, TimelineJsonReturnsMergedEntries)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundleTimelineCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_GT(Json["count"].get<int>(), 0);
}

// ===================================================================
// blockers
// ===================================================================

TEST_F(FBundleTestFixture, BlockersJsonReturnsResults)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundleBlockersCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
}

// ===================================================================
// validate
// ===================================================================

TEST_F(FBundleTestFixture, ValidatePassesOnValidFixture)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunBundleValidateCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
}

TEST_F(FBundleTestFixture, ValidateMissingTopicFails)
{
    StartCapture();
    const int Code = UniPlan::RunBundleValidateCommand(
        {"--topic", "NoSuchTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}
