#pragma once

#include "UniPlanEnums.h"
#include "UniPlanTaxonomyTypes.h"

#include <optional>
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
    bool mbBrief = false;     // --brief: compact view for session resume
    bool mbExecution = false; // --execution: jobs/tasks only
    bool mbReference = false; // --reference: design material only
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

struct FTopicSetOptions : BaseOptions
{
    std::string mTopic;
    std::optional<ETopicStatus> opStatus;
    std::string mNextActions;
    // Metadata fields
    std::string mSummary;
    std::string mGoals;
    std::string mNonGoals;
    std::string mRisks;
    std::string mAcceptanceCriteria;
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
