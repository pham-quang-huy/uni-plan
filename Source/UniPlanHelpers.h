#pragma once

#include "UniPlanTypes.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <climits>
#include <mach-o/dyld.h>
#endif

namespace UniPlan
{

// ---------------------------------------------------------------------------
// String utilities
// ---------------------------------------------------------------------------

inline std::string ToLower(std::string InValue)
{
    std::transform(InValue.begin(), InValue.end(), InValue.begin(),
                   [](const unsigned char InCharacter)
                   { return static_cast<char>(std::tolower(InCharacter)); });
    return InValue;
}

inline std::string Trim(const std::string &InValue)
{
    size_t Start = 0;
    while (Start < InValue.size() &&
           std::isspace(static_cast<unsigned char>(InValue[Start])) != 0)
    {
        ++Start;
    }
    size_t End = InValue.size();
    while (End > Start &&
           std::isspace(static_cast<unsigned char>(InValue[End - 1])) != 0)
    {
        --End;
    }
    return InValue.substr(Start, End - Start);
}

inline std::string ToGenericPath(const fs::path &InPath)
{
    return InPath.generic_string();
}

inline bool EndsWith(const std::string &InValue, const std::string &InSuffix)
{
    if (InSuffix.size() > InValue.size())
    {
        return false;
    }
    return InValue.compare(InValue.size() - InSuffix.size(), InSuffix.size(),
                           InSuffix) == 0;
}

inline std::string JoinCommaSeparated(const std::vector<std::string> &InValues)
{
    std::ostringstream Stream;
    for (size_t Index = 0; Index < InValues.size(); ++Index)
    {
        if (Index > 0)
        {
            Stream << ",";
        }
        Stream << InValues[Index];
    }
    return Stream.str();
}

inline std::string GetUtcNow()
{
    const auto Now = std::chrono::system_clock::now();
    const std::time_t Timestamp = std::chrono::system_clock::to_time_t(Now);
    std::tm UtcTime{};
#ifdef _WIN32
    gmtime_s(&UtcTime, &Timestamp);
#else
    gmtime_r(&Timestamp, &UtcTime);
#endif
    std::ostringstream Stream;
    Stream << std::put_time(&UtcTime, "%Y-%m-%dT%H:%M:%SZ");
    return Stream.str();
}

// ---------------------------------------------------------------------------
// Warning / status helpers
// ---------------------------------------------------------------------------

inline void AddWarning(std::vector<std::string> &OutWarnings,
                       const std::string &InMessage)
{
    OutWarnings.push_back(InMessage);
}

inline void NormalizeWarnings(std::vector<std::string> &InOutWarnings)
{
    std::sort(InOutWarnings.begin(), InOutWarnings.end());
    InOutWarnings.erase(std::unique(InOutWarnings.begin(), InOutWarnings.end()),
                        InOutWarnings.end());
}

inline const std::string &GetDisplayStatus(const std::string &InStatus)
{
    static const std::string kFallback = "unknown";
    return InStatus.empty() ? kFallback : InStatus;
}

inline std::string NormalizeStatusValue(const std::string &InRawValue)
{
    std::string Value = ToLower(Trim(InRawValue));
    // Strip backticks (e.g., "`completed`" → "completed")
    if (Value.size() >= 2 && Value.front() == '`' && Value.back() == '`')
    {
        Value = Value.substr(1, Value.size() - 2);
    }
    for (char &Character : Value)
    {
        if (Character == '_' || Character == '-')
        {
            Character = ' ';
        }
    }
    if (Value.find("not started") != std::string::npos || Value == "pending" ||
        Value == "todo")
    {
        return "not_started";
    }
    if (Value.find("in progress") != std::string::npos || Value == "active" ||
        Value == "ongoing" || Value == "started" ||
        (Value.find("started") != std::string::npos &&
         Value.find("not started") == std::string::npos) ||
        Value.find("partial") != std::string::npos ||
        Value.find("deferred") != std::string::npos ||
        Value.find("investigated") != std::string::npos)
    {
        return "in_progress";
    }
    if (Value.find("complete") != std::string::npos || Value == "done")
    {
        return "completed";
    }
    if (Value.find("closed") != std::string::npos)
    {
        return "closed";
    }
    if (Value.find("blocked") != std::string::npos)
    {
        return "blocked";
    }
    if (Value.find("canceled") != std::string::npos ||
        Value.find("cancelled") != std::string::npos)
    {
        return "canceled";
    }
    return "unknown";
}

inline bool MatchesStatusFilter(const std::string &InFilter,
                                const std::string &InStatus)
{
    if (InFilter == "all")
    {
        return true;
    }
    if (!InFilter.empty() && InFilter[0] == '!')
    {
        const std::string Excluded = InFilter.substr(1);
        return InStatus != Excluded;
    }
    return InFilter == InStatus;
}

inline void AddStatusCandidate(StatusCounters &OutCounters,
                               const std::string &InRawValue)
{
    const std::string RawValue = Trim(InRawValue);
    if (RawValue.empty())
    {
        return;
    }
    const std::string Normalized = NormalizeStatusValue(RawValue);
    if (Normalized == "unknown")
    {
        return;
    }
    if (OutCounters.mFirstRaw.empty())
    {
        OutCounters.mFirstRaw = RawValue;
    }
    if (Normalized == "not_started")
    {
        OutCounters.mNotStarted += 1;
        return;
    }
    if (Normalized == "in_progress")
    {
        OutCounters.mInProgress += 1;
        return;
    }
    if (Normalized == "completed")
    {
        OutCounters.mCompleted += 1;
        return;
    }
    if (Normalized == "closed")
    {
        OutCounters.mClosed += 1;
        return;
    }
    if (Normalized == "blocked")
    {
        OutCounters.mBlocked += 1;
        return;
    }
    if (Normalized == "canceled")
    {
        OutCounters.mCanceled += 1;
    }
}

inline std::string ResolveNormalizedStatus(const StatusCounters &InCounters)
{
    if (InCounters.mBlocked > 0)
    {
        return "blocked";
    }
    if (InCounters.mInProgress > 0)
    {
        return "in_progress";
    }
    if (InCounters.mNotStarted > 0 &&
        (InCounters.mCompleted > 0 || InCounters.mClosed > 0))
    {
        return "in_progress";
    }
    if (InCounters.mNotStarted > 0)
    {
        return "not_started";
    }
    if (InCounters.mCompleted > 0)
    {
        return "completed";
    }
    if (InCounters.mClosed > 0)
    {
        return "closed";
    }
    if (InCounters.mCanceled > 0)
    {
        return "canceled";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------

inline bool TryReadFileLines(const fs::path &InPath,
                             std::vector<std::string> &OutLines,
                             std::string &OutError)
{
    std::ifstream Input(InPath);
    if (!Input.is_open())
    {
        OutError = "Unable to open file.";
        return false;
    }
    std::string Line;
    while (std::getline(Input, Line))
    {
        OutLines.push_back(Line);
    }
    if (Input.bad())
    {
        OutError = "File read failure.";
        return false;
    }
    return true;
}

inline void PrintRepoInfo(const fs::path &InRepoRoot)
{
    std::cerr << "[repo: " << InRepoRoot.string() << "]\n";
}

inline void PrintScanInfo(size_t InDocCount)
{
    std::cerr << "[scanned " << InDocCount << " docs]\n";
}

// ---------------------------------------------------------------------------
// Markdown table utilities
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// JSON output helpers
// ---------------------------------------------------------------------------

inline std::string JsonEscape(const std::string &InValue)
{
    std::ostringstream Stream;
    for (const char Character : InValue)
    {
        switch (Character)
        {
        case '\\':
            Stream << "\\\\";
            break;
        case '"':
            Stream << "\\\"";
            break;
        case '\n':
            Stream << "\\n";
            break;
        case '\r':
            Stream << "\\r";
            break;
        case '\t':
            Stream << "\\t";
            break;
        default:
            Stream << Character;
            break;
        }
    }
    return Stream.str();
}

inline std::string JsonQuote(const std::string &InValue)
{
    return "\"" + JsonEscape(InValue) + "\"";
}
inline std::string JsonNullOrQuote(const std::string &InValue)
{
    return InValue.empty() ? "null" : JsonQuote(InValue);
}

inline void PrintJsonHeader(const char *InSchema, const std::string &InUtc,
                            const std::string &InRoot)
{
    std::cout << "{\"schema\":" << JsonQuote(InSchema)
              << ",\"generated_utc\":" << JsonQuote(InUtc)
              << ",\"repo_root\":" << JsonQuote(InRoot) << ",";
}

inline void PrintJsonSep(const size_t InIndex)
{
    if (InIndex > 0)
    {
        std::cout << ",";
    }
}

inline void PrintJsonStringArray(const char *InName,
                                 const std::vector<std::string> &InItems)
{
    std::cout << "\"" << InName << "\":[";
    for (size_t Index = 0; Index < InItems.size(); ++Index)
    {
        if (Index > 0)
        {
            std::cout << ",";
        }
        std::cout << JsonQuote(InItems[Index]);
    }
    std::cout << "]";
}

inline void PrintJsonWarnings(const std::vector<std::string> &InWarnings)
{
    PrintJsonStringArray("warnings", InWarnings);
}
inline void PrintJsonClose(const std::vector<std::string> &InWarnings)
{
    PrintJsonWarnings(InWarnings);
    std::cout << "}\n";
}

inline void EmitJsonField(const char *InName, const std::string &InValue,
                          bool InTrailingComma = true)
{
    std::cout << "\"" << InName << "\":" << JsonQuote(InValue);
    if (InTrailingComma)
    {
        std::cout << ",";
    }
}

inline void EmitJsonFieldNullable(const char *InName,
                                  const std::string &InValue,
                                  bool InTrailingComma = true)
{
    std::cout << "\"" << InName << "\":" << JsonNullOrQuote(InValue);
    if (InTrailingComma)
    {
        std::cout << ",";
    }
}

inline void EmitJsonFieldInt(const char *InName, int InValue,
                             bool InTrailingComma = true)
{
    std::cout << "\"" << InName << "\":" << InValue;
    if (InTrailingComma)
    {
        std::cout << ",";
    }
}

inline void EmitJsonFieldBool(const char *InName, bool InValue,
                              bool InTrailingComma = true)
{
    std::cout << "\"" << InName << "\":" << (InValue ? "true" : "false");
    if (InTrailingComma)
    {
        std::cout << ",";
    }
}

inline void EmitJsonFieldSizeT(const char *InName, size_t InValue,
                               bool InTrailingComma = true)
{
    std::cout << "\"" << InName << "\":" << InValue;
    if (InTrailingComma)
    {
        std::cout << ",";
    }
}

// ---------------------------------------------------------------------------
// Text output helpers
// ---------------------------------------------------------------------------

inline void PrintTextWarnings(const std::vector<std::string> &InWarnings)
{
    for (const std::string &Warning : InWarnings)
    {
        std::cout << "WARN " << Warning << "\n";
    }
}

// ---------------------------------------------------------------------------
// Human-mode helpers
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Inventory helpers
// ---------------------------------------------------------------------------

inline std::map<std::string, std::string>
BuildTopicPairStateMap(const Inventory &InInventory)
{
    std::map<std::string, std::string> Result;
    for (const TopicPairRecord &Pair : InInventory.mPairs)
    {
        Result[Pair.mTopicKey] = Pair.mPairState;
    }
    return Result;
}

inline std::set<std::string>
BuildTopicSet(const std::vector<DocumentRecord> &InRecords)
{
    std::set<std::string> Topics;
    for (const DocumentRecord &Record : InRecords)
    {
        Topics.insert(Record.mTopicKey);
    }
    return Topics;
}

// ---------------------------------------------------------------------------
// Executable directory (resolves symlinks)
// ---------------------------------------------------------------------------

inline fs::path GetExecutableDirectory()
{
#ifdef __APPLE__
    char RawPath[4096];
    uint32_t Size = sizeof(RawPath);
    if (_NSGetExecutablePath(RawPath, &Size) == 0)
    {
        char Resolved[PATH_MAX];
        if (realpath(RawPath, Resolved) != nullptr)
        {
            return fs::path(Resolved).parent_path();
        }
    }
#endif
    // Fallback: current working directory
    return fs::current_path();
}

} // namespace UniPlan
