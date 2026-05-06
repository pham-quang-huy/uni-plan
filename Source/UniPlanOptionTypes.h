#pragma once

#include "UniPlanEnums.h"
#include "UniPlanTaxonomyTypes.h"

#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Error types
// ---------------------------------------------------------------------------

struct UsageError : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------------------
// CLI options structs
// ---------------------------------------------------------------------------

struct BaseOptions
{
    std::string mRepoRoot;
    bool mbJson = true;
    bool mbHuman = false;
    // --ack-only (v0.105.0+): when set on a mutation command, the
    // response envelope switches to the compact kMutationAckSchema shape
    // ("changed_fields":[<names>]) instead of the full kMutationSchema
    // ("changes":[{field,old,new}]). Bundle persistence, auto-changelog,
    // and exit codes are unaffected. Opt-in; default false preserves
    // v0.104.1 behavior for every existing caller.
    bool mbAckOnly = false;
    // mAppendFields (v0.105.0+): set of field names whose mutation
    // should use append-concat semantics instead of replace. Populated
    // by TryConsumeStringOrFileOrAppendFileOption when the caller passes
    // --<field>-append-file <path>. The mutation handler queries this
    // set per-field via ComputeAppendOrReplace (see UniPlanHelpers.h) to
    // decide whether to concat-with-seam or replace the stored value.
    // Empty in the default case — every existing replace-only flag
    // keeps its v0.104.1 behavior. See also: --<field>-append-file row
    // on any phase set field.
    std::set<std::string> mAppendFields;
};

struct CacheInfoOptions : BaseOptions
{
};

struct CacheClearOptions : BaseOptions
{
};

struct CacheConfigOptions : BaseOptions
{
    std::string mDir;
    bool mbDirSet = false;
    std::string mEnabled;
    std::string mVerbose;
};

// ---------------------------------------------------------------------------
// V4 bundle-native option structs
// ---------------------------------------------------------------------------

struct FTopicListOptions : BaseOptions
{
    std::string mStatus = "all";
};

struct FTopicGetOptions : BaseOptions
{
    std::string mTopic;
    // --sections <csv>: filter output to named top-level sections.
    // Empty = emit everything (backward-compatible default).
    // Populated = emit only the named sections; identity fields
    // (topic, status, title, schema envelope) are always emitted.
    // Valid names: summary, goals, non_goals, risks, acceptance_criteria,
    // problem_statement, validation_commands, baseline_audit,
    // execution_strategy, locked_decisions, source_references,
    // dependencies, next_actions, phases. Added v0.84.0.
    std::vector<std::string> mSections;
};

struct FPhaseListOptions : BaseOptions
{
    std::string mTopic;
    std::string mStatus = "all";
};

struct FPhaseGetOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::vector<int> mPhaseIndices; // --phases <csv>: batch mode (v0.84.0)
                                    // mutually exclusive with --phase <N>
    // --all-phases (v0.105.0+): sugar meaning "every phase index 0..N-1".
    // Mutually exclusive with --phase and --phases at parse time; the
    // handler expands mbAllPhases to a full mPhaseIndices set after the
    // bundle is loaded so downstream batch-emission code is unchanged.
    bool mbAllPhases = false;
    bool mbBrief = false;     // --brief: compact view for session resume
    bool mbExecution = false; // --execution: jobs/tasks/lanes + structural
                              //              dependencies / validation_commands
    bool mbDesign = false;    // --design: only the fields that feed
                              //           ComputePhaseDesignChars (scope +
                              //           output + 7 design material prose
                              //           fields). Renamed from --reference in
                              //           v0.83.0 to match the design_chars
                              //           measure and FPhaseDesignMaterial.
};

struct FPhaseMetricOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::vector<int> mPhaseIndices;
    // --all-phases (v0.105.0+): sugar for every index; mutually exclusive
    // with --phase and --phases. Same semantics as FPhaseGetOptions.
    bool mbAllPhases = false;
    std::string mStatus = "all";
};

struct FBundleChangelogOptions : BaseOptions
{
    std::string mTopic;
    std::string mScopeFilter; // "plan", "implementation", or index
    bool mbHasScopeFilter = false;
};

struct FBundleVerificationOptions : BaseOptions
{
    std::string mTopic;
    std::string mScopeFilter;
    bool mbHasScopeFilter = false;
};

struct FBundleTimelineOptions : BaseOptions
{
    std::string mTopic;
    std::string mSince;
    int mPhaseFilter = -2; // -2 = no filter
    bool mbHasPhaseFilter = false;
};

struct FBundleBlockersOptions : BaseOptions
{
    std::string mTopic; // optional — empty means all topics
};

struct FBundleValidateOptions : BaseOptions
{
    std::string mTopic; // optional — empty means all topics
    bool mbStrict = false;
};

// ---------------------------------------------------------------------------
// Mutation option structs
// ---------------------------------------------------------------------------

// FTopicAddOptions — v0.94.0 CLI gap closure for `uni-plan topic add`. Creates
// a brand-new .Plan.json bundle file at Docs/Plans/<TopicKey>.Plan.json. Every
// prose field has a --<field>-file sibling parsed via
// TryConsumeStringOrFileOption (see v0.76.0 notes). The fresh bundle is an
// empty-phases shell — `phase add` is called next to seed Phase 0. Symmetric
// with `phase add`, `risk add`, etc.
struct FTopicAddOptions : BaseOptions
{
    std::string mTopic; // required — must match ^[A-Z][A-Za-z0-9]*$
    std::string mTitle; // required — enforced by required_fields evaluator
    std::optional<ETopicStatus> opStatus; // unset -> NotStarted
    // Metadata prose fields (mirror FTopicSetOptions; all optional)
    std::string mSummary;
    std::string mGoals;
    std::string mNonGoals;
    std::string mProblemStatement;
    std::string mBaselineAudit;
    std::string mExecutionStrategy;
    std::string mLockedDecisions;
    std::string mSourceReferences;
};

struct FTopicSetOptions : BaseOptions
{
    std::string mTopic;
    std::optional<ETopicStatus> opStatus;
    // `risks`, `next_actions`, `acceptance_criteria` are typed arrays as of
    // v0.89.0 and are mutated via the dedicated `uni-plan risk`,
    // `uni-plan next-action`, `uni-plan acceptance-criterion` groups. The
    // former `--risks`, `--next-actions`, `--acceptance-criteria` flags
    // (plus their `-file` variants) were removed in the same release.
    // Metadata fields
    std::string mSummary;
    std::string mGoals;
    std::string mNonGoals;
    std::string mProblemStatement;
    // Typed validation_commands mutation (structured, replaces the former
    // string form). --validation-clear wipes the existing vector before
    // --validation-add entries are appended. This lets agents build the
    // set in one call or incrementally across calls.
    bool mbValidationClear = false;
    std::vector<FValidationCommand> mValidationAdd;
    std::string mBaselineAudit;
    std::string mExecutionStrategy;
    std::string mLockedDecisions;
    std::string mSourceReferences;
    // Typed dependencies mutation (replaces the former string form).
    // --dependency-clear empties the existing vector before --dependency-add
    // entries are appended.
    bool mbDependencyClear = false;
    std::vector<FBundleReference> mDependencyAdd;
};

struct FPhaseAddOptions : BaseOptions
{
    std::string mTopic;
    std::string mScope;
    std::string mOutput;
    std::optional<EExecutionStatus> opStatus; // unset -> NotStarted
};

struct FPhaseNormalizeOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    bool mbDryRun = false;
};

struct FPhaseBoardReplaceOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::string mBoardJSONFile;
};

struct FPhaseSetOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::optional<EExecutionStatus> opStatus;
    std::string mDone;
    // Explicit "set to empty" flag for prose fields. `--done <value>` is
    // ignored when value is the empty string (a long-standing convention
    // used to signal "no change"). When a caller genuinely needs to clear
    // the field — e.g., reverting a not_started phase whose `done`
    // carries a stale "Not started" placeholder — the matching `*-clear`
    // flag below is the typed, explicit way.
    bool mbDoneClear = false;
    std::string mRemaining;
    bool mbRemainingClear = false;
    std::string mBlockers;
    bool mbBlockersClear = false;
    std::string mContext; // agent_context
    // Explicit timestamp overrides — when present, these values win over
    // the auto-stamp that `phase set --status` normally emits. Intended
    // for migration/repair passes that need to backfill historical
    // started_at / completed_at from legacy evidence instead of "now".
    // Format is validated against IsValidISOTimestamp at apply time.
    std::string mStartedAt;
    std::string mCompletedAt;
    // Phase-level fields
    std::string mScope;
    std::string mOutput;
    // Design material fields
    std::string mInvestigation;
    std::string mCodeEntityContract;
    std::string mCodeSnippets;
    std::string mBestPractices;
    std::string mMultiPlatforming;
    std::string mReadinessGate;
    std::string mHandoff;
    // Typed validation_commands mutation (see FTopicSetOptions above).
    bool mbValidationClear = false;
    std::vector<FValidationCommand> mValidationAdd;
    // Typed dependencies mutation (see FTopicSetOptions above).
    bool mbDependencyClear = false;
    std::vector<FBundleReference> mDependencyAdd;
    // Semantic provenance stamp. Empty / unset = leave unchanged. Any
    // value must round-trip through PhaseOriginFromString; parser
    // enforces the allowed set at parse time (exit 2 on invalid).
    std::optional<EPhaseOrigin> opOrigin;
    // Explicit-no-manifest opt-out (v0.86.0). opNoFileManifest carries
    // the requested boolean state when --no-file-manifest <true|false>
    // is passed; absence means "leave unchanged." The reason flag is
    // applied independently, but the post-mutation invariant
    // (no_file_manifest=true ⇒ non-empty reason) is enforced by the
    // schema serializer + parser, so callers must set the reason in the
    // same `phase set` invocation as flipping the bool to true.
    std::optional<bool> opNoFileManifest;
    std::string mFileManifestSkipReason;
    bool mbFileManifestSkipReasonClear = false;
};

struct FJobSetOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mJobIndex = -1;
    std::optional<EExecutionStatus> opStatus;
    std::string mScope;
    std::string mOutput;
    std::string mExitCriteria;
    int mLaneIndex = -1; // -1 means unchanged; >=0 reassigns job's lane ref
    int mWave = -1;      // -1 means unchanged
};

struct FTaskSetOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mJobIndex = -1;
    int mTaskIndex = -1;
    std::optional<EExecutionStatus> opStatus;
    std::string mEvidence;
    std::string mNotes;
    // --description / --description-file (v0.105.0+): mutate the task's
    // description text. mbDescriptionSet distinguishes "caller passed the
    // flag" from "caller did not" because "" is a valid description (a
    // legitimate 'clear the description' request). The handler gates the
    // mutation: if Task.mStatus != NotStarted, it refuses unless mbForce
    // is set AND mReason is non-empty-after-trim, and records before/
    // after text + reason in the auto-changelog entry.
    std::string mDescription;
    bool mbDescriptionSet = false;
    bool mbForce = false;
    std::string mReason;
};

struct FChangelogAddOptions : BaseOptions
{
    std::string mTopic;
    std::string mScope;
    std::string mChange;
    EChangeType mType = EChangeType::Chore;
    std::string mAffected;
};

struct FVerificationAddOptions : BaseOptions
{
    std::string mTopic;
    std::string mScope;
    std::string mCheck;
    std::string mResult;
    std::string mDetail;
};

// ---------------------------------------------------------------------------
// Semantic command option structs
// ---------------------------------------------------------------------------

// Tier 1 — Phase lifecycle mutations
struct FPhaseStartOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::string mContext;
};

struct FPhaseCompleteOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::string mDone;
    std::string mVerification;
};

struct FPhaseBlockOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::string mReason;
};

struct FPhaseUnblockOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
};

struct FPhaseCancelOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::string mReason;
};

struct FPhaseProgressOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::string mDone;
    std::string mRemaining;
};

struct FPhaseCompleteJobsOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
};

// Tier 2 — Topic lifecycle mutations
struct FTopicStartOptions : BaseOptions
{
    std::string mTopic;
};

struct FTopicCompleteOptions : BaseOptions
{
    std::string mTopic;
    std::string mVerification;
};

struct FTopicBlockOptions : BaseOptions
{
    std::string mTopic;
    std::string mReason;
};

// Tier 4 — Query helpers (shared for readiness + wave-status)
struct FPhaseQueryOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    // --all-phases (v0.105.0+) on `phase readiness`: sugar for a batch
    // sweep over every phase. Mutually exclusive with --phase at parse
    // time. `phase wave-status` shares this struct but does not accept
    // --all-phases (current semantics require a specific phase); the
    // parser rejects it there with a UsageError.
    bool mbAllPhases = false;
};

// ---------------------------------------------------------------------------
// Tier 5 — Missing entity coverage
// ---------------------------------------------------------------------------

struct FLaneSetOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mLaneIndex = -1;
    std::optional<EExecutionStatus> opStatus;
    std::string mScope;
    std::string mExitCriteria;
};

// v0.101.0 — lane complete semantic command. Symmetric with phase complete:
// refuses completion when any job on the lane is not terminal (Completed or
// Canceled). Raw `lane set --status completed` remains available for manual
// repair but bypasses this gate — use the semantic command by default.
struct FLaneCompleteOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mLaneIndex = -1;
};

// v0.102.0 — phase sync-execution reconciliation command. Rolls up terminal
// status from tasks → jobs → lanes (child → parent only; never downgrades,
// never touches phase status itself). --dry-run reports what would change
// without writing. Use after bulk leaf-level task/job updates to avoid
// stepping `job set --status completed` + `lane complete` on every entity
// manually before `phase complete`.
struct FPhaseSyncExecutionOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    bool mbDryRun = false;
};

struct FTestingAddOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::string mSession;
    std::optional<ETestingActor> opActor; // unset -> Human
    std::string mStep;
    std::string mAction;
    std::string mExpected;
    std::string mEvidence;
};

struct FManifestAddOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::string mFile;
    std::optional<EFileAction> opAction; // required; parser enforces
    std::string mDescription;
};

// Modify-existing array entries (unlike add, these target an index)

struct FTestingSetOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mIndex = -1;
    std::string mSession;
    std::optional<ETestingActor> opActor;
    std::string mStep;
    std::string mAction;
    std::string mExpected;
    std::string mEvidence;
};

struct FVerificationSetOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
    std::string mCheck;
    std::string mResult;
    std::string mDetail;
};

struct FManifestRemoveOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mIndex = -1;
};

struct FManifestListOptions : BaseOptions
{
    std::string mTopic;         // optional — filters to single topic
    int mPhaseIndex = -1;       // optional — filters to single phase
    bool mbMissingOnly = false; // --missing-only: only emit entries whose
                                // file_path does not resolve on disk
    bool mbStalePlan = false;   // --stale-plan: only emit entries where
                                // the plan intent disagrees with on-disk
                                // reality (stale_create / stale_delete /
                                // dangling_modify). Orthogonal to
                                // --missing-only; when both are set the
                                // predicates intersect (AND).
};

// manifest suggest command options (v0.86.0). Backfill tool: scans the
// phase's git-history window (started_at..completed_at) and proposes
// file_manifest entries for files touched but not yet recorded.
// Defaults to dry-run (read-only) — pass --apply to actually call
// manifest add for each suggestion.
struct FManifestSuggestOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    bool mbApply = false; // --apply: actually invoke manifest add for
                          // each suggested row; default is dry-run
};

// phase drift command options (v0.84.0). Reports phases where declared
// lifecycle status lags behind evidence stored elsewhere in the bundle.
// Optional --topic scopes to a single bundle; omit to scan the whole repo.
struct FPhaseDriftOptions : BaseOptions
{
    std::string mTopic; // optional — scan all topics when empty
};

struct FManifestSetOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mIndex = -1;
    std::string mFile;
    std::optional<EFileAction> opAction;
    std::string mDescription;
};

struct FLaneAddOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::optional<EExecutionStatus> opStatus; // unset -> NotStarted
    std::string mScope;
    std::string mExitCriteria;
};

// ---------------------------------------------------------------------------
// Tier 7 — CRUD symmetry (v0.93.0+)
//
// Fills the gaps left by the original Tier 5 entity surface: job/task had
// only `set`; lane/testing lacked `remove`/`list`; topic lacked
// `normalize`. Mirror `FLaneAddOptions` + `FManifestRemoveOptions` +
// `FManifestListOptions` patterns.
// ---------------------------------------------------------------------------

struct FJobAddOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::optional<EExecutionStatus> opStatus; // unset -> NotStarted
    std::string mScope;
    std::string mOutput;
    std::string mExitCriteria;
    int mLaneIndex = -1; // -1 means unassigned
    int mWave = -1;      // -1 means unassigned
};

struct FJobRemoveOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mJobIndex = -1;
};

struct FJobListOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1; // optional — filters to single phase
};

struct FTaskAddOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mJobIndex = -1;
    std::optional<EExecutionStatus> opStatus; // unset -> NotStarted
    std::string mDescription;
    std::string mEvidence;
    std::string mNotes;
};

struct FTaskRemoveOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mJobIndex = -1;
    int mTaskIndex = -1;
};

struct FTaskListOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1; // optional — filters to single phase
    int mJobIndex = -1;   // optional — filters to single job within phase
};

struct FLaneRemoveOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mLaneIndex = -1;
};

struct FLaneListOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1; // optional — filters to single phase
};

struct FTestingRemoveOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mIndex = -1;
};

struct FTestingListOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1; // optional — filters to single phase
};

struct FTopicNormalizeOptions : BaseOptions
{
    std::string mTopic;
    bool mbDryRun = false;
};

struct FChangelogRemoveOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
};

struct FChangelogSetOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
    int mPhase = -2; // -2 = unchanged; -1 = topic-level; >=0 = phase index
    std::string mDate;
    std::string mChange;
    std::optional<EChangeType> opType;
    std::string mAffected;
};

// ---------------------------------------------------------------------------
// Tier 6 — Risk entry CLI groups (v0.89.0+)
//
// `uni-plan risk add --topic <T> --statement <t> [--mitigation <t>]
//   [--severity low|medium|high|critical] [--status open|mitigated|accepted|
//   closed] [--id <t>] [--notes <t>]` — append typed risk entry.
// `uni-plan risk set --topic <T> --index <N> [same flags]` — mutate one
// entry; all fields optional; only passed flags update.
// `uni-plan risk remove --topic <T> --index <N>` — erase entry by index.
// `uni-plan risk list --topic <T> [--severity <filter>] [--status <filter>]`
// — emit array filtered by severity/status.
// ---------------------------------------------------------------------------

struct FRiskAddOptions : BaseOptions
{
    std::string mTopic;
    std::string mId;
    std::string mStatement;
    std::string mMitigation;
    std::optional<ERiskSeverity> opSeverity; // unset -> Medium
    std::optional<ERiskStatus> opStatus;     // unset -> Open
    std::string mNotes;
};

struct FRiskSetOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
    // opPresent flags distinguish "--id ''" (clear) from "not passed"
    // (unchanged). Strings use empty-means-unchanged semantics, matching
    // FChangelogSetOptions.
    std::string mId;
    std::string mStatement;
    std::string mMitigation;
    std::optional<ERiskSeverity> opSeverity;
    std::optional<ERiskStatus> opStatus;
    std::string mNotes;
};

struct FRiskRemoveOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
};

struct FRiskListOptions : BaseOptions
{
    std::string mTopic;
    std::optional<ERiskSeverity> opSeverityFilter;
    std::optional<ERiskStatus> opStatusFilter;
};

// ---------------------------------------------------------------------------
// Tier 6 — Next-action entry CLI groups (v0.89.0+)
// ---------------------------------------------------------------------------

struct FNextActionAddOptions : BaseOptions
{
    std::string mTopic;
    int mOrder = 0; // 0 = auto-assign (size+1); callers can set explicitly
    std::string mStatement;
    std::string mRationale;
    std::string mOwner;
    std::optional<EActionStatus> opStatus;
    std::string mTargetDate;
};

struct FNextActionSetOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
    int mOrder = -1; // -1 = unchanged; >=0 = new order
    std::string mStatement;
    std::string mRationale;
    std::string mOwner;
    std::optional<EActionStatus> opStatus;
    std::string mTargetDate;
};

struct FNextActionRemoveOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
};

struct FNextActionListOptions : BaseOptions
{
    std::string mTopic;
    std::optional<EActionStatus> opStatusFilter;
};

// ---------------------------------------------------------------------------
// Tier 6 — Acceptance-criterion entry CLI groups (v0.89.0+)
// ---------------------------------------------------------------------------

struct FAcceptanceCriterionAddOptions : BaseOptions
{
    std::string mTopic;
    std::string mId;
    std::string mStatement;
    std::optional<ECriterionStatus> opStatus;
    std::string mMeasure;
    std::string mEvidence;
};

struct FAcceptanceCriterionSetOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
    std::string mId;
    std::string mStatement;
    std::optional<ECriterionStatus> opStatus;
    std::string mMeasure;
    std::string mEvidence;
};

struct FAcceptanceCriterionRemoveOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
};

struct FAcceptanceCriterionListOptions : BaseOptions
{
    std::string mTopic;
    std::optional<ECriterionStatus> opStatusFilter;
};

// ---------------------------------------------------------------------------
// Tier 6b — Priority-grouping / runbook / residual-risk CLI groups (v0.98.0+)
//
// Typed homes for what previously lived in sidecar `.md` files. Each group
// exposes add/set/remove/list leaves with the same shape as the v0.89.0
// risk/next-action/acceptance-criterion groups. All prose flags have
// `-file` siblings parsed via TryConsumeStringOrFileOption.
// ---------------------------------------------------------------------------

struct FPriorityGroupingAddOptions : BaseOptions
{
    std::string mTopic;
    std::string mID;
    // --phase-index can be passed multiple times to build the list, or as a
    // comma-separated CSV via --phase-indices. The parser accepts both shapes
    // and merges them.
    std::vector<int> mPhaseIndices;
    std::string mRule;
};

struct FPriorityGroupingSetOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
    std::string mID;
    // Present-vs-absent distinction: mbPhaseIndicesSet = true means "replace
    // the list with what was passed" (may be empty to clear via --phase-
    // indices-clear). false means "leave the list unchanged."
    bool mbPhaseIndicesSet = false;
    std::vector<int> mPhaseIndices;
    std::string mRule;
};

struct FPriorityGroupingRemoveOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
};

struct FPriorityGroupingListOptions : BaseOptions
{
    std::string mTopic;
};

struct FRunbookAddOptions : BaseOptions
{
    std::string mTopic;
    std::string mName;
    std::string mTrigger;
    // Commands are ordered and typically supplied by repeated --command flags
    // or comma-free --commands-file; parser supports both and appends in
    // invocation order.
    std::vector<std::string> mCommands;
    std::string mDescription;
};

struct FRunbookSetOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
    std::string mName;
    std::string mTrigger;
    bool mbCommandsSet = false;
    std::vector<std::string> mCommands;
    std::string mDescription;
};

struct FRunbookRemoveOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
};

struct FRunbookListOptions : BaseOptions
{
    std::string mTopic;
};

struct FResidualRiskAddOptions : BaseOptions
{
    std::string mTopic;
    std::string mArea;
    std::string mObservation;
    std::string mWhyDeferred;
    std::string mTargetPhase;
    std::string mRecordedDate;
    std::string mClosureSha;
};

struct FResidualRiskSetOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
    std::string mArea;
    std::string mObservation;
    std::string mWhyDeferred;
    std::string mTargetPhase;
    std::string mRecordedDate;
    std::string mClosureSha;
};

struct FResidualRiskRemoveOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
};

struct FResidualRiskListOptions : BaseOptions
{
    std::string mTopic;
};

// ---------------------------------------------------------------------------
// Graph command options (v0.98.0+). Walks typed topic + phase dependency
// graph across all bundles and emits uni-plan-graph-v1 JSON.
//
// --topic focuses on one topic's reachable neighborhood. When empty, the
// full corpus graph is emitted. --depth bounds the walk depth when --topic
// is set (-1 = unlimited, the default).
// ---------------------------------------------------------------------------

struct FGraphOptions : BaseOptions
{
    std::string mTopic; // empty = all topics
    int mDepth = -1;    // -1 = unlimited; >=0 limits walk depth from --topic
};

// ---------------------------------------------------------------------------
// Legacy-gap audit option struct (stateless V3 <-> V4 parity, 0.75.0+)
// ---------------------------------------------------------------------------

// Options for `uni-plan legacy-gap`. Defaults to all topics, all categories.
// Stateless: discovers legacy .md files on disk at invoke time; reads no
// path-based index from the bundle.
struct FLegacyGapOptions : BaseOptions
{
    std::string mTopic;                          // optional; empty = all topics
    std::optional<EPhaseGapCategory> opCategory; // filter to single category
};

} // namespace UniPlan
