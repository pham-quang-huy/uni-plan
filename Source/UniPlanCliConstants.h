#pragma once

#include <string>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// CLI version and JSON schema constants
// ---------------------------------------------------------------------------

static constexpr const char *kCliVersion = "0.83.0";
static constexpr const char *kListSchema = "uni-plan-list-v1";
static constexpr const char *kPairListSchema = "uni-plan-pair-list-v1";
static constexpr const char *kLintSchema = "uni-plan-lint-v1";
static constexpr const char *kInventorySchema = "uni-plan-inventory-v1";
static constexpr const char *kOrphanCheckSchema = "uni-plan-orphan-check-v1";
static constexpr const char *kArtifactsSchema = "uni-plan-artifacts-v1";
static constexpr const char *kChangelogSchema = "uni-plan-changelog-v1";
static constexpr const char *kVerificationSchema = "uni-plan-verification-v1";
static constexpr const char *kSchemaSchema = "uni-plan-schema-v1";
static constexpr const char *kRulesSchema = "uni-plan-rules-v1";
static constexpr const char *kValidateSchema = "uni-plan-validate-v1";
static constexpr const char *kSectionResolveSchema =
    "uni-plan-section-resolve-v1";
static constexpr const char *kExcerptSchema = "uni-plan-excerpt-v1";
static constexpr const char *kTableListSchema = "uni-plan-table-list-v1";
static constexpr const char *kTableGetSchema = "uni-plan-table-get-v1";
static constexpr const char *kGraphSchema = "uni-plan-graph-v1";
static constexpr const char *kDriftDiagnoseSchema =
    "uni-plan-drift-diagnose-v1";
static constexpr const char *kTimelineSchema = "uni-plan-timeline-v1";
static constexpr const char *kBlockersSchema = "uni-plan-blockers-v1";
static constexpr const char *kPhaseListSchema = "uni-plan-phase-list-v1";
static constexpr const char *kInventoryCacheSchema =
    "uni-plan-inventory-cache-v1";
static constexpr const char *kSectionSchemaSchema =
    "uni-plan-section-schema-v1";
static constexpr const char *kSectionListSchema = "uni-plan-section-list-v2";
static constexpr const char *kCacheInfoSchema = "uni-plan-cache-info-v1";
static constexpr const char *kCacheClearSchema = "uni-plan-cache-clear-v1";
static constexpr const char *kCacheConfigSchema = "uni-plan-cache-config-v1";
static constexpr const char *kSectionContentSchema =
    "uni-plan-section-content-v1";

// ---------------------------------------------------------------------------
// Canonical mutation target strings
//
// Emitted in `target` fields of mutation output JSON and written to
// FChangeLogEntry.mAffected by AppendAutoChangelog. Phase-scoped targets
// are built at runtime via MakePhaseTarget / MakeJobTarget / etc.
//
// Reference convention: plural container name with positional index —
// "phases[N]", "jobs[N]", "lanes[N]", "tasks[N]". Matches the JSON key
// layout and JSON-pointer semantics (".../phases/0/jobs/1"). See
// CLAUDE.md -> documentation_rules.
// ---------------------------------------------------------------------------

static constexpr const char *kTargetPlan = "plan";
static constexpr const char *kTargetChangelogs = "changelogs";
static constexpr const char *kTargetVerifications = "verifications";

inline std::string MakePhaseTarget(int InPhaseIndex)
{
    return "phases[" + std::to_string(InPhaseIndex) + "]";
}
inline std::string MakeJobTarget(int InPhaseIndex, int InJobIndex)
{
    return MakePhaseTarget(InPhaseIndex) + ".jobs[" +
           std::to_string(InJobIndex) + "]";
}
inline std::string MakeLaneTarget(int InPhaseIndex, int InLaneIndex)
{
    return MakePhaseTarget(InPhaseIndex) + ".lanes[" +
           std::to_string(InLaneIndex) + "]";
}
inline std::string MakeTaskTarget(int InPhaseIndex, int InJobIndex,
                                  int InTaskIndex)
{
    return MakeJobTarget(InPhaseIndex, InJobIndex) + ".tasks[" +
           std::to_string(InTaskIndex) + "]";
}
inline std::string MakeTestingTarget(int InPhaseIndex, int InIndex)
{
    return MakePhaseTarget(InPhaseIndex) + ".testing[" +
           std::to_string(InIndex) + "]";
}
inline std::string MakeManifestTarget(int InPhaseIndex, int InIndex)
{
    return MakePhaseTarget(InPhaseIndex) + ".file_manifest[" +
           std::to_string(InIndex) + "]";
}
inline std::string MakeVerificationTarget(int InIndex)
{
    return std::string(kTargetVerifications) + "[" + std::to_string(InIndex) +
           "]";
}
inline std::string MakeChangelogTarget(int InIndex)
{
    return std::string(kTargetChangelogs) + "[" + std::to_string(InIndex) + "]";
}

// V4 bundle-native command schemas
static constexpr const char *kTopicListSchema = "uni-plan-topic-list-v1";
static constexpr const char *kTopicGetSchema = "uni-plan-topic-get-v1";
static constexpr const char *kPhaseGetSchema = "uni-plan-phase-get-v1";
static constexpr const char *kPhaseListSchemaV2 = "uni-plan-phase-list-v2";
static constexpr const char *kChangelogSchemaV2 = "uni-plan-changelog-v2";
static constexpr const char *kVerificationSchemaV2 = "uni-plan-verification-v2";

static constexpr const char *kMutationSchema = "uni-plan-mutation-v1";

// Legacy-gap command schema (stateless V3 <-> V4 parity audit, 0.75.0+).
// Always register a new output schema here rather than emitting the string
// inline — the k*Schema constant is the single source of truth consumed by
// PrintJsonHeader() and by schema-aware downstream tooling.
static constexpr const char *kLegacyGapSchema = "uni-plan-legacy-gap-v1";

// ---------------------------------------------------------------------------
// ANSI color codes for --human mode
// ---------------------------------------------------------------------------

static constexpr const char *kColorReset = "\033[0m";
static constexpr const char *kColorBold = "\033[1m";
static constexpr const char *kColorDim = "\033[2m";
static constexpr const char *kColorRed = "\033[31m";
static constexpr const char *kColorYellow = "\033[33m";
static constexpr const char *kColorGreen = "\033[38;5;114m";
static constexpr const char *kColorOrange = "\033[38;5;208m";

// ---------------------------------------------------------------------------
// Sidecar and extension constants
// ---------------------------------------------------------------------------

static constexpr const char *kSidecarChangeLog = "ChangeLog";
static constexpr const char *kSidecarVerification = "Verification";
static constexpr const char *kExtPlan = ".Plan.json";
static constexpr const char *kExtImpl = ".Impl.json";
static constexpr const char *kExtPlaybook = ".Playbook.json";

// ---------------------------------------------------------------------------
// Human-mode label constants
// ---------------------------------------------------------------------------

static constexpr const char *kHumanTable =
    "  --human                 Output as formatted ANSI table\n";
static constexpr const char *kHumanList =
    "  --human                 Output as formatted ANSI list\n";
static constexpr const char *kHumanDisplay =
    "  --human                 Output as formatted ANSI display\n";
static constexpr const char *kHumanTables =
    "  --human                 Output as formatted ANSI tables\n";

} // namespace UniPlan
