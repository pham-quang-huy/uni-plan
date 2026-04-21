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

// v0.101.0 lifecycle gate: phase complete refuses when any descendant
// (lane, job, or task) is not terminal.
TEST_F(FBundleTestFixture, PhaseCompleteRejectsIncompleteLane)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FLaneRecord Lane;
    Lane.mStatus = UniPlan::EExecutionStatus::InProgress;
    Lane.mScope = "Lane 0 scope";
    Lane.mExitCriteria = "All green";
    Bundle.mPhases[0].mLanes.push_back(std::move(Lane));
    Bundle.mPhases[0].mbNoFileManifest = true;
    Bundle.mPhases[0].mFileManifestSkipReason = "doc-only";
    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunPhaseCompleteCommand(
        {"--topic", "T", "--phase", "0", "--done", "All done", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
    EXPECT_NE(mCapturedStderr.find("execution descendants"), std::string::npos);
    EXPECT_NE(mCapturedStderr.find("lanes[0]"), std::string::npos);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("T", After));
    EXPECT_EQ(After.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::InProgress);
}

TEST_F(FBundleTestFixture, PhaseCompleteRejectsIncompleteTask)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FJobRecord Job;
    Job.mWave = 1;
    Job.mLane = 0;
    Job.mStatus = UniPlan::EExecutionStatus::Completed;
    Job.mScope = "job scope";
    Job.mExitCriteria = "done";
    UniPlan::FTaskRecord Task;
    Task.mStatus = UniPlan::EExecutionStatus::InProgress;
    Task.mDescription = "unfinished task";
    Task.mEvidence = "";
    Job.mTasks.push_back(std::move(Task));
    Bundle.mPhases[0].mJobs.push_back(std::move(Job));
    Bundle.mPhases[0].mbNoFileManifest = true;
    Bundle.mPhases[0].mFileManifestSkipReason = "doc-only";
    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunPhaseCompleteCommand(
        {"--topic", "T", "--phase", "0", "--done", "All done", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
    EXPECT_NE(mCapturedStderr.find("tasks[0]"), std::string::npos);
}

TEST_F(FBundleTestFixture, PhaseCompleteAllowsCanceledDescendants)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FJobRecord Job;
    Job.mWave = 1;
    Job.mLane = 0;
    Job.mStatus = UniPlan::EExecutionStatus::Canceled;
    Job.mScope = "canceled job";
    Job.mExitCriteria = "n/a";
    Bundle.mPhases[0].mJobs.push_back(std::move(Job));
    Bundle.mPhases[0].mbNoFileManifest = true;
    Bundle.mPhases[0].mFileManifestSkipReason = "doc-only";
    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunPhaseCompleteCommand(
        {"--topic", "T", "--phase", "0", "--done", "All done", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0) << mCapturedStderr;
}

// v0.101.0 — lane complete semantic command
TEST_F(FBundleTestFixture, LaneCompleteHappyPath)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FLaneRecord Lane;
    Lane.mStatus = UniPlan::EExecutionStatus::InProgress;
    Lane.mScope = "Lane 0";
    Lane.mExitCriteria = "done";
    Bundle.mPhases[0].mLanes.push_back(std::move(Lane));
    // Job on this lane, already complete.
    UniPlan::FJobRecord Job;
    Job.mWave = 1;
    Job.mLane = 0;
    Job.mStatus = UniPlan::EExecutionStatus::Completed;
    Job.mScope = "job scope";
    Job.mExitCriteria = "done";
    Bundle.mPhases[0].mJobs.push_back(std::move(Job));
    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunLaneCompleteCommand(
        {"--topic", "T", "--phase", "0", "--lane", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0) << mCapturedStderr;
    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("T", After));
    EXPECT_EQ(After.mPhases[0].mLanes[0].mStatus,
              UniPlan::EExecutionStatus::Completed);
}

TEST_F(FBundleTestFixture, LaneCompleteRejectsIncompleteJobOnLane)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    UniPlan::FLaneRecord Lane;
    Lane.mStatus = UniPlan::EExecutionStatus::InProgress;
    Lane.mScope = "Lane 0";
    Lane.mExitCriteria = "done";
    Bundle.mPhases[0].mLanes.push_back(std::move(Lane));
    UniPlan::FJobRecord Job;
    Job.mWave = 1;
    Job.mLane = 0;
    Job.mStatus = UniPlan::EExecutionStatus::InProgress;
    Job.mScope = "unfinished job";
    Job.mExitCriteria = "done";
    Bundle.mPhases[0].mJobs.push_back(std::move(Job));
    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunLaneCompleteCommand(
        {"--topic", "T", "--phase", "0", "--lane", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
    EXPECT_NE(mCapturedStderr.find("jobs on this lane"), std::string::npos);
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
// phase cancel (v0.89.0)
// ===================================================================

TEST_F(FBundleTestFixture, PhaseCancelHappyPathFromNotStarted)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    StartCapture();
    const int Code = UniPlan::RunPhaseCancelCommand(
        {"--topic", "T", "--phase", "0", "--reason", "Superseded by phases[21]",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["target"], "phases[0]");

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::Canceled);
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mBlockers,
              "Superseded by phases[21]");
    // Canceled phases do NOT stamp completed_at — the phase never
    // actually finished.
    EXPECT_TRUE(Bundle.mPhases[0].mLifecycle.mCompletedAt.empty());

    // Auto-changelog should record the cancellation with reason.
    ASSERT_FALSE(Bundle.mChangeLogs.empty());
    bool bFoundCancelEntry = false;
    for (const auto &Entry : Bundle.mChangeLogs)
    {
        if (Entry.mAffected == "phases[0]" &&
            Entry.mChange.find("canceled") != std::string::npos &&
            Entry.mChange.find("Superseded by phases[21]") != std::string::npos)
        {
            bFoundCancelEntry = true;
        }
    }
    EXPECT_TRUE(bFoundCancelEntry)
        << "auto-changelog must record the cancellation reason";
}

TEST_F(FBundleTestFixture, PhaseCancelHappyPathFromInProgress)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress);
    StartCapture();
    const int Code = UniPlan::RunPhaseCancelCommand(
        {"--topic", "T", "--phase", "0", "--reason", "Scope dropped",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::Canceled);
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mBlockers, "Scope dropped");
}

TEST_F(FBundleTestFixture, PhaseCancelHappyPathFromBlocked)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::Blocked);
    StartCapture();
    const int Code = UniPlan::RunPhaseCancelCommand(
        {"--topic", "T", "--phase", "0", "--reason", "Blocker now permanent",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::Canceled);
    // Reason replaces prior blocker text.
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mBlockers, "Blocker now permanent");
}

TEST_F(FBundleTestFixture, PhaseCancelRejectsCompleted)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::Completed);
    StartCapture();
    const int Code = UniPlan::RunPhaseCancelCommand(
        {"--topic", "T", "--phase", "0", "--reason", "Should fail",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
    EXPECT_NE(mCapturedStderr.find("completed"), std::string::npos)
        << "must explain why the cancel was refused";

    // Phase still completed on disk — the refusal is atomic.
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::Completed);
}

TEST_F(FBundleTestFixture, PhaseCancelRejectsAlreadyCanceled)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    // First cancel: succeeds.
    StartCapture();
    const int Code1 = UniPlan::RunPhaseCancelCommand(
        {"--topic", "T", "--phase", "0", "--reason", "First", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code1, 0);

    // Second cancel: rejected.
    StartCapture();
    const int Code2 = UniPlan::RunPhaseCancelCommand(
        {"--topic", "T", "--phase", "0", "--reason", "Second", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code2, 1);
    EXPECT_NE(mCapturedStderr.find("already canceled"), std::string::npos);

    // Reason on disk should still be "First" — second call did not mutate.
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_EQ(Bundle.mPhases[0].mLifecycle.mBlockers, "First");
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
    EXPECT_TRUE(mCapturedStderr.find("not terminal") != std::string::npos);
}

// v0.89.0: canceled phases count as terminal for the topic-complete gate.
TEST_F(FBundleTestFixture, TopicCompleteAcceptsCanceledPhases)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::Completed);
    // Cancel phase 1 — reuse the happy path by first flipping to
    // NotStarted then canceling (canceling from Completed is rejected
    // by design).
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[1].mLifecycle.mStatus =
        UniPlan::EExecutionStatus::NotStarted;
    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int CancelCode = UniPlan::RunPhaseCancelCommand(
        {"--topic", "T", "--phase", "1", "--reason", "Scope moved",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(CancelCode, 0);

    // The cancel cascade should have already auto-completed the topic —
    // one Completed + one Canceled = all terminal with at least one
    // shipped.
    UniPlan::FTopicBundle Final;
    ASSERT_TRUE(ReloadBundle("T", Final));
    EXPECT_EQ(Final.mStatus, UniPlan::ETopicStatus::Completed)
        << "phase cancel should auto-cascade topic to Completed when all "
           "phases are terminal and at least one shipped";
}

// v0.89.0: all-canceled topic does NOT auto-cascade to Completed because
// nothing was ever delivered. Caller must decide (topic complete / topic
// block / reactivate).
TEST_F(FBundleTestFixture, PhaseCancelDoesNotAutoCascadeAllCanceledTopic)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    StartCapture();
    const int Code = UniPlan::RunPhaseCancelCommand(
        {"--topic", "T", "--phase", "0", "--reason", "Whole topic dropped",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    // Topic stays in_progress — nothing shipped, so auto-completion
    // would be semantically wrong.
    EXPECT_EQ(Bundle.mStatus, UniPlan::ETopicStatus::InProgress);
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

// ===================================================================
// v0.102.0 — phase sync-execution (child→parent rollup)
// ===================================================================

namespace
{
// Helper: build a phase with a single lane + one job + N tasks, all on
// that one lane. Task statuses are provided by the caller.
UniPlan::FTopicBundle
MakeSyncFixture(const std::vector<UniPlan::EExecutionStatus> &InTaskStatuses,
                UniPlan::EExecutionStatus InJobStart =
                    UniPlan::EExecutionStatus::InProgress,
                UniPlan::EExecutionStatus InLaneStart =
                    UniPlan::EExecutionStatus::InProgress)
{
    UniPlan::FTopicBundle B;
    B.mTopicKey = "Sync";
    B.mStatus = UniPlan::ETopicStatus::InProgress;
    B.mMetadata.mTitle = "Sync fixture";
    B.mMetadata.mGoals = "g";
    B.mMetadata.mNonGoals = "ng";
    UniPlan::FNextActionEntry NA;
    NA.mOrder = 1;
    NA.mStatement = "step";
    B.mNextActions.push_back(NA);

    UniPlan::FPhaseRecord P;
    P.mScope = "Phase 0";
    P.mLifecycle.mStatus = UniPlan::EExecutionStatus::InProgress;
    P.mLifecycle.mStartedAt = "2026-01-15T09:00:00Z";

    UniPlan::FLaneRecord L;
    L.mStatus = InLaneStart;
    L.mScope = "Lane scope";
    L.mExitCriteria = "done";
    P.mLanes.push_back(L);

    UniPlan::FJobRecord J;
    J.mWave = 1;
    J.mLane = 0;
    J.mStatus = InJobStart;
    J.mScope = "Job scope";
    J.mExitCriteria = "done";
    for (auto TS : InTaskStatuses)
    {
        UniPlan::FTaskRecord T;
        T.mStatus = TS;
        T.mDescription = "task";
        T.mEvidence = "";
        J.mTasks.push_back(T);
    }
    P.mJobs.push_back(J);

    B.mPhases.push_back(P);
    return B;
}

void WriteSync(const fs::path &InRepoRoot, const UniPlan::FTopicBundle &InB)
{
    const fs::path Path = InRepoRoot / "Docs" / "Plans" / "Sync.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(InB, Path, Error)) << Error;
}
} // namespace

TEST_F(FBundleTestFixture, SyncExecutionHappyPathPropagatesUp)
{
    // All tasks Completed → job flips Completed → lane flips Completed.
    const auto B = MakeSyncFixture({UniPlan::EExecutionStatus::Completed,
                                    UniPlan::EExecutionStatus::Completed});
    WriteSync(mRepoRoot, B);

    StartCapture();
    const int Code = UniPlan::RunPhaseSyncExecutionCommand(
        {"--topic", "Sync", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto J = ParseCapturedJSON();
    EXPECT_EQ(J["schema"], "uni-plan-sync-execution-v1");
    EXPECT_FALSE(J["dry_run"].get<bool>());
    EXPECT_EQ(J["summary"]["jobs_flipped"].get<int>(), 1);
    EXPECT_EQ(J["summary"]["lanes_flipped"].get<int>(), 1);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("Sync", After));
    EXPECT_EQ(After.mPhases[0].mJobs[0].mStatus,
              UniPlan::EExecutionStatus::Completed);
    EXPECT_EQ(After.mPhases[0].mLanes[0].mStatus,
              UniPlan::EExecutionStatus::Completed);
    // Phase is NEVER flipped by sync-execution.
    EXPECT_EQ(After.mPhases[0].mLifecycle.mStatus,
              UniPlan::EExecutionStatus::InProgress);
}

TEST_F(FBundleTestFixture, SyncExecutionDryRunMakesNoOnDiskChanges)
{
    const auto B = MakeSyncFixture({UniPlan::EExecutionStatus::Completed});
    WriteSync(mRepoRoot, B);
    const std::string BeforeBytes = ReadBundleFile("Sync");

    StartCapture();
    const int Code = UniPlan::RunPhaseSyncExecutionCommand(
        {"--topic", "Sync", "--phase", "0", "--dry-run", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto J = ParseCapturedJSON();
    EXPECT_TRUE(J["dry_run"].get<bool>());
    EXPECT_EQ(J["summary"]["jobs_flipped"].get<int>(), 1);

    // On-disk must be byte-identical.
    EXPECT_EQ(ReadBundleFile("Sync"), BeforeBytes);
}

TEST_F(FBundleTestFixture, SyncExecutionMixedStatusesSkipsJob)
{
    // One task InProgress → job not rolled up. Lane depends on job, so
    // lane also stays put.
    const auto B = MakeSyncFixture({UniPlan::EExecutionStatus::Completed,
                                    UniPlan::EExecutionStatus::InProgress});
    WriteSync(mRepoRoot, B);

    StartCapture();
    const int Code = UniPlan::RunPhaseSyncExecutionCommand(
        {"--topic", "Sync", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto J = ParseCapturedJSON();
    EXPECT_EQ(J["summary"]["jobs_flipped"].get<int>(), 0);
    EXPECT_EQ(J["summary"]["lanes_flipped"].get<int>(), 0);
}

TEST_F(FBundleTestFixture, SyncExecutionAllCanceledTasksRollupAsCanceled)
{
    const auto B = MakeSyncFixture({UniPlan::EExecutionStatus::Canceled,
                                    UniPlan::EExecutionStatus::Canceled});
    WriteSync(mRepoRoot, B);

    StartCapture();
    const int Code = UniPlan::RunPhaseSyncExecutionCommand(
        {"--topic", "Sync", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("Sync", After));
    EXPECT_EQ(After.mPhases[0].mJobs[0].mStatus,
              UniPlan::EExecutionStatus::Canceled);
    EXPECT_EQ(After.mPhases[0].mLanes[0].mStatus,
              UniPlan::EExecutionStatus::Canceled);
}

TEST_F(FBundleTestFixture, SyncExecutionMixedCompletedAndCanceledIsCompleted)
{
    // ≥1 Completed task → job flips to Completed (not Canceled), because
    // real work happened even if some subtasks were dropped.
    const auto B = MakeSyncFixture({UniPlan::EExecutionStatus::Completed,
                                    UniPlan::EExecutionStatus::Canceled});
    WriteSync(mRepoRoot, B);

    StartCapture();
    const int Code = UniPlan::RunPhaseSyncExecutionCommand(
        {"--topic", "Sync", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("Sync", After));
    EXPECT_EQ(After.mPhases[0].mJobs[0].mStatus,
              UniPlan::EExecutionStatus::Completed);
}

TEST_F(FBundleTestFixture, SyncExecutionSkipsAlreadyTerminalParent)
{
    // Job already Completed → skipped even though a task is NotStarted.
    // (The completed_jobs_have_completed_tasks validator surfaces this
    // inconsistency separately; sync-execution is non-destructive and
    // does not downgrade.)
    const auto B = MakeSyncFixture({UniPlan::EExecutionStatus::NotStarted},
                                   UniPlan::EExecutionStatus::Completed);
    WriteSync(mRepoRoot, B);

    StartCapture();
    const int Code = UniPlan::RunPhaseSyncExecutionCommand(
        {"--topic", "Sync", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("Sync", After));
    // Parent stayed Completed.
    EXPECT_EQ(After.mPhases[0].mJobs[0].mStatus,
              UniPlan::EExecutionStatus::Completed);
    // Task unchanged.
    EXPECT_EQ(After.mPhases[0].mJobs[0].mTasks[0].mStatus,
              UniPlan::EExecutionStatus::NotStarted);
}

TEST_F(FBundleTestFixture, SyncExecutionIsIdempotentOnRerun)
{
    const auto B = MakeSyncFixture({UniPlan::EExecutionStatus::Completed});
    WriteSync(mRepoRoot, B);

    // First run: flips job and lane.
    StartCapture();
    UniPlan::RunPhaseSyncExecutionCommand(
        {"--topic", "Sync", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto First = ParseCapturedJSON();
    EXPECT_EQ(First["summary"]["jobs_flipped"].get<int>(), 1);
    EXPECT_EQ(First["summary"]["lanes_flipped"].get<int>(), 1);

    // Second run: no changes — parents already terminal, skipped.
    StartCapture();
    UniPlan::RunPhaseSyncExecutionCommand(
        {"--topic", "Sync", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    const auto Second = ParseCapturedJSON();
    EXPECT_EQ(Second["summary"]["jobs_flipped"].get<int>(), 0);
    EXPECT_EQ(Second["summary"]["lanes_flipped"].get<int>(), 0);
    EXPECT_EQ(Second["changes"].size(), 0u);
}
