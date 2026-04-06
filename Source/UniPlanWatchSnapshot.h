#pragma once

#include "UniPlanTypes.h"

#include <string>
#include <vector>

namespace UniPlan
{

struct FWatchInventoryCounters
{
    int mPlanCount = 0;
    int mPlaybookCount = 0;
    int mImplementationCount = 0;
    int mSidecarCount = 0;
    int mPairCount = 0;
    int mActivePlanCount = 0;
    int mNonActivePlanCount = 0;
};

struct FWatchLaneItem
{
    std::string mLaneID;
    std::string mStatus;
    std::string mScope;
    std::string mExitCriteria;
};

struct FWatchJobItem
{
    std::string mWaveID;
    std::string mLaneID;
    std::string mJobID;
    std::string mJobName;
    std::string mStatus;
    std::string mScope;
    std::string mExitCriteria;
};

struct FWatchTaskItem
{
    std::string mJobRef;
    std::string mTaskID;
    std::string mStatus;
    std::string mDescription;
    std::string mEvidence;
};

struct FWatchFileManifestItem
{
    std::string mFilePath;
    std::string mAction;
    std::string mDescription;
};

struct FWatchPhaseTaxonomy
{
    std::string mPhaseKey;
    std::string mPlaybookPath;
    std::vector<FWatchLaneItem> mLanes;
    std::vector<FWatchJobItem> mJobs;
    std::vector<FWatchTaskItem> mTasks;
    std::vector<FWatchFileManifestItem> mFileManifest;
    int mWaveCount = 0;
    int mPlaybookLineCount = 0;
};

struct FWatchHeadingCheck
{
    std::string mSectionId;
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
    int mPlaybookCount = 0;
    int mBlockerCount = 0;
    std::vector<PhaseItem> mPhases;
    std::vector<BlockerItem> mBlockers;
    std::vector<FWatchPhaseTaxonomy> mPhaseTaxonomies;
    std::vector<FWatchSidecarSummary> mSidecarSummaries;
    FWatchTopicSchemaResult mSchemaResult;
};

struct FWatchValidationSummary
{
    int mTotalChecks = 0;
    int mPassedChecks = 0;
    int mFailedChecks = 0;
    int mCriticalFailures = 0;
    bool mbOk = true;
    std::vector<ValidateCheck> mFailedCheckDetails;
};

struct FWatchLintSummary
{
    int mWarningCount = 0;
    int mNamePatternWarnings = 0;
    int mMissingH1Warnings = 0;
};

struct FDocWatchSnapshot
{
    FWatchInventoryCounters mInventory{};

    std::vector<FWatchPlanSummary> mActivePlans;
    std::vector<FWatchPlanSummary> mNonActivePlans;

    FWatchValidationSummary mValidation{};
    FWatchLintSummary mLint{};

    std::vector<DriftItem> mDriftItems;
    std::vector<BlockerItem> mAllBlockers;

    std::string mSnapshotAtUTC;
    std::string mRepoRoot;
    int mPollDurationMs = 0;
};

FDocWatchSnapshot BuildWatchSnapshot(const std::string& InRepoRoot, bool InUseCache, const std::string& InCacheDir, bool InCacheVerbose);

} // namespace UniPlan
