#include "UniPlanWatchPanels.h"
#include "UniPlanHelpers.h"
#include "UniPlanTopicTypes.h" // kPhaseHollowChars, ComputePhaseDesignChars
#include "UniPlanTypes.h"

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/dom/requirement.hpp>
#include <ftxui/dom/table.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace UniPlan
{

using namespace ftxui;

// ---------------------------------------------------------------------------
// Weighted-flex decorator
//
// FTXUI's stock `| flex` hard-codes a grow weight of 1, so multiple flex
// siblings in an hbox always split extra space 50/50 / 33/33/33 / etc.
// The hbox layout in box_helper.cpp distributes extra space as
//   added = extra * flex_grow / sum(flex_grow)
// so a true 2:1 split needs flex_grow=2 on one child, 1 on the other.
// Stock decorators don't expose the weight, hence this tiny Node subclass.
//
// min_x is also forced to 0. FTXUI's hbox respects each child's reported
// min_x BEFORE distributing extra_space via flex_grow — so if a long prose
// line makes the left column's min_x 200, the hbox grants that 200 first
// and only weights what's left. On wide terminals that left the right
// column with almost nothing despite flex_grow=1. Zeroing min_x lets
// flex_grow control the full width; children whose rendered content
// exceeds the final column box truncate at the boundary (same behavior
// stock `text()` already had — just at the proper 2/3 cut, not past it).
// ---------------------------------------------------------------------------

namespace
{
class WeightedFlexNode : public Node
{
  public:
    WeightedFlexNode(Element InChild, int InWeight)
        : Node(Elements{std::move(InChild)}), mWeight(InWeight)
    {
    }

    void ComputeRequirement() override
    {
        children_[0]->ComputeRequirement();
        requirement_ = children_[0]->requirement();
        requirement_.flex_grow_x = mWeight;
        requirement_.min_x = 0;
    }

    void SetBox(Box InBox) override
    {
        Node::SetBox(InBox);
        children_[0]->SetBox(InBox);
    }

    void Render(Screen &InScreen) override
    {
        children_[0]->Render(InScreen);
    }

  private:
    int mWeight;
};
} // namespace

static Decorator FlexWithWeight(int InWeight)
{
    return [InWeight](Element InChild) -> Element
    {
        return std::make_shared<WeightedFlexNode>(std::move(InChild), InWeight);
    };
}

// ---------------------------------------------------------------------------
// Color helpers
// ---------------------------------------------------------------------------

static Element ColorStatus(const std::string &InStatus)
{
    if (InStatus == "completed" || InStatus == "closed")
    {
        return text(InStatus) | dim;
    }
    if (InStatus == "in_progress")
    {
        return text(InStatus) | color(Color::Green);
    }
    if (InStatus == "blocked")
    {
        return text(InStatus) | color(Color::Red);
    }
    if (InStatus == "not_started")
    {
        return text(InStatus) | color(Color::Yellow);
    }
    return text(InStatus);
}

// Fixed-width gauge so plans with different phase counts render at the
// same visual length, enabling cross-row completion comparison in the
// ACTIVE PLANS panel. Width matches kDesignBarWidth.
static Element PhaseProgressBar(int InDone, int InTotal)
{
    if (InTotal == 0)
    {
        return text("no phases") | dim;
    }
    static constexpr int kPhaseProgressBarWidth = 10;
    int Filled = (InDone * kPhaseProgressBarWidth + InTotal - 1) / InTotal;
    if (InDone > 0 && Filled == 0)
    {
        Filled = 1;
    }
    if (InDone < InTotal && Filled == kPhaseProgressBarWidth)
    {
        Filled = kPhaseProgressBarWidth - 1;
    }
    Filled = std::min(kPhaseProgressBarWidth, std::max(0, Filled));
    const int Empty = kPhaseProgressBarWidth - Filled;
    Elements Bar;
    for (int Index = 0; Index < Filled; ++Index)
    {
        Bar.push_back(text("\xe2\x96\x88") | color(Color::Green));
    }
    for (int Index = 0; Index < Empty; ++Index)
    {
        Bar.push_back(text("\xe2\x96\x91") | dim);
    }
    Bar.push_back(
        text(" " + std::to_string(InDone) + "/" + std::to_string(InTotal)));
    return hbox(std::move(Bar));
}

// Render the V4 design-char count as the PHASE DETAIL `Design` column.
// Thresholds come from UniPlanTopicTypes.h (`kPhaseHollowChars` = 3000,
// `kPhaseRichMinChars` = 10000 as of v0.83.0, calibrated against V4
// schema semantics — see that header for the derivation).
// Color coding:
//   • < kPhaseHollowChars   → dim red ("hollow", phase needs more authoring)
//   • [hollow, rich)        → yellow  ("thin", executable but sparse)
//   • ≥ kPhaseRichMinChars  → green   ("rich", properly detailed)
// Empty (= 0) renders as "-" to distinguish "no content" from
// "some content but short".
static constexpr int kDesignBarWidth = 10;

static Element DesignCharsBar(size_t InChars)
{
    if (InChars == 0)
    {
        return text("-") | dim;
    }
    // Scale fill against kPhaseRichMinChars so the bar fills at the
    // "rich" threshold; values above still render at full width.
    const int Filled = std::min(
        kDesignBarWidth,
        static_cast<int>((InChars * kDesignBarWidth + kPhaseRichMinChars - 1) /
                         kPhaseRichMinChars));
    const int Empty = kDesignBarWidth - Filled;
    const auto BarColor = (InChars < kPhaseHollowChars) ? color(Color::Red)
                          : (InChars < kPhaseRichMinChars)
                              ? color(Color::Yellow)
                              : color(Color::Green);
    Elements Bar;
    for (int Index = 0; Index < Filled; ++Index)
    {
        Bar.push_back(text("\xe2\x96\x88") | BarColor);
    }
    for (int Index = 0; Index < Empty; ++Index)
    {
        Bar.push_back(text("\xe2\x96\x91") | dim);
    }
    Bar.push_back(text(" " + std::to_string(InChars)));
    return hbox(std::move(Bar));
}

static Element MetricGaugeBar(const size_t InValue, const size_t InHollow,
                              const size_t InRich, const size_t InFillMax,
                              const std::string &InLabel)
{
    static constexpr int kMetricBarWidth = 8;
    const size_t Denominator = std::max<size_t>(1, InFillMax);
    int Filled = static_cast<int>((InValue * kMetricBarWidth) / Denominator);
    if (InValue > 0 && Filled == 0)
    {
        Filled = 1;
    }
    Filled = std::min(kMetricBarWidth, Filled);
    const int Empty = kMetricBarWidth - Filled;
    const auto BarColor = (InValue < InHollow) ? color(Color::Red)
                          : (InValue < InRich) ? color(Color::Yellow)
                                               : color(Color::Green);
    Elements Bar;
    for (int Index = 0; Index < Filled; ++Index)
    {
        Bar.push_back(text("\xe2\x96\x88") | BarColor);
    }
    for (int Index = 0; Index < Empty; ++Index)
    {
        Bar.push_back(text("\xe2\x96\x91") | dim);
    }
    Bar.push_back(text(" " + InLabel));
    return hbox(std::move(Bar));
}

struct FMetricGaugeScale
{
    size_t mHollow = 0;
    size_t mRich = 0;
    size_t mMinValue = 0;
    size_t mMaxValue = 0;
    bool mbUseRelativeRichRange = false;
};

static FMetricGaugeScale
BuildMetricGaugeScale(const std::vector<PhaseItem> &InPhases,
                      const size_t InHollow, const size_t InRich,
                      size_t (*InReadValue)(const FPhaseRuntimeMetrics &))
{
    FMetricGaugeScale Scale;
    Scale.mHollow = InHollow;
    Scale.mRich = InRich;

    bool bSawValue = false;
    bool bAllValuesRich = true;
    for (const PhaseItem &Phase : InPhases)
    {
        const size_t Value = InReadValue(Phase.mMetrics);
        if (!bSawValue)
        {
            Scale.mMinValue = Value;
            Scale.mMaxValue = Value;
            bSawValue = true;
        }
        else
        {
            Scale.mMinValue = std::min(Scale.mMinValue, Value);
            Scale.mMaxValue = std::max(Scale.mMaxValue, Value);
        }
        if (Value < InRich)
        {
            bAllValuesRich = false;
        }
    }

    Scale.mbUseRelativeRichRange =
        bSawValue && bAllValuesRich && Scale.mMaxValue > Scale.mMinValue;
    return Scale;
}

static size_t ResolveMetricFillMax(const size_t InValue,
                                   const FMetricGaugeScale &InScale)
{
    if (!InScale.mbUseRelativeRichRange)
    {
        return std::max(InScale.mRich, InScale.mMaxValue);
    }

    // When every visible phase already meets the absolute rich threshold,
    // the gauge becomes a relative intensity signal. Keep the weakest rich
    // row half-full so green rows still read as healthy while the stronger
    // rows remain visually distinguishable.
    static constexpr size_t kMetricBarWidth = 8;
    static constexpr size_t kRelativeRichBaseFill = kMetricBarWidth / 2;
    const size_t Range = InScale.mMaxValue - InScale.mMinValue;
    const size_t ExtraWidth = kMetricBarWidth - kRelativeRichBaseFill;
    const size_t RelativeFill =
        kRelativeRichBaseFill +
        ((InValue - InScale.mMinValue) * ExtraWidth) / Range;
    const size_t ClampedFill =
        std::min(kMetricBarWidth, std::max<size_t>(1, RelativeFill));
    return std::max<size_t>(1, (InValue * kMetricBarWidth) / ClampedFill);
}

static Element MetricGaugeBar(const size_t InValue,
                              const FMetricGaugeScale &InScale,
                              const std::string &InLabel)
{
    return MetricGaugeBar(InValue, InScale.mHollow, InScale.mRich,
                          ResolveMetricFillMax(InValue, InScale), InLabel);
}

// Truncate helper removed in v0.97.0 — FTXUI renders panel content
// verbatim; frame overflow is handled by the layout engine. Consumers
// that need a preview trim on their end.

// Insert separatorEmpty() between each cell in a gridbox row for consistent
// column spacing.
static Elements PadGridRow(Elements InCells)
{
    Elements Result;
    for (size_t Index = 0; Index < InCells.size(); ++Index)
    {
        if (Index > 0)
        {
            Result.push_back(separatorEmpty());
        }
        Result.push_back(std::move(InCells[Index]));
    }
    return Result;
}

// ---------------------------------------------------------------------------
// InventoryPanel
// ---------------------------------------------------------------------------

Element InventoryPanel::Render(const FWatchInventoryCounters &InCounters) const
{
    // V4-native counters only. The V3-era rows (Playbooks, Impls, Pairs,
    // Sidecars) that lived here were ghost fields — never populated
    // after the .md→bundle migration — and were always 0 in every repo.
    // Removed rather than retrofit: a V4 bundle is the single source of
    // truth, so aggregate topic/phase counts are the honest signal.
    std::vector<std::vector<Element>> Data;
    Data.push_back(
        {text("Plans") | dim, text(std::to_string(InCounters.mPlanCount))});

    auto InvTable = Table(std::move(Data));

    return window(
        text(" INVENTORY ") | bold,
        vbox({
            InvTable.Render() | flex,
            separator(),
            hbox({text("\xe2\x96\x88 ") | color(Color::Green),
                  text(std::to_string(InCounters.mActivePlanCount) +
                       " active")}),
            hbox({text("\xe2\x96\x88 ") | dim,
                  text(std::to_string(InCounters.mNonActivePlanCount) +
                       " non-active")}),
        }));
}

// ---------------------------------------------------------------------------
// ValidationPanel
// ---------------------------------------------------------------------------

Element
ValidationPanel::Render(const FWatchValidationSummary &InValidation) const
{
    if (InValidation.mState == FWatchValidationSummary::EState::Running ||
        InValidation.mState == FWatchValidationSummary::EState::Pending)
    {
        const std::string Message = InValidation.mStateMessage.empty()
                                        ? "Validation pending"
                                        : InValidation.mStateMessage;
        return window(text(" VALIDATION  pending ") | bold,
                      vbox({
                          text(Message) | color(Color::Yellow) | bold,
                          separator(),
                          text("Plan inventory is available") | dim,
                          text("Results will appear after validation") | dim,
                      })) |
               size(HEIGHT, EQUAL, 9);
    }

    auto SummaryLine = hbox({
        text("\xe2\x9c\x93 " + std::to_string(InValidation.mPassedChecks) +
             " passed") |
            color(Color::Green),
        text("  "),
        text("\xe2\x9c\x97 " + std::to_string(InValidation.mFailedChecks) +
             " failed") |
            color(InValidation.mFailedChecks > 0 ? Color::Red : Color::Green),
        text("  (" + std::to_string(InValidation.mErrorMajorCount) +
             " major, " + std::to_string(InValidation.mErrorMinorCount) +
             " minor)") |
            dim,
    });

    Elements Content;
    Content.push_back(SummaryLine);
    Content.push_back(separator());

    if (InValidation.mState == FWatchValidationSummary::EState::Stale)
    {
        const std::string Message = InValidation.mStateMessage.empty()
                                        ? "Validation stale"
                                        : InValidation.mStateMessage;
        Content.push_back(text(Message) | color(Color::Yellow) | bold);
    }
    else if (InValidation.mFailedChecks == 0)
    {
        Content.push_back(text("All checks passed") | color(Color::Green) |
                          bold);
    }
    else
    {
        std::vector<std::vector<Element>> CheckData;
        CheckData.push_back({text("Check") | bold, text("Result") | bold});
        for (const ValidateCheck &Check : InValidation.mFailedCheckDetails)
        {
            CheckData.push_back(
                {text(Check.mID) | flex, text("FAIL") | color(Color::Red)});
        }
        auto CheckTable = Table(std::move(CheckData));
        CheckTable.SelectAll().SeparatorVertical(EMPTY);
        CheckTable.SelectRow(0).SeparatorHorizontal(LIGHT);
        Content.push_back(CheckTable.Render() | flex);
    }

    return window(text(" VALIDATION  " +
                       std::to_string(InValidation.mTotalChecks) + " checks ") |
                      bold,
                  vbox(std::move(Content))) |
           size(HEIGHT, EQUAL, 9);
}

// ---------------------------------------------------------------------------
// LintPanel
// ---------------------------------------------------------------------------

Element LintPanel::Render(const FWatchLintSummary &InLint) const
{
    if (InLint.mState == FWatchLintSummary::EState::Running ||
        InLint.mState == FWatchLintSummary::EState::Pending)
    {
        const std::string Message = InLint.mStateMessage.empty()
                                        ? "Lint pending"
                                        : InLint.mStateMessage;
        return window(text(" LINT ") | bold,
                      text(Message) | color(Color::Yellow)) |
               size(HEIGHT, EQUAL, 5);
    }

    std::vector<std::vector<Element>> Data;
    Data.push_back(
        {text("Total warnings") | dim,
         text(std::to_string(InLint.mWarningCount)) |
             (InLint.mWarningCount > 0 ? color(Color::Yellow) : dim)});
    Data.push_back({text("Name pattern") | dim,
                    text(std::to_string(InLint.mNamePatternWarnings))});
    Data.push_back({text("Missing H1") | dim,
                    text(std::to_string(InLint.mMissingH1Warnings))});

    auto LintTable = Table(std::move(Data));

    Elements Content;
    Content.push_back(LintTable.Render() | flex);
    Content.push_back(separator());

    if (InLint.mWarningCount > 0)
    {
        const float Ratio =
            std::min(1.0f, static_cast<float>(InLint.mWarningCount) / 200.0f);
        const int LintBarWidth = 15;
        const int LintFilled = std::min(
            LintBarWidth,
            static_cast<int>(Ratio * static_cast<float>(LintBarWidth)));
        const int LintEmpty = LintBarWidth - LintFilled;
        Elements LintBar;
        for (int Index = 0; Index < LintFilled; ++Index)
        {
            LintBar.push_back(text("\xe2\x96\x88") | color(Color::Yellow));
        }
        for (int Index = 0; Index < LintEmpty; ++Index)
        {
            LintBar.push_back(text("\xe2\x96\x91") | dim);
        }
        LintBar.push_back(text(" " + std::to_string(InLint.mWarningCount)));
        Content.push_back(hbox(std::move(LintBar)));
    }
    else
    {
        Content.push_back(text("Clean") | color(Color::Green) | bold);
    }

    return window(text(" LINT ") | bold, vbox(std::move(Content)));
}

// ---------------------------------------------------------------------------
// ActivePlansPanel
// ---------------------------------------------------------------------------

Element ActivePlansPanel::Render(const std::vector<FWatchPlanSummary> &InPlans,
                                 int InSelectedIndex) const
{
    const int MaxVisible = 30;
    const int Count = static_cast<int>(InPlans.size());

    // Compute scroll window so selected index is always visible
    int ScrollOffset = 0;
    if (Count > MaxVisible)
    {
        ScrollOffset = InSelectedIndex - MaxVisible / 2;
        if (ScrollOffset < 0)
        {
            ScrollOffset = 0;
        }
        if (ScrollOffset > Count - MaxVisible)
        {
            ScrollOffset = Count - MaxVisible;
        }
    }
    const int VisibleEnd = std::min(ScrollOffset + MaxVisible, Count);

    std::vector<std::vector<Element>> ActiveTableData;
    ActiveTableData.push_back({text("Topic") | bold | flex,
                               text("Status") | bold, text("Phases") | bold,
                               text("BLK") | bold});
    int SelectedActiveRow = -1;

    for (int Index = ScrollOffset; Index < VisibleEnd; ++Index)
    {
        const FWatchPlanSummary &Plan = InPlans[static_cast<size_t>(Index)];
        const bool Selected = (Index == InSelectedIndex);
        const std::string Marker;

        // Find active phase key
        std::string ActivePhase;
        for (const PhaseItem &Phase : Plan.mPhases)
        {
            if (Phase.mStatus == EExecutionStatus::InProgress)
            {
                ActivePhase = Phase.mPhaseKey;
                break;
            }
        }

        Elements PhaseCol;
        PhaseCol.push_back(
            PhaseProgressBar(Plan.mPhaseCompleted, Plan.mPhaseCount));
        if (!ActivePhase.empty())
        {
            PhaseCol.push_back(text("  "));
            PhaseCol.push_back(text("\xe2\x96\xb6" + ActivePhase) |
                               color(Color::Cyan));
        }

        ActiveTableData.push_back({
            text(Marker + Plan.mTopicKey) | flex | (Selected ? bold : nothing),
            ColorStatus(Plan.mPlanStatus) | (Selected ? bold : nothing),
            hbox(std::move(PhaseCol)) | (Selected ? bold : nothing),
            text(std::to_string(Plan.mBlockers.size())) |
                (Plan.mBlockers.empty() ? dim : color(Color::Red)) |
                (Selected ? bold : nothing),
        });
        if (Selected)
        {
            SelectedActiveRow = static_cast<int>(ActiveTableData.size()) - 1;
        }
    }

    auto ActiveTable = Table(std::move(ActiveTableData));
    ActiveTable.SelectAll().SeparatorVertical(EMPTY);
    ActiveTable.SelectRow(0).SeparatorHorizontal(LIGHT);
    if (SelectedActiveRow >= 0)
    {
        ActiveTable.SelectRow(SelectedActiveRow).Decorate(inverted);
    }

    Elements FinalRows;
    if (ScrollOffset > 0)
    {
        FinalRows.push_back(
            text("  \xe2\x86\x91 " + std::to_string(ScrollOffset) + " above") |
            dim);
    }
    FinalRows.push_back(ActiveTable.Render() | flex);
    if (VisibleEnd < Count)
    {
        FinalRows.push_back(text("  \xe2\x86\x93 " +
                                 std::to_string(Count - VisibleEnd) +
                                 " below") |
                            dim);
    }

    const std::string Title =
        " [A]CTIVE PLANS (" + std::to_string(Count) + ") ";
    return window(text(Title) | bold | color(Color::Green),
                  vbox(std::move(FinalRows))) |
           size(HEIGHT, EQUAL, 35);
}

// ---------------------------------------------------------------------------
// PhaseDetailPanel
// ---------------------------------------------------------------------------

Element PhaseDetailPanel::Render(const FWatchPlanSummary &InPlan,
                                 int InSelectedPhaseIndex,
                                 bool InMetricView) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" PHASE DETAIL ") | bold,
                      text("No plan selected") | dim);
    }

    // gridbox layout — auto-aligns columns without Table's flex_shrink override
    std::vector<Elements> GridRows;
    if (InMetricView)
    {
        GridRows.push_back(PadGridRow({
            text("P") | bold | size(WIDTH, EQUAL, 3),
            text("Status") | bold | size(WIDTH, EQUAL, 12),
            text("Design") | bold | size(WIDTH, EQUAL, 14),
            text("SOLID") | bold | size(WIDTH, EQUAL, 14),
            text("Words") | bold | size(WIDTH, EQUAL, 14),
            text("Fields") | bold | size(WIDTH, EQUAL, 14),
            text("Work") | bold | size(WIDTH, EQUAL, 14),
            text("Tests") | bold | size(WIDTH, EQUAL, 14),
            text("Files") | bold | size(WIDTH, EQUAL, 14),
            text("Evidence") | bold | size(WIDTH, EQUAL, 14),
            text("Scope") | bold | flex,
        }));
    }
    else
    {
        GridRows.push_back(PadGridRow({
            text("P") | bold | size(WIDTH, EQUAL, 3),
            text("Status") | bold | size(WIDTH, EQUAL, 14),
            text("Design") | bold | size(WIDTH, EQUAL, 16),
            text("Taxonomy") | bold | size(WIDTH, EQUAL, 30),
            text("Scope") | bold | flex,
            text("Output") | bold | flex,
        }));
    }

    const int MaxPhases = 20;
    const int Count = static_cast<int>(InPlan.mPhases.size());

    FMetricGaugeScale DesignScale;
    FMetricGaugeScale SolidScale;
    FMetricGaugeScale WordsScale;
    FMetricGaugeScale FieldsScale;
    FMetricGaugeScale WorkScale;
    FMetricGaugeScale TestsScale;
    FMetricGaugeScale FilesScale;
    FMetricGaugeScale EvidenceScale;
    if (InMetricView)
    {
        DesignScale = BuildMetricGaugeScale(
            InPlan.mPhases, kPhaseHollowChars, kPhaseRichMinChars,
            [](const FPhaseRuntimeMetrics &InMetrics)
            { return InMetrics.mDesignChars; });
        SolidScale =
            BuildMetricGaugeScale(InPlan.mPhases, kPhaseMetricSolidHollowWords,
                                  kPhaseMetricSolidRichWords,
                                  [](const FPhaseRuntimeMetrics &InMetrics)
                                  { return InMetrics.mSolidWordCount; });
        WordsScale = BuildMetricGaugeScale(
            InPlan.mPhases, kPhaseMetricRecursiveHollowWords,
            kPhaseMetricRecursiveRichWords,
            [](const FPhaseRuntimeMetrics &InMetrics)
            { return InMetrics.mRecursiveWordCount; });
        FieldsScale = BuildMetricGaugeScale(
            InPlan.mPhases, kPhaseMetricFieldCoverageHollowPercent,
            kPhaseMetricFieldCoverageRichPercent,
            [](const FPhaseRuntimeMetrics &InMetrics)
            { return InMetrics.mFieldCoveragePercent; });
        WorkScale = BuildMetricGaugeScale(
            InPlan.mPhases, kPhaseMetricWorkHollowItems,
            kPhaseMetricWorkRichItems, [](const FPhaseRuntimeMetrics &InMetrics)
            { return InMetrics.mWorkItemCount; });
        TestsScale = BuildMetricGaugeScale(
            InPlan.mPhases, kPhaseMetricTestingHollowRecords,
            kPhaseMetricTestingRichRecords,
            [](const FPhaseRuntimeMetrics &InMetrics)
            { return InMetrics.mTestingRecordCount; });
        FilesScale = BuildMetricGaugeScale(
            InPlan.mPhases, kPhaseMetricFileManifestHollowEntries,
            kPhaseMetricFileManifestRichEntries,
            [](const FPhaseRuntimeMetrics &InMetrics)
            { return InMetrics.mFileManifestCount; });
        EvidenceScale = BuildMetricGaugeScale(
            InPlan.mPhases, kPhaseMetricEvidenceHollowItems,
            kPhaseMetricEvidenceRichItems,
            [](const FPhaseRuntimeMetrics &InMetrics)
            { return InMetrics.mEvidenceItemCount; });
    }

    // Scroll window
    int ScrollOffset = 0;
    if (Count > MaxPhases && InSelectedPhaseIndex >= 0)
    {
        ScrollOffset = InSelectedPhaseIndex - MaxPhases / 2;
        if (ScrollOffset < 0)
        {
            ScrollOffset = 0;
        }
        if (ScrollOffset > Count - MaxPhases)
        {
            ScrollOffset = Count - MaxPhases;
        }
    }
    const int VisibleEnd = std::min(ScrollOffset + MaxPhases, Count);

    for (int Index = ScrollOffset; Index < VisibleEnd; ++Index)
    {
        const PhaseItem &Phase = InPlan.mPhases[static_cast<size_t>(Index)];
        const bool Selected = (Index == InSelectedPhaseIndex);
        const std::string Marker;

        // Typed fields from FPhaseRecord — no fuzzy key matching.
        const std::string &Desc = Phase.mScope;
        const std::string &Output = Phase.mOutput;

        // Taxonomy summary
        std::string TaxSummary = "-";

        for (const FPhaseTaxonomy &Tax : InPlan.mPhaseTaxonomies)
        {
            if (Tax.mPhaseIndex == Index)
            {

                int LD = 0, LA = 0, LT = 0;
                for (const FLaneRecord &L : Tax.mLanes)
                {
                    if (L.mStatus == EExecutionStatus::Completed)
                        LD++;
                    else if (L.mStatus == EExecutionStatus::InProgress)
                        LA++;
                    else
                        LT++;
                }
                int JD = 0, JA = 0, JT = 0;
                for (const FJobRecord &J : Tax.mJobs)
                {
                    if (J.mStatus == EExecutionStatus::Completed)
                        JD++;
                    else if (J.mStatus == EExecutionStatus::InProgress)
                        JA++;
                    else
                        JT++;
                }
                int TD = 0, TA = 0, TT = 0;
                for (const FTaskRecord &T : Tax.mTasks)
                {
                    if (T.mStatus == EExecutionStatus::Completed)
                        TD++;
                    else if (T.mStatus == EExecutionStatus::InProgress)
                        TA++;
                    else
                        TT++;
                }

                TaxSummary = "";
                if (!Tax.mLanes.empty())
                {
                    TaxSummary += std::to_string(Tax.mLanes.size()) +
                                  "L:" + std::to_string(LD) + "d " +
                                  std::to_string(LA) + "a " +
                                  std::to_string(LT) + "t";
                }
                if (!Tax.mJobs.empty())
                {
                    if (!TaxSummary.empty())
                    {
                        TaxSummary += " ";
                    }
                    TaxSummary += std::to_string(Tax.mJobs.size()) +
                                  "J:" + std::to_string(JD) + "d " +
                                  std::to_string(JA) + "a " +
                                  std::to_string(JT) + "t";
                }
                if (!Tax.mTasks.empty())
                {
                    if (!TaxSummary.empty())
                    {
                        TaxSummary += " ";
                    }
                    TaxSummary += std::to_string(Tax.mTasks.size()) +
                                  "T:" + std::to_string(TD) + "d " +
                                  std::to_string(TA) + "a " +
                                  std::to_string(TT) + "t";
                }
                if (TaxSummary.empty())
                {
                    TaxSummary = "-";
                }
                break;
            }
        }

        Elements RowCells;
        if (InMetricView)
        {
            const FPhaseRuntimeMetrics &Metrics = Phase.mMetrics;
            RowCells = PadGridRow({
                text(Marker + Phase.mPhaseKey) | size(WIDTH, EQUAL, 3),
                ColorStatus(ToString(Phase.mStatus)) | size(WIDTH, EQUAL, 12),
                MetricGaugeBar(Metrics.mDesignChars, DesignScale,
                               std::to_string(Metrics.mDesignChars)) |
                    size(WIDTH, EQUAL, 14),
                MetricGaugeBar(Metrics.mSolidWordCount, SolidScale,
                               std::to_string(Metrics.mSolidWordCount)) |
                    size(WIDTH, EQUAL, 14),
                MetricGaugeBar(Metrics.mRecursiveWordCount, WordsScale,
                               std::to_string(Metrics.mRecursiveWordCount)) |
                    size(WIDTH, EQUAL, 14),
                MetricGaugeBar(Metrics.mFieldCoveragePercent, FieldsScale,
                               std::to_string(Metrics.mFieldCoveragePercent) +
                                   "%") |
                    size(WIDTH, EQUAL, 14),
                MetricGaugeBar(Metrics.mWorkItemCount, WorkScale,
                               std::to_string(Metrics.mWorkItemCount)) |
                    size(WIDTH, EQUAL, 14),
                MetricGaugeBar(Metrics.mTestingRecordCount, TestsScale,
                               std::to_string(Metrics.mTestingRecordCount)) |
                    size(WIDTH, EQUAL, 14),
                MetricGaugeBar(Metrics.mFileManifestCount, FilesScale,
                               std::to_string(Metrics.mFileManifestCount)) |
                    size(WIDTH, EQUAL, 14),
                MetricGaugeBar(Metrics.mEvidenceItemCount, EvidenceScale,
                               std::to_string(Metrics.mEvidenceItemCount)) |
                    size(WIDTH, EQUAL, 14),
                text(Desc) | dim | flex,
            });
        }
        else
        {
            RowCells = PadGridRow({
                text(Marker + Phase.mPhaseKey) | size(WIDTH, EQUAL, 3),
                ColorStatus(ToString(Phase.mStatus)) | size(WIDTH, EQUAL, 14),
                DesignCharsBar(Phase.mV4DesignChars) | size(WIDTH, EQUAL, 16),
                text(TaxSummary) | dim | size(WIDTH, EQUAL, 30),
                text(Desc) | dim | flex,
                text(Output) | dim | flex,
            });
        }
        if (Selected)
        {
            for (auto &Cell : RowCells)
            {
                Cell = Cell | inverted | bold;
            }
        }
        GridRows.push_back(std::move(RowCells));
    }

    Elements FinalRows;
    if (ScrollOffset > 0)
    {
        FinalRows.push_back(
            text("  \xe2\x86\x91 " + std::to_string(ScrollOffset) + " above") |
            dim);
    }
    FinalRows.push_back(gridbox(std::move(GridRows)) | flex);
    if (VisibleEnd < Count)
    {
        FinalRows.push_back(text("  \xe2\x86\x93 " +
                                 std::to_string(Count - VisibleEnd) +
                                 " below") |
                            dim);
    }

    // v0.97.0 no-truncation contract applies here too — FTXUI's frame
    // handles overflow at the terminal boundary; the CLI layer emits
    // the verbatim topic key.
    const std::string Title =
        std::string(" [P]HASE DETAIL") + (InMetricView ? " METRICS: " : ": ") +
        InPlan.mTopicKey + " (" + std::to_string(Count) + ") ";
    return window(text(Title) | bold | color(Color::Cyan),
                  vbox(std::move(FinalRows))) |
           size(HEIGHT, EQUAL, 25);
}

// ---------------------------------------------------------------------------
// BlockersPanel
// ---------------------------------------------------------------------------

Element BlockersPanel::Render(const std::vector<BlockerItem> &InBlockers) const
{
    Elements Rows;

    if (InBlockers.empty())
    {
        Rows.push_back(text("(no open blockers)") | dim);
        return window(text(" BLOCKERS ") | bold, vbox(std::move(Rows)));
    }

    std::vector<std::vector<Element>> BlockerTableData;
    BlockerTableData.push_back({text("Topic") | bold, text("Phase") | bold,
                                text("Status") | bold,
                                text("Blocker") | bold | flex});

    const int MaxVisible = 10;
    const int Count = static_cast<int>(InBlockers.size());
    const int VisibleCount = std::min(MaxVisible, Count);

    for (int Index = 0; Index < VisibleCount; ++Index)
    {
        const BlockerItem &Blocker = InBlockers[static_cast<size_t>(Index)];
        BlockerTableData.push_back({
            text(Blocker.mTopicKey),
            text("P" + std::to_string(Blocker.mPhaseIndex)),
            ColorStatus(Blocker.mStatus),
            text(Blocker.mAction) | dim | flex,
        });
    }

    auto BlockerTable = Table(std::move(BlockerTableData));
    BlockerTable.SelectAll().SeparatorVertical(EMPTY);
    BlockerTable.SelectRow(0).SeparatorHorizontal(LIGHT);

    return window(text(" BLOCKERS (" + std::to_string(Count) + ") ") | bold |
                      color(Color::Red),
                  BlockerTable.Render() | flex);
}

// ---------------------------------------------------------------------------
// NonActivePlansPanel
// ---------------------------------------------------------------------------

Element
NonActivePlansPanel::Render(const std::vector<FWatchPlanSummary> &InPlans,
                            int InSelectedIndex) const
{
    const int MaxVisible = 30;
    const int Count = static_cast<int>(InPlans.size());

    // Compute scroll window
    int ScrollOffset = 0;
    if (Count > MaxVisible)
    {
        ScrollOffset = InSelectedIndex - MaxVisible / 2;
        if (ScrollOffset < 0)
        {
            ScrollOffset = 0;
        }
        if (ScrollOffset > Count - MaxVisible)
        {
            ScrollOffset = Count - MaxVisible;
        }
    }
    const int VisibleEnd = std::min(ScrollOffset + MaxVisible, Count);

    std::vector<std::vector<Element>> NonActiveTableData;
    NonActiveTableData.push_back(
        {text("Topic") | bold | flex, text("Status") | bold});
    int SelectedNonActiveRow = -1;

    for (int Index = ScrollOffset; Index < VisibleEnd; ++Index)
    {
        const FWatchPlanSummary &Plan = InPlans[static_cast<size_t>(Index)];
        const bool Selected = (Index == InSelectedIndex);
        const std::string Marker;

        NonActiveTableData.push_back({
            text(Marker + Plan.mTopicKey) | flex | (Selected ? bold : nothing),
            ColorStatus(Plan.mPlanStatus) | (Selected ? bold : nothing),
        });
        if (Selected)
        {
            SelectedNonActiveRow =
                static_cast<int>(NonActiveTableData.size()) - 1;
        }
    }

    auto NonActiveTable = Table(std::move(NonActiveTableData));
    NonActiveTable.SelectAll().SeparatorVertical(EMPTY);
    NonActiveTable.SelectRow(0).SeparatorHorizontal(LIGHT);
    if (SelectedNonActiveRow >= 0)
    {
        NonActiveTable.SelectRow(SelectedNonActiveRow).Decorate(inverted);
    }

    Elements NonActiveFinal;
    if (ScrollOffset > 0)
    {
        NonActiveFinal.push_back(
            text("  \xe2\x86\x91 " + std::to_string(ScrollOffset) + " above") |
            dim);
    }
    NonActiveFinal.push_back(NonActiveTable.Render() | flex);
    if (VisibleEnd < Count)
    {
        NonActiveFinal.push_back(text("  \xe2\x86\x93 " +
                                      std::to_string(Count - VisibleEnd) +
                                      " below") |
                                 dim);
    }

    if (Count == 0)
    {
        NonActiveFinal.push_back(text("(none)") | dim);
    }

    return window(text(" [N]ON-ACTIVE (" + std::to_string(Count) + ") ") |
                      bold | dim,
                  vbox(std::move(NonActiveFinal))) |
           size(HEIGHT, EQUAL, 35);
}

// ---------------------------------------------------------------------------
// DeferredPlansPanel
// ---------------------------------------------------------------------------

Element
DeferredPlansPanel::Render(const std::vector<FWatchPlanSummary> &InPlans) const
{
    const int Count = static_cast<int>(InPlans.size());

    if (Count == 0)
    {
        return window(text(" DEFERRED ") | bold | color(Color::Yellow),
                      text("(none)") | dim);
    }

    std::vector<std::vector<Element>> Data;
    Data.push_back({text("Topic") | bold | flex, text("Status") | bold});

    for (int Index = 0; Index < std::min(10, Count); ++Index)
    {
        const FWatchPlanSummary &Plan = InPlans[static_cast<size_t>(Index)];
        Data.push_back(
            {text(Plan.mTopicKey) | flex, ColorStatus(Plan.mPlanStatus)});
    }

    auto DefTable = Table(std::move(Data));
    DefTable.SelectAll().SeparatorVertical(EMPTY);
    DefTable.SelectRow(0).SeparatorHorizontal(LIGHT);

    return window(text(" DEFERRED (" + std::to_string(Count) + ") ") | bold |
                      color(Color::Yellow),
                  DefTable.Render() | flex);
}

// ---------------------------------------------------------------------------
// ValidationFailPanel
// ---------------------------------------------------------------------------

Element
ValidationFailPanel::Render(const FWatchValidationSummary &InValidation) const
{
    if (InValidation.mState == FWatchValidationSummary::EState::Running ||
        InValidation.mState == FWatchValidationSummary::EState::Pending)
    {
        const std::string Message = InValidation.mStateMessage.empty()
                                        ? "Validation pending"
                                        : InValidation.mStateMessage;
        return window(text(" VALIDATION FAILURES ") | bold,
                      text(Message) | color(Color::Yellow) | bold) |
               size(HEIGHT, EQUAL, 5);
    }

    const std::vector<ValidateCheck> &FailedChecks =
        InValidation.mFailedCheckDetails;
    if (FailedChecks.empty())
    {
        return window(text(" VALIDATION FAILURES ") | bold,
                      text("All checks passed") | color(Color::Green) | bold) |
               size(HEIGHT, EQUAL, 5);
    }

    std::vector<std::vector<Element>> Data;
    Data.push_back({text("Check") | bold, text("Detail") | bold | flex});

    for (const ValidateCheck &Check : FailedChecks)
    {
        std::string Detail = Check.mDetail;
        if (Detail.empty() && !Check.mDiagnostics.empty())
        {
            Detail = Check.mDiagnostics[0];
        }
        Data.push_back(
            {text(Check.mID) | color(Color::Red), text(Detail) | dim | flex});
    }

    auto FailTable = Table(std::move(Data));
    FailTable.SelectAll().SeparatorVertical(EMPTY);
    FailTable.SelectRow(0).SeparatorHorizontal(LIGHT);

    return window(text(" VALIDATION FAILURES (" +
                       std::to_string(FailedChecks.size()) + ") ") |
                      bold | color(Color::Red),
                  FailTable.Render() | flex) |
           size(HEIGHT, EQUAL, 5);
}

// ---------------------------------------------------------------------------
// ExecutionTaxonomyPanel
// ---------------------------------------------------------------------------

Element ExecutionTaxonomyPanel::Render(const FWatchPlanSummary &InPlan,
                                       int InSelectedPhaseIndex,
                                       int InSelectedWaveIndex,
                                       int InSelectedLaneIndex,
                                       bool InFocusMode) const
{
    // Find the taxonomy for the selected phase
    const FPhaseTaxonomy *rpTax = nullptr;
    if (InSelectedPhaseIndex >= 0 &&
        InSelectedPhaseIndex < static_cast<int>(InPlan.mPhases.size()))
    {
        for (const FPhaseTaxonomy &Tax : InPlan.mPhaseTaxonomies)
        {
            if (Tax.mPhaseIndex == InSelectedPhaseIndex)
            {
                rpTax = &Tax;
                break;
            }
        }
    }

    if (rpTax == nullptr || (rpTax->mLanes.empty() && rpTax->mJobs.empty() &&
                             rpTax->mTasks.empty()))
    {
        std::string Hint = InPlan.mTopicKey.empty()
                               ? "No plan selected"
                               : "No execution taxonomy for selected phase";
        return window(text(" EXECUTION TAXONOMY ") | bold | dim,
                      text("  " + Hint) | dim);
    }

    const FPhaseTaxonomy &Tax = *rpTax;

    // --- LANES sub-panel (gridbox for fixed column widths) ---
    // Count done lanes across the full set (summary line shows the total).
    int LanesDone = 0;
    for (const FLaneRecord &Lane : Tax.mLanes)
    {
        if (Lane.mStatus == EExecutionStatus::Completed)
        {
            LanesDone++;
        }
    }

    // Scroll window: keep InSelectedLaneIndex centered when Count exceeds the
    // on-screen cap. Without this, pressing 'l'/'L' moves the selection past
    // the visible cut line and the highlight disappears.
    const int LaneCount = static_cast<int>(Tax.mLanes.size());
    const int MaxVisibleLanes = 8;
    int LaneScrollOffset = 0;
    if (LaneCount > MaxVisibleLanes && InSelectedLaneIndex >= 0)
    {
        LaneScrollOffset = InSelectedLaneIndex - MaxVisibleLanes / 2;
        if (LaneScrollOffset < 0)
        {
            LaneScrollOffset = 0;
        }
        if (LaneScrollOffset > LaneCount - MaxVisibleLanes)
        {
            LaneScrollOffset = LaneCount - MaxVisibleLanes;
        }
    }
    const int LaneVisibleEnd =
        std::min(LaneScrollOffset + MaxVisibleLanes, LaneCount);

    std::vector<Elements> LaneGridRows;
    LaneGridRows.push_back(PadGridRow({
        text("Lane") | bold,
        text("Status") | bold | size(WIDTH, EQUAL, 14),
        text("Scope") | bold | flex,
        text("Exit Criteria") | bold | flex,
    }));

    for (int Index = LaneScrollOffset; Index < LaneVisibleEnd; ++Index)
    {
        const FLaneRecord &Lane = Tax.mLanes[static_cast<size_t>(Index)];
        const bool LaneSel = (Index == InSelectedLaneIndex);
        const std::string Marker;

        Elements RowCells = PadGridRow({
            text(Marker + "L" + std::to_string(Index)),
            ColorStatus(ToString(Lane.mStatus)) | size(WIDTH, EQUAL, 14),
            text(Lane.mScope) | dim | flex,
            text(Lane.mExitCriteria) | dim | flex,
        });
        if (LaneSel)
        {
            for (auto &Cell : RowCells)
            {
                Cell = Cell | inverted | bold;
            }
        }
        LaneGridRows.push_back(std::move(RowCells));
    }

    Elements LaneFinalRows;
    if (LaneScrollOffset > 0)
    {
        LaneFinalRows.push_back(text("  \xe2\x86\x91 " +
                                     std::to_string(LaneScrollOffset) +
                                     " above") |
                                dim);
    }
    LaneFinalRows.push_back(gridbox(std::move(LaneGridRows)) | flex);
    if (LaneVisibleEnd < LaneCount)
    {
        LaneFinalRows.push_back(
            text("  \xe2\x86\x93 " +
                 std::to_string(LaneCount - LaneVisibleEnd) + " below") |
            dim);
    }

    auto LanesPanel =
        window(text(" [L]ANES: " + ("P" + std::to_string(Tax.mPhaseIndex)) +
                    " (" + std::to_string(LaneCount) + ") ") |
                   bold,
               vbox(std::move(LaneFinalRows))) |
        size(HEIGHT, EQUAL, 13);

    // --- JOB BOARD sub-panel (using ftxui::Table) ---
    // Collect unique waves for filtering
    std::vector<int> UniqueWaves;
    for (const FJobRecord &Job : Tax.mJobs)
    {
        bool Found = false;
        for (int W : UniqueWaves)
        {
            if (W == Job.mWave)
            {
                Found = true;
                break;
            }
        }
        if (!Found)
        {
            UniqueWaves.push_back(Job.mWave);
        }
    }

    int WaveFilter = -1;
    if (InSelectedWaveIndex >= 0 &&
        InSelectedWaveIndex < static_cast<int>(UniqueWaves.size()))
    {
        WaveFilter = UniqueWaves[static_cast<size_t>(InSelectedWaveIndex)];
    }
    int LaneFilter = -1;
    if (InSelectedLaneIndex >= 0 &&
        InSelectedLaneIndex < static_cast<int>(Tax.mLanes.size()))
    {
        LaneFilter = InSelectedLaneIndex;
    }

    std::vector<std::vector<Element>> JobTableData;
    JobTableData.push_back({text("Wave") | bold, text("Lane") | bold,
                            text("Status") | bold,
                            text("Scope") | bold | flex});

    int JobsDone = 0, JobsActive = 0, JobsTodo = 0;
    int VisibleJobCount = 0;
    for (size_t JI = 0; JI < Tax.mJobs.size(); ++JI)
    {
        const FJobRecord &Job = Tax.mJobs[JI];
        if (Job.mStatus == EExecutionStatus::Completed)
        {
            JobsDone++;
        }
        else if (Job.mStatus == EExecutionStatus::InProgress)
        {
            JobsActive++;
        }
        else
        {
            JobsTodo++;
        }

        if (WaveFilter >= 0 && Job.mWave != WaveFilter)
        {
            continue;
        }
        if (LaneFilter >= 0 && Job.mLane != LaneFilter)
        {
            continue;
        }

        JobTableData.push_back({
            text("W" + std::to_string(Job.mWave)),
            text("L" + std::to_string(Job.mLane)),
            ColorStatus(ToString(Job.mStatus)),
            text(Job.mScope) | dim | flex,
        });
        VisibleJobCount++;
    }

    Element JobsContent;
    if (VisibleJobCount == 0 && !Tax.mJobs.empty())
    {
        JobsContent = text("  (no jobs match current wave/lane filter)") | dim;
    }
    else
    {
        auto JobTable = Table(std::move(JobTableData));
        JobTable.SelectAll().SeparatorVertical(EMPTY);
        JobTable.SelectRow(0).SeparatorHorizontal(LIGHT);
        JobsContent = JobTable.Render() | flex;
    }

    std::string WaveTitle =
        WaveFilter < 0 ? "all" : ("W" + std::to_string(WaveFilter));
    std::string LaneTitle =
        LaneFilter < 0 ? "all" : ("L" + std::to_string(LaneFilter));
    auto JobsPanel = window(text(" [W]AVE/[L]ANE/JOB BOARD: " +
                                 ("P" + std::to_string(Tax.mPhaseIndex)) +
                                 " (W=" + WaveTitle + " L=" + LaneTitle + ", " +
                                 std::to_string(Tax.mJobs.size()) + " jobs) ") |
                                bold,
                            JobsContent);

    // --- TASKS sub-panel (using ftxui::Table) ---
    std::vector<std::vector<Element>> TaskTableData;
    TaskTableData.push_back(
        {text("Job") | bold, text("Task") | bold, text("Status") | bold,
         text("Description") | bold | flex, text("Evidence") | bold | flex});

    int TasksDone = 0, TasksActive = 0, TasksTodo = 0;
    int VisibleTaskCount = 0;
    for (size_t JI = 0; JI < Tax.mJobs.size(); ++JI)
    {
        const FJobRecord &Job = Tax.mJobs[JI];
        if (LaneFilter >= 0 && Job.mLane != LaneFilter)
        {
            continue;
        }
        for (size_t TI = 0; TI < Job.mTasks.size(); ++TI)
        {
            const FTaskRecord &Task = Job.mTasks[TI];
            if (Task.mStatus == EExecutionStatus::Completed)
            {
                TasksDone++;
            }
            else if (Task.mStatus == EExecutionStatus::InProgress)
            {
                TasksActive++;
            }
            else
            {
                TasksTodo++;
            }

            TaskTableData.push_back({
                text("J" + std::to_string(JI)),
                text("T" + std::to_string(TI)),
                ColorStatus(ToString(Task.mStatus)),
                text(Task.mDescription) | dim | flex,
                text(Task.mEvidence) | dim | flex,
            });
            VisibleTaskCount++;
        }
    }

    Element TasksContent;
    if (Tax.mTasks.empty())
    {
        const std::string Hint =
            Tax.mJobs.empty()
                ? "  (phase has no job board yet)"
                : "  (phase decomposes to job granularity only; no "
                  "per-job tasks authored)";
        TasksContent = text(Hint) | dim;
    }
    else
    {
        auto TaskTable = Table(std::move(TaskTableData));
        TaskTable.SelectAll().SeparatorVertical(EMPTY);
        TaskTable.SelectRow(0).SeparatorHorizontal(LIGHT);
        TasksContent = TaskTable.Render() | flex;
    }

    auto TasksPanel =
        window(text(" TASKS: " + ("P" + std::to_string(Tax.mPhaseIndex)) +
                    " (" + std::to_string(Tax.mTasks.size()) + ") ") |
                   bold,
               TasksContent);

    // --- Summary ---
    auto SummaryLine = hbox({
        text(" ") | bold,
        text(std::to_string(Tax.mLanes.size()) + " lanes (" +
             std::to_string(LanesDone) + " done)") |
            dim,
        text(" | ") | dim,
        text(std::to_string(Tax.mWaveCount) + " waves") | dim,
        text(" | ") | dim,
        text(std::to_string(Tax.mJobs.size()) + " jobs: ") | dim,
        text(std::to_string(JobsDone) + "d") | color(Color::Green),
        text(" " + std::to_string(JobsActive) + "a") | color(Color::Cyan),
        text(" " + std::to_string(JobsTodo) + "t") | color(Color::Yellow),
        text(" | ") | dim,
        text(std::to_string(Tax.mTasks.size()) + " tasks: ") | dim,
        text(std::to_string(TasksDone) + "d") | color(Color::Green),
        text(" " + std::to_string(TasksActive) + "a") | color(Color::Cyan),
        text(" " + std::to_string(TasksTodo) + "t") | color(Color::Yellow),
    });

    // The LANES / JOB BOARD / TASKS sub-panels only render when the watch
    // app is in focus mode. In default mode the summary line alone closes
    // the panel, so the right pane stays compact for overview use.
    Elements TaxonomyRows;
    if (InFocusMode)
    {
        TaxonomyRows.push_back(LanesPanel);
        TaxonomyRows.push_back(JobsPanel);
        TaxonomyRows.push_back(TasksPanel);
    }
    TaxonomyRows.push_back(SummaryLine);
    return vbox(std::move(TaxonomyRows));
}

// ---------------------------------------------------------------------------
// SchemaPanel — merged heading list with color coding
// ---------------------------------------------------------------------------

static Element RenderSchemaBlock(const std::string &InTitle,
                                 const FWatchDocSchemaResult &InResult)
{
    if (InResult.mDocPath.empty())
    {
        return window(text(" " + InTitle + " ") | bold | dim,
                      text("  (no document)") | dim);
    }

    Elements Rows;
    for (const FWatchHeadingCheck &Check : InResult.mHeadings)
    {
        const std::string Indent = (Check.mLevel == 3) ? " " : "";
        Element Row;

        if (Check.mbPresent && Check.mbCanonical && Check.mbRequired)
        {
            Row = text(Indent + Check.mSectionID) | color(Color::Green);
        }
        else if (Check.mbPresent && Check.mbCanonical && !Check.mbRequired)
        {
            Row = text(Indent + Check.mSectionID) | color(Color::Blue);
        }
        else if (!Check.mbPresent && Check.mbCanonical && !Check.mbRequired)
        {
            Row = text(Indent + Check.mSectionID) | dim;
        }
        else if (Check.mbPresent && !Check.mbCanonical)
        {
            Row = text(Indent + Check.mSectionID) | color(Color::Orange1);
        }
        else if (!Check.mbPresent && Check.mbRequired)
        {
            Row = text(Indent + Check.mSectionID + " (MISSING)") |
                  color(Color::Red);
        }
        else
        {
            continue;
        }

        Rows.push_back(Row);
    }

    std::string Summary = std::to_string(InResult.mRequiredPresent) + "/" +
                          std::to_string(InResult.mRequiredCount) + " required";
    if (InResult.mExtraCount > 0)
    {
        Summary += ", " + std::to_string(InResult.mExtraCount) + " extra";
    }

    return window(text(" " + InTitle + " (" + Summary + ") ") | bold,
                  vbox(std::move(Rows)));
}

Element SchemaPanel::Render(const FWatchPlanSummary &InPlan,
                            int InSelectedPhaseIndex) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" SCHEMA ") | bold | dim,
                      text("  No plan selected") | dim);
    }

    const FWatchTopicSchemaResult &Schema = InPlan.mSchemaResult;

    // Block 1: Plan schema
    auto PlanBlock = RenderSchemaBlock("PLAN", Schema.mPlan);

    // Block 2: Impl schema
    auto ImplBlock = RenderSchemaBlock("IMPL", Schema.mImpl);

    // Block 3: Playbook schema for selected phase
    Element PlaybookBlock;
    if (InSelectedPhaseIndex >= 0 &&
        InSelectedPhaseIndex < static_cast<int>(Schema.mPlaybooks.size()))
    {
        const FWatchDocSchemaResult &PB =
            Schema.mPlaybooks[static_cast<size_t>(InSelectedPhaseIndex)];
        PlaybookBlock = RenderSchemaBlock("PLAYBOOK " + PB.mPhaseKey, PB);
    }
    else if (!Schema.mPlaybooks.empty())
    {
        // Find playbook matching selected phase by key
        const FWatchDocSchemaResult *rpMatch = nullptr;
        if (InSelectedPhaseIndex >= 0 &&
            InSelectedPhaseIndex < static_cast<int>(InPlan.mPhases.size()))
        {
            const std::string &PhaseKey =
                InPlan.mPhases[static_cast<size_t>(InSelectedPhaseIndex)]
                    .mPhaseKey;
            for (const FWatchDocSchemaResult &PB : Schema.mPlaybooks)
            {
                if (PB.mPhaseKey == PhaseKey)
                {
                    rpMatch = &PB;
                    break;
                }
            }
        }
        if (rpMatch != nullptr)
        {
            PlaybookBlock =
                RenderSchemaBlock("PLAYBOOK " + rpMatch->mPhaseKey, *rpMatch);
        }
        else
        {
            PlaybookBlock =
                window(text(" PLAYBOOK ") | bold | dim,
                       text("  No playbook for selected phase") | dim);
        }
    }
    else
    {
        PlaybookBlock = window(text(" PLAYBOOK ") | bold | dim,
                               text("  No playbooks") | dim);
    }

    // Plan sidecar schemas
    auto PlanCLBlock =
        RenderSchemaBlock("PLAN CHANGELOG", Schema.mPlanChangeLog);
    auto PlanVerifBlock =
        RenderSchemaBlock("PLAN VERIFICATION", Schema.mPlanVerification);

    // Impl sidecar schemas
    auto ImplCLBlock =
        RenderSchemaBlock("IMPL CHANGELOG", Schema.mImplChangeLog);
    auto ImplVerifBlock =
        RenderSchemaBlock("IMPL VERIFICATION", Schema.mImplVerification);

    // Playbook sidecar schemas for selected phase
    std::string SelectedPhaseKey;
    if (InSelectedPhaseIndex >= 0 &&
        InSelectedPhaseIndex < static_cast<int>(InPlan.mPhases.size()))
    {
        SelectedPhaseKey =
            InPlan.mPhases[static_cast<size_t>(InSelectedPhaseIndex)].mPhaseKey;
    }

    Element PBCLBlock;
    const FWatchDocSchemaResult *rpPBCL = nullptr;
    for (const FWatchDocSchemaResult &PBCL : Schema.mPlaybookChangeLogs)
    {
        if (PBCL.mPhaseKey == SelectedPhaseKey)
        {
            rpPBCL = &PBCL;
            break;
        }
    }
    if (rpPBCL != nullptr)
    {
        PBCLBlock =
            RenderSchemaBlock("PB CHANGELOG " + rpPBCL->mPhaseKey, *rpPBCL);
    }
    else
    {
        PBCLBlock = window(text(" PB CHANGELOG ") | bold | dim,
                           text("  No sidecar for phase") | dim);
    }

    Element PBVerifBlock;
    const FWatchDocSchemaResult *rpPBVerif = nullptr;
    for (const FWatchDocSchemaResult &PBV : Schema.mPlaybookVerifications)
    {
        if (PBV.mPhaseKey == SelectedPhaseKey)
        {
            rpPBVerif = &PBV;
            break;
        }
    }
    if (rpPBVerif != nullptr)
    {
        PBVerifBlock = RenderSchemaBlock(
            "PB VERIFICATION " + rpPBVerif->mPhaseKey, *rpPBVerif);
    }
    else
    {
        PBVerifBlock = window(text(" PB VERIFICATION ") | bold | dim,
                              text("  No sidecar for phase") | dim);
    }

    static constexpr int kSchemaColumnWidth = 60;

    auto LeftColumn = vbox({PlanBlock, PlanCLBlock, PlanVerifBlock, ImplBlock,
                            ImplCLBlock, ImplVerifBlock});
    auto RightColumn = vbox({PlaybookBlock, PBCLBlock, PBVerifBlock});

    return hbox({
        LeftColumn | size(WIDTH, EQUAL, kSchemaColumnWidth),
        RightColumn | size(WIDTH, EQUAL, kSchemaColumnWidth),
    });
}

// ---------------------------------------------------------------------------
// Sidecar lookup helper
// ---------------------------------------------------------------------------

static const FWatchSidecarSummary *
FindSidecar(const FWatchPlanSummary &InPlan, const std::string &InOwnerKind,
            const std::string &InDocKind, const std::string &InPhaseKey = "")
{
    for (const FWatchSidecarSummary &Sidecar : InPlan.mSidecarSummaries)
    {
        if (Sidecar.mOwnerKind == InOwnerKind && Sidecar.mDocKind == InDocKind)
        {
            if (InPhaseKey.empty() || Sidecar.mPhaseKey == InPhaseKey)
            {
                return &Sidecar;
            }
        }
    }
    return nullptr;
}

static Element RenderSidecarPanel(const std::string &InTitle,
                                  const FWatchSidecarSummary *InRpSidecar)
{
    if (InRpSidecar == nullptr)
    {
        return window(text(" " + InTitle + " ") | bold | dim,
                      text("  (not found)") | dim);
    }

    Elements Rows;
    Rows.push_back(text(" Path: " + InRpSidecar->mPath) | dim);
    Rows.push_back(
        text(" Entries: " + std::to_string(InRpSidecar->mEntryCount)) |
        color(InRpSidecar->mEntryCount > 0 ? Color::Green : Color::Yellow));
    if (!InRpSidecar->mLatestDate.empty())
    {
        Rows.push_back(text(" Latest: " + InRpSidecar->mLatestDate) | dim);
    }
    return window(text(" " + InTitle + " ") | bold, vbox(std::move(Rows)));
}

// ---------------------------------------------------------------------------
// PlaybookChangeLogPanel
// ---------------------------------------------------------------------------

Element PlaybookChangeLogPanel::Render(const FWatchPlanSummary &InPlan,
                                       int InSelectedPhaseIndex) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" PB CHANGELOG ") | bold | dim,
                      text("  No plan selected") | dim);
    }

    std::string PhaseKey;
    if (InSelectedPhaseIndex >= 0 &&
        InSelectedPhaseIndex < static_cast<int>(InPlan.mPhases.size()))
    {
        PhaseKey =
            InPlan.mPhases[static_cast<size_t>(InSelectedPhaseIndex)].mPhaseKey;
    }

    const FWatchSidecarSummary *rpSidecar =
        FindSidecar(InPlan, "Playbook", "ChangeLog", PhaseKey);
    return RenderSidecarPanel("PB CHANGELOG " + PhaseKey, rpSidecar);
}

// ---------------------------------------------------------------------------
// PlaybookVerificationPanel
// ---------------------------------------------------------------------------

Element PlaybookVerificationPanel::Render(const FWatchPlanSummary &InPlan,
                                          int InSelectedPhaseIndex) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" PB VERIFICATION ") | bold | dim,
                      text("  No plan selected") | dim);
    }

    std::string PhaseKey;
    if (InSelectedPhaseIndex >= 0 &&
        InSelectedPhaseIndex < static_cast<int>(InPlan.mPhases.size()))
    {
        PhaseKey =
            InPlan.mPhases[static_cast<size_t>(InSelectedPhaseIndex)].mPhaseKey;
    }

    const FWatchSidecarSummary *rpSidecar =
        FindSidecar(InPlan, "Playbook", "Verification", PhaseKey);
    return RenderSidecarPanel("PB VERIFICATION " + PhaseKey, rpSidecar);
}

// ---------------------------------------------------------------------------
// PlanChangeLogPanel
// ---------------------------------------------------------------------------

Element PlanChangeLogPanel::Render(const FWatchPlanSummary &InPlan) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" PLAN CHANGELOG ") | bold | dim,
                      text("  No plan selected") | dim);
    }

    const FWatchSidecarSummary *rpSidecar =
        FindSidecar(InPlan, "Plan", "ChangeLog");
    return RenderSidecarPanel("PLAN CHANGELOG", rpSidecar);
}

// ---------------------------------------------------------------------------
// PlanVerificationPanel
// ---------------------------------------------------------------------------

Element PlanVerificationPanel::Render(const FWatchPlanSummary &InPlan) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" PLAN VERIFICATION ") | bold | dim,
                      text("  No plan selected") | dim);
    }

    const FWatchSidecarSummary *rpSidecar =
        FindSidecar(InPlan, "Plan", "Verification");
    return RenderSidecarPanel("PLAN VERIFICATION", rpSidecar);
}

// ---------------------------------------------------------------------------
// ImplChangeLogPanel
// ---------------------------------------------------------------------------

Element ImplChangeLogPanel::Render(const FWatchPlanSummary &InPlan) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" IMPL CHANGELOG ") | bold | dim,
                      text("  No plan selected") | dim);
    }

    const FWatchSidecarSummary *rpSidecar =
        FindSidecar(InPlan, "Impl", "ChangeLog");
    return RenderSidecarPanel("IMPL CHANGELOG", rpSidecar);
}

// ---------------------------------------------------------------------------
// ImplVerificationPanel
// ---------------------------------------------------------------------------

Element ImplVerificationPanel::Render(const FWatchPlanSummary &InPlan) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" IMPL VERIFICATION ") | bold | dim,
                      text("  No plan selected") | dim);
    }

    const FWatchSidecarSummary *rpSidecar =
        FindSidecar(InPlan, "Impl", "Verification");
    return RenderSidecarPanel("IMPL VERIFICATION", rpSidecar);
}

// ---------------------------------------------------------------------------
// FileManifestPanel
// ---------------------------------------------------------------------------

Element FileManifestPanel::Render(const FPhaseTaxonomy &InTaxonomy,
                                  int InFilePageIndex) const
{
    if (InTaxonomy.mFileManifest.empty())
    {
        return window(
            text(" FILES: " + ("P" + std::to_string(InTaxonomy.mPhaseIndex)) +
                 " (0) ") |
                bold,
            text("  (no file changes planned)") | dim);
    }

    static constexpr int kPageSize = 10;
    const int TotalFiles = static_cast<int>(InTaxonomy.mFileManifest.size());
    const int PageStart = InFilePageIndex * kPageSize;
    const int PageEnd = std::min(PageStart + kPageSize, TotalFiles);

    std::vector<std::vector<Element>> TableData;
    TableData.push_back({text("File") | bold | flex, text("Action") | bold,
                         text("Description") | bold | flex});

    for (int Index = PageStart; Index < PageEnd; ++Index)
    {
        const FFileManifestItem &Item =
            InTaxonomy.mFileManifest[static_cast<size_t>(Index)];
        auto ActionColor = Color::White;
        if (Item.mAction == EFileAction::Create)
        {
            ActionColor = Color::Green;
        }
        else if (Item.mAction == EFileAction::Modify)
        {
            ActionColor = Color::Yellow;
        }
        else if (Item.mAction == EFileAction::Delete)
        {
            ActionColor = Color::Red;
        }
        TableData.push_back({
            text(Item.mFilePath) | flex,
            text(std::string(ToString(Item.mAction))) | color(ActionColor),
            text(Item.mDescription) | dim | flex,
        });
    }

    auto FileTable = Table(std::move(TableData));
    FileTable.SelectAll().SeparatorVertical(EMPTY);
    FileTable.SelectRow(0).SeparatorHorizontal(LIGHT);

    std::string Title =
        " FILES: " + ("P" + std::to_string(InTaxonomy.mPhaseIndex)) + " (" +
        std::to_string(TotalFiles) + ")";
    if (TotalFiles > kPageSize)
    {
        const int PageCount = (TotalFiles + kPageSize - 1) / kPageSize;
        Title += " [" + std::to_string(InFilePageIndex + 1) + "/" +
                 std::to_string(PageCount) + "]";
    }
    Title += " ";

    return window(text(Title) | bold, FileTable.Render() | flex);
}

// ---------------------------------------------------------------------------
// PlanDetailPanel
// ---------------------------------------------------------------------------

Element PlanDetailPanel::Render(const FWatchPlanSummary &InPlan) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" PLAN DETAIL ") | bold | dim,
                      text("  No plan selected") | dim);
    }

    // Two-column layout. Left column: descriptive plan framing (Summary /
    // Goals / Non-Goals). Right column: actionable state (Risks / Next
    // Actions / Acceptance Criteria). Separating the narrative from the
    // live-state entries keeps wide screens legible and stops the single
    // column from stretching past a reasonable reading width.
    Elements LeftRows;

    // Summary sub-section
    LeftRows.push_back(text("  Summary") | bold);
    if (InPlan.mSummaryLines.empty())
    {
        LeftRows.push_back(text("    (none)") | dim);
    }
    else
    {
        for (const std::string &Line : InPlan.mSummaryLines)
        {
            LeftRows.push_back(text("    " + Line) | dim);
        }
    }

    LeftRows.push_back(text(""));

    // Goals sub-section — matches schema `goals` field. Label must
    // match the schema vocabulary so AI agents reading the watch TUI
    // and the CLI/JSON see the same noun. Pre-v0.90.0 this was labeled
    // "Scope" which collided with the per-phase/lane/job `scope` field.
    LeftRows.push_back(text("  Goals") | bold);
    if (InPlan.mGoalStatements.empty())
    {
        LeftRows.push_back(text("    (none)") | dim);
    }
    else
    {
        for (const std::string &Goal : InPlan.mGoalStatements)
        {
            LeftRows.push_back(text("    \xe2\x97\x8f " + Goal) |
                               color(Color::Green));
        }
    }

    LeftRows.push_back(text(""));

    // Non-Goals sub-section — matches schema `non_goals` field. See
    // note above for the rename rationale.
    LeftRows.push_back(text("  Non-Goals") | bold);
    if (InPlan.mNonGoalStatements.empty())
    {
        LeftRows.push_back(text("    (none)") | dim);
    }
    else
    {
        for (const std::string &NonGoal : InPlan.mNonGoalStatements)
        {
            LeftRows.push_back(text("    \xe2\x97\x8b " + NonGoal) |
                               color(Color::Yellow));
        }
    }

    // Right column — v0.89.0 typed-array sub-sections: Risks, Next Actions,
    // Acceptance Criteria. Each renders with status/severity color-coding
    // so the operator can tell at a glance which entries need attention.
    Elements RightRows;
    const auto SeverityColor = [](ERiskSeverity InSeverity) -> Color
    {
        switch (InSeverity)
        {
        case ERiskSeverity::Low:
            return Color::Green;
        case ERiskSeverity::Medium:
            return Color::Yellow;
        case ERiskSeverity::High:
        case ERiskSeverity::Critical:
            return Color::Red;
        }
        return Color::White;
    };
    const auto CriterionGlyph = [](ECriterionStatus InStatus) -> const char *
    {
        switch (InStatus)
        {
        case ECriterionStatus::Met:
            return "\xe2\x97\x8f"; // filled
        case ECriterionStatus::NotMet:
            return "\xe2\x97\x8b"; // hollow
        case ECriterionStatus::Partial:
            return "\xe2\x97\x90"; // half
        case ECriterionStatus::NotApplicable:
            return "\xe2\x8a\x98"; // crossed
        }
        return " ";
    };
    const auto CriterionColor = [](ECriterionStatus InStatus) -> Color
    {
        switch (InStatus)
        {
        case ECriterionStatus::Met:
            return Color::Green;
        case ECriterionStatus::NotMet:
            return Color::Yellow;
        case ECriterionStatus::Partial:
            return Color::Yellow;
        case ECriterionStatus::NotApplicable:
            return Color::GrayDark;
        }
        return Color::White;
    };

    RightRows.push_back(text("  Risks") | bold);
    if (InPlan.mRiskEntries.empty())
    {
        RightRows.push_back(text("    (none)") | dim);
    }
    else
    {
        for (const FRiskEntry &R : InPlan.mRiskEntries)
        {
            const std::string Prefix = std::string("    [") +
                                       ToString(R.mSeverity) + "/" +
                                       ToString(R.mStatus) + "] ";
            RightRows.push_back(text(Prefix + R.mStatement) |
                                color(SeverityColor(R.mSeverity)));
        }
    }

    RightRows.push_back(text(""));
    RightRows.push_back(text("  Next Actions") | bold);
    if (InPlan.mNextActionEntries.empty())
    {
        RightRows.push_back(text("    (none)") | dim);
    }
    else
    {
        // Sort by mOrder for display without mutating the snapshot.
        std::vector<FNextActionEntry> Sorted = InPlan.mNextActionEntries;
        std::sort(Sorted.begin(), Sorted.end(),
                  [](const FNextActionEntry &A, const FNextActionEntry &B)
                  { return A.mOrder < B.mOrder; });
        for (const FNextActionEntry &A : Sorted)
        {
            const std::string Prefix = "    " + std::to_string(A.mOrder) +
                                       ". [" + ToString(A.mStatus) + "] ";
            const Color C =
                (A.mStatus == EActionStatus::Completed)
                    ? Color::Green
                    : (A.mStatus == EActionStatus::Abandoned ? Color::GrayDark
                                                             : Color::Cyan);
            RightRows.push_back(text(Prefix + A.mStatement) | color(C));
        }
    }

    RightRows.push_back(text(""));
    RightRows.push_back(text("  Acceptance Criteria") | bold);
    if (InPlan.mAcceptanceCriteria.empty())
    {
        RightRows.push_back(text("    (none)") | dim);
    }
    else
    {
        for (const FAcceptanceCriterionEntry &C : InPlan.mAcceptanceCriteria)
        {
            const std::string Glyph = CriterionGlyph(C.mStatus);
            const std::string IdPart = C.mId.empty() ? "" : (C.mId + " ");
            RightRows.push_back(
                text("    " + Glyph + " " + IdPart + C.mStatement) |
                color(CriterionColor(C.mStatus)));
        }
    }

    // True 2:1 proportional split via the weighted-flex decorator defined
    // at the top of this file. Extra horizontal space in the hbox is
    // distributed 2/3 to the narrative left column, 1/3 to the live-state
    // right column — holds the proportion across any terminal width,
    // unlike the earlier fixed-width right column that skewed on wide
    // terminals.
    return window(text(" PLAN DETAIL ") | bold | color(Color::Cyan),
                  hbox({
                      vbox(std::move(LeftRows)) | FlexWithWeight(2),
                      separator(),
                      vbox(std::move(RightRows)) | FlexWithWeight(1),
                  }));
}

// ---------------------------------------------------------------------------
// WatchStatusBar
// ---------------------------------------------------------------------------

Element WatchStatusBar::Render(
    const std::string &InVersion, const std::string &InTime, int InTick,
    int InPollMs, const FWatchInventoryCounters &InCounters,
    const FDocWatchSnapshot::FPerformance &InPerformance) const
{
    const std::string Validation =
        InPerformance.mbValidationRan
            ? std::to_string(InPerformance.mValidationDurationMs) + "ms"
            : "pending/reuse";
    const std::string Lint =
        InPerformance.mbLintRan
            ? std::to_string(InPerformance.mLintDurationMs) + "ms"
            : "pending/reuse";
    return hbox({
        text(" Poll #" + std::to_string(InTick)) | bold,
        text("  |  Last: " + std::to_string(InPollMs) + "ms") | dim,
        text("  |  Plans: " + std::to_string(InCounters.mPlanCount)) | dim,
        text("  |  Bundles: " +
             std::to_string(InPerformance.mBundleReloadCount) + "r/" +
             std::to_string(InPerformance.mBundleReuseCount) + "u") |
            dim,
        text("  |  Metrics: " +
             std::to_string(InPerformance.mMetricRecomputeCount)) |
            dim,
        text("  |  Val: " + Validation) | dim,
        text("  |  Lint: " + Lint) | dim,
        filler(),
        text("q=quit  a/A=plan  n/N=non-active  p/P=phase  d=metrics  "
             "w/W=wave  l/L=lane  f/F=files  s=schema  i=impl  r=refresh") |
            dim,
        text("  "),
    });
}

} // namespace UniPlan
