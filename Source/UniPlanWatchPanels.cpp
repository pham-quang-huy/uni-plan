#include "UniPlanWatchPanels.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace UniPlan
{

using namespace ftxui;

// ---------------------------------------------------------------------------
// Color helpers
// ---------------------------------------------------------------------------

static Element ColorStatus(const std::string& InStatus)
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

static Element PhaseProgressBar(int InDone, int InTotal)
{
    if (InTotal == 0)
    {
        return text("no phases") | dim;
    }
    Elements Bar;
    for (int Index = 0; Index < InTotal; ++Index)
    {
        if (Index < InDone)
        {
            Bar.push_back(text("\xe2\x96\x88") | color(Color::Green));
        }
        else
        {
            Bar.push_back(text("\xe2\x96\x91") | dim);
        }
    }
    Bar.push_back(text(" " + std::to_string(InDone) + "/" + std::to_string(InTotal)));
    return hbox(std::move(Bar));
}

static constexpr int kPBLinesGlobalMax = 500;
static constexpr int kPBLinesBarWidth = 10;

static Element PBLinesBar(int InLines)
{
    if (InLines <= 0)
    {
        return text("-") | dim;
    }
    const int Filled = std::min(kPBLinesBarWidth, (InLines * kPBLinesBarWidth + kPBLinesGlobalMax - 1) / kPBLinesGlobalMax);
    const int Empty = kPBLinesBarWidth - Filled;
    const auto BarColor = (InLines > kPBLinesGlobalMax) ? color(Color::Red) : color(Color::Cyan);
    Elements Bar;
    for (int Index = 0; Index < Filled; ++Index)
    {
        Bar.push_back(text("\xe2\x96\x88") | BarColor);
    }
    for (int Index = 0; Index < Empty; ++Index)
    {
        Bar.push_back(text("\xe2\x96\x91") | dim);
    }
    Bar.push_back(text(" " + std::to_string(InLines)));
    return hbox(std::move(Bar));
}

static std::string Truncate(const std::string& InText, size_t InMax)
{
    if (InText.size() <= InMax)
    {
        return InText;
    }
    if (InMax <= 3)
    {
        return InText.substr(0, InMax);
    }
    return InText.substr(0, InMax - 2) + "..";
}

// Insert separatorEmpty() between each cell in a gridbox row for consistent column spacing.
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

Element InventoryPanel::Render(const FWatchInventoryCounters& InCounters) const
{
    std::vector<std::vector<Element>> Data;
    Data.push_back({text("Plans") | dim, text(std::to_string(InCounters.mPlanCount))});
    Data.push_back({text("Playbooks") | dim, text(std::to_string(InCounters.mPlaybookCount))});
    Data.push_back({text("Impls") | dim, text(std::to_string(InCounters.mImplementationCount))});
    Data.push_back({text("Pairs") | dim, text(std::to_string(InCounters.mPairCount))});
    Data.push_back({text("Sidecars") | dim, text(std::to_string(InCounters.mSidecarCount))});

    auto InvTable = Table(std::move(Data));

    return window(
        text(" INVENTORY ") | bold,
        vbox({
            InvTable.Render() | flex,
            separator(),
            hbox({text("\xe2\x96\x88 ") | color(Color::Green), text(std::to_string(InCounters.mActivePlanCount) + " active")}),
            hbox({text("\xe2\x96\x88 ") | dim, text(std::to_string(InCounters.mNonActivePlanCount) + " non-active")}),
        }));
}

// ---------------------------------------------------------------------------
// ValidationPanel
// ---------------------------------------------------------------------------

Element ValidationPanel::Render(const FWatchValidationSummary& InValidation) const
{
    auto SummaryLine = hbox({
        text("\xe2\x9c\x93 " + std::to_string(InValidation.mPassedChecks) + " passed") | color(Color::Green),
        text("  "),
        text("\xe2\x9c\x97 " + std::to_string(InValidation.mFailedChecks) + " failed") | color(InValidation.mFailedChecks > 0 ? Color::Red : Color::Green),
        text("  (" + std::to_string(InValidation.mCriticalFailures) + " critical)") | dim,
    });

    Elements Content;
    Content.push_back(SummaryLine);
    Content.push_back(separator());

    if (InValidation.mFailedChecks == 0)
    {
        Content.push_back(text("All checks passed") | color(Color::Green) | bold);
    }
    else
    {
        std::vector<std::vector<Element>> CheckData;
        CheckData.push_back({text("Check") | bold, text("Result") | bold});
        for (const ValidateCheck& Check : InValidation.mFailedCheckDetails)
        {
            CheckData.push_back({text(Check.mId) | flex, text("FAIL") | color(Color::Red)});
        }
        auto CheckTable = Table(std::move(CheckData));
        CheckTable.SelectAll().SeparatorVertical(EMPTY);
        CheckTable.SelectRow(0).SeparatorHorizontal(LIGHT);
        Content.push_back(CheckTable.Render() | flex);
    }

    return window(text(" VALIDATION  " + std::to_string(InValidation.mTotalChecks) + " checks ") | bold, vbox(std::move(Content)));
}

// ---------------------------------------------------------------------------
// LintPanel
// ---------------------------------------------------------------------------

Element LintPanel::Render(const FWatchLintSummary& InLint) const
{
    std::vector<std::vector<Element>> Data;
    Data.push_back({text("Total warnings") | dim, text(std::to_string(InLint.mWarningCount)) | (InLint.mWarningCount > 0 ? color(Color::Yellow) : dim)});
    Data.push_back({text("Name pattern") | dim, text(std::to_string(InLint.mNamePatternWarnings))});
    Data.push_back({text("Missing H1") | dim, text(std::to_string(InLint.mMissingH1Warnings))});

    auto LintTable = Table(std::move(Data));

    Elements Content;
    Content.push_back(LintTable.Render() | flex);
    Content.push_back(separator());

    if (InLint.mWarningCount > 0)
    {
        const float Ratio = std::min(1.0f, static_cast<float>(InLint.mWarningCount) / 200.0f);
        const int LintBarWidth = 15;
        const int LintFilled = std::min(LintBarWidth, static_cast<int>(Ratio * static_cast<float>(LintBarWidth)));
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

Element ActivePlansPanel::Render(const std::vector<FWatchPlanSummary>& InPlans, int InSelectedIndex) const
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
    ActiveTableData.push_back({text("Topic") | bold | flex, text("Status") | bold, text("Phases") | bold, text("PB") | bold, text("BLK") | bold});
    int SelectedActiveRow = -1;

    for (int Index = ScrollOffset; Index < VisibleEnd; ++Index)
    {
        const FWatchPlanSummary& Plan = InPlans[static_cast<size_t>(Index)];
        const bool Selected = (Index == InSelectedIndex);
        const std::string Marker;

        // Find active phase key
        std::string ActivePhase;
        for (const PhaseItem& Phase : Plan.mPhases)
        {
            if (Phase.mStatus == "in_progress")
            {
                ActivePhase = Phase.mPhaseKey;
                break;
            }
        }

        Elements PhaseCol;
        PhaseCol.push_back(PhaseProgressBar(Plan.mPhaseCompleted, Plan.mPhaseCount));
        if (!ActivePhase.empty())
        {
            PhaseCol.push_back(text("  "));
            PhaseCol.push_back(text("\xe2\x96\xb6" + ActivePhase) | color(Color::Cyan));
        }

        ActiveTableData.push_back({
            text(Marker + Plan.mTopicKey) | flex | (Selected ? bold : nothing),
            ColorStatus(Plan.mPlanStatus) | (Selected ? bold : nothing),
            hbox(std::move(PhaseCol)) | (Selected ? bold : nothing),
            text(std::to_string(Plan.mPlaybookCount)) | (Selected ? bold : nothing),
            text(std::to_string(Plan.mBlockerCount)) | (Plan.mBlockerCount > 0 ? color(Color::Red) : dim) | (Selected ? bold : nothing),
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
        FinalRows.push_back(text("  \xe2\x86\x91 " + std::to_string(ScrollOffset) + " above") | dim);
    }
    FinalRows.push_back(ActiveTable.Render() | flex);
    if (VisibleEnd < Count)
    {
        FinalRows.push_back(text("  \xe2\x86\x93 " + std::to_string(Count - VisibleEnd) + " below") | dim);
    }

    const std::string Title = " [A]CTIVE PLANS (" + std::to_string(Count) + ") ";
    return window(text(Title) | bold | color(Color::Green), vbox(std::move(FinalRows)));
}

// ---------------------------------------------------------------------------
// PhaseDetailPanel
// ---------------------------------------------------------------------------

Element PhaseDetailPanel::Render(const FWatchPlanSummary& InPlan, int InSelectedPhaseIndex) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" PHASE DETAIL ") | bold, text("No plan selected") | dim);
    }

    // gridbox layout — auto-aligns columns without Table's flex_shrink override
    std::vector<Elements> GridRows;
    GridRows.push_back(PadGridRow({
        text("Phase") | bold | size(WIDTH, EQUAL, 14),
        text("Status") | bold | size(WIDTH, EQUAL, 14),
        text("PB") | bold | size(WIDTH, EQUAL, 4),
        text("PBLines") | bold | size(WIDTH, EQUAL, 16),
        text("Taxonomy") | bold | size(WIDTH, EQUAL, 30),
        text("Scope") | bold | flex,
        text("Output") | bold | flex,
    }));

    const int MaxPhases = 30;
    const int Count = static_cast<int>(InPlan.mPhases.size());

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
        const PhaseItem& Phase = InPlan.mPhases[static_cast<size_t>(Index)];
        const bool Selected = (Index == InSelectedPhaseIndex);
        const std::string Marker;

        const bool HasPlaybook = !Phase.mPlaybookPath.empty();
        const std::string PlaybookMarker = HasPlaybook ? "\xe2\x9c\x93" : "\xe2\x9c\x97";
        const auto PlaybookColor = HasPlaybook ? color(Color::Green) : color(Color::Red);

        std::string Desc;
        for (const auto& Field : Phase.mFields)
        {
            const std::string Key = ToLower(Field.first);
            if (Key == "scope" || Key == "description" || Key == "output" || Key == "goal" || Key == "deliverables" || Key == "deliverable" || Key == "name" || Key == "focus" || Key == "main tasks" || Key == "work")
            {
                Desc = Field.second;
                break;
            }
        }

        // Extract Output field
        std::string Output;
        for (const auto& Field : Phase.mFields)
        {
            const std::string FieldKey = ToLower(Field.first);
            if (FieldKey == "output" || FieldKey == "deliverables" || FieldKey == "deliverable" || FieldKey == "exit criteria" || FieldKey == "exit_criteria" || FieldKey == "main tasks")
            {
                Output = Field.second;
                break;
            }
        }

        // Playbook line count
        std::string PBLines = "-";
        int PBLinesInt = 0;
        // Taxonomy summary
        std::string TaxSummary = "-";

        for (const FWatchPhaseTaxonomy& Tax : InPlan.mPhaseTaxonomies)
        {
            if (Tax.mPhaseKey == Phase.mPhaseKey)
            {
                PBLines = std::to_string(Tax.mPlaybookLineCount);
                PBLinesInt = Tax.mPlaybookLineCount;

                int LD = 0, LA = 0, LT = 0;
                for (const FWatchLaneItem& L : Tax.mLanes)
                {
                    if (L.mStatus == "completed" || L.mStatus == "closed")
                    {
                        LD++;
                    }
                    else if (L.mStatus == "in_progress")
                    {
                        LA++;
                    }
                    else
                    {
                        LT++;
                    }
                }
                int JD = 0, JA = 0, JT = 0;
                for (const FWatchJobItem& J : Tax.mJobs)
                {
                    if (J.mStatus == "completed" || J.mStatus == "closed")
                    {
                        JD++;
                    }
                    else if (J.mStatus == "in_progress")
                    {
                        JA++;
                    }
                    else
                    {
                        JT++;
                    }
                }
                int TD = 0, TA = 0, TT = 0;
                for (const FWatchTaskItem& T : Tax.mTasks)
                {
                    if (T.mStatus == "completed" || T.mStatus == "closed")
                    {
                        TD++;
                    }
                    else if (T.mStatus == "in_progress")
                    {
                        TA++;
                    }
                    else
                    {
                        TT++;
                    }
                }

                TaxSummary = "";
                if (!Tax.mLanes.empty())
                {
                    TaxSummary += std::to_string(Tax.mLanes.size()) + "L:" + std::to_string(LD) + "d " + std::to_string(LA) + "a " + std::to_string(LT) + "t";
                }
                if (!Tax.mJobs.empty())
                {
                    if (!TaxSummary.empty())
                    {
                        TaxSummary += " ";
                    }
                    TaxSummary += std::to_string(Tax.mJobs.size()) + "J:" + std::to_string(JD) + "d " + std::to_string(JA) + "a " + std::to_string(JT) + "t";
                }
                if (!Tax.mTasks.empty())
                {
                    if (!TaxSummary.empty())
                    {
                        TaxSummary += " ";
                    }
                    TaxSummary += std::to_string(Tax.mTasks.size()) + "T:" + std::to_string(TD) + "d " + std::to_string(TA) + "a " + std::to_string(TT) + "t";
                }
                if (TaxSummary.empty())
                {
                    TaxSummary = "-";
                }
                break;
            }
        }

        Elements RowCells = PadGridRow({
            text(Marker + Phase.mPhaseKey) | size(WIDTH, EQUAL, 14),
            ColorStatus(Phase.mStatus) | size(WIDTH, EQUAL, 14),
            text(PlaybookMarker) | PlaybookColor | size(WIDTH, EQUAL, 4),
            PBLinesBar(PBLinesInt) | size(WIDTH, EQUAL, 16),
            text(TaxSummary) | dim | size(WIDTH, EQUAL, 30),
            text(Desc) | dim | flex,
            text(Output) | dim | flex,
        });
        if (Selected)
        {
            for (auto& Cell : RowCells)
            {
                Cell = Cell | inverted | bold;
            }
        }
        GridRows.push_back(std::move(RowCells));
    }

    Elements FinalRows;
    if (ScrollOffset > 0)
    {
        FinalRows.push_back(text("  \xe2\x86\x91 " + std::to_string(ScrollOffset) + " above") | dim);
    }
    FinalRows.push_back(gridbox(std::move(GridRows)) | flex);
    if (VisibleEnd < Count)
    {
        FinalRows.push_back(text("  \xe2\x86\x93 " + std::to_string(Count - VisibleEnd) + " below") | dim);
    }

    const std::string Title = " [P]HASE DETAIL: " + Truncate(InPlan.mTopicKey, 30) + " (" + std::to_string(Count) + ") ";
    return window(text(Title) | bold | color(Color::Cyan), vbox(std::move(FinalRows)));
}

// ---------------------------------------------------------------------------
// BlockersPanel
// ---------------------------------------------------------------------------

Element BlockersPanel::Render(const std::vector<BlockerItem>& InBlockers) const
{
    Elements Rows;

    if (InBlockers.empty())
    {
        Rows.push_back(text("(no open blockers)") | dim);
        return window(text(" BLOCKERS ") | bold, vbox(std::move(Rows)));
    }

    std::vector<std::vector<Element>> BlockerTableData;
    BlockerTableData.push_back({text("Topic") | bold, text("Kind") | bold, text("Action") | bold | flex});

    const int MaxVisible = 10;
    const int Count = static_cast<int>(InBlockers.size());
    const int VisibleCount = std::min(MaxVisible, Count);

    for (int Index = 0; Index < VisibleCount; ++Index)
    {
        const BlockerItem& Blocker = InBlockers[static_cast<size_t>(Index)];
        BlockerTableData.push_back({
            text(Blocker.mTopicKey),
            text(Blocker.mKind),
            text(Blocker.mAction) | dim | flex,
        });
    }

    auto BlockerTable = Table(std::move(BlockerTableData));
    BlockerTable.SelectAll().SeparatorVertical(EMPTY);
    BlockerTable.SelectRow(0).SeparatorHorizontal(LIGHT);

    return window(text(" BLOCKERS (" + std::to_string(Count) + ") ") | bold | color(Color::Red), BlockerTable.Render() | flex);
}

// ---------------------------------------------------------------------------
// CompletedPlansPanel
// ---------------------------------------------------------------------------

Element CompletedPlansPanel::Render(const std::vector<FWatchPlanSummary>& InPlans, int InSelectedIndex) const
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

    std::vector<std::vector<Element>> ActiveTableData;
    ActiveTableData.push_back({text("Topic") | bold | flex, text("Status") | bold, text("Phases") | bold, text("PB") | bold, text("BLK") | bold});
    int SelectedActiveRow = -1;

    std::vector<std::vector<Element>> NonActiveTableData;
    NonActiveTableData.push_back({text("Topic") | bold | flex, text("Status") | bold});
    int SelectedNonActiveRow = -1;

    for (int Index = ScrollOffset; Index < VisibleEnd; ++Index)
    {
        const FWatchPlanSummary& Plan = InPlans[static_cast<size_t>(Index)];
        const bool Selected = (Index == InSelectedIndex);
        const std::string Marker;

        NonActiveTableData.push_back({
            text(Marker + Plan.mTopicKey) | flex | (Selected ? bold : nothing),
            ColorStatus(Plan.mPlanStatus) | (Selected ? bold : nothing),
        });
        if (Selected)
        {
            SelectedNonActiveRow = static_cast<int>(NonActiveTableData.size()) - 1;
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
        NonActiveFinal.push_back(text("  \xe2\x86\x91 " + std::to_string(ScrollOffset) + " above") | dim);
    }
    NonActiveFinal.push_back(NonActiveTable.Render() | flex);
    if (VisibleEnd < Count)
    {
        NonActiveFinal.push_back(text("  \xe2\x86\x93 " + std::to_string(Count - VisibleEnd) + " below") | dim);
    }

    if (Count == 0)
    {
        NonActiveFinal.push_back(text("(none)") | dim);
    }

    return window(text(" [N]ON-ACTIVE (" + std::to_string(Count) + ") ") | bold | dim, vbox(std::move(NonActiveFinal)));
}

// ---------------------------------------------------------------------------
// DeferredPlansPanel
// ---------------------------------------------------------------------------

Element DeferredPlansPanel::Render(const std::vector<FWatchPlanSummary>& InPlans) const
{
    const int Count = static_cast<int>(InPlans.size());

    if (Count == 0)
    {
        return window(text(" DEFERRED ") | bold | color(Color::Yellow), text("(none)") | dim);
    }

    std::vector<std::vector<Element>> Data;
    Data.push_back({text("Topic") | bold | flex, text("Status") | bold});

    for (int Index = 0; Index < std::min(10, Count); ++Index)
    {
        const FWatchPlanSummary& Plan = InPlans[static_cast<size_t>(Index)];
        Data.push_back({text(Plan.mTopicKey) | flex, ColorStatus(Plan.mPlanStatus)});
    }

    auto DefTable = Table(std::move(Data));
    DefTable.SelectAll().SeparatorVertical(EMPTY);
    DefTable.SelectRow(0).SeparatorHorizontal(LIGHT);

    return window(text(" DEFERRED (" + std::to_string(Count) + ") ") | bold | color(Color::Yellow), DefTable.Render() | flex);
}

// ---------------------------------------------------------------------------
// ValidationFailPanel
// ---------------------------------------------------------------------------

Element ValidationFailPanel::Render(const std::vector<ValidateCheck>& InFailedChecks) const
{
    if (InFailedChecks.empty())
    {
        return window(text(" VALIDATION FAILURES ") | bold, text("All checks passed") | color(Color::Green) | bold);
    }

    std::vector<std::vector<Element>> Data;
    Data.push_back({text("Check") | bold, text("Detail") | bold | flex});

    for (const ValidateCheck& Check : InFailedChecks)
    {
        std::string Detail = Check.mDetail;
        if (Detail.empty() && !Check.mDiagnostics.empty())
        {
            Detail = Check.mDiagnostics[0];
        }
        Data.push_back({text(Check.mId) | color(Color::Red), text(Detail) | dim | flex});
    }

    auto FailTable = Table(std::move(Data));
    FailTable.SelectAll().SeparatorVertical(EMPTY);
    FailTable.SelectRow(0).SeparatorHorizontal(LIGHT);

    return window(text(" VALIDATION FAILURES (" + std::to_string(InFailedChecks.size()) + ") ") | bold | color(Color::Red), FailTable.Render() | flex);
}

// ---------------------------------------------------------------------------
// ExecutionTaxonomyPanel
// ---------------------------------------------------------------------------

Element ExecutionTaxonomyPanel::Render(const FWatchPlanSummary& InPlan, int InSelectedPhaseIndex, int InSelectedWaveIndex, int InSelectedLaneIndex) const
{
    // Find the taxonomy for the selected phase
    const FWatchPhaseTaxonomy* rpTax = nullptr;
    if (InSelectedPhaseIndex >= 0 && InSelectedPhaseIndex < static_cast<int>(InPlan.mPhases.size()))
    {
        const std::string& SelectedKey = InPlan.mPhases[static_cast<size_t>(InSelectedPhaseIndex)].mPhaseKey;
        for (const FWatchPhaseTaxonomy& Tax : InPlan.mPhaseTaxonomies)
        {
            if (Tax.mPhaseKey == SelectedKey)
            {
                rpTax = &Tax;
                break;
            }
        }
    }

    if (rpTax == nullptr || (rpTax->mLanes.empty() && rpTax->mJobs.empty() && rpTax->mTasks.empty()))
    {
        std::string Hint = InPlan.mTopicKey.empty() ? "No plan selected" : "No execution taxonomy for selected phase";
        return window(text(" EXECUTION TAXONOMY ") | bold | dim, text("  " + Hint) | dim);
    }

    const FWatchPhaseTaxonomy& Tax = *rpTax;

    // --- LANES sub-panel (gridbox for fixed column widths) ---
    int LanesDone = 0;
    std::vector<Elements> LaneGridRows;
    LaneGridRows.push_back(PadGridRow({
        text("Lane") | bold,
        text("Status") | bold | size(WIDTH, EQUAL, 14),
        text("Scope") | bold | flex,
        text("Exit Criteria") | bold | flex,
    }));

    for (int Index = 0; Index < static_cast<int>(Tax.mLanes.size()); ++Index)
    {
        const FWatchLaneItem& Lane = Tax.mLanes[static_cast<size_t>(Index)];
        if (Lane.mStatus == "completed" || Lane.mStatus == "closed")
        {
            LanesDone++;
        }
        const bool LaneSel = (Index == InSelectedLaneIndex);
        const std::string Marker;

        Elements RowCells = PadGridRow({
            text(Marker + Lane.mLaneID),
            ColorStatus(Lane.mStatus) | size(WIDTH, EQUAL, 14),
            text(Lane.mScope) | dim | flex,
            text(Lane.mExitCriteria) | dim | flex,
        });
        if (LaneSel)
        {
            for (auto& Cell : RowCells)
            {
                Cell = Cell | inverted | bold;
            }
        }
        LaneGridRows.push_back(std::move(RowCells));
    }

    auto LanesPanel = window(text(" [L]ANES: " + Tax.mPhaseKey + " (" + std::to_string(Tax.mLanes.size()) + ") ") | bold, gridbox(std::move(LaneGridRows)) | flex);

    // --- JOB BOARD sub-panel (using ftxui::Table) ---
    // Collect unique waves for filtering
    std::vector<std::string> UniqueWaves;
    for (const FWatchJobItem& Job : Tax.mJobs)
    {
        bool Found = false;
        for (const std::string& W : UniqueWaves)
        {
            if (W == Job.mWaveID)
            {
                Found = true;
                break;
            }
        }
        if (!Found && !Job.mWaveID.empty())
        {
            UniqueWaves.push_back(Job.mWaveID);
        }
    }

    std::string WaveFilter;
    if (InSelectedWaveIndex >= 0 && InSelectedWaveIndex < static_cast<int>(UniqueWaves.size()))
    {
        WaveFilter = UniqueWaves[static_cast<size_t>(InSelectedWaveIndex)];
    }
    std::string LaneFilter;
    if (InSelectedLaneIndex >= 0 && InSelectedLaneIndex < static_cast<int>(Tax.mLanes.size()))
    {
        LaneFilter = Tax.mLanes[static_cast<size_t>(InSelectedLaneIndex)].mLaneID;
    }

    std::vector<std::vector<Element>> JobTableData;
    JobTableData.push_back({text("Wave") | bold, text("Lane") | bold, text("Job") | bold, text("Status") | bold, text("Scope") | bold | flex});

    int JobsDone = 0, JobsActive = 0, JobsTodo = 0;
    int VisibleJobCount = 0;
    for (const FWatchJobItem& Job : Tax.mJobs)
    {
        if (Job.mStatus == "completed" || Job.mStatus == "closed")
        {
            JobsDone++;
        }
        else if (Job.mStatus == "in_progress")
        {
            JobsActive++;
        }
        else
        {
            JobsTodo++;
        }

        if (!WaveFilter.empty() && Job.mWaveID != WaveFilter)
        {
            continue;
        }
        if (!LaneFilter.empty() && Job.mLaneID != LaneFilter)
        {
            continue;
        }

        std::string JobLabel = Job.mJobID;
        if (!Job.mJobName.empty())
        {
            JobLabel += " " + Job.mJobName;
        }

        JobTableData.push_back({
            text(Job.mWaveID),
            text(Job.mLaneID),
            text(JobLabel),
            ColorStatus(Job.mStatus),
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

    std::string WaveTitle = WaveFilter.empty() ? "all" : WaveFilter;
    std::string LaneTitle = LaneFilter.empty() ? "all" : LaneFilter;
    auto JobsPanel = window(text(" [W]AVE/[L]ANE/JOB BOARD: " + Tax.mPhaseKey + " (W=" + WaveTitle + " L=" + LaneTitle + ", " + std::to_string(Tax.mJobs.size()) + " jobs) ") | bold, JobsContent);

    // --- TASKS sub-panel (using ftxui::Table) ---
    std::vector<std::vector<Element>> TaskTableData;
    TaskTableData.push_back({text("Job") | bold, text("Task") | bold, text("Status") | bold, text("Description") | bold | flex, text("Evidence") | bold | flex});

    int TasksDone = 0, TasksActive = 0, TasksTodo = 0;
    int VisibleTaskCount = 0;
    for (const FWatchTaskItem& Task : Tax.mTasks)
    {
        if (Task.mStatus == "completed" || Task.mStatus == "closed")
        {
            TasksDone++;
        }
        else if (Task.mStatus == "in_progress")
        {
            TasksActive++;
        }
        else
        {
            TasksTodo++;
        }

        if (!LaneFilter.empty() && Task.mJobRef.find("/" + LaneFilter + "/") == std::string::npos && Task.mJobRef.find(LaneFilter + "/") != 0)
        {
            continue;
        }

        TaskTableData.push_back({
            text(Task.mJobRef),
            text(Task.mTaskID),
            ColorStatus(Task.mStatus),
            text(Task.mDescription) | dim | flex,
            text(Task.mEvidence) | dim | flex,
        });
        VisibleTaskCount++;
    }

    Element TasksContent;
    if (Tax.mTasks.empty())
    {
        TasksContent = text("  (no task checklist defined)") | dim;
    }
    else
    {
        auto TaskTable = Table(std::move(TaskTableData));
        TaskTable.SelectAll().SeparatorVertical(EMPTY);
        TaskTable.SelectRow(0).SeparatorHorizontal(LIGHT);
        TasksContent = TaskTable.Render() | flex;
    }

    auto TasksPanel = window(text(" TASKS: " + Tax.mPhaseKey + " (" + std::to_string(Tax.mTasks.size()) + ") ") | bold, TasksContent);

    // --- Summary ---
    auto SummaryLine = hbox({
        text(" ") | bold,
        text(std::to_string(Tax.mLanes.size()) + " lanes (" + std::to_string(LanesDone) + " done)") | dim,
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

    return vbox({
        LanesPanel,
        JobsPanel,
        TasksPanel,
        SummaryLine,
    });
}

// ---------------------------------------------------------------------------
// SchemaPanel — merged heading list with color coding
// ---------------------------------------------------------------------------

static Element RenderSchemaBlock(const std::string& InTitle, const FWatchDocSchemaResult& InResult)
{
    if (InResult.mDocPath.empty())
    {
        return window(text(" " + InTitle + " ") | bold | dim, text("  (no document)") | dim);
    }

    Elements Rows;
    for (const FWatchHeadingCheck& Check : InResult.mHeadings)
    {
        const std::string Indent = (Check.mLevel == 3) ? " " : "";
        Element Row;

        if (Check.mbPresent && Check.mbCanonical && Check.mbRequired)
        {
            Row = text(Indent + Check.mSectionId) | color(Color::Green);
        }
        else if (Check.mbPresent && Check.mbCanonical && !Check.mbRequired)
        {
            Row = text(Indent + Check.mSectionId) | color(Color::Blue);
        }
        else if (!Check.mbPresent && Check.mbCanonical && !Check.mbRequired)
        {
            Row = text(Indent + Check.mSectionId) | dim;
        }
        else if (Check.mbPresent && !Check.mbCanonical)
        {
            Row = text(Indent + Check.mSectionId) | color(Color::Orange1);
        }
        else if (!Check.mbPresent && Check.mbRequired)
        {
            Row = text(Indent + Check.mSectionId + " (MISSING)") | color(Color::Red);
        }
        else
        {
            continue;
        }

        Rows.push_back(Row);
    }

    std::string Summary = std::to_string(InResult.mRequiredPresent) + "/" + std::to_string(InResult.mRequiredCount) + " required";
    if (InResult.mExtraCount > 0)
    {
        Summary += ", " + std::to_string(InResult.mExtraCount) + " extra";
    }

    return window(text(" " + InTitle + " (" + Summary + ") ") | bold, vbox(std::move(Rows)));
}

Element SchemaPanel::Render(const FWatchPlanSummary& InPlan, int InSelectedPhaseIndex) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" SCHEMA ") | bold | dim, text("  No plan selected") | dim);
    }

    const FWatchTopicSchemaResult& Schema = InPlan.mSchemaResult;

    // Block 1: Plan schema
    auto PlanBlock = RenderSchemaBlock("PLAN", Schema.mPlan);

    // Block 2: Impl schema
    auto ImplBlock = RenderSchemaBlock("IMPL", Schema.mImpl);

    // Block 3: Playbook schema for selected phase
    Element PlaybookBlock;
    if (InSelectedPhaseIndex >= 0 && InSelectedPhaseIndex < static_cast<int>(Schema.mPlaybooks.size()))
    {
        const FWatchDocSchemaResult& PB = Schema.mPlaybooks[static_cast<size_t>(InSelectedPhaseIndex)];
        PlaybookBlock = RenderSchemaBlock("PLAYBOOK " + PB.mPhaseKey, PB);
    }
    else if (!Schema.mPlaybooks.empty())
    {
        // Find playbook matching selected phase by key
        const FWatchDocSchemaResult* rpMatch = nullptr;
        if (InSelectedPhaseIndex >= 0 && InSelectedPhaseIndex < static_cast<int>(InPlan.mPhases.size()))
        {
            const std::string& PhaseKey = InPlan.mPhases[static_cast<size_t>(InSelectedPhaseIndex)].mPhaseKey;
            for (const FWatchDocSchemaResult& PB : Schema.mPlaybooks)
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
            PlaybookBlock = RenderSchemaBlock("PLAYBOOK " + rpMatch->mPhaseKey, *rpMatch);
        }
        else
        {
            PlaybookBlock = window(text(" PLAYBOOK ") | bold | dim, text("  No playbook for selected phase") | dim);
        }
    }
    else
    {
        PlaybookBlock = window(text(" PLAYBOOK ") | bold | dim, text("  No playbooks") | dim);
    }

    // Plan sidecar schemas
    auto PlanCLBlock = RenderSchemaBlock("PLAN CHANGELOG", Schema.mPlanChangeLog);
    auto PlanVerifBlock = RenderSchemaBlock("PLAN VERIFICATION", Schema.mPlanVerification);

    // Impl sidecar schemas
    auto ImplCLBlock = RenderSchemaBlock("IMPL CHANGELOG", Schema.mImplChangeLog);
    auto ImplVerifBlock = RenderSchemaBlock("IMPL VERIFICATION", Schema.mImplVerification);

    // Playbook sidecar schemas for selected phase
    std::string SelectedPhaseKey;
    if (InSelectedPhaseIndex >= 0 && InSelectedPhaseIndex < static_cast<int>(InPlan.mPhases.size()))
    {
        SelectedPhaseKey = InPlan.mPhases[static_cast<size_t>(InSelectedPhaseIndex)].mPhaseKey;
    }

    Element PBCLBlock;
    const FWatchDocSchemaResult* rpPBCL = nullptr;
    for (const FWatchDocSchemaResult& PBCL : Schema.mPlaybookChangeLogs)
    {
        if (PBCL.mPhaseKey == SelectedPhaseKey)
        {
            rpPBCL = &PBCL;
            break;
        }
    }
    if (rpPBCL != nullptr)
    {
        PBCLBlock = RenderSchemaBlock("PB CHANGELOG " + rpPBCL->mPhaseKey, *rpPBCL);
    }
    else
    {
        PBCLBlock = window(text(" PB CHANGELOG ") | bold | dim, text("  No sidecar for phase") | dim);
    }

    Element PBVerifBlock;
    const FWatchDocSchemaResult* rpPBVerif = nullptr;
    for (const FWatchDocSchemaResult& PBV : Schema.mPlaybookVerifications)
    {
        if (PBV.mPhaseKey == SelectedPhaseKey)
        {
            rpPBVerif = &PBV;
            break;
        }
    }
    if (rpPBVerif != nullptr)
    {
        PBVerifBlock = RenderSchemaBlock("PB VERIFICATION " + rpPBVerif->mPhaseKey, *rpPBVerif);
    }
    else
    {
        PBVerifBlock = window(text(" PB VERIFICATION ") | bold | dim, text("  No sidecar for phase") | dim);
    }

    static constexpr int kSchemaColumnWidth = 60;

    auto LeftColumn = vbox({PlanBlock, PlanCLBlock, PlanVerifBlock, ImplBlock, ImplCLBlock, ImplVerifBlock});
    auto RightColumn = vbox({PlaybookBlock, PBCLBlock, PBVerifBlock});

    return hbox({
        LeftColumn | size(WIDTH, EQUAL, kSchemaColumnWidth),
        RightColumn | size(WIDTH, EQUAL, kSchemaColumnWidth),
    });
}

// ---------------------------------------------------------------------------
// Sidecar lookup helper
// ---------------------------------------------------------------------------

static const FWatchSidecarSummary* FindSidecar(const FWatchPlanSummary& InPlan, const std::string& InOwnerKind, const std::string& InDocKind, const std::string& InPhaseKey = "")
{
    for (const FWatchSidecarSummary& Sidecar : InPlan.mSidecarSummaries)
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

static Element RenderSidecarPanel(const std::string& InTitle, const FWatchSidecarSummary* InRpSidecar)
{
    if (InRpSidecar == nullptr)
    {
        return window(text(" " + InTitle + " ") | bold | dim, text("  (not found)") | dim);
    }

    Elements Rows;
    Rows.push_back(text(" Path: " + InRpSidecar->mPath) | dim);
    Rows.push_back(text(" Entries: " + std::to_string(InRpSidecar->mEntryCount)) | color(InRpSidecar->mEntryCount > 0 ? Color::Green : Color::Yellow));
    if (!InRpSidecar->mLatestDate.empty())
    {
        Rows.push_back(text(" Latest: " + InRpSidecar->mLatestDate) | dim);
    }
    return window(text(" " + InTitle + " ") | bold, vbox(std::move(Rows)));
}

// ---------------------------------------------------------------------------
// PlaybookChangeLogPanel
// ---------------------------------------------------------------------------

Element PlaybookChangeLogPanel::Render(const FWatchPlanSummary& InPlan, int InSelectedPhaseIndex) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" PB CHANGELOG ") | bold | dim, text("  No plan selected") | dim);
    }

    std::string PhaseKey;
    if (InSelectedPhaseIndex >= 0 && InSelectedPhaseIndex < static_cast<int>(InPlan.mPhases.size()))
    {
        PhaseKey = InPlan.mPhases[static_cast<size_t>(InSelectedPhaseIndex)].mPhaseKey;
    }

    const FWatchSidecarSummary* rpSidecar = FindSidecar(InPlan, "Playbook", "ChangeLog", PhaseKey);
    return RenderSidecarPanel("PB CHANGELOG " + PhaseKey, rpSidecar);
}

// ---------------------------------------------------------------------------
// PlaybookVerificationPanel
// ---------------------------------------------------------------------------

Element PlaybookVerificationPanel::Render(const FWatchPlanSummary& InPlan, int InSelectedPhaseIndex) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" PB VERIFICATION ") | bold | dim, text("  No plan selected") | dim);
    }

    std::string PhaseKey;
    if (InSelectedPhaseIndex >= 0 && InSelectedPhaseIndex < static_cast<int>(InPlan.mPhases.size()))
    {
        PhaseKey = InPlan.mPhases[static_cast<size_t>(InSelectedPhaseIndex)].mPhaseKey;
    }

    const FWatchSidecarSummary* rpSidecar = FindSidecar(InPlan, "Playbook", "Verification", PhaseKey);
    return RenderSidecarPanel("PB VERIFICATION " + PhaseKey, rpSidecar);
}

// ---------------------------------------------------------------------------
// PlanChangeLogPanel
// ---------------------------------------------------------------------------

Element PlanChangeLogPanel::Render(const FWatchPlanSummary& InPlan) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" PLAN CHANGELOG ") | bold | dim, text("  No plan selected") | dim);
    }

    const FWatchSidecarSummary* rpSidecar = FindSidecar(InPlan, "Plan", "ChangeLog");
    return RenderSidecarPanel("PLAN CHANGELOG", rpSidecar);
}

// ---------------------------------------------------------------------------
// PlanVerificationPanel
// ---------------------------------------------------------------------------

Element PlanVerificationPanel::Render(const FWatchPlanSummary& InPlan) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" PLAN VERIFICATION ") | bold | dim, text("  No plan selected") | dim);
    }

    const FWatchSidecarSummary* rpSidecar = FindSidecar(InPlan, "Plan", "Verification");
    return RenderSidecarPanel("PLAN VERIFICATION", rpSidecar);
}

// ---------------------------------------------------------------------------
// ImplChangeLogPanel
// ---------------------------------------------------------------------------

Element ImplChangeLogPanel::Render(const FWatchPlanSummary& InPlan) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" IMPL CHANGELOG ") | bold | dim, text("  No plan selected") | dim);
    }

    const FWatchSidecarSummary* rpSidecar = FindSidecar(InPlan, "Impl", "ChangeLog");
    return RenderSidecarPanel("IMPL CHANGELOG", rpSidecar);
}

// ---------------------------------------------------------------------------
// ImplVerificationPanel
// ---------------------------------------------------------------------------

Element ImplVerificationPanel::Render(const FWatchPlanSummary& InPlan) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" IMPL VERIFICATION ") | bold | dim, text("  No plan selected") | dim);
    }

    const FWatchSidecarSummary* rpSidecar = FindSidecar(InPlan, "Impl", "Verification");
    return RenderSidecarPanel("IMPL VERIFICATION", rpSidecar);
}

// ---------------------------------------------------------------------------
// FileManifestPanel
// ---------------------------------------------------------------------------

Element FileManifestPanel::Render(const FWatchPhaseTaxonomy& InTaxonomy, int InFilePageIndex) const
{
    if (InTaxonomy.mFileManifest.empty())
    {
        return window(text(" FILES: " + InTaxonomy.mPhaseKey + " (0) ") | bold, text("  (no file changes planned)") | dim);
    }

    static constexpr int kPageSize = 10;
    const int TotalFiles = static_cast<int>(InTaxonomy.mFileManifest.size());
    const int PageStart = InFilePageIndex * kPageSize;
    const int PageEnd = std::min(PageStart + kPageSize, TotalFiles);

    std::vector<std::vector<Element>> TableData;
    TableData.push_back({text("File") | bold | flex, text("Action") | bold, text("Description") | bold | flex});

    for (int Index = PageStart; Index < PageEnd; ++Index)
    {
        const FWatchFileManifestItem& Item = InTaxonomy.mFileManifest[static_cast<size_t>(Index)];
        auto ActionColor = Color::White;
        if (Item.mAction == "create")
        {
            ActionColor = Color::Green;
        }
        else if (Item.mAction == "modify")
        {
            ActionColor = Color::Yellow;
        }
        else if (Item.mAction == "remove")
        {
            ActionColor = Color::Red;
        }
        TableData.push_back({
            text(Item.mFilePath) | flex,
            text(Item.mAction) | color(ActionColor),
            text(Item.mDescription) | dim | flex,
        });
    }

    auto FileTable = Table(std::move(TableData));
    FileTable.SelectAll().SeparatorVertical(EMPTY);
    FileTable.SelectRow(0).SeparatorHorizontal(LIGHT);

    std::string Title = " FILES: " + InTaxonomy.mPhaseKey + " (" + std::to_string(TotalFiles) + ")";
    if (TotalFiles > kPageSize)
    {
        const int PageCount = (TotalFiles + kPageSize - 1) / kPageSize;
        Title += " [" + std::to_string(InFilePageIndex + 1) + "/" + std::to_string(PageCount) + "]";
    }
    Title += " ";

    return window(text(Title) | bold, FileTable.Render() | flex);
}

// ---------------------------------------------------------------------------
// WatchStatusBar
// ---------------------------------------------------------------------------

Element WatchStatusBar::Render(const std::string& InVersion, const std::string& InTime, int InTick, int InPollMs, const FWatchInventoryCounters& InCounters) const
{
    return hbox({
        text(" Poll #" + std::to_string(InTick)) | bold,
        text("  |  Last: " + std::to_string(InPollMs) + "ms") | dim,
        text("  |  Plans: " + std::to_string(InCounters.mPlanCount) + "  Playbooks: " + std::to_string(InCounters.mPlaybookCount) + "  Impls: " + std::to_string(InCounters.mImplementationCount)) | dim,
        filler(),
        text("q=quit  a/A=plan  n/N=non-active  p/P=phase  w/W=wave  l/L=lane  f/F=files  s=schema  i=impl  r=refresh") | dim,
        text("  "),
    });
}

} // namespace UniPlan
