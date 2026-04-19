// UniPlanLegacyDiscovery.h — stateless filesystem discovery of legacy V3
// markdown artifacts.
//
// Both the `legacy-gap` command and the `watch` snapshot builder need to
// surface information about the legacy `.md` corpus without storing
// paths in the bundle (paths are a durability hazard — every stored
// path becomes dangling after deletion). Callers invoke
// `DiscoverLegacyArtifactsForTopic` at query time, then map per-phase
// keys to phase indices via `ResolvePhaseKeyToIndex`.
//
// The V4 bundle never references these results persistently.
//
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace UniPlan
{

namespace fs = std::filesystem;

// Legacy artifact categories. Values are internal to discovery — they
// do not appear in the bundle schema.
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

struct FLegacyDiscoveryHit
{
    fs::path mPath;
    ELegacyArtifactKind mKind = ELegacyArtifactKind::Plan;
    bool mbPerPhase = false;
    std::string mPhaseKey;
};

// Walk the repo tree for a single topic, matching filename conventions
// `<Topic>.*` within a valid V3 parent directory
// (`Docs/Plans`, `Docs/Implementation`, `Docs/Playbooks`, or the same
// under any `<Area>/Docs/` root). Returns one hit per match.
std::vector<FLegacyDiscoveryHit>
DiscoverLegacyArtifactsForTopic(const fs::path &InRepoRoot,
                                const std::string &InTopic);

// Map distinct per-phase legacy keys (e.g. `CR0`, `CR1`, ...) to 0-based
// V4 phase indices. When every key terminates in a distinct integer
// < InPhaseCount, the numeric suffix is used directly; otherwise keys
// are sorted lexicographically and truncated to InPhaseCount.
std::map<std::string, int>
ResolvePhaseKeyToIndex(const std::vector<FLegacyDiscoveryHit> &InHits,
                       size_t InPhaseCount,
                       std::vector<std::string> &OutWarnings);

// Read a legacy `.md` file, skip the V3 archival banner
// (a run of `> ...` lines at the top followed by a blank line), and
// return the number of non-empty content lines. Returns 0 on any error.
int CountLegacyContentLines(const fs::path &InPath);

} // namespace UniPlan
