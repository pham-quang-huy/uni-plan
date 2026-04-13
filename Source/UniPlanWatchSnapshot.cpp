#include "UniPlanWatchSnapshot.h"
#include "UniPlanDocumentStore.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <string>
#include <vector>

namespace UniPlan
{

// Identify table type by headers instead of section ID.
// Tables may be in parent sections due to H3 flattening.
static bool HasHeader(const FStructuredTable &InTable,
                      const std::string &InName)
{
    for (const std::string &H : InTable.mHeaders)
    {
        if (ToLower(Trim(H)) == InName)
            return true;
    }
    return false;
}

static bool IsLaneTable(const FStructuredTable &InTable)
{
    return HasHeader(InTable, "lane") && HasHeader(InTable, "status") &&
           !HasHeader(InTable, "wave") && !HasHeader(InTable, "job");
}

static bool IsJobBoardTable(const FStructuredTable &InTable)
{
    return HasHeader(InTable, "wave") && HasHeader(InTable, "lane") &&
           HasHeader(InTable, "job");
}

static bool IsTaskChecklistTable(const FStructuredTable &InTable)
{
    return HasHeader(InTable, "task") &&
           (HasHeader(InTable, "job") || HasHeader(InTable, "job ref"));
}

static bool IsFileManifestTable(const FStructuredTable &InTable)
{
    return HasHeader(InTable, "file") && HasHeader(InTable, "action");
}

static FPhaseTaxonomy BuildPhaseTaxonomy(const PhaseItem &InPhase,
                                         const fs::path &InRepoRoot)
{
    FPhaseTaxonomy Taxonomy;
    Taxonomy.mPhaseKey = InPhase.mPhaseKey;
    Taxonomy.mPlaybookPath = InPhase.mPlaybookPath;

    if (InPhase.mPlaybookPath.empty())
    {
        return Taxonomy;
    }

    // Load playbook via document store (bundle-aware)
    FDocument PlaybookDoc;
    std::string LoadError;
    if (!TryLoadDocument(InRepoRoot, InPhase.mPlaybookPath, PlaybookDoc,
                         LoadError))
    {
        return Taxonomy;
    }

    // Collect all tables from all sections into a flat list
    // for the existing table-scanning logic below.
    struct FNamedTable
    {
        std::string mSectionId;
        const FStructuredTable *rpTable = nullptr;
    };
    std::vector<FNamedTable> AllTables;
    for (const auto &SecPair : PlaybookDoc.mSections)
    {
        for (const FStructuredTable &Table : SecPair.second.mTables)
        {
            AllTables.push_back({SecPair.first, &Table});
        }
    }

    // Parse execution_lanes
    for (const FNamedTable &NT : AllTables)
    {
        const FStructuredTable &Table = *NT.rpTable;
        if (!IsLaneTable(Table))
        {
            continue;
        }
        int LaneCol = -1;
        int StatusCol = -1;
        int ScopeCol = -1;
        int ExitCol = -1;
        for (int Col = 0; Col < static_cast<int>(Table.mHeaders.size()); ++Col)
        {
            const std::string Lower =
                ToLower(Trim(Table.mHeaders[static_cast<size_t>(Col)]));
            if (Lower == "lane")
            {
                LaneCol = Col;
            }
            else if (Lower == "status")
            {
                StatusCol = Col;
            }
            else if (Lower == "scope" || Lower == "owner")
            {
                ScopeCol = Col;
            }
            else if (Lower == "exit criteria" || Lower == "exit_criteria" ||
                     Lower == "exit")
            {
                ExitCol = Col;
            }
        }
        if (LaneCol < 0)
        {
            continue;
        }
        for (const std::vector<FTableCell> &Row : Table.mRows)
        {
            FLaneRecord Lane;
            Lane.mLaneID = (LaneCol < static_cast<int>(Row.size()))
                               ? Trim(Row[static_cast<size_t>(LaneCol)].mValue)
                               : "";
            Lane.mStatus =
                (StatusCol >= 0 && StatusCol < static_cast<int>(Row.size()))
                    ? NormalizeStatusValue(
                          Row[static_cast<size_t>(StatusCol)].mValue)
                    : "unknown";
            Lane.mScope =
                (ScopeCol >= 0 && ScopeCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(ScopeCol)].mValue)
                    : "";
            Lane.mExitCriteria =
                (ExitCol >= 0 && ExitCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(ExitCol)].mValue)
                    : "";
            if (!Lane.mLaneID.empty())
            {
                Taxonomy.mLanes.push_back(std::move(Lane));
            }
        }
    }

    // Parse wave_lane_job_board
    std::set<std::string> UniqueWaves;
    for (const FNamedTable &NT : AllTables)
    {
        const FStructuredTable &Table = *NT.rpTable;
        if (!IsJobBoardTable(Table))
        {
            continue;
        }
        int WaveCol = -1, LaneCol = -1, JobCol = -1, StatusCol = -1,
            ScopeCol = -1, ExitCol = -1;
        for (int Col = 0; Col < static_cast<int>(Table.mHeaders.size()); ++Col)
        {
            const std::string Lower =
                ToLower(Trim(Table.mHeaders[static_cast<size_t>(Col)]));
            if (Lower == "wave" || Lower == "wave_id")
            {
                WaveCol = Col;
            }
            else if (Lower == "lane" || Lower == "lane_id")
            {
                LaneCol = Col;
            }
            else if (Lower == "job" || Lower == "job_id")
            {
                JobCol = Col;
            }
            else if (Lower == "status")
            {
                StatusCol = Col;
            }
            else if (Lower == "scope")
            {
                ScopeCol = Col;
            }
            else if (Lower == "exit criteria" || Lower == "exit_criteria" ||
                     Lower == "exit")
            {
                ExitCol = Col;
            }
        }
        for (const std::vector<FTableCell> &Row : Table.mRows)
        {
            FJobRecord Job;
            Job.mWaveID =
                (WaveCol >= 0 && WaveCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(WaveCol)].mValue)
                    : "";
            Job.mLaneID =
                (LaneCol >= 0 && LaneCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(LaneCol)].mValue)
                    : "";
            std::string JobCell =
                (JobCol >= 0 && JobCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(JobCol)].mValue)
                    : "";
            // Parse "J1 description" into ID + name
            const size_t SpacePos = JobCell.find(' ');
            if (SpacePos != std::string::npos)
            {
                Job.mJobID = JobCell.substr(0, SpacePos);
                Job.mJobName = JobCell.substr(SpacePos + 1);
            }
            else
            {
                Job.mJobID = JobCell;
            }
            Job.mStatus =
                (StatusCol >= 0 && StatusCol < static_cast<int>(Row.size()))
                    ? NormalizeStatusValue(
                          Row[static_cast<size_t>(StatusCol)].mValue)
                    : "unknown";
            Job.mScope =
                (ScopeCol >= 0 && ScopeCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(ScopeCol)].mValue)
                    : "";
            Job.mExitCriteria =
                (ExitCol >= 0 && ExitCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(ExitCol)].mValue)
                    : "";
            if (!Job.mWaveID.empty() || !Job.mJobID.empty())
            {
                UniqueWaves.insert(Job.mWaveID);
                Taxonomy.mJobs.push_back(std::move(Job));
            }
        }
    }
    Taxonomy.mWaveCount = static_cast<int>(UniqueWaves.size());

    // Parse job_task_checklist
    for (const FNamedTable &NT : AllTables)
    {
        const FStructuredTable &Table = *NT.rpTable;
        if (!IsTaskChecklistTable(Table))
        {
            continue;
        }
        int JobCol = -1, TaskCol = -1, StatusCol = -1, DescCol = -1,
            EvidenceCol = -1;
        for (int Col = 0; Col < static_cast<int>(Table.mHeaders.size()); ++Col)
        {
            const std::string Lower =
                ToLower(Trim(Table.mHeaders[static_cast<size_t>(Col)]));
            if (Lower == "job" || Lower == "job_id" || Lower == "parent_job")
            {
                JobCol = Col;
            }
            else if (Lower == "task id" || Lower == "task_id")
            {
                TaskCol = Col;
            }
            else if (Lower == "status")
            {
                StatusCol = Col;
            }
            else if (Lower == "task" || Lower == "description")
            {
                DescCol = Col;
            }
            else if (Lower == "evidence" || Lower == "evidence target" ||
                     Lower == "evidence_target")
            {
                EvidenceCol = Col;
            }
        }
        for (const std::vector<FTableCell> &Row : Table.mRows)
        {
            FTaskRecord Task;
            Task.mJobRef =
                (JobCol >= 0 && JobCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(JobCol)].mValue)
                    : "";
            Task.mTaskID =
                (TaskCol >= 0 && TaskCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(TaskCol)].mValue)
                    : "";
            Task.mStatus =
                (StatusCol >= 0 && StatusCol < static_cast<int>(Row.size()))
                    ? NormalizeStatusValue(
                          Row[static_cast<size_t>(StatusCol)].mValue)
                    : "unknown";
            Task.mDescription =
                (DescCol >= 0 && DescCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(DescCol)].mValue)
                    : "";
            Task.mEvidence =
                (EvidenceCol >= 0 && EvidenceCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(EvidenceCol)].mValue)
                    : "";
            if (!Task.mJobRef.empty() || !Task.mTaskID.empty())
            {
                Taxonomy.mTasks.push_back(std::move(Task));
            }
        }
    }

    // Parse target_file_manifest
    for (const FNamedTable &NT : AllTables)
    {
        const FStructuredTable &Table = *NT.rpTable;
        if (!IsFileManifestTable(Table))
        {
            continue;
        }
        int FileCol = -1, ActionCol = -1, DescCol = -1;
        for (int Col = 0; Col < static_cast<int>(Table.mHeaders.size()); ++Col)
        {
            const std::string Lower =
                ToLower(Trim(Table.mHeaders[static_cast<size_t>(Col)]));
            if (Lower == "file")
            {
                FileCol = Col;
            }
            else if (Lower == "action")
            {
                ActionCol = Col;
            }
            else if (Lower == "description")
            {
                DescCol = Col;
            }
        }
        if (FileCol < 0)
        {
            continue;
        }
        for (const std::vector<FTableCell> &Row : Table.mRows)
        {
            const std::string FilePath =
                (FileCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(FileCol)].mValue)
                    : "";
            if (FilePath.empty() || FilePath == "N/A")
            {
                continue;
            }
            FFileManifestItem Item;
            Item.mFilePath = (FilePath.size() >= 2 && FilePath.front() == '`' &&
                              FilePath.back() == '`')
                                 ? FilePath.substr(1, FilePath.size() - 2)
                                 : FilePath;
            Item.mAction =
                (ActionCol >= 0 && ActionCol < static_cast<int>(Row.size()))
                    ? ToLower(Trim(Row[static_cast<size_t>(ActionCol)].mValue))
                    : "";
            Item.mDescription =
                (DescCol >= 0 && DescCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(DescCol)].mValue)
                    : "";
            Taxonomy.mFileManifest.push_back(std::move(Item));
        }
    }

    return Taxonomy;
}

static FWatchDocSchemaResult
BuildDocSchemaResult(const fs::path &InRepoRoot, const std::string &InDocPath,
                     const std::string &InDocType,
                     const std::string &InPhaseKey = "")
{
    FWatchDocSchemaResult Result;
    Result.mDocPath = InDocPath;
    Result.mDocType = InDocType;
    Result.mPhaseKey = InPhaseKey;

    if (InDocPath.empty())
    {
        return Result;
    }

    // Get canonical schema sections
    const std::vector<SectionSchemaEntry> Schema =
        BuildSectionSchemaEntries(InDocType, InRepoRoot);

    // Load document via document store (bundle-aware)
    FDocument Doc;
    std::string LoadError;
    if (!TryLoadDocument(InRepoRoot, InDocPath, Doc, LoadError))
    {
        return Result;
    }

    // Collect section IDs from FDocument
    struct DocHeading
    {
        std::string mSectionId;
        int mLevel;
    };
    std::vector<DocHeading> DocHeadings;
    std::set<std::string> PresentSections;
    for (const auto &SecPair : Doc.mSections)
    {
        const int Level = SecPair.second.mLevel;
        if (Level == 2 || Level == 3)
        {
            DocHeadings.push_back({SecPair.first, Level});
            PresentSections.insert(SecPair.first);
        }
        // Also add subsection IDs
        for (const std::string &SubId : SecPair.second.mSubsectionIDs)
        {
            DocHeadings.push_back({SubId, 3});
            PresentSections.insert(SubId);
        }
    }

    // Build canonical sections list (H2 only from schema)
    std::set<std::string> CanonicalIds;
    for (const SectionSchemaEntry &Entry : Schema)
    {
        CanonicalIds.insert(Entry.mSectionId);
    }

    // Walk document headings in order, inserting canonical checks + children
    std::set<std::string> EmittedCanonical;
    for (const DocHeading &DH : DocHeadings)
    {
        // If this is a canonical H2, emit it
        if (DH.mLevel == 2 && CanonicalIds.count(DH.mSectionId) > 0)
        {
            FWatchHeadingCheck Check;
            Check.mSectionId = DH.mSectionId;
            Check.mbPresent = true;
            Check.mbCanonical = true;
            Check.mLevel = 2;
            // Find if required
            for (const SectionSchemaEntry &Entry : Schema)
            {
                if (Entry.mSectionId == DH.mSectionId)
                {
                    Check.mbRequired = Entry.mbRequired;
                    break;
                }
            }
            Result.mHeadings.push_back(Check);
            EmittedCanonical.insert(DH.mSectionId);

            if (Check.mbRequired)
            {
                Result.mRequiredCount++;
                Result.mRequiredPresent++;
            }
        }
        else if (DH.mLevel == 2 && CanonicalIds.count(DH.mSectionId) == 0)
        {
            // Extra H2 heading
            FWatchHeadingCheck Check;
            Check.mSectionId = DH.mSectionId;
            Check.mbPresent = true;
            Check.mbCanonical = false;
            Check.mLevel = 2;
            Result.mHeadings.push_back(Check);
            Result.mExtraCount++;
        }
        else if (DH.mLevel == 3)
        {
            // H3 child heading — always canonical (legitimate child of parent
            // H2)
            FWatchHeadingCheck Check;
            Check.mSectionId = DH.mSectionId;
            Check.mbPresent = true;
            Check.mbCanonical = true;
            Check.mbRequired = false;
            Check.mLevel = 3;
            Result.mHeadings.push_back(Check);
        }
    }

    // Add missing required canonical sections that weren't in the document
    for (const SectionSchemaEntry &Entry : Schema)
    {
        if (Entry.mbRequired && EmittedCanonical.count(Entry.mSectionId) == 0)
        {
            FWatchHeadingCheck Check;
            Check.mSectionId = Entry.mSectionId;
            Check.mbRequired = true;
            Check.mbPresent = false;
            Check.mbCanonical = true;
            Check.mLevel = 2;
            Result.mHeadings.push_back(Check);
            Result.mRequiredCount++;
        }
        else if (!Entry.mbRequired &&
                 EmittedCanonical.count(Entry.mSectionId) == 0)
        {
            // Optional + absent — still add for dim display
            FWatchHeadingCheck Check;
            Check.mSectionId = Entry.mSectionId;
            Check.mbRequired = false;
            Check.mbPresent = false;
            Check.mbCanonical = true;
            Check.mLevel = 2;
            Result.mHeadings.push_back(Check);
        }
    }

    // Count required (need to recount since we do it inline above only for
    // present ones)
    Result.mRequiredCount = 0;
    Result.mRequiredPresent = 0;
    for (const FWatchHeadingCheck &Check : Result.mHeadings)
    {
        if (Check.mbRequired && Check.mLevel == 2)
        {
            Result.mRequiredCount++;
            if (Check.mbPresent)
            {
                Result.mRequiredPresent++;
            }
        }
    }

    return Result;
}

static FWatchTopicSchemaResult
BuildTopicSchemaResult(const fs::path &InRepoRoot,
                       const FWatchPlanSummary &InSummary,
                       const Inventory &InInventory)
{
    FWatchTopicSchemaResult Result;

    // Plan schema
    Result.mPlan =
        BuildDocSchemaResult(InRepoRoot, InSummary.mPlanPath, "plan");

    // Find impl path for this topic
    for (const DocumentRecord &Impl : InInventory.mImplementations)
    {
        if (Impl.mTopicKey == InSummary.mTopicKey)
        {
            Result.mImpl =
                BuildDocSchemaResult(InRepoRoot, Impl.mPath, "implementation");
            break;
        }
    }

    // Playbook schemas (one per phase with playbook)
    for (const PhaseItem &Phase : InSummary.mPhases)
    {
        if (!Phase.mPlaybookPath.empty())
        {
            Result.mPlaybooks.push_back(BuildDocSchemaResult(
                InRepoRoot, Phase.mPlaybookPath, "playbook", Phase.mPhaseKey));
        }
    }

    // Sidecar schemas
    for (const SidecarRecord &Sidecar : InInventory.mSidecars)
    {
        if (Sidecar.mTopicKey != InSummary.mTopicKey)
        {
            continue;
        }
        const std::string &Owner = Sidecar.mOwnerKind;
        const std::string &Kind = Sidecar.mDocKind;
        if (Owner == "Plan" && Kind == "ChangeLog")
        {
            Result.mPlanChangeLog = BuildDocSchemaResult(
                InRepoRoot, Sidecar.mPath, "plan_changelog");
        }
        else if (Owner == "Plan" && Kind == "Verification")
        {
            Result.mPlanVerification = BuildDocSchemaResult(
                InRepoRoot, Sidecar.mPath, "plan_verification");
        }
        else if (Owner == "Impl" && Kind == "ChangeLog")
        {
            Result.mImplChangeLog = BuildDocSchemaResult(
                InRepoRoot, Sidecar.mPath, "impl_changelog");
        }
        else if (Owner == "Impl" && Kind == "Verification")
        {
            Result.mImplVerification = BuildDocSchemaResult(
                InRepoRoot, Sidecar.mPath, "impl_verification");
        }
        else if (Owner == "Playbook" && Kind == "ChangeLog")
        {
            Result.mPlaybookChangeLogs.push_back(
                BuildDocSchemaResult(InRepoRoot, Sidecar.mPath,
                                     "playbook_changelog", Sidecar.mPhaseKey));
        }
        else if (Owner == "Playbook" && Kind == "Verification")
        {
            Result.mPlaybookVerifications.push_back(BuildDocSchemaResult(
                InRepoRoot, Sidecar.mPath, "playbook_verification",
                Sidecar.mPhaseKey));
        }
    }

    return Result;
}

static FWatchPlanSummary
BuildPlanSummary(const PhaseListAllEntry &InEntry, const fs::path &InRepoRoot,
                 const std::vector<DocumentRecord> &InPlaybooks,
                 const Inventory &InInventory)
{
    FWatchPlanSummary Summary;
    Summary.mTopicKey = InEntry.mTopicKey;
    Summary.mPlanPath = InEntry.mPlanPath;
    Summary.mPlanStatus = InEntry.mPlanStatus;
    Summary.mPhases = InEntry.mPhases;
    Summary.mPhaseCount = static_cast<int>(InEntry.mPhases.size());

    for (const PhaseItem &Phase : InEntry.mPhases)
    {
        if (Phase.mStatus == "completed" || Phase.mStatus == "closed")
        {
            Summary.mPhaseCompleted++;
        }
        else if (Phase.mStatus == "in_progress")
        {
            Summary.mPhaseInProgress++;
        }
        else if (Phase.mStatus == "not_started")
        {
            Summary.mPhaseNotStarted++;
        }
        else if (Phase.mStatus == "blocked")
        {
            Summary.mPhaseBlocked++;
        }
    }

    // Plan summary, goals, and non-goals are deferred to
    // on-demand loading when the user selects a specific plan.
    // Loading 42 bundle files just for summary text is too slow.

    // Count playbooks for this topic
    for (const DocumentRecord &Playbook : InPlaybooks)
    {
        if (Playbook.mTopicKey == InEntry.mTopicKey)
        {
            Summary.mPlaybookCount++;
        }
    }

    // Blocker collection deferred — requires loading plan
    // bundle which is too expensive for initial 42-topic scan.
    Summary.mBlockerCount = 0;

    // Build execution taxonomy for phases with playbooks.
    // Bundle cache makes this fast — each playbook load is a
    // cache hit (bundle was already parsed during inventory).
    for (const PhaseItem &Phase : InEntry.mPhases)
    {
        if (!Phase.mPlaybookPath.empty())
        {
            Summary.mPhaseTaxonomies.push_back(
                BuildPhaseTaxonomy(Phase, InRepoRoot));
        }
    }

    return Summary;
}

FDocWatchSnapshot BuildWatchSnapshot(const std::string &InRepoRoot,
                                     const bool InUseCache,
                                     const std::string &InCacheDir,
                                     const bool InCacheVerbose)
{
    const auto StartTime = std::chrono::steady_clock::now();

    // Clear bundle cache from previous snapshot rebuild
    ClearBundleCache();

    FDocWatchSnapshot Snapshot;
    Snapshot.mRepoRoot = InRepoRoot;
    Snapshot.mSnapshotAtUTC = GetUtcNow();

    // 1. Build inventory (leverages FNV1a cache)
    const Inventory Inv = BuildInventory(InRepoRoot, InUseCache, InCacheDir,
                                         InCacheVerbose, /*InQuiet=*/true);

    Snapshot.mInventory.mPlanCount = static_cast<int>(Inv.mPlans.size());
    Snapshot.mInventory.mPlaybookCount =
        static_cast<int>(Inv.mPlaybooks.size());
    Snapshot.mInventory.mImplementationCount =
        static_cast<int>(Inv.mImplementations.size());
    Snapshot.mInventory.mSidecarCount = static_cast<int>(Inv.mSidecars.size());
    Snapshot.mInventory.mPairCount = static_cast<int>(Inv.mPairs.size());

    // 2. Build phase list for all plans
    const fs::path RepoRoot = NormalizeRepoRootPath(InRepoRoot);
    std::vector<std::string> PhaseWarnings;
    const std::vector<PhaseListAllEntry> AllPhases =
        BuildPhaseListAll(RepoRoot, Inv, "all", PhaseWarnings);

    // 3. Partition plans by status
    for (const PhaseListAllEntry &Entry : AllPhases)
    {
        FWatchPlanSummary Summary =
            BuildPlanSummary(Entry, RepoRoot, Inv.mPlaybooks, Inv);

        if (Entry.mPlanStatus == "in_progress")
        {
            Snapshot.mActivePlans.push_back(std::move(Summary));
        }
        else
        {
            Snapshot.mNonActivePlans.push_back(std::move(Summary));
        }
    }

    Snapshot.mInventory.mActivePlanCount =
        static_cast<int>(Snapshot.mActivePlans.size());
    Snapshot.mInventory.mNonActivePlanCount =
        static_cast<int>(Snapshot.mNonActivePlans.size());

    // 4. Validation checks
    std::vector<std::string> ValidateErrors;
    std::vector<std::string> ValidateWarnings;
    bool ValidateOk = true;
    const std::vector<ValidateCheck> Checks = BuildValidateChecks(
        Inv, RepoRoot, false, ValidateErrors, ValidateWarnings, ValidateOk);

    Snapshot.mValidation.mbOk = ValidateOk;
    Snapshot.mValidation.mTotalChecks = static_cast<int>(Checks.size());
    for (const ValidateCheck &Check : Checks)
    {
        if (Check.mbOk)
        {
            Snapshot.mValidation.mPassedChecks++;
        }
        else
        {
            Snapshot.mValidation.mFailedChecks++;
            if (Check.mbCritical)
            {
                Snapshot.mValidation.mCriticalFailures++;
            }
            Snapshot.mValidation.mFailedCheckDetails.push_back(Check);
        }
    }

    // 5. Lint
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
