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

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mTesting.size(), CountBefore + 1);
    EXPECT_EQ(After.mPhases[1].mTesting.back().mStep, "Build");
    EXPECT_EQ(After.mPhases[1].mTesting.back().mActor,
              UniPlan::ETestingActor::AI);
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
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

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mFileManifest.size(), CountBefore + 1);
    EXPECT_EQ(After.mPhases[1].mFileManifest.back().mFilePath,
              "Source/New.cpp");
    EXPECT_EQ(After.mPhases[1].mFileManifest.back().mAction,
              UniPlan::EFileAction::Create);
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
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
