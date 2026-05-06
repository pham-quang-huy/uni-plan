#include "UniPlanTestFixture.h"

#include "UniPlanBundleIndex.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanWatchSnapshot.h"

#include <fstream>
#include <utility>

#if defined(UPLAN_WATCH)

#include "UniPlanWatchInteraction.h"
#include "UniPlanWatchPanels.h"

#include <ftxui/dom/node.hpp>
#include <ftxui/screen/color.hpp>
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

bool BundleIndexContainsTopic(const UniPlan::FBundleFileIndexResult &InIndex,
                              const std::string &InTopic)
{
    for (const UniPlan::FBundleFileIndexEntry &Entry : InIndex.mBundles)
    {
        if (Entry.mTopicKey == InTopic)
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

bool TryFindTextForeground(const ftxui::Screen &InScreen,
                           const std::string &InNeedle,
                           ftxui::Color &OutColor)
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
                OutColor = InScreen.PixelAt(X, Y).foreground_color;
                return true;
            }
        }
    }
    return false;
}

bool TryFindTextBackground(const ftxui::Screen &InScreen,
                           const std::string &InNeedle,
                           ftxui::Color &OutColor)
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
                OutColor = InScreen.PixelAt(X, Y).background_color;
                return true;
            }
        }
    }
    return false;
}

bool TryFindTextBackgroundAfter(const ftxui::Screen &InScreen,
                                const std::string &InNeedle,
                                const int InCellsAfter,
                                ftxui::Color &OutColor)
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
                const int TargetX = X + NeedleCells + InCellsAfter;
                if (TargetX >= InScreen.dimx())
                {
                    return false;
                }
                OutColor = InScreen.PixelAt(TargetX, Y).background_color;
                return true;
            }
        }
    }
    return false;
}

bool ScreenHasControlGlyph(const ftxui::Screen &InScreen)
{
    for (int Y = 0; Y < InScreen.dimy(); ++Y)
    {
        for (int X = 0; X < InScreen.dimx(); ++X)
        {
            for (const unsigned char Character :
                 InScreen.PixelAt(X, Y).character)
            {
                if (Character < 0x20 || Character == 0x7f)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

ftxui::Color PhaseDetailsCodeBackgroundForTest()
{
    return ftxui::Color(static_cast<uint8_t>(0), static_cast<uint8_t>(24),
                        static_cast<uint8_t>(40));
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

UniPlan::FWatchPlanSummary MakePhaseListPlan(const int InPhaseCount)
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
    UniPlan::FWatchPlanSummary Plan = MakePhaseListPlan(1);
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

UniPlan::FWatchPlanSummary MakeRichTaxonomyPlan(const int InLaneCount,
                                                const int InJobsPerLane,
                                                const int InTasksPerJob)
{
    UniPlan::FWatchPlanSummary Plan = MakePhaseListPlan(1);
    Plan.mTopicKey = "TaxonomyCache";

    UniPlan::FPhaseTaxonomy Taxonomy;
    Taxonomy.mPhaseIndex = 0;
    Taxonomy.mWaveCount = 2;
    for (int LaneIndex = 0; LaneIndex < InLaneCount; ++LaneIndex)
    {
        UniPlan::FLaneRecord Lane;
        Lane.mStatus = LaneIndex % 2 == 0
                           ? UniPlan::EExecutionStatus::Completed
                           : UniPlan::EExecutionStatus::InProgress;
        Lane.mScope = "Lane scope " + std::to_string(LaneIndex);
        Lane.mExitCriteria = "Lane exit " + std::to_string(LaneIndex);
        Taxonomy.mLanes.push_back(std::move(Lane));

        for (int JobIndex = 0; JobIndex < InJobsPerLane; ++JobIndex)
        {
            UniPlan::FJobRecord Job;
            Job.mWave = JobIndex % 2;
            Job.mLane = LaneIndex;
            Job.mStatus = JobIndex % 3 == 0
                              ? UniPlan::EExecutionStatus::Completed
                              : UniPlan::EExecutionStatus::NotStarted;
            Job.mScope = "Job scope L" + std::to_string(LaneIndex) + " J" +
                         std::to_string(JobIndex);

            for (int TaskIndex = 0; TaskIndex < InTasksPerJob; ++TaskIndex)
            {
                UniPlan::FTaskRecord Task;
                Task.mStatus = TaskIndex % 2 == 0
                                   ? UniPlan::EExecutionStatus::Completed
                                   : UniPlan::EExecutionStatus::NotStarted;
                Task.mDescription =
                    "Task description " + std::to_string(TaskIndex);
                Task.mEvidence =
                    "Task evidence " + std::to_string(TaskIndex);
                Job.mTasks.push_back(Task);
                Taxonomy.mTasks.push_back(std::move(Task));
            }
            Taxonomy.mJobs.push_back(std::move(Job));
        }
    }

    Plan.mPhaseTaxonomies.push_back(std::move(Taxonomy));
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

UniPlan::FWatchPlanSummary MakePhaseDetailsPlan()
{
    UniPlan::FWatchPlanSummary Plan;
    Plan.mTopicKey = "PhaseSide";
    Plan.mPlanStatus = "in_progress";
    Plan.mPhaseCount = 1;

    UniPlan::PhaseItem Phase;
    Phase.mPhaseKey = "0";
    Phase.mStatus = UniPlan::EExecutionStatus::InProgress;
    Phase.mStatusRaw = "in_progress";
    Phase.mScope =
        "Build a durable side pane for selected phase details";
    Phase.mOutput = "Phase details render from typed snapshot fields";
    Phase.mDone = "Panel plumbing implemented";
    Phase.mRemaining = "Cross-platform verification";
    Phase.mBlockers = "No blocker";
    Phase.mStartedAt = "2026-05-06T08:00:00Z";
    Phase.mCompletedAt = "";
    Phase.mAgentContext = "Use the watch panel architecture";
    Phase.mInvestigation = "Side panes need replace-one-at-a-time behavior";
    Phase.mCodeEntityContract = "No raw JSON access in watch projection";
    Phase.mBestPractices = "Keep panels stateless";
    Phase.mMultiPlatforming = "Windows and macOS share render code";
    Phase.mReadinessGate = "Tests cover gutter and scroll behavior";
    Phase.mHandoff = "F5 opens phase details, F12 opens code";
    Phase.mCodeSnippets = "```cpp\nint RawCodeSentinel = 1;\n```";
    Phase.mV4DesignChars = 1234;

    UniPlan::FBundleReference Dependency;
    Dependency.mKind = UniPlan::EDependencyKind::Phase;
    Dependency.mTopic = "Audio";
    Dependency.mPhase = 2;
    Dependency.mNote = "phase detail dependency";
    Phase.mDependencies.push_back(std::move(Dependency));

    UniPlan::FValidationCommand Command;
    Command.mPlatform = UniPlan::EPlatformScope::Windows;
    Command.mCommand = ".\\build.ps1 -Tests";
    Command.mDescription = "Windows test suite passes";
    Phase.mValidationCommands.push_back(std::move(Command));

    UniPlan::FTestingRecord Testing;
    Testing.mActor = UniPlan::ETestingActor::Automated;
    Testing.mSession = "watch-panel";
    Testing.mStep = "Render side pane";
    Testing.mAction = "Open F5";
    Testing.mExpected = "Phase details visible";
    Testing.mEvidence = "unit test";
    Phase.mTesting.push_back(std::move(Testing));

    Plan.mPhases.push_back(std::move(Phase));

    UniPlan::FPhaseTaxonomy Taxonomy;
    Taxonomy.mPhaseIndex = 0;
    Taxonomy.mWaveCount = 1;

    UniPlan::FLaneRecord Lane;
    Lane.mStatus = UniPlan::EExecutionStatus::InProgress;
    Lane.mScope = "Lane scope";
    Lane.mExitCriteria = "Lane exit";
    Taxonomy.mLanes.push_back(std::move(Lane));

    UniPlan::FTaskRecord Task;
    Task.mStatus = UniPlan::EExecutionStatus::NotStarted;
    Task.mDescription = "Task description";
    Task.mEvidence = "Task evidence";
    Task.mNotes = "Task notes";

    UniPlan::FJobRecord Job;
    Job.mWave = 0;
    Job.mLane = 0;
    Job.mStatus = UniPlan::EExecutionStatus::InProgress;
    Job.mScope = "Job scope";
    Job.mOutput = "Job output";
    Job.mExitCriteria = "Job exit";
    Job.mStartedAt = "2026-05-06T08:05:00Z";
    Job.mTasks.push_back(Task);
    Taxonomy.mJobs.push_back(std::move(Job));
    Taxonomy.mTasks.push_back(std::move(Task));

    UniPlan::FFileManifestItem File;
    File.mAction = UniPlan::EFileAction::Modify;
    File.mFilePath = "Source/PhaseSide.cpp";
    File.mDescription = "phase side pane implementation";
    Taxonomy.mFileManifest.push_back(std::move(File));

    Plan.mPhaseTaxonomies.push_back(std::move(Taxonomy));
    return Plan;
}

UniPlan::FWatchPlanSummary MakePlanDetailWrapPlan()
{
    UniPlan::FWatchPlanSummary Plan;
    Plan.mTopicKey = "WrapPlan";
    Plan.mPlanStatus = "in_progress";
    Plan.mPhaseCount = 1;
    Plan.mSummaryLines.push_back(
        "alpha beta gamma delta epsilon zeta eta theta iota kappa lambda");
    Plan.mGoalStatements.push_back(
        "Extend the renderer with stable word wrapping for readable panels");
    Plan.mNonGoalStatements.push_back(
        "Do not mutate stored prose or add ellipsis truncation");

    UniPlan::FRiskEntry Risk;
    Risk.mSeverity = UniPlan::ERiskSeverity::High;
    Risk.mStatus = UniPlan::ERiskStatus::Open;
    Risk.mStatement =
        "Plan detail prose can overflow narrow panes unless wrapping works";
    Plan.mRiskEntries.push_back(std::move(Risk));

    UniPlan::FNextActionEntry Action;
    Action.mOrder = 1;
    Action.mStatus = UniPlan::EActionStatus::Pending;
    Action.mStatement =
        "Verify continuation rows keep words whole while using blank gutters";
    Plan.mNextActionEntries.push_back(std::move(Action));

    UniPlan::FAcceptanceCriterionEntry Criterion;
    Criterion.mId = "WRAP-AC1";
    Criterion.mStatus = UniPlan::ECriterionStatus::Met;
    Criterion.mStatement =
        "Wrapped plan detail text renders with line numbers and no padding";
    Plan.mAcceptanceCriteria.push_back(std::move(Criterion));

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

TEST(WatchInteraction, PhaseNavigationKeepsPhaseListScrollStable)
{
    UniPlan::FWatchInteractionState State;
    State.mSelectedPhaseIndex = -1;
    State.mSelectedWaveIndex = 2;
    State.mSelectedLaneIndex = 3;
    State.mScrollState.mPhaseList.mOffset = 7;
    State.mScrollState.mPhaseList.mMaxOffset = 10;
    State.mScrollState.mLanes.mOffset = 4;
    State.mScrollState.mFileManifest.mOffset = 5;
    State.mScrollState.mPhaseDetails.mOffset = 6;
    State.mScrollState.mCodeSnippets.mOffset = 8;

    UniPlan::StepWatchPhaseSelection(State, 5, -1);

    EXPECT_EQ(State.mSelectedPhaseIndex, 4);
    EXPECT_EQ(State.mSelectedWaveIndex, -1);
    EXPECT_EQ(State.mSelectedLaneIndex, -1);
    EXPECT_EQ(State.mScrollState.mPhaseList.mOffset, 7);
    EXPECT_EQ(State.mScrollState.mPhaseList.mMaxOffset, 10);
    EXPECT_EQ(State.mScrollState.mLanes.mOffset, 0);
    EXPECT_EQ(State.mScrollState.mFileManifest.mOffset, 0);
    EXPECT_EQ(State.mScrollState.mPhaseDetails.mOffset, 0);
    EXPECT_EQ(State.mScrollState.mCodeSnippets.mOffset, 0);

    UniPlan::StepWatchPhaseSelection(State, 5, -1);
    EXPECT_EQ(State.mSelectedPhaseIndex, 3);

    UniPlan::StepWatchPhaseSelection(State, 5, 1);
    EXPECT_EQ(State.mSelectedPhaseIndex, 4);
}

TEST(WatchInteraction, PlanScopedResetClearsPhaseSelectionAndPhaseScroll)
{
    UniPlan::FWatchInteractionState State;
    State.mSelectedPhaseIndex = 3;
    State.mSelectedWaveIndex = 2;
    State.mSelectedLaneIndex = 1;
    State.mScrollState.mActivePlans.mOffset = 9;
    State.mScrollState.mNonActivePlans.mOffset = 8;
    State.mScrollState.mPhaseList.mOffset = 7;
    State.mScrollState.mLanes.mOffset = 6;
    State.mScrollState.mFileManifest.mOffset = 5;
    State.mScrollState.mPhaseDetails.mOffset = 4;
    State.mScrollState.mCodeSnippets.mOffset = 3;

    UniPlan::ResetWatchPlanScopedScroll(State);

    EXPECT_EQ(State.mSelectedPhaseIndex, -1);
    EXPECT_EQ(State.mSelectedWaveIndex, -1);
    EXPECT_EQ(State.mSelectedLaneIndex, -1);
    EXPECT_EQ(State.mScrollState.mActivePlans.mOffset, 9);
    EXPECT_EQ(State.mScrollState.mNonActivePlans.mOffset, 8);
    EXPECT_EQ(State.mScrollState.mPhaseList.mOffset, 0);
    EXPECT_EQ(State.mScrollState.mLanes.mOffset, 0);
    EXPECT_EQ(State.mScrollState.mFileManifest.mOffset, 0);
    EXPECT_EQ(State.mScrollState.mPhaseDetails.mOffset, 0);
    EXPECT_EQ(State.mScrollState.mCodeSnippets.mOffset, 0);
}

TEST(WatchInteraction, SidePaneToggleIsExclusiveAndResetsOpenedPane)
{
    UniPlan::FWatchInteractionState State;
    State.mScrollState.mPhaseDetails.mOffset = 5;
    State.mScrollState.mPhaseDetails.mMaxOffset = 9;
    State.mScrollState.mFileManifest.mOffset = 7;
    State.mScrollState.mFileManifest.mMaxOffset = 13;
    State.mScrollState.mCodeSnippets.mOffset = 6;
    State.mScrollState.mCodeSnippets.mMaxOffset = 11;

    UniPlan::ToggleWatchSidePane(State, UniPlan::EWatchSidePane::PhaseDetails);

    EXPECT_EQ(State.mSidePane, UniPlan::EWatchSidePane::PhaseDetails);
    EXPECT_EQ(State.mScrollState.mPhaseDetails.mOffset, 0);
    EXPECT_EQ(State.mScrollState.mPhaseDetails.mMaxOffset, 0);
    EXPECT_EQ(State.mScrollState.mFileManifest.mOffset, 7);
    EXPECT_EQ(State.mScrollState.mCodeSnippets.mOffset, 6);

    UniPlan::ToggleWatchSidePane(State, UniPlan::EWatchSidePane::FileManifest);

    EXPECT_EQ(State.mSidePane, UniPlan::EWatchSidePane::FileManifest);
    EXPECT_EQ(State.mScrollState.mFileManifest.mOffset, 0);
    EXPECT_EQ(State.mScrollState.mFileManifest.mMaxOffset, 0);
    EXPECT_EQ(State.mScrollState.mCodeSnippets.mOffset, 6);

    UniPlan::ToggleWatchSidePane(State, UniPlan::EWatchSidePane::CodeSnippets);

    EXPECT_EQ(State.mSidePane, UniPlan::EWatchSidePane::CodeSnippets);
    EXPECT_EQ(State.mScrollState.mCodeSnippets.mOffset, 0);
    EXPECT_EQ(State.mScrollState.mCodeSnippets.mMaxOffset, 0);
    EXPECT_EQ(State.mScrollState.mPhaseDetails.mOffset, 0);

    State.mScrollState.mCodeSnippets.mOffset = 4;
    UniPlan::ToggleWatchSidePane(State, UniPlan::EWatchSidePane::CodeSnippets);

    EXPECT_EQ(State.mSidePane, UniPlan::EWatchSidePane::None);
    EXPECT_EQ(State.mScrollState.mCodeSnippets.mOffset, 4);
}

TEST(WatchInteraction, SidePaneScrollTargetsOnlyVisiblePane)
{
    UniPlan::FWatchInteractionState State;

    EXPECT_FALSE(UniPlan::ScrollWatchSidePane(State, 1));

    UniPlan::ToggleWatchSidePane(State, UniPlan::EWatchSidePane::FileManifest);
    EXPECT_TRUE(UniPlan::ScrollWatchSidePane(State, 1));
    EXPECT_EQ(State.mScrollState.mFileManifest.mOffset, 1);
    EXPECT_EQ(State.mScrollState.mCodeSnippets.mOffset, 0);
    EXPECT_EQ(State.mScrollState.mPhaseDetails.mOffset, 0);

    EXPECT_TRUE(UniPlan::ScrollWatchSidePane(State, -1));
    EXPECT_EQ(State.mScrollState.mFileManifest.mOffset, 0);
    EXPECT_TRUE(UniPlan::ScrollWatchSidePane(State, -1));
    EXPECT_EQ(State.mScrollState.mFileManifest.mOffset, 0);
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
    const UniPlan::PhaseListPanel PhasePanel;
    const UniPlan::ExecutionTaxonomyPanel TaxonomyPanel;

    const UniPlan::FWatchPlanSummary PhasePlan = MakePhaseListPlan(30);
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

TEST_F(FBundleTestFixture, ExecutionTaxonomyPanelNonFocusSkipsRowModels)
{
    const UniPlan::FWatchPlanSummary Plan = MakeRichTaxonomyPlan(8, 4, 3);
    const UniPlan::ExecutionTaxonomyPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    UniPlan::FWatchExecutionTaxonomyLayoutCache Cache;

    const std::string Rendered = StripAnsiCodes(RenderElementToString(
        Panel.Render(Plan, 0, -1, -1, false, ScrollState, Cache, 44), 120,
        5));

    EXPECT_EQ(Cache.mBuildCount, 0);
    EXPECT_TRUE(Cache.mLanes.empty());
    EXPECT_TRUE(Cache.mJobs.empty());
    EXPECT_TRUE(Cache.mTasks.empty());
    EXPECT_NE(Rendered.find("8 lanes"), std::string::npos);
    EXPECT_NE(Rendered.find("32 jobs"), std::string::npos);
    EXPECT_EQ(Rendered.find("[L]ANES"), std::string::npos);
}

TEST_F(FBundleTestFixture, ExecutionTaxonomyPanelFocusReusesRowModels)
{
    const UniPlan::FWatchPlanSummary Plan = MakeRichTaxonomyPlan(4, 3, 2);
    const UniPlan::ExecutionTaxonomyPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    UniPlan::FWatchExecutionTaxonomyLayoutCache Cache;

    RenderElementToString(
        Panel.Render(Plan, 0, -1, -1, true, ScrollState, Cache, 45), 140,
        28);
    ASSERT_EQ(Cache.mBuildCount, 1);
    EXPECT_EQ(Cache.mLanes.size(), 4);
    EXPECT_EQ(Cache.mJobs.size(), 12);
    EXPECT_EQ(Cache.mTasks.size(), 24);

    RenderElementToString(
        Panel.Render(Plan, 0, -1, -1, true, ScrollState, Cache, 45), 140,
        28);
    EXPECT_EQ(Cache.mBuildCount, 1);

    RenderElementToString(
        Panel.Render(Plan, 0, -1, 2, true, ScrollState, Cache, 45), 140,
        28);
    EXPECT_EQ(Cache.mBuildCount, 2);
    EXPECT_EQ(Cache.mJobs.size(), 3);
    EXPECT_EQ(Cache.mTasks.size(), 6);

    RenderElementToString(
        Panel.Render(Plan, 0, -1, 2, true, ScrollState, Cache, 46), 140,
        28);
    EXPECT_EQ(Cache.mBuildCount, 3);
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
    EXPECT_NE(First.find("\xe2\x86\x93 "), std::string::npos);
    EXPECT_EQ(First.find("  \xe2\x86\x93"), std::string::npos);

    const std::string Second = StripAnsiCodes(RenderElementToString(
        Panel.Render(Taxonomy, ScrollState), 80, 8));
    EXPECT_EQ(Second, First);
}

TEST_F(FBundleTestFixture, FileManifestPanelCachesAndVirtualizesViewport)
{
    const UniPlan::FileManifestPanel Panel;
    const UniPlan::FPhaseTaxonomy Taxonomy = MakeFileManifestTaxonomy(40);
    UniPlan::FWatchScrollRegionState ScrollState;
    UniPlan::FWatchFileManifestLayoutCache Cache;

    const std::string First = StripAnsiCodes(RenderElementToString(
        Panel.Render("FileCache", Taxonomy, ScrollState, Cache, 50), 80, 8));
    ASSERT_EQ(Cache.mBuildCount, 1);
    ASSERT_EQ(Cache.mVisualBuildCount, 1);
    EXPECT_NE(First.find("File-00.cpp"), std::string::npos);
    EXPECT_EQ(First.find("File-39.cpp"), std::string::npos);
    EXPECT_GT(ScrollState.mMaxOffset, 0);

    const std::string Second = StripAnsiCodes(RenderElementToString(
        Panel.Render("FileCache", Taxonomy, ScrollState, Cache, 50), 80, 8));
    EXPECT_EQ(Cache.mBuildCount, 1);
    EXPECT_EQ(Cache.mVisualBuildCount, 1);
    EXPECT_EQ(Second, First);

    ScrollState.mOffset = 1;
    const std::string Scrolled = StripAnsiCodes(RenderElementToString(
        Panel.Render("FileCache", Taxonomy, ScrollState, Cache, 50), 80, 8));
    EXPECT_EQ(Cache.mBuildCount, 1);
    EXPECT_EQ(Cache.mVisualBuildCount, 1);
    EXPECT_NE(Scrolled.find("above"), std::string::npos);

    RenderElementToString(
        Panel.Render("FileCache", Taxonomy, ScrollState, Cache, 51), 80, 8);
    EXPECT_EQ(Cache.mBuildCount, 2);
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

TEST_F(FBundleTestFixture, WatchSnapshotProjectsPhaseDetailsFields)
{
    CreateMinimalFixture("PhaseDetails", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);

    StartCapture();
    const int Code = UniPlan::RunPhaseSetCommand(
        {"--topic", "PhaseDetails", "--phase", "0",
         "--scope", "Snapshot scope",
         "--output", "Snapshot output",
         "--done", "Snapshot done",
         "--remaining", "Snapshot remaining",
         "--blockers", "Snapshot blocker",
         "--investigation", "Snapshot investigation",
         "--code-entity-contract", "Snapshot contract",
         "--best-practices", "Snapshot practices",
         "--multi-platforming", "Snapshot platform notes",
         "--readiness-gate", "Snapshot readiness",
         "--handoff", "Snapshot handoff",
         "--no-file-manifest", "true",
         "--no-file-manifest-reason", "Snapshot no files",
         "--repo-root", mRepoRoot.string()},
        mRepoRoot.string());
    StopCapture();
    ASSERT_EQ(Code, 0) << mCapturedStderr;

    UniPlan::FWatchSnapshotCache Cache;
    const UniPlan::FDocWatchSnapshot Snapshot =
        UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                          Cache, true);
    const UniPlan::FWatchPlanSummary *Plan =
        FindPlan(Snapshot, "PhaseDetails");
    ASSERT_NE(Plan, nullptr);
    ASSERT_FALSE(Plan->mPhases.empty());
    const UniPlan::PhaseItem &Phase = Plan->mPhases[0];

    EXPECT_EQ(Phase.mScope, "Snapshot scope");
    EXPECT_EQ(Phase.mOutput, "Snapshot output");
    EXPECT_EQ(Phase.mDone, "Snapshot done");
    EXPECT_EQ(Phase.mRemaining, "Snapshot remaining");
    EXPECT_EQ(Phase.mBlockers, "Snapshot blocker");
    EXPECT_EQ(Phase.mInvestigation, "Snapshot investigation");
    EXPECT_EQ(Phase.mCodeEntityContract, "Snapshot contract");
    EXPECT_EQ(Phase.mBestPractices, "Snapshot practices");
    EXPECT_EQ(Phase.mMultiPlatforming, "Snapshot platform notes");
    EXPECT_EQ(Phase.mReadinessGate, "Snapshot readiness");
    EXPECT_EQ(Phase.mHandoff, "Snapshot handoff");
    EXPECT_TRUE(Phase.mbNoFileManifest);
    EXPECT_EQ(Phase.mFileManifestSkipReason, "Snapshot no files");
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
        "alpha bravo charlie delta echo foxtrot golf hotel india juliet "
        "kilo lima mike november oscar papa quebec romeo sierra tango\n"
        "final sentinel WRAPTAIL";
    const UniPlan::FWatchPlanSummary Plan = MakeCodeSnippetPlan(Snippet);

    const UniPlan::CodeSnippetPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const std::string First = StripAnsiCodes(
        RenderElementToString(Panel.Render(Plan, 0, ScrollState), 36, 7));
    ASSERT_GT(ScrollState.mMaxOffset, 0);
    EXPECT_EQ(ScrollState.mOffset, 0);
    EXPECT_NE(First.find("below"), std::string::npos);
    EXPECT_NE(First.find("\xe2\x86\x93 "), std::string::npos);
    EXPECT_EQ(First.find("  \xe2\x86\x93"), std::string::npos);

    ++ScrollState.mOffset;
    const std::string Second = StripAnsiCodes(
        RenderElementToString(Panel.Render(Plan, 0, ScrollState), 36, 7));
    EXPECT_EQ(ScrollState.mOffset, 1);
    EXPECT_NE(Second.find("above"), std::string::npos);
    EXPECT_NE(Second.find("\xe2\x86\x91 "), std::string::npos);
    EXPECT_EQ(Second.find("  \xe2\x86\x91"), std::string::npos);

    ScrollState.mOffset = 999;
    const std::string Last = StripAnsiCodes(
        RenderElementToString(Panel.Render(Plan, 0, ScrollState), 36, 7));
    EXPECT_EQ(ScrollState.mOffset, ScrollState.mMaxOffset);
    EXPECT_NE(Last.find("WRAPTAIL"), std::string::npos);
    EXPECT_EQ(Last.find("..."), std::string::npos);
}

TEST_F(FBundleTestFixture, CodeSnippetPanelCachesWrappedRows)
{
    const std::string Snippet =
        "first\n"
        "alpha bravo charlie delta echo foxtrot golf hotel india juliet "
        "kilo lima mike november oscar papa quebec romeo sierra tango\n"
        "final sentinel";
    const UniPlan::FWatchPlanSummary Plan = MakeCodeSnippetPlan(Snippet);

    const UniPlan::CodeSnippetPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    UniPlan::FWatchCodeSnippetLayoutCache Cache;

    RenderElementToString(
        Panel.Render(Plan, 0, ScrollState, Cache, 60), 36, 7);
    ASSERT_EQ(Cache.mLogicalBuildCount, 1);
    ASSERT_EQ(Cache.mVisualBuildCount, 1);

    RenderElementToString(
        Panel.Render(Plan, 0, ScrollState, Cache, 60), 36, 7);
    EXPECT_EQ(Cache.mLogicalBuildCount, 1);
    EXPECT_EQ(Cache.mVisualBuildCount, 1);

    ++ScrollState.mOffset;
    RenderElementToString(
        Panel.Render(Plan, 0, ScrollState, Cache, 60), 36, 7);
    EXPECT_EQ(Cache.mLogicalBuildCount, 1);
    EXPECT_EQ(Cache.mVisualBuildCount, 1);

    RenderElementToString(
        Panel.Render(Plan, 0, ScrollState, Cache, 60), 52, 7);
    EXPECT_EQ(Cache.mLogicalBuildCount, 1);
    EXPECT_EQ(Cache.mVisualBuildCount, 2);

    RenderElementToString(
        Panel.Render(Plan, 0, ScrollState, Cache, 61), 52, 7);
    EXPECT_EQ(Cache.mLogicalBuildCount, 2);
}

TEST_F(FBundleTestFixture, CodeSnippetPanelDoesNotSplitLongSingleToken)
{
    const std::string Snippet =
        "first\n"
        "SUPERSIZEDTOKENWITHOUTANYSPACESORWORDWRAPPOINTS1234567890\n"
        "final";
    const UniPlan::FWatchPlanSummary Plan = MakeCodeSnippetPlan(Snippet);

    const UniPlan::CodeSnippetPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const std::string Rendered = StripAnsiCodes(
        RenderElementToString(Panel.Render(Plan, 0, ScrollState), 34, 8));

    EXPECT_EQ(ScrollState.mMaxOffset, 0);
    EXPECT_EQ(Rendered.find("below"), std::string::npos);
    EXPECT_EQ(Rendered.find("..."), std::string::npos);
    EXPECT_NE(Rendered.find("SUPERSIZEDTOKEN"), std::string::npos);
}

TEST_F(FBundleTestFixture, PhaseDetailsPanelRendersTypedContent)
{
    const UniPlan::FWatchPlanSummary Plan = MakePhaseDetailsPlan();
    ASSERT_FALSE(Plan.mPhaseTaxonomies.empty());

    const UniPlan::PhaseDetailsPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const ftxui::Screen Screen = RenderElementToScreen(
        Panel.Render(Plan, 0, Plan.mPhaseTaxonomies[0], ScrollState), 150,
        140);
    const std::string Rendered = StripAnsiCodes(Screen.ToString());

    EXPECT_NE(Rendered.find("PHASE DETAILS: PhaseSide P0"),
              std::string::npos);
    EXPECT_NE(Rendered.find("1 overview"), std::string::npos);
    EXPECT_NE(Rendered.find("status"), std::string::npos);
    EXPECT_NE(Rendered.find("in_progress"), std::string::npos);
    EXPECT_EQ(Rendered.find("Status: in_progress"), std::string::npos);
    EXPECT_NE(Rendered.find("Build a durable side pane"),
              std::string::npos);
    EXPECT_EQ(Rendered.find("Scope: Build a durable side pane"),
              std::string::npos);
    EXPECT_NE(Rendered.find("Side panes need"),
              std::string::npos);
    EXPECT_EQ(Rendered.find("Investigation: Side panes need"),
              std::string::npos);
    EXPECT_NE(Rendered.find("code_entity_contract"), std::string::npos);
    EXPECT_NE(Rendered.find("best_practices"), std::string::npos);
    EXPECT_NE(Rendered.find("design_chars"), std::string::npos);
    EXPECT_NE(Rendered.find("multi_platforming"), std::string::npos);
    EXPECT_NE(Rendered.find("readiness_gate"), std::string::npos);
    EXPECT_NE(Rendered.find("code_snippets"), std::string::npos);
    EXPECT_EQ(Rendered.find("Code entity contract"), std::string::npos);
    EXPECT_EQ(Rendered.find("Best practices"), std::string::npos);
    EXPECT_EQ(Rendered.find("Multi-platforming"), std::string::npos);
    EXPECT_EQ(Rendered.find("Readiness gate"), std::string::npos);
    EXPECT_NE(Rendered.find("topic=Audio"), std::string::npos);
    EXPECT_NE(Rendered.find("command=.\\build.ps1 -Tests"),
              std::string::npos);
    EXPECT_NE(Rendered.find("L0"), std::string::npos);
    EXPECT_NE(Rendered.find("J0"), std::string::npos);
    EXPECT_NE(Rendered.find("T0"), std::string::npos);
    EXPECT_NE(Rendered.find("actor=automated"), std::string::npos);
    EXPECT_NE(Rendered.find("path=Source/PhaseSide.cpp"),
              std::string::npos);
    EXPECT_NE(Rendered.find("3 lines; press F12"),
              std::string::npos);
    EXPECT_EQ(Rendered.find("Code snippets: 3 lines; press F12"),
              std::string::npos);
    EXPECT_EQ(Rendered.find("RawCodeSentinel"), std::string::npos);

    ftxui::Color Foreground;
    ASSERT_TRUE(
        TryFindTextForeground(Screen, "code_entity_contract", Foreground));
    EXPECT_EQ(Foreground, ftxui::Color::CyanLight);
    ASSERT_TRUE(TryFindTextForeground(Screen, "best_practices", Foreground));
    EXPECT_EQ(Foreground, ftxui::Color::CyanLight);
    ASSERT_TRUE(TryFindTextForeground(Screen, "design_chars", Foreground));
    EXPECT_EQ(Foreground, ftxui::Color::Yellow);
    ASSERT_TRUE(TryFindTextForeground(Screen, "code_snippets", Foreground));
    EXPECT_EQ(Foreground, ftxui::Color::Yellow);
    ASSERT_TRUE(TryFindTextForeground(Screen, "overview", Foreground));
    EXPECT_EQ(Foreground, ftxui::Color::Yellow);

    const std::string LeftBorder = "\xe2\x94\x82";
    const std::string Rule = "\xe2\x94\x80";
    EXPECT_NE(Rendered.find(LeftBorder + Rule + Rule + Rule),
              std::string::npos);
    EXPECT_EQ(Rendered.find(LeftBorder + " " + Rule), std::string::npos);
}

TEST_F(FBundleTestFixture, PhaseDetailsPanelHighlightsTaggedFenceBodies)
{
    UniPlan::FWatchPlanSummary Plan = MakePhaseDetailsPlan();
    Plan.mPhases[0].mInvestigation =
        "intro prose\n"
        "```cpp\n"
        "int phase_detail_cpp_body = 1;\n"
        "```\n"
        "```python\n"
        "phase_detail_python_body()\n"
        "```\n"
        "```bash\n"
        "phase_detail_bash_body\n"
        "```\n"
        "```json\n"
        "{\"phase_detail_json_body\": true}\n"
        "```\n"
        "inline `phase_detail_inline_ticks`\n"
        "```\n"
        "phase_detail_untagged_body\n"
        "```";

    const UniPlan::PhaseDetailsPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const ftxui::Screen Screen = RenderElementToScreen(
        Panel.Render(Plan, 0, Plan.mPhaseTaxonomies[0], ScrollState), 150,
        160);

    const ftxui::Color CodeBackground = PhaseDetailsCodeBackgroundForTest();
    ftxui::Color Background;
    ASSERT_TRUE(TryFindTextBackground(Screen, "int phase_detail_cpp_body",
                                      Background));
    EXPECT_EQ(Background, CodeBackground);
    EXPECT_NE(Background, ftxui::Color::BlueLight);
    ASSERT_TRUE(TryFindTextBackground(Screen, "phase_detail_python_body",
                                      Background));
    EXPECT_EQ(Background, CodeBackground);
    ASSERT_TRUE(TryFindTextBackground(Screen, "phase_detail_bash_body",
                                      Background));
    EXPECT_EQ(Background, CodeBackground);
    ASSERT_TRUE(TryFindTextBackground(Screen, "phase_detail_json_body",
                                      Background));
    EXPECT_EQ(Background, CodeBackground);
    ASSERT_TRUE(TryFindTextBackgroundAfter(Screen, "phase_detail_bash_body",
                                           8, Background));
    EXPECT_EQ(Background, CodeBackground);

    ASSERT_TRUE(TryFindTextBackground(Screen, "```cpp", Background));
    EXPECT_EQ(Background, ftxui::Color::Default);
    ASSERT_TRUE(TryFindTextBackground(Screen, "inline `phase_detail_inline",
                                      Background));
    EXPECT_EQ(Background, ftxui::Color::Default);
    ASSERT_TRUE(TryFindTextBackground(Screen, "phase_detail_untagged_body",
                                      Background));
    EXPECT_EQ(Background, ftxui::Color::Default);
}

TEST_F(FBundleTestFixture,
       PhaseDetailsPanelHighlightsWrappedUnclosedTaggedFenceBodies)
{
    UniPlan::FWatchPlanSummary Plan = MakePhaseDetailsPlan();
    Plan.mPhases[0].mScope =
        "```python\n"
        "phase_detail_wrap_start alpha bravo charlie delta echo foxtrot "
        "golf hotel india juliet kilo phase_detail_wrap_tail";

    const UniPlan::PhaseDetailsPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const ftxui::Screen Screen = RenderElementToScreen(
        Panel.Render(Plan, 0, Plan.mPhaseTaxonomies[0], ScrollState), 56,
        140);

    const ftxui::Color CodeBackground = PhaseDetailsCodeBackgroundForTest();
    ftxui::Color Background;
    ASSERT_TRUE(TryFindTextBackground(Screen, "```python", Background));
    EXPECT_EQ(Background, ftxui::Color::Default);
    ASSERT_TRUE(TryFindTextBackground(Screen, "phase_detail_wrap_start",
                                      Background));
    EXPECT_EQ(Background, CodeBackground);
    ASSERT_TRUE(TryFindTextBackground(Screen, "phase_detail_wrap_tail",
                                      Background));
    EXPECT_EQ(Background, CodeBackground);
}

TEST_F(FBundleTestFixture, PhaseDetailsPanelRendersEmptyState)
{
    UniPlan::FWatchPlanSummary Plan;
    Plan.mTopicKey = "EmptyPhase";
    const UniPlan::FPhaseTaxonomy Taxonomy;
    const UniPlan::PhaseDetailsPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    ScrollState.mOffset = 9;
    ScrollState.mMaxOffset = 12;

    const std::string Rendered = StripAnsiCodes(RenderElementToString(
        Panel.Render(Plan, 0, Taxonomy, ScrollState), 70, 8));

    EXPECT_NE(Rendered.find("No phase selected"), std::string::npos);
    EXPECT_EQ(ScrollState.mOffset, 0);
    EXPECT_EQ(ScrollState.mMaxOffset, 0);
}

TEST_F(FBundleTestFixture, PhaseDetailsPanelWrapsAndScrollsOneVisualLine)
{
    UniPlan::FWatchPlanSummary Plan = MakePhaseDetailsPlan();
    Plan.mPhases[0].mScope =
        "alpha bravo charlie delta echo foxtrot golf hotel india juliet "
        "kilo lima mike november oscar papa quebec romeo sierra tango";
    Plan.mPhases[0].mRemaining =
        "SUPERSIZEDTOKENWITHOUTANYSPACESORWORDWRAPPOINTS1234567890";

    const UniPlan::PhaseDetailsPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const std::string First = StripAnsiCodes(RenderElementToString(
        Panel.Render(Plan, 0, Plan.mPhaseTaxonomies[0], ScrollState), 48,
        9));

    ASSERT_GT(ScrollState.mMaxOffset, 0);
    EXPECT_EQ(ScrollState.mOffset, 0);
    EXPECT_NE(First.find("below"), std::string::npos);
    EXPECT_NE(First.find("\xe2\x86\x93 "), std::string::npos);
    EXPECT_EQ(First.find("  \xe2\x86\x93"), std::string::npos);
    EXPECT_EQ(First.find("..."), std::string::npos);

    ++ScrollState.mOffset;
    const std::string Second = StripAnsiCodes(RenderElementToString(
        Panel.Render(Plan, 0, Plan.mPhaseTaxonomies[0], ScrollState), 48,
        9));
    EXPECT_EQ(ScrollState.mOffset, 1);
    EXPECT_NE(Second.find("above"), std::string::npos);
    EXPECT_NE(Second.find("\xe2\x86\x91 "), std::string::npos);
    EXPECT_EQ(Second.find("  \xe2\x86\x91"), std::string::npos);

    UniPlan::FWatchScrollRegionState FullScrollState;
    const std::string Full = StripAnsiCodes(RenderElementToString(
        Panel.Render(Plan, 0, Plan.mPhaseTaxonomies[0], FullScrollState), 48,
        40));
    EXPECT_NE(Full.find("SUPERSIZEDTOKEN"), std::string::npos);
    EXPECT_EQ(Full.find("..."), std::string::npos);
}

TEST_F(FBundleTestFixture, PhaseDetailsPanelReusesLayoutAcrossScroll)
{
    UniPlan::FWatchPlanSummary Plan = MakePhaseDetailsPlan();
    Plan.mPhases[0].mScope =
        "alpha bravo charlie delta echo foxtrot golf hotel india juliet "
        "kilo lima mike november oscar papa quebec romeo sierra tango";

    const UniPlan::PhaseDetailsPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    UniPlan::FWatchProseLayoutCache Cache;

    RenderElementToString(Panel.Render(Plan, 0, Plan.mPhaseTaxonomies[0],
                                       ScrollState, Cache, 7),
                          48, 9);
    ASSERT_EQ(Cache.mLogicalBuildCount, 1);
    ASSERT_EQ(Cache.mVisualBuildCount, 1);
    ASSERT_GT(ScrollState.mMaxOffset, 0);

    ++ScrollState.mOffset;
    RenderElementToString(Panel.Render(Plan, 0, Plan.mPhaseTaxonomies[0],
                                       ScrollState, Cache, 7),
                          48, 9);
    EXPECT_EQ(Cache.mLogicalBuildCount, 1);
    EXPECT_EQ(Cache.mVisualBuildCount, 1);
    EXPECT_EQ(ScrollState.mOffset, 1);

    RenderElementToString(Panel.Render(Plan, 0, Plan.mPhaseTaxonomies[0],
                                       ScrollState, Cache, 8),
                          48, 9);
    EXPECT_EQ(Cache.mLogicalBuildCount, 2);
    EXPECT_EQ(Cache.mVisualBuildCount, 2);
}

TEST_F(FBundleTestFixture, PlanDetailPanelWrapsProseWithLineGutters)
{
    const UniPlan::FWatchPlanSummary Plan = MakePlanDetailWrapPlan();

    const UniPlan::PlanDetailPanel Panel;
    const std::string Rendered =
        StripAnsiCodes(RenderElementToString(Panel.Render(Plan), 76, 20));

    EXPECT_NE(Rendered.find("1 Summary"), std::string::npos);
    EXPECT_NE(Rendered.find("2 alpha beta gamma"), std::string::npos);
    EXPECT_NE(Rendered.find("3 Goals"), std::string::npos);
    EXPECT_NE(Rendered.find("\xe2\x97\x8f Extend"), std::string::npos);
    EXPECT_NE(Rendered.find("1 Risks"), std::string::npos);
    EXPECT_NE(Rendered.find("[high/open] Plan"), std::string::npos);
    EXPECT_NE(Rendered.find("Acceptance Criteria"), std::string::npos);
    EXPECT_EQ(Rendered.find("  Summary"), std::string::npos);
    EXPECT_EQ(Rendered.find("    \xe2\x97\x8f"), std::string::npos);
    EXPECT_EQ(Rendered.find("    [high/open]"), std::string::npos);
    EXPECT_EQ(Rendered.find("..."), std::string::npos);
}

TEST_F(FBundleTestFixture, WatchStatusBarListsSidePaneKeys)
{
    const UniPlan::WatchStatusBar Panel;
    UniPlan::FWatchInventoryCounters Counters;
    UniPlan::FDocWatchSnapshot::FPerformance Performance;

    const std::string Rendered =
        StripAnsiCodes(RenderElementToString(Panel.Render(
            UniPlan::kCliVersion, "now", 23, 150, Counters, Performance)));

    EXPECT_EQ(Rendered.find("s=schema"), std::string::npos);
    EXPECT_EQ(Rendered.find("i=impl"), std::string::npos);
    EXPECT_EQ(Rendered.find("f/F=files"), std::string::npos);
    EXPECT_NE(Rendered.find("F5=phase"), std::string::npos);
    EXPECT_NE(Rendered.find("F6=files"), std::string::npos);
    EXPECT_NE(Rendered.find("F12=code"), std::string::npos);
    EXPECT_NE(Rendered.find("[/]=side scroll"), std::string::npos);
    EXPECT_NE(Rendered.find("Disc:"), std::string::npos);
}

TEST_F(FBundleTestFixture, WatchFileIndexMatchesSeparateIndexes)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    fs::create_directories(mRepoRoot / "Docs");
    WriteTextFile(mRepoRoot / "Docs" / "README.md", "# Alpha\n");

    UniPlan::FBundleFileIndexResult BundleIndex;
    UniPlan::FMarkdownFileIndexResult MarkdownIndex;
    UniPlan::FWatchFileIndexResult WatchIndex;
    std::string Error;

    ASSERT_TRUE(
        UniPlan::TryBuildBundleFileIndex(mRepoRoot, BundleIndex, Error))
        << Error;
    ASSERT_TRUE(
        UniPlan::TryBuildMarkdownFileIndex(mRepoRoot, MarkdownIndex, Error))
        << Error;
    ASSERT_TRUE(UniPlan::TryBuildWatchFileIndex(mRepoRoot, WatchIndex, Error))
        << Error;

    EXPECT_EQ(WatchIndex.mBundleIndex.mSignature, BundleIndex.mSignature);
    EXPECT_EQ(WatchIndex.mMarkdownIndex.mSignature, MarkdownIndex.mSignature);
    EXPECT_EQ(WatchIndex.mBundleIndex.mBundles.size(),
              BundleIndex.mBundles.size());
    EXPECT_EQ(WatchIndex.mMarkdownIndex.mFiles.size(),
              MarkdownIndex.mFiles.size());
}

TEST_F(FBundleTestFixture, WatchFileIndexFastRefreshFindsCanonicalPlanAdds)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);

    UniPlan::FWatchFileIndexResult InitialIndex;
    UniPlan::FWatchFileIndexResult FastIndex;
    std::string Error;
    ASSERT_TRUE(
        UniPlan::TryBuildWatchFileIndex(mRepoRoot, InitialIndex, Error))
        << Error;
    ASSERT_TRUE(BundleIndexContainsTopic(InitialIndex.mBundleIndex, "Alpha"));

    CreateMinimalFixture("Beta", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);
    ASSERT_TRUE(UniPlan::TryRefreshWatchFileIndexFast(
        mRepoRoot, InitialIndex, FastIndex, Error))
        << Error;

    EXPECT_TRUE(BundleIndexContainsTopic(FastIndex.mBundleIndex, "Alpha"));
    EXPECT_TRUE(BundleIndexContainsTopic(FastIndex.mBundleIndex, "Beta"));
    EXPECT_NE(FastIndex.mBundleIndex.mSignature,
              InitialIndex.mBundleIndex.mSignature);
}

TEST_F(FBundleTestFixture, PhaseListPanelRendersDefaultAndMetricsViews)
{
    CreateMinimalFixture("Alpha", UniPlan::ETopicStatus::InProgress, 1,
                         UniPlan::EExecutionStatus::NotStarted, true);

    UniPlan::FWatchSnapshotCache Cache;
    const UniPlan::FDocWatchSnapshot Snapshot =
        UniPlan::BuildWatchSnapshotCached(mRepoRoot.string(), true, "", false,
                                          Cache, true);
    const UniPlan::FWatchPlanSummary *Plan = FindPlan(Snapshot, "Alpha");
    ASSERT_NE(Plan, nullptr);

    const UniPlan::PhaseListPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const std::string DefaultView =
        RenderElementToString(Panel.Render(*Plan, 0, false, ScrollState));
    const std::string MetricsView =
        RenderElementToString(Panel.Render(*Plan, 0, true, ScrollState));
    const std::string PlainDefaultView = StripAnsiCodes(DefaultView);
    const std::string PlainMetricsView = StripAnsiCodes(MetricsView);

    EXPECT_NE(PlainDefaultView.find("[P]HASE LIST"), std::string::npos);
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

TEST_F(FBundleTestFixture, PhaseListPanelSanitizesMultilineCellGlyphs)
{
    UniPlan::FWatchPlanSummary Plan = MakePhaseListPlan(32);
    constexpr int TargetPhaseIndex = 14;
    Plan.mTopicKey = "ControlGlyphs";
    Plan.mPhases[static_cast<size_t>(TargetPhaseIndex)].mScope =
        "Scope first line\nScope second line\rScope third line\tTabbed";
    Plan.mPhases[static_cast<size_t>(TargetPhaseIndex)].mOutput =
        "Output first line\nOutput second line\x1b[31m";

    const UniPlan::PhaseListPanel Panel;
    UniPlan::FWatchScrollRegionState DefaultScroll;
    DefaultScroll.mOffset = 5;
    const ftxui::Screen DefaultScreen = RenderElementToScreen(
        Panel.Render(Plan, TargetPhaseIndex, false, DefaultScroll), 150, 25);
    EXPECT_FALSE(ScreenHasControlGlyph(DefaultScreen));

    UniPlan::FWatchScrollRegionState MetricScroll;
    MetricScroll.mOffset = 5;
    const ftxui::Screen MetricScreen = RenderElementToScreen(
        Panel.Render(Plan, TargetPhaseIndex, true, MetricScroll), 150, 25);
    EXPECT_FALSE(ScreenHasControlGlyph(MetricScreen));
}

TEST_F(FBundleTestFixture, PhaseListPanelKeepsWideNumericLabelsVisible)
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

    const UniPlan::PhaseListPanel Panel;
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

TEST_F(FBundleTestFixture, PhaseListPanelKeepsRichMetricsComparable)
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

    const UniPlan::PhaseListPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    const std::string MetricsView =
        RenderElementToString(Panel.Render(Plan, 0, true, ScrollState));

    EXPECT_NE(MetricsView.find("20000"), std::string::npos);
    EXPECT_NE(MetricsView.find("40000"), std::string::npos);
    EXPECT_NE(MetricsView.find("\xe2\x96\x91"), std::string::npos);
}

TEST_F(FBundleTestFixture, PhaseListPanelReusesRowsAndVirtualizesViewport)
{
    const UniPlan::FWatchPlanSummary Plan = MakePhaseListPlan(80);
    const UniPlan::PhaseListPanel Panel;
    UniPlan::FWatchScrollRegionState ScrollState;
    UniPlan::FWatchPhaseListLayoutCache Cache;

    const std::string First = StripAnsiCodes(RenderElementToString(
        Panel.Render(Plan, 79, false, ScrollState, Cache, 11), 100, 12));
    ASSERT_EQ(Cache.mBuildCount, 1);
    EXPECT_EQ(ScrollState.mOffset, 0);
    EXPECT_NE(First.find("Scope-79"), std::string::npos);
    EXPECT_NE(First.find("Scope-78"), std::string::npos);
    EXPECT_EQ(First.find("Scope-20"), std::string::npos);

    const std::string Second = StripAnsiCodes(RenderElementToString(
        Panel.Render(Plan, 79, false, ScrollState, Cache, 11), 100, 12));
    EXPECT_EQ(Cache.mBuildCount, 1);
    EXPECT_EQ(Second, First);

    RenderElementToString(
        Panel.Render(Plan, 60, false, ScrollState, Cache, 11), 100, 12);
    EXPECT_EQ(Cache.mBuildCount, 1);
    EXPECT_GT(ScrollState.mOffset, 0);

    RenderElementToString(
        Panel.Render(Plan, 60, false, ScrollState, Cache, 12), 100, 12);
    EXPECT_EQ(Cache.mBuildCount, 2);
}

TEST_F(FBundleTestFixture,
       PhaseListPanelDoesNotSaturateFieldWorkAndTestBaselines)
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

    const UniPlan::PhaseListPanel Panel;
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
