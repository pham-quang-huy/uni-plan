#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanJsonIO.h"
#include "UniPlanRuntime.h"
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
    EXPECT_EQ(Json["target"], "phases[1].testing");
    AssertNoLegacyPhasePath(Json["target"].get<std::string>());

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mTesting.size(), CountBefore + 1);
    EXPECT_EQ(After.mPhases[1].mTesting.back().mStep, "Build");
    EXPECT_EQ(After.mPhases[1].mTesting.back().mActor,
              UniPlan::ETestingActor::AI);
    EXPECT_GT(After.mChangeLogs.size(), ChangelogsBefore);
    ASSERT_FALSE(After.mChangeLogs.empty());
    EXPECT_EQ(After.mChangeLogs.back().mAffected, "phases[1].testing");
    AssertNoLegacyPhasePath(After.mChangeLogs.back().mAffected);
}

TEST_F(FBundleTestFixture, TestingAddInvalidActorFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTestingAddCommand(
        {"--topic", "SampleTopic", "--phase", "0", "--session", "T1", "--actor",
         "robot", "--step", "X", "--action", "Y", "--expected", "Z",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, TestingAddOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTestingAddCommand(
        {"--topic", "SampleTopic", "--phase", "99", "--session", "T1", "--step",
         "X", "--action", "Y", "--expected", "Z", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST(OptionParsing, TestingAddRequiresSession)
{
    EXPECT_THROW(UniPlan::ParseTestingAddOptions(
                     {"--topic", "T", "--phase", "0", "--step", "X", "--action",
                      "Y", "--expected", "Z"}),
                 UniPlan::UsageError);
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
    EXPECT_EQ(Json["target"], "phases[1].file_manifest");
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
    EXPECT_EQ(After.mChangeLogs.back().mAffected, "phases[1].file_manifest");
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

TEST_F(FBundleTestFixture, ManifestRemoveDropsEntry)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t CountBefore = Before.mPhases[1].mFileManifest.size();
    ASSERT_GT(CountBefore, 0u);
    const std::string RemovedFile =
        Before.mPhases[1].mFileManifest[0].mFilePath;

    StartCapture();
    const int Code = UniPlan::RunManifestRemoveCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--index", "0",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mFileManifest.size(), CountBefore - 1);
    EXPECT_GT(After.mChangeLogs.size(), 0u);
    EXPECT_NE(After.mChangeLogs.back().mChange.find("file_manifest[0] removed"),
              std::string::npos);
    EXPECT_NE(After.mChangeLogs.back().mChange.find(RemovedFile),
              std::string::npos);
}

TEST_F(FBundleTestFixture, ManifestRemoveOutOfRangeFails)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunManifestRemoveCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--index", "999",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

// v0.71.0: manifest list is the CLI-native path for cross-topic
// manifest enumeration (replaces the raw-JSON loop that violated the
// "uni-plan CLI is the only interface to .Plan.json" rule).
TEST_F(FBundleTestFixture, ManifestListEnumeratesEntries)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunManifestListCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    ASSERT_TRUE(Json.contains("entries"));
    EXPECT_GT(Json["entry_count"].get<size_t>(), 0u);
    const auto &First = Json["entries"][0];
    EXPECT_TRUE(First.contains("topic"));
    EXPECT_TRUE(First.contains("phase_index"));
    EXPECT_TRUE(First.contains("manifest_index"));
    EXPECT_TRUE(First.contains("file_path"));
    EXPECT_TRUE(First.contains("action"));
    EXPECT_TRUE(First.contains("description"));
    EXPECT_TRUE(First.contains("exists_on_disk"));
}

TEST_F(FBundleTestFixture, ManifestListFiltersByPhase)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunManifestListCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    for (const auto &E : Json["entries"])
        EXPECT_EQ(E["phase_index"].get<size_t>(), 1u);
}

TEST_F(FBundleTestFixture, ManifestListMissingOnlyFiltersExistent)
{
    CopyFixture("SampleTopic");
    // Seed one non-existent path alongside whatever the fixture has.
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    UniPlan::FFileManifestItem Item;
    Item.mFilePath = "does/not/exist/on/disk.cpp";
    Item.mAction = UniPlan::EFileAction::Create;
    Item.mDescription = "Invented path for test";
    Bundle.mPhases[0].mFileManifest.push_back(std::move(Item));
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "SampleTopic.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunManifestListCommand(
        {"--topic", "SampleTopic", "--missing-only", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    ASSERT_GT(Json["entry_count"].get<size_t>(), 0u);
    for (const auto &E : Json["entries"])
        EXPECT_FALSE(E["exists_on_disk"].get<bool>())
            << "--missing-only returned an existing path: "
            << E["file_path"].get<std::string>();
}

TEST(OptionParsing, ManifestListAcceptsEmptyArgs)
{
    // All args optional — no throw when called with no filters.
    EXPECT_NO_THROW(UniPlan::ParseManifestListOptions({}));
}

// v0.71.1 regression: exists_on_disk checks must resolve relative
// file_manifest paths against --repo-root, not the process cwd. The
// test-process cwd is the build dir, so a path like
// "Docs/Plans/SampleTopic.Plan.json" exists only when the check walks
// through mRepoRoot — prior to the fix it was always reported missing.
TEST_F(FBundleTestFixture, ManifestListResolvesPathsAgainstRepoRoot)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    UniPlan::FFileManifestItem Item;
    Item.mFilePath = "Docs/Plans/SampleTopic.Plan.json";
    Item.mAction = UniPlan::EFileAction::Modify;
    Item.mDescription = "Bundle file — exists relative to repo root";
    Bundle.mPhases[0].mFileManifest.push_back(std::move(Item));
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "SampleTopic.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunManifestListCommand(
        {"--topic", "SampleTopic", "--phase", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();

    bool bFound = false;
    for (const auto &E : Json["entries"])
    {
        if (E["file_path"].get<std::string>() ==
            "Docs/Plans/SampleTopic.Plan.json")
        {
            EXPECT_TRUE(E["exists_on_disk"].get<bool>())
                << "--repo-root not threaded: path relative to repo root "
                   "reported missing when cwd != repo-root";
            bFound = true;
        }
    }
    EXPECT_TRUE(bFound) << "seeded manifest entry not returned by list";
}

// v0.71.1 regression: validate summary.file_manifest_missing must also
// use the repo-root-aware existence check. Seeds one existing relative
// path + one missing path and asserts the counter is exactly 1.
TEST_F(FBundleTestFixture, ValidateSummaryResolvesManifestAgainstRepoRoot)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    Bundle.mPhases[0].mFileManifest.clear();
    UniPlan::FFileManifestItem Exists;
    Exists.mFilePath = "Docs/Plans/SampleTopic.Plan.json";
    Exists.mAction = UniPlan::EFileAction::Modify;
    Exists.mDescription = "Real relative path";
    Bundle.mPhases[0].mFileManifest.push_back(Exists);
    UniPlan::FFileManifestItem Missing;
    Missing.mFilePath = "does/not/exist.cpp";
    Missing.mAction = UniPlan::EFileAction::Create;
    Missing.mDescription = "Invented path";
    Bundle.mPhases[0].mFileManifest.push_back(Missing);
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "SampleTopic.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunBundleValidateCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    ASSERT_TRUE(Json.contains("summary"));
    const auto &Phase0 = Json["summary"]["topics"][0]["phases"][0];
    EXPECT_EQ(Phase0["file_manifest_count"].get<size_t>(), 2u);
    EXPECT_EQ(Phase0["file_manifest_missing"].get<size_t>(), 1u)
        << "summary.file_manifest_missing did not resolve against "
           "--repo-root";
}

// v0.71.1 regression: RunMain must route UsageError output to stderr
// (error message + usage banner) and leave stdout empty so downstream
// `2>/dev/null | jq` pipelines see empty input instead of ANSI-laden
// usage text.
TEST_F(FBundleTestFixture, RunMainRoutesInvalidFlagToStderr)
{
    const char *Arg0 = "uni-plan";
    const char *Arg1 = "validate";
    const char *Arg2 = "--definitely-not-a-real-flag";
    char *Argv[] = {const_cast<char *>(Arg0), const_cast<char *>(Arg1),
                    const_cast<char *>(Arg2)};
    StartCapture();
    const int Code = UniPlan::RunMain(3, Argv);
    StopCapture();
    EXPECT_EQ(Code, 2);
    EXPECT_TRUE(mCapturedStdout.empty())
        << "Usage banner leaked to stdout: " << mCapturedStdout;
    EXPECT_NE(mCapturedStderr.find("Unknown option"), std::string::npos)
        << "Error line missing from stderr: " << mCapturedStderr;
    EXPECT_NE(mCapturedStderr.find("Usage:"), std::string::npos)
        << "Usage banner missing from stderr: " << mCapturedStderr;
}

// v0.71.1 regression: RunMain success paths (--help, bare invocation,
// --version) must still write to stdout, not stderr.
TEST_F(FBundleTestFixture, RunMainHelpGoesToStdout)
{
    const char *Arg0 = "uni-plan";
    const char *Arg1 = "--help";
    char *Argv[] = {const_cast<char *>(Arg0), const_cast<char *>(Arg1)};
    StartCapture();
    const int Code = UniPlan::RunMain(2, Argv);
    StopCapture();
    EXPECT_EQ(Code, 0);
    EXPECT_NE(mCapturedStdout.find("Usage:"), std::string::npos)
        << "--help did not write usage to stdout";
    EXPECT_TRUE(mCapturedStderr.empty())
        << "--help leaked to stderr: " << mCapturedStderr;
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
