#include "UniPlanWatchSnapshot.h"
#include "UniPlanBundleWriteGuard.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanJSONIO.h"
#include "UniPlanPhaseMetrics.h"
#include "UniPlanTopicTypes.h" // ComputePhaseDesignChars
#include "UniPlanTypes.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace UniPlan
{

static void SplitLines(const std::string &InText,
                       std::vector<std::string> &OutLines)
{
    std::istringstream Stream(InText);
    std::string Line;
    while (std::getline(Stream, Line))
    {
        if (!Line.empty())
            OutLines.push_back(Line);
    }
}

static FWatchPlanSummary
BuildPlanSummaryFromBundle(const FTopicBundle &InBundle)
{
    FWatchPlanSummary Summary;
    Summary.mTopicKey = InBundle.mTopicKey;
    Summary.mPlanPath = "Docs/Plans/" + InBundle.mTopicKey + ".Plan.json";
    Summary.mPlanStatus = ToString(InBundle.mStatus);
    Summary.mPhaseCount = static_cast<int>(InBundle.mPhases.size());

    // Build PhaseItem list from bundle phases. Pure V4 projection — every
    // column the watch TUI shows reads typed members off FPhaseRecord.
    // Runtime metrics feed the PHASE DETAIL `Design` column and metrics
    // view. They are computed from the loaded bundle only and never written
    // back into .Plan.json.
    for (size_t I = 0; I < InBundle.mPhases.size(); ++I)
    {
        const FPhaseRecord &Phase = InBundle.mPhases[I];
        PhaseItem Item;
        Item.mPhaseKey = std::to_string(I);
        Item.mStatus = Phase.mLifecycle.mStatus;
        Item.mStatusRaw = ToString(Phase.mLifecycle.mStatus);
        Item.mScope = Phase.mScope;
        Item.mOutput = Phase.mOutput;
        Item.mDone = Phase.mLifecycle.mDone;
        Item.mRemaining = Phase.mLifecycle.mRemaining;
        Item.mMetrics = ComputePhaseDepthMetrics(InBundle, I);
        Item.mV4DesignChars = Item.mMetrics.mDesignChars;
        Summary.mPhases.push_back(std::move(Item));

        switch (Phase.mLifecycle.mStatus)
        {
        case EExecutionStatus::Completed:
            Summary.mPhaseCompleted++;
            break;
        case EExecutionStatus::InProgress:
            Summary.mPhaseInProgress++;
            break;
        case EExecutionStatus::NotStarted:
            Summary.mPhaseNotStarted++;
            break;
        case EExecutionStatus::Blocked:
            Summary.mPhaseBlocked++;
            break;
        case EExecutionStatus::Canceled:
            Summary.mPhaseCanceled++;
            break;
        }
    }

    // Execution taxonomy — one entry per phase with bundle-native
    // jobs/lanes. V3 legacy .md discovery was removed in v0.80.0 along
    // with the PB / PBLines columns that depended on it; `legacy-gap`
    // remains the canonical CLI for per-phase V3 ↔ V4 parity audits.
    for (size_t I = 0; I < InBundle.mPhases.size(); ++I)
    {
        const FPhaseRecord &Phase = InBundle.mPhases[I];
        if (Phase.mJobs.empty() && Phase.mLanes.empty())
        {
            continue;
        }

        FPhaseTaxonomy Taxonomy;
        Taxonomy.mPhaseIndex = static_cast<int>(I);
        Taxonomy.mLanes = Phase.mLanes;
        Taxonomy.mJobs = Phase.mJobs;
        Taxonomy.mFileManifest = Phase.mFileManifest;

        for (const FJobRecord &Job : Phase.mJobs)
        {
            for (const FTaskRecord &Task : Job.mTasks)
                Taxonomy.mTasks.push_back(Task);
        }

        std::set<int> UniqueWaves;
        for (const FJobRecord &Job : Phase.mJobs)
            UniqueWaves.insert(Job.mWave);
        Taxonomy.mWaveCount = static_cast<int>(UniqueWaves.size());

        Summary.mPhaseTaxonomies.push_back(std::move(Taxonomy));
    }

    // Plan metadata
    SplitLines(InBundle.mMetadata.mSummary, Summary.mSummaryLines);
    SplitLines(InBundle.mMetadata.mGoals, Summary.mGoalStatements);
    SplitLines(InBundle.mMetadata.mNonGoals, Summary.mNonGoalStatements);
    // v0.89.0 typed arrays — pass-through; the panel renderer handles
    // per-entry formatting and severity/status color.
    Summary.mRiskEntries = InBundle.mMetadata.mRisks;
    Summary.mNextActionEntries = InBundle.mNextActions;
    Summary.mAcceptanceCriteria = InBundle.mMetadata.mAcceptanceCriteria;

    // Blockers — single source of truth. Populate the per-plan list so
    // the watch BLOCKERS panel and the per-plan blocker count come from
    // the same data (count is derived via mBlockers.size(), not a
    // separately-maintained integer).
    Summary.mBlockers = CollectBundleBlockers(InBundle);

    return Summary;
}

static FWatchValidationSummary
BuildWatchValidationSummary(const std::vector<FTopicBundle> &InBundles,
                            const fs::path &InRepoRoot)
{
    FWatchValidationSummary Validation;
    const std::vector<ValidateCheck> Checks =
        ValidateAllBundles(InBundles, InRepoRoot);

    for (const ValidateCheck &Check : Checks)
    {
        if (Check.mbOk)
        {
            Validation.mPassedChecks++;
        }
        else
        {
            Validation.mFailedChecks++;
            if (Check.mSeverity == EValidationSeverity::ErrorMajor)
            {
                Validation.mErrorMajorCount++;
            }
            else if (Check.mSeverity == EValidationSeverity::ErrorMinor)
            {
                Validation.mErrorMinorCount++;
            }
            else
            {
                Validation.mWarningCount++;
            }
            Validation.mFailedCheckDetails.push_back(Check);
        }
    }
    Validation.mTotalChecks = static_cast<int>(Checks.size());
    Validation.mbOk = (Validation.mErrorMajorCount == 0);
    return Validation;
}

static FWatchLintSummary BuildWatchLintSummary(const std::string &InRepoRoot)
{
    FWatchLintSummary Summary;
    const LintResult Lint = BuildLintResult(InRepoRoot, /*InQuiet=*/true);
    Summary.mWarningCount = Lint.mWarningCount;
    Summary.mNamePatternWarnings = Lint.mNamePatternWarningCount;
    Summary.mMissingH1Warnings = Lint.mMissingH1WarningCount;
    return Summary;
}

static bool TryLoadIndexedBundle(const FBundleFileIndexEntry &InEntry,
                                 FTopicBundle &OutBundle,
                                 std::string &OutWarning)
{
    std::string Error;
    if (!TryReadTopicBundle(fs::path(InEntry.mFingerprint.mPath), OutBundle,
                            Error))
    {
        OutWarning = "Failed to read " + InEntry.mFingerprint.mRelativePath +
                     ": " + Error;
        return false;
    }

    OutBundle.mBundlePath = InEntry.mFingerprint.mPath;
    std::string SessionError;
    if (!CaptureReadSession(fs::path(InEntry.mFingerprint.mPath),
                            OutBundle.mReadSession, SessionError))
    {
        OutWarning = "Bundle read-session capture failed for " +
                     InEntry.mFingerprint.mRelativePath + ": " + SessionError;
        return false;
    }
    return true;
}

static bool IsBundleCached(const FWatchSnapshotCache &InCache,
                           const FBundleFileIndexEntry &InEntry)
{
    const auto It = InCache.mBundlesByPath.find(InEntry.mFingerprint.mPath);
    return It != InCache.mBundlesByPath.end() &&
           It->second.mFingerprint == InEntry.mFingerprint;
}

static void RemoveDeletedBundlesFromCache(
    FWatchSnapshotCache &InOutCache,
    const std::vector<FBundleFileIndexEntry> &InCurrentBundles)
{
    std::set<std::string> CurrentPaths;
    for (const FBundleFileIndexEntry &Entry : InCurrentBundles)
    {
        CurrentPaths.insert(Entry.mFingerprint.mPath);
    }

    for (auto It = InOutCache.mBundlesByPath.begin();
         It != InOutCache.mBundlesByPath.end();)
    {
        if (CurrentPaths.count(It->first) == 0)
        {
            It = InOutCache.mBundlesByPath.erase(It);
        }
        else
        {
            ++It;
        }
    }
}

static void AddPlanSummaryToSnapshot(FDocWatchSnapshot &InOutSnapshot,
                                     const FWatchPlanSummary &InSummary)
{
    if (InSummary.mPlanStatus == ToString(ETopicStatus::InProgress))
    {
        InOutSnapshot.mActivePlans.push_back(InSummary);
    }
    else
    {
        InOutSnapshot.mNonActivePlans.push_back(InSummary);
    }
}

FDocWatchSnapshot BuildWatchSnapshotCached(const std::string &InRepoRoot,
                                           const bool InUseCache,
                                           const std::string &InCacheDir,
                                           const bool InCacheVerbose,
                                           FWatchSnapshotCache &InOutCache,
                                           const bool InForceRefresh)
{
    (void)InCacheDir;
    const auto StartTime = std::chrono::steady_clock::now();

    FDocWatchSnapshot Snapshot;
    Snapshot.mRepoRoot = InRepoRoot;
    Snapshot.mSnapshotAtUTC = GetUtcNow();
    Snapshot.mPerformance.mbForceRefresh = InForceRefresh;

    const fs::path RepoRoot = NormalizeRepoRootPath(InRepoRoot);

    const auto DiscoveryStart = std::chrono::steady_clock::now();
    FBundleFileIndexResult BundleIndex;
    std::string BundleIndexError;
    if (!TryBuildBundleFileIndex(RepoRoot, BundleIndex, BundleIndexError))
    {
        throw std::runtime_error(BundleIndexError);
    }
    Snapshot.mWarnings.insert(Snapshot.mWarnings.end(),
                              BundleIndex.mWarnings.begin(),
                              BundleIndex.mWarnings.end());

    FMarkdownFileIndexResult MarkdownIndex;
    std::string MarkdownIndexError;
    if (!TryBuildMarkdownFileIndex(RepoRoot, MarkdownIndex, MarkdownIndexError))
    {
        throw std::runtime_error(MarkdownIndexError);
    }
    Snapshot.mWarnings.insert(Snapshot.mWarnings.end(),
                              MarkdownIndex.mWarnings.begin(),
                              MarkdownIndex.mWarnings.end());
    const auto DiscoveryEnd = std::chrono::steady_clock::now();
    Snapshot.mPerformance.mDiscoveryDurationMs =
        static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                             DiscoveryEnd - DiscoveryStart)
                             .count());

    const bool bBundleSignatureChanged =
        InForceRefresh || !InUseCache || !InOutCache.mbBundleSignatureValid ||
        InOutCache.mBundleSignature != BundleIndex.mSignature;
    const bool bMarkdownSignatureChanged =
        InForceRefresh || !InUseCache || !InOutCache.mbMarkdownSignatureValid ||
        InOutCache.mMarkdownSignature != MarkdownIndex.mSignature;
    Snapshot.mPerformance.mbBundleSignatureChanged = bBundleSignatureChanged;
    Snapshot.mPerformance.mbMarkdownSignatureChanged =
        bMarkdownSignatureChanged;

    if (!InUseCache)
    {
        InOutCache = FWatchSnapshotCache{};
    }
    else if (bBundleSignatureChanged)
    {
        RemoveDeletedBundlesFromCache(InOutCache, BundleIndex.mBundles);
    }

    std::vector<FTopicBundle> Bundles;
    Bundles.reserve(BundleIndex.mBundles.size());

    for (const FBundleFileIndexEntry &IndexedBundle : BundleIndex.mBundles)
    {
        if (InUseCache && !InForceRefresh &&
            IsBundleCached(InOutCache, IndexedBundle))
        {
            const FWatchCachedBundle &Cached =
                InOutCache.mBundlesByPath[IndexedBundle.mFingerprint.mPath];
            Bundles.push_back(Cached.mBundle);
            AddPlanSummaryToSnapshot(Snapshot, Cached.mSummary);
            Snapshot.mPerformance.mBundleReuseCount++;
            continue;
        }

        FTopicBundle Bundle;
        std::string Warning;
        if (!TryLoadIndexedBundle(IndexedBundle, Bundle, Warning))
        {
            Snapshot.mWarnings.push_back(Warning);
            continue;
        }

        FWatchPlanSummary Summary = BuildPlanSummaryFromBundle(Bundle);
        Snapshot.mPerformance.mBundleReloadCount++;
        Snapshot.mPerformance.mMetricRecomputeCount +=
            static_cast<int>(Bundle.mPhases.size());
        Bundles.push_back(Bundle);
        AddPlanSummaryToSnapshot(Snapshot, Summary);

        if (InUseCache)
        {
            FWatchCachedBundle Cached;
            Cached.mFingerprint = IndexedBundle.mFingerprint;
            Cached.mBundle = std::move(Bundle);
            Cached.mSummary = std::move(Summary);
            InOutCache.mBundlesByPath[IndexedBundle.mFingerprint.mPath] =
                std::move(Cached);
        }
    }

    Snapshot.mInventory.mPlanCount = static_cast<int>(Bundles.size());
    Snapshot.mInventory.mActivePlanCount =
        static_cast<int>(Snapshot.mActivePlans.size());
    Snapshot.mInventory.mNonActivePlanCount =
        static_cast<int>(Snapshot.mNonActivePlans.size());

    if (InUseCache && !bBundleSignatureChanged && InOutCache.mbValidationValid)
    {
        Snapshot.mValidation = InOutCache.mValidation;
    }
    else
    {
        const auto ValidationStart = std::chrono::steady_clock::now();
        Snapshot.mValidation = BuildWatchValidationSummary(Bundles, RepoRoot);
        const auto ValidationEnd = std::chrono::steady_clock::now();
        Snapshot.mPerformance.mbValidationRan = true;
        Snapshot.mPerformance.mValidationDurationMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                ValidationEnd - ValidationStart)
                .count());
        if (InUseCache)
        {
            InOutCache.mValidation = Snapshot.mValidation;
            InOutCache.mbValidationValid = true;
        }
    }

    if (InUseCache && !bMarkdownSignatureChanged && InOutCache.mbLintValid)
    {
        Snapshot.mLint = InOutCache.mLint;
    }
    else
    {
        const auto LintStart = std::chrono::steady_clock::now();
        Snapshot.mLint = BuildWatchLintSummary(InRepoRoot);
        const auto LintEnd = std::chrono::steady_clock::now();
        Snapshot.mPerformance.mbLintRan = true;
        Snapshot.mPerformance.mLintDurationMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(LintEnd -
                                                                  LintStart)
                .count());
        if (InUseCache)
        {
            InOutCache.mLint = Snapshot.mLint;
            InOutCache.mbLintValid = true;
        }
    }

    // Aggregate all blockers
    for (const FWatchPlanSummary &Plan : Snapshot.mActivePlans)
    {
        Snapshot.mAllBlockers.insert(Snapshot.mAllBlockers.end(),
                                     Plan.mBlockers.begin(),
                                     Plan.mBlockers.end());
    }

    if (InUseCache)
    {
        InOutCache.mBundleSignature = BundleIndex.mSignature;
        InOutCache.mMarkdownSignature = MarkdownIndex.mSignature;
        InOutCache.mbBundleSignatureValid = true;
        InOutCache.mbMarkdownSignatureValid = true;
    }

    if (InCacheVerbose)
    {
        std::cerr << "[watch-cache] reload="
                  << Snapshot.mPerformance.mBundleReloadCount
                  << " reuse=" << Snapshot.mPerformance.mBundleReuseCount
                  << " metrics=" << Snapshot.mPerformance.mMetricRecomputeCount
                  << " validation="
                  << (Snapshot.mPerformance.mbValidationRan ? "run" : "reuse")
                  << " lint="
                  << (Snapshot.mPerformance.mbLintRan ? "run" : "reuse")
                  << "\n";
    }

    const auto EndTime = std::chrono::steady_clock::now();
    Snapshot.mPollDurationMs =
        static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                             EndTime - StartTime)
                             .count());

    return Snapshot;
}

FDocWatchSnapshot BuildWatchSnapshot(const std::string &InRepoRoot,
                                     const bool InUseCache,
                                     const std::string &InCacheDir,
                                     const bool InCacheVerbose)
{
    FWatchSnapshotCache Cache;
    return BuildWatchSnapshotCached(InRepoRoot, InUseCache, InCacheDir,
                                    InCacheVerbose, Cache,
                                    /*InForceRefresh=*/true);
}

} // namespace UniPlan
