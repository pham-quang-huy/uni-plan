#pragma once

#include "UniPlanTypes.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace UniPlan
{

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

} // namespace UniPlan
