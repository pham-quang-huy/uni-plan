#pragma once

#include "UniPlanBundleIndex.h"
#include "UniPlanTaxonomyTypes.h"
#include "UniPlanTypes.h"

#include <map>
#include <string>
#include <vector>

namespace UniPlan
{

// V4-native counters only. The V3-era fields (mPlaybookCount,
// mImplementationCount, mSidecarCount, mPairCount) were removed in
// v0.80.0 — they were ghost fields never populated after the
// .md → .Plan.json migration, and the INVENTORY panel that read them
// always displayed 0. A V4 bundle is the single source of truth, so
// mPlanCount (bundle count) is the canonical inventory measure.
struct FWatchInventoryCounters
{
    int mPlanCount = 0;
    int mActivePlanCount = 0;
    int mNonActivePlanCount = 0;
};

struct FWatchHeadingCheck
{
    std::string mSectionID;
    bool mbRequired = false;
    bool mbPresent = false;
    bool mbCanonical = true;
    int mLevel = 2;
};

struct FWatchSidecarSummary
{
    std::string mPath;
    std::string mOwnerKind;
    std::string mDocKind;
    std::string mPhaseKey;
    int mEntryCount = 0;
    std::string mLatestDate;
};

struct FWatchDocSchemaResult
{
    std::string mDocPath;
    std::string mDocType;
    std::string mPhaseKey;
    std::vector<FWatchHeadingCheck> mHeadings;
    int mRequiredCount = 0;
    int mRequiredPresent = 0;
    int mExtraCount = 0;
};

struct FWatchTopicSchemaResult
{
    FWatchDocSchemaResult mPlan;
    FWatchDocSchemaResult mImpl;
    std::vector<FWatchDocSchemaResult> mPlaybooks;
    FWatchDocSchemaResult mPlanChangeLog;
    FWatchDocSchemaResult mPlanVerification;
    FWatchDocSchemaResult mImplChangeLog;
    FWatchDocSchemaResult mImplVerification;
    std::vector<FWatchDocSchemaResult> mPlaybookChangeLogs;
    std::vector<FWatchDocSchemaResult> mPlaybookVerifications;
};

struct FWatchPlanSummary
{
    std::string mTopicKey;
    std::string mPlanPath;
    std::string mPlanStatus;
    int mPhaseCount = 0;
    int mPhaseCompleted = 0;
    int mPhaseInProgress = 0;
    int mPhaseNotStarted = 0;
    int mPhaseBlocked = 0;
    int mPhaseCanceled = 0;
    std::vector<PhaseItem> mPhases;
    std::vector<BlockerItem> mBlockers;
    std::vector<std::string> mSummaryLines;
    std::vector<std::string> mGoalStatements;
    std::vector<std::string> mNonGoalStatements;
    std::vector<FPhaseTaxonomy> mPhaseTaxonomies;
    std::vector<FWatchSidecarSummary> mSidecarSummaries;
    FWatchTopicSchemaResult mSchemaResult;
    // v0.89.0 typed arrays surfaced in PLAN DETAIL. Copied directly from
    // the bundle — no line splitting needed since each is a typed vector.
    std::vector<FRiskEntry> mRiskEntries;
    std::vector<FNextActionEntry> mNextActionEntries;
    std::vector<FAcceptanceCriterionEntry> mAcceptanceCriteria;
};

struct FWatchValidationSummary
{
    enum class EState
    {
        Pending,
        Running,
        Ready,
        Stale
    };

    int mTotalChecks = 0;
    int mPassedChecks = 0;
    int mFailedChecks = 0;
    int mErrorMajorCount = 0;
    int mErrorMinorCount = 0;
    int mWarningCount = 0;
    bool mbOk = true;
    EState mState = EState::Pending;
    std::string mStateMessage;
    std::vector<ValidateCheck> mFailedCheckDetails;
};

struct FWatchLintSummary
{
    enum class EState
    {
        Pending,
        Running,
        Ready,
        Stale
    };

    int mWarningCount = 0;
    int mNamePatternWarnings = 0;
    int mMissingH1Warnings = 0;
    EState mState = EState::Pending;
    std::string mStateMessage;
};

struct FDocWatchSnapshot
{
    FWatchInventoryCounters mInventory{};

    std::vector<FWatchPlanSummary> mActivePlans;
    std::vector<FWatchPlanSummary> mNonActivePlans;

    FWatchValidationSummary mValidation{};
    FWatchLintSummary mLint{};

    std::vector<BlockerItem> mAllBlockers;

    std::string mSnapshotAtUTC;
    std::string mRepoRoot;
    int mPollDurationMs = 0;
    std::vector<std::string> mWarnings;

    struct FPerformance
    {
        int mDiscoveryDurationMs = 0;
        int mValidationDurationMs = 0;
        int mLintDurationMs = 0;
        int mBundleReloadCount = 0;
        int mBundleReuseCount = 0;
        int mMetricRecomputeCount = 0;
        bool mbBundleSignatureChanged = false;
        bool mbMarkdownSignatureChanged = false;
        bool mbValidationRan = false;
        bool mbLintRan = false;
        bool mbForceRefresh = false;
    } mPerformance;
};

struct FWatchCachedBundle
{
    FFileFingerprint mFingerprint;
    FTopicBundle mBundle;
    FWatchPlanSummary mSummary;
};

struct FWatchSnapshotCache
{
    std::map<std::string, FWatchCachedBundle> mBundlesByPath;
    FWatchFileIndexResult mFileIndex;
    uint64_t mBundleSignature = 0;
    uint64_t mMarkdownSignature = 0;
    int mPollsSinceFullDiscovery = 0;
    bool mbFileIndexValid = false;
    bool mbBundleSignatureValid = false;
    bool mbMarkdownSignatureValid = false;
    bool mbValidationValid = false;
    bool mbLintValid = false;
    FWatchValidationSummary mValidation{};
    FWatchLintSummary mLint{};
};

struct FWatchSnapshotBuildOptions
{
    bool mbRunValidation = true;
    bool mbRunLint = true;
    bool mbMarkSkippedValidationRunning = false;
    bool mbMarkSkippedLintRunning = false;
};

FDocWatchSnapshot BuildWatchSnapshot(const std::string &InRepoRoot,
                                     bool InUseCache,
                                     const std::string &InCacheDir,
                                     bool InCacheVerbose);
FDocWatchSnapshot
BuildWatchSnapshotCached(const std::string &InRepoRoot, bool InUseCache,
                         const std::string &InCacheDir, bool InCacheVerbose,
                         FWatchSnapshotCache &InOutCache, bool InForceRefresh);
FDocWatchSnapshot
BuildWatchSnapshotCached(const std::string &InRepoRoot, bool InUseCache,
                         const std::string &InCacheDir, bool InCacheVerbose,
                         FWatchSnapshotCache &InOutCache, bool InForceRefresh,
                         const FWatchSnapshotBuildOptions &InOptions);

} // namespace UniPlan
