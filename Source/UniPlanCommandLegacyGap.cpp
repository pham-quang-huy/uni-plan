#include "UniPlanCommandMutationCommon.h"
#include "UniPlanFileHelpers.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanJSONHelpers.h"
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

// Kind of legacy .md artifact discovered on disk. Used only inside
// legacy-gap; does not appear in the bundle schema.
enum class ELegacyArtifactKind : uint8_t
{
    Plan,
    Implementation,
    Playbook,
    PlanChangeLog,
    PlanVerification,
    ImplementationChangeLog,
    ImplementationVerification,
    PlaybookChangeLog,
    PlaybookVerification
};

static int ExtractPhaseNumericSuffix(const std::string &InPhaseKey)
{
    size_t Index = InPhaseKey.size();
    while (Index > 0 &&
           std::isdigit(static_cast<unsigned char>(InPhaseKey[Index - 1])) != 0)
    {
        --Index;
    }
    if (Index == InPhaseKey.size())
    {
        return -1;
    }
    try
    {
        return std::stoi(InPhaseKey.substr(Index));
    }
    catch (...)
    {
        return -1;
    }
}

struct FLegacyDiscoveryHit
{
    fs::path mPath;
    ELegacyArtifactKind mKind = ELegacyArtifactKind::Plan;
    bool mbPerPhase = false;
    std::string mPhaseKey;
};

static bool IsValidV3Directory(const fs::path &InPath,
                               ELegacyArtifactKind InKind)
{
    const std::string Parent = InPath.parent_path().filename().string();
    switch (InKind)
    {
    case ELegacyArtifactKind::Plan:
    case ELegacyArtifactKind::PlanChangeLog:
    case ELegacyArtifactKind::PlanVerification:
        return Parent == "Plans";
    case ELegacyArtifactKind::Implementation:
    case ELegacyArtifactKind::ImplementationChangeLog:
    case ELegacyArtifactKind::ImplementationVerification:
        return Parent == "Implementation";
    case ELegacyArtifactKind::Playbook:
    case ELegacyArtifactKind::PlaybookChangeLog:
    case ELegacyArtifactKind::PlaybookVerification:
        return Parent == "Playbooks";
    }
    return false;
}

static std::vector<FLegacyDiscoveryHit>
DiscoverLegacyArtifactsForTopic(const fs::path &InRepoRoot,
                                const std::string &InTopic)
{
    std::vector<FLegacyDiscoveryHit> Hits;
    if (!fs::exists(InRepoRoot))
    {
        return Hits;
    }
    struct FSuffixKind
    {
        const char *mSuffix;
        ELegacyArtifactKind mKind;
        bool mbPerPhase;
    };
    static const FSuffixKind kTopicLevelSuffixes[] = {
        {".Plan.ChangeLog.md", ELegacyArtifactKind::PlanChangeLog, false},
        {".Plan.Verification.md", ELegacyArtifactKind::PlanVerification, false},
        {".Impl.ChangeLog.md", ELegacyArtifactKind::ImplementationChangeLog,
         false},
        {".Impl.Verification.md",
         ELegacyArtifactKind::ImplementationVerification, false},
        {".Plan.md", ELegacyArtifactKind::Plan, false},
        {".Impl.md", ELegacyArtifactKind::Implementation, false},
    };
    static const FSuffixKind kPerPhaseSuffixes[] = {
        {".Playbook.ChangeLog.md", ELegacyArtifactKind::PlaybookChangeLog,
         true},
        {".Playbook.Verification.md", ELegacyArtifactKind::PlaybookVerification,
         true},
        {".Playbook.md", ELegacyArtifactKind::Playbook, true},
    };

    std::error_code EC;
    for (auto It = fs::recursive_directory_iterator(InRepoRoot, EC);
         It != fs::recursive_directory_iterator(); ++It)
    {
        if (EC)
        {
            break;
        }
        if (It->is_directory() && ShouldSkipRecursionDirectory(It->path()))
        {
            It.disable_recursion_pending();
            continue;
        }
        if (!It->is_regular_file())
        {
            continue;
        }
        const std::string Name = It->path().filename().string();

        for (const FSuffixKind &K : kTopicLevelSuffixes)
        {
            const std::string Expected = InTopic + K.mSuffix;
            if (Name == Expected && IsValidV3Directory(It->path(), K.mKind))
            {
                FLegacyDiscoveryHit Hit;
                Hit.mPath = It->path();
                Hit.mKind = K.mKind;
                Hit.mbPerPhase = false;
                Hits.push_back(std::move(Hit));
                break;
            }
        }
        for (const FSuffixKind &K : kPerPhaseSuffixes)
        {
            const std::string Prefix = InTopic + ".";
            const std::string Suffix = K.mSuffix;
            if (Name.size() <= Prefix.size() + Suffix.size())
            {
                continue;
            }
            if (Name.rfind(Prefix, 0) != 0)
            {
                continue;
            }
            if (Name.size() < Suffix.size() ||
                Name.compare(Name.size() - Suffix.size(), Suffix.size(),
                             Suffix) != 0)
            {
                continue;
            }
            if (!IsValidV3Directory(It->path(), K.mKind))
            {
                continue;
            }
            const std::string PhaseKey = Name.substr(
                Prefix.size(), Name.size() - Prefix.size() - Suffix.size());
            if (PhaseKey.empty() || PhaseKey.find('.') != std::string::npos)
            {
                continue;
            }
            FLegacyDiscoveryHit Hit;
            Hit.mPath = It->path();
            Hit.mKind = K.mKind;
            Hit.mbPerPhase = true;
            Hit.mPhaseKey = PhaseKey;
            Hits.push_back(std::move(Hit));
            break;
        }
    }
    return Hits;
}

// Map distinct phase keys (e.g. "CR0", "CR1", ...) to 0-based phase
// indices. When every key terminates in a distinct integer < InPhaseCount
// the suffix maps to the index directly; otherwise falls back to
// lexicographic ordering truncated to InPhaseCount.
static std::map<std::string, int>
ResolvePhaseKeyToIndex(const std::vector<FLegacyDiscoveryHit> &InHits,
                       size_t InPhaseCount,
                       std::vector<std::string> &OutWarnings)
{
    std::map<std::string, int> Result;
    std::vector<std::string> UniqueKeys;
    std::set<std::string> Seen;
    for (const FLegacyDiscoveryHit &H : InHits)
    {
        if (!H.mbPerPhase || H.mPhaseKey.empty())
        {
            continue;
        }
        if (Seen.insert(H.mPhaseKey).second)
        {
            UniqueKeys.push_back(H.mPhaseKey);
        }
    }
    if (UniqueKeys.empty())
    {
        return Result;
    }
    std::sort(
        UniqueKeys.begin(), UniqueKeys.end(),
        [](const std::string &A, const std::string &B)
        {
            size_t AI = A.size();
            while (AI > 0 &&
                   std::isdigit(static_cast<unsigned char>(A[AI - 1])) != 0)
                --AI;
            size_t BI = B.size();
            while (BI > 0 &&
                   std::isdigit(static_cast<unsigned char>(B[BI - 1])) != 0)
                --BI;
            const std::string PrefixA = A.substr(0, AI);
            const std::string PrefixB = B.substr(0, BI);
            if (PrefixA != PrefixB)
                return PrefixA < PrefixB;
            const int NumA = ExtractPhaseNumericSuffix(A);
            const int NumB = ExtractPhaseNumericSuffix(B);
            return NumA < NumB;
        });

    bool bCanUseSuffixDirect = true;
    std::map<std::string, int> SuffixMap;
    for (const std::string &K : UniqueKeys)
    {
        const int N = ExtractPhaseNumericSuffix(K);
        if (N < 0 || static_cast<size_t>(N) >= InPhaseCount ||
            SuffixMap.count(K) > 0)
        {
            bCanUseSuffixDirect = false;
            break;
        }
        SuffixMap[K] = N;
    }
    if (bCanUseSuffixDirect && UniqueKeys.size() <= InPhaseCount)
    {
        return SuffixMap;
    }

    if (UniqueKeys.size() > InPhaseCount)
    {
        OutWarnings.push_back(
            "legacy-gap: " + std::to_string(UniqueKeys.size()) +
            " distinct phase keys but only " + std::to_string(InPhaseCount) +
            " phases; truncating to first " + std::to_string(InPhaseCount));
    }
    for (size_t I = 0; I < UniqueKeys.size() && I < InPhaseCount; ++I)
    {
        Result[UniqueKeys[I]] = static_cast<int>(I);
    }
    return Result;
}

// ---------------------------------------------------------------------------
// Categorization thresholds — agents choose rebuild strategy based on the
// resulting EPhaseGapCategory. Documented next to EPhaseGapCategory in
// UniPlanEnums.h.
// ---------------------------------------------------------------------------

static constexpr int kLegacyRichMinLoc = 150;
static constexpr int kLegacyThinMinLoc = 50;
static constexpr size_t kV4HollowMaxChars = 500;
static constexpr size_t kV4RichMinChars = 2000;
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

static size_t ComputeV4DesignChars(const FPhaseRecord &InPhase)
{
    const FPhaseDesignMaterial &D = InPhase.mDesign;
    return InPhase.mScope.size() + InPhase.mOutput.size() +
           D.mInvestigation.size() + D.mCodeEntityContract.size() +
           D.mCodeSnippets.size() + D.mBestPractices.size() +
           D.mHandoff.size() + D.mReadinessGate.size() +
           D.mMultiPlatforming.size();
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
    Row.mV4DesignChars = ComputeV4DesignChars(P);
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
