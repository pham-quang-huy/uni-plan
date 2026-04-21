#include "UniPlanCommandMutationCommon.h"

#include "UniPlanBundleWriteGuard.h"
#include "UniPlanEnums.h"
#include "UniPlanHelpers.h"
#include "UniPlanJSONIO.h"

#include <cstdlib>
#include <iostream>

namespace UniPlan
{

void EmitMutationJson(
    const std::string &InTopic, const std::string &InTarget,
    const std::vector<
        std::pair<std::string, std::pair<std::string, std::string>>> &InChanges,
    bool InAutoChangelog)
{
    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", InTopic);
    EmitJsonField("target", InTarget);
    std::cout << "\"changes\":[";
    for (size_t I = 0; I < InChanges.size(); ++I)
    {
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonField("field", InChanges[I].first);
        EmitJsonField("old", InChanges[I].second.first);
        EmitJsonField("new", InChanges[I].second.second, false);
        std::cout << "}";
    }
    std::cout << "],";
    EmitJsonFieldBool("auto_changelog", InAutoChangelog, false);
    std::cout << "}\n";
}

void AppendAutoChangelog(FTopicBundle &InOutBundle, const std::string &InTarget,
                         const std::string &InDescription)
{
    FChangeLogEntry Entry;
    static constexpr const char *kPhasePrefix = "phases[";
    static constexpr size_t kPhasePrefixLen = 7;
    if (InTarget.compare(0, kPhasePrefixLen, kPhasePrefix) == 0)
    {
        const size_t Close = InTarget.find(']', kPhasePrefixLen);
        if (Close != std::string::npos)
            Entry.mPhase = std::atoi(
                InTarget.substr(kPhasePrefixLen, Close - kPhasePrefixLen)
                    .c_str());
        else
            Entry.mPhase = -1;
    }
    else
    {
        Entry.mPhase = -1;
    }
    const std::string UTC = GetUtcNow();
    Entry.mDate = UTC.substr(0, 10);
    Entry.mChange = InDescription;
    Entry.mAffected = InTarget;
    Entry.mType = EChangeType::Chore;
    Entry.mActor = ETestingActor::AI;
    InOutBundle.mChangeLogs.push_back(std::move(Entry));
}

int WriteBundleBack(const FTopicBundle &InBundle,
                    const fs::path & /*InRepoRoot*/, std::string &OutError)
{
    if (InBundle.mBundlePath.empty())
    {
        OutError = "Bundle has no source path (was it loaded via "
                   "TryLoadBundleByTopic?)";
        return 1;
    }
    return GuardedWriteBundle(InBundle, OutError);
}

} // namespace UniPlan
