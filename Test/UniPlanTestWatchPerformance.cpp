#include "UniPlanTestFixture.h"

#include "UniPlanBundleIndex.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanWatchSnapshot.h"

#include <fstream>

#if defined(UPLAN_WATCH)

#include "UniPlanWatchPanels.h"

#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

namespace
{

void AppendTextFile(const fs::path &InPath, const std::string &InText)
{
    std::ofstream Stream(InPath, std::ios::app);
    ASSERT_TRUE(Stream.is_open()) << "Cannot append to " << InPath.string();
    Stream << InText;
}

void WriteTextFile(const fs::path &InPath, const std::string &InText)
{
    fs::create_directories(InPath.parent_path());
    std::ofstream Stream(InPath);
    ASSERT_TRUE(Stream.is_open()) << "Cannot write " << InPath.string();
    Stream << InText;
}

const UniPlan::FWatchPlanSummary *
FindPlan(const UniPlan::FDocWatchSnapshot &InSnapshot,
         const std::string &InTopic)
{
    for (const UniPlan::FWatchPlanSummary &Plan : InSnapshot.mActivePlans)
    {
        if (Plan.mTopicKey == InTopic)
        {
            return &Plan;
        }
    }
    for (const UniPlan::FWatchPlanSummary &Plan : InSnapshot.mNonActivePlans)
    {
        if (Plan.mTopicKey == InTopic)
        {
            return &Plan;
        }
    }
    return nullptr;
}

bool HasWarningContaining(const UniPlan::FDocWatchSnapshot &InSnapshot,
                          const std::string &InNeedle)
{
    for (const std::string &Warning : InSnapshot.mWarnings)
    {
        if (Warning.find(InNeedle) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

std::string RenderElementToString(ftxui::Element InElement)
{
    ftxui::Screen Screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(220),
                                                 ftxui::Dimension::Fixed(40));
    ftxui::Render(Screen, InElement);
    return Screen.ToString();
}

void ExpectSnapshotCoreEquivalent(const UniPlan::FDocWatchSnapshot &InLeft,
                                  const UniPlan::FDocWatchSnapshot &InRight)
{
    EXPECT_EQ(InLeft.mInventory.mPlanCount, InRight.mInventory.mPlanCount);
    EXPECT_EQ(InLeft.mInventory.mActivePlanCount,
              InRight.mInventory.mActivePlanCount);
    EXPECT_EQ(InLeft.mInventory.mNonActivePlanCount,
              InRight.mInventory.mNonActivePlanCount);
    EXPECT_EQ(InLeft.mValidation.mErrorMajorCount,
              InRight.mValidation.mErrorMajorCount);
    EXPECT_EQ(InLeft.mValidation.mErrorMinorCount,
              InRight.mValidation.mErrorMinorCount);
    EXPECT_EQ(InLeft.mValidation.mWarningCount,
              InRight.mValidation.mWarningCount);
    EXPECT_EQ(InLeft.mLint.mWarningCount, InRight.mLint.mWarningCount);
    EXPECT_EQ(InLeft.mAllBlockers.size(), InRight.mAllBlockers.size());
    EXPECT_EQ(InLeft.mActivePlans.size(), InRight.mActivePlans.size());
    EXPECT_EQ(InLeft.mNonActivePlans.size(), InRight.mNonActivePlans.size());

    const auto ExpectPlansEquivalent =
        [](const std::vector<UniPlan::FWatchPlanSummary> &InLeftPlans,
           const std::vector<UniPlan::FWatchPlanSummary> &InRightPlans)
    {
        for (size_t PlanIndex = 0; PlanIndex < InLeftPlans.size(); ++PlanIndex)
        {
            const UniPlan::FWatchPlanSummary &Left = InLeftPlans[PlanIndex];
            const UniPlan::FWatchPlanSummary &Right = InRightPlans[PlanIndex];
            EXPECT_EQ(Left.mTopicKey, Right.mTopicKey);
            EXPECT_EQ(Left.mPlanStatus, Right.mPlanStatus);
            EXPECT_EQ(Left.mPhaseTaxonomies.size(),
                      Right.mPhaseTaxonomies.size());
            ASSERT_EQ(Left.mPhases.size(), Right.mPhases.size());
            for (size_t PhaseIndex = 0; PhaseIndex < Left.mPhases.size();
                 ++PhaseIndex)
            {
                EXPECT_EQ(Left.mPhases[PhaseIndex].mStatus,
                          Right.mPhases[PhaseIndex].mStatus);
                EXPECT_EQ(Left.mPhases[PhaseIndex].mMetrics.mDesignChars,
                          Right.mPhases[PhaseIndex].mMetrics.mDesignChars);
                EXPECT_EQ(
                    Left.mPhases[PhaseIndex].mMetrics.mRecursiveWordCount,
                    Right.mPhases[PhaseIndex].mMetrics.mRecursiveWordCount);
            }
        }
    };

    ExpectPlansEquivalent(InLeft.mActivePlans, InRight.mActivePlans);
    ExpectPlansEquivalent(InLeft.mNonActivePlans, InRight.mNonActivePlans);
}

} // namespace

TEST_F(FBundleTestFixture, BundleIndexDiscoversPlansAndSkipsBuildDirs)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 1);
    WriteTextFile(
        mRepoRoot / "Build" / "Docs" / "Plans" / "BuildOnly.Plan.json", "{}");
    WriteTextFile(mRepoRoot / ".hidden" / "Docs" / "Plans" /
                      "HiddenOnly.Plan.json",
                  "{}");

    UniPlan::FBundleFileIndexResult Index;
    std::string Error;
    ASSERT_TRUE(UniPlan::TryBuildBundleFileIndex(mRepoRoot, Index, Error))
        << Error;
    ASSERT_EQ(Index.mBundles.size(), 1u);
    EXPECT_EQ(Index.mBundles[0].mTopicKey, "Alpha");
    EXPECT_NE(Index.mSignature, 0u);
}

TEST_F(FBundleTestFixture, BundleIndexDetectsChangeAddAndDelete)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 1);

    UniPlan::FBundleFileIndexResult First;
    std::string Error;
    ASSERT_TRUE(UniPlan::TryBuildBundleFileIndex(mRepoRoot, First, Error))
        << Error;
    ASSERT_EQ(First.mBundles.size(), 1u);

    AppendTextFile(mRepoRoot / "Docs" / "Plans" / "Alpha.Plan.json", "\n");

    UniPlan::FBundleFileIndexResult Changed;
    ASSERT_TRUE(UniPlan::TryBuildBundleFileIndex(mRepoRoot, Changed, Error))
        << Error;
    EXPECT_NE(Changed.mSignature, First.mSignature);

    CreateMinimalFixture("Beta", UniPlan::ETopicStatus::NotStarted, 1);
    UniPlan::FBundleFileIndexResult Added;
    ASSERT_TRUE(UniPlan::TryBuildBundleFileIndex(mRepoRoot, Added, Error))
        << Error;
    ASSERT_EQ(Added.mBundles.size(), 2u);
    EXPECT_EQ(Added.mBundles[0].mTopicKey, "Alpha");
    EXPECT_EQ(Added.mBundles[1].mTopicKey, "Beta");

    fs::remove(mRepoRoot / "Docs" / "Plans" / "Alpha.Plan.json");
    UniPlan::FBundleFileIndexResult Deleted;
    ASSERT_TRUE(UniPlan::TryBuildBundleFileIndex(mRepoRoot, Deleted, Error))
        << Error;
    ASSERT_EQ(Deleted.mBundles.size(), 1u);
    EXPECT_EQ(Deleted.mBundles[0].mTopicKey, "Beta");
}

TEST_F(FBundleTestFixture, MarkdownSignatureChangesIndependentlyFromBundles)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 1);

    UniPlan::FBundleFileIndexResult BundleFirst;
    UniPlan::FMarkdownFileIndexResult MarkdownFirst;
    std::string Error;
    ASSERT_TRUE(UniPlan::TryBuildBundleFileIndex(mRepoRoot, BundleFirst, Error))
        << Error;
    ASSERT_TRUE(
        UniPlan::TryBuildMarkdownFileIndex(mRepoRoot, MarkdownFirst, Error))
        << Error;

    WriteTextFile(mRepoRoot / "Docs" / "Notes.md", "# Notes\n");
    UniPlan::FBundleFileIndexResult BundleSecond;
    UniPlan::FMarkdownFileIndexResult MarkdownSecond;
    ASSERT_TRUE(
        UniPlan::TryBuildBundleFileIndex(mRepoRoot, BundleSecond, Error))
        << Error;
    ASSERT_TRUE(
        UniPlan::TryBuildMarkdownFileIndex(mRepoRoot, MarkdownSecond, Error))
        << Error;
    EXPECT_EQ(BundleSecond.mSignature, BundleFirst.mSignature);
    EXPECT_NE(MarkdownSecond.mSignature, MarkdownFirst.mSignature);

    WriteTextFile(mRepoRoot / "Docs" / "Ignored.txt", "not markdown\n");
    UniPlan::FMarkdownFileIndexResult MarkdownThird;
    ASSERT_TRUE(
        UniPlan::TryBuildMarkdownFileIndex(mRepoRoot, MarkdownThird, Error))
        << Error;
    EXPECT_EQ(MarkdownThird.mSignature, MarkdownSecond.mSignature);

    WriteTextFile(mRepoRoot / "Build" / "Ignored.md", "# ignored\n");
    WriteTextFile(mRepoRoot / ".hidden" / "Ignored.md", "# ignored\n");
    UniPlan::FMarkdownFileIndexResult MarkdownSkipped;
    ASSERT_TRUE(
        UniPlan::TryBuildMarkdownFileIndex(mRepoRoot, MarkdownSkipped, Error))
        << Error;
    EXPECT_EQ(MarkdownSkipped.mSignature, MarkdownSecond.mSignature);
}

TEST_F(FBundleTestFixture, WatchCacheReusesNoChangeSnapshot)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, true);
    CreateMinimalFixture("Beta", UniPlan::ETopicStatus::NotStarted, 3,
                         UniPlan::EExecutionStatus::NotStarted, true);

    UniPlan::FWatchSnapshotCache Cache;
    const UniPlan::FDocWatchSnapshot First = UniPlan::BuildWatchSnapshotCached(
        mRepoRoot.string(), true, "", false, Cache, true);
    EXPECT_EQ(First.mPerformance.mBundleReloadCount, 2);
    EXPECT_EQ(First.mPerformance.mBundleReuseCount, 0);
    EXPECT_EQ(First.mPerformance.mMetricRecomputeCount, 5);
    EXPECT_TRUE(First.mPerformance.mbValidationRan);
    EXPECT_TRUE(First.mPerformance.mbLintRan);

    const UniPlan::FDocWatchSnapshot Second = UniPlan::BuildWatchSnapshotCached(
        mRepoRoot.string(), true, "", false, Cache, false);
    EXPECT_EQ(Second.mPerformance.mBundleReloadCount, 0);
    EXPECT_EQ(Second.mPerformance.mBundleReuseCount, 2);
    EXPECT_EQ(Second.mPerformance.mMetricRecomputeCount, 0);
    EXPECT_FALSE(Second.mPerformance.mbValidationRan);
    EXPECT_FALSE(Second.mPerformance.mbLintRan);
    EXPECT_EQ(Second.mInventory.mPlanCount, First.mInventory.mPlanCount);
    ASSERT_NE(FindPlan(Second, "Alpha"), nullptr);
    ASSERT_NE(FindPlan(Second, "Beta"), nullptr);
}

TEST_F(FBundleTestFixture, WatchCacheSnapshotMatchesFullRebuild)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, true);
    CreateMinimalFixture("Beta", UniPlan::ETopicStatus::NotStarted, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);

    UniPlan::FWatchSnapshotCache Cache;
    UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                      Cache, true);
    const UniPlan::FDocWatchSnapshot Incremental =
        UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                          Cache, false);
    const UniPlan::FDocWatchSnapshot Full =
        UniPlan::BuildWatchSnapshot(mRepoRoot.string(), true, "", false);

    EXPECT_EQ(Incremental.mPerformance.mBundleReloadCount, 0);
    EXPECT_EQ(Incremental.mPerformance.mMetricRecomputeCount, 0);
    ExpectSnapshotCoreEquivalent(Incremental, Full);
}

TEST_F(FBundleTestFixture, WatchCacheReloadsOnlyChangedBundle)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, true);
    CreateMinimalFixture("Beta", UniPlan::ETopicStatus::NotStarted, 3,
                         UniPlan::EExecutionStatus::NotStarted, true);

    UniPlan::FWatchSnapshotCache Cache;
    UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                      Cache, true);

    StartCapture();
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "Alpha", "--phase", "0", "--output", "Changed",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    const UniPlan::FDocWatchSnapshot Changed =
        UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                          Cache, false);
    EXPECT_EQ(Changed.mPerformance.mBundleReloadCount, 1);
    EXPECT_EQ(Changed.mPerformance.mBundleReuseCount, 1);
    EXPECT_EQ(Changed.mPerformance.mMetricRecomputeCount, 2);
    EXPECT_TRUE(Changed.mPerformance.mbValidationRan);
    EXPECT_FALSE(Changed.mPerformance.mbLintRan);
}

TEST_F(FBundleTestFixture, WatchCacheSeparatesMarkdownOnlyChanges)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, true);
    CreateMinimalFixture("Beta", UniPlan::ETopicStatus::NotStarted, 3,
                         UniPlan::EExecutionStatus::NotStarted, true);

    UniPlan::FWatchSnapshotCache Cache;
    UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                      Cache, true);
    UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                      Cache, false);

    WriteTextFile(mRepoRoot / "Docs" / "Notes.md", "# Notes\n");
    const UniPlan::FDocWatchSnapshot MarkdownOnly =
        UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                          Cache, false);
    EXPECT_EQ(MarkdownOnly.mPerformance.mBundleReloadCount, 0);
    EXPECT_EQ(MarkdownOnly.mPerformance.mBundleReuseCount, 2);
    EXPECT_EQ(MarkdownOnly.mPerformance.mMetricRecomputeCount, 0);
    EXPECT_FALSE(MarkdownOnly.mPerformance.mbValidationRan);
    EXPECT_TRUE(MarkdownOnly.mPerformance.mbLintRan);
}

TEST_F(FBundleTestFixture, WatchCacheDropsDeletedBundle)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, true);
    CreateMinimalFixture("Beta", UniPlan::ETopicStatus::NotStarted, 3,
                         UniPlan::EExecutionStatus::NotStarted, true);

    UniPlan::FWatchSnapshotCache Cache;
    UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                      Cache, true);
    fs::remove(mRepoRoot / "Docs" / "Plans" / "Beta.Plan.json");

    const UniPlan::FDocWatchSnapshot Deleted =
        UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                          Cache, false);
    EXPECT_EQ(Deleted.mInventory.mPlanCount, 1);
    EXPECT_EQ(Deleted.mPerformance.mBundleReloadCount, 0);
    EXPECT_EQ(Deleted.mPerformance.mBundleReuseCount, 1);
    EXPECT_EQ(Deleted.mPerformance.mMetricRecomputeCount, 0);
    EXPECT_TRUE(Deleted.mPerformance.mbValidationRan);
    EXPECT_NE(FindPlan(Deleted, "Alpha"), nullptr);
    EXPECT_EQ(FindPlan(Deleted, "Beta"), nullptr);
}

TEST_F(FBundleTestFixture, WatchCacheForceRefreshReloadsCacheableData)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, true);

    UniPlan::FWatchSnapshotCache Cache;
    UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                      Cache, true);
    UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                      Cache, false);

    const UniPlan::FDocWatchSnapshot Forced = UniPlan::BuildWatchSnapshotCached(
        mRepoRoot.string(), true, "", false, Cache, true);
    EXPECT_EQ(Forced.mPerformance.mBundleReloadCount, 1);
    EXPECT_EQ(Forced.mPerformance.mBundleReuseCount, 0);
    EXPECT_EQ(Forced.mPerformance.mMetricRecomputeCount, 2);
    EXPECT_TRUE(Forced.mPerformance.mbValidationRan);
    EXPECT_TRUE(Forced.mPerformance.mbLintRan);
    EXPECT_TRUE(Forced.mPerformance.mbForceRefresh);
}

TEST_F(FBundleTestFixture, WatchCacheReportsMalformedBundleWarning)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    WriteTextFile(mRepoRoot / "Docs" / "Plans" / "Broken.Plan.json",
                  "{ not valid json");

    UniPlan::FWatchSnapshotCache Cache;
    const UniPlan::FDocWatchSnapshot Snapshot =
        UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                          Cache, true);
    EXPECT_EQ(Snapshot.mInventory.mPlanCount, 1);
    EXPECT_TRUE(HasWarningContaining(Snapshot, "Broken.Plan.json"));
}

TEST_F(FBundleTestFixture, PhaseDetailPanelRendersDefaultAndMetricsViews)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);

    UniPlan::FWatchSnapshotCache Cache;
    const UniPlan::FDocWatchSnapshot Snapshot =
        UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                          Cache, true);
    const UniPlan::FWatchPlanSummary *Plan = FindPlan(Snapshot, "Alpha");
    ASSERT_NE(Plan, nullptr);

    const UniPlan::PhaseDetailPanel Panel;
    const std::string DefaultView =
        RenderElementToString(Panel.Render(*Plan, 0, false));
    const std::string MetricsView =
        RenderElementToString(Panel.Render(*Plan, 0, true));

    EXPECT_NE(DefaultView.find("Design"), std::string::npos);
    EXPECT_NE(DefaultView.find("Taxonomy"), std::string::npos);
    EXPECT_EQ(DefaultView.find("SOLID"), std::string::npos);
    EXPECT_NE(MetricsView.find("Design"), std::string::npos);
    EXPECT_NE(MetricsView.find("SOLID"), std::string::npos);
    EXPECT_NE(MetricsView.find("Evidence"), std::string::npos);
}

TEST_F(FBundleTestFixture, WatchMetricsMatchPhaseMetricCommand)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, true);

    UniPlan::FWatchSnapshotCache Cache;
    const UniPlan::FDocWatchSnapshot Snapshot =
        UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                          Cache, true);
    const UniPlan::FWatchPlanSummary *Plan = FindPlan(Snapshot, "Alpha");
    ASSERT_NE(Plan, nullptr);
    ASSERT_FALSE(Plan->mPhases.empty());

    StartCapture();
    const int Code =
        UniPlan::RunBundlePhaseCommand({"metric", "--topic", "Alpha", "--phase",
                                        "0", "--repo-root", mRepoRoot.string()},
                                       mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;
    const nlohmann::json Json = ParseCapturedJSON();
    ASSERT_EQ(Json["phases"].size(), 1u);
    EXPECT_EQ(Json["phases"][0]["design_chars"],
              Plan->mPhases[0].mMetrics.mDesignChars);
    EXPECT_EQ(Json["phases"][0]["solid_words"],
              Plan->mPhases[0].mMetrics.mSolidWordCount);
    EXPECT_EQ(Json["phases"][0]["recursive_words"],
              Plan->mPhases[0].mMetrics.mRecursiveWordCount);
}

TEST_F(FBundleTestFixture, WatchValidationSummaryMatchesValidateCommand)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);

    UniPlan::FWatchSnapshotCache Cache;
    const UniPlan::FDocWatchSnapshot Snapshot =
        UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                          Cache, true);

    StartCapture();
    const int Code = UniPlan::RunBundleValidateCommand(
        {"--repo-root", mRepoRoot.string()}, mRepoRoot.string());
    StopCapture();
    EXPECT_EQ(Code, 0);
    const nlohmann::json Json = ParseCapturedJSON();
    EXPECT_EQ(Json["error_major"], Snapshot.mValidation.mErrorMajorCount);
    EXPECT_EQ(Json["error_minor"], Snapshot.mValidation.mErrorMinorCount);
    int WarningCount = 0;
    for (const nlohmann::json &Issue : Json["issues"])
    {
        if (Issue["severity"] == "warning")
        {
            WarningCount++;
        }
    }
    EXPECT_EQ(WarningCount, Snapshot.mValidation.mWarningCount);
}

#endif // defined(UPLAN_WATCH)
