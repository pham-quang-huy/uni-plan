#pragma once

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

static constexpr const char *kCliVersion = "0.26.0";
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
static constexpr const char *kExtPlan = ".Plan.md";
static constexpr const char *kExtImpl = ".Impl.md";
static constexpr const char *kExtPlaybook = ".Playbook.md";

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

struct LintOptions : BaseOptions
{
    bool mbFailOnWarning = false;
};

struct InventoryOptions : BaseOptions
{
};

struct OrphanCheckOptions : BaseOptions
{
};

struct ArtifactsOptions : BaseOptions
{
    std::string mTopic;
    std::string mKind = "all";
};

struct PhaseOptions : BaseOptions
{
    std::string mTopic;
    std::string mStatus = "all";
};

struct EvidenceOptions : BaseOptions
{
    std::string mTopic;
    std::string mDocClass;
    std::string mPhaseKey;
};

struct SchemaOptions : BaseOptions
{
    std::string mType;
};

struct RulesOptions : BaseOptions
{
};

struct ValidateOptions : BaseOptions
{
    bool mbStrict = false;
};

struct SectionResolveOptions : BaseOptions
{
    std::string mDocPath;
    std::string mSection;
};

struct SectionSchemaOptions : BaseOptions
{
    std::string mType = "doc";
};

struct SectionListOptions : BaseOptions
{
    std::string mDocPath;
    bool mbCount = false;
};

struct SectionContentOptions : BaseOptions
{
    std::string mDocPath;
    std::string mSection;
    int mLineCharLimit = 0;
};

struct ExcerptOptions : BaseOptions
{
    std::string mDocPath;
    std::string mSection;
    int mContextLines = 2;
};

struct TableListOptions : BaseOptions
{
    std::string mDocPath;
};

struct TableGetOptions : BaseOptions
{
    std::string mDocPath;
    int mTableId = 0;
};

struct GraphOptions : BaseOptions
{
    std::string mTopic;
    int mDepth = 2;
};

struct DiagnoseDriftOptions : BaseOptions
{
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

struct StatusInference
{
    std::string mRaw;
    std::string mNormalized = "unknown";
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

struct InventoryItem
{
    std::string mPath;
    int mLineCount = 0;
    std::string mLastCommit;
};

struct InventoryResult : FBaseResult
{
    std::vector<InventoryItem> mItems;
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
    std::string mId;
    bool mbOk = true;
    bool mbCritical = false;
    std::string mDetail;
    std::string mRuleId;
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

struct TimelineItem
{
    std::string mDate;
    std::string mDocClass;
    std::string mPhaseKey;
    std::string mSourcePath;
    std::string mUpdate;
    std::string mEvidence;
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

struct SectionCount
{
    std::string mHeading;
    std::string mSectionId;
    int mCount = 0;
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
