#pragma once

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

inline void PrintHumanWarnings(const std::vector<std::string> &InWarnings)
{
    for (const std::string &Warning : InWarnings)
    {
        std::cout << Colorize(kColorYellow, "WARN") << " " << Warning << "\n";
    }
}

inline std::string TruncateForDisplay(const std::string &InText,
                                      size_t InMaxWidth)
{
    if (InMaxWidth == 0 || InText.size() <= InMaxWidth)
    {
        return InText;
    }
    if (InMaxWidth <= 3)
    {
        return InText.substr(0, InMaxWidth);
    }
    return InText.substr(0, InMaxWidth - 3) + "...";
}

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
