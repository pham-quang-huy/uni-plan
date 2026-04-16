#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanTypes.h"

#include <gtest/gtest.h>

// ===================================================================
// testing add
// ===================================================================

TEST_F(FBundleTestFixture, TestingAddAppendsRecord)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t CountBefore = Before.mPhases[1].mTesting.size();
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunTestingAddCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--session", "smoke",
         "--actor", "ai", "--step", "Build", "--action", "cmake build",
         "--expected", "0 errors", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json["ok"].get<bool>());
    EXPECT_EQ(Json["target"], "phase[1].testing");
    AssertNoLegacyPhasePath(Json["target"].get<std::string>());

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mTesting.size(), CountBefore + 1);
    EXPECT_EQ(After.mPhases[1].mTesting.back().mStep, "Build");
    EXPECT_EQ(After.mPhases[1].mTesting.back().mActor,
              UniPlan::ETestingActor::AI);
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
    ASSERT_FALSE(After.mChangeLogs.empty());
    EXPECT_EQ(After.mChangeLogs.back().mAffected, "phase[1].testing");
    AssertNoLegacyPhasePath(After.mChangeLogs.back().mAffected);
}

TEST_F(FBundleTestFixture, TestingAddInvalidActorFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTestingAddCommand(
        {"--topic", "SampleTopic", "--phase", "0", "--actor", "robot", "--step",
         "X", "--action", "Y", "--expected", "Z", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, TestingAddOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTestingAddCommand(
        {"--topic", "SampleTopic", "--phase", "99", "--step", "X", "--action",
         "Y", "--expected", "Z", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// manifest add
// ===================================================================

TEST_F(FBundleTestFixture, ManifestAddAppendsItem)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t CountBefore = Before.mPhases[1].mFileManifest.size();
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunManifestAddCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--file", "Source/New.cpp",
         "--action", "create", "--description", "New source file",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["target"], "phase[1].file_manifest");
    AssertNoLegacyPhasePath(Json["target"].get<std::string>());

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mFileManifest.size(), CountBefore + 1);
    EXPECT_EQ(After.mPhases[1].mFileManifest.back().mFilePath,
              "Source/New.cpp");
    EXPECT_EQ(After.mPhases[1].mFileManifest.back().mAction,
              UniPlan::EFileAction::Create);
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
    ASSERT_FALSE(After.mChangeLogs.empty());
    EXPECT_EQ(After.mChangeLogs.back().mAffected, "phase[1].file_manifest");
    AssertNoLegacyPhasePath(After.mChangeLogs.back().mAffected);
}

TEST_F(FBundleTestFixture, ManifestAddInvalidActionFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunManifestAddCommand(
        {"--topic", "SampleTopic", "--phase", "0", "--file", "X", "--action",
         "rename", "--description", "Y", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, ManifestAddOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunManifestAddCommand(
        {"--topic", "SampleTopic", "--phase", "99", "--file", "X", "--action",
         "create", "--description", "Y", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// testing set
// ===================================================================

TEST_F(FBundleTestFixture, TestingSetUpdatesRecord)
{
    CopyFixture("SampleTopic");
    // Add a testing record first so we have index 0
    StartCapture();
    UniPlan::RunTestingAddCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--session", "s1", "--actor",
         "human", "--step", "old step", "--action", "old action", "--expected",
         "old expected", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t TestingIndex = Before.mPhases[1].mTesting.size() - 1;
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunTestingSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--index",
         std::to_string(TestingIndex), "--step", "new step", "--action",
         "new action", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mTesting[TestingIndex].mStep, "new step");
    EXPECT_EQ(After.mPhases[1].mTesting[TestingIndex].mAction, "new action");
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
}

TEST_F(FBundleTestFixture, TestingSetOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTestingSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--index", "999", "--step",
         "x", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// verification set
// ===================================================================

TEST_F(FBundleTestFixture, VerificationSetUpdatesEntry)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    ASSERT_GT(Before.mVerifications.size(), 0u);
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunVerificationSetCommand(
        {"--topic", "SampleTopic", "--index", "0", "--check", "Updated check",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mVerifications[0].mCheck, "Updated check");
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
}

TEST_F(FBundleTestFixture, VerificationSetOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunVerificationSetCommand(
        {"--topic", "SampleTopic", "--index", "999", "--check", "x",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// manifest set
// ===================================================================

TEST_F(FBundleTestFixture, ManifestSetUpdatesDescription)
{
    CopyFixture("SampleTopic");
    // Add a manifest entry first
    StartCapture();
    UniPlan::RunManifestAddCommand({"--topic", "SampleTopic", "--phase", "1",
                                    "--file", "Foo.cpp", "--action", "create",
                                    "--description", "old desc", "--repo-root",
                                    mRepoRoot.string()},
                                   mRepoRoot.string());
    StopCapture();

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t MIdx = Before.mPhases[1].mFileManifest.size() - 1;
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunManifestSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--index",
         std::to_string(MIdx), "--description", "new desc", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mFileManifest[MIdx].mDescription, "new desc");
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
}

TEST_F(FBundleTestFixture, ManifestSetOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunManifestSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--index", "999",
         "--description", "x", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// lane add
// ===================================================================

TEST_F(FBundleTestFixture, LaneAddAppendsLane)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t CountBefore = Before.mPhases[1].mLanes.size();
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunLaneAddCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--scope", "New lane scope",
         "--exit-criteria", "Done when X", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mLanes.size(), CountBefore + 1);
    EXPECT_EQ(After.mPhases[1].mLanes.back().mScope, "New lane scope");
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
}

TEST_F(FBundleTestFixture, LaneAddOutOfRangePhaseFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunLaneAddCommand(
        {"--topic", "SampleTopic", "--phase", "99", "--scope", "X",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// ===================================================================
// job set --lane reassignment
// ===================================================================

TEST_F(FBundleTestFixture, JobSetReassignsLane)
{
    CopyFixture("SampleTopic");

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t ChangelogsBefore = Before.mChangeLogs.size();

    StartCapture();
    const int Code = UniPlan::RunJobSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--job", "0", "--lane", "1",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mJobs[0].mLane, 1);
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
}

TEST_F(FBundleTestFixture, JobSetLaneOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunJobSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--job", "0", "--lane", "99",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}
