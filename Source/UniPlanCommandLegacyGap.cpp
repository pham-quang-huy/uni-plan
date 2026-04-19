#include "UniPlanCommandMutationCommon.h"
#include "UniPlanFileHelpers.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanJSONHelpers.h"
#include "UniPlanLegacyDiscovery.h"
#include "UniPlanStringHelpers.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace UniPlan
{

// ---------------------------------------------------------------------------
// legacy-gap
//
// Stateless V3 ↔ V4 parity audit. Discovers legacy `.md` artifacts at
// invoke time via filename convention, matches them against V4 bundle
// phases, and buckets each phase into an `EPhaseGapCategory`. The
// bundle carries no path-based legacy index — the semantic `origin`
// stamp is sufficient provenance. When the legacy corpus is deleted,
// every phase falls into `legacy_absent` / `v4_only`, which is the
// correct steady state.
// ---------------------------------------------------------------------------

// Discovery types + helpers live in `UniPlanLegacyDiscovery.{h,cpp}`.
// Watch mode used to share them (v0.78.0–0.79.0, for the now-removed
// PB / PBLines columns); v0.80.0 reverted watch to pure V4 projection,
// leaving `legacy-gap` as the sole consumer.

// ---------------------------------------------------------------------------
// Categorization thresholds — agents choose rebuild strategy based on the
// resulting EPhaseGapCategory. Documented next to EPhaseGapCategory in
// UniPlanEnums.h.
//
// Calibrated to the V3 Playbook.md discipline: a proper per-phase
// playbook carried 200+ lines of content, ~400 for a comprehensive
// one. At ~80 chars/line that's 16000 / 32000 V4 chars. The LOC-form
// thresholds (kLegacyRichMinLoc / kLegacyThinMinLoc) and the chars-form
// thresholds (kV4HollowMaxChars = kPhaseHollowChars / kV4RichMinChars =
// kPhaseRichMinChars) are kept in lockstep so legacy and V4 phases
// fall into the same hollow / thin / rich buckets.
//
// Bumped in v0.80.0. Prior values (50 / 150 LOC, 500 / 2000 chars)
// counted bare-skeleton phases as "authored," which is what caused the
// PB-col always-✓ false-positive that motivated the column's removal.
// ---------------------------------------------------------------------------

static constexpr int kLegacyRichMinLoc = 200;
static constexpr int kLegacyThinMinLoc = 50;
static constexpr size_t kV4HollowMaxChars = kPhaseHollowChars; // 4000
static constexpr size_t kV4RichMinChars = kPhaseRichMinChars;  // 16000
static constexpr size_t kV4RichMinJobs = 3;

static EPhaseGapCategory CategorizePhase(int InLegacyLoc, size_t InV4Chars,
                                         size_t InV4Jobs,
                                         EExecutionStatus InStatus)
{
    const bool bHasLegacy = InLegacyLoc > 0;
    if (!bHasLegacy)
    {
        if (InV4Chars >= kV4RichMinChars && InV4Jobs >= kV4RichMinJobs)
        {
            return EPhaseGapCategory::V4Only;
        }
        return EPhaseGapCategory::LegacyAbsent;
    }
    if (InLegacyLoc < kLegacyThinMinLoc)
    {
        if (InStatus == EExecutionStatus::Completed &&
            InV4Chars < kV4HollowMaxChars)
        {
            return EPhaseGapCategory::HollowBoth;
        }
        return EPhaseGapCategory::LegacyStub;
    }
    if (InLegacyLoc >= kLegacyRichMinLoc)
    {
        if (InV4Chars < kV4HollowMaxChars)
        {
            return EPhaseGapCategory::LegacyRich;
        }
        if (InV4Chars >= kV4RichMinChars)
        {
            return EPhaseGapCategory::LegacyRichMatched;
        }
        return EPhaseGapCategory::LegacyThin;
    }
    (void)kLegacyThinMinLoc;
    return EPhaseGapCategory::LegacyThin;
}

static FPhaseGapRow
BuildPhaseGapRow(const fs::path &InRepoRoot, const FTopicBundle &InBundle,
                 int InPhaseIndex,
                 const std::vector<FLegacyDiscoveryHit> &InDiscoveredHits,
                 const std::map<std::string, int> &InKeyToIndex)
{
    FPhaseGapRow Row;
    Row.mTopic = InBundle.mTopicKey;
    Row.mPhaseIndex = InPhaseIndex;
    const FPhaseRecord &P = InBundle.mPhases[InPhaseIndex];
    Row.mPhaseStatus = P.mLifecycle.mStatus;
    // `ComputePhaseDesignChars` moved to UniPlanTopicTypes.h so the watch
    // TUI can use the same measure via a shared helper, not a copy-paste.
    Row.mV4DesignChars = ComputePhaseDesignChars(P);
    Row.mV4JobsCount = P.mJobs.size();

    // Walk the discovered hits and find the first Playbook entry that
    // resolves to this phase index via the key-to-index map. The bundle
    // carries no path index — discovery is purely filesystem-driven.
    for (const FLegacyDiscoveryHit &H : InDiscoveredHits)
    {
        if (!H.mbPerPhase || H.mKind != ELegacyArtifactKind::Playbook)
        {
            continue;
        }
        auto It = InKeyToIndex.find(H.mPhaseKey);
        if (It == InKeyToIndex.end() || It->second != InPhaseIndex)
        {
            continue;
        }
        Row.mLegacyPath = fs::relative(H.mPath, InRepoRoot).generic_string();
        Row.mLegacyLoc = LegacyMdContentLineCount(H.mPath.string());
        Row.mCategory = CategorizePhase(Row.mLegacyLoc, Row.mV4DesignChars,
                                        Row.mV4JobsCount, Row.mPhaseStatus);
        return Row;
    }
    Row.mCategory = CategorizePhase(0, Row.mV4DesignChars, Row.mV4JobsCount,
                                    Row.mPhaseStatus);
    return Row;
}

int RunLegacyGapCommand(const std::vector<std::string> &InArgs,
                        const std::string &InRepoRoot)
{
    FLegacyGapOptions Options = ParseLegacyGapOptions(InArgs);
    const fs::path RepoRoot = Options.mRepoRoot.empty()
                                  ? fs::path(InRepoRoot)
                                  : fs::path(Options.mRepoRoot);

    std::vector<std::string> BundleWarnings;
    std::vector<FTopicBundle> Bundles =
        LoadAllBundles(RepoRoot, BundleWarnings);

    FLegacyGapReport Report;
    Report.mGeneratedUtc = GetUtcNow();
    Report.mRepoRoot = RepoRoot.string();
    Report.mBundleCount = static_cast<int>(Bundles.size());
    Report.mWarnings = std::move(BundleWarnings);

    for (const FTopicBundle &B : Bundles)
    {
        if (!Options.mTopic.empty() && B.mTopicKey != Options.mTopic)
        {
            continue;
        }
        const std::vector<FLegacyDiscoveryHit> DiscoveredHits =
            DiscoverLegacyArtifactsForTopic(RepoRoot, B.mTopicKey);
        const std::map<std::string, int> KeyToIndex = ResolvePhaseKeyToIndex(
            DiscoveredHits, B.mPhases.size(), Report.mWarnings);

        for (size_t I = 0; I < B.mPhases.size(); ++I)
        {
            FPhaseGapRow Row = BuildPhaseGapRow(
                RepoRoot, B, static_cast<int>(I), DiscoveredHits, KeyToIndex);
            if (Options.opCategory.has_value() &&
                Row.mCategory != *Options.opCategory)
            {
                continue;
            }
            Report.mRows.push_back(std::move(Row));
        }
    }

    if (Options.mbHuman)
    {
        PrintRepoInfo(RepoRoot);
        std::map<EPhaseGapCategory, int> Counts;
        for (const FPhaseGapRow &R : Report.mRows)
        {
            ++Counts[R.mCategory];
        }
        std::cout << "\n"
                  << kColorBold << "Legacy-V3 <-> V4 parity audit"
                  << kColorReset << " (" << Report.mRows.size()
                  << " phases across " << Report.mBundleCount << " topics)\n\n";
        std::cout << "  Category counts\n";
        const EPhaseGapCategory kOrdered[] = {
            EPhaseGapCategory::LegacyRich,
            EPhaseGapCategory::HollowBoth,
            EPhaseGapCategory::LegacyThin,
            EPhaseGapCategory::LegacyStub,
            EPhaseGapCategory::LegacyAbsent,
            EPhaseGapCategory::LegacyRichMatched,
            EPhaseGapCategory::V4Only,
            EPhaseGapCategory::Drift,
        };
        for (EPhaseGapCategory C : kOrdered)
        {
            const int N = Counts.count(C) ? Counts[C] : 0;
            std::cout << "    " << std::string(ToString(C)) << ": " << N
                      << "\n";
        }
        std::cout << "\n";

        std::cout << "  " << kColorBold << std::left << std::setw(30) << "Topic"
                  << std::right << std::setw(4) << "Idx"
                  << "  " << std::left << std::setw(13) << "Status"
                  << std::right << std::setw(7) << "Legacy" << std::setw(10)
                  << "V4 chars" << std::setw(9) << "V4 jobs"
                  << "  "
                  << "Category" << kColorReset << "\n";
        std::cout << "  " << std::string(90, '-') << "\n";
        for (const FPhaseGapRow &R : Report.mRows)
        {
            const char *Color = "";
            const char *Reset = "";
            if (R.mCategory == EPhaseGapCategory::LegacyRich ||
                R.mCategory == EPhaseGapCategory::HollowBoth)
            {
                Color = kColorYellow;
                Reset = kColorReset;
            }
            std::cout << "  " << Color << std::left << std::setw(30) << R.mTopic
                      << std::right << std::setw(4) << R.mPhaseIndex << "  "
                      << std::left << std::setw(13) << ToString(R.mPhaseStatus)
                      << std::right << std::setw(7) << R.mLegacyLoc
                      << std::setw(10) << R.mV4DesignChars << std::setw(9)
                      << R.mV4JobsCount << "  " << ToString(R.mCategory)
                      << Reset << "\n";
        }
        if (!Report.mWarnings.empty())
        {
            std::cout << "\n"
                      << kColorYellow << "Warnings:" << kColorReset << "\n";
            for (const std::string &W : Report.mWarnings)
            {
                std::cout << "  - " << W << "\n";
            }
        }
        return 0;
    }

    PrintJsonHeader(kLegacyGapSchema, Report.mGeneratedUtc, Report.mRepoRoot);
    EmitJsonFieldInt("bundle_count", Report.mBundleCount);
    std::cout << "\"rows\":[";
    for (size_t I = 0; I < Report.mRows.size(); ++I)
    {
        PrintJsonSep(I);
        const FPhaseGapRow &R = Report.mRows[I];
        std::cout << "{";
        EmitJsonField("topic", R.mTopic);
        EmitJsonFieldInt("phase_index", R.mPhaseIndex);
        EmitJsonField("phase_status", ToString(R.mPhaseStatus));
        EmitJsonField("legacy_path", R.mLegacyPath);
        EmitJsonFieldInt("legacy_loc", R.mLegacyLoc);
        EmitJsonFieldSizeT("v4_design_chars", R.mV4DesignChars);
        EmitJsonFieldSizeT("v4_jobs_count", R.mV4JobsCount);
        EmitJsonField("category", ToString(R.mCategory), false);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonClose(Report.mWarnings);
    return 0;
}

} // namespace UniPlan
