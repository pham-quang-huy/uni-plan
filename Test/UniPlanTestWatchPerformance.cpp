#include "UniPlanTestFixture.h"

#include "UniPlanBundleIndex.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanWatchSnapshot.h"

#include <fstream>
#include <utility>

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

void WriteBinaryFile(const fs::path &InPath, const std::string &InText)
{
    fs::create_directories(InPath.parent_path());
    std::ofstream Stream(InPath, std::ios::binary);
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

std::string RepeatString(const std::string &InText, const size_t InCount)
{
    std::string Result;
    for (size_t Index = 0; Index < InCount; ++Index)
    {
        Result += InText;
    }
    return Result;
}

std::string StripAnsiCodes(const std::string &InText)
{
    std::string Result;
    for (size_t Index = 0; Index < InText.size();)
    {
        if (InText[Index] == '\033' && Index + 1 < InText.size() &&
            InText[Index + 1] == '[')
        {
            Index += 2;
            while (Index < InText.size() &&
                   !(InText[Index] >= '@' && InText[Index] <= '~'))
            {
                ++Index;
            }
            if (Index < InText.size())
            {
                ++Index;
            }
            continue;
        }
        Result.push_back(InText[Index]);
        ++Index;
    }
    return Result;
}

std::string RenderElementToString(ftxui::Element InElement)
{
    ftxui::Screen Screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(220),
                                                 ftxui::Dimension::Fixed(40));
    ftxui::Render(Screen, InElement);
    return Screen.ToString();
}

std::string RenderElementToString(ftxui::Element InElement, int InWidth,
                                  int InHeight)
{
    ftxui::Screen Screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(InWidth), ftxui::Dimension::Fixed(InHeight));
    ftxui::Render(Screen, InElement);
    return Screen.ToString();
}

ftxui::Screen RenderElementToScreen(ftxui::Element InElement, int InWidth,
                                    int InHeight)
{
    ftxui::Screen Screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(InWidth), ftxui::Dimension::Fixed(InHeight));
    ftxui::Render(Screen, InElement);
    return Screen;
}

bool TryFindTextDim(const ftxui::Screen &InScreen,
                    const std::string &InNeedle, bool &OutDim)
{
    const int NeedleCells = static_cast<int>(InNeedle.size());
    for (int Y = 0; Y < InScreen.dimy(); ++Y)
    {
        for (int X = 0; X + NeedleCells <= InScreen.dimx(); ++X)
        {
            bool bMatched = true;
            for (int Index = 0; Index < NeedleCells; ++Index)
            {
                const std::string Expected(
                    1, InNeedle[static_cast<size_t>(Index)]);
                if (InScreen.PixelAt(X + Index, Y).character != Expected)
                {
                    bMatched = false;
                    break;
                }
            }
            if (bMatched)
            {
                OutDim = InScreen.PixelAt(X, Y).dim;
                return true;
            }
        }
    }
    return false;
}

std::vector<UniPlan::FWatchPlanSummary>
MakeWatchPlanRows(const std::string &InPrefix, const std::string &InStatus,
                  int InCount)
{
    std::vector<UniPlan::FWatchPlanSummary> Plans;
    for (int Index = 0; Index < InCount; ++Index)
    {
        UniPlan::FWatchPlanSummary Plan;
        const std::string Suffix =
            Index < 10 ? "0" + std::to_string(Index) : std::to_string(Index);
        Plan.mTopicKey = InPrefix + Suffix;
        Plan.mPlanStatus = InStatus;
        Plan.mPhaseCount = 1;
        if (InStatus == "in_progress")
        {
            Plan.mPhaseInProgress = 1;
        }
        else
        {
            Plan.mPhaseCompleted = 1;
        }
        Plans.push_back(Plan);
    }
    return Plans;
}

UniPlan::FWatchPlanSummary MakePhaseDetailPlan(const int InPhaseCount)
{
    UniPlan::FWatchPlanSummary Plan;
    Plan.mTopicKey = "PhaseScroll";
    Plan.mPlanStatus = "in_progress";
    Plan.mPhaseCount = InPhaseCount;

    for (int Index = 0; Index < InPhaseCount; ++Index)
    {
        UniPlan::PhaseItem Phase;
        const std::string Suffix =
            Index < 10 ? "0" + std::to_string(Index) : std::to_string(Index);
        Phase.mPhaseKey = std::to_string(Index);
        Phase.mStatus = UniPlan::EExecutionStatus::NotStarted;
        Phase.mScope = "Scope-" + Suffix;
        Phase.mOutput = "Output-" + Suffix;
        Phase.mV4DesignChars = static_cast<size_t>((Index + 1) * 100);
        Plan.mPhases.push_back(Phase);
    }

    return Plan;
}

UniPlan::FWatchPlanSummary MakeLaneTaxonomyPlan(const int InLaneCount)
{
    UniPlan::FWatchPlanSummary Plan = MakePhaseDetailPlan(1);
    Plan.mTopicKey = "LaneScroll";

    UniPlan::FPhaseTaxonomy Taxonomy;
    Taxonomy.mPhaseIndex = 0;
    Taxonomy.mWaveCount = 1;
    for (int Index = 0; Index < InLaneCount; ++Index)
    {
        UniPlan::FLaneRecord Lane;
        Lane.mStatus = UniPlan::EExecutionStatus::NotStarted;
        Lane.mScope = "LaneScope-" + std::to_string(Index);
        Lane.mExitCriteria = "LaneExit-" + std::to_string(Index);
        Taxonomy.mLanes.push_back(Lane);
    }
    Plan.mPhaseTaxonomies.push_back(Taxonomy);

    return Plan;
}

UniPlan::FWatchPlanSummary MakeCodeSnippetPlan(const std::string &InSnippets)
{
    UniPlan::FWatchPlanSummary Plan;
    Plan.mTopicKey = "CodePane";
    Plan.mPlanStatus = "in_progress";
    Plan.mPhaseCount = 1;

    UniPlan::PhaseItem Phase;
    Phase.mPhaseKey = "0";
    Phase.mStatus = UniPlan::EExecutionStatus::InProgress;
    Phase.mStatusRaw = "in_progress";
    Phase.mCodeSnippets = InSnippets;
    Plan.mPhases.push_back(std::move(Phase));

    return Plan;
}

UniPlan::FPhaseTaxonomy MakeFileManifestTaxonomy(const int InFileCount)
{
    UniPlan::FPhaseTaxonomy Taxonomy;
    Taxonomy.mPhaseIndex = 1;
    for (int Index = 0; Index < InFileCount; ++Index)
    {
        UniPlan::FFileManifestItem Item;
        const std::string Suffix =
            Index < 10 ? "0" + std::to_string(Index) : std::to_string(Index);
        Item.mFilePath = "Source/File-" + Suffix + ".cpp";
        Item.mAction = UniPlan::EFileAction::Modify;
        Item.mDescription = "Manifest row " + Suffix;
        Taxonomy.mFileManifest.push_back(std::move(Item));
    }
    return Taxonomy;
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
                EXPECT_EQ(Left.mPhases[PhaseIndex].mCodeSnippets,
                          Right.mPhases[PhaseIndex].mCodeSnippets);
            }
        }
    };

    ExpectPlansEquivalent(InLeft.mActivePlans, InRight.mActivePlans);
    ExpectPlansEquivalent(InLeft.mNonActivePlans, InRight.mNonActivePlans);
}

const UniPlan::FPhaseTaxonomy *FindTaxonomyForPhase(
    const UniPlan::FWatchPlanSummary &InPlan, const int InPhaseIndex)
{
    for (const UniPlan::FPhaseTaxonomy &Taxonomy : InPlan.mPhaseTaxonomies)
    {
        if (Taxonomy.mPhaseIndex == InPhaseIndex)
        {
            return &Taxonomy;
        }
    }
    return nullptr;
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

TEST_F(FBundleTestFixture, WatchSnapshotCanPostInventoryBeforeValidation)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, true);
    CreateMinimalFixture("Beta", UniPlan::ETopicStatus::NotStarted, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);

    UniPlan::FWatchSnapshotCache Cache;
    UniPlan::FWatchSnapshotBuildOptions FastOptions;
    FastOptions.mbRunValidation = false;
    FastOptions.mbRunLint = false;
    FastOptions.mbMarkSkippedValidationRunning = true;
    FastOptions.mbMarkSkippedLintRunning = true;

    const UniPlan::FDocWatchSnapshot Fast = UniPlan::BuildWatchSnapshotCached(
        mRepoRoot.string(), true, "", false, Cache, true, FastOptions);
    EXPECT_EQ(Fast.mInventory.mPlanCount, 2);
    EXPECT_EQ(Fast.mInventory.mActivePlanCount, 1);
    EXPECT_EQ(Fast.mInventory.mNonActivePlanCount, 1);
    EXPECT_NE(FindPlan(Fast, "Alpha"), nullptr);
    EXPECT_NE(FindPlan(Fast, "Beta"), nullptr);
    EXPECT_EQ(Fast.mValidation.mState,
              UniPlan::FWatchValidationSummary::EState::Running);
    EXPECT_EQ(Fast.mLint.mState, UniPlan::FWatchLintSummary::EState::Running);
    EXPECT_FALSE(Fast.mPerformance.mbValidationRan);
    EXPECT_FALSE(Fast.mPerformance.mbLintRan);

    const UniPlan::FDocWatchSnapshot Full = UniPlan::BuildWatchSnapshotCached(
        mRepoRoot.string(), true, "", false, Cache, false);
    EXPECT_EQ(Full.mPerformance.mBundleReloadCount, 0);
    EXPECT_EQ(Full.mPerformance.mBundleReuseCount, 2);
    EXPECT_TRUE(Full.mPerformance.mbValidationRan);
    EXPECT_TRUE(Full.mPerformance.mbLintRan);
    EXPECT_EQ(Full.mValidation.mState,
              UniPlan::FWatchValidationSummary::EState::Ready);
    EXPECT_EQ(Full.mLint.mState, UniPlan::FWatchLintSummary::EState::Ready);
}

TEST_F(FBundleTestFixture, WatchSnapshotShowsStaleValidationDuringRefresh)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);

    UniPlan::FWatchSnapshotCache Cache;
    const UniPlan::FDocWatchSnapshot Ready = UniPlan::BuildWatchSnapshotCached(
        mRepoRoot.string(), true, "", false, Cache, true);
    ASSERT_EQ(Ready.mValidation.mState,
              UniPlan::FWatchValidationSummary::EState::Ready);

    StartCapture();
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "Alpha", "--phase", "0", "--output", "Changed",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    UniPlan::FWatchSnapshotBuildOptions FastOptions;
    FastOptions.mbRunValidation = false;
    FastOptions.mbRunLint = false;
    FastOptions.mbMarkSkippedValidationRunning = true;
    FastOptions.mbMarkSkippedLintRunning = true;

    const UniPlan::FDocWatchSnapshot Fast = UniPlan::BuildWatchSnapshotCached(
        mRepoRoot.string(), true, "", false, Cache, false, FastOptions);
    EXPECT_EQ(Fast.mInventory.mPlanCount, 1);
    EXPECT_EQ(Fast.mValidation.mState,
              UniPlan::FWatchValidationSummary::EState::Stale);
    EXPECT_NE(Fast.mValidation.mStateMessage.find("running"),
              std::string::npos);
    EXPECT_FALSE(Fast.mPerformance.mbValidationRan);
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

TEST_F(FBundleTestFixture, PlanNavigatorPanelsUseEdgeScroll)
{
    const UniPlan::ActivePlansPanel ActivePanel;
    const UniPlan::NonActivePlansPanel NonActivePanel;
    const std::vector<UniPlan::FWatchPlanSummary> ActivePlans =
        MakeWatchPlanRows("ActiveTopic", "in_progress", 20);
    const std::vector<UniPlan::FWatchPlanSummary> NonActivePlans =
        MakeWatchPlanRows("DoneTopic", "completed", 20);

    UniPlan::FWatchScrollRegionState ActiveScroll;
    const std::string ActiveBeforeEdge = StripAnsiCodes(RenderElementToString(
        ActivePanel.Render(ActivePlans, 5, ActiveScroll), 80, 12));
    EXPECT_EQ(ActiveScroll.mOffset, 0);
    EXPECT_NE(ActiveBeforeEdge.find("ActiveTopic00"), std::string::npos);

    int PreviousOffset = ActiveScroll.mOffset;
    for (int Selected = 6; Selected < 15; ++Selected)
    {
        RenderElementToString(
            ActivePanel.Render(ActivePlans, Selected, ActiveScroll), 80, 12);
        EXPECT_LE(ActiveScroll.mOffset, PreviousOffset + 1);
        PreviousOffset = ActiveScroll.mOffset;
    }
    EXPECT_GT(ActiveScroll.mOffset, 0);

    UniPlan::FWatchScrollRegionState NonActiveScroll;
    const std::string NonActiveBeforeEdge =
        StripAnsiCodes(RenderElementToString(
            NonActivePanel.Render(NonActivePlans, 5, NonActiveScroll), 80,
            12));
    EXPECT_EQ(NonActiveScroll.mOffset, 0);
    EXPECT_NE(NonActiveBeforeEdge.find("DoneTopic00"), std::string::npos);

    PreviousOffset = NonActiveScroll.mOffset;
    for (int Selected = 6; Selected < 15; ++Selected)
    {
        RenderElementToString(NonActivePanel.Render(
                                  NonActivePlans, Selected, NonActiveScroll),
                              80, 12);
        EXPECT_LE(NonActiveScroll.mOffset, PreviousOffset + 1);
        PreviousOffset = NonActiveScroll.mOffset;
    }
    EXPECT_GT(NonActiveScroll.mOffset, 0);
}

TEST_F(FBundleTestFixture, SelectedRowPanelsUseEdgeScroll)
{
    const UniPlan::PhaseDetailPanel PhasePanel;
    const UniPlan::ExecutionTaxonomyPanel TaxonomyPanel;

    const UniPlan::FWatchPlanSummary PhasePlan = MakePhaseDetailPlan(30);
    UniPlan::FWatchScrollRegionState PhaseScroll;
    const std::string PhaseBeforeEdge =
        StripAnsiCodes(RenderElementToString(
            PhasePanel.Render(PhasePlan, 29, false, PhaseScroll), 100, 12));
    EXPECT_EQ(PhaseScroll.mOffset, 0);
    EXPECT_NE(PhaseBeforeEdge.find("Scope-29"), std::string::npos);
    EXPECT_NE(PhaseBeforeEdge.find("Scope-28"), std::string::npos);
    EXPECT_LT(PhaseBeforeEdge.find("Scope-29"),
              PhaseBeforeEdge.find("Scope-28"));

    int PreviousOffset = PhaseScroll.mOffset;
    for (int Selected = 28; Selected >= 10; --Selected)
    {
        RenderElementToString(
            PhasePanel.Render(PhasePlan, Selected, false, PhaseScroll), 100,
            12);
        EXPECT_LE(PhaseScroll.mOffset, PreviousOffset + 1);
        PreviousOffset = PhaseScroll.mOffset;
    }
    EXPECT_GT(PhaseScroll.mOffset, 0);
    const std::string PhaseAfterEdge =
        StripAnsiCodes(RenderElementToString(
            PhasePanel.Render(PhasePlan, 10, false, PhaseScroll), 100, 12));
    EXPECT_NE(PhaseAfterEdge.find("above"), std::string::npos);

    const UniPlan::FWatchPlanSummary LanePlan = MakeLaneTaxonomyPlan(24);
    UniPlan::FWatchScrollRegionState LaneScroll;
    const std::string LaneBeforeEdge =
        StripAnsiCodes(RenderElementToString(
            TaxonomyPanel.Render(LanePlan, 0, -1, 5, true, LaneScroll), 120,
            20));
    EXPECT_EQ(LaneScroll.mOffset, 0);
    EXPECT_NE(LaneBeforeEdge.find("LaneScope-0"), std::string::npos);

    PreviousOffset = LaneScroll.mOffset;
    for (int Selected = 6; Selected < 18; ++Selected)
    {
        RenderElementToString(TaxonomyPanel.Render(LanePlan, 0, -1, Selected,
                                                   true, LaneScroll),
                              120, 20);
        EXPECT_LE(LaneScroll.mOffset, PreviousOffset + 1);
        PreviousOffset = LaneScroll.mOffset;
    }
    EXPECT_GT(LaneScroll.mOffset, 0);
    const std::string LaneAfterEdge =
        StripAnsiCodes(RenderElementToString(TaxonomyPanel.Render(
                                                 LanePlan, 0, -1, 17, true,
                                                 LaneScroll),
                                             120, 20));
    EXPECT_NE(LaneAfterEdge.find("above"), std::string::npos);
}

TEST_F(FBundleTestFixture, FileManifestPanelScrollIndicatorsAreStable)
{
    const UniPlan::FileManifestPanel Panel;
    const UniPlan::FPhaseTaxonomy Taxonomy = MakeFileManifestTaxonomy(12);
    UniPlan::FWatchScrollRegionState ScrollState;

    const std::string First = StripAnsiCodes(RenderElementToString(
        Panel.Render(Taxonomy, ScrollState), 80, 8));
    EXPECT_EQ(ScrollState.mOffset, 0);
    EXPECT_GT(ScrollState.mMaxOffset, 0);
    EXPECT_NE(First.find("FILES: P1 (12)"), std::string::npos);
    EXPECT_NE(First.find("File-00.cpp"), std::string::npos);
    EXPECT_NE(First.find("below"), std::string::npos);

    const std::string Second = StripAnsiCodes(RenderElementToString(
        Panel.Render(Taxonomy, ScrollState), 80, 8));
    EXPECT_EQ(Second, First);
}

TEST_F(FBundleTestFixture, WatchSnapshotKeepsManifestOnlyPhaseTaxonomy)
{
    CreateMinimalFixture("FilePane", UniPlan::ETopicStatus::InProgress, 2,
                         UniPlan::EExecutionStatus::NotStarted, true);

    StartCapture();
    const int Code = UniPlan::RunManifestAddCommand(
        {"--topic", "FilePane", "--phase", "1", "--file",
         "Source/FilePane.cpp", "--action", "modify", "--description",
         "Phase one manifest-only file", "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    UniPlan::FWatchSnapshotCache Cache;
    UniPlan::FWatchSnapshotBuildOptions FastOptions;
    FastOptions.mbRunValidation = false;
    FastOptions.mbRunLint = false;
    FastOptions.mbMarkSkippedValidationRunning = true;
    FastOptions.mbMarkSkippedLintRunning = true;

    const UniPlan::FDocWatchSnapshot FastSnapshot =
        UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                          Cache, true, FastOptions);
    const UniPlan::FDocWatchSnapshot FullSnapshot =
        UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                          Cache, false);

    const UniPlan::FWatchPlanSummary *FastPlan =
        FindPlan(FastSnapshot, "FilePane");
    const UniPlan::FWatchPlanSummary *FullPlan =
        FindPlan(FullSnapshot, "FilePane");
    ASSERT_NE(FastPlan, nullptr);
    ASSERT_NE(FullPlan, nullptr);

    const UniPlan::FPhaseTaxonomy *FastTaxonomy =
        FindTaxonomyForPhase(*FastPlan, 1);
    const UniPlan::FPhaseTaxonomy *FullTaxonomy =
        FindTaxonomyForPhase(*FullPlan, 1);
    ASSERT_NE(FastTaxonomy, nullptr);
    ASSERT_NE(FullTaxonomy, nullptr);
    ASSERT_EQ(FastTaxonomy->mFileManifest.size(), 1u);
    ASSERT_EQ(FullTaxonomy->mFileManifest.size(), 1u);
    EXPECT_EQ(FastTaxonomy->mFileManifest[0].mFilePath,
              "Source/FilePane.cpp");
    EXPECT_EQ(FullTaxonomy->mFileManifest[0].mFilePath,
              "Source/FilePane.cpp");
}

TEST_F(FBundleTestFixture, WatchSnapshotProjectsPhaseCodeSnippets)
{
    CreateMinimalFixture("CodePane", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);

    const std::string Snippets =
        "void Tick()\n{\n    ApplyPhaseSelection();\n}\n";
    const fs::path SnippetPath = mRepoRoot / "snippet.txt";
    WriteBinaryFile(SnippetPath, Snippets);

    StartCapture();
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "CodePane", "--phase", "0", "--code-snippets-file",
         SnippetPath.string(), "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    UniPlan::FWatchSnapshotCache Cache;
    const UniPlan::FDocWatchSnapshot Snapshot =
        UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                          Cache, true);
    const UniPlan::FWatchPlanSummary *Plan = FindPlan(Snapshot, "CodePane");
    ASSERT_NE(Plan, nullptr);
    ASSERT_FALSE(Plan->mPhases.empty());
    EXPECT_EQ(Plan->mPhases[0].mCodeSnippets, Snippets);
}

TEST_F(FBundleTestFixture, CodeSnippetPanelRendersLineNumbersAndTitle)
{
    const UniPlan::FWatchPlanSummary Plan =
        MakeCodeSnippetPlan("int main()\n{\n    return 0;\n}");

    const UniPlan::CodeSnippetPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const std::string Rendered = StripAnsiCodes(
        RenderElementToString(Panel.Render(Plan, 0, ScrollState), 70, 10));

    EXPECT_NE(Rendered.find("CODE SNIPPETS: CodePane P0 (4 lines)"),
              std::string::npos);
    EXPECT_NE(Rendered.find("1 int main()"), std::string::npos);
    EXPECT_NE(Rendered.find("2 {"), std::string::npos);
    EXPECT_NE(Rendered.find("3     return 0;"), std::string::npos);
}

TEST_F(FBundleTestFixture, CodeSnippetPanelDimsTextOutsideCppFence)
{
    const UniPlan::FWatchPlanSummary Plan = MakeCodeSnippetPlan(
        "intro prose\n```cpp\nint main()\n```\noutro prose");

    const UniPlan::CodeSnippetPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const ftxui::Screen Screen =
        RenderElementToScreen(Panel.Render(Plan, 0, ScrollState), 80, 12);

    bool bDim = false;
    ASSERT_TRUE(TryFindTextDim(Screen, "intro prose", bDim));
    EXPECT_TRUE(bDim);
    ASSERT_TRUE(TryFindTextDim(Screen, "```cpp", bDim));
    EXPECT_TRUE(bDim);
    ASSERT_TRUE(TryFindTextDim(Screen, "int main()", bDim));
    EXPECT_FALSE(bDim);
    ASSERT_TRUE(TryFindTextDim(Screen, "outro prose", bDim));
    EXPECT_TRUE(bDim);
}

TEST_F(FBundleTestFixture, CodeSnippetPanelKeepsPlainSnippetBright)
{
    const UniPlan::FWatchPlanSummary Plan =
        MakeCodeSnippetPlan("int legacy_plain_code = 1;");

    const UniPlan::CodeSnippetPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const ftxui::Screen Screen =
        RenderElementToScreen(Panel.Render(Plan, 0, ScrollState), 80, 8);

    bool bDim = true;
    ASSERT_TRUE(TryFindTextDim(Screen, "int legacy_plain_code = 1;", bDim));
    EXPECT_FALSE(bDim);
}

TEST_F(FBundleTestFixture, CodeSnippetPanelStylesMultipleCppFences)
{
    const UniPlan::FWatchPlanSummary Plan = MakeCodeSnippetPlan(
        "before\n```CPP\nint First;\n```\nbetween\n```c++\nint Second;\n```");

    const UniPlan::CodeSnippetPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const ftxui::Screen Screen =
        RenderElementToScreen(Panel.Render(Plan, 0, ScrollState), 80, 14);

    bool bDim = false;
    ASSERT_TRUE(TryFindTextDim(Screen, "before", bDim));
    EXPECT_TRUE(bDim);
    ASSERT_TRUE(TryFindTextDim(Screen, "int First;", bDim));
    EXPECT_FALSE(bDim);
    ASSERT_TRUE(TryFindTextDim(Screen, "between", bDim));
    EXPECT_TRUE(bDim);
    ASSERT_TRUE(TryFindTextDim(Screen, "int Second;", bDim));
    EXPECT_FALSE(bDim);
}

TEST_F(FBundleTestFixture, CodeSnippetPanelStylesUnclosedCppFenceToEnd)
{
    const UniPlan::FWatchPlanSummary Plan =
        MakeCodeSnippetPlan("before\n```cpp\nint Tail;");

    const UniPlan::CodeSnippetPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const ftxui::Screen Screen =
        RenderElementToScreen(Panel.Render(Plan, 0, ScrollState), 80, 10);

    bool bDim = false;
    ASSERT_TRUE(TryFindTextDim(Screen, "before", bDim));
    EXPECT_TRUE(bDim);
    ASSERT_TRUE(TryFindTextDim(Screen, "```cpp", bDim));
    EXPECT_TRUE(bDim);
    ASSERT_TRUE(TryFindTextDim(Screen, "int Tail;", bDim));
    EXPECT_FALSE(bDim);
}

TEST_F(FBundleTestFixture, CodeSnippetPanelRendersEmptyState)
{
    const UniPlan::FWatchPlanSummary Plan = MakeCodeSnippetPlan("");

    const UniPlan::CodeSnippetPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    ScrollState.mOffset = 7;
    ScrollState.mMaxOffset = 9;
    const std::string Rendered = StripAnsiCodes(
        RenderElementToString(Panel.Render(Plan, 0, ScrollState), 60, 7));

    EXPECT_NE(Rendered.find("(no code snippets)"), std::string::npos);
    EXPECT_EQ(ScrollState.mOffset, 0);
    EXPECT_EQ(ScrollState.mMaxOffset, 0);
}

TEST_F(FBundleTestFixture, CodeSnippetPanelWrapsAndScrollsOneVisualLine)
{
    const std::string Snippet =
        "first\n"
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n"
        "final sentinel WRAPTAIL";
    const UniPlan::FWatchPlanSummary Plan = MakeCodeSnippetPlan(Snippet);

    const UniPlan::CodeSnippetPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const std::string First = StripAnsiCodes(
        RenderElementToString(Panel.Render(Plan, 0, ScrollState), 36, 7));
    ASSERT_GT(ScrollState.mMaxOffset, 0);
    EXPECT_EQ(ScrollState.mOffset, 0);
    EXPECT_NE(First.find("below"), std::string::npos);

    ++ScrollState.mOffset;
    const std::string Second = StripAnsiCodes(
        RenderElementToString(Panel.Render(Plan, 0, ScrollState), 36, 7));
    EXPECT_EQ(ScrollState.mOffset, 1);
    EXPECT_NE(Second.find("above"), std::string::npos);

    ScrollState.mOffset = 999;
    const std::string Last = StripAnsiCodes(
        RenderElementToString(Panel.Render(Plan, 0, ScrollState), 36, 7));
    EXPECT_EQ(ScrollState.mOffset, ScrollState.mMaxOffset);
    EXPECT_NE(Last.find("WRAPTAIL"), std::string::npos);
    EXPECT_EQ(Last.find("..."), std::string::npos);
}

TEST_F(FBundleTestFixture, WatchStatusBarListsCodePaneKeys)
{
    const UniPlan::WatchStatusBar Panel;
    UniPlan::FWatchInventoryCounters Counters;
    UniPlan::FDocWatchSnapshot::FPerformance Performance;

    const std::string Rendered =
        StripAnsiCodes(RenderElementToString(Panel.Render(
            "0.109.0", "now", 23, 150, Counters, Performance)));

    EXPECT_EQ(Rendered.find("s=schema"), std::string::npos);
    EXPECT_EQ(Rendered.find("i=impl"), std::string::npos);
    EXPECT_NE(Rendered.find("F12=code"), std::string::npos);
    EXPECT_NE(Rendered.find("[=up ]=down"), std::string::npos);
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
    UniPlan::FWatchScrollRegionState ScrollState;
    const std::string DefaultView =
        RenderElementToString(Panel.Render(*Plan, 0, false, ScrollState));
    const std::string MetricsView =
        RenderElementToString(Panel.Render(*Plan, 0, true, ScrollState));
    const std::string PlainMetricsView = StripAnsiCodes(MetricsView);

    EXPECT_NE(DefaultView.find("Design"), std::string::npos);
    EXPECT_NE(DefaultView.find("Taxonomy"), std::string::npos);
    EXPECT_NE(DefaultView.find("Scope"), std::string::npos);
    EXPECT_EQ(DefaultView.find("SOLID"), std::string::npos);
    EXPECT_NE(MetricsView.find("Design"), std::string::npos);
    EXPECT_NE(MetricsView.find("Scope"), std::string::npos);
    EXPECT_NE(MetricsView.find("Phase 0"), std::string::npos);
    EXPECT_NE(MetricsView.find("SOLID"), std::string::npos);
    EXPECT_NE(MetricsView.find("Evidence"), std::string::npos);
    const size_t EvidenceColumn = PlainMetricsView.find("Evidence");
    const size_t ScopeColumn = PlainMetricsView.find("Scope");
    ASSERT_NE(EvidenceColumn, std::string::npos);
    ASSERT_NE(ScopeColumn, std::string::npos);
    EXPECT_LT(EvidenceColumn, ScopeColumn);
}

TEST_F(FBundleTestFixture, PhaseDetailPanelKeepsWideNumericLabelsVisible)
{
    UniPlan::FWatchPlanSummary Plan;
    Plan.mTopicKey = "WideNumbers";
    Plan.mPlanStatus = "in_progress";

    UniPlan::PhaseItem Phase;
    Phase.mPhaseKey = "1234";
    Phase.mStatus = UniPlan::EExecutionStatus::NotStarted;
    Phase.mScope = "Retain scope context";
    Phase.mV4DesignChars = 199307;
    Phase.mMetrics.mDesignChars = 199307;
    Phase.mMetrics.mSolidWordCount = 123456;
    Phase.mMetrics.mRecursiveWordCount = 987654;
    Phase.mMetrics.mFieldCoveragePercent = 100;
    Phase.mMetrics.mWorkItemCount = 12345;
    Phase.mMetrics.mTestingRecordCount = 67890;
    Phase.mMetrics.mFileManifestCount = 54321;
    Phase.mMetrics.mEvidenceItemCount = 24680;
    Plan.mPhases.push_back(Phase);

    const UniPlan::PhaseDetailPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const std::string DefaultView =
        StripAnsiCodes(
            RenderElementToString(Panel.Render(Plan, 0, false, ScrollState)));
    const std::string MetricsView =
        StripAnsiCodes(
            RenderElementToString(Panel.Render(Plan, 0, true, ScrollState)));

    EXPECT_NE(DefaultView.find("199307"), std::string::npos);
    EXPECT_NE(DefaultView.find("1234"), std::string::npos);
    EXPECT_NE(MetricsView.find("199307"), std::string::npos);
    EXPECT_NE(MetricsView.find("123456"), std::string::npos);
    EXPECT_NE(MetricsView.find("987654"), std::string::npos);
    EXPECT_NE(MetricsView.find("67890"), std::string::npos);
}

TEST_F(FBundleTestFixture, PhaseDetailPanelKeepsRichMetricsComparable)
{
    UniPlan::FWatchPlanSummary Plan;
    Plan.mTopicKey = "Scale";
    Plan.mPlanStatus = "in_progress";

    UniPlan::PhaseItem Low;
    Low.mPhaseKey = "0";
    Low.mStatus = UniPlan::EExecutionStatus::NotStarted;
    Low.mMetrics.mDesignChars = 20000;
    Low.mMetrics.mSolidWordCount = 50;
    Low.mMetrics.mRecursiveWordCount = 3000;
    Low.mMetrics.mFieldCoveragePercent = 90;
    Low.mMetrics.mWorkItemCount = 30;
    Low.mMetrics.mTestingRecordCount = 4;
    Low.mMetrics.mFileManifestCount = 8;
    Low.mMetrics.mEvidenceItemCount = 10;

    UniPlan::PhaseItem High = Low;
    High.mPhaseKey = "1";
    High.mMetrics.mDesignChars = 40000;
    High.mMetrics.mSolidWordCount = 100;
    High.mMetrics.mRecursiveWordCount = 6000;
    High.mMetrics.mFieldCoveragePercent = 100;
    High.mMetrics.mWorkItemCount = 60;
    High.mMetrics.mTestingRecordCount = 8;
    High.mMetrics.mFileManifestCount = 16;
    High.mMetrics.mEvidenceItemCount = 20;

    Plan.mPhases.push_back(Low);
    Plan.mPhases.push_back(High);

    const UniPlan::PhaseDetailPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const std::string MetricsView =
        RenderElementToString(Panel.Render(Plan, 0, true, ScrollState));

    EXPECT_NE(MetricsView.find("20000"), std::string::npos);
    EXPECT_NE(MetricsView.find("40000"), std::string::npos);
    EXPECT_NE(MetricsView.find("\xe2\x96\x91"), std::string::npos);
}

TEST_F(FBundleTestFixture,
       PhaseDetailPanelDoesNotSaturateFieldWorkAndTestBaselines)
{
    UniPlan::FWatchPlanSummary Plan;
    Plan.mTopicKey = "Scale";
    Plan.mPlanStatus = "in_progress";

    UniPlan::PhaseItem Phase;
    Phase.mPhaseKey = "0";
    Phase.mStatus = UniPlan::EExecutionStatus::NotStarted;
    Phase.mScope = "Retain scope context";
    Phase.mMetrics.mFieldCoveragePercent = 86;
    Phase.mMetrics.mWorkItemCount = 20;
    Phase.mMetrics.mTestingRecordCount = 3;
    Plan.mPhases.push_back(Phase);

    const UniPlan::PhaseDetailPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const std::string MetricsView =
        RenderElementToString(Panel.Render(Plan, 0, true, ScrollState));
    const std::string PlainMetricsView = StripAnsiCodes(MetricsView);
    const std::string Full = "\xe2\x96\x88";
    const std::string Empty = "\xe2\x96\x91";

    EXPECT_NE(PlainMetricsView.find(RepeatString(Full, 6) +
                                    RepeatString(Empty, 2) + " 86%"),
              std::string::npos);
    EXPECT_EQ(PlainMetricsView.find(RepeatString(Full, 8) + " 86%"),
              std::string::npos);
    EXPECT_NE(PlainMetricsView.find(RepeatString(Full, 4) +
                                    RepeatString(Empty, 4) + " 20"),
              std::string::npos);
    EXPECT_EQ(PlainMetricsView.find(RepeatString(Full, 8) + " 20"),
              std::string::npos);
    EXPECT_NE(PlainMetricsView.find(RepeatString(Full, 3) +
                                    RepeatString(Empty, 5) + " 3"),
              std::string::npos);
    EXPECT_EQ(PlainMetricsView.find(RepeatString(Full, 8) + " 3"),
              std::string::npos);
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
    EXPECT_EQ(Json["phases"][0]["largest_design_field_chars"],
              Plan->mPhases[0].mMetrics.mLargestDesignFieldChars);
    EXPECT_EQ(Json["phases"][0]["repeated_design_block_count"],
              Plan->mPhases[0].mMetrics.mRepeatedDesignBlockCount);
    EXPECT_EQ(Json["phases"][0]["design_bloat_ratio"],
              Plan->mPhases[0].mMetrics.mDesignBloatRatio);
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

TEST_F(FBundleTestFixture, ValidationPanelsRenderRunningState)
{
    UniPlan::FWatchValidationSummary Validation;
    Validation.mState = UniPlan::FWatchValidationSummary::EState::Running;
    Validation.mStateMessage = "Validation running";

    const UniPlan::ValidationPanel SummaryPanel;
    const UniPlan::ValidationFailPanel FailurePanel;
    const std::string Summary =
        StripAnsiCodes(RenderElementToString(SummaryPanel.Render(Validation)));
    const std::string Failures =
        StripAnsiCodes(RenderElementToString(FailurePanel.Render(Validation)));

    EXPECT_NE(Summary.find("Validation running"), std::string::npos);
    EXPECT_NE(Summary.find("Plan inventory is available"), std::string::npos);
    EXPECT_EQ(Summary.find("All checks passed"), std::string::npos);
    EXPECT_NE(Failures.find("Validation running"), std::string::npos);
    EXPECT_EQ(Failures.find("All checks passed"), std::string::npos);
}

#endif // defined(UPLAN_WATCH)
