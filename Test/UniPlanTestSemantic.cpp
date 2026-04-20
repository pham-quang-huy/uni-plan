#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanJSONIO.h"
#include "UniPlanTypes.h"

#include <gtest/gtest.h>

// ===================================================================
// phase start
// ===================================================================

TEST_F(FBundleTestFixture, PhaseStartHappyPath)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, true);
    StartCapture();
    const int Code = UniPlan::RunPhaseStartCommand(
        {"--topic", "T", "--phase", "0", "--context", "test ctx", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["target"], "phases[0]");
    AssertNoLegacyPhasePath(Json["target"].get<std::string>());

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::InProgress);
    AssertISOTimestamp(Bundle.mPhases[0].mLifecycle.mStartedAt);
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mAgentContext, "test ctx");
    ASSERT_FALSE(Bundle.mChangeLogs.empty());
    EXPECT_EQ(Bundle.mChangeLogs.back().mAffected, "phases[0]");
    AssertNoLegacyPhasePath(Bundle.mChangeLogs.back().mAffected);
}

TEST_F(FBundleTestFixture, PhaseStartRejectsNonNotStarted)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);
    StartCapture();
    const int Code = UniPlan::RunPhaseStartCommand(
        {"--topic", "T", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
    EXPECT_TRUE(mCapturedStderr.find("expected not_started") !=
                std::string::npos);
}

TEST_F(FBundleTestFixture, PhaseStartRejectsEmptyDesign)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, false);
    StartCapture();
    const int Code = UniPlan::RunPhaseStartCommand(
        {"--topic", "T", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
    EXPECT_TRUE(mCapturedStderr.find("design material") != std::string::npos);
}

TEST_F(FBundleTestFixture, PhaseStartAutoCascadesTopicStatus)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::NotStarted, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    StartCapture();
    const int Code = UniPlan::RunPhaseStartCommand(
        {"--topic", "T", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mStatus, UniPlan::ETopicStatus::InProgress);
}

// ===================================================================
// phase complete
// ===================================================================

TEST_F(FBundleTestFixture, PhaseCompleteHappyPath)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress);
    StartCapture();
    const int Code = UniPlan::RunPhaseCompleteCommand(
        {"--topic", "T", "--phase", "0", "--done", "All done", "--verification",
         "Build passes", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["target"], "phases[0]");
    AssertNoLegacyPhasePath(Json["target"].get<std::string>());

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::Completed);
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mDone, "All done");
    AssertISOTimestamp(Bundle.mPhases[0].mLifecycle.mCompletedAt);
    EXPECT_FALSE(Bundle.mVerifications.empty());
    ASSERT_FALSE(Bundle.mChangeLogs.empty());
    bool bFoundPhaseClosure = false;
    for (const auto &Entry : Bundle.mChangeLogs)
    {
        if (Entry.mAffected == "phases[0]")
        {
            bFoundPhaseClosure = true;
            AssertNoLegacyPhasePath(Entry.mAffected);
        }
    }
    EXPECT_TRUE(bFoundPhaseClosure);
}

// v0.88.0 lifecycle gate: code-bearing phase cannot complete with
// empty file_manifest unless explicit opt-out is set.
TEST_F(FBundleTestFixture, PhaseCompleteRejectsCodeBearingWithEmptyManifest)
{
    // InPopulateDesign=true sets code_entity_contract → code-bearing.
    // No manifest entries → gate fires → exit 1, phase remains
    // in_progress.
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);
    StartCapture();
    const int Code = UniPlan::RunPhaseCompleteCommand(
        {"--topic", "T", "--phase", "0", "--done", "All done", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
    EXPECT_NE(mCapturedStderr.find("file_manifest"), std::string::npos)
        << "must explain why the close was refused";
    EXPECT_NE(mCapturedStderr.find("manifest suggest"), std::string::npos)
        << "must point at the backfill remediation";

    // Phase still in_progress on disk — the refusal is atomic.
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::InProgress);
}

TEST_F(FBundleTestFixture, PhaseCompleteAllowsCodeBearingWithOptOut)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mbNoFileManifest = true;
    Bundle.mPhases[0].mFileManifestSkipReason =
        "Doc-only phase: no code touched";
    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunPhaseCompleteCommand(
        {"--topic", "T", "--phase", "0", "--done", "All done", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("T", After));
    EXPECT_EQ(After.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::Completed);
}

TEST_F(FBundleTestFixture, PhaseCompleteAllowsCodeBearingWithManifestEntries)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FFileManifestItem Item;
    Item.mFilePath = "Engine/Foo.cpp";
    Item.mAction = UniPlan::EFileAction::Modify;
    Item.mDescription = "Edited the foo path";
    Bundle.mPhases[0].mFileManifest.push_back(std::move(Item));
    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunPhaseCompleteCommand(
        {"--topic", "T", "--phase", "0", "--done", "All done", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
}

TEST_F(FBundleTestFixture, PhaseCompleteRejectsNotInProgress)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted);
    StartCapture();
    const int Code = UniPlan::RunPhaseCompleteCommand(
        {"--topic", "T", "--phase", "0", "--done", "Done", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, PhaseCompleteAutoCascadesTopicCompleted)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress);
    StartCapture();
    const int Code = UniPlan::RunPhaseCompleteCommand(
        {"--topic", "T", "--phase", "0", "--done", "Done", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mStatus, UniPlan::ETopicStatus::Completed);
}

// ===================================================================
// phase block / unblock
// ===================================================================

TEST_F(FBundleTestFixture, PhaseBlockHappyPath)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress);
    StartCapture();
    const int Code = UniPlan::RunPhaseBlockCommand(
        {"--topic", "T", "--phase", "0", "--reason", "Waiting", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::Blocked);
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mBlockers, "Waiting");
}

TEST_F(FBundleTestFixture, PhaseBlockRejectsNotInProgress)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted);
    StartCapture();
    const int Code = UniPlan::RunPhaseBlockCommand(
        {"--topic", "T", "--phase", "0", "--reason", "X", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, PhaseUnblockHappyPath)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::Blocked);
    StartCapture();
    const int Code = UniPlan::RunPhaseUnblockCommand(
        {"--topic", "T", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::InProgress);
    EXPECT_TRUE(Bundle.mPhases[0].mLifecycle.mBlockers.empty());
}

TEST_F(FBundleTestFixture, PhaseUnblockRejectsNotBlocked)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress);
    StartCapture();
    const int Code = UniPlan::RunPhaseUnblockCommand(
        {"--topic", "T", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// phase progress
// ===================================================================

TEST_F(FBundleTestFixture, PhaseProgressUpdatesDoneRemaining)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress);
    StartCapture();
    const int Code = UniPlan::RunPhaseProgressCommand(
        {"--topic", "T", "--phase", "0", "--done", "W1 done", "--remaining",
         "W2", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mDone, "W1 done");
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mRemaining, "W2");
}

TEST_F(FBundleTestFixture, PhaseProgressRejectsNotInProgress)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted);
    StartCapture();
    const int Code = UniPlan::RunPhaseProgressCommand(
        {"--topic", "T", "--phase", "0", "--done", "X", "--remaining", "Y",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// phase complete-jobs
// ===================================================================

TEST_F(FBundleTestFixture, PhaseCompleteJobsBulkCompletes)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunPhaseCompleteJobsCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    for (const auto &Job : Bundle.mPhases[1].mJobs)
    {
        EXPECT_EQ(Job.mStatus, UniPlan::EExecutionStatus::Completed);
    }
}

TEST_F(FBundleTestFixture, PhaseCompleteJobsRejectsNotInProgress)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted);
    StartCapture();
    const int Code = UniPlan::RunPhaseCompleteJobsCommand(
        {"--topic", "T", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// topic start
// ===================================================================

TEST_F(FBundleTestFixture, TopicStartHappyPath)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::NotStarted, 1);
    StartCapture();
    const int Code = UniPlan::RunTopicStartCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mStatus, UniPlan::ETopicStatus::InProgress);
}

TEST_F(FBundleTestFixture, TopicStartRejectsNonNotStarted)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1);
    StartCapture();
    const int Code = UniPlan::RunTopicStartCommand(
        {"--topic", "T", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// topic complete
// ===================================================================

TEST_F(FBundleTestFixture, TopicCompleteHappyPath)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::Completed);
    StartCapture();
    const int Code = UniPlan::RunTopicCompleteCommand(
        {"--topic", "T", "--verification", "All phases pass", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mStatus, UniPlan::ETopicStatus::Completed);
}

TEST_F(FBundleTestFixture, TopicCompleteRejectsIncompletePhases)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTopicCompleteCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
    EXPECT_TRUE(mCapturedStderr.find("not completed") != std::string::npos);
}

// ===================================================================
// topic block
// ===================================================================

TEST_F(FBundleTestFixture, TopicBlockHappyPath)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1);
    StartCapture();
    const int Code =
        UniPlan::RunTopicBlockCommand({"--topic", "T", "--reason", "Ext dep",
                                       "--repo-root", mRepoRoot.string()},
                                      mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mStatus, UniPlan::ETopicStatus::Blocked);
}

TEST_F(FBundleTestFixture, TopicBlockRejectsNotInProgress)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::NotStarted, 1);
    StartCapture();
    const int Code = UniPlan::RunTopicBlockCommand(
        {"--topic", "T", "--reason", "X", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}
