#include "UniPlanTestFixture.h"

#include "UniPlanForwardDecls.h"
#include "UniPlanJSONIO.h"
#include "UniPlanRuntime.h"
#include "UniPlanTypes.h"

#include <fstream>

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
    EXPECT_THROW(UniPlan::RunTestingAddCommand(
                     {"--topic", "SampleTopic", "--phase", "0", "--session",
                      "T1", "--actor", "robot", "--step", "X", "--action", "Y",
                      "--expected", "Z", "--repo-root", mRepoRoot.string()},
                     mRepoRoot.string()),
                 UniPlan::UsageError);
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
    EXPECT_THROW(UniPlan::RunManifestAddCommand(
                     {"--topic", "SampleTopic", "--phase", "0", "--file", "X",
                      "--action", "rename", "--description", "Y", "--repo-root",
                      mRepoRoot.string()},
                     mRepoRoot.string()),
                 UniPlan::UsageError);
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

// v0.86.0: opt-out invariants. The schema requires
// no_file_manifest=true ⇒ non-empty reason. Both serializer write and
// the phase set mutation handler enforce this — a malformed bundle can
// never round-trip cleanly and a malformed mutation cannot land.
TEST_F(FBundleTestFixture, NoFileManifestRoundTripsWithReason)
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

    UniPlan::FTopicBundle Roundtrip;
    ASSERT_TRUE(ReloadBundle("T", Roundtrip));
    EXPECT_TRUE(Roundtrip.mPhases[0].mbNoFileManifest);
    EXPECT_EQ(Roundtrip.mPhases[0].mFileManifestSkipReason,
              "Doc-only phase: no code touched");
}

TEST_F(FBundleTestFixture, NoFileManifestRequiresReasonOnWrite)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    Bundle.mPhases[0].mbNoFileManifest = true;
    // Reason intentionally left empty to assert the write/parse rejection.
    const fs::path Path = mRepoRoot / "Docs" / "Plans" / "T.Plan.json";
    std::string Error;
    // The write itself succeeds (no schema gate at write time), but the
    // round-trip parse must reject it because the deserializer enforces
    // the invariant. This is a defense-in-depth test: even if a future
    // mutation handler regression let an empty reason through, the next
    // load would surface the invariant violation immediately.
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;
    UniPlan::FTopicBundle Roundtrip;
    EXPECT_FALSE(ReloadBundle("T", Roundtrip));
}

TEST_F(FBundleTestFixture, PhaseSetNoFileManifestMutates)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);
    StartCapture();
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "T", "--phase", "0", "--no-file-manifest", "true",
         "--no-file-manifest-reason", "Doc plan", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_TRUE(Bundle.mPhases[0].mbNoFileManifest);
    EXPECT_EQ(Bundle.mPhases[0].mFileManifestSkipReason, "Doc plan");
}

TEST_F(FBundleTestFixture, PhaseSetNoFileManifestRejectsWithoutReason)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);
    StartCapture();
    // No --no-file-manifest-reason provided alongside =true → handler
    // refuses with exit 1, leaves bundle untouched.
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "T", "--phase", "0", "--no-file-manifest", "true",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);

    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("T", Bundle));
    EXPECT_FALSE(Bundle.mPhases[0].mbNoFileManifest);
    EXPECT_TRUE(Bundle.mPhases[0].mFileManifestSkipReason.empty());
}

TEST(OptionParsing, PhaseSetNoFileManifestEnforcesBoolValue)
{
    EXPECT_THROW(UniPlan::ParsePhaseSetOptions(
                     {"--topic", "T", "--phase", "0", "--no-file-manifest",
                      "yes"}),
                 UniPlan::UsageError);
}

// v0.86.0: manifest suggest. Smoke test the dry-run JSON shape; full
// git-history flow is exercised by the live FIE corpus during release
// smoke. These tests assert the contract (schema, fields, exit codes,
// missing-started-at gate) without depending on real git history.
TEST_F(FBundleTestFixture, ManifestSuggestRequiresStartedAt)
{
    // not_started phase has no started_at → command refuses with
    // exit 1 and a clear remediation message on stderr.
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    StartCapture();
    const int Code = UniPlan::RunManifestSuggestCommand(
        {"--topic", "T", "--phase", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
    EXPECT_NE(mCapturedStderr.find("started_at"), std::string::npos)
        << "must explain why the command refused";
}

TEST_F(FBundleTestFixture, ManifestSuggestRejectsOutOfRangePhase)
{
    CreateMinimalFixture("T", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::InProgress, true);
    StartCapture();
    const int Code = UniPlan::RunManifestSuggestCommand(
        {"--topic", "T", "--phase", "99", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST(OptionParsing, ManifestSuggestRequiresTopicAndPhase)
{
    EXPECT_THROW(UniPlan::ParseManifestSuggestOptions({}),
                 UniPlan::UsageError);
    EXPECT_THROW(
        UniPlan::ParseManifestSuggestOptions({"--topic", "T"}),
        UniPlan::UsageError);
    EXPECT_THROW(
        UniPlan::ParseManifestSuggestOptions({"--phase", "0"}),
        UniPlan::UsageError);
}

// v0.84.0: --stale-plan classifies plan↔disk drift in 3 subcategories.
// stale_create   — action=create, file already exists
// stale_delete   — action=delete, file still exists
// dangling_modify— action=modify, file does not exist
TEST_F(FBundleTestFixture, ManifestListStalePlanFlagsStaleCreate)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    for (auto &Phase : Bundle.mPhases)
        Phase.mFileManifest.clear();
    UniPlan::FFileManifestItem Item;
    // Points at a file that really exists on disk (the bundle itself),
    // but marked action=create → stale_create.
    Item.mFilePath = "Docs/Plans/SampleTopic.Plan.json";
    Item.mAction = UniPlan::EFileAction::Create;
    Item.mDescription = "Stale create — file already landed";
    Bundle.mPhases[0].mFileManifest.push_back(std::move(Item));
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "SampleTopic.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunManifestListCommand(
        {"--topic", "SampleTopic", "--stale-plan", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    ASSERT_EQ(Json["entry_count"].get<size_t>(), 1u);
    const auto &E = Json["entries"][0];
    EXPECT_EQ(E["action"].get<std::string>(), "create");
    EXPECT_TRUE(E["exists_on_disk"].get<bool>());
    EXPECT_EQ(E["stale_reason"].get<std::string>(), "stale_create");
}

TEST_F(FBundleTestFixture, ManifestListStalePlanFlagsStaleDelete)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    for (auto &Phase : Bundle.mPhases)
        Phase.mFileManifest.clear();
    UniPlan::FFileManifestItem Item;
    // File exists but plan says "delete" — promised removal never happened.
    Item.mFilePath = "Docs/Plans/SampleTopic.Plan.json";
    Item.mAction = UniPlan::EFileAction::Delete;
    Item.mDescription = "Stale delete — plan promised removal";
    Bundle.mPhases[0].mFileManifest.push_back(std::move(Item));
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "SampleTopic.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunManifestListCommand(
        {"--topic", "SampleTopic", "--stale-plan", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    ASSERT_EQ(Json["entry_count"].get<size_t>(), 1u);
    const auto &E = Json["entries"][0];
    EXPECT_EQ(E["action"].get<std::string>(), "delete");
    EXPECT_TRUE(E["exists_on_disk"].get<bool>());
    EXPECT_EQ(E["stale_reason"].get<std::string>(), "stale_delete");
}

TEST_F(FBundleTestFixture, ManifestListStalePlanFlagsDanglingModify)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    for (auto &Phase : Bundle.mPhases)
        Phase.mFileManifest.clear();
    UniPlan::FFileManifestItem Item;
    // File does not exist but plan says "modify" — dangling reference.
    Item.mFilePath = "does/not/exist/anywhere.cpp";
    Item.mAction = UniPlan::EFileAction::Modify;
    Item.mDescription = "Dangling modify — file is absent";
    Bundle.mPhases[0].mFileManifest.push_back(std::move(Item));
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "SampleTopic.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunManifestListCommand(
        {"--topic", "SampleTopic", "--stale-plan", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    ASSERT_EQ(Json["entry_count"].get<size_t>(), 1u);
    const auto &E = Json["entries"][0];
    EXPECT_EQ(E["action"].get<std::string>(), "modify");
    EXPECT_FALSE(E["exists_on_disk"].get<bool>());
    EXPECT_EQ(E["stale_reason"].get<std::string>(), "dangling_modify");
}

TEST_F(FBundleTestFixture, ManifestListStalePlanSkipsAlignedEntries)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    for (auto &Phase : Bundle.mPhases)
        Phase.mFileManifest.clear();
    // Aligned entry: action=modify + file exists → NOT stale.
    UniPlan::FFileManifestItem Aligned;
    Aligned.mFilePath = "Docs/Plans/SampleTopic.Plan.json";
    Aligned.mAction = UniPlan::EFileAction::Modify;
    Aligned.mDescription = "Aligned — file exists and plan says modify";
    Bundle.mPhases[0].mFileManifest.push_back(Aligned);
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "SampleTopic.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunManifestListCommand(
        {"--topic", "SampleTopic", "--stale-plan", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["entry_count"].get<size_t>(), 0u)
        << "aligned manifest entries must not be reported under --stale-plan";
}

// v0.84.0: --human renders an ANSI table with explicit count/filter
// header. Pre-v0.84.0 the flag was silently ignored (command always
// emitted JSON). These tests gate the new behavior.
TEST_F(FBundleTestFixture, ManifestListHumanRendersTableHeader)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunManifestListCommand(
        {"--topic", "SampleTopic", "--human", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const std::string &Output = mCapturedStdout;
    // Must NOT emit JSON envelope — the silent-ignore path would have.
    EXPECT_EQ(Output.find("\"schema\""), std::string::npos)
        << "--human path leaked JSON envelope";
    // Must emit the human-mode summary line + table headers.
    EXPECT_NE(Output.find("File manifest"), std::string::npos);
    EXPECT_NE(Output.find("Topic"), std::string::npos);
    EXPECT_NE(Output.find("Action"), std::string::npos);
    EXPECT_NE(Output.find("Stale"), std::string::npos);
}

TEST_F(FBundleTestFixture, ManifestListHumanShowsStaleColumn)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    for (auto &Phase : Bundle.mPhases)
        Phase.mFileManifest.clear();
    UniPlan::FFileManifestItem Item;
    Item.mFilePath = "Docs/Plans/SampleTopic.Plan.json";
    Item.mAction = UniPlan::EFileAction::Create;
    Item.mDescription = "stale create target";
    Bundle.mPhases[0].mFileManifest.push_back(std::move(Item));
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "SampleTopic.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunManifestListCommand(
        {"--topic", "SampleTopic", "--stale-plan", "--human", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const std::string &Output = mCapturedStdout;
    EXPECT_NE(Output.find("stale_create"), std::string::npos)
        << "--human must surface stale_reason in the Stale column";
    EXPECT_NE(Output.find("[--stale-plan]"), std::string::npos)
        << "--human header must reflect the active --stale-plan filter";
}

TEST_F(FBundleTestFixture, ManifestListHumanShowsEmptyMessageWhenNoEntries)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    for (auto &Phase : Bundle.mPhases)
        Phase.mFileManifest.clear();
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "SampleTopic.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    StartCapture();
    const int Code = UniPlan::RunManifestListCommand(
        {"--topic", "SampleTopic", "--human", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const std::string &Output = mCapturedStdout;
    EXPECT_NE(Output.find("No manifest entries match the filter"),
              std::string::npos);
}

TEST_F(FBundleTestFixture, ManifestListEmitsStaleReasonNullForAlignedEntries)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Bundle;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Bundle));
    for (auto &Phase : Bundle.mPhases)
        Phase.mFileManifest.clear();
    UniPlan::FFileManifestItem Aligned;
    Aligned.mFilePath = "Docs/Plans/SampleTopic.Plan.json";
    Aligned.mAction = UniPlan::EFileAction::Modify;
    Aligned.mDescription = "Aligned entry";
    Bundle.mPhases[0].mFileManifest.push_back(Aligned);
    const fs::path Path =
        mRepoRoot / "Docs" / "Plans" / "SampleTopic.Plan.json";
    std::string Error;
    ASSERT_TRUE(UniPlan::TryWriteTopicBundle(Bundle, Path, Error)) << Error;

    // Default listing (no --stale-plan) emits every row with stale_reason.
    StartCapture();
    const int Code = UniPlan::RunManifestListCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    ASSERT_EQ(Json["entry_count"].get<size_t>(), 1u);
    const auto &E = Json["entries"][0];
    ASSERT_TRUE(E.contains("stale_reason"));
    // Aligned entries serialize stale_reason as JSON null.
    EXPECT_TRUE(E["stale_reason"].is_null())
        << "aligned manifest entries must emit stale_reason=null";
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
    for (auto &Phase : Bundle.mPhases)
        Phase.mFileManifest.clear();
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

// ===================================================================
// v0.93.0 CRUD symmetry — job/task/lane/testing add/remove/list +
// topic normalize + --validation-commands-file
// ===================================================================

TEST_F(FBundleTestFixture, JobAddAppendsJob)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t CountBefore = Before.mPhases[1].mJobs.size();

    StartCapture();
    const int Code = UniPlan::RunJobAddCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--scope", "New job",
         "--lane", "0", "--wave", "0", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json["ok"].get<bool>());
    EXPECT_EQ(Json["job_index"], CountBefore);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mJobs.size(), CountBefore + 1);
    EXPECT_EQ(After.mPhases[1].mJobs.back().mScope, "New job");
    EXPECT_EQ(After.mPhases[1].mJobs.back().mLane, 0);
    EXPECT_EQ(After.mPhases[1].mJobs.back().mWave, 0);
}

TEST_F(FBundleTestFixture, JobAddRejectsOutOfRangeLane)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunJobAddCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--scope", "X", "--lane",
         "99", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, JobRemoveErasesJob)
{
    CopyFixture("SampleTopic");
    // First add one, then remove it so we don't tamper with fixture baseline.
    StartCapture();
    ASSERT_EQ(UniPlan::RunJobAddCommand(
                  {"--topic", "SampleTopic", "--phase", "1", "--scope",
                   "Ephemeral", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    StopCapture();

    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t CountBefore = Before.mPhases[1].mJobs.size();
    const int EphemeralIndex = static_cast<int>(CountBefore) - 1;

    StartCapture();
    const int Code = UniPlan::RunJobRemoveCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--job",
         std::to_string(EphemeralIndex), "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mJobs.size(), CountBefore - 1);
}

TEST_F(FBundleTestFixture, JobListEmitsCountedEntries)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunJobListCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["schema"], "uni-plan-list-v1");
    EXPECT_TRUE(Json["entries"].is_array());
    EXPECT_EQ(Json["entry_count"].get<size_t>(), Json["entries"].size());
    EXPECT_GE(Json["entries"].size(), 1u);
}

TEST_F(FBundleTestFixture, TaskAddRemoveRoundtrip)
{
    CopyFixture("SampleTopic");

    // Pick an existing job that already has tasks (phase 1 jobs[2]).
    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t CountBefore = Before.mPhases[1].mJobs[2].mTasks.size();

    StartCapture();
    ASSERT_EQ(UniPlan::RunTaskAddCommand(
                  {"--topic", "SampleTopic", "--phase", "1", "--job", "2",
                   "--description", "Ephemeral task", "--repo-root",
                   mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    StopCapture();

    UniPlan::FTopicBundle Mid;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Mid));
    ASSERT_EQ(Mid.mPhases[1].mJobs[2].mTasks.size(), CountBefore + 1);

    const int NewIndex = static_cast<int>(CountBefore);
    StartCapture();
    ASSERT_EQ(UniPlan::RunTaskRemoveCommand(
                  {"--topic", "SampleTopic", "--phase", "1", "--job", "2",
                   "--task", std::to_string(NewIndex), "--repo-root",
                   mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    StopCapture();

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mJobs[2].mTasks.size(), CountBefore);
}

TEST_F(FBundleTestFixture, TaskListFiltersByJob)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTaskListCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--job", "2", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_TRUE(Json["entries"].is_array());
    for (const auto &E : Json["entries"])
    {
        EXPECT_EQ(E["phase_index"].get<int>(), 1);
        EXPECT_EQ(E["job_index"].get<int>(), 2);
    }
}

TEST_F(FBundleTestFixture, LaneRemoveRefusesWhenJobsReferenceIt)
{
    CopyFixture("SampleTopic");
    // phase 1 lane 0 is referenced by job[0] and job[1]; remove must refuse.
    StartCapture();
    const int Code = UniPlan::RunLaneRemoveCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--lane", "0", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 1);
}

TEST_F(FBundleTestFixture, LaneRemoveSucceedsAfterJobsReassigned)
{
    CopyFixture("SampleTopic");

    // Add an orphan lane (index N) with no jobs, then remove it — must pass.
    StartCapture();
    ASSERT_EQ(UniPlan::RunLaneAddCommand(
                  {"--topic", "SampleTopic", "--phase", "1", "--scope",
                   "Orphan lane", "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    StopCapture();

    UniPlan::FTopicBundle Mid;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Mid));
    const int OrphanIndex = static_cast<int>(Mid.mPhases[1].mLanes.size()) - 1;
    const size_t LanesBefore = Mid.mPhases[1].mLanes.size();

    StartCapture();
    const int Code = UniPlan::RunLaneRemoveCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--lane",
         std::to_string(OrphanIndex), "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mLanes.size(), LanesBefore - 1);
}

TEST_F(FBundleTestFixture, LaneListEmitsCountedEntries)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunLaneListCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["schema"], "uni-plan-list-v1");
    EXPECT_GE(Json["entries"].size(), 1u);
}

TEST_F(FBundleTestFixture, TestingRemoveErasesEntry)
{
    CopyFixture("SampleTopic");
    StartCapture();
    ASSERT_EQ(UniPlan::RunTestingAddCommand(
                  {"--topic", "SampleTopic", "--phase", "1", "--session", "s",
                   "--step", "stp", "--action", "ac", "--expected", "ex",
                   "--repo-root", mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    StopCapture();

    UniPlan::FTopicBundle Mid;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Mid));
    const int EphemeralIndex =
        static_cast<int>(Mid.mPhases[1].mTesting.size()) - 1;
    const size_t CountBefore = Mid.mPhases[1].mTesting.size();

    StartCapture();
    ASSERT_EQ(UniPlan::RunTestingRemoveCommand(
                  {"--topic", "SampleTopic", "--phase", "1", "--index",
                   std::to_string(EphemeralIndex), "--repo-root",
                   mRepoRoot.string()},
                  mRepoRoot.string()),
              0);
    StopCapture();

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mPhases[1].mTesting.size(), CountBefore - 1);
}

TEST_F(FBundleTestFixture, TestingListEmitsEntries)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunTestingListCommand(
        {"--topic", "SampleTopic", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const auto Json = ParseCapturedJSON();
    EXPECT_EQ(Json["schema"], "uni-plan-list-v1");
    EXPECT_TRUE(Json["entries"].is_array());
}

TEST_F(FBundleTestFixture, TopicNormalizeDryRunReportsOnly)
{
    CopyFixture("SampleTopic");
    UniPlan::FTopicBundle Before;
    ASSERT_TRUE(ReloadBundle("SampleTopic", Before));
    const size_t ChangelogsBefore = Before.mChangeLogs.size();
    const std::string SummaryBefore = Before.mMetadata.mSummary;

    StartCapture();
    const int Code = UniPlan::RunTopicNormalizeCommand(
        {"--topic", "SampleTopic", "--dry-run", "--repo-root",
         mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    EXPECT_EQ(After.mMetadata.mSummary, SummaryBefore);
    EXPECT_EQ(After.mChangeLogs.size(), ChangelogsBefore);
}

TEST_F(FBundleTestFixture, PhaseSetValidationCommandsFileReplacesArray)
{
    CopyFixture("SampleTopic");

    const fs::path VcPath = mRepoRoot / "vc.txt";
    {
        std::ofstream Out(VcPath);
        Out << "macos|bash build.sh|macOS build check\n";
        Out << "windows|build.bat|Windows build check\n";
    }

    StartCapture();
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--validation-commands-file",
         VcPath.string(), "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mPhases[1].mDesign.mValidationCommands.size(), 2u);
    EXPECT_EQ(After.mPhases[1].mDesign.mValidationCommands[0].mCommand,
              "bash build.sh");
    EXPECT_EQ(After.mPhases[1].mDesign.mValidationCommands[1].mCommand,
              "build.bat");
}

TEST_F(FBundleTestFixture, PhaseSetValidationCommandsInlineReplacesArray)
{
    CopyFixture("SampleTopic");
    StartCapture();
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "SampleTopic", "--phase", "1", "--validation-commands",
         "macos|bash build.sh|macOS", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);

    UniPlan::FTopicBundle After;
    ASSERT_TRUE(ReloadBundle("SampleTopic", After));
    ASSERT_EQ(After.mPhases[1].mDesign.mValidationCommands.size(), 1u);
    EXPECT_EQ(After.mPhases[1].mDesign.mValidationCommands[0].mCommand,
              "bash build.sh");
}

TEST(OptionParsing, JobAddRequiresPhase)
{
    EXPECT_THROW(UniPlan::ParseJobAddOptions({"--topic", "T"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, JobRemoveRequiresJobIndex)
{
    EXPECT_THROW(
        UniPlan::ParseJobRemoveOptions({"--topic", "T", "--phase", "0"}),
        UniPlan::UsageError);
}

TEST(OptionParsing, TaskAddRequiresDescription)
{
    EXPECT_THROW(UniPlan::ParseTaskAddOptions(
                     {"--topic", "T", "--phase", "0", "--job", "0"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, TaskListJobRequiresPhase)
{
    EXPECT_THROW(UniPlan::ParseTaskListOptions({"--topic", "T", "--job", "0"}),
                 UniPlan::UsageError);
}

TEST(OptionParsing, TopicNormalizeRequiresTopic)
{
    EXPECT_THROW(UniPlan::ParseTopicNormalizeOptions({"--dry-run"}),
                 UniPlan::UsageError);
}
