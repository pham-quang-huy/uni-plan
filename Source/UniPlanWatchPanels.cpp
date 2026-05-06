#include "UniPlanWatchPanels.h"
#include "UniPlanHelpers.h"
#include "UniPlanTopicTypes.h" // kPhaseHollowChars, ComputePhaseDesignChars
#include "UniPlanTypes.h"
#include "UniPlanWatchScroll.h"

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/dom/requirement.hpp>
#include <ftxui/dom/table.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/string.hpp>

#include <algorithm>
#include <cctype>
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
                              const FMetricGaugeScale &InScale,
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

struct FCodeSnippetLogicalLine
{
    std::string mText;
    bool mbCode = true;
};

struct FCodeSnippetVisualRow
{
    int mLineNumber = 0;
    bool mbContinuation = false;
    bool mbCode = true;
    std::string mText;
};

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

static std::vector<std::string>
SplitCodeSnippetLines(const std::string &InSnippets)
{
    std::vector<std::string> Lines;
    std::string Current;
    for (const char Character : InSnippets)
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

static std::vector<std::string> WrapCodeLineForDisplay(
    const std::string &InLine, const int InAvailableCells)
{
    std::vector<std::string> Wrapped;
    if (InAvailableCells <= 0)
    {
        Wrapped.push_back("");
        return Wrapped;
    }

    const std::string Expanded = ExpandTabsForDisplay(InLine);
    std::vector<std::string> Cells = Utf8ToGlyphs(Expanded);
    if (Cells.empty())
    {
        Wrapped.push_back("");
        return Wrapped;
    }

    int Start = 0;
    const int CellCount = static_cast<int>(Cells.size());
    while (Start < CellCount)
    {
        int End = std::min(Start + InAvailableCells, CellCount);
        if (End < CellCount && End > Start && Cells[static_cast<size_t>(End)]
                                                  .empty())
        {
            --End;
        }
        if (End <= Start)
        {
            End = std::min(Start + 1, CellCount);
        }

        std::string Segment;
        for (int Index = Start; Index < End; ++Index)
        {
            Segment += Cells[static_cast<size_t>(Index)];
        }
        Wrapped.push_back(Segment);

        Start = End;
        while (Start < CellCount && Cells[static_cast<size_t>(Start)].empty())
        {
            ++Start;
        }
    }

    return Wrapped;
}

static std::vector<FCodeSnippetVisualRow> BuildCodeSnippetVisualRows(
    const std::vector<FCodeSnippetLogicalLine> &InLines,
    const int InAvailableCells)
{
    std::vector<FCodeSnippetVisualRow> Rows;
    for (size_t LineIndex = 0; LineIndex < InLines.size(); ++LineIndex)
    {
        const std::vector<std::string> Wrapped =
            WrapCodeLineForDisplay(InLines[LineIndex].mText, InAvailableCells);
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

static bool IsDrawableCell(const Screen &InScreen, const int InX,
                           const int InY)
{
    return InX >= 0 && InX < InScreen.dimx() && InY >= 0 &&
           InY < InScreen.dimy() && InX >= InScreen.stencil.x_min &&
           InX <= InScreen.stencil.x_max && InY >= InScreen.stencil.y_min &&
           InY <= InScreen.stencil.y_max;
}

static void DrawScreenText(Screen &InScreen, const int InX, const int InY,
                           const std::string &InText,
                           const Color InForeground, const bool bDim,
                           const bool bBold = false)
{
    int X = InX;
    for (const std::string &Glyph : Utf8ToGlyphs(InText))
    {
        if (IsDrawableCell(InScreen, X, InY))
        {
            Pixel &Cell = InScreen.PixelAt(X, InY);
            Cell.character = Glyph;
            Cell.foreground_color = InForeground;
            Cell.dim = bDim;
            Cell.bold = bBold;
        }
        ++X;
    }
}

class CodeSnippetViewportNode : public Node
{
  public:
    CodeSnippetViewportNode(std::vector<FCodeSnippetLogicalLine> InLines,
                            FWatchScrollRegionState &InOutScrollState)
        : mLines(std::move(InLines)), rpScrollState(&InOutScrollState)
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
        mGutterWidth =
            DecimalDigitCount(static_cast<int>(mLines.size())) + 1;
        mCodeX = InBox.x_min + mGutterWidth;
        const int CodeWidth = std::max(1, Width - mGutterWidth);

        mRows = BuildCodeSnippetVisualRows(mLines, CodeWidth);
        const int ContentHeight = static_cast<int>(mRows.size());
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

        int Y = box_.y_min;
        if (mHasScrollIndicators)
        {
            if (rpScrollState->mOffset > 0)
            {
                DrawScreenText(InScreen, box_.x_min, Y,
                               "  \xe2\x86\x91 " +
                                   std::to_string(rpScrollState->mOffset) +
                                   " above",
                               Color::GrayDark, true);
            }
            ++Y;
        }

        const int EndRow = std::min(
            static_cast<int>(mRows.size()),
            rpScrollState->mOffset + std::max(0, mViewportHeight));
        for (int RowIndex = rpScrollState->mOffset; RowIndex < EndRow;
             ++RowIndex)
        {
            DrawCodeRow(InScreen, mRows[static_cast<size_t>(RowIndex)], Y);
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
                               "  \xe2\x86\x93 " + std::to_string(Below) +
                                   " below",
                               Color::GrayDark, true);
            }
        }
    }

  private:
    void DrawCodeRow(Screen &InScreen, const FCodeSnippetVisualRow &InRow,
                     const int InY) const
    {
        if (InY < box_.y_min || InY > box_.y_max)
        {
            return;
        }

        std::string Gutter(static_cast<size_t>(mGutterWidth), ' ');
        if (!InRow.mbContinuation)
        {
            const std::string Number = std::to_string(InRow.mLineNumber);
            const int Padding =
                std::max(0, mGutterWidth - 1 -
                                static_cast<int>(Number.size()));
            Gutter = std::string(static_cast<size_t>(Padding), ' ') + Number +
                     " ";
        }

        DrawScreenText(InScreen, box_.x_min, InY, Gutter, Color::GrayDark,
                       true);
        DrawScreenText(InScreen, mCodeX, InY, InRow.mText, Color::Default,
                       !InRow.mbCode);
    }

    std::vector<FCodeSnippetLogicalLine> mLines;
    std::vector<FCodeSnippetVisualRow> mRows;
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
// PhaseDetailPanel
// ---------------------------------------------------------------------------

Element PhaseDetailPanel::Render(const FWatchPlanSummary &InPlan,
                                 int InSelectedPhaseIndex,
                                 bool InMetricView,
                                 FWatchScrollRegionState &InOutScrollState) const
{
    if (InPlan.mTopicKey.empty())
    {
        return window(text(" PHASE DETAIL ") | bold,
                      text("No plan selected") | dim);
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

    const int PhaseColumnWidth =
        ComputeTextColumnWidth("P", PhaseLabels, 3);
    const int DefaultDesignColumnWidth = ComputeGaugeColumnWidth(
        "Design", DefaultDesignLabels, 16, kDesignBarPreferredWidth);
    const int TaxonomyColumnWidth =
        ComputeTextColumnWidth("Taxonomy", TaxonomyLabels, 30);
    const int MetricDesignColumnWidth = ComputeGaugeColumnWidth(
        "Design", MetricDesignLabels, 14, kMetricBarPreferredWidth);
    const int BloatColumnWidth =
        ComputeTextColumnWidth("Bloat", BloatLabels, 16);
    const int SolidColumnWidth = ComputeGaugeColumnWidth(
        "SOLID", SolidLabels, 14, kMetricBarPreferredWidth);
    const int WordsColumnWidth = ComputeGaugeColumnWidth(
        "Words", WordsLabels, 14, kMetricBarPreferredWidth);
    const int FieldsColumnWidth = ComputeGaugeColumnWidth(
        "Fields", FieldsLabels, 14, kMetricBarPreferredWidth);
    const int WorkColumnWidth = ComputeGaugeColumnWidth(
        "Work", WorkLabels, 14, kMetricBarPreferredWidth);
    const int TestsColumnWidth = ComputeGaugeColumnWidth(
        "Tests", TestsLabels, 14, kMetricBarPreferredWidth);
    const int FilesColumnWidth = ComputeGaugeColumnWidth(
        "Files", FilesLabels, 14, kMetricBarPreferredWidth);
    const int EvidenceColumnWidth = ComputeGaugeColumnWidth(
        "Evidence", EvidenceLabels, 14, kMetricBarPreferredWidth);

    // gridbox layout — auto-aligns columns without Table's flex_shrink override
    std::vector<Elements> GridRows;
    if (InMetricView)
    {
        GridRows.push_back(PadGridRow({
            text("P") | bold | size(WIDTH, EQUAL, PhaseColumnWidth),
            text("Status") | bold | size(WIDTH, EQUAL, 12),
            text("Design") | bold | size(WIDTH, EQUAL,
                                          MetricDesignColumnWidth),
            text("Bloat") | bold | size(WIDTH, EQUAL, BloatColumnWidth),
            text("SOLID") | bold | size(WIDTH, EQUAL, SolidColumnWidth),
            text("Words") | bold | size(WIDTH, EQUAL, WordsColumnWidth),
            text("Fields") | bold | size(WIDTH, EQUAL, FieldsColumnWidth),
            text("Work") | bold | size(WIDTH, EQUAL, WorkColumnWidth),
            text("Tests") | bold | size(WIDTH, EQUAL, TestsColumnWidth),
            text("Files") | bold | size(WIDTH, EQUAL, FilesColumnWidth),
            text("Evidence") | bold | size(WIDTH, EQUAL,
                                            EvidenceColumnWidth),
            text("Scope") | bold | flex,
        }));
    }
    else
    {
        GridRows.push_back(PadGridRow({
            text("P") | bold | size(WIDTH, EQUAL, PhaseColumnWidth),
            text("Status") | bold | size(WIDTH, EQUAL, 14),
            text("Design") | bold | size(WIDTH, EQUAL,
                                          DefaultDesignColumnWidth),
            text("Taxonomy") | bold | size(WIDTH, EQUAL,
                                            TaxonomyColumnWidth),
            text("Scope") | bold | flex,
            text("Output") | bold | flex,
        }));
    }

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

    // Newest phase first. The selected index and taxonomy lookups still use
    // the source phase index; only the visual order is reversed.
    for (int DisplayIndex = 0; DisplayIndex < Count; ++DisplayIndex)
    {
        const int Index = Count - DisplayIndex - 1;
        const PhaseItem &Phase = InPlan.mPhases[static_cast<size_t>(Index)];
        const bool Selected = (Index == InSelectedPhaseIndex);
        const std::string Marker;

        // Typed fields from FPhaseRecord — no fuzzy key matching.
        const std::string &Desc = Phase.mScope;
        const std::string &Output = Phase.mOutput;

        const std::string TaxSummary =
            BuildPhaseTaxonomySummary(InPlan.mPhaseTaxonomies, Index);

        Elements RowCells;
        Element PhaseKeyCell =
            text(Marker + Phase.mPhaseKey) |
            size(WIDTH, EQUAL, PhaseColumnWidth);
        if (Selected)
        {
            PhaseKeyCell = PhaseKeyCell | focus;
        }
        if (InMetricView)
        {
            const FPhaseRuntimeMetrics &Metrics = Phase.mMetrics;
            RowCells = PadGridRow({
                PhaseKeyCell,
                ColorStatus(ToString(Phase.mStatus)) | size(WIDTH, EQUAL, 12),
                MetricGaugeBar(Metrics.mDesignChars, DesignScale,
                               std::to_string(Metrics.mDesignChars),
                               MetricDesignColumnWidth) |
                    size(WIDTH, EQUAL, MetricDesignColumnWidth),
                BloatMetricText(Metrics) |
                    size(WIDTH, EQUAL, BloatColumnWidth),
                MetricGaugeBar(Metrics.mSolidWordCount, SolidScale,
                               std::to_string(Metrics.mSolidWordCount),
                               SolidColumnWidth) |
                    size(WIDTH, EQUAL, SolidColumnWidth),
                MetricGaugeBar(Metrics.mRecursiveWordCount, WordsScale,
                               std::to_string(Metrics.mRecursiveWordCount),
                               WordsColumnWidth) |
                    size(WIDTH, EQUAL, WordsColumnWidth),
                MetricGaugeBar(Metrics.mFieldCoveragePercent, FieldsScale,
                               std::to_string(Metrics.mFieldCoveragePercent) +
                                   "%",
                               FieldsColumnWidth) |
                    size(WIDTH, EQUAL, FieldsColumnWidth),
                MetricGaugeBar(Metrics.mWorkItemCount, WorkScale,
                               std::to_string(Metrics.mWorkItemCount),
                               WorkColumnWidth) |
                    size(WIDTH, EQUAL, WorkColumnWidth),
                MetricGaugeBar(Metrics.mTestingRecordCount, TestsScale,
                               std::to_string(Metrics.mTestingRecordCount),
                               TestsColumnWidth) |
                    size(WIDTH, EQUAL, TestsColumnWidth),
                MetricGaugeBar(Metrics.mFileManifestCount, FilesScale,
                               std::to_string(Metrics.mFileManifestCount),
                               FilesColumnWidth) |
                    size(WIDTH, EQUAL, FilesColumnWidth),
                MetricGaugeBar(Metrics.mEvidenceItemCount, EvidenceScale,
                               std::to_string(Metrics.mEvidenceItemCount),
                               EvidenceColumnWidth) |
                    size(WIDTH, EQUAL, EvidenceColumnWidth),
                text(Desc) | dim | flex,
            });
        }
        else
        {
            RowCells = PadGridRow({
                PhaseKeyCell,
                ColorStatus(ToString(Phase.mStatus)) | size(WIDTH, EQUAL, 14),
                DesignCharsBar(Phase.mV4DesignChars,
                               DefaultDesignColumnWidth) |
                    size(WIDTH, EQUAL, DefaultDesignColumnWidth),
                text(TaxSummary) | dim |
                    size(WIDTH, EQUAL, TaxonomyColumnWidth),
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
    FinalRows.push_back(gridbox(std::move(GridRows)) | flex);

    // v0.97.0 no-truncation contract applies here too — FTXUI's frame
    // handles overflow at the terminal boundary; the CLI layer emits
    // the verbatim topic key.
    const std::string Title =
        std::string(" [P]HASE DETAIL") + (InMetricView ? " METRICS: " : ": ") +
        InPlan.mTopicKey + " (" + std::to_string(Count) + ") ";
    return window(text(Title) | bold | color(Color::Cyan),
                  ScrollFrame(vbox(std::move(FinalRows)),
                              InOutScrollState)) |
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
                                       bool InFocusMode,
                                       FWatchScrollRegionState
                                           &InOutLaneScrollState) const
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

    const int LaneCount = static_cast<int>(Tax.mLanes.size());
    std::vector<Elements> LaneGridRows;
    LaneGridRows.push_back(PadGridRow({
        text("Lane") | bold,
        text("Status") | bold | size(WIDTH, EQUAL, 14),
        text("Scope") | bold | flex,
        text("Exit Criteria") | bold | flex,
    }));

    for (int Index = 0; Index < LaneCount; ++Index)
    {
        const FLaneRecord &Lane = Tax.mLanes[static_cast<size_t>(Index)];
        const bool LaneSel = (Index == InSelectedLaneIndex);
        const std::string Marker;

        Element LaneKeyCell = text(Marker + "L" + std::to_string(Index));
        if (LaneSel)
        {
            LaneKeyCell = LaneKeyCell | focus;
        }
        Elements RowCells = PadGridRow({
            LaneKeyCell,
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
    LaneFinalRows.push_back(gridbox(std::move(LaneGridRows)) | flex);

    auto LanesPanel =
        window(text(" [L]ANES: " + ("P" + std::to_string(Tax.mPhaseIndex)) +
                    " (" + std::to_string(LaneCount) + ") ") |
                   bold,
               ScrollFrame(vbox(std::move(LaneFinalRows)),
                           InOutLaneScrollState)) |
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

Element FileManifestPanel::Render(const FPhaseTaxonomy &InTaxonomy,
                                  FWatchScrollRegionState
                                      &InOutScrollState) const
{
    if (InTaxonomy.mFileManifest.empty())
    {
        InOutScrollState.Reset();
        return window(
            text(" FILES: " + ("P" + std::to_string(InTaxonomy.mPhaseIndex)) +
                 " (0) ") |
                bold,
            text("  (no file changes planned)") | dim);
    }

    const int TotalFiles = static_cast<int>(InTaxonomy.mFileManifest.size());

    std::vector<std::vector<Element>> TableData;
    TableData.push_back({text("File") | bold | flex, text("Action") | bold,
                         text("Description") | bold | flex});

    for (int Index = 0; Index < TotalFiles; ++Index)
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
        std::to_string(TotalFiles) + ") ";

    return window(text(Title) | bold,
                  ScrollFrame(FileTable.Render() | flex,
                              InOutScrollState));
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
                      text("  No plan selected") | dim);
    }

    if (InSelectedPhaseIndex < 0 ||
        InSelectedPhaseIndex >= static_cast<int>(InPlan.mPhases.size()))
    {
        InOutScrollState.Reset();
        return window(text(" CODE SNIPPETS: " + InPlan.mTopicKey + " ") |
                          bold | dim,
                      text("  No phase selected") | dim);
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
                      text("  (no code snippets)") | dim);
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
             "F12=code  [=up ]=down  w/W=wave  l/L=lane  f/F=files  "
             "r=refresh") |
            dim,
        text("  "),
    });
}

} // namespace UniPlan
