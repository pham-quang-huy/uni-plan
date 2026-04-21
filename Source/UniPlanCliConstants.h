#pragma once

#include <string>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// CLI version and JSON schema constants
// ---------------------------------------------------------------------------

static constexpr const char *kCliVersion = "0.102.0";
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
// kTargetTopic — emitted by `uni-plan topic add` (v0.94.0) in the mutation
// envelope's `target` field. Distinguishes "topic bundle created" from
// "plan-level field mutated on an existing bundle" (which emits kTargetPlan).
// Mutation consumers parsing `target` as a structural path should recognize
// this as an opaque bundle-creation token, not an indexed reference.
static constexpr const char *kTargetTopic = "topic";
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
// Batch multi-phase get (v0.84.0). Single-phase --phase <N> continues to
// emit v1 at the top level (backward compat). --phases 1,3,5 emits this
// wrapped schema with the phase objects inside a `phases` array.
static constexpr const char *kPhaseGetBatchSchema = "uni-plan-phase-get-v2";
static constexpr const char *kPhaseListSchemaV2 = "uni-plan-phase-list-v2";
static constexpr const char *kChangelogSchemaV2 = "uni-plan-changelog-v2";
static constexpr const char *kVerificationSchemaV2 = "uni-plan-verification-v2";

static constexpr const char *kMutationSchema = "uni-plan-mutation-v1";

// Legacy-gap command schema (stateless V3 <-> V4 parity audit, 0.75.0+).
// Always register a new output schema here rather than emitting the string
// inline — the k*Schema constant is the single source of truth consumed by
// PrintJsonHeader() and by schema-aware downstream tooling.
static constexpr const char *kLegacyGapSchema = "uni-plan-legacy-gap-v1";

// Phase-drift command schema (v0.84.0). Reports phases where declared
// lifecycle status disagrees with the evidence stored elsewhere in the
// bundle (completed lanes, substantive `done` prose, completed_at
// timestamps). Consumed by agents to find status-lag drift without
// reading each phase manually.
static constexpr const char *kPhaseDriftSchema = "uni-plan-phase-drift-v1";

// Manifest-suggest command schema (v0.86.0). Reports git-history-derived
// suggestions for file_manifest entries on a single phase, scanning the
// phase's started_at..completed_at window. Backfill tool that closes the
// retrofit gap created by file_manifest_required_for_code_phases — authors
// review the suggestions and call `manifest add` (or use --apply for
// auto-add).
static constexpr const char *kManifestSuggestSchema =
    "uni-plan-manifest-suggest-v1";

// Catalog command schema (v0.93.0). Dump the full CLI verb / subcommand /
// flag surface as machine-readable JSON. External tooling — notably the
// FIE skill-recipe linter — consumes this to detect drift between
// documented recipes and the shipping CLI without having to parse
// `--help` prose.
static constexpr const char *kCatalogSchema = "uni-plan-catalog-v1";

// Phase sync-execution schema (v0.102.0). Emitted by `uni-plan phase
// sync-execution` — reports the child→parent rollups applied (or the ones
// that would be applied under --dry-run) to reconcile lane / job status
// from their descendants. Strictly non-downgrading, never touches phase
// status, idempotent on rerun. See FPhaseSyncExecutionOptions and
// RunPhaseSyncExecutionCommand.
static constexpr const char *kPhaseSyncExecutionSchema =
    "uni-plan-sync-execution-v1";

// catalog_schema_version is independent from kCliVersion and from
// uni-plan-catalog-v1: it tracks the SHAPE of the catalog record
// (field names, nesting), not the CLI content. Bumping kCliVersion
// leaves this at 1; renaming a top-level field or dropping a nested
// column requires bumping this + the consumer-side min-version gate.
static constexpr int kCatalogSchemaVersion = 1;

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
