#pragma once

#include "UniPlanTopicTypes.h" // kPhaseHollowChars, kPhaseRichMinChars
#include "UniPlanTypes.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace UniPlan
{

inline void PrintTextWarnings(const std::vector<std::string> &InWarnings)
{
    for (const std::string &Warning : InWarnings)
    {
        std::cout << "WARN " << Warning << "\n";
    }
}

inline std::string Colorize(const char *InColor, const std::string &InText)
{
    return std::string(InColor) + InText + kColorReset;
}

inline std::string ColorBold(const std::string &InText)
{
    return std::string(kColorBold) + InText + kColorReset;
}

inline std::string ColorizeStatus(const std::string &InStatus)
{
    if (InStatus == "completed" || InStatus == "ok" || InStatus == "paired" ||
        InStatus == "closed" || InStatus == "in_progress" ||
        InStatus == "started")
    {
        return Colorize(kColorGreen, InStatus);
    }
    if (InStatus == "failed" || InStatus == "missing_implementation" ||
        InStatus == "missing_plan" || InStatus == "orphaned" ||
        InStatus == "missing_phase_playbook")
    {
        return Colorize(kColorRed, InStatus);
    }
    if (InStatus == "blocked" || InStatus == "unknown")
    {
        return Colorize(kColorYellow, InStatus);
    }
    return InStatus;
}

// Render a design-char count as a colored human-mode cell, using the
// shared hollow / thin / rich thresholds. Same color scheme as the
// watch TUI `Design` column so phase list, phase get, topic status, and
// the watch dashboard all agree visually on authored-plan depth.
//   < kPhaseHollowChars    — red    ("hollow", phase needs more authoring)
//   [hollow, rich) chars   — yellow ("thin", executable but sparse)
//   ≥ kPhaseRichMinChars   — green  ("rich", properly detailed)
inline std::string ColorizeDesignChars(size_t InChars)
{
    const char *Color = kColorGreen;
    if (InChars < kPhaseHollowChars)
        Color = kColorRed;
    else if (InChars < kPhaseRichMinChars)
        Color = kColorYellow;
    return Colorize(Color, std::to_string(InChars));
}

// One-word category label for the same thresholds. Useful in compact
// single-phase views (e.g. `phase get --human`) where a word reads
// clearer than a raw number.
inline const char *GetDesignDepthLabel(size_t InChars)
{
    if (InChars < kPhaseHollowChars)
        return "hollow";
    if (InChars < kPhaseRichMinChars)
        return "thin";
    return "rich";
}

inline void PrintHumanWarnings(const std::vector<std::string> &InWarnings)
{
    for (const std::string &Warning : InWarnings)
    {
        std::cout << Colorize(kColorYellow, "WARN") << " " << Warning << "\n";
    }
}

// TruncateForDisplay (removed in v0.97.0) — the CLI no longer
// truncates prose anywhere. Every query and mutation emits byte-
// identical stored content in both JSON and --human paths. If an
// operator or agent needs a preview they perform the trim at the
// consumer end.

struct HumanTable
{
    std::vector<std::string> mHeaders;
    std::vector<std::vector<std::string>> mRows;

    void AddRow(const std::vector<std::string> &InRow)
    {
        mRows.push_back(InRow);
    }

    static size_t VisibleWidth(const std::string &InText)
    {
        size_t Width = 0;
        bool InEscape = false;
        for (const char Char : InText)
        {
            if (Char == '\033')
            {
                InEscape = true;
                continue;
            }
            if (InEscape)
            {
                if (Char == 'm')
                    InEscape = false;
                continue;
            }
            Width += 1;
        }
        return Width;
    }

    void Print() const
    {
        if (mHeaders.empty())
            return;
        const size_t ColumnCount = mHeaders.size();
        std::vector<size_t> Widths(ColumnCount, 0);
        for (size_t Col = 0; Col < ColumnCount; ++Col)
            Widths[Col] = VisibleWidth(mHeaders[Col]);
        for (const auto &Row : mRows)
        {
            for (size_t Col = 0; Col < Row.size() && Col < ColumnCount; ++Col)
            {
                Widths[Col] = (std::max)(Widths[Col], VisibleWidth(Row[Col]));
            }
        }
        PrintRow(mHeaders, Widths, true);
        PrintSeparator(Widths);
        for (const auto &Row : mRows)
            PrintRow(Row, Widths, false);
    }

  private:
    void PrintRow(const std::vector<std::string> &InCells,
                  const std::vector<size_t> &InWidths, const bool InBold) const
    {
        for (size_t Col = 0; Col < InWidths.size(); ++Col)
        {
            if (Col > 0)
                std::cout << "  ";
            const std::string Cell = (Col < InCells.size()) ? InCells[Col] : "";
            const size_t Visible = VisibleWidth(Cell);
            if (InBold)
                std::cout << kColorBold << Cell << kColorReset;
            else
                std::cout << Cell;
            if (Visible < InWidths[Col])
                std::cout << std::string(InWidths[Col] - Visible, ' ');
        }
        std::cout << "\n";
    }

    void PrintSeparator(const std::vector<size_t> &InWidths) const
    {
        for (size_t Col = 0; Col < InWidths.size(); ++Col)
        {
            if (Col > 0)
                std::cout << "  ";
            std::cout << std::string(InWidths[Col], '-');
        }
        std::cout << "\n";
    }
};

} // namespace UniPlan
