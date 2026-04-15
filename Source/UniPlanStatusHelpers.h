#pragma once

#include "UniPlanStringHelpers.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <string>
#include <vector>

namespace UniPlan
{

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
    // Strip backticks (e.g., "`completed`" -> "completed")
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

} // namespace UniPlan
