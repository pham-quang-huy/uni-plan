#pragma once

#include "UniPlanStringHelpers.h"

#include <string>
#include <vector>

namespace UniPlan
{

inline std::vector<std::string> SplitMarkdownTableRow(const std::string &InLine)
{
    std::string Working = InLine;
    if (!Working.empty() && Working.front() == '|')
    {
        Working.erase(Working.begin());
    }
    if (!Working.empty() && Working.back() == '|')
    {
        Working.pop_back();
    }

    // Backtick-aware pipe splitting: pipes inside backtick
    // spans are literal text, not column delimiters.
    std::vector<std::string> Cells;
    std::string Cell;
    bool InBacktick = false;
    size_t BacktickRunLength = 0;
    size_t OpenRunLength = 0;

    for (size_t Index = 0; Index < Working.size(); ++Index)
    {
        const char Character = Working[Index];

        if (Character == '`')
        {
            // Count consecutive backticks
            size_t RunLength = 1;
            while (Index + RunLength < Working.size() &&
                   Working[Index + RunLength] == '`')
            {
                ++RunLength;
            }
            if (!InBacktick)
            {
                InBacktick = true;
                OpenRunLength = RunLength;
            }
            else if (RunLength == OpenRunLength)
            {
                InBacktick = false;
                OpenRunLength = 0;
            }
            for (size_t Tick = 0; Tick < RunLength; ++Tick)
            {
                Cell += '`';
            }
            Index += RunLength - 1;
            continue;
        }

        if (Character == '|' && !InBacktick)
        {
            Cells.push_back(Trim(Cell));
            Cell.clear();
        }
        else
        {
            Cell += Character;
        }
    }

    if (!Cell.empty() || !Cells.empty())
    {
        Cells.push_back(Trim(Cell));
    }

    return Cells;
}

inline bool IsDividerCell(const std::string &InCell)
{
    if (InCell.empty())
    {
        return false;
    }
    for (const char Character : InCell)
    {
        if (Character != '-' && Character != ':' &&
            std::isspace(static_cast<unsigned char>(Character)) == 0)
        {
            return false;
        }
    }
    return true;
}

inline bool IsDividerRow(const std::vector<std::string> &InCells)
{
    if (InCells.empty())
    {
        return false;
    }
    for (const std::string &Cell : InCells)
    {
        if (!IsDividerCell(Cell))
        {
            return false;
        }
    }
    return true;
}

} // namespace UniPlan
