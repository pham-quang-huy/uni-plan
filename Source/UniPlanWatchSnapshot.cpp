#include "UniPlanWatchSnapshot.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

namespace UniPlan
{

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

    // Read and parse the playbook
    const fs::path PlaybookAbsPath = InRepoRoot / InPhase.mPlaybookPath;
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(PlaybookAbsPath, Lines, ReadError))
    {
        return Taxonomy;
    }

    Taxonomy.mPlaybookLineCount = static_cast<int>(Lines.size());
    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    const std::vector<MarkdownTableRecord> Tables =
        ParseMarkdownTables(Lines, Headings);

    // Parse execution_lanes
    for (const MarkdownTableRecord &Table : Tables)
    {
        if (Table.mSectionId != "execution_lanes")
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
        for (const std::vector<std::string> &Row : Table.mRows)
        {
            FLaneRecord Lane;
            Lane.mLaneID = (LaneCol < static_cast<int>(Row.size()))
                               ? Trim(Row[static_cast<size_t>(LaneCol)])
                               : "";
            Lane.mStatus =
                (StatusCol >= 0 && StatusCol < static_cast<int>(Row.size()))
                    ? NormalizeStatusValue(Row[static_cast<size_t>(StatusCol)])
                    : "unknown";
            Lane.mScope =
                (ScopeCol >= 0 && ScopeCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(ScopeCol)])
                    : "";
            Lane.mExitCriteria =
                (ExitCol >= 0 && ExitCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(ExitCol)])
                    : "";
            if (!Lane.mLaneID.empty())
            {
                Taxonomy.mLanes.push_back(std::move(Lane));
            }
        }
    }

    // Parse wave_lane_job_board
    std::set<std::string> UniqueWaves;
    for (const MarkdownTableRecord &Table : Tables)
    {
        if (Table.mSectionId != "wave_lane_job_board")
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
        for (const std::vector<std::string> &Row : Table.mRows)
        {
            FJobRecord Job;
            Job.mWaveID =
                (WaveCol >= 0 && WaveCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(WaveCol)])
                    : "";
            Job.mLaneID =
                (LaneCol >= 0 && LaneCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(LaneCol)])
                    : "";
            std::string JobCell =
                (JobCol >= 0 && JobCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(JobCol)])
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
                    ? NormalizeStatusValue(Row[static_cast<size_t>(StatusCol)])
                    : "unknown";
            Job.mScope =
                (ScopeCol >= 0 && ScopeCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(ScopeCol)])
                    : "";
            Job.mExitCriteria =
                (ExitCol >= 0 && ExitCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(ExitCol)])
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
    for (const MarkdownTableRecord &Table : Tables)
    {
        if (Table.mSectionId != "job_task_checklist")
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
        for (const std::vector<std::string> &Row : Table.mRows)
        {
            FTaskRecord Task;
            Task.mJobRef =
                (JobCol >= 0 && JobCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(JobCol)])
                    : "";
            Task.mTaskID =
                (TaskCol >= 0 && TaskCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(TaskCol)])
                    : "";
            Task.mStatus =
                (StatusCol >= 0 && StatusCol < static_cast<int>(Row.size()))
                    ? NormalizeStatusValue(Row[static_cast<size_t>(StatusCol)])
                    : "unknown";
            Task.mDescription =
                (DescCol >= 0 && DescCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(DescCol)])
                    : "";
            Task.mEvidence =
                (EvidenceCol >= 0 && EvidenceCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(EvidenceCol)])
                    : "";
            if (!Task.mJobRef.empty() || !Task.mTaskID.empty())
            {
                Taxonomy.mTasks.push_back(std::move(Task));
            }
        }
    }

    // Parse target_file_manifest
    for (const MarkdownTableRecord &Table : Tables)
    {
        if (Table.mSectionId != "target_file_manifest")
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
        for (const std::vector<std::string> &Row : Table.mRows)
        {
            const std::string FilePath =
                (FileCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(FileCol)])
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
                    ? ToLower(Trim(Row[static_cast<size_t>(ActionCol)]))
                    : "";
            Item.mDescription =
                (DescCol >= 0 && DescCol < static_cast<int>(Row.size()))
                    ? Trim(Row[static_cast<size_t>(DescCol)])
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

    // Read document headings
    const fs::path AbsPath = InRepoRoot / fs::path(InDocPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsPath, Lines, ReadError))
    {
        return Result;
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);

    // Collect all H2 and H3 section IDs from the document, preserving order and
    // parent/child
    struct DocHeading
    {
        std::string mSectionId;
        int mLevel;
    };
    std::vector<DocHeading> DocHeadings;
    std::set<std::string> PresentSections;
    for (const HeadingRecord &Heading : Headings)
    {
        if (Heading.mLevel == 2 || Heading.mLevel == 3)
        {
            DocHeadings.push_back({Heading.mSectionId, Heading.mLevel});
            PresentSections.insert(Heading.mSectionId);
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

    // Extract plan summary, goals, and non-goals from plan document
    {
        const fs::path PlanAbsPath = InRepoRoot / fs::path(InEntry.mPlanPath);
        std::vector<std::string> PlanLines;
        std::string PlanReadError;
        if (TryReadFileLines(PlanAbsPath, PlanLines, PlanReadError))
        {
            const std::vector<HeadingRecord> PlanHeadings =
                ParseHeadingRecords(PlanLines);

            // Extract summary section text (lines between ## summary and next
            // H2)
            for (size_t HIdx = 0; HIdx < PlanHeadings.size(); ++HIdx)
            {
                if (PlanHeadings[HIdx].mSectionId != "summary" ||
                    PlanHeadings[HIdx].mLevel != 2)
                {
                    continue;
                }
                const int StartLine = PlanHeadings[HIdx].mLine + 1;
                // Find the next H2 heading (skip H3+ children)
                int EndLine = static_cast<int>(PlanLines.size());
                for (size_t NextIdx = HIdx + 1; NextIdx < PlanHeadings.size();
                     ++NextIdx)
                {
                    if (PlanHeadings[NextIdx].mLevel <= 2)
                    {
                        EndLine = PlanHeadings[NextIdx].mLine;
                        break;
                    }
                }
                for (int LineIdx = StartLine;
                     LineIdx < EndLine &&
                     static_cast<int>(Summary.mSummaryLines.size()) < 10;
                     ++LineIdx)
                {
                    const std::string Trimmed =
                        Trim(PlanLines[static_cast<size_t>(LineIdx)]);
                    if (Trimmed.empty() || Trimmed.front() == '|' ||
                        Trimmed.front() == '#' || Trimmed.find("---") == 0)
                    {
                        continue;
                    }
                    // Strip leading markdown bullets
                    std::string Clean = Trimmed;
                    if (Clean.size() >= 2 && Clean[0] == '-' && Clean[1] == ' ')
                    {
                        Clean = Clean.substr(2);
                    }
                    Summary.mSummaryLines.push_back(Clean);
                }
                break;
            }

            // Extract goals and non-goals
            const std::vector<MarkdownTableRecord> PlanTables =
                ParseMarkdownTables(PlanLines, PlanHeadings);

            // Strategy 1: goals_and_non_goals section table (Type/Statement
            // cols)
            for (const MarkdownTableRecord &Table : PlanTables)
            {
                if (Table.mSectionId != "goals_and_non_goals")
                {
                    continue;
                }
                int TypeCol = -1;
                int StatementCol = -1;
                int AreaCol = -1;
                for (int Col = 0; Col < static_cast<int>(Table.mHeaders.size());
                     ++Col)
                {
                    const std::string Lower =
                        ToLower(Trim(Table.mHeaders[static_cast<size_t>(Col)]));
                    if (Lower == "type")
                    {
                        TypeCol = Col;
                    }
                    else if (Lower == "statement")
                    {
                        StatementCol = Col;
                    }
                    else if (Lower == "area")
                    {
                        AreaCol = Col;
                    }
                }
                if (TypeCol < 0)
                {
                    break;
                }
                for (const std::vector<std::string> &Row : Table.mRows)
                {
                    const std::string TypeVal =
                        (TypeCol < static_cast<int>(Row.size()))
                            ? ToLower(Trim(Row[static_cast<size_t>(TypeCol)]))
                            : "";
                    std::string Statement =
                        (StatementCol >= 0 &&
                         StatementCol < static_cast<int>(Row.size()))
                            ? Trim(Row[static_cast<size_t>(StatementCol)])
                            : "";
                    if (Statement.empty() && AreaCol >= 0 &&
                        AreaCol < static_cast<int>(Row.size()))
                    {
                        Statement = Trim(Row[static_cast<size_t>(AreaCol)]);
                    }
                    if (Statement.empty())
                    {
                        continue;
                    }
                    if (TypeVal.find("non") != std::string::npos)
                    {
                        Summary.mNonGoalStatements.push_back(Statement);
                    }
                    else if (TypeVal.find("goal") != std::string::npos)
                    {
                        Summary.mGoalStatements.push_back(Statement);
                    }
                }
                break;
            }
        }
    }

    // Count playbooks for this topic
    for (const DocumentRecord &Playbook : InPlaybooks)
    {
        if (Playbook.mTopicKey == InEntry.mTopicKey)
        {
            Summary.mPlaybookCount++;
        }
    }

    // Collect blockers for this topic
    for (const DocumentRecord &Plan : InInventory.mPlans)
    {
        if (Plan.mTopicKey == InEntry.mTopicKey)
        {
            std::vector<std::string> BlockerWarnings;
            std::vector<BlockerItem> Items = CollectBlockerItemsFromDocument(
                InRepoRoot, Plan, "plan", BlockerWarnings);
            Summary.mBlockers.insert(Summary.mBlockers.end(), Items.begin(),
                                     Items.end());
        }
    }
    Summary.mBlockerCount = static_cast<int>(Summary.mBlockers.size());

    // Build schema compliance result
    Summary.mSchemaResult =
        BuildTopicSchemaResult(InRepoRoot, Summary, InInventory);

    // Build sidecar summaries for this topic
    for (const SidecarRecord &Sidecar : InInventory.mSidecars)
    {
        if (Sidecar.mTopicKey != InEntry.mTopicKey)
        {
            continue;
        }
        FWatchSidecarSummary SidecarSummary;
        SidecarSummary.mPath = Sidecar.mPath;
        SidecarSummary.mOwnerKind = Sidecar.mOwnerKind;
        SidecarSummary.mDocKind = Sidecar.mDocKind;
        SidecarSummary.mPhaseKey = Sidecar.mPhaseKey;

        const fs::path SidecarAbsPath = InRepoRoot / fs::path(Sidecar.mPath);
        std::vector<std::string> SidecarLines;
        std::string SidecarReadError;
        if (TryReadFileLines(SidecarAbsPath, SidecarLines, SidecarReadError))
        {
            const std::vector<HeadingRecord> SidecarHeadings =
                ParseHeadingRecords(SidecarLines);
            const std::vector<MarkdownTableRecord> SidecarTables =
                ParseMarkdownTables(SidecarLines, SidecarHeadings);
            for (const MarkdownTableRecord &Table : SidecarTables)
            {
                if (Table.mSectionId == "entries" ||
                    Table.mSectionId == "verification_entries")
                {
                    SidecarSummary.mEntryCount =
                        static_cast<int>(Table.mRows.size());
                    if (!Table.mRows.empty() && !Table.mRows.back().empty())
                    {
                        SidecarSummary.mLatestDate =
                            Trim(Table.mRows.back()[0]);
                    }
                    break;
                }
            }
        }
        Summary.mSidecarSummaries.push_back(std::move(SidecarSummary));
    }

    // Build execution taxonomy for ALL phases with playbooks
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
