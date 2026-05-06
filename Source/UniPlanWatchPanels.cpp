#include "UniPlanWatchPanels.h"
#include "UniPlanHelpers.h"
#include "UniPlanTopicTypes.h" // kPhaseHollowChars, ComputePhaseDesignChars
#include "UniPlanTypes.h"
#include "UniPlanWatchScroll.h"
#include "UniPlanWatchScreenDraw.h"

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/dom/requirement.hpp>
#include <ftxui/dom/table.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/string.hpp>
#include <ftxui/util/autoreset.hpp>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace UniPlan
{

using namespace ftxui;

void FWatchProseLayoutCache::Reset()
{
    mSnapshotGeneration = 0;
    mTopicKey.clear();
    mPhaseIndex = -1;
    mAvailableTextCells = -1;
    mLineCount = 0;
    mGutterWidth = 0;
    mLogicalBuildCount = 0;
    mVisualBuildCount = 0;
    mbLogicalValid = false;
    mLines.clear();
    mRows.clear();
}

void FWatchPhaseListLayoutCache::Reset()
{
    mSnapshotGeneration = 0;
    mTopicKey.clear();
    mbMetricView = false;
    mbValid = false;
    mBuildCount = 0;
    mPhaseColumnWidth = 0;
    mDefaultDesignColumnWidth = 0;
    mTaxonomyColumnWidth = 0;
    mMetricDesignColumnWidth = 0;
    mBloatColumnWidth = 0;
    mSolidColumnWidth = 0;
    mWordsColumnWidth = 0;
    mFieldsColumnWidth = 0;
    mWorkColumnWidth = 0;
    mTestsColumnWidth = 0;
    mFilesColumnWidth = 0;
    mEvidenceColumnWidth = 0;
    mDesignScale = FWatchMetricGaugeScale{};
    mSolidScale = FWatchMetricGaugeScale{};
    mWordsScale = FWatchMetricGaugeScale{};
    mFieldsScale = FWatchMetricGaugeScale{};
    mWorkScale = FWatchMetricGaugeScale{};
    mTestsScale = FWatchMetricGaugeScale{};
    mFilesScale = FWatchMetricGaugeScale{};
    mEvidenceScale = FWatchMetricGaugeScale{};
    mRows.clear();
}

void FWatchCodeSnippetLayoutCache::Reset()
{
    mSnapshotGeneration = 0;
    mTopicKey.clear();
    mPhaseIndex = -1;
    mAvailableTextCells = -1;
    mLineCount = 0;
    mGutterWidth = 0;
    mLogicalBuildCount = 0;
    mVisualBuildCount = 0;
    mbLogicalValid = false;
    mLines.clear();
    mRows.clear();
}

void FWatchFileManifestLayoutCache::Reset()
{
    mSnapshotGeneration = 0;
    mTopicKey.clear();
    mPhaseIndex = -1;
    mAvailableDescriptionCells = -1;
    mBuildCount = 0;
    mVisualBuildCount = 0;
    mFileColumnWidth = 0;
    mActionColumnWidth = 0;
    mbValid = false;
    mRows.clear();
    mVisualRows.clear();
}

void FWatchExecutionTaxonomyLayoutCache::Reset()
{
    mSnapshotGeneration = 0;
    mTopicKey.clear();
    mPhaseIndex = -1;
    mSelectedWaveIndex = -1;
    mSelectedLaneIndex = -1;
    mWaveFilter = -1;
    mLaneFilter = -1;
    mLanesDone = 0;
    mJobsDone = 0;
    mJobsActive = 0;
    mJobsTodo = 0;
    mTasksDone = 0;
    mTasksActive = 0;
    mTasksTodo = 0;
    mVisibleJobCount = 0;
    mVisibleTaskCount = 0;
    mBuildCount = 0;
    mbValid = false;
    mbHasFlatTasks = false;
    mLanes.clear();
    mJobs.clear();
    mTasks.clear();
}

void FWatchPanelRenderCache::Reset()
{
    mPhaseDetails.Reset();
    mPhaseList.Reset();
    mCodeSnippets.Reset();
    mFileManifest.Reset();
    mExecutionTaxonomy.Reset();
}

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
// exceeds the final column box clips at the boundary (same behavior
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

static constexpr int kDesignBarPreferredWidth = 10;
static constexpr int kMetricBarPreferredWidth = 8;

static int TextCellWidth(const std::string &InText)
{
    return static_cast<int>(InText.size());
}

static int ComputeTextColumnWidth(const std::string &InHeader,
                                  const std::vector<std::string> &InLabels,
                                  const int InMinimumWidth)
{
    int Width = std::max(InMinimumWidth, TextCellWidth(InHeader));
    for (const std::string &Label : InLabels)
    {
        Width = std::max(Width, TextCellWidth(Label));
    }
    return Width;
}

static int ComputeGaugeColumnWidth(const std::string &InHeader,
                                   const std::vector<std::string> &InLabels,
                                   const int InMinimumWidth,
                                   const int InPreferredBarWidth)
{
    int Width = std::max(InMinimumWidth, TextCellWidth(InHeader));
    for (const std::string &Label : InLabels)
    {
        const int LabelWidth = TextCellWidth(Label);
        const int FullGaugeWidth =
            LabelWidth + ((InPreferredBarWidth > 0) ? InPreferredBarWidth + 1
                                                    : 0);
        Width = std::max(Width, FullGaugeWidth);
    }
    return Width;
}

static Element ValueGaugeBar(const size_t InValue, const size_t InHollow,
                             const size_t InRich, const size_t InFillMax,
                             const std::string &InLabel,
                             const int InPreferredBarWidth,
                             const int InCellWidth, const bool bCeilFill)
{
    if (InLabel.empty())
    {
        return text("");
    }

    int BarWidth = std::max(0, InPreferredBarWidth);
    if (InCellWidth > 0)
    {
        const int AvailableForBar =
            InCellWidth - TextCellWidth(InLabel) - 1;
        BarWidth = std::min(BarWidth, std::max(0, AvailableForBar));
    }

    // Numeric labels are authoritative data; the colored bar is context.
    // When a terminal or column is narrow, keep the number and shrink the bar.
    if (BarWidth <= 0)
    {
        return text(InLabel);
    }

    const size_t Denominator = std::max<size_t>(1, InFillMax);
    const size_t ClampedValue = std::min(InValue, Denominator);
    int Filled = 0;
    if (bCeilFill)
    {
        Filled = static_cast<int>((ClampedValue * BarWidth + Denominator - 1) /
                                  Denominator);
    }
    else
    {
        Filled = static_cast<int>((ClampedValue * BarWidth) / Denominator);
    }
    if (InValue > 0 && Filled == 0)
    {
        Filled = 1;
    }
    Filled = std::min(BarWidth, std::max(0, Filled));
    const int Empty = BarWidth - Filled;

    const Color BarColor = (InValue < InHollow) ? Color::Red
                           : (InValue < InRich) ? Color::Yellow
                                                : Color::Green;
    Elements Bar;
    for (int Index = 0; Index < Filled; ++Index)
    {
        Bar.push_back(text("\xe2\x96\x88") | color(BarColor));
    }
    for (int Index = 0; Index < Empty; ++Index)
    {
        Bar.push_back(text("\xe2\x96\x91") | dim);
    }
    Bar.push_back(text(" " + InLabel));
    return hbox(std::move(Bar));
}

// Fixed-width gauge so plans with different phase counts render at the
// same visual length, enabling cross-row completion comparison in the
// ACTIVE PLANS panel. Width matches the Design gauge's preferred width.
static Element PhaseProgressBar(int InDone, int InTotal)
{
    if (InTotal == 0)
    {
        return text("no phases") | dim;
    }
    static constexpr int kPhaseProgressBarWidth = kDesignBarPreferredWidth;
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

// Render the V4 design-char count as the PHASE LIST `Design` column.
// Thresholds come from UniPlanTopicTypes.h (`kPhaseHollowChars` = 3000,
// `kPhaseRichMinChars` = 10000 as of v0.83.0, calibrated against V4
// schema semantics — see that header for the derivation).
// Color coding:
//   • < kPhaseHollowChars   → dim red ("hollow", phase needs more authoring)
//   • [hollow, rich)        → yellow  ("thin", executable but sparse)
//   • ≥ kPhaseRichMinChars  → green   ("rich", properly detailed)
// Empty (= 0) renders as "-" to distinguish "no content" from
// "some content but short".
static Element DesignCharsBar(const size_t InChars, const int InCellWidth)
{
    if (InChars == 0)
    {
        return text("-") | dim;
    }
    return ValueGaugeBar(InChars, kPhaseHollowChars, kPhaseRichMinChars,
                         kPhaseRichMinChars, std::to_string(InChars),
                         kDesignBarPreferredWidth, InCellWidth, true);
}

static Element MetricGaugeBar(const size_t InValue, const size_t InHollow,
                              const size_t InRich, const size_t InFillMax,
                              const std::string &InLabel,
                              const int InCellWidth)
{
    return ValueGaugeBar(InValue, InHollow, InRich, InFillMax, InLabel,
                         kMetricBarPreferredWidth, InCellWidth, false);
}

static FWatchMetricGaugeScale
BuildMetricGaugeScale(const std::vector<PhaseItem> &InPhases,
                      const size_t InHollow, const size_t InRich,
                      size_t (*InReadValue)(const FPhaseRuntimeMetrics &))
{
    FWatchMetricGaugeScale Scale;
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
                                   const FWatchMetricGaugeScale &InScale)
{
    if (!InScale.mbUseRelativeRichRange)
    {
        return std::max(InScale.mRich, InScale.mMaxValue);
    }

    // When every visible phase already meets the absolute rich threshold,
    // the gauge becomes a relative intensity signal. Keep the weakest rich
    // row half-full so green rows still read as healthy while the stronger
    // rows remain visually distinguishable.
    static constexpr size_t kRelativeRichBaseFill =
        kMetricBarPreferredWidth / 2;
    const size_t Range = InScale.mMaxValue - InScale.mMinValue;
    const size_t ExtraWidth = kMetricBarPreferredWidth - kRelativeRichBaseFill;
    const size_t RelativeFill =
        kRelativeRichBaseFill +
        ((InValue - InScale.mMinValue) * ExtraWidth) / Range;
    const size_t ClampedFill =
        std::min(static_cast<size_t>(kMetricBarPreferredWidth),
                 std::max<size_t>(1, RelativeFill));
    return std::max<size_t>(
        1, (InValue * kMetricBarPreferredWidth) / ClampedFill);
}

static Element MetricGaugeBar(const size_t InValue,
                              const FWatchMetricGaugeScale &InScale,
                              const std::string &InLabel,
                              const int InCellWidth)
{
    return MetricGaugeBar(InValue, InScale.mHollow, InScale.mRich,
                          ResolveMetricFillMax(InValue, InScale), InLabel,
                          InCellWidth);
}

static std::string BuildBloatMetricLabel(const FPhaseRuntimeMetrics &InMetrics)
{
    return "m" + std::to_string(InMetrics.mLargestDesignFieldChars) + " r" +
           std::to_string(InMetrics.mRepeatedDesignBlockCount) + " " +
           std::to_string(InMetrics.mDesignBloatRatio) + "%";
}

static Element BloatMetricText(const FPhaseRuntimeMetrics &InMetrics)
{
    const std::string Label = BuildBloatMetricLabel(InMetrics);
    if (InMetrics.mRepeatedDesignBlockCount > 0 ||
        InMetrics.mDesignBloatRatio > 150)
    {
        return text(Label) | color(Color::Red);
    }
    if (InMetrics.mDesignBloatRatio > 100)
    {
        return text(Label) | color(Color::Yellow);
    }
    return text(Label) | color(Color::Green);
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

static std::string BuildPhaseTaxonomySummary(
    const std::vector<FPhaseTaxonomy> &InTaxonomies, const int InPhaseIndex)
{
    for (const FPhaseTaxonomy &Tax : InTaxonomies)
    {
        if (Tax.mPhaseIndex != InPhaseIndex)
        {
            continue;
        }

        int LaneDone = 0;
        int LaneActive = 0;
        int LaneTodo = 0;
        for (const FLaneRecord &Lane : Tax.mLanes)
        {
            if (Lane.mStatus == EExecutionStatus::Completed)
            {
                ++LaneDone;
            }
            else if (Lane.mStatus == EExecutionStatus::InProgress)
            {
                ++LaneActive;
            }
            else
            {
                ++LaneTodo;
            }
        }

        int JobDone = 0;
        int JobActive = 0;
        int JobTodo = 0;
        for (const FJobRecord &Job : Tax.mJobs)
        {
            if (Job.mStatus == EExecutionStatus::Completed)
            {
                ++JobDone;
            }
            else if (Job.mStatus == EExecutionStatus::InProgress)
            {
                ++JobActive;
            }
            else
            {
                ++JobTodo;
            }
        }

        int TaskDone = 0;
        int TaskActive = 0;
        int TaskTodo = 0;
        for (const FTaskRecord &Task : Tax.mTasks)
        {
            if (Task.mStatus == EExecutionStatus::Completed)
            {
                ++TaskDone;
            }
            else if (Task.mStatus == EExecutionStatus::InProgress)
            {
                ++TaskActive;
            }
            else
            {
                ++TaskTodo;
            }
        }

        std::string Summary;
        if (!Tax.mLanes.empty())
        {
            Summary += std::to_string(Tax.mLanes.size()) + "L:" +
                       std::to_string(LaneDone) + "d " +
                       std::to_string(LaneActive) + "a " +
                       std::to_string(LaneTodo) + "t";
        }
        if (!Tax.mJobs.empty())
        {
            if (!Summary.empty())
            {
                Summary += " ";
            }
            Summary += std::to_string(Tax.mJobs.size()) + "J:" +
                       std::to_string(JobDone) + "d " +
                       std::to_string(JobActive) + "a " +
                       std::to_string(JobTodo) + "t";
        }
        if (!Tax.mTasks.empty())
        {
            if (!Summary.empty())
            {
                Summary += " ";
            }
            Summary += std::to_string(Tax.mTasks.size()) + "T:" +
                       std::to_string(TaskDone) + "d " +
                       std::to_string(TaskActive) + "a " +
                       std::to_string(TaskTodo) + "t";
        }
        return Summary.empty() ? "-" : Summary;
    }

    return "-";
}

struct FWatchWrapToken
{
    std::string mText;
    int mCells = 0;
    bool mbWhitespace = false;
};

static void AddWatchProseLine(std::vector<FWatchProseLogicalLine> &OutLines,
                              std::string InText,
                              Color InColor = Color::Default,
                              const bool bDim = false,
                              const bool bBoldLine = false,
                              std::string InPrefix = "",
                              const bool bNumbered = true,
                              const Color InBackground = Color::Default)
{
    FWatchProseLogicalLine Line;
    Line.mText = std::move(InText);
    Line.mPrefix = std::move(InPrefix);
    Line.mForeground = InColor;
    Line.mBackground = InBackground;
    Line.mbDim = bDim;
    Line.mbBold = bBoldLine;
    Line.mbNumbered = bNumbered;
    OutLines.push_back(std::move(Line));
}

static void AddWatchProseSeparator(
    std::vector<FWatchProseLogicalLine> &OutLines)
{
    FWatchProseLogicalLine Line;
    Line.mbNumbered = false;
    Line.mbSeparator = true;
    OutLines.push_back(std::move(Line));
}

static void AddWatchProseGap(std::vector<FWatchProseLogicalLine> &OutLines)
{
    AddWatchProseLine(OutLines, "", Color::Default, false, false, "", false);
}

static int CountNumberedProseLines(
    const std::vector<FWatchProseLogicalLine> &InLines)
{
    int Count = 0;
    for (const FWatchProseLogicalLine &Line : InLines)
    {
        if (Line.mbNumbered)
        {
            ++Count;
        }
    }
    return Count;
}

static int DecimalDigitCount(const int InValue)
{
    int Value = std::max(1, InValue);
    int Count = 0;
    do
    {
        ++Count;
        Value /= 10;
    } while (Value > 0);
    return Count;
}

static std::string ExpandTabsForDisplay(const std::string &InLine)
{
    std::string Result;
    int Column = 0;
    for (const char Character : InLine)
    {
        if (Character == '\t')
        {
            const int Spaces = 4 - (Column % 4);
            Result.append(static_cast<size_t>(Spaces), ' ');
            Column += Spaces;
            continue;
        }
        Result.push_back(Character);
        ++Column;
    }
    return Result;
}

static bool IsDisplayWhitespaceGlyph(const std::string &InGlyph)
{
    return InGlyph == " ";
}

static std::vector<FWatchWrapToken>
BuildWatchWrapTokens(const std::string &InText)
{
    std::vector<FWatchWrapToken> Tokens;
    const std::vector<std::string> Glyphs = Utf8ToGlyphs(InText);
    for (const std::string &Glyph : Glyphs)
    {
        const bool bWhitespace = IsDisplayWhitespaceGlyph(Glyph);
        if (Tokens.empty() || Tokens.back().mbWhitespace != bWhitespace)
        {
            FWatchWrapToken Token;
            Token.mbWhitespace = bWhitespace;
            Tokens.push_back(std::move(Token));
        }
        Tokens.back().mText += Glyph;
        ++Tokens.back().mCells;
    }
    return Tokens;
}

static void TrimRightDisplaySpaces(std::string &InOutText)
{
    while (!InOutText.empty() && InOutText.back() == ' ')
    {
        InOutText.pop_back();
    }
}

static void PushWrappedDisplayRow(std::vector<std::string> &OutRows,
                                  std::string &InOutCurrent,
                                  int &InOutCurrentCells,
                                  bool &InOutCurrentHasText)
{
    TrimRightDisplaySpaces(InOutCurrent);
    if (!InOutCurrent.empty() || OutRows.empty())
    {
        OutRows.push_back(InOutCurrent);
    }
    InOutCurrent.clear();
    InOutCurrentCells = 0;
    InOutCurrentHasText = false;
}

static std::vector<std::string> WrapTextLineForDisplay(
    const std::string &InLine, const int InAvailableCells)
{
    std::vector<std::string> Wrapped;
    if (InAvailableCells <= 0)
    {
        Wrapped.push_back("");
        return Wrapped;
    }

    const std::string Expanded = ExpandTabsForDisplay(InLine);
    const std::vector<FWatchWrapToken> Tokens = BuildWatchWrapTokens(Expanded);
    if (Tokens.empty())
    {
        Wrapped.push_back("");
        return Wrapped;
    }

    std::string Current;
    int CurrentCells = 0;
    bool bCurrentHasText = false;
    for (const FWatchWrapToken &Token : Tokens)
    {
        if (Token.mbWhitespace)
        {
            if (Current.empty() && !Wrapped.empty())
            {
                continue;
            }
            Current += Token.mText;
            CurrentCells += Token.mCells;
            continue;
        }

        if (CurrentCells > 0 &&
            CurrentCells + Token.mCells > InAvailableCells &&
            bCurrentHasText)
        {
            PushWrappedDisplayRow(Wrapped, Current, CurrentCells,
                                  bCurrentHasText);
        }

        Current += Token.mText;
        CurrentCells += Token.mCells;
        bCurrentHasText = true;

        if (Token.mCells > InAvailableCells)
        {
            PushWrappedDisplayRow(Wrapped, Current, CurrentCells,
                                  bCurrentHasText);
        }
    }

    TrimRightDisplaySpaces(Current);
    if (!Current.empty() || Wrapped.empty())
    {
        Wrapped.push_back(Current);
    }
    return Wrapped;
}

static std::string BuildLineNumberGutter(const int InLineNumber,
                                         const int InGutterWidth,
                                         const bool bContinuation)
{
    if (InGutterWidth <= 0)
    {
        return "";
    }
    if (bContinuation || InLineNumber <= 0)
    {
        return std::string(static_cast<size_t>(InGutterWidth), ' ');
    }

    const std::string Number = std::to_string(InLineNumber);
    const int Padding =
        std::max(0, InGutterWidth - 1 - static_cast<int>(Number.size()));
    return std::string(static_cast<size_t>(Padding), ' ') + Number + " ";
}

static std::vector<std::string>
SplitTextLinesForDisplay(const std::string &InText)
{
    std::vector<std::string> Lines;
    std::string Current;
    for (const char Character : InText)
    {
        if (Character == '\r')
        {
            continue;
        }
        if (Character == '\n')
        {
            Lines.push_back(Current);
            Current.clear();
            continue;
        }
        Current.push_back(Character);
    }
    Lines.push_back(Current);
    return Lines;
}

static std::vector<std::string>
SplitCodeSnippetLines(const std::string &InSnippets)
{
    return SplitTextLinesForDisplay(InSnippets);
}

static std::string TrimASCIIWhitespace(const std::string &InText)
{
    size_t Begin = 0;
    while (Begin < InText.size() &&
           std::isspace(static_cast<unsigned char>(InText[Begin])) != 0)
    {
        ++Begin;
    }

    size_t End = InText.size();
    while (End > Begin &&
           std::isspace(static_cast<unsigned char>(InText[End - 1])) != 0)
    {
        --End;
    }

    return InText.substr(Begin, End - Begin);
}

static std::string ToLowerASCII(std::string InText)
{
    std::transform(
        InText.begin(), InText.end(), InText.begin(),
        [](const unsigned char InCharacter)
        { return static_cast<char>(std::tolower(InCharacter)); });
    return InText;
}

static bool IsFenceLine(const std::string &InLine)
{
    const std::string Trimmed = TrimASCIIWhitespace(InLine);
    return Trimmed.size() >= 3 && Trimmed.compare(0, 3, "```") == 0;
}

static bool IsCppFenceOpeningLine(const std::string &InLine)
{
    const std::string Trimmed = TrimASCIIWhitespace(InLine);
    if (Trimmed.size() < 6 || Trimmed.compare(0, 3, "```") != 0)
    {
        return false;
    }

    std::string Info = TrimASCIIWhitespace(Trimmed.substr(3));
    const size_t FirstSpace = Info.find_first_of(" \t");
    if (FirstSpace != std::string::npos)
    {
        Info = Info.substr(0, FirstSpace);
    }
    Info = ToLowerASCII(Info);
    return Info == "cpp" || Info == "c++";
}

static bool IsTaggedFenceOpeningLine(const std::string &InLine)
{
    const std::string Trimmed = TrimASCIIWhitespace(InLine);
    if (Trimmed.size() < 3 || Trimmed.compare(0, 3, "```") != 0)
    {
        return false;
    }

    size_t FenceEnd = 0;
    while (FenceEnd < Trimmed.size() && Trimmed[FenceEnd] == '`')
    {
        ++FenceEnd;
    }
    if (FenceEnd < 3)
    {
        return false;
    }

    const std::string Info = TrimASCIIWhitespace(Trimmed.substr(FenceEnd));
    return !Info.empty();
}

static std::vector<FCodeSnippetLogicalLine>
StyleCodeSnippetLines(const std::vector<std::string> &InLines)
{
    bool bHasCppFence = false;
    for (const std::string &Line : InLines)
    {
        if (IsCppFenceOpeningLine(Line))
        {
            bHasCppFence = true;
            break;
        }
    }

    std::vector<FCodeSnippetLogicalLine> Styled;
    bool bInsideCppFence = false;
    for (const std::string &Line : InLines)
    {
        FCodeSnippetLogicalLine StyledLine;
        StyledLine.mText = Line;

        if (!bHasCppFence)
        {
            StyledLine.mbCode = true;
            Styled.push_back(std::move(StyledLine));
            continue;
        }

        if (!bInsideCppFence && IsCppFenceOpeningLine(Line))
        {
            StyledLine.mbCode = false;
            bInsideCppFence = true;
            Styled.push_back(std::move(StyledLine));
            continue;
        }

        if (bInsideCppFence && IsFenceLine(Line))
        {
            StyledLine.mbCode = false;
            bInsideCppFence = false;
            Styled.push_back(std::move(StyledLine));
            continue;
        }

        StyledLine.mbCode = bInsideCppFence;
        Styled.push_back(std::move(StyledLine));
    }

    return Styled;
}

static std::vector<FCodeSnippetVisualRow> BuildCodeSnippetVisualRows(
    const std::vector<FCodeSnippetLogicalLine> &InLines,
    const int InAvailableCells)
{
    std::vector<FCodeSnippetVisualRow> Rows;
    for (size_t LineIndex = 0; LineIndex < InLines.size(); ++LineIndex)
    {
        const std::vector<std::string> Wrapped =
            WrapTextLineForDisplay(InLines[LineIndex].mText, InAvailableCells);
        for (size_t WrapIndex = 0; WrapIndex < Wrapped.size(); ++WrapIndex)
        {
            FCodeSnippetVisualRow Row;
            Row.mLineNumber = static_cast<int>(LineIndex) + 1;
            Row.mbContinuation = WrapIndex > 0;
            Row.mbCode = InLines[LineIndex].mbCode;
            Row.mText = Wrapped[WrapIndex];
            Rows.push_back(std::move(Row));
        }
    }
    return Rows;
}

static std::vector<FWatchProseVisualRow> BuildProseVisualRows(
    const std::vector<FWatchProseLogicalLine> &InLines,
    const int InAvailableCells)
{
    std::vector<FWatchProseVisualRow> Rows;
    int LineNumber = 0;
    for (const FWatchProseLogicalLine &Line : InLines)
    {
        if (Line.mbSeparator)
        {
            FWatchProseVisualRow Row;
            Row.mbSeparator = true;
            Row.mForeground = Color::GrayDark;
            Row.mbDim = true;
            Rows.push_back(std::move(Row));
            continue;
        }

        int DisplayLineNumber = 0;
        if (Line.mbNumbered)
        {
            ++LineNumber;
            DisplayLineNumber = LineNumber;
        }

        const std::string DisplayText = Line.mPrefix + Line.mText;
        const std::vector<std::string> Wrapped =
            WrapTextLineForDisplay(DisplayText, InAvailableCells);
        for (size_t WrapIndex = 0; WrapIndex < Wrapped.size(); ++WrapIndex)
        {
            FWatchProseVisualRow Row;
            Row.mLineNumber = DisplayLineNumber;
            Row.mbContinuation = WrapIndex > 0;
            Row.mText = Wrapped[WrapIndex];
            Row.mForeground = Line.mForeground;
            Row.mBackground = Line.mBackground;
            Row.mbDim = Line.mbDim;
            Row.mbBold = Line.mbBold;
            Rows.push_back(std::move(Row));
        }
    }
    return Rows;
}

static Color PhaseDetailsCodeBackground()
{
    return Color(static_cast<uint8_t>(0), static_cast<uint8_t>(24),
                 static_cast<uint8_t>(40));
}

static void DrawScreenText(Screen &InScreen, const int InX, const int InY,
                           const std::string &InText,
                           const Color InForeground, const bool bDim,
                           const bool bBold = false,
                           const Color InBackground = Color::Default)
{
    DrawWatchScreenText(
        InScreen, InX, InY, InText,
        FWatchScreenTextStyle{InForeground, InBackground, bDim, bBold,
                              false});
}

class WatchProseBlockNode : public Node
{
  public:
    explicit WatchProseBlockNode(std::vector<FWatchProseLogicalLine> InLines,
                                 FWatchScrollRegionState *rpInScrollState =
                                     nullptr)
        : mLines(std::move(InLines)), rpScrollState(rpInScrollState)
    {
    }

    WatchProseBlockNode(FWatchProseLayoutCache &InOutLayoutCache,
                        FWatchScrollRegionState &InOutScrollState)
        : rpLayoutCache(&InOutLayoutCache), rpScrollState(&InOutScrollState)
    {
    }

    void ComputeRequirement() override
    {
        const int LogicalHeight = rpLayoutCache == nullptr
                                      ? static_cast<int>(mLines.size())
                                      : rpLayoutCache->mLineCount;
        requirement_.min_x = 0;
        requirement_.min_y = rpScrollState == nullptr ? LogicalHeight : 0;
        requirement_.flex_grow_x = 1;
        requirement_.flex_shrink_x = 1;
        requirement_.flex_grow_y = 1;
        requirement_.flex_shrink_y = 1;
    }

    void SetBox(Box InBox) override
    {
        Node::SetBox(InBox);
        const int Width = std::max(0, InBox.x_max - InBox.x_min + 1);
        mHeight = std::max(0, InBox.y_max - InBox.y_min + 1);

        const int NumberedCount =
            rpLayoutCache == nullptr ? CountNumberedProseLines(mLines)
                                     : rpLayoutCache->mLineCount;

        mGutterWidth = NumberedCount > 0 ? DecimalDigitCount(NumberedCount) + 1
                                         : 0;
        mTextX = InBox.x_min + mGutterWidth;
        const int TextWidth = std::max(1, Width - mGutterWidth);
        if (rpLayoutCache == nullptr)
        {
            mRows = BuildProseVisualRows(mLines, TextWidth);
        }
        else if (rpLayoutCache->mAvailableTextCells != TextWidth)
        {
            rpLayoutCache->mRows =
                BuildProseVisualRows(rpLayoutCache->mLines, TextWidth);
            rpLayoutCache->mAvailableTextCells = TextWidth;
            rpLayoutCache->mGutterWidth = mGutterWidth;
            ++rpLayoutCache->mVisualBuildCount;
        }
        const std::vector<FWatchProseVisualRow> &Rows = GetRows();
        if (rpScrollState != nullptr)
        {
            const int ContentHeight = static_cast<int>(Rows.size());
            mHasScrollIndicators = ContentHeight > mHeight;
            mViewportHeight =
                mHasScrollIndicators ? std::max(0, mHeight - 2) : mHeight;
            const int MaxOffset =
                mViewportHeight > 0
                    ? std::max(0, ContentHeight - mViewportHeight)
                    : 0;
            rpScrollState->mOffset =
                std::clamp(rpScrollState->mOffset, 0, MaxOffset);
            rpScrollState->mMaxOffset = MaxOffset;
        }
    }

    void Render(Screen &InScreen) override
    {
        if (mHeight <= 0)
        {
            return;
        }

        const AutoReset<Box> Stencil(
            &InScreen.stencil, Box::Intersection(box_, InScreen.stencil));

        if (rpScrollState == nullptr)
        {
            const std::vector<FWatchProseVisualRow> &Rows = GetRows();
            const int EndRow =
                std::min(static_cast<int>(Rows.size()), mHeight);
            for (int RowIndex = 0; RowIndex < EndRow; ++RowIndex)
            {
                DrawProseRow(InScreen, Rows[static_cast<size_t>(RowIndex)],
                             box_.y_min + RowIndex);
            }
            return;
        }

        int Y = box_.y_min;
        if (mHasScrollIndicators)
        {
            if (rpScrollState->mOffset > 0)
            {
                DrawScreenText(InScreen, box_.x_min, Y,
                               "\xe2\x86\x91 " +
                                   std::to_string(rpScrollState->mOffset) +
                                   " above",
                               Color::GrayDark, true);
            }
            ++Y;
        }

        const std::vector<FWatchProseVisualRow> &Rows = GetRows();
        const int EndRow =
            std::min(static_cast<int>(Rows.size()),
                     rpScrollState->mOffset + std::max(0, mViewportHeight));
        for (int RowIndex = rpScrollState->mOffset; RowIndex < EndRow;
             ++RowIndex)
        {
            DrawProseRow(InScreen, Rows[static_cast<size_t>(RowIndex)], Y);
            ++Y;
        }

        if (mHasScrollIndicators)
        {
            const int Below =
                std::max(0,
                         rpScrollState->mMaxOffset - rpScrollState->mOffset);
            if (Below > 0)
            {
                DrawScreenText(InScreen, box_.x_min, box_.y_max,
                               "\xe2\x86\x93 " + std::to_string(Below) +
                                   " below",
                               Color::GrayDark, true);
            }
        }
    }

  private:
    const std::vector<FWatchProseVisualRow> &GetRows() const
    {
        return rpLayoutCache == nullptr ? mRows : rpLayoutCache->mRows;
    }

    void DrawProseRow(Screen &InScreen, const FWatchProseVisualRow &InRow,
                      const int InY) const
    {
        if (InY < box_.y_min || InY > box_.y_max)
        {
            return;
        }

        if (InRow.mbSeparator)
        {
            DrawHorizontalRule(InScreen, InY);
            return;
        }

        const std::string Gutter = BuildLineNumberGutter(
            InRow.mLineNumber, mGutterWidth, InRow.mbContinuation);
        if (InRow.mBackground != Color::Default)
        {
            FillWatchScreenBackground(InScreen, mTextX, box_.x_max, InY,
                                      InRow.mBackground);
        }
        DrawScreenText(InScreen, box_.x_min, InY, Gutter, Color::GrayDark,
                       true);
        DrawScreenText(InScreen, mTextX, InY, InRow.mText, InRow.mForeground,
                       InRow.mbDim, InRow.mbBold, InRow.mBackground);
    }

    void DrawHorizontalRule(Screen &InScreen, const int InY) const
    {
        for (int X = box_.x_min; X <= box_.x_max; ++X)
        {
            if (IsWatchDrawableCell(InScreen, X, InY))
            {
                Pixel &Cell = InScreen.PixelAt(X, InY);
                Cell.character = "\xe2\x94\x80";
                Cell.foreground_color = Color::GrayDark;
                Cell.dim = true;
            }
        }
    }

    std::vector<FWatchProseLogicalLine> mLines;
    std::vector<FWatchProseVisualRow> mRows;
    FWatchProseLayoutCache *rpLayoutCache = nullptr;
    FWatchScrollRegionState *rpScrollState = nullptr;
    int mGutterWidth = 0;
    int mTextX = 0;
    int mHeight = 0;
    int mViewportHeight = 0;
    bool mHasScrollIndicators = false;
};

static Element WatchProseBlock(std::vector<FWatchProseLogicalLine> InLines)
{
    return std::make_shared<WatchProseBlockNode>(std::move(InLines));
}

static Element WatchScrollableProseBlock(
    std::vector<FWatchProseLogicalLine> InLines,
    FWatchScrollRegionState &InOutScrollState)
{
    return std::make_shared<WatchProseBlockNode>(std::move(InLines),
                                                 &InOutScrollState);
}

static Element WatchScrollableProseBlock(
    FWatchProseLayoutCache &InOutLayoutCache,
    FWatchScrollRegionState &InOutScrollState)
{
    return std::make_shared<WatchProseBlockNode>(InOutLayoutCache,
                                                 InOutScrollState);
}

class CodeSnippetViewportNode : public Node
{
  public:
    CodeSnippetViewportNode(std::vector<FCodeSnippetLogicalLine> InLines,
                            FWatchScrollRegionState &InOutScrollState)
        : mLines(std::move(InLines)), rpScrollState(&InOutScrollState)
    {
    }

    CodeSnippetViewportNode(FWatchCodeSnippetLayoutCache &InOutLayoutCache,
                            FWatchScrollRegionState &InOutScrollState)
        : rpLayoutCache(&InOutLayoutCache),
          rpScrollState(&InOutScrollState)
    {
    }

    void ComputeRequirement() override
    {
        requirement_.min_x = 0;
        requirement_.min_y = 0;
        requirement_.flex_grow_x = 1;
        requirement_.flex_shrink_x = 1;
        requirement_.flex_grow_y = 1;
        requirement_.flex_shrink_y = 1;
    }

    void SetBox(Box InBox) override
    {
        Node::SetBox(InBox);

        const int Width = std::max(0, InBox.x_max - InBox.x_min + 1);
        mHeight = std::max(0, InBox.y_max - InBox.y_min + 1);
        const int LineCount = rpLayoutCache == nullptr
                                  ? static_cast<int>(mLines.size())
                                  : rpLayoutCache->mLineCount;
        mGutterWidth = DecimalDigitCount(LineCount) + 1;
        mCodeX = InBox.x_min + mGutterWidth;
        const int CodeWidth = std::max(1, Width - mGutterWidth);

        if (rpLayoutCache == nullptr)
        {
            mRows = BuildCodeSnippetVisualRows(mLines, CodeWidth);
        }
        else if (rpLayoutCache->mAvailableTextCells != CodeWidth)
        {
            rpLayoutCache->mRows =
                BuildCodeSnippetVisualRows(rpLayoutCache->mLines, CodeWidth);
            rpLayoutCache->mAvailableTextCells = CodeWidth;
            rpLayoutCache->mGutterWidth = mGutterWidth;
            ++rpLayoutCache->mVisualBuildCount;
        }
        const std::vector<FCodeSnippetVisualRow> &Rows = GetRows();
        const int ContentHeight = static_cast<int>(Rows.size());
        mHasScrollIndicators = ContentHeight > mHeight;
        mViewportHeight =
            mHasScrollIndicators ? std::max(0, mHeight - 2) : mHeight;

        const int MaxOffset = mViewportHeight > 0
                                  ? std::max(0, ContentHeight - mViewportHeight)
                                  : 0;
        rpScrollState->mOffset =
            std::clamp(rpScrollState->mOffset, 0, MaxOffset);
        rpScrollState->mMaxOffset = MaxOffset;
    }

    void Render(Screen &InScreen) override
    {
        if (mHeight <= 0)
        {
            return;
        }

        const AutoReset<Box> Stencil(
            &InScreen.stencil, Box::Intersection(box_, InScreen.stencil));

        int Y = box_.y_min;
        if (mHasScrollIndicators)
        {
            if (rpScrollState->mOffset > 0)
            {
                DrawScreenText(InScreen, box_.x_min, Y,
                               "\xe2\x86\x91 " +
                                   std::to_string(rpScrollState->mOffset) +
                                   " above",
                               Color::GrayDark, true);
            }
            ++Y;
        }

        const int EndRow = std::min(
            static_cast<int>(GetRows().size()),
            rpScrollState->mOffset + std::max(0, mViewportHeight));
        for (int RowIndex = rpScrollState->mOffset; RowIndex < EndRow;
             ++RowIndex)
        {
            DrawCodeRow(InScreen, GetRows()[static_cast<size_t>(RowIndex)], Y);
            ++Y;
        }

        if (mHasScrollIndicators)
        {
            const int Below =
                std::max(0,
                         rpScrollState->mMaxOffset - rpScrollState->mOffset);
            if (Below > 0)
            {
                DrawScreenText(InScreen, box_.x_min, box_.y_max,
                               "\xe2\x86\x93 " + std::to_string(Below) +
                                   " below",
                               Color::GrayDark, true);
            }
        }
    }

  private:
    const std::vector<FCodeSnippetVisualRow> &GetRows() const
    {
        return rpLayoutCache == nullptr ? mRows : rpLayoutCache->mRows;
    }

    void DrawCodeRow(Screen &InScreen, const FCodeSnippetVisualRow &InRow,
                     const int InY) const
    {
        if (InY < box_.y_min || InY > box_.y_max)
        {
            return;
        }

        const std::string Gutter = BuildLineNumberGutter(
            InRow.mLineNumber, mGutterWidth, InRow.mbContinuation);
        DrawScreenText(InScreen, box_.x_min, InY, Gutter, Color::GrayDark,
                       true);
        DrawScreenText(InScreen, mCodeX, InY, InRow.mText, Color::Default,
                       !InRow.mbCode);
    }

    std::vector<FCodeSnippetLogicalLine> mLines;
    std::vector<FCodeSnippetVisualRow> mRows;
    FWatchCodeSnippetLayoutCache *rpLayoutCache = nullptr;
    FWatchScrollRegionState *rpScrollState;
    int mGutterWidth = 0;
    int mCodeX = 0;
    int mHeight = 0;
    int mViewportHeight = 0;
    bool mHasScrollIndicators = false;
};

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
                                 int InSelectedIndex,
                                 FWatchScrollRegionState &InOutScrollState) const
{
    const int Count = static_cast<int>(InPlans.size());

    std::vector<std::vector<Element>> ActiveTableData;
    ActiveTableData.push_back({text("Topic") | bold | flex,
                               text("Status") | bold, text("Phases") | bold,
                               text("BLK") | bold});
    int SelectedActiveRow = -1;

    for (int Index = 0; Index < Count; ++Index)
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

        Element TopicCell =
            text(Marker + Plan.mTopicKey) | flex | (Selected ? bold : nothing);
        if (Selected)
        {
            TopicCell = TopicCell | focus;
        }

        ActiveTableData.push_back({
            TopicCell,
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
    FinalRows.push_back(ActiveTable.Render() | flex);
    if (Count == 0)
    {
        FinalRows.push_back(text("(none)") | dim);
    }

    const std::string Title =
        " [A]CTIVE PLANS (" + std::to_string(Count) + ") ";
    return window(text(Title) | bold | color(Color::Green),
                  ScrollFrame(vbox(std::move(FinalRows)),
                              InOutScrollState));
}

// ---------------------------------------------------------------------------
// PhaseListPanel
// ---------------------------------------------------------------------------

static void EnsurePhaseListLayoutCache(
    const FWatchPlanSummary &InPlan, const bool bMetricView,
    FWatchPhaseListLayoutCache &InOutCache,
    const uint64_t InSnapshotGeneration)
{
    const bool bCacheMatches =
        InOutCache.mbValid &&
        InOutCache.mSnapshotGeneration == InSnapshotGeneration &&
        InOutCache.mTopicKey == InPlan.mTopicKey &&
        InOutCache.mbMetricView == bMetricView;
    if (bCacheMatches)
    {
        return;
    }

    InOutCache.mSnapshotGeneration = InSnapshotGeneration;
    InOutCache.mTopicKey = InPlan.mTopicKey;
    InOutCache.mbMetricView = bMetricView;
    InOutCache.mbValid = true;
    InOutCache.mRows.clear();

    if (InPlan.mTopicKey.empty())
    {
        ++InOutCache.mBuildCount;
        return;
    }

    const int Count = static_cast<int>(InPlan.mPhases.size());
    std::vector<std::string> PhaseLabels;
    std::vector<std::string> DefaultDesignLabels;
    std::vector<std::string> TaxonomyLabels;
    std::vector<std::string> MetricDesignLabels;
    std::vector<std::string> BloatLabels;
    std::vector<std::string> SolidLabels;
    std::vector<std::string> WordsLabels;
    std::vector<std::string> FieldsLabels;
    std::vector<std::string> WorkLabels;
    std::vector<std::string> TestsLabels;
    std::vector<std::string> FilesLabels;
    std::vector<std::string> EvidenceLabels;
    for (int Index = 0; Index < Count; ++Index)
    {
        const PhaseItem &Phase = InPlan.mPhases[static_cast<size_t>(Index)];
        const FPhaseRuntimeMetrics &Metrics = Phase.mMetrics;
        PhaseLabels.push_back(Phase.mPhaseKey);
        DefaultDesignLabels.push_back(Phase.mV4DesignChars == 0
                                          ? "-"
                                          : std::to_string(
                                                Phase.mV4DesignChars));
        TaxonomyLabels.push_back(BuildPhaseTaxonomySummary(
            InPlan.mPhaseTaxonomies, Index));
        MetricDesignLabels.push_back(std::to_string(Metrics.mDesignChars));
        BloatLabels.push_back(BuildBloatMetricLabel(Metrics));
        SolidLabels.push_back(std::to_string(Metrics.mSolidWordCount));
        WordsLabels.push_back(std::to_string(Metrics.mRecursiveWordCount));
        FieldsLabels.push_back(
            std::to_string(Metrics.mFieldCoveragePercent) + "%");
        WorkLabels.push_back(std::to_string(Metrics.mWorkItemCount));
        TestsLabels.push_back(std::to_string(Metrics.mTestingRecordCount));
        FilesLabels.push_back(std::to_string(Metrics.mFileManifestCount));
        EvidenceLabels.push_back(std::to_string(Metrics.mEvidenceItemCount));
    }

    InOutCache.mPhaseColumnWidth =
        ComputeTextColumnWidth("P", PhaseLabels, 3);
    InOutCache.mDefaultDesignColumnWidth = ComputeGaugeColumnWidth(
        "Design", DefaultDesignLabels, 16, kDesignBarPreferredWidth);
    InOutCache.mTaxonomyColumnWidth =
        ComputeTextColumnWidth("Taxonomy", TaxonomyLabels, 30);
    InOutCache.mMetricDesignColumnWidth = ComputeGaugeColumnWidth(
        "Design", MetricDesignLabels, 14, kMetricBarPreferredWidth);
    InOutCache.mBloatColumnWidth =
        ComputeTextColumnWidth("Bloat", BloatLabels, 16);
    InOutCache.mSolidColumnWidth = ComputeGaugeColumnWidth(
        "SOLID", SolidLabels, 14, kMetricBarPreferredWidth);
    InOutCache.mWordsColumnWidth = ComputeGaugeColumnWidth(
        "Words", WordsLabels, 14, kMetricBarPreferredWidth);
    InOutCache.mFieldsColumnWidth = ComputeGaugeColumnWidth(
        "Fields", FieldsLabels, 14, kMetricBarPreferredWidth);
    InOutCache.mWorkColumnWidth = ComputeGaugeColumnWidth(
        "Work", WorkLabels, 14, kMetricBarPreferredWidth);
    InOutCache.mTestsColumnWidth = ComputeGaugeColumnWidth(
        "Tests", TestsLabels, 14, kMetricBarPreferredWidth);
    InOutCache.mFilesColumnWidth = ComputeGaugeColumnWidth(
        "Files", FilesLabels, 14, kMetricBarPreferredWidth);
    InOutCache.mEvidenceColumnWidth = ComputeGaugeColumnWidth(
        "Evidence", EvidenceLabels, 14, kMetricBarPreferredWidth);

    if (bMetricView)
    {
        InOutCache.mDesignScale = BuildMetricGaugeScale(
            InPlan.mPhases, kPhaseHollowChars, kPhaseRichMinChars,
            [](const FPhaseRuntimeMetrics &InMetrics)
            { return InMetrics.mDesignChars; });
        InOutCache.mSolidScale =
            BuildMetricGaugeScale(InPlan.mPhases, kPhaseMetricSolidHollowWords,
                                  kPhaseMetricSolidRichWords,
                                  [](const FPhaseRuntimeMetrics &InMetrics)
                                  { return InMetrics.mSolidWordCount; });
        InOutCache.mWordsScale = BuildMetricGaugeScale(
            InPlan.mPhases, kPhaseMetricRecursiveHollowWords,
            kPhaseMetricRecursiveRichWords,
            [](const FPhaseRuntimeMetrics &InMetrics)
            { return InMetrics.mRecursiveWordCount; });
        InOutCache.mFieldsScale = BuildMetricGaugeScale(
            InPlan.mPhases, kPhaseMetricFieldCoverageHollowPercent,
            kPhaseMetricFieldCoverageRichPercent,
            [](const FPhaseRuntimeMetrics &InMetrics)
            { return InMetrics.mFieldCoveragePercent; });
        InOutCache.mWorkScale = BuildMetricGaugeScale(
            InPlan.mPhases, kPhaseMetricWorkHollowItems,
            kPhaseMetricWorkRichItems, [](const FPhaseRuntimeMetrics &InMetrics)
            { return InMetrics.mWorkItemCount; });
        InOutCache.mTestsScale = BuildMetricGaugeScale(
            InPlan.mPhases, kPhaseMetricTestingHollowRecords,
            kPhaseMetricTestingRichRecords,
            [](const FPhaseRuntimeMetrics &InMetrics)
            { return InMetrics.mTestingRecordCount; });
        InOutCache.mFilesScale = BuildMetricGaugeScale(
            InPlan.mPhases, kPhaseMetricFileManifestHollowEntries,
            kPhaseMetricFileManifestRichEntries,
            [](const FPhaseRuntimeMetrics &InMetrics)
            { return InMetrics.mFileManifestCount; });
        InOutCache.mEvidenceScale = BuildMetricGaugeScale(
            InPlan.mPhases, kPhaseMetricEvidenceHollowItems,
            kPhaseMetricEvidenceRichItems,
            [](const FPhaseRuntimeMetrics &InMetrics)
            { return InMetrics.mEvidenceItemCount; });
    }

    InOutCache.mRows.reserve(static_cast<size_t>(Count));
    for (int DisplayIndex = 0; DisplayIndex < Count; ++DisplayIndex)
    {
        const int Index = Count - DisplayIndex - 1;
        const PhaseItem &Phase = InPlan.mPhases[static_cast<size_t>(Index)];
        FWatchPhaseListRowModel Row;
        Row.mPhaseIndex = Index;
        Row.mPhaseKey = Phase.mPhaseKey;
        Row.mStatus = ToString(Phase.mStatus);
        Row.mDesignChars = Phase.mV4DesignChars;
        Row.mTaxonomy = BuildPhaseTaxonomySummary(InPlan.mPhaseTaxonomies,
                                                  Index);
        Row.mScope = Phase.mScope;
        Row.mOutput = Phase.mOutput;
        Row.mMetrics = Phase.mMetrics;
        InOutCache.mRows.push_back(std::move(Row));
    }
    ++InOutCache.mBuildCount;
}

static Color StatusColor(const std::string &InStatus)
{
    if (InStatus == "in_progress")
    {
        return Color::Green;
    }
    if (InStatus == "blocked")
    {
        return Color::Red;
    }
    if (InStatus == "not_started")
    {
        return Color::Yellow;
    }
    return Color::Default;
}

static bool StatusDim(const std::string &InStatus)
{
    return InStatus == "completed" || InStatus == "closed";
}

class PhaseListViewportNode : public Node
{
  public:
    PhaseListViewportNode(FWatchPhaseListLayoutCache &InCache,
                          const int InSelectedPhaseIndex,
                          FWatchScrollRegionState &InOutScrollState)
        : rpCache(&InCache), mSelectedPhaseIndex(InSelectedPhaseIndex),
          rpScrollState(&InOutScrollState)
    {
    }

    PhaseListViewportNode(
        std::shared_ptr<FWatchPhaseListLayoutCache> InOwnedCache,
        const int InSelectedPhaseIndex,
        FWatchScrollRegionState &InOutScrollState)
        : mOwnedCache(std::move(InOwnedCache)), rpCache(mOwnedCache.get()),
          mSelectedPhaseIndex(InSelectedPhaseIndex),
          rpScrollState(&InOutScrollState)
    {
    }

    void ComputeRequirement() override
    {
        requirement_.min_x = 0;
        requirement_.min_y = 0;
        requirement_.flex_grow_x = 1;
        requirement_.flex_shrink_x = 1;
        requirement_.flex_grow_y = 1;
        requirement_.flex_shrink_y = 1;
    }

    void SetBox(Box InBox) override
    {
        Node::SetBox(InBox);
        mHeight = std::max(0, InBox.y_max - InBox.y_min + 1);
        mWidth = std::max(0, InBox.x_max - InBox.x_min + 1);

        const int ContentRows = static_cast<int>(rpCache->mRows.size());
        mHasScrollIndicators = ContentRows > std::max(0, mHeight - 1);
        const int IndicatorRows = mHasScrollIndicators ? 2 : 0;
        mHeaderY = box_.y_min + (mHasScrollIndicators ? 1 : 0);
        mRowsY = mHeaderY + 1;
        mViewportHeight = std::max(0, mHeight - 1 - IndicatorRows);
        const int MaxOffset =
            mViewportHeight > 0
                ? std::max(0, ContentRows - mViewportHeight)
                : 0;

        int ScrollOffset = std::clamp(rpScrollState->mOffset, 0, MaxOffset);
        const int SelectedDisplayIndex = FindSelectedDisplayIndex();
        if (SelectedDisplayIndex >= 0 && mViewportHeight > 0)
        {
            if (SelectedDisplayIndex < ScrollOffset)
            {
                ScrollOffset = SelectedDisplayIndex;
            }
            else if (SelectedDisplayIndex >= ScrollOffset + mViewportHeight)
            {
                ScrollOffset = SelectedDisplayIndex - mViewportHeight + 1;
            }
            ScrollOffset = std::clamp(ScrollOffset, 0, MaxOffset);
        }

        rpScrollState->mOffset = ScrollOffset;
        rpScrollState->mMaxOffset = MaxOffset;
    }

    void Render(Screen &InScreen) override
    {
        if (mHeight <= 0)
        {
            return;
        }

        const AutoReset<Box> Stencil(
            &InScreen.stencil, Box::Intersection(box_, InScreen.stencil));

        if (mHasScrollIndicators && rpScrollState->mOffset > 0)
        {
            DrawText(InScreen, box_.x_min, box_.y_min, mWidth,
                     "\xe2\x86\x91 " +
                         std::to_string(rpScrollState->mOffset) + " above",
                     Color::GrayDark, true, false, false);
        }

        DrawHeader(InScreen, mHeaderY);

        const int EndRow =
            std::min(static_cast<int>(rpCache->mRows.size()),
                     rpScrollState->mOffset + mViewportHeight);
        int Y = mRowsY;
        for (int RowIndex = rpScrollState->mOffset; RowIndex < EndRow;
             ++RowIndex)
        {
            DrawRow(InScreen, rpCache->mRows[static_cast<size_t>(RowIndex)],
                    Y);
            ++Y;
        }

        if (mHasScrollIndicators)
        {
            const int Below =
                std::max(0,
                         rpScrollState->mMaxOffset - rpScrollState->mOffset);
            if (Below > 0)
            {
                DrawText(InScreen, box_.x_min, box_.y_max, mWidth,
                         "\xe2\x86\x93 " + std::to_string(Below) +
                             " below",
                         Color::GrayDark, true, false, false);
            }
        }
    }

  private:
    int FindSelectedDisplayIndex() const
    {
        for (size_t Index = 0; Index < rpCache->mRows.size(); ++Index)
        {
            if (rpCache->mRows[Index].mPhaseIndex == mSelectedPhaseIndex)
            {
                return static_cast<int>(Index);
            }
        }
        return -1;
    }

    void DrawText(Screen &InScreen, const int InX, const int InY,
                  const int InWidth, const std::string &InText,
                  const Color InForeground, const bool bDim,
                  const bool bBold, const bool bInverted) const
    {
        if (InWidth <= 0 || InY < box_.y_min || InY > box_.y_max)
        {
            return;
        }

        int X = InX;
        const int EndX = std::min(box_.x_max, InX + InWidth - 1);
        DrawWatchScreenText(
            InScreen, X, InY, EndX - X + 1, InText,
            FWatchScreenTextStyle{InForeground, Color::Default, bDim, bBold,
                                  bInverted});
    }

    void DrawGauge(Screen &InScreen, const int InX, const int InY,
                   const int InWidth, const size_t InValue,
                   const size_t InHollow, const size_t InRich,
                   const size_t InFillMax, const std::string &InLabel,
                   const int InPreferredBarWidth, const bool bCeilFill,
                   const bool bInverted) const
    {
        if (InWidth <= 0 || InLabel.empty())
        {
            return;
        }

        int BarWidth = std::max(0, InPreferredBarWidth);
        const int AvailableForBar = InWidth - TextCellWidth(InLabel) - 1;
        BarWidth = std::min(BarWidth, std::max(0, AvailableForBar));
        if (BarWidth <= 0)
        {
            DrawText(InScreen, InX, InY, InWidth, InLabel, Color::Default,
                     false, bInverted, bInverted);
            return;
        }

        const size_t Denominator = std::max<size_t>(1, InFillMax);
        const size_t ClampedValue = std::min(InValue, Denominator);
        int Filled = 0;
        if (bCeilFill)
        {
            Filled = static_cast<int>((ClampedValue * BarWidth +
                                       Denominator - 1) /
                                      Denominator);
        }
        else
        {
            Filled =
                static_cast<int>((ClampedValue * BarWidth) / Denominator);
        }
        if (InValue > 0 && Filled == 0)
        {
            Filled = 1;
        }
        Filled = std::min(BarWidth, std::max(0, Filled));

        const Color BarColor = (InValue < InHollow) ? Color::Red
                               : (InValue < InRich) ? Color::Yellow
                                                    : Color::Green;
        int X = InX;
        for (int Index = 0; Index < Filled; ++Index)
        {
            DrawText(InScreen, X, InY, 1, "\xe2\x96\x88", BarColor, false,
                     bInverted, bInverted);
            ++X;
        }
        for (int Index = Filled; Index < BarWidth; ++Index)
        {
            DrawText(InScreen, X, InY, 1, "\xe2\x96\x91", Color::Default, true,
                     bInverted, bInverted);
            ++X;
        }
        DrawText(InScreen, X, InY, InWidth - (X - InX), " " + InLabel,
                 Color::Default, false, bInverted, bInverted);
    }

    void DrawDesign(Screen &InScreen, const int InX, const int InY,
                    const int InWidth, const size_t InChars,
                    const bool bInverted) const
    {
        if (InChars == 0)
        {
            DrawText(InScreen, InX, InY, InWidth, "-", Color::Default, true,
                     bInverted, bInverted);
            return;
        }

        DrawGauge(InScreen, InX, InY, InWidth, InChars, kPhaseHollowChars,
                  kPhaseRichMinChars, kPhaseRichMinChars,
                  std::to_string(InChars), kDesignBarPreferredWidth,
                  /*bCeilFill=*/true, bInverted);
    }

    void DrawMetricGauge(Screen &InScreen, const int InX, const int InY,
                         const int InWidth, const size_t InValue,
                         const FWatchMetricGaugeScale &InScale,
                         const std::string &InLabel,
                         const bool bInverted) const
    {
        DrawGauge(InScreen, InX, InY, InWidth, InValue, InScale.mHollow,
                  InScale.mRich, ResolveMetricFillMax(InValue, InScale),
                  InLabel, kMetricBarPreferredWidth, /*bCeilFill=*/false,
                  bInverted);
    }

    void DrawHeader(Screen &InScreen, const int InY) const
    {
        if (InY < box_.y_min || InY > box_.y_max)
        {
            return;
        }

        int X = box_.x_min;
        if (rpCache->mbMetricView)
        {
            DrawHeaderCell(InScreen, X, InY, rpCache->mPhaseColumnWidth, "P");
            DrawHeaderCell(InScreen, X, InY, 12, "Status");
            DrawHeaderCell(InScreen, X, InY,
                           rpCache->mMetricDesignColumnWidth, "Design");
            DrawHeaderCell(InScreen, X, InY, rpCache->mBloatColumnWidth,
                           "Bloat");
            DrawHeaderCell(InScreen, X, InY, rpCache->mSolidColumnWidth,
                           "SOLID");
            DrawHeaderCell(InScreen, X, InY, rpCache->mWordsColumnWidth,
                           "Words");
            DrawHeaderCell(InScreen, X, InY, rpCache->mFieldsColumnWidth,
                           "Fields");
            DrawHeaderCell(InScreen, X, InY, rpCache->mWorkColumnWidth,
                           "Work");
            DrawHeaderCell(InScreen, X, InY, rpCache->mTestsColumnWidth,
                           "Tests");
            DrawHeaderCell(InScreen, X, InY, rpCache->mFilesColumnWidth,
                           "Files");
            DrawHeaderCell(InScreen, X, InY, rpCache->mEvidenceColumnWidth,
                           "Evidence");
            DrawText(InScreen, X, InY, std::max(0, box_.x_max - X + 1),
                     "Scope", Color::Default, false, true, false);
            return;
        }

        DrawHeaderCell(InScreen, X, InY, rpCache->mPhaseColumnWidth, "P");
        DrawHeaderCell(InScreen, X, InY, 14, "Status");
        DrawHeaderCell(InScreen, X, InY, rpCache->mDefaultDesignColumnWidth,
                       "Design");
        DrawHeaderCell(InScreen, X, InY, rpCache->mTaxonomyColumnWidth,
                       "Taxonomy");
        const int Remaining = std::max(0, box_.x_max - X + 1);
        const int ScopeWidth = Remaining / 2;
        DrawHeaderCell(InScreen, X, InY, ScopeWidth, "Scope");
        DrawText(InScreen, X, InY, std::max(0, box_.x_max - X + 1),
                 "Output", Color::Default, false, true, false);
    }

    void DrawHeaderCell(Screen &InScreen, int &InOutX, const int InY,
                        const int InWidth, const std::string &InText) const
    {
        DrawText(InScreen, InOutX, InY, InWidth, InText, Color::Default,
                 false, true, false);
        InOutX += InWidth + 1;
    }

    void DrawRow(Screen &InScreen, const FWatchPhaseListRowModel &InRow,
                 const int InY) const
    {
        const bool bSelected = InRow.mPhaseIndex == mSelectedPhaseIndex;
        int X = box_.x_min;
        if (rpCache->mbMetricView)
        {
            DrawCell(InScreen, X, InY, rpCache->mPhaseColumnWidth,
                     InRow.mPhaseKey, Color::Default, false, bSelected);
            DrawCell(InScreen, X, InY, 12, InRow.mStatus,
                     StatusColor(InRow.mStatus), StatusDim(InRow.mStatus),
                     bSelected);
            DrawMetricGauge(InScreen, X, InY,
                            rpCache->mMetricDesignColumnWidth,
                            InRow.mMetrics.mDesignChars,
                            rpCache->mDesignScale,
                            std::to_string(InRow.mMetrics.mDesignChars),
                            bSelected);
            X += rpCache->mMetricDesignColumnWidth + 1;
            DrawCell(InScreen, X, InY, rpCache->mBloatColumnWidth,
                     BuildBloatMetricLabel(InRow.mMetrics), Color::Default,
                     false, bSelected);
            DrawMetricGaugeCell(InScreen, X, InY, rpCache->mSolidColumnWidth,
                                InRow.mMetrics.mSolidWordCount,
                                rpCache->mSolidScale,
                                std::to_string(
                                    InRow.mMetrics.mSolidWordCount),
                                bSelected);
            DrawMetricGaugeCell(InScreen, X, InY, rpCache->mWordsColumnWidth,
                                InRow.mMetrics.mRecursiveWordCount,
                                rpCache->mWordsScale,
                                std::to_string(
                                    InRow.mMetrics.mRecursiveWordCount),
                                bSelected);
            DrawMetricGaugeCell(InScreen, X, InY, rpCache->mFieldsColumnWidth,
                                InRow.mMetrics.mFieldCoveragePercent,
                                rpCache->mFieldsScale,
                                std::to_string(
                                    InRow.mMetrics.mFieldCoveragePercent) +
                                    "%",
                                bSelected);
            DrawMetricGaugeCell(InScreen, X, InY, rpCache->mWorkColumnWidth,
                                InRow.mMetrics.mWorkItemCount,
                                rpCache->mWorkScale,
                                std::to_string(
                                    InRow.mMetrics.mWorkItemCount),
                                bSelected);
            DrawMetricGaugeCell(InScreen, X, InY, rpCache->mTestsColumnWidth,
                                InRow.mMetrics.mTestingRecordCount,
                                rpCache->mTestsScale,
                                std::to_string(
                                    InRow.mMetrics.mTestingRecordCount),
                                bSelected);
            DrawMetricGaugeCell(InScreen, X, InY, rpCache->mFilesColumnWidth,
                                InRow.mMetrics.mFileManifestCount,
                                rpCache->mFilesScale,
                                std::to_string(
                                    InRow.mMetrics.mFileManifestCount),
                                bSelected);
            DrawMetricGaugeCell(InScreen, X, InY,
                                rpCache->mEvidenceColumnWidth,
                                InRow.mMetrics.mEvidenceItemCount,
                                rpCache->mEvidenceScale,
                                std::to_string(
                                    InRow.mMetrics.mEvidenceItemCount),
                                bSelected);
            DrawText(InScreen, X, InY, std::max(0, box_.x_max - X + 1),
                     InRow.mScope, Color::Default, true, bSelected,
                     bSelected);
            return;
        }

        DrawCell(InScreen, X, InY, rpCache->mPhaseColumnWidth,
                 InRow.mPhaseKey, Color::Default, false, bSelected);
        DrawCell(InScreen, X, InY, 14, InRow.mStatus,
                 StatusColor(InRow.mStatus), StatusDim(InRow.mStatus),
                 bSelected);
        DrawDesign(InScreen, X, InY, rpCache->mDefaultDesignColumnWidth,
                   InRow.mDesignChars, bSelected);
        X += rpCache->mDefaultDesignColumnWidth + 1;
        DrawCell(InScreen, X, InY, rpCache->mTaxonomyColumnWidth,
                 InRow.mTaxonomy, Color::Default, true, bSelected);
        const int Remaining = std::max(0, box_.x_max - X + 1);
        const int ScopeWidth = Remaining / 2;
        DrawCell(InScreen, X, InY, ScopeWidth, InRow.mScope, Color::Default,
                 true, bSelected);
        DrawText(InScreen, X, InY, std::max(0, box_.x_max - X + 1),
                 InRow.mOutput, Color::Default, true, bSelected, bSelected);
    }

    void DrawCell(Screen &InScreen, int &InOutX, const int InY,
                  const int InWidth, const std::string &InText,
                  const Color InForeground, const bool bDim,
                  const bool bSelected) const
    {
        DrawText(InScreen, InOutX, InY, InWidth, InText, InForeground, bDim,
                 bSelected, bSelected);
        InOutX += InWidth + 1;
    }

    void DrawMetricGaugeCell(Screen &InScreen, int &InOutX, const int InY,
                             const int InWidth, const size_t InValue,
                             const FWatchMetricGaugeScale &InScale,
                             const std::string &InLabel,
                             const bool bSelected) const
    {
        DrawMetricGauge(InScreen, InOutX, InY, InWidth, InValue, InScale,
                        InLabel, bSelected);
        InOutX += InWidth + 1;
    }

    std::shared_ptr<FWatchPhaseListLayoutCache> mOwnedCache;
    FWatchPhaseListLayoutCache *rpCache;
    int mSelectedPhaseIndex = -1;
    FWatchScrollRegionState *rpScrollState;
    int mHeight = 0;
    int mWidth = 0;
    int mHeaderY = 0;
    int mRowsY = 0;
    int mViewportHeight = 0;
    bool mHasScrollIndicators = false;
};

Element PhaseListPanel::Render(const FWatchPlanSummary &InPlan,
                               int InSelectedPhaseIndex, bool InMetricView,
                               FWatchScrollRegionState &InOutScrollState,
                               FWatchPhaseListLayoutCache &InOutCache,
                               uint64_t InSnapshotGeneration) const
{
    if (InPlan.mTopicKey.empty())
    {
        InOutScrollState.Reset();
        InOutCache.Reset();
        return window(text(" PHASE LIST ") | bold,
                      text("No plan selected") | dim);
    }

    EnsurePhaseListLayoutCache(InPlan, InMetricView, InOutCache,
                               InSnapshotGeneration);

    // v0.97.0 no-truncation contract applies here too — FTXUI's frame
    // handles overflow at the terminal boundary; the CLI layer emits
    // the verbatim topic key.
    const int Count = static_cast<int>(InPlan.mPhases.size());
    const std::string Title =
        std::string(" [P]HASE LIST") + (InMetricView ? " METRICS: " : ": ") +
        InPlan.mTopicKey + " (" + std::to_string(Count) + ") ";
    return window(text(Title) | bold | color(Color::Cyan),
                  std::make_shared<PhaseListViewportNode>(
                      InOutCache, InSelectedPhaseIndex, InOutScrollState)) |
           size(HEIGHT, EQUAL, 25);
}

Element PhaseListPanel::Render(const FWatchPlanSummary &InPlan,
                               int InSelectedPhaseIndex, bool InMetricView,
                               FWatchScrollRegionState &InOutScrollState) const
{
    if (InPlan.mTopicKey.empty())
    {
        InOutScrollState.Reset();
        return window(text(" PHASE LIST ") | bold,
                      text("No plan selected") | dim);
    }

    auto Cache = std::make_shared<FWatchPhaseListLayoutCache>();
    EnsurePhaseListLayoutCache(InPlan, InMetricView, *Cache, 0);

    const int Count = static_cast<int>(InPlan.mPhases.size());
    const std::string Title =
        std::string(" [P]HASE LIST") + (InMetricView ? " METRICS: " : ": ") +
        InPlan.mTopicKey + " (" + std::to_string(Count) + ") ";
    return window(text(Title) | bold | color(Color::Cyan),
                  std::make_shared<PhaseListViewportNode>(
                      Cache, InSelectedPhaseIndex, InOutScrollState)) |
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
            paragraph(Blocker.mAction) | dim | flex,
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
                            int InSelectedIndex,
                            FWatchScrollRegionState &InOutScrollState) const
{
    const int Count = static_cast<int>(InPlans.size());

    std::vector<std::vector<Element>> NonActiveTableData;
    NonActiveTableData.push_back(
        {text("Topic") | bold | flex, text("Status") | bold});
    int SelectedNonActiveRow = -1;

    for (int Index = 0; Index < Count; ++Index)
    {
        const FWatchPlanSummary &Plan = InPlans[static_cast<size_t>(Index)];
        const bool Selected = (Index == InSelectedIndex);
        const std::string Marker;

        Element TopicCell =
            text(Marker + Plan.mTopicKey) | flex | (Selected ? bold : nothing);
        if (Selected)
        {
            TopicCell = TopicCell | focus;
        }

        NonActiveTableData.push_back({
            TopicCell,
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
    NonActiveFinal.push_back(NonActiveTable.Render() | flex);
    if (Count == 0)
    {
        NonActiveFinal.push_back(text("(none)") | dim);
    }

    return window(text(" [N]ON-ACTIVE (" + std::to_string(Count) + ") ") |
                      bold | dim,
                  ScrollFrame(vbox(std::move(NonActiveFinal)),
                              InOutScrollState));
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
            {text(Check.mID) | color(Color::Red),
             paragraph(Detail) | dim | flex});
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
                                       bool InFocusMode,
                                       FWatchScrollRegionState
                                           &InOutLaneScrollState) const
{
    FWatchExecutionTaxonomyLayoutCache Cache;
    return Render(InPlan, InSelectedPhaseIndex, InSelectedWaveIndex,
                  InSelectedLaneIndex, InFocusMode, InOutLaneScrollState,
                  Cache, 0);
}

static const FPhaseTaxonomy *
FindSelectedPhaseTaxonomy(const FWatchPlanSummary &InPlan,
                          const int InSelectedPhaseIndex)
{
    if (InSelectedPhaseIndex < 0 ||
        InSelectedPhaseIndex >= static_cast<int>(InPlan.mPhases.size()))
    {
        return nullptr;
    }

    for (const FPhaseTaxonomy &Tax : InPlan.mPhaseTaxonomies)
    {
        if (Tax.mPhaseIndex == InSelectedPhaseIndex)
        {
            return &Tax;
        }
    }
    return nullptr;
}

static std::vector<int> BuildUniqueWaves(const FPhaseTaxonomy &InTaxonomy)
{
    std::vector<int> UniqueWaves;
    for (const FJobRecord &Job : InTaxonomy.mJobs)
    {
        if (std::find(UniqueWaves.begin(), UniqueWaves.end(), Job.mWave) ==
            UniqueWaves.end())
        {
            UniqueWaves.push_back(Job.mWave);
        }
    }
    return UniqueWaves;
}

static int ResolveWaveFilter(const FPhaseTaxonomy &InTaxonomy,
                             const int InSelectedWaveIndex)
{
    const std::vector<int> UniqueWaves = BuildUniqueWaves(InTaxonomy);
    if (InSelectedWaveIndex >= 0 &&
        InSelectedWaveIndex < static_cast<int>(UniqueWaves.size()))
    {
        return UniqueWaves[static_cast<size_t>(InSelectedWaveIndex)];
    }
    return -1;
}

static int ResolveLaneFilter(const FPhaseTaxonomy &InTaxonomy,
                             const int InSelectedLaneIndex)
{
    if (InSelectedLaneIndex >= 0 &&
        InSelectedLaneIndex < static_cast<int>(InTaxonomy.mLanes.size()))
    {
        return InSelectedLaneIndex;
    }
    return -1;
}

static void FillExecutionTaxonomySummary(
    const FPhaseTaxonomy &InTaxonomy, const int InLaneFilter,
    FWatchExecutionTaxonomyLayoutCache &InOutCache)
{
    InOutCache.mLanesDone = 0;
    InOutCache.mJobsDone = 0;
    InOutCache.mJobsActive = 0;
    InOutCache.mJobsTodo = 0;
    InOutCache.mTasksDone = 0;
    InOutCache.mTasksActive = 0;
    InOutCache.mTasksTodo = 0;

    for (const FLaneRecord &Lane : InTaxonomy.mLanes)
    {
        if (Lane.mStatus == EExecutionStatus::Completed)
        {
            ++InOutCache.mLanesDone;
        }
    }

    for (const FJobRecord &Job : InTaxonomy.mJobs)
    {
        if (Job.mStatus == EExecutionStatus::Completed)
        {
            ++InOutCache.mJobsDone;
        }
        else if (Job.mStatus == EExecutionStatus::InProgress)
        {
            ++InOutCache.mJobsActive;
        }
        else
        {
            ++InOutCache.mJobsTodo;
        }

        if (InLaneFilter >= 0 && Job.mLane != InLaneFilter)
        {
            continue;
        }
        for (const FTaskRecord &Task : Job.mTasks)
        {
            if (Task.mStatus == EExecutionStatus::Completed)
            {
                ++InOutCache.mTasksDone;
            }
            else if (Task.mStatus == EExecutionStatus::InProgress)
            {
                ++InOutCache.mTasksActive;
            }
            else
            {
                ++InOutCache.mTasksTodo;
            }
        }
    }
}

static Element RenderExecutionTaxonomySummaryLine(
    const FPhaseTaxonomy &InTaxonomy,
    const FWatchExecutionTaxonomyLayoutCache &InCache)
{
    return hbox({
        text(" ") | bold,
        text(std::to_string(InTaxonomy.mLanes.size()) + " lanes (" +
             std::to_string(InCache.mLanesDone) + " done)") |
            dim,
        text(" | ") | dim,
        text(std::to_string(InTaxonomy.mWaveCount) + " waves") | dim,
        text(" | ") | dim,
        text(std::to_string(InTaxonomy.mJobs.size()) + " jobs: ") | dim,
        text(std::to_string(InCache.mJobsDone) + "d") |
            color(Color::Green),
        text(" " + std::to_string(InCache.mJobsActive) + "a") |
            color(Color::Cyan),
        text(" " + std::to_string(InCache.mJobsTodo) + "t") |
            color(Color::Yellow),
        text(" | ") | dim,
        text(std::to_string(InTaxonomy.mTasks.size()) + " tasks: ") | dim,
        text(std::to_string(InCache.mTasksDone) + "d") |
            color(Color::Green),
        text(" " + std::to_string(InCache.mTasksActive) + "a") |
            color(Color::Cyan),
        text(" " + std::to_string(InCache.mTasksTodo) + "t") |
            color(Color::Yellow),
    });
}

static void EnsureExecutionTaxonomyLayoutCache(
    const FWatchPlanSummary &InPlan, const FPhaseTaxonomy &InTaxonomy,
    const int InSelectedWaveIndex, const int InSelectedLaneIndex,
    FWatchExecutionTaxonomyLayoutCache &InOutCache,
    const uint64_t InSnapshotGeneration)
{
    const int WaveFilter =
        ResolveWaveFilter(InTaxonomy, InSelectedWaveIndex);
    const int LaneFilter =
        ResolveLaneFilter(InTaxonomy, InSelectedLaneIndex);
    const bool bCacheMatches =
        InOutCache.mbValid &&
        InOutCache.mSnapshotGeneration == InSnapshotGeneration &&
        InOutCache.mTopicKey == InPlan.mTopicKey &&
        InOutCache.mPhaseIndex == InTaxonomy.mPhaseIndex &&
        InOutCache.mSelectedWaveIndex == InSelectedWaveIndex &&
        InOutCache.mSelectedLaneIndex == InSelectedLaneIndex &&
        InOutCache.mWaveFilter == WaveFilter &&
        InOutCache.mLaneFilter == LaneFilter;
    if (bCacheMatches)
    {
        return;
    }

    InOutCache.mSnapshotGeneration = InSnapshotGeneration;
    InOutCache.mTopicKey = InPlan.mTopicKey;
    InOutCache.mPhaseIndex = InTaxonomy.mPhaseIndex;
    InOutCache.mSelectedWaveIndex = InSelectedWaveIndex;
    InOutCache.mSelectedLaneIndex = InSelectedLaneIndex;
    InOutCache.mWaveFilter = WaveFilter;
    InOutCache.mLaneFilter = LaneFilter;
    InOutCache.mVisibleJobCount = 0;
    InOutCache.mVisibleTaskCount = 0;
    InOutCache.mbHasFlatTasks = !InTaxonomy.mTasks.empty();
    InOutCache.mLanes.clear();
    InOutCache.mJobs.clear();
    InOutCache.mTasks.clear();
    FillExecutionTaxonomySummary(InTaxonomy, LaneFilter, InOutCache);

    for (int Index = 0; Index < static_cast<int>(InTaxonomy.mLanes.size());
         ++Index)
    {
        const FLaneRecord &Lane =
            InTaxonomy.mLanes[static_cast<size_t>(Index)];
        FWatchTaxonomyLaneRowModel Row;
        Row.mLaneIndex = Index;
        Row.mStatus = std::string(ToString(Lane.mStatus));
        Row.mScope = Lane.mScope;
        Row.mExitCriteria = Lane.mExitCriteria;
        InOutCache.mLanes.push_back(std::move(Row));
    }

    for (size_t JobIndex = 0; JobIndex < InTaxonomy.mJobs.size(); ++JobIndex)
    {
        const FJobRecord &Job = InTaxonomy.mJobs[JobIndex];
        if ((WaveFilter >= 0 && Job.mWave != WaveFilter) ||
            (LaneFilter >= 0 && Job.mLane != LaneFilter))
        {
            continue;
        }

        FWatchTaxonomyJobRowModel Row;
        Row.mJobIndex = static_cast<int>(JobIndex);
        Row.mWave = Job.mWave;
        Row.mLane = Job.mLane;
        Row.mStatus = std::string(ToString(Job.mStatus));
        Row.mScope = Job.mScope;
        InOutCache.mJobs.push_back(std::move(Row));
        ++InOutCache.mVisibleJobCount;
    }

    for (size_t JobIndex = 0; JobIndex < InTaxonomy.mJobs.size(); ++JobIndex)
    {
        const FJobRecord &Job = InTaxonomy.mJobs[JobIndex];
        if (LaneFilter >= 0 && Job.mLane != LaneFilter)
        {
            continue;
        }
        for (size_t TaskIndex = 0; TaskIndex < Job.mTasks.size();
             ++TaskIndex)
        {
            const FTaskRecord &Task = Job.mTasks[TaskIndex];
            FWatchTaxonomyTaskRowModel Row;
            Row.mJobIndex = static_cast<int>(JobIndex);
            Row.mTaskIndex = static_cast<int>(TaskIndex);
            Row.mStatus = std::string(ToString(Task.mStatus));
            Row.mDescription = Task.mDescription;
            Row.mEvidence = Task.mEvidence;
            InOutCache.mTasks.push_back(std::move(Row));
            ++InOutCache.mVisibleTaskCount;
        }
    }

    InOutCache.mbValid = true;
    ++InOutCache.mBuildCount;
}

Element ExecutionTaxonomyPanel::Render(
    const FWatchPlanSummary &InPlan, int InSelectedPhaseIndex,
    int InSelectedWaveIndex, int InSelectedLaneIndex, bool InFocusMode,
    FWatchScrollRegionState &InOutLaneScrollState,
    FWatchExecutionTaxonomyLayoutCache &InOutCache,
    const uint64_t InSnapshotGeneration) const
{
    const FPhaseTaxonomy *rpTax =
        FindSelectedPhaseTaxonomy(InPlan, InSelectedPhaseIndex);
    if (rpTax == nullptr || (rpTax->mLanes.empty() && rpTax->mJobs.empty() &&
                             rpTax->mTasks.empty()))
    {
        InOutCache.Reset();
        std::string Hint = InPlan.mTopicKey.empty()
                               ? "No plan selected"
                               : "No execution taxonomy for selected phase";
        return window(text(" EXECUTION TAXONOMY ") | bold | dim,
                      text(Hint) | dim);
    }

    const FPhaseTaxonomy &Tax = *rpTax;
    const int LaneFilter = ResolveLaneFilter(Tax, InSelectedLaneIndex);
    if (!InFocusMode)
    {
        FWatchExecutionTaxonomyLayoutCache Summary;
        FillExecutionTaxonomySummary(Tax, LaneFilter, Summary);
        return RenderExecutionTaxonomySummaryLine(Tax, Summary);
    }

    EnsureExecutionTaxonomyLayoutCache(
        InPlan, Tax, InSelectedWaveIndex, InSelectedLaneIndex, InOutCache,
        InSnapshotGeneration);

    std::vector<Elements> LaneGridRows;
    LaneGridRows.push_back(PadGridRow({
        text("Lane") | bold,
        text("Status") | bold | size(WIDTH, EQUAL, 14),
        text("Scope") | bold | flex,
        text("Exit Criteria") | bold | flex,
    }));
    for (const FWatchTaxonomyLaneRowModel &Lane : InOutCache.mLanes)
    {
        const bool bLaneSelected =
            Lane.mLaneIndex == InSelectedLaneIndex;
        Element LaneKeyCell =
            text("L" + std::to_string(Lane.mLaneIndex));
        if (bLaneSelected)
        {
            LaneKeyCell = LaneKeyCell | focus;
        }
        Elements RowCells = PadGridRow({
            LaneKeyCell,
            ColorStatus(Lane.mStatus) | size(WIDTH, EQUAL, 14),
            paragraph(Lane.mScope) | dim | flex,
            paragraph(Lane.mExitCriteria) | dim | flex,
        });
        if (bLaneSelected)
        {
            for (auto &Cell : RowCells)
            {
                Cell = Cell | inverted | bold;
            }
        }
        LaneGridRows.push_back(std::move(RowCells));
    }

    Elements LaneFinalRows;
    LaneFinalRows.push_back(gridbox(std::move(LaneGridRows)) | flex);
    auto LanesPanel =
        window(text(" [L]ANES: P" + std::to_string(Tax.mPhaseIndex) + " (" +
                    std::to_string(Tax.mLanes.size()) + ") ") |
                   bold,
               ScrollFrame(vbox(std::move(LaneFinalRows)),
                           InOutLaneScrollState)) |
        size(HEIGHT, EQUAL, 13);

    std::vector<std::vector<Element>> JobTableData;
    JobTableData.push_back({text("Wave") | bold, text("Lane") | bold,
                            text("Status") | bold,
                            text("Scope") | bold | flex});
    for (const FWatchTaxonomyJobRowModel &Job : InOutCache.mJobs)
    {
        JobTableData.push_back({
            text("W" + std::to_string(Job.mWave)),
            text("L" + std::to_string(Job.mLane)),
            ColorStatus(Job.mStatus),
            paragraph(Job.mScope) | dim | flex,
        });
    }

    Element JobsContent;
    if (InOutCache.mVisibleJobCount == 0 && !Tax.mJobs.empty())
    {
        JobsContent = text("(no jobs match current wave/lane filter)") | dim;
    }
    else
    {
        auto JobTable = Table(std::move(JobTableData));
        JobTable.SelectAll().SeparatorVertical(EMPTY);
        JobTable.SelectRow(0).SeparatorHorizontal(LIGHT);
        JobsContent = JobTable.Render() | flex;
    }

    const std::string WaveTitle =
        InOutCache.mWaveFilter < 0
            ? "all"
            : ("W" + std::to_string(InOutCache.mWaveFilter));
    const std::string LaneTitle =
        InOutCache.mLaneFilter < 0
            ? "all"
            : ("L" + std::to_string(InOutCache.mLaneFilter));
    auto JobsPanel =
        window(text(" [W]AVE/[L]ANE/JOB BOARD: P" +
                    std::to_string(Tax.mPhaseIndex) + " (W=" + WaveTitle +
                    " L=" + LaneTitle + ", " +
                    std::to_string(Tax.mJobs.size()) + " jobs) ") |
                   bold,
               JobsContent);

    std::vector<std::vector<Element>> TaskTableData;
    TaskTableData.push_back(
        {text("Job") | bold, text("Task") | bold, text("Status") | bold,
         text("Description") | bold | flex, text("Evidence") | bold | flex});
    for (const FWatchTaxonomyTaskRowModel &Task : InOutCache.mTasks)
    {
        TaskTableData.push_back({
            text("J" + std::to_string(Task.mJobIndex)),
            text("T" + std::to_string(Task.mTaskIndex)),
            ColorStatus(Task.mStatus),
            paragraph(Task.mDescription) | dim | flex,
            paragraph(Task.mEvidence) | dim | flex,
        });
    }

    Element TasksContent;
    if (!InOutCache.mbHasFlatTasks)
    {
        const std::string Hint =
            Tax.mJobs.empty()
                ? "(phase has no job board yet)"
                : "(phase decomposes to job granularity only; no "
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
        window(text(" TASKS: P" + std::to_string(Tax.mPhaseIndex) + " (" +
                    std::to_string(Tax.mTasks.size()) + ") ") |
                   bold,
               TasksContent);

    return vbox({
        LanesPanel,
        JobsPanel,
        TasksPanel,
        RenderExecutionTaxonomySummaryLine(Tax, InOutCache),
    });
}

static Color FileActionColor(const EFileAction InAction)
{
    switch (InAction)
    {
    case EFileAction::Create:
        return Color::Green;
    case EFileAction::Modify:
        return Color::Yellow;
    case EFileAction::Delete:
        return Color::Red;
    }
    return Color::Default;
}

static std::vector<FWatchFileManifestVisualRow>
BuildFileManifestVisualRows(
    const std::vector<FWatchFileManifestRowModel> &InRows,
    const int InAvailableDescriptionCells)
{
    std::vector<FWatchFileManifestVisualRow> Rows;
    for (const FWatchFileManifestRowModel &LogicalRow : InRows)
    {
        const std::vector<std::string> WrappedDescription =
            WrapTextLineForDisplay(LogicalRow.mDescription,
                                   InAvailableDescriptionCells);
        for (size_t WrapIndex = 0; WrapIndex < WrappedDescription.size();
             ++WrapIndex)
        {
            FWatchFileManifestVisualRow Row;
            Row.mFilePath = WrapIndex == 0 ? LogicalRow.mFilePath : "";
            Row.mActionText = WrapIndex == 0 ? LogicalRow.mActionText : "";
            Row.mAction = LogicalRow.mAction;
            Row.mDescription = WrappedDescription[WrapIndex];
            Row.mbContinuation = WrapIndex > 0;
            Rows.push_back(std::move(Row));
        }
    }
    return Rows;
}

static void EnsureFileManifestLayoutCache(
    const std::string &InTopicKey, const FPhaseTaxonomy &InTaxonomy,
    FWatchFileManifestLayoutCache &InOutCache,
    const uint64_t InSnapshotGeneration)
{
    const bool bCacheMatches =
        InOutCache.mbValid &&
        InOutCache.mSnapshotGeneration == InSnapshotGeneration &&
        InOutCache.mTopicKey == InTopicKey &&
        InOutCache.mPhaseIndex == InTaxonomy.mPhaseIndex;
    if (bCacheMatches)
    {
        return;
    }

    InOutCache.mSnapshotGeneration = InSnapshotGeneration;
    InOutCache.mTopicKey = InTopicKey;
    InOutCache.mPhaseIndex = InTaxonomy.mPhaseIndex;
    InOutCache.mAvailableDescriptionCells = -1;
    InOutCache.mRows.clear();
    InOutCache.mVisualRows.clear();
    InOutCache.mFileColumnWidth = 0;
    InOutCache.mActionColumnWidth = 0;
    InOutCache.mbValid = true;

    for (const FFileManifestItem &Item : InTaxonomy.mFileManifest)
    {
        FWatchFileManifestRowModel Row;
        Row.mFilePath = Item.mFilePath;
        Row.mAction = Item.mAction;
        Row.mActionText = std::string(ToString(Item.mAction));
        Row.mDescription = Item.mDescription;
        InOutCache.mFileColumnWidth =
            std::max(InOutCache.mFileColumnWidth, TextCellWidth(Row.mFilePath));
        InOutCache.mActionColumnWidth =
            std::max(InOutCache.mActionColumnWidth,
                     TextCellWidth(Row.mActionText));
        InOutCache.mRows.push_back(std::move(Row));
    }
    InOutCache.mFileColumnWidth =
        std::max(InOutCache.mFileColumnWidth, TextCellWidth("File"));
    InOutCache.mActionColumnWidth =
        std::max(InOutCache.mActionColumnWidth, TextCellWidth("Action"));
    ++InOutCache.mBuildCount;
}

class FileManifestViewportNode : public Node
{
  public:
    FileManifestViewportNode(FWatchFileManifestLayoutCache &InCache,
                             FWatchScrollRegionState &InOutScrollState)
        : rpCache(&InCache), rpScrollState(&InOutScrollState)
    {
    }

    FileManifestViewportNode(
        std::shared_ptr<FWatchFileManifestLayoutCache> InOwnedCache,
        FWatchScrollRegionState &InOutScrollState)
        : mOwnedCache(std::move(InOwnedCache)), rpCache(mOwnedCache.get()),
          rpScrollState(&InOutScrollState)
    {
    }

    void ComputeRequirement() override
    {
        requirement_.min_x = 0;
        requirement_.min_y = 0;
        requirement_.flex_grow_x = 1;
        requirement_.flex_shrink_x = 1;
        requirement_.flex_grow_y = 1;
        requirement_.flex_shrink_y = 1;
    }

    void SetBox(Box InBox) override
    {
        Node::SetBox(InBox);
        mHeight = std::max(0, InBox.y_max - InBox.y_min + 1);
        mWidth = std::max(0, InBox.x_max - InBox.x_min + 1);

        const int PaddedActionWidth = std::max(6, rpCache->mActionColumnWidth);
        const int RemainingAfterAction =
            std::max(0, mWidth - PaddedActionWidth - 2);
        mFileColumnWidth =
            std::min(std::max(12, RemainingAfterAction / 2),
                     std::max(12, rpCache->mFileColumnWidth));
        mFileColumnWidth = std::min(mFileColumnWidth, RemainingAfterAction);
        mActionColumnWidth = PaddedActionWidth;
        mDescriptionColumnWidth =
            std::max(1, mWidth - mFileColumnWidth - mActionColumnWidth - 2);

        if (rpCache->mAvailableDescriptionCells != mDescriptionColumnWidth)
        {
            rpCache->mVisualRows = BuildFileManifestVisualRows(
                rpCache->mRows, mDescriptionColumnWidth);
            rpCache->mAvailableDescriptionCells = mDescriptionColumnWidth;
            ++rpCache->mVisualBuildCount;
        }

        const int ContentRows = static_cast<int>(rpCache->mVisualRows.size());
        mHasScrollIndicators = ContentRows > std::max(0, mHeight - 1);
        const int IndicatorRows = mHasScrollIndicators ? 2 : 0;
        mHeaderY = box_.y_min + (mHasScrollIndicators ? 1 : 0);
        mRowsY = mHeaderY + 1;
        mViewportHeight = std::max(0, mHeight - 1 - IndicatorRows);
        const int MaxOffset =
            mViewportHeight > 0
                ? std::max(0, ContentRows - mViewportHeight)
                : 0;
        rpScrollState->mOffset =
            std::clamp(rpScrollState->mOffset, 0, MaxOffset);
        rpScrollState->mMaxOffset = MaxOffset;
    }

    void Render(Screen &InScreen) override
    {
        if (mHeight <= 0)
        {
            return;
        }

        const AutoReset<Box> Stencil(
            &InScreen.stencil, Box::Intersection(box_, InScreen.stencil));

        if (mHasScrollIndicators && rpScrollState->mOffset > 0)
        {
            DrawText(InScreen, box_.x_min, box_.y_min, mWidth,
                     "\xe2\x86\x91 " +
                         std::to_string(rpScrollState->mOffset) + " above",
                     Color::GrayDark, true, false);
        }

        DrawHeader(InScreen, mHeaderY);

        const int EndRow =
            std::min(static_cast<int>(rpCache->mVisualRows.size()),
                     rpScrollState->mOffset + mViewportHeight);
        int Y = mRowsY;
        for (int RowIndex = rpScrollState->mOffset; RowIndex < EndRow;
             ++RowIndex)
        {
            DrawRow(InScreen,
                    rpCache->mVisualRows[static_cast<size_t>(RowIndex)], Y);
            ++Y;
        }

        if (mHasScrollIndicators)
        {
            const int Below =
                std::max(0,
                         rpScrollState->mMaxOffset - rpScrollState->mOffset);
            if (Below > 0)
            {
                DrawText(InScreen, box_.x_min, box_.y_max, mWidth,
                         "\xe2\x86\x93 " + std::to_string(Below) +
                             " below",
                         Color::GrayDark, true, false);
            }
        }
    }

  private:
    void DrawText(Screen &InScreen, const int InX, const int InY,
                  const int InWidth, const std::string &InText,
                  const Color InForeground, const bool bDim,
                  const bool bBold) const
    {
        if (InWidth <= 0 || InY < box_.y_min || InY > box_.y_max)
        {
            return;
        }

        DrawWatchScreenText(
            InScreen, InX, InY, std::min(box_.x_max, InX + InWidth - 1) -
                                  InX + 1,
            InText,
            FWatchScreenTextStyle{InForeground, Color::Default, bDim, bBold,
                                  false});
    }

    void DrawHeader(Screen &InScreen, const int InY) const
    {
        int X = box_.x_min;
        DrawCell(InScreen, X, InY, mFileColumnWidth, "File", Color::Default,
                 false, true);
        DrawCell(InScreen, X, InY, mActionColumnWidth, "Action",
                 Color::Default, false, true);
        DrawText(InScreen, X, InY, std::max(0, box_.x_max - X + 1),
                 "Description", Color::Default, false, true);
    }

    void DrawRow(Screen &InScreen,
                 const FWatchFileManifestVisualRow &InRow,
                 const int InY) const
    {
        int X = box_.x_min;
        DrawCell(InScreen, X, InY, mFileColumnWidth, InRow.mFilePath,
                 Color::Default, InRow.mbContinuation, false);
        DrawCell(InScreen, X, InY, mActionColumnWidth, InRow.mActionText,
                 FileActionColor(InRow.mAction), InRow.mbContinuation, false);
        DrawText(InScreen, X, InY, std::max(0, box_.x_max - X + 1),
                 InRow.mDescription, Color::Default, true, false);
    }

    void DrawCell(Screen &InScreen, int &InOutX, const int InY,
                  const int InWidth, const std::string &InText,
                  const Color InForeground, const bool bDim,
                  const bool bBold) const
    {
        DrawText(InScreen, InOutX, InY, InWidth, InText, InForeground, bDim,
                 bBold);
        InOutX += InWidth + 1;
    }

    std::shared_ptr<FWatchFileManifestLayoutCache> mOwnedCache;
    FWatchFileManifestLayoutCache *rpCache;
    FWatchScrollRegionState *rpScrollState;
    int mHeight = 0;
    int mWidth = 0;
    int mFileColumnWidth = 0;
    int mActionColumnWidth = 0;
    int mDescriptionColumnWidth = 0;
    int mHeaderY = 0;
    int mRowsY = 0;
    int mViewportHeight = 0;
    bool mHasScrollIndicators = false;
};

Element FileManifestPanel::Render(const FPhaseTaxonomy &InTaxonomy,
                                  FWatchScrollRegionState
                                      &InOutScrollState) const
{
    auto Cache = std::make_shared<FWatchFileManifestLayoutCache>();
    if (InTaxonomy.mFileManifest.empty())
    {
        InOutScrollState.Reset();
        return window(
            text(" FILES: " + ("P" + std::to_string(InTaxonomy.mPhaseIndex)) +
                 " (0) ") |
                bold,
            text("(no file changes planned)") | dim);
    }
    EnsureFileManifestLayoutCache("", InTaxonomy, *Cache, 0);
    const std::string Title =
        " FILES: " + ("P" + std::to_string(InTaxonomy.mPhaseIndex)) + " (" +
        std::to_string(InTaxonomy.mFileManifest.size()) + ") ";
    return window(text(Title) | bold,
                  std::make_shared<FileManifestViewportNode>(
                      Cache, InOutScrollState));
}

Element FileManifestPanel::Render(
    const std::string &InTopicKey, const FPhaseTaxonomy &InTaxonomy,
    FWatchScrollRegionState &InOutScrollState,
    FWatchFileManifestLayoutCache &InOutCache,
    const uint64_t InSnapshotGeneration) const
{
    if (InTaxonomy.mFileManifest.empty())
    {
        InOutScrollState.Reset();
        InOutCache.Reset();
        return window(
            text(" FILES: " + ("P" + std::to_string(InTaxonomy.mPhaseIndex)) +
                 " (0) ") |
                bold,
            text("(no file changes planned)") | dim);
    }

    EnsureFileManifestLayoutCache(InTopicKey, InTaxonomy, InOutCache,
                                  InSnapshotGeneration);

    const std::string Title =
        " FILES: " + ("P" + std::to_string(InTaxonomy.mPhaseIndex)) + " (" +
        std::to_string(InTaxonomy.mFileManifest.size()) + ") ";

    return window(text(Title) | bold,
                  std::make_shared<FileManifestViewportNode>(
                      InOutCache, InOutScrollState));
}

// ---------------------------------------------------------------------------
// CodeSnippetPanel
// ---------------------------------------------------------------------------

Element CodeSnippetPanel::Render(const FWatchPlanSummary &InPlan,
                                 int InSelectedPhaseIndex,
                                 FWatchScrollRegionState
                                     &InOutScrollState) const
{
    if (InPlan.mTopicKey.empty())
    {
        InOutScrollState.Reset();
        return window(text(" CODE SNIPPETS ") | bold | dim,
                      text("No plan selected") | dim);
    }

    if (InSelectedPhaseIndex < 0 ||
        InSelectedPhaseIndex >= static_cast<int>(InPlan.mPhases.size()))
    {
        InOutScrollState.Reset();
        return window(text(" CODE SNIPPETS: " + InPlan.mTopicKey + " ") |
                          bold | dim,
                      text("No phase selected") | dim);
    }

    const PhaseItem &Phase =
        InPlan.mPhases[static_cast<size_t>(InSelectedPhaseIndex)];
    if (Phase.mCodeSnippets.empty())
    {
        InOutScrollState.Reset();
        return window(text(" CODE SNIPPETS: " + InPlan.mTopicKey + " P" +
                           std::to_string(InSelectedPhaseIndex) +
                           " (0 lines) ") |
                          bold | dim,
                      text("(no code snippets)") | dim);
    }

    const std::vector<std::string> RawLines =
        SplitCodeSnippetLines(Phase.mCodeSnippets);
    std::vector<FCodeSnippetLogicalLine> Lines =
        StyleCodeSnippetLines(RawLines);
    const std::string Title = " CODE SNIPPETS: " + InPlan.mTopicKey + " P" +
                              std::to_string(InSelectedPhaseIndex) + " (" +
                              std::to_string(Lines.size()) + " lines) ";

    return window(
        text(Title) | bold | color(Color::Cyan),
        std::make_shared<CodeSnippetViewportNode>(std::move(Lines),
                                                  InOutScrollState));
}

static void EnsureCodeSnippetLayoutCache(
    const FWatchPlanSummary &InPlan, const int InSelectedPhaseIndex,
    FWatchCodeSnippetLayoutCache &InOutCache,
    const uint64_t InSnapshotGeneration)
{
    const bool bCacheMatches =
        InOutCache.mbLogicalValid &&
        InOutCache.mSnapshotGeneration == InSnapshotGeneration &&
        InOutCache.mTopicKey == InPlan.mTopicKey &&
        InOutCache.mPhaseIndex == InSelectedPhaseIndex;
    if (bCacheMatches)
    {
        return;
    }

    InOutCache.mSnapshotGeneration = InSnapshotGeneration;
    InOutCache.mTopicKey = InPlan.mTopicKey;
    InOutCache.mPhaseIndex = InSelectedPhaseIndex;
    InOutCache.mAvailableTextCells = -1;
    InOutCache.mGutterWidth = 0;
    InOutCache.mRows.clear();
    InOutCache.mLines.clear();

    const PhaseItem &Phase =
        InPlan.mPhases[static_cast<size_t>(InSelectedPhaseIndex)];
    const std::vector<std::string> RawLines =
        SplitCodeSnippetLines(Phase.mCodeSnippets);
    InOutCache.mLines = StyleCodeSnippetLines(RawLines);
    InOutCache.mLineCount = static_cast<int>(InOutCache.mLines.size());
    InOutCache.mbLogicalValid = true;
    ++InOutCache.mLogicalBuildCount;
}

Element CodeSnippetPanel::Render(const FWatchPlanSummary &InPlan,
                                 int InSelectedPhaseIndex,
                                 FWatchScrollRegionState &InOutScrollState,
                                 FWatchCodeSnippetLayoutCache &InOutCache,
                                 uint64_t InSnapshotGeneration) const
{
    if (InPlan.mTopicKey.empty())
    {
        InOutScrollState.Reset();
        InOutCache.Reset();
        return window(text(" CODE SNIPPETS ") | bold | dim,
                      text("No plan selected") | dim);
    }

    if (InSelectedPhaseIndex < 0 ||
        InSelectedPhaseIndex >= static_cast<int>(InPlan.mPhases.size()))
    {
        InOutScrollState.Reset();
        InOutCache.Reset();
        return window(text(" CODE SNIPPETS: " + InPlan.mTopicKey + " ") |
                          bold | dim,
                      text("No phase selected") | dim);
    }

    const PhaseItem &Phase =
        InPlan.mPhases[static_cast<size_t>(InSelectedPhaseIndex)];
    if (Phase.mCodeSnippets.empty())
    {
        InOutScrollState.Reset();
        InOutCache.Reset();
        return window(text(" CODE SNIPPETS: " + InPlan.mTopicKey + " P" +
                           std::to_string(InSelectedPhaseIndex) +
                           " (0 lines) ") |
                          bold | dim,
                      text("(no code snippets)") | dim);
    }

    EnsureCodeSnippetLayoutCache(InPlan, InSelectedPhaseIndex, InOutCache,
                                 InSnapshotGeneration);
    const std::string Title = " CODE SNIPPETS: " + InPlan.mTopicKey + " P" +
                              std::to_string(InSelectedPhaseIndex) + " (" +
                              std::to_string(InOutCache.mLineCount) +
                              " lines) ";

    return window(
        text(Title) | bold | color(Color::Cyan),
        std::make_shared<CodeSnippetViewportNode>(InOutCache,
                                                  InOutScrollState));
}

// ---------------------------------------------------------------------------
// PhaseDetailsPanel
// ---------------------------------------------------------------------------

static std::string JoinPhaseDetailParts(
    const std::vector<std::string> &InParts)
{
    std::string Result;
    for (const std::string &Part : InParts)
    {
        if (Part.empty())
        {
            continue;
        }
        if (!Result.empty())
        {
            Result += " | ";
        }
        Result += Part;
    }
    return Result.empty() ? "-" : Result;
}

static void AddPhaseDetailsSection(
    std::vector<FWatchProseLogicalLine> &OutLines, const std::string &InTitle)
{
    AddWatchProseLine(OutLines, InTitle, Color::Yellow, false, true);
}

static void AddPhaseDetailsField(
    std::vector<FWatchProseLogicalLine> &OutLines, const std::string &InLabel,
    const std::string &InValue, const Color InColor = Color::Default,
    const bool bDim = false, const Color InHeaderColor = Color::CyanLight)
{
    AddWatchProseLine(OutLines, InLabel, InHeaderColor, false, true);

    if (InValue.empty())
    {
        AddWatchProseLine(OutLines, "-", Color::Default, true);
        AddWatchProseSeparator(OutLines);
        return;
    }

    const std::vector<std::string> Lines = SplitTextLinesForDisplay(InValue);
    bool bInsideTaggedFence = false;
    for (const std::string &Line : Lines)
    {
        if (!bInsideTaggedFence && IsTaggedFenceOpeningLine(Line))
        {
            AddWatchProseLine(OutLines, Line, InColor, bDim);
            bInsideTaggedFence = true;
            continue;
        }

        if (bInsideTaggedFence && IsFenceLine(Line))
        {
            AddWatchProseLine(OutLines, Line, InColor, bDim);
            bInsideTaggedFence = false;
            continue;
        }

        Color Background = Color::Default;
        if (bInsideTaggedFence)
        {
            Background = PhaseDetailsCodeBackground();
        }
        AddWatchProseLine(OutLines, Line, InColor, bDim, false, "", true,
                          Background);
    }
    AddWatchProseSeparator(OutLines);
}

static void AddPhaseDetailsDerivedField(
    std::vector<FWatchProseLogicalLine> &OutLines, const std::string &InLabel,
    const std::string &InValue, const Color InColor = Color::Default,
    const bool bDim = false)
{
    AddPhaseDetailsField(OutLines, InLabel, InValue, InColor, bDim,
                         Color::Yellow);
}

static void AddPhaseDetailsCollectionItem(
    std::vector<FWatchProseLogicalLine> &OutLines, const std::string &InText)
{
    AddWatchProseLine(OutLines, InText.empty() ? "-" : InText);
    AddWatchProseSeparator(OutLines);
}

static void AddPhaseDetailsEmptyCollection(
    std::vector<FWatchProseLogicalLine> &OutLines)
{
    AddWatchProseLine(OutLines, "(none)", Color::Default, true);
    AddWatchProseSeparator(OutLines);
}

static std::string FormatDependency(const FBundleReference &InReference)
{
    std::vector<std::string> Parts;
    Parts.push_back(std::string(ToString(InReference.mKind)));
    if (!InReference.mTopic.empty())
    {
        Parts.push_back("topic=" + InReference.mTopic);
    }
    if (InReference.mPhase >= 0)
    {
        Parts.push_back("phase=P" + std::to_string(InReference.mPhase));
    }
    if (!InReference.mPath.empty())
    {
        Parts.push_back("path=" + InReference.mPath);
    }
    if (!InReference.mNote.empty())
    {
        Parts.push_back("note=" + InReference.mNote);
    }
    return JoinPhaseDetailParts(Parts);
}

static std::string FormatValidationCommand(
    const FValidationCommand &InCommand)
{
    return JoinPhaseDetailParts({
        "platform=" + std::string(ToString(InCommand.mPlatform)),
        "command=" + InCommand.mCommand,
        "description=" + InCommand.mDescription,
    });
}

static std::string FormatLaneDetails(const FLaneRecord &InLane,
                                     const int InIndex)
{
    return JoinPhaseDetailParts({
        "L" + std::to_string(InIndex),
        "status=" + std::string(ToString(InLane.mStatus)),
        "scope=" + InLane.mScope,
        "exit=" + InLane.mExitCriteria,
    });
}

static std::string FormatJobDetails(const FJobRecord &InJob,
                                    const int InIndex)
{
    return JoinPhaseDetailParts({
        "J" + std::to_string(InIndex),
        "wave=W" + std::to_string(InJob.mWave),
        "lane=L" + std::to_string(InJob.mLane),
        "status=" + std::string(ToString(InJob.mStatus)),
        "scope=" + InJob.mScope,
        "output=" + InJob.mOutput,
        "exit=" + InJob.mExitCriteria,
        "started=" + InJob.mStartedAt,
        "completed=" + InJob.mCompletedAt,
    });
}

static std::string FormatTaskDetails(const FTaskRecord &InTask,
                                     const int InIndex)
{
    return JoinPhaseDetailParts({
        "T" + std::to_string(InIndex),
        "status=" + std::string(ToString(InTask.mStatus)),
        "description=" + InTask.mDescription,
        "evidence=" + InTask.mEvidence,
        "notes=" + InTask.mNotes,
        "completed=" + InTask.mCompletedAt,
    });
}

static std::string FormatTestingRecord(const FTestingRecord &InRecord,
                                       const int InIndex)
{
    return JoinPhaseDetailParts({
        "test " + std::to_string(InIndex),
        "actor=" + std::string(ToString(InRecord.mActor)),
        "session=" + InRecord.mSession,
        "step=" + InRecord.mStep,
        "action=" + InRecord.mAction,
        "expected=" + InRecord.mExpected,
        "evidence=" + InRecord.mEvidence,
    });
}

static std::string FormatFileManifestItem(const FFileManifestItem &InItem,
                                          const int InIndex)
{
    return JoinPhaseDetailParts({
        "file " + std::to_string(InIndex),
        "action=" + std::string(ToString(InItem.mAction)),
        "path=" + InItem.mFilePath,
        "description=" + InItem.mDescription,
    });
}

static void BuildPhaseDetailsLines(
    const PhaseItem &InPhase, const FPhaseTaxonomy &InTaxonomy,
    std::vector<FWatchProseLogicalLine> &OutLines)
{
    AddPhaseDetailsSection(OutLines, "overview");
    AddPhaseDetailsField(OutLines, "status", ToString(InPhase.mStatus));
    AddPhaseDetailsField(OutLines, "origin", ToString(InPhase.mOrigin),
                         Color::Default, true);
    AddPhaseDetailsDerivedField(OutLines, "design_chars",
                                std::to_string(InPhase.mV4DesignChars));
    AddPhaseDetailsField(OutLines, "started_at", InPhase.mStartedAt);
    AddPhaseDetailsField(OutLines, "completed_at", InPhase.mCompletedAt);
    AddPhaseDetailsField(OutLines, "no_file_manifest",
                         InPhase.mbNoFileManifest ? "yes" : "no");
    if (InPhase.mbNoFileManifest ||
        !InPhase.mFileManifestSkipReason.empty())
    {
        AddPhaseDetailsField(OutLines, "file_manifest_skip_reason",
                             InPhase.mFileManifestSkipReason);
    }

    AddPhaseDetailsSection(OutLines, "scope_and_output");
    AddPhaseDetailsField(OutLines, "scope", InPhase.mScope);
    AddPhaseDetailsField(OutLines, "output", InPhase.mOutput);

    AddPhaseDetailsSection(OutLines, "lifecycle");
    AddPhaseDetailsField(OutLines, "done", InPhase.mDone);
    AddPhaseDetailsField(OutLines, "remaining", InPhase.mRemaining);
    AddPhaseDetailsField(OutLines, "blockers", InPhase.mBlockers);
    AddPhaseDetailsField(OutLines, "agent_context", InPhase.mAgentContext);

    AddPhaseDetailsSection(OutLines, "design");
    AddPhaseDetailsField(OutLines, "investigation", InPhase.mInvestigation);
    AddPhaseDetailsField(OutLines, "code_entity_contract",
                         InPhase.mCodeEntityContract);
    AddPhaseDetailsField(OutLines, "best_practices", InPhase.mBestPractices);
    AddPhaseDetailsField(OutLines, "multi_platforming",
                         InPhase.mMultiPlatforming);
    AddPhaseDetailsField(OutLines, "readiness_gate",
                         InPhase.mReadinessGate);
    AddPhaseDetailsField(OutLines, "handoff", InPhase.mHandoff);
    const int CodeLineCount =
        InPhase.mCodeSnippets.empty()
            ? 0
            : static_cast<int>(
                  SplitTextLinesForDisplay(InPhase.mCodeSnippets).size());
    AddPhaseDetailsDerivedField(OutLines, "code_snippets",
                                std::to_string(CodeLineCount) +
                                    " lines; press F12 for raw code");

    AddPhaseDetailsSection(OutLines, "dependencies");
    if (InPhase.mDependencies.empty())
    {
        AddPhaseDetailsEmptyCollection(OutLines);
    }
    else
    {
        for (const FBundleReference &Reference : InPhase.mDependencies)
        {
            AddPhaseDetailsCollectionItem(OutLines,
                                          FormatDependency(Reference));
        }
    }

    AddPhaseDetailsSection(OutLines, "validation_commands");
    if (InPhase.mValidationCommands.empty())
    {
        AddPhaseDetailsEmptyCollection(OutLines);
    }
    else
    {
        for (const FValidationCommand &Command :
             InPhase.mValidationCommands)
        {
            AddPhaseDetailsCollectionItem(OutLines,
                                          FormatValidationCommand(Command));
        }
    }

    AddPhaseDetailsSection(OutLines, "execution_taxonomy");
    if (InTaxonomy.mLanes.empty() && InTaxonomy.mJobs.empty() &&
        InTaxonomy.mTasks.empty())
    {
        AddPhaseDetailsEmptyCollection(OutLines);
    }
    for (size_t Index = 0; Index < InTaxonomy.mLanes.size(); ++Index)
    {
        AddPhaseDetailsCollectionItem(
            OutLines, FormatLaneDetails(InTaxonomy.mLanes[Index],
                                        static_cast<int>(Index)));
    }
    for (size_t Index = 0; Index < InTaxonomy.mJobs.size(); ++Index)
    {
        AddPhaseDetailsCollectionItem(
            OutLines, FormatJobDetails(InTaxonomy.mJobs[Index],
                                       static_cast<int>(Index)));
    }
    for (size_t Index = 0; Index < InTaxonomy.mTasks.size(); ++Index)
    {
        AddPhaseDetailsCollectionItem(
            OutLines, FormatTaskDetails(InTaxonomy.mTasks[Index],
                                        static_cast<int>(Index)));
    }

    AddPhaseDetailsSection(OutLines, "testing_records");
    if (InPhase.mTesting.empty())
    {
        AddPhaseDetailsEmptyCollection(OutLines);
    }
    else
    {
        for (size_t Index = 0; Index < InPhase.mTesting.size(); ++Index)
        {
            AddPhaseDetailsCollectionItem(
                OutLines, FormatTestingRecord(InPhase.mTesting[Index],
                                              static_cast<int>(Index)));
        }
    }

    AddPhaseDetailsSection(OutLines, "file_manifest");
    if (InPhase.mbNoFileManifest)
    {
        AddPhaseDetailsField(OutLines, "file_manifest_skip_reason",
                             InPhase.mFileManifestSkipReason);
    }
    else if (InTaxonomy.mFileManifest.empty())
    {
        AddPhaseDetailsEmptyCollection(OutLines);
    }
    else
    {
        for (size_t Index = 0; Index < InTaxonomy.mFileManifest.size();
             ++Index)
        {
            AddPhaseDetailsCollectionItem(
                OutLines,
                FormatFileManifestItem(InTaxonomy.mFileManifest[Index],
                                       static_cast<int>(Index)));
        }
    }
}

Element PhaseDetailsPanel::Render(const FWatchPlanSummary &InPlan,
                                  int InSelectedPhaseIndex,
                                  const FPhaseTaxonomy &InTaxonomy,
                                  FWatchScrollRegionState
                                      &InOutScrollState) const
{
    if (InPlan.mTopicKey.empty())
    {
        InOutScrollState.Reset();
        return window(text(" PHASE DETAILS ") | bold | dim,
                      text("No plan selected") | dim);
    }

    if (InSelectedPhaseIndex < 0 ||
        InSelectedPhaseIndex >= static_cast<int>(InPlan.mPhases.size()))
    {
        InOutScrollState.Reset();
        return window(text(" PHASE DETAILS: " + InPlan.mTopicKey + " ") |
                          bold | dim,
                      text("No phase selected") | dim);
    }

    const PhaseItem &Phase =
        InPlan.mPhases[static_cast<size_t>(InSelectedPhaseIndex)];
    std::vector<FWatchProseLogicalLine> Lines;
    BuildPhaseDetailsLines(Phase, InTaxonomy, Lines);

    const std::string Title =
        " PHASE DETAILS: " + InPlan.mTopicKey + " P" +
        std::to_string(InSelectedPhaseIndex) + " (" +
        std::to_string(CountNumberedProseLines(Lines)) + " lines) ";

    return window(text(Title) | bold | color(Color::Cyan),
                  WatchScrollableProseBlock(std::move(Lines),
                                            InOutScrollState));
}

static void EnsurePhaseDetailsLayoutCache(
    const FWatchPlanSummary &InPlan, const int InSelectedPhaseIndex,
    const FPhaseTaxonomy &InTaxonomy, FWatchProseLayoutCache &InOutCache,
    const uint64_t InSnapshotGeneration)
{
    const bool bCacheMatches =
        InOutCache.mbLogicalValid &&
        InOutCache.mSnapshotGeneration == InSnapshotGeneration &&
        InOutCache.mTopicKey == InPlan.mTopicKey &&
        InOutCache.mPhaseIndex == InSelectedPhaseIndex;
    if (bCacheMatches)
    {
        return;
    }

    InOutCache.mSnapshotGeneration = InSnapshotGeneration;
    InOutCache.mTopicKey = InPlan.mTopicKey;
    InOutCache.mPhaseIndex = InSelectedPhaseIndex;
    InOutCache.mAvailableTextCells = -1;
    InOutCache.mGutterWidth = 0;
    InOutCache.mRows.clear();
    InOutCache.mLines.clear();

    const PhaseItem &Phase =
        InPlan.mPhases[static_cast<size_t>(InSelectedPhaseIndex)];
    BuildPhaseDetailsLines(Phase, InTaxonomy, InOutCache.mLines);
    InOutCache.mLineCount = CountNumberedProseLines(InOutCache.mLines);
    InOutCache.mbLogicalValid = true;
    ++InOutCache.mLogicalBuildCount;
}

Element PhaseDetailsPanel::Render(const FWatchPlanSummary &InPlan,
                                  int InSelectedPhaseIndex,
                                  const FPhaseTaxonomy &InTaxonomy,
                                  FWatchScrollRegionState &InOutScrollState,
                                  FWatchProseLayoutCache &InOutCache,
                                  uint64_t InSnapshotGeneration) const
{
    if (InPlan.mTopicKey.empty())
    {
        InOutScrollState.Reset();
        InOutCache.Reset();
        return window(text(" PHASE DETAILS ") | bold | dim,
                      text("No plan selected") | dim);
    }

    if (InSelectedPhaseIndex < 0 ||
        InSelectedPhaseIndex >= static_cast<int>(InPlan.mPhases.size()))
    {
        InOutScrollState.Reset();
        InOutCache.Reset();
        return window(text(" PHASE DETAILS: " + InPlan.mTopicKey + " ") |
                          bold | dim,
                      text("No phase selected") | dim);
    }

    EnsurePhaseDetailsLayoutCache(InPlan, InSelectedPhaseIndex, InTaxonomy,
                                  InOutCache, InSnapshotGeneration);

    const std::string Title =
        " PHASE DETAILS: " + InPlan.mTopicKey + " P" +
        std::to_string(InSelectedPhaseIndex) + " (" +
        std::to_string(InOutCache.mLineCount) + " lines) ";

    return window(text(Title) | bold | color(Color::Cyan),
                  WatchScrollableProseBlock(InOutCache, InOutScrollState));
}

// ---------------------------------------------------------------------------
// PlanDetailPanel
// ---------------------------------------------------------------------------

Element PlanDetailPanel::Render(const FWatchPlanSummary &InPlan) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" PLAN DETAIL ") | bold | dim,
                      text("No plan selected") | dim);
    }

    const auto AddProseLine =
        [](std::vector<FWatchProseLogicalLine> &OutLines,
           std::string InText, Color InColor = Color::Default,
           const bool bDim = false, const bool bBoldLine = false,
           std::string InPrefix = "", const bool bNumbered = true)
    {
        FWatchProseLogicalLine Line;
        Line.mText = std::move(InText);
        Line.mPrefix = std::move(InPrefix);
        Line.mForeground = InColor;
        Line.mbDim = bDim;
        Line.mbBold = bBoldLine;
        Line.mbNumbered = bNumbered;
        OutLines.push_back(std::move(Line));
    };

    const auto AddGap = [&AddProseLine](
                            std::vector<FWatchProseLogicalLine> &OutLines)
    {
        AddProseLine(OutLines, "", Color::Default, false, false, "", false);
    };

    std::vector<FWatchProseLogicalLine> LeftRows;

    AddProseLine(LeftRows, "Summary", Color::Default, false, true);
    if (InPlan.mSummaryLines.empty())
    {
        AddProseLine(LeftRows, "(none)", Color::Default, true);
    }
    else
    {
        for (const std::string &Line : InPlan.mSummaryLines)
        {
            AddProseLine(LeftRows, Line, Color::Default, true);
        }
    }

    AddGap(LeftRows);

    AddProseLine(LeftRows, "Goals", Color::Default, false, true);
    if (InPlan.mGoalStatements.empty())
    {
        AddProseLine(LeftRows, "(none)", Color::Default, true);
    }
    else
    {
        for (const std::string &Goal : InPlan.mGoalStatements)
        {
            AddProseLine(LeftRows, Goal, Color::Green, false, false,
                         "\xe2\x97\x8f ");
        }
    }

    AddGap(LeftRows);

    AddProseLine(LeftRows, "Non-Goals", Color::Default, false, true);
    if (InPlan.mNonGoalStatements.empty())
    {
        AddProseLine(LeftRows, "(none)", Color::Default, true);
    }
    else
    {
        for (const std::string &NonGoal : InPlan.mNonGoalStatements)
        {
            AddProseLine(LeftRows, NonGoal, Color::Yellow, false, false,
                         "\xe2\x97\x8b ");
        }
    }

    std::vector<FWatchProseLogicalLine> RightRows;
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

    AddProseLine(RightRows, "Risks", Color::Default, false, true);
    if (InPlan.mRiskEntries.empty())
    {
        AddProseLine(RightRows, "(none)", Color::Default, true);
    }
    else
    {
        for (const FRiskEntry &R : InPlan.mRiskEntries)
        {
            const std::string Prefix = std::string("[") +
                                       ToString(R.mSeverity) + "/" +
                                       ToString(R.mStatus) + "] ";
            AddProseLine(RightRows, R.mStatement,
                         SeverityColor(R.mSeverity), false, false, Prefix);
        }
    }

    AddGap(RightRows);
    AddProseLine(RightRows, "Next Actions", Color::Default, false, true);
    if (InPlan.mNextActionEntries.empty())
    {
        AddProseLine(RightRows, "(none)", Color::Default, true);
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
            const std::string Prefix = std::to_string(A.mOrder) + ". [" +
                                       ToString(A.mStatus) + "] ";
            const Color C =
                (A.mStatus == EActionStatus::Completed)
                    ? Color::Green
                    : (A.mStatus == EActionStatus::Abandoned ? Color::GrayDark
                                                              : Color::Cyan);
            AddProseLine(RightRows, A.mStatement, C, false, false, Prefix);
        }
    }

    AddGap(RightRows);
    AddProseLine(RightRows, "Acceptance Criteria", Color::Default, false,
                 true);
    if (InPlan.mAcceptanceCriteria.empty())
    {
        AddProseLine(RightRows, "(none)", Color::Default, true);
    }
    else
    {
        for (const FAcceptanceCriterionEntry &C : InPlan.mAcceptanceCriteria)
        {
            const std::string Glyph = CriterionGlyph(C.mStatus);
            const std::string IdPart = C.mId.empty() ? "" : (C.mId + " ");
            AddProseLine(RightRows, C.mStatement,
                         CriterionColor(C.mStatus), false, false,
                         Glyph + std::string(" ") + IdPart);
        }
    }

    return window(text(" PLAN DETAIL ") | bold | color(Color::Cyan),
                  hbox({
                      WatchProseBlock(std::move(LeftRows)) |
                          FlexWithWeight(2),
                      separator(),
                      WatchProseBlock(std::move(RightRows)) |
                          FlexWithWeight(1),
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
        text("  |  Disc: " +
             std::to_string(InPerformance.mDiscoveryDurationMs) + "ms") |
            dim,
        text("  |  Metrics: " +
             std::to_string(InPerformance.mMetricRecomputeCount)) |
            dim,
        text("  |  Val: " + Validation) | dim,
        text("  |  Lint: " + Lint) | dim,
        filler(),
        text("q=quit  a/A=plan  n/N=non-active  p/P=phase  d=metrics  "
             "F5=phase  F6=files  F12=code  [/]=side scroll  "
             "w/W=wave  l/L=lane  r=refresh") |
            dim,
        text("  "),
    });
}

} // namespace UniPlan
