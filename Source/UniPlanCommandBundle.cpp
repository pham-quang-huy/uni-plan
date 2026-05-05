#include "UniPlanBundleWriteGuard.h"
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

enum class EBundleLoadProbeResult
{
    Missing,
    Loaded,
    Failed
};

static EBundleLoadProbeResult TryLoadBundleFile(const fs::path &InPath,
                                                FTopicBundle &OutBundle,
                                                std::string &OutError)
{
    std::error_code EC;
    if (!fs::is_regular_file(InPath, EC))
    {
        return EBundleLoadProbeResult::Missing;
    }

    if (!TryReadTopicBundle(InPath, OutBundle, OutError))
    {
        return EBundleLoadProbeResult::Failed;
    }

    OutBundle.mBundlePath = InPath.string();
    std::string SessionError;
    if (!CaptureReadSession(InPath, OutBundle.mReadSession, SessionError))
    {
        OutError = "Bundle read-session capture failed: " + SessionError;
        return EBundleLoadProbeResult::Failed;
    }
    return EBundleLoadProbeResult::Loaded;
}

// ---------------------------------------------------------------------------
// TryLoadBundleByTopic — canonical direct lookup, then recursive fallback
// ---------------------------------------------------------------------------

bool TryLoadBundleByTopic(const fs::path &InRepoRoot,
                          const std::string &InTopicKey,
                          FTopicBundle &OutBundle, std::string &OutError)
{
    const std::string TargetName = InTopicKey + ".Plan.json";
    const fs::path CanonicalPath = InRepoRoot / "Docs" / "Plans" / TargetName;

    const EBundleLoadProbeResult DirectResult =
        TryLoadBundleFile(CanonicalPath, OutBundle, OutError);
    if (DirectResult == EBundleLoadProbeResult::Loaded)
    {
        return true;
    }
    if (DirectResult == EBundleLoadProbeResult::Failed)
    {
        return false;
    }

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
        const EBundleLoadProbeResult Result =
            TryLoadBundleFile(Iterator->path(), OutBundle, OutError);
        if (Result == EBundleLoadProbeResult::Loaded)
        {
            return true;
        }
        return false;
    }
    OutError = "Bundle not found: " + TargetName +
               " (searched recursively from " + InRepoRoot.string() + ")";
    return false;
}

// ---------------------------------------------------------------------------
// CollectBundleBlockers — single source of truth for blocker detection.
//
// A phase contributes a blocker when EITHER:
//   - its lifecycle status is Blocked, OR
//   - its lifecycle blockers text is non-placeholder (not empty, "none",
//     "n/a", or "-" — case-insensitive).
//
// Both conditions can fire at once; that is still one BlockerItem per
// phase. Emitted BlockerItem carries topic key, source bundle path,
// phase index, status string, blocker text, and a kind tag describing
// whether it was detected via status or via text.
// ---------------------------------------------------------------------------

static bool HasRealBlockerText(const std::string &InText)
{
    if (InText.empty())
        return false;
    std::string Lower;
    Lower.reserve(InText.size());
    for (char C : InText)
        Lower += static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
    return Lower != "none" && Lower != "none." && Lower != "n/a" &&
           Lower != "-";
}

std::vector<BlockerItem> CollectBundleBlockers(const FTopicBundle &InBundle)
{
    std::vector<BlockerItem> Blockers;
    for (size_t I = 0; I < InBundle.mPhases.size(); ++I)
    {
        const FPhaseRecord &Phase = InBundle.mPhases[I];
        const bool bStatusBlocked =
            Phase.mLifecycle.mStatus == EExecutionStatus::Blocked;
        const bool bHasText = HasRealBlockerText(Phase.mLifecycle.mBlockers);
        if (!bStatusBlocked && !bHasText)
            continue;

        BlockerItem Item;
        Item.mTopicKey = InBundle.mTopicKey;
        Item.mSourcePath = InBundle.mBundlePath;
        Item.mPhaseIndex = static_cast<int>(I);
        Item.mStatus = ToString(Phase.mLifecycle.mStatus);
        Item.mKind =
            bStatusBlocked ? (bHasText ? "status+text" : "status") : "text";
        Item.mAction = Phase.mLifecycle.mBlockers;
        Item.mNotes = Phase.mScope;
        Blockers.push_back(std::move(Item));
    }
    return Blockers;
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
            // Advisory: capture failure leaves mReadSession.mbValid=false,
            // which falls back to lock-only protection in GuardedWriteBundle.
            std::string IgnoredSessionErr;
            CaptureReadSession(Iterator->path(), Bundle.mReadSession,
                               IgnoredSessionErr);
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
