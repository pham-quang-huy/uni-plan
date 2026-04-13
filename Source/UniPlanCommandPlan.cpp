#include "UniPlanDocumentStore.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanMutation.h"
#include "UniPlanTaxonomyTypes.h"
#include "UniPlanTypes.h"

#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// plan info — comprehensive topic metadata in one call
// ---------------------------------------------------------------------------

static int RunPlanInfo(const PlanCommandOptions &InOptions,
                       const bool InUseCache, const DocConfig &InConfig)
{
    const fs::path RepoRoot = NormalizeRepoRootPath(InOptions.mRepoRoot);
    const Inventory Inv =
        BuildInventory(RepoRoot.string(), InUseCache, InConfig.mCacheDir,
                       InConfig.mbCacheVerbose);

    const std::string TopicKey =
        ResolveTopicKeyFromInventory(Inv, InOptions.mTopic);
    if (TopicKey.empty())
    {
        std::cerr << "Topic not found: " << InOptions.mTopic << "\n";
        return 1;
    }

    // Find plan record
    const DocumentRecord *rpPlan =
        FindSingleRecordByTopic(Inv.mPlans, TopicKey);

    // Collect phases
    std::vector<std::string> Warnings;
    const std::vector<PhaseItem> Phases =
        rpPlan ? CollectPhaseItemsFromPlan(RepoRoot, *rpPlan, Inv.mPlaybooks,
                                           "all", Warnings)
               : std::vector<PhaseItem>{};

    // Collect artifact paths
    std::vector<std::string> PlaybookPaths;
    for (const DocumentRecord &PB : Inv.mPlaybooks)
    {
        if (PB.mTopicKey == TopicKey)
        {
            PlaybookPaths.push_back(PB.mPath);
        }
    }

    std::vector<std::string> SidecarPaths;
    for (const SidecarRecord &SC : Inv.mSidecars)
    {
        if (SC.mTopicKey == TopicKey)
        {
            SidecarPaths.push_back(SC.mPath);
        }
    }

    const DocumentRecord *rpImpl =
        FindSingleRecordByTopic(Inv.mImplementations, TopicKey);

    // Find pair state
    std::string PairState = "unknown";
    for (const TopicPairRecord &Pair : Inv.mPairs)
    {
        if (Pair.mTopicKey == TopicKey)
        {
            PairState = Pair.mPairState;
            break;
        }
    }

    // Count phase statuses
    int PhaseCompleted = 0;
    int PhaseInProgress = 0;
    int PhaseNotStarted = 0;
    int PhaseBlocked = 0;
    for (const PhaseItem &Phase : Phases)
    {
        if (Phase.mStatus == "completed" || Phase.mStatus == "closed")
            PhaseCompleted++;
        else if (Phase.mStatus == "in_progress")
            PhaseInProgress++;
        else if (Phase.mStatus == "blocked")
            PhaseBlocked++;
        else
            PhaseNotStarted++;
    }

    // Emit JSON
    PrintJsonHeader(kPlanInfoSchema, GetUtcNow(), ToGenericPath(RepoRoot));
    std::cout << "\"topic_key\":" << JsonQuote(TopicKey);
    std::cout << ",\"plan_path\":"
              << (rpPlan ? JsonQuote(rpPlan->mPath) : "null");
    std::cout << ",\"plan_status\":"
              << (rpPlan ? JsonQuote(rpPlan->mStatus) : "null");
    std::cout << ",\"impl_path\":"
              << (rpImpl ? JsonQuote(rpImpl->mPath) : "null");
    std::cout << ",\"pair_state\":" << JsonQuote(PairState);

    std::cout << ",\"phase_count\":" << static_cast<int>(Phases.size());
    std::cout << ",\"phase_completed\":" << PhaseCompleted;
    std::cout << ",\"phase_in_progress\":" << PhaseInProgress;
    std::cout << ",\"phase_not_started\":" << PhaseNotStarted;
    std::cout << ",\"phase_blocked\":" << PhaseBlocked;

    std::cout << ",\"phases\":[";
    for (size_t Index = 0; Index < Phases.size(); ++Index)
    {
        if (Index > 0)
            std::cout << ",";
        const PhaseItem &Phase = Phases[Index];
        std::cout << "{\"key\":" << JsonQuote(Phase.mPhaseKey)
                  << ",\"status\":" << JsonQuote(Phase.mStatus)
                  << ",\"playbook\":" << JsonNullOrQuote(Phase.mPlaybookPath)
                  << ",\"description\":" << JsonNullOrQuote(Phase.mDescription)
                  << "}";
    }
    std::cout << "]";

    std::cout << ",\"playbook_count\":"
              << static_cast<int>(PlaybookPaths.size());

    std::cout << ",";
    PrintJsonStringArray("playbook_paths", PlaybookPaths);
    std::cout << ",";
    PrintJsonStringArray("sidecar_paths", SidecarPaths);

    std::cout << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// plan field get — atomic field access
// ---------------------------------------------------------------------------

static int RunPlanFieldGet(const PlanCommandOptions &InOptions,
                           const bool InUseCache, const DocConfig &InConfig)
{
    const fs::path RepoRoot = NormalizeRepoRootPath(InOptions.mRepoRoot);
    const Inventory Inv =
        BuildInventory(RepoRoot.string(), InUseCache, InConfig.mCacheDir,
                       InConfig.mbCacheVerbose);

    const std::string TopicKey =
        ResolveTopicKeyFromInventory(Inv, InOptions.mTopic);
    const DocumentRecord *rpPlan =
        FindSingleRecordByTopic(Inv.mPlans, TopicKey);
    if (!rpPlan)
    {
        std::cerr << "Plan not found for topic: " << InOptions.mTopic << "\n";
        return 1;
    }

    FDocument Document;
    std::string Error;
    if (!TryLoadDocument(RepoRoot, rpPlan->mPath, Document, Error))
    {
        std::cerr << "Failed to load plan: " << Error << "\n";
        return 1;
    }

    const FSectionContent Section =
        ResolveSectionFromDocument(Document, InOptions.mSection);
    if (Section.mSectionID.empty())
    {
        std::cerr << "Section not found: " << InOptions.mSection << "\n";
        return 1;
    }

    const auto It = Section.mFields.find(InOptions.mField);
    if (It == Section.mFields.end())
    {
        std::cerr << "Field not found: " << InOptions.mField << "\n";
        return 1;
    }

    PrintJsonHeader(kPlanFieldSchema, GetUtcNow(), ToGenericPath(RepoRoot));
    std::cout << "\"topic_key\":" << JsonQuote(TopicKey)
              << ",\"section\":" << JsonQuote(InOptions.mSection)
              << ",\"field\":" << JsonQuote(InOptions.mField)
              << ",\"value\":" << JsonQuote(It->second) << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// plan field set — atomic field mutation
// ---------------------------------------------------------------------------

static int RunPlanFieldSet(const PlanCommandOptions &InOptions,
                           const bool InUseCache, const DocConfig &InConfig)
{
    const fs::path RepoRoot = NormalizeRepoRootPath(InOptions.mRepoRoot);
    const Inventory Inv =
        BuildInventory(RepoRoot.string(), InUseCache, InConfig.mCacheDir,
                       InConfig.mbCacheVerbose);

    const std::string TopicKey =
        ResolveTopicKeyFromInventory(Inv, InOptions.mTopic);
    const DocumentRecord *rpPlan =
        FindSingleRecordByTopic(Inv.mPlans, TopicKey);
    if (!rpPlan)
    {
        std::cerr << "Plan not found for topic: " << InOptions.mTopic << "\n";
        return 1;
    }

    FMutationResult MutResult;
    std::string Error;
    if (!TryUpdateSectionField(RepoRoot, rpPlan->mPath, InOptions.mSection,
                               InOptions.mField, InOptions.mValue, MutResult,
                               Error))
    {
        std::cout << "{\"schema\":" << JsonQuote(kPlanMutationSchema)
                  << ",\"ok\":false"
                  << ",\"error\":" << JsonQuote(Error) << "}\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JsonQuote(kPlanMutationSchema)
              << ",\"ok\":true"
              << ",\"target_path\":" << JsonQuote(MutResult.mTargetPath)
              << ",\"changes\":[";
    for (size_t Index = 0; Index < MutResult.mChanges.size(); ++Index)
    {
        if (Index > 0)
            std::cout << ",";
        const FMutationChange &Change = MutResult.mChanges[Index];
        std::cout << "{\"field\":" << JsonQuote(Change.mField)
                  << ",\"old\":" << JsonQuote(Change.mOldValue)
                  << ",\"new\":" << JsonQuote(Change.mNewValue) << "}";
    }
    std::cout << "]}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// phase detail — full phase information for agent consumption
// ---------------------------------------------------------------------------

static int RunPhaseDetail(const PhaseCommandOptions &InOptions,
                          const bool InUseCache, const DocConfig &InConfig)
{
    const fs::path RepoRoot = NormalizeRepoRootPath(InOptions.mRepoRoot);
    const Inventory Inv =
        BuildInventory(RepoRoot.string(), InUseCache, InConfig.mCacheDir,
                       InConfig.mbCacheVerbose);

    const std::string TopicKey =
        ResolveTopicKeyFromInventory(Inv, InOptions.mTopic);
    const DocumentRecord *rpPlan =
        FindSingleRecordByTopic(Inv.mPlans, TopicKey);
    if (!rpPlan)
    {
        std::cerr << "Plan not found for topic: " << InOptions.mTopic << "\n";
        return 1;
    }

    // Find the phase
    std::vector<std::string> Warnings;
    const std::vector<PhaseItem> Phases = CollectPhaseItemsFromPlan(
        RepoRoot, *rpPlan, Inv.mPlaybooks, "all", Warnings);

    const PhaseItem *rpPhase = nullptr;
    for (const PhaseItem &Phase : Phases)
    {
        if (Phase.mPhaseKey == InOptions.mPhaseKey)
        {
            rpPhase = &Phase;
            break;
        }
    }

    if (!rpPhase)
    {
        std::cerr << "Phase not found: " << InOptions.mPhaseKey << "\n";
        return 1;
    }

    // Load playbook for execution lanes if available
    std::vector<FLaneRecord> Lanes;
    if (!rpPhase->mPlaybookPath.empty())
    {
        FDocument PlaybookDoc;
        std::string Error;
        if (TryLoadDocument(RepoRoot, rpPhase->mPlaybookPath, PlaybookDoc,
                            Error))
        {
            const auto It = PlaybookDoc.mSections.find("execution_lanes");
            if (It != PlaybookDoc.mSections.end())
            {
                for (const FStructuredTable &Table : It->second.mTables)
                {
                    for (const auto &Row : Table.mRows)
                    {
                        FLaneRecord Lane;
                        if (Row.size() > 0)
                            Lane.mLaneID = Row[0].mValue;
                        if (Row.size() > 1)
                            Lane.mStatus = Row[1].mValue;
                        if (Row.size() > 2)
                            Lane.mScope = Row[2].mValue;
                        if (Row.size() > 3)
                            Lane.mExitCriteria = Row[3].mValue;
                        Lanes.push_back(std::move(Lane));
                    }
                }
            }
        }
    }

    PrintJsonHeader(kPhaseDetailSchema, GetUtcNow(), ToGenericPath(RepoRoot));
    std::cout << "\"topic_key\":" << JsonQuote(TopicKey)
              << ",\"phase_key\":" << JsonQuote(rpPhase->mPhaseKey)
              << ",\"status\":" << JsonQuote(rpPhase->mStatus)
              << ",\"playbook_path\":"
              << JsonNullOrQuote(rpPhase->mPlaybookPath)
              << ",\"description\":" << JsonNullOrQuote(rpPhase->mDescription);

    std::cout << ",\"lanes\":[";
    for (size_t Index = 0; Index < Lanes.size(); ++Index)
    {
        if (Index > 0)
            std::cout << ",";
        const FLaneRecord &Lane = Lanes[Index];
        std::cout << "{\"lane\":" << JsonQuote(Lane.mLaneID)
                  << ",\"status\":" << JsonQuote(Lane.mStatus)
                  << ",\"scope\":" << JsonQuote(Lane.mScope)
                  << ",\"exit_criteria\":" << JsonQuote(Lane.mExitCriteria)
                  << "}";
    }
    std::cout << "]";

    std::cout << ",\"lane_count\":" << static_cast<int>(Lanes.size()) << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// phase transition — governed status change with state machine
// ---------------------------------------------------------------------------

static int RunPhaseTransition(const PhaseCommandOptions &InOptions,
                              const bool InUseCache, const DocConfig &InConfig)
{
    const fs::path RepoRoot = NormalizeRepoRootPath(InOptions.mRepoRoot);
    const Inventory Inv =
        BuildInventory(RepoRoot.string(), InUseCache, InConfig.mCacheDir,
                       InConfig.mbCacheVerbose);

    const std::string TopicKey =
        ResolveTopicKeyFromInventory(Inv, InOptions.mTopic);
    const DocumentRecord *rpPlan =
        FindSingleRecordByTopic(Inv.mPlans, TopicKey);
    if (!rpPlan)
    {
        std::cerr << "Plan not found for topic: " << InOptions.mTopic << "\n";
        return 1;
    }

    // Find the phase and its current status
    std::vector<std::string> Warnings;
    const std::vector<PhaseItem> Phases = CollectPhaseItemsFromPlan(
        RepoRoot, *rpPlan, Inv.mPlaybooks, "all", Warnings);

    const PhaseItem *rpPhase = nullptr;
    for (const PhaseItem &Phase : Phases)
    {
        if (Phase.mPhaseKey == InOptions.mPhaseKey)
        {
            rpPhase = &Phase;
            break;
        }
    }

    if (!rpPhase)
    {
        std::cerr << "Phase not found: " << InOptions.mPhaseKey << "\n";
        return 1;
    }

    const EPhaseStatus CurrentStatus = PhaseStatusFromString(rpPhase->mStatus);
    const EPhaseStatus TargetStatus =
        PhaseStatusFromString(InOptions.mToStatus);

    // Validate transition
    std::vector<EPhaseStatus> AllowedTargets;
    if (!ValidatePhaseTransition(CurrentStatus, TargetStatus, AllowedTargets))
    {
        std::cout << "{\"schema\":" << JsonQuote(kPhaseTransitionSchema)
                  << ",\"ok\":false"
                  << ",\"current_status\":"
                  << JsonQuote(ToString(CurrentStatus))
                  << ",\"target_status\":" << JsonQuote(ToString(TargetStatus))
                  << ",\"error\":\"Invalid transition\""
                  << ",\"allowed_targets\":[";
        for (size_t Index = 0; Index < AllowedTargets.size(); ++Index)
        {
            if (Index > 0)
                std::cout << ",";
            std::cout << JsonQuote(ToString(AllowedTargets[Index]));
        }
        std::cout << "]}\n";
        return 1;
    }

    // Playbook required for transition
    if (rpPhase->mPlaybookPath.empty())
    {
        std::cout << "{\"schema\":" << JsonQuote(kPhaseTransitionSchema)
                  << ",\"ok\":false"
                  << ",\"error\":\"No playbook for phase "
                  << InOptions.mPhaseKey << " — cannot transition\"}\n";
        return 1;
    }

    // Update execution_lanes Status column in playbook
    FDocument PlaybookDoc;
    std::string Error;
    if (!TryLoadDocument(RepoRoot, rpPhase->mPlaybookPath, PlaybookDoc, Error))
    {
        std::cout << "{\"schema\":" << JsonQuote(kPhaseTransitionSchema)
                  << ",\"ok\":false"
                  << ",\"error\":"
                  << JsonQuote("Failed to load playbook: " + Error) << "}\n";
        return 1;
    }

    // Find and update execution_lanes
    auto LanesIt = PlaybookDoc.mSections.find("execution_lanes");
    if (LanesIt != PlaybookDoc.mSections.end())
    {
        for (FStructuredTable &Table : LanesIt->second.mTables)
        {
            int StatusCol = -1;
            for (int Col = 0; Col < static_cast<int>(Table.mHeaders.size());
                 ++Col)
            {
                if (ToLower(Trim(Table.mHeaders[static_cast<size_t>(Col)])) ==
                    "status")
                {
                    StatusCol = Col;
                    break;
                }
            }
            if (StatusCol >= 0)
            {
                for (auto &Row : Table.mRows)
                {
                    if (StatusCol < static_cast<int>(Row.size()))
                    {
                        Row[static_cast<size_t>(StatusCol)].mValue =
                            ToString(TargetStatus);
                    }
                }
            }
        }
    }

    // Save the updated playbook
    if (!TrySaveDocument(RepoRoot, PlaybookDoc, Error))
    {
        std::cout << "{\"schema\":" << JsonQuote(kPhaseTransitionSchema)
                  << ",\"ok\":false"
                  << ",\"error\":"
                  << JsonQuote("Failed to save playbook: " + Error) << "}\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JsonQuote(kPhaseTransitionSchema)
              << ",\"ok\":true"
              << ",\"topic_key\":" << JsonQuote(TopicKey)
              << ",\"phase_key\":" << JsonQuote(InOptions.mPhaseKey)
              << ",\"old_status\":" << JsonQuote(ToString(CurrentStatus))
              << ",\"new_status\":" << JsonQuote(ToString(TargetStatus))
              << ",\"playbook_path\":" << JsonQuote(rpPhase->mPlaybookPath)
              << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// Option parsing
// ---------------------------------------------------------------------------

PlanCommandOptions
ParsePlanCommandOptions(const std::vector<std::string> &InTokens)
{
    PlanCommandOptions Options;
    if (InTokens.empty())
    {
        throw UsageError("Missing plan subcommand. Usage: "
                         "uni-plan plan <info|field|update> "
                         "[options]");
    }

    Options.mSubcommand = InTokens[0];
    std::vector<std::string> Remaining(InTokens.begin() + 1, InTokens.end());
    ConsumeCommonOptions(Remaining, Options, false);

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic" && Index + 1 < Remaining.size())
            Options.mTopic = Remaining[++Index];
        else if (Token == "--section" && Index + 1 < Remaining.size())
            Options.mSection = Remaining[++Index];
        else if (Token == "--field" && Index + 1 < Remaining.size())
            Options.mField = Remaining[++Index];
        else if (Token == "--value" && Index + 1 < Remaining.size())
            Options.mValue = Remaining[++Index];
        else if (Token == "--content" && Index + 1 < Remaining.size())
            Options.mContent = Remaining[++Index];
        else if (Token == "--title" && Index + 1 < Remaining.size())
            Options.mTitle = Remaining[++Index];
        else if (Token == "--reason" && Index + 1 < Remaining.size())
            Options.mReason = Remaining[++Index];
    }

    return Options;
}

PhaseCommandOptions
ParsePhaseCommandOptions(const std::vector<std::string> &InTokens)
{
    PhaseCommandOptions Options;
    if (InTokens.empty())
    {
        throw UsageError("Missing phase subcommand. Usage: "
                         "uni-plan phase <detail|transition> [options]");
    }

    Options.mSubcommand = InTokens[0];
    std::vector<std::string> Remaining(InTokens.begin() + 1, InTokens.end());
    ConsumeCommonOptions(Remaining, Options, false);

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic" && Index + 1 < Remaining.size())
            Options.mTopic = Remaining[++Index];
        else if (Token == "--key" && Index + 1 < Remaining.size())
            Options.mPhaseKey = Remaining[++Index];
        else if (Token == "--to" && Index + 1 < Remaining.size())
            Options.mToStatus = Remaining[++Index];
        else if (Token == "--scope" && Index + 1 < Remaining.size())
            Options.mScope = Remaining[++Index];
        else if (Token == "--force")
            Options.mbForce = true;
    }

    return Options;
}

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------

int RunPlanCommand(const PlanCommandOptions &InOptions, const bool InUseCache,
                   const DocConfig &InConfig)
{
    if (InOptions.mSubcommand == "info")
    {
        return RunPlanInfo(InOptions, InUseCache, InConfig);
    }

    if (InOptions.mSubcommand == "field")
    {
        if (!InOptions.mValue.empty())
        {
            return RunPlanFieldSet(InOptions, InUseCache, InConfig);
        }
        return RunPlanFieldGet(InOptions, InUseCache, InConfig);
    }

    std::cerr << "Unknown plan subcommand: " << InOptions.mSubcommand
              << "\nSupported: info, field\n";
    return 2;
}

int RunPhaseExtendedCommand(const PhaseCommandOptions &InOptions,
                            const bool InUseCache, const DocConfig &InConfig)
{
    if (InOptions.mSubcommand == "detail")
    {
        return RunPhaseDetail(InOptions, InUseCache, InConfig);
    }

    if (InOptions.mSubcommand == "transition")
    {
        return RunPhaseTransition(InOptions, InUseCache, InConfig);
    }

    std::cerr << "Unknown phase subcommand: " << InOptions.mSubcommand
              << "\nSupported: detail, transition\n";
    return 2;
}

} // namespace UniPlan
