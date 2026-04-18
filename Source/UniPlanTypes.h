#pragma once

#include "UniPlanEnums.h"
#include "UniPlanTaxonomyTypes.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace UniPlan
{

// ---------------------------------------------------------------------------
// CLI version and JSON schema constants
// ---------------------------------------------------------------------------

static constexpr const char *kCliVersion = "0.68.0";
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
// CLAUDE.md → documentation_rules.
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

// ---------------------------------------------------------------------------
// INI data typedef
// ---------------------------------------------------------------------------

using IniData = std::map<std::string, std::map<std::string, std::string>>;

// ---------------------------------------------------------------------------
// Enum classes
// ---------------------------------------------------------------------------

enum class EDocumentKind
{
    Plan,
    Playbook,
    Implementation
};

// ---------------------------------------------------------------------------
// Core domain structs
// ---------------------------------------------------------------------------

struct DocConfig
{
    std::string mCacheDir; // [cache] dir — empty means use built-in default
    bool mbCacheEnabled = true;  // [cache] enabled — global toggle
    bool mbCacheVerbose = false; // [cache] verbose — print hit/miss to stderr
};

struct DocumentRecord
{
    EDocumentKind mKind = EDocumentKind::Plan;
    std::string mTopicKey;
    std::string mPhaseKey;
    std::string mStatusRaw;
    std::string mStatus;
    std::string mPath;
};

struct SidecarRecord
{
    std::string mTopicKey;
    std::string mPhaseKey;
    std::string mOwnerKind;
    std::string mDocKind;
    std::string mPath;
};

struct TopicPairRecord
{
    std::string mTopicKey;
    std::string mPlanPath;
    std::string mPlanStatus;
    std::string mImplementationPath;
    std::string mImplementationStatus;
    std::string mOverallStatus;
    std::vector<DocumentRecord> mPlaybooks;
    std::string mPairState;
};

struct Inventory
{
    std::string mGeneratedUtc;
    std::string mRepoRoot;
    std::vector<DocumentRecord> mPlans;
    std::vector<DocumentRecord> mPlaybooks;
    std::vector<DocumentRecord> mImplementations;
    std::vector<SidecarRecord> mSidecars;
    std::vector<TopicPairRecord> mPairs;
    std::vector<std::string> mWarnings;
};

struct MarkdownSignatureEntry
{
    std::string mPath;
    uint64_t mWriteTime = 0;
    uint64_t mFileSize = 0;
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

struct ListOptions : BaseOptions
{
    std::string mKind;
    std::string mStatus = "all";
};

struct ValidateOptions : BaseOptions
{
    bool mbStrict = false;
};

struct TimelineOptions : BaseOptions
{
    std::string mTopic;
    std::string mSince;
};

struct BlockersOptions : BaseOptions
{
    std::string mStatus = "open";
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

struct PlanCommandOptions : BaseOptions
{
    std::string mSubcommand; // "info", "field", "update",
                             // "create", "init", "archive"
    std::string mTopic;
    std::string mSection;
    std::string mField;
    std::string mValue;
    std::string mContent;
    std::string mTitle;
    std::string mPhases;
    std::string mReason;
    std::string mTemplate = "minimal";
};

struct PhaseCommandOptions : BaseOptions
{
    std::string mSubcommand; // "detail", "transition",
                             // "add", "remove"
    std::string mTopic;
    std::string mPhaseKey;
    std::string mToStatus;
    std::string mScope;
    bool mbForce = false;
};

static constexpr const char *kPlanInfoSchema = "uni-plan-plan-info-v1";
static constexpr const char *kPlanFieldSchema = "uni-plan-plan-field-v1";
static constexpr const char *kPlanMutationSchema = "uni-plan-plan-mutation-v1";
static constexpr const char *kPhaseDetailSchema = "uni-plan-phase-detail-v1";
static constexpr const char *kPhaseTransitionSchema =
    "uni-plan-phase-transition-v1";

// V4 bundle-native option structs
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

// Mutation option structs
static constexpr const char *kMutationSchema = "uni-plan-mutation-v1";

struct FTopicSetOptions : BaseOptions
{
    std::string mTopic;
    std::string mStatus;
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
    std::string mStatus; // optional; default not_started
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
    std::string mStatus;
    std::string mDone;
    std::string mRemaining;
    std::string mBlockers;
    std::string mContext; // agent_context
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
};

struct FJobSetOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mJobIndex = -1;
    std::string mStatus;
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
    std::string mStatus;
    std::string mEvidence;
    std::string mNotes;
};

struct FChangelogAddOptions : BaseOptions
{
    std::string mTopic;
    std::string mScope;
    std::string mChange;
    std::string mType = "chore";
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

// Tier 5 — Missing entity coverage
struct FLaneSetOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mLaneIndex = -1;
    std::string mStatus;
    std::string mScope;
    std::string mExitCriteria;
};

struct FTestingAddOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::string mSession;
    std::string mActor;
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
    std::string mAction;
    std::string mDescription;
};

// Modify-existing array entries (unlike add, these target an index)

struct FTestingSetOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mIndex = -1;
    std::string mSession;
    std::string mActor;
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

struct FManifestSetOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    int mIndex = -1;
    std::string mFile;
    std::string mAction;
    std::string mDescription;
};

struct FLaneAddOptions : BaseOptions
{
    std::string mTopic;
    int mPhaseIndex = -1;
    std::string mStatus;
    std::string mScope;
    std::string mExitCriteria;
};

struct FChangelogSetOptions : BaseOptions
{
    std::string mTopic;
    int mIndex = -1;
    int mPhase = -2; // -2 = unchanged; -1 = topic-level; >=0 = phase index
    std::string mDate;
    std::string mChange;
    std::string mType;
    std::string mAffected;
};

// ---------------------------------------------------------------------------
// Error types
// ---------------------------------------------------------------------------

struct UsageError : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------------------
// Result and data structs
// ---------------------------------------------------------------------------

struct StatusCounters
{
    int mNotStarted = 0;
    int mInProgress = 0;
    int mCompleted = 0;
    int mClosed = 0;
    int mBlocked = 0;
    int mCanceled = 0;
    std::string mFirstRaw;
};

struct MarkdownDocument
{
    fs::path mAbsolutePath;
    std::string mRelativePath;
};

struct FBaseResult
{
    std::string mGeneratedUtc;
    std::string mRepoRoot;
    std::vector<std::string> mWarnings;
};

struct LintResult : FBaseResult
{
    int mWarningCount = 0;
    int mNamePatternWarningCount = 0;
    int mMissingH1WarningCount = 0;
};

struct OrphanCheckResult : FBaseResult
{
    std::vector<std::string> mOrphans;
    std::vector<std::string> mIgnoredRoots;
};

struct CacheInfoResult
{
    std::string mGeneratedUtc;
    std::string mCacheDir;
    std::string mConfigCacheDir;
    std::string mIniPath;
    bool mbCacheEnabled = true;
    bool mbCacheVerbose = false;
    bool mbCacheExists = false;
    uint64_t mCacheSizeBytes = 0;
    int mCacheEntryCount = 0;
    std::string mCurrentRepoCachePath;
    bool mbCurrentRepoCacheExists = false;
    std::vector<std::string> mWarnings;
};

struct CacheClearResult
{
    std::string mGeneratedUtc;
    std::string mCacheDir;
    int mEntriesRemoved = 0;
    uint64_t mBytesFreed = 0;
    bool mbSuccess = true;
    std::string mError;
    std::vector<std::string> mWarnings;
};

struct CacheConfigResult
{
    std::string mGeneratedUtc;
    std::string mIniPath;
    bool mbSuccess = true;
    std::string mError;
    std::string mDir;
    bool mbEnabled = true;
    bool mbVerbose = false;
    std::vector<std::string> mWarnings;
};

struct HeadingRecord
{
    int mLine = 0;
    int mLevel = 0;
    std::string mText;
    std::string mSectionId;
};

struct MarkdownTableRecord
{
    int mTableId = 0;
    int mStartLine = 0;
    int mEndLine = 0;
    std::string mSectionId;
    std::string mSectionHeading;
    std::vector<std::string> mHeaders;
    std::vector<std::vector<std::string>> mRows;
};

struct SectionResolution
{
    bool mbFound = false;
    std::string mSectionQuery;
    std::string mSectionId;
    std::string mSectionHeading;
    int mLevel = 0;
    int mStartLine = 0;
    int mEndLine = 0;
};

struct EvidenceEntry
{
    std::string mSourcePath;
    std::string mPhaseKey;
    int mTableId = 0;
    int mRowIndex = 0;
    std::vector<std::pair<std::string, std::string>> mFields;
};

struct RuleEntry
{
    std::string mId;
    std::string mDescription;
    std::string mSource;
    std::string mSourcePath;
    std::string mSourceSectionId;
    int mSourceTableId = 0;
    int mSourceRowIndex = 0;
    std::string mSourceEvidence;
    bool mbSourceResolved = false;
};

struct RuleProvenanceProbe
{
    std::string mPath;
    std::string mSectionId;
    std::vector<std::string> mRowTerms;
};

struct SchemaField
{
    std::string mSectionId;
    std::string mProperty;
    std::string mValue;
};

struct ValidateCheck
{
    std::string mID;
    EValidationSeverity mSeverity = EValidationSeverity::Warning;
    bool mbOk = true;
    std::string mTopic; // which bundle (empty for aggregate checks)
    std::string mPath;  // e.g. "phases[2].jobs[1].tasks[0]"
    std::string mDetail;
    int mLine = -1; // 1-based line in the bundle JSON; -1 = unknown
    std::vector<std::string> mDiagnostics;
};

struct GraphNode
{
    std::string mId;
    std::string mType;
    std::string mPath;
    std::string mTopicKey;
    std::string mPhaseKey;
    std::string mOwnerKind;
    std::string mDocKind;
};

struct GraphEdge
{
    std::string mFromNodeId;
    std::string mToNodeId;
    std::string mKind;
    int mDepth = 0;
};

struct DriftItem
{
    std::string mId;
    std::string mSeverity;
    std::string mTopicKey;
    std::string mPath;
    std::string mMessage;
};

struct BlockerItem
{
    std::string mTopicKey;
    std::string mSourcePath;
    std::string mKind;
    std::string mStatus;
    std::string mPhaseKey;
    std::string mPriority;
    std::string mAction;
    std::string mOwner;
    std::string mNotes;
};

struct PhaseItem
{
    std::string mPhaseKey;
    std::string mStatusRaw;
    std::string mStatus;
    std::string mPlaybookPath;
    std::string mDescription;
    std::string mNextAction;
    int mTableId = 0;
    int mRowIndex = 0;
    std::vector<std::pair<std::string, std::string>> mFields;
};

struct PhaseListAllEntry
{
    std::string mTopicKey;
    std::string mPlanPath;
    std::string mPlanStatus;
    std::vector<PhaseItem> mPhases;
};

struct FCommandHelpEntry
{
    const char *mName;
    const char *mUsageLine;
    const char *mDescription;
    const char *mRequiredOptions;
    const char *mSpecificOptions;
    const char *mHumanLabel;
    const char *mExamples;
};

// ---------------------------------------------------------------------------
// Validation result structs
// ---------------------------------------------------------------------------

struct ActivePhaseRecord
{
    std::string mTopicKey;
    std::string mPlanPath;
    std::string mPhaseKey;
    std::string mStatusRaw;
    std::string mStatus;
};

struct PhaseEntryGateResult
{
    int mActivePhaseCount = 0;
    int mMissingPlaybookCount = 0;
    int mUnpreparedPlaybookCount = 0;
};

struct ArtifactRoleBoundaryResult
{
    int mPlaybookViolationCount = 0;
    int mImplementationViolationCount = 0;
};

struct PlanSchemaValidationResult
{
    int mPlanCount = 0;
    int mReadFailureCount = 0;
    int mMissingRequiredPlanCount = 0;
    int mOrderDriftPlanCount = 0;
    int mLiteralMismatchPlanCount = 0;
    int mHeadingCheckedCount = 0;
    int mHeadingNonCompliantCount = 0;
    int mHeadingIndexedPrefixCount = 0;
    int mHeadingNamingDriftPlanCount = 0;
    int mHeadingIndexedPrefixPlanCount = 0;
    std::vector<std::string> mMissingRequiredDiagnostics;
    std::vector<std::string> mOrderDriftDiagnostics;
    std::vector<std::string> mLiteralMismatchDiagnostics;
    std::vector<std::string> mHeadingNamingDiagnostics;
    std::vector<std::string> mHeadingIndexedPrefixDiagnostics;
};

struct BlankSectionsResult
{
    int mPlanCount = 0;
    int mReadFailureCount = 0;
    int mBlankSectionPlanCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct CrossStatusResult
{
    int mTopicCount = 0;
    int mMismatchCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct PlaybookSchemaResult
{
    int mPlaybookCount = 0;
    int mReadFailureCount = 0;
    int mMissingSectionPlaybookCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct LinkIntegrityResult
{
    int mDocCount = 0;
    int mBrokenLinkCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct TaxonomyJobCompletenessResult
{
    int mPlaybookCount = 0;
    int mIncompleteJobCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct TaxonomyTaskTraceabilityResult
{
    int mPlaybookCount = 0;
    int mUntraceableTaskCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct ValidationHeadingOwnershipResult
{
    int mPlanViolationCount = 0;
    int mImplViolationCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct TestingActorCoverageResult
{
    int mPlaybookCount = 0;
    int mMissingActorPlaybookCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct HeadingAliasResult
{
    int mDocCount = 0;
    int mAliasDocCount = 0;
    int mAliasHeadingCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct ImplSchemaValidationResult
{
    int mImplCount = 0;
    int mReadFailureCount = 0;
    int mMissingRequiredImplCount = 0;
    std::vector<std::string> mMissingRequiredDiagnostics;
    int mOrderDriftImplCount = 0;
    std::vector<std::string> mOrderDriftDiagnostics;
    int mHeadingCheckedCount = 0;
    int mHeadingNonCompliantCount = 0;
    int mHeadingNamingDriftImplCount = 0;
    std::vector<std::string> mHeadingNamingDiagnostics;
    int mHeadingIndexedPrefixCount = 0;
    int mHeadingIndexedPrefixImplCount = 0;
    std::vector<std::string> mHeadingIndexedPrefixDiagnostics;
};

struct PlaybookOrderResult
{
    int mPlaybookCount = 0;
    int mReadFailureCount = 0;
    int mOrderDriftPlaybookCount = 0;
    std::vector<std::string> mOrderDriftDiagnostics;
};

struct PlaybookHeadingNamingResult
{
    int mPlaybookCount = 0;
    int mReadFailureCount = 0;
    int mHeadingCheckedCount = 0;
    int mHeadingNonCompliantCount = 0;
    int mHeadingNamingDriftPlaybookCount = 0;
    std::vector<std::string> mHeadingNamingDiagnostics;
    int mHeadingIndexedPrefixCount = 0;
    int mHeadingIndexedPrefixPlaybookCount = 0;
    std::vector<std::string> mHeadingIndexedPrefixDiagnostics;
};

struct PlaybookBlankSectionsResult
{
    int mPlaybookCount = 0;
    int mReadFailureCount = 0;
    int mBlankSectionPlaybookCount = 0;
    std::vector<std::string> mDiagnostics;
};

struct SectionSchemaEntry
{
    std::string mSectionId;
    bool mbRequired = false;
    int mOrder = 0;
};

struct ResolvedDocument
{
    fs::path mRepoRoot;
    fs::path mAbsolutePath;
    std::string mRelativePath;
    std::vector<std::string> mLines;
    std::vector<HeadingRecord> mHeadings;
};

} // namespace UniPlan
