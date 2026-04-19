#include "UniPlanWatchSnapshot.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"

#include <chrono>
#include <sstream>
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
BuildPlanSummaryFromBundle(const FTopicBundle &InBundle,
                           const fs::path &InRepoRoot)
{
    FWatchPlanSummary Summary;
    Summary.mTopicKey = InBundle.mTopicKey;
    Summary.mPlanPath = "Docs/Plans/" + InBundle.mTopicKey + ".Plan.json";
    Summary.mPlanStatus = ToString(InBundle.mStatus);
    Summary.mPhaseCount = static_cast<int>(InBundle.mPhases.size());

    // Build PhaseItem list from bundle phases
    for (size_t I = 0; I < InBundle.mPhases.size(); ++I)
    {
        const FPhaseRecord &Phase = InBundle.mPhases[I];
        PhaseItem Item;
        Item.mPhaseKey = std::to_string(I);
        Item.mStatus = Phase.mLifecycle.mStatus;
        Item.mStatusRaw = ToString(Phase.mLifecycle.mStatus);
        Item.mDescription = Phase.mScope;
        Summary.mPhases.push_back(Item);

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
        }
    }

    // Build execution taxonomy directly from bundle phases
    for (size_t I = 0; I < InBundle.mPhases.size(); ++I)
    {
        const FPhaseRecord &Phase = InBundle.mPhases[I];
        if (Phase.mJobs.empty() && Phase.mLanes.empty())
            continue;

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

    // Blockers — single source of truth. Populate the per-plan list so
    // the watch BLOCKERS panel and the per-plan blocker count come from
    // the same data (count is derived via mBlockers.size(), not a
    // separately-maintained integer).
    Summary.mBlockers = CollectBundleBlockers(InBundle);

    return Summary;
}

FDocWatchSnapshot BuildWatchSnapshot(const std::string &InRepoRoot,
                                     const bool InUseCache,
                                     const std::string &InCacheDir,
                                     const bool InCacheVerbose)
{
    const auto StartTime = std::chrono::steady_clock::now();

    FDocWatchSnapshot Snapshot;
    Snapshot.mRepoRoot = InRepoRoot;
    Snapshot.mSnapshotAtUTC = GetUtcNow();

    const fs::path RepoRoot = NormalizeRepoRootPath(InRepoRoot);

    // 1. Load all V4 bundles directly
    std::vector<std::string> BundleWarnings;
    const std::vector<FTopicBundle> Bundles =
        LoadAllBundles(RepoRoot, BundleWarnings);

    Snapshot.mInventory.mPlanCount = static_cast<int>(Bundles.size());

    // 2. Build plan summaries from bundles
    for (const FTopicBundle &Bundle : Bundles)
    {
        FWatchPlanSummary Summary =
            BuildPlanSummaryFromBundle(Bundle, RepoRoot);

        if (Bundle.mStatus == ETopicStatus::InProgress)
            Snapshot.mActivePlans.push_back(std::move(Summary));
        else
            Snapshot.mNonActivePlans.push_back(std::move(Summary));
    }

    Snapshot.mInventory.mActivePlanCount =
        static_cast<int>(Snapshot.mActivePlans.size());
    Snapshot.mInventory.mNonActivePlanCount =
        static_cast<int>(Snapshot.mNonActivePlans.size());

    // 3. Bundle validation (no .md reads)
    const std::vector<ValidateCheck> Checks = ValidateAllBundles(Bundles);

    for (const ValidateCheck &Check : Checks)
    {
        if (Check.mbOk)
        {
            Snapshot.mValidation.mPassedChecks++;
        }
        else
        {
            Snapshot.mValidation.mFailedChecks++;
            if (Check.mSeverity == EValidationSeverity::ErrorMajor)
                Snapshot.mValidation.mErrorMajorCount++;
            else if (Check.mSeverity == EValidationSeverity::ErrorMinor)
                Snapshot.mValidation.mErrorMinorCount++;
            else
                Snapshot.mValidation.mWarningCount++;
            Snapshot.mValidation.mFailedCheckDetails.push_back(Check);
        }
    }
    Snapshot.mValidation.mTotalChecks = static_cast<int>(Checks.size());
    Snapshot.mValidation.mbOk = (Snapshot.mValidation.mErrorMajorCount == 0);

    // 4. Lint
    const LintResult Lint = BuildLintResult(InRepoRoot, /*InQuiet=*/true);
    Snapshot.mLint.mWarningCount = Lint.mWarningCount;
    Snapshot.mLint.mNamePatternWarnings = Lint.mNamePatternWarningCount;
    Snapshot.mLint.mMissingH1Warnings = Lint.mMissingH1WarningCount;

    // 6. Aggregate all blockers
    for (const FWatchPlanSummary &Plan : Snapshot.mActivePlans)
    {
        Snapshot.mAllBlockers.insert(Snapshot.mAllBlockers.end(),
                                     Plan.mBlockers.begin(),
                                     Plan.mBlockers.end());
    }

    // 7. Measure poll duration
    const auto EndTime = std::chrono::steady_clock::now();
    Snapshot.mPollDurationMs =
        static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                             EndTime - StartTime)
                             .count());

    return Snapshot;
}

} // namespace UniPlan
