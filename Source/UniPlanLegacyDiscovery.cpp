// UniPlanLegacyDiscovery.cpp — stateless filesystem discovery of legacy
// V3 markdown artifacts. See `UniPlanLegacyDiscovery.h` for rationale.
//
// Sole consumer as of v0.80.0:
//   * `UniPlanCommandLegacyGap.cpp`  (per-phase parity audit)
//
// Watch mode dropped its legacy-discovery use in v0.80.0 along with
// the PB / PBLines columns; the V4 bundle is the single source of
// truth for the watch TUI and `legacy-gap` is the CLI for auditing
// any remaining V3 .md corpus.

#include "UniPlanLegacyDiscovery.h"

#include "UniPlanForwardDecls.h" // ShouldSkipRecursionDirectory

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <string>

namespace UniPlan
{

namespace
{

int ExtractPhaseNumericSuffix(const std::string &InPhaseKey)
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

bool IsValidV3Directory(const fs::path &InPath, ELegacyArtifactKind InKind)
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

} // namespace

std::vector<FLegacyDiscoveryHit>
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

std::map<std::string, int>
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

int CountLegacyContentLines(const fs::path &InPath)
{
    std::ifstream Stream(InPath);
    if (!Stream.is_open())
    {
        return 0;
    }
    int Count = 0;
    bool bInBanner = false;
    bool bAfterBanner = false;
    std::string Line;
    while (std::getline(Stream, Line))
    {
        // The V3 archival banner is a run of `>` blockquote lines at the
        // very top, terminated by a blank line. Skip it wholesale so the
        // line count reflects human-authored content only.
        if (!bAfterBanner)
        {
            const auto FirstNonSpace = Line.find_first_not_of(" \t");
            const bool bBlank = (FirstNonSpace == std::string::npos);
            const bool bIsBlockquote = !bBlank && Line[FirstNonSpace] == '>';
            if (!bInBanner && bIsBlockquote &&
                Line.find("ARCHIVAL") != std::string::npos)
            {
                bInBanner = true;
                continue;
            }
            if (bInBanner)
            {
                if (bIsBlockquote)
                {
                    continue;
                }
                if (bBlank)
                {
                    bInBanner = false;
                    bAfterBanner = true;
                    continue;
                }
                // Non-blank, non-blockquote line terminates the banner.
                bInBanner = false;
                bAfterBanner = true;
                // fall through to count this line
            }
            else if (!bBlank)
            {
                // Real content encountered without seeing a banner — we
                // are past the banner region.
                bAfterBanner = true;
            }
        }
        // Count non-blank content lines.
        if (Line.find_first_not_of(" \t") != std::string::npos)
        {
            ++Count;
        }
    }
    return Count;
}

} // namespace UniPlan
