#include "UniPlanCommandMutationCommon.h"
#include "UniPlanEnums.h"
#include "UniPlanFileHelpers.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanJSONIO.h"
#include "UniPlanJSONLineIndex.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <fstream>
#include <unordered_map>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace UniPlan
{


// ---------------------------------------------------------------------------
// TryLoadBundleByTopic — recursive search for <TopicKey>.Plan.json
// ---------------------------------------------------------------------------

bool TryLoadBundleByTopic(const fs::path &InRepoRoot,
                          const std::string &InTopicKey,
                          FTopicBundle &OutBundle, std::string &OutError)
{
    const std::string TargetName = InTopicKey + ".Plan.json";
    std::error_code EC;
    for (auto Iterator = fs::recursive_directory_iterator(InRepoRoot, EC);
         Iterator != fs::recursive_directory_iterator(); ++Iterator)
    {
        if (EC)
            break;
        if (Iterator->is_directory() &&
            ShouldSkipRecursionDirectory(Iterator->path()))
        {
            Iterator.disable_recursion_pending();
            continue;
        }
        if (!Iterator->is_regular_file())
            continue;
        if (Iterator->path().filename().string() != TargetName)
            continue;
        if (!TryReadTopicBundle(Iterator->path(), OutBundle, OutError))
            return false;
        OutBundle.mBundlePath = Iterator->path().string();
        return true;
    }
    OutError = "Bundle not found: " + TargetName +
               " (searched recursively from " + InRepoRoot.string() + ")";
    return false;
}

// ---------------------------------------------------------------------------
// LoadAllBundles — recursive scan for all *.Plan.json
// ---------------------------------------------------------------------------

std::vector<FTopicBundle> LoadAllBundles(const fs::path &InRepoRoot,
                                         std::vector<std::string> &OutWarnings)
{
    std::vector<FTopicBundle> Bundles;
    static const std::regex BundleRegex(R"(^([A-Za-z0-9]+)\.Plan\.json$)");

    std::error_code EC;
    for (auto Iterator = fs::recursive_directory_iterator(InRepoRoot, EC);
         Iterator != fs::recursive_directory_iterator(); ++Iterator)
    {
        if (EC)
            break;
        if (Iterator->is_directory() &&
            ShouldSkipRecursionDirectory(Iterator->path()))
        {
            Iterator.disable_recursion_pending();
            continue;
        }
        if (!Iterator->is_regular_file())
            continue;
        const std::string Filename = Iterator->path().filename().string();
        std::smatch Match;
        if (!std::regex_match(Filename, Match, BundleRegex))
            continue;
        FTopicBundle Bundle;
        std::string Error;
        if (TryReadTopicBundle(Iterator->path(), Bundle, Error))
        {
            Bundle.mBundlePath = Iterator->path().string();
            Bundles.push_back(std::move(Bundle));
        }
        else
        {
            OutWarnings.push_back("Failed to read " + Filename + ": " + Error);
        }
    }

    // Sort by topic key for deterministic output
    std::sort(Bundles.begin(), Bundles.end(),
              [](const FTopicBundle &A, const FTopicBundle &B)
              { return A.mTopicKey < B.mTopicKey; });
    return Bundles;
}




} // namespace UniPlan
