#include "UniPlanEnums.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanJsonIO.h"
#include "UniPlanJsonLineIndex.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <fstream>
#include <unordered_map>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace UniPlan
{

// ---------------------------------------------------------------------------
// EmitValidationCommandsJson — write a typed validation_commands array
// as JSON: [{"platform":"...","command":"...","description":"..."}, ...]
// Emits an empty array when the input vector is empty (never null).
// ---------------------------------------------------------------------------

static void
EmitValidationCommandsJson(const char *InName,
                           const std::vector<FValidationCommand> &InCommands,
                           bool InTrailingComma = true)
{
    std::cout << "\"" << InName << "\":[";
    for (size_t I = 0; I < InCommands.size(); ++I)
    {
        if (I > 0)
            std::cout << ",";
        const FValidationCommand &C = InCommands[I];
        std::cout << "{";
        EmitJsonField("platform", ToString(C.mPlatform));
        EmitJsonField("command", C.mCommand);
        EmitJsonField("description", C.mDescription, false);
        std::cout << "}";
    }
    std::cout << "]";
    if (InTrailingComma)
        std::cout << ",";
}

// ---------------------------------------------------------------------------
// EmitDependenciesJson — write a typed dependencies array as JSON:
//   [{"kind":"bundle","topic":"X","phase":null,"path":"","note":"..."}, ...]
// Emits an empty array when the input vector is empty (never null).
// ---------------------------------------------------------------------------

static void EmitDependenciesJson(const char *InName,
                                 const std::vector<FBundleReference> &InDeps,
                                 bool InTrailingComma = true)
{
    std::cout << "\"" << InName << "\":[";
    for (size_t I = 0; I < InDeps.size(); ++I)
    {
        if (I > 0)
            std::cout << ",";
        const FBundleReference &R = InDeps[I];
        std::cout << "{";
        EmitJsonField("kind", ToString(R.mKind));
        EmitJsonField("topic", R.mTopic);
        if (R.mPhase < 0)
            std::cout << "\"phase\":null,";
        else
            std::cout << "\"phase\":" << R.mPhase << ",";
        EmitJsonField("path", R.mPath);
        EmitJsonField("note", R.mNote, false);
        std::cout << "}";
    }
    std::cout << "]";
    if (InTrailingComma)
        std::cout << ",";
}

// ---------------------------------------------------------------------------
// TryLoadBundleByTopic — recursive search for <TopicKey>.Plan.json
// ---------------------------------------------------------------------------

bool TryLoadBundleByTopic(const fs::path &InRepoRoot,
                          const std::string &InTopicKey,
                          FTopicBundle &OutBundle, std::string &OutError)
{
    const std::string TargetName = InTopicKey + ".Plan.json";
    std::error_code EC;
    for (auto Iterator = fs::recursive_directory_iterator(InRepoRoot, EC);
         Iterator != fs::recursive_directory_iterator(); ++Iterator)
    {
        if (EC)
            break;
        if (Iterator->is_directory() &&
            ShouldSkipRecursionDirectory(Iterator->path()))
        {
            Iterator.disable_recursion_pending();
            continue;
        }
        if (!Iterator->is_regular_file())
            continue;
        if (Iterator->path().filename().string() != TargetName)
            continue;
        if (!TryReadTopicBundle(Iterator->path(), OutBundle, OutError))
            return false;
        OutBundle.mBundlePath = Iterator->path().string();
        return true;
    }
    OutError = "Bundle not found: " + TargetName +
               " (searched recursively from " + InRepoRoot.string() + ")";
    return false;
}

// ---------------------------------------------------------------------------
// LoadAllBundles — recursive scan for all *.Plan.json
// ---------------------------------------------------------------------------

std::vector<FTopicBundle> LoadAllBundles(const fs::path &InRepoRoot,
                                         std::vector<std::string> &OutWarnings)
{
    std::vector<FTopicBundle> Bundles;
    static const std::regex BundleRegex(R"(^([A-Za-z0-9]+)\.Plan\.json$)");

    std::error_code EC;
    for (auto Iterator = fs::recursive_directory_iterator(InRepoRoot, EC);
         Iterator != fs::recursive_directory_iterator(); ++Iterator)
    {
        if (EC)
            break;
        if (Iterator->is_directory() &&
            ShouldSkipRecursionDirectory(Iterator->path()))
        {
            Iterator.disable_recursion_pending();
            continue;
        }
        if (!Iterator->is_regular_file())
            continue;
        const std::string Filename = Iterator->path().filename().string();
        std::smatch Match;
        if (!std::regex_match(Filename, Match, BundleRegex))
            continue;
        FTopicBundle Bundle;
        std::string Error;
        if (TryReadTopicBundle(Iterator->path(), Bundle, Error))
        {
            Bundle.mBundlePath = Iterator->path().string();
            Bundles.push_back(std::move(Bundle));
        }
        else
        {
            OutWarnings.push_back("Failed to read " + Filename + ": " + Error);
        }
    }

    // Sort by topic key for deterministic output
    std::sort(Bundles.begin(), Bundles.end(),
              [](const FTopicBundle &A, const FTopicBundle &B)
              { return A.mTopicKey < B.mTopicKey; });
    return Bundles;
}

// ---------------------------------------------------------------------------
// Topic List — JSON output
// ---------------------------------------------------------------------------

static int RunTopicListJson(const fs::path &InRepoRoot,
                            const FTopicListOptions &InOptions)
{
    std::vector<std::string> Warnings;
    std::vector<FTopicBundle> Bundles = LoadAllBundles(InRepoRoot, Warnings);

    // Status filter
    const bool bFilter = (InOptions.mStatus != "all");
    if (bFilter)
    {
        Bundles.erase(std::remove_if(
                          Bundles.begin(), Bundles.end(),
                          [&](const FTopicBundle &B)
                          { return ToString(B.mStatus) != InOptions.mStatus; }),
                      Bundles.end());
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kTopicListSchema, UTC, InRepoRoot.string());
    if (bFilter)
        EmitJsonField("status_filter", InOptions.mStatus);
    EmitJsonFieldSizeT("count", Bundles.size());
    std::cout << "\"topics\":[";
    for (size_t I = 0; I < Bundles.size(); ++I)
    {
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonField("topic", Bundles[I].mTopicKey);
        EmitJsonField("status", ToString(Bundles[I].mStatus));
        EmitJsonField("title", Bundles[I].mMetadata.mTitle);
        EmitJsonFieldSizeT("phase_count", Bundles[I].mPhases.size(), false);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Topic List — Human output
// ---------------------------------------------------------------------------

static int RunTopicListHuman(const fs::path &InRepoRoot,
                             const FTopicListOptions &InOptions)
{
    std::vector<std::string> Warnings;
    std::vector<FTopicBundle> Bundles = LoadAllBundles(InRepoRoot, Warnings);

    const bool bFilter = (InOptions.mStatus != "all");
    if (bFilter)
    {
        Bundles.erase(std::remove_if(
                          Bundles.begin(), Bundles.end(),
                          [&](const FTopicBundle &B)
                          { return ToString(B.mStatus) != InOptions.mStatus; }),
                      Bundles.end());
    }

    std::cout << kColorBold << "Topics" << kColorReset;
    if (bFilter)
        std::cout << " [" << kColorYellow << "status=" << InOptions.mStatus
                  << kColorReset << "]";
    std::cout << " count=" << kColorOrange << Bundles.size() << kColorReset
              << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Topic", "Status", "Phases", "Title"};
    for (const FTopicBundle &B : Bundles)
    {
        Table.AddRow({B.mTopicKey, ColorizeStatus(ToString(B.mStatus)),
                      std::to_string(B.mPhases.size()), B.mMetadata.mTitle});
    }
    Table.Print();
    return 0;
}

// ---------------------------------------------------------------------------
// Topic Get — JSON output
// ---------------------------------------------------------------------------

static int RunTopicGetJson(const fs::path &InRepoRoot,
                           const FTopicGetOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kTopicGetSchema, UTC, InRepoRoot.string());
    EmitJsonField("topic", Bundle.mTopicKey);
    EmitJsonField("status", ToString(Bundle.mStatus));
    const FPlanMetadata &Meta = Bundle.mMetadata;
    EmitJsonField("title", Meta.mTitle);
    EmitJsonFieldNullable("summary", Meta.mSummary);
    EmitJsonFieldNullable("goals", Meta.mGoals);
    EmitJsonFieldNullable("non_goals", Meta.mNonGoals);
    EmitJsonFieldNullable("risks", Meta.mRisks);
    EmitJsonFieldNullable("acceptance_criteria", Meta.mAcceptanceCriteria);
    EmitJsonFieldNullable("problem_statement", Meta.mProblemStatement);
    EmitValidationCommandsJson("validation_commands", Meta.mValidationCommands);
    EmitJsonFieldNullable("baseline_audit", Meta.mBaselineAudit);
    EmitJsonFieldNullable("execution_strategy", Meta.mExecutionStrategy);
    EmitJsonFieldNullable("locked_decisions", Meta.mLockedDecisions);
    EmitJsonFieldNullable("source_references", Meta.mSourceReferences);
    EmitDependenciesJson("dependencies", Meta.mDependencies);
    EmitJsonFieldNullable("next_actions", Bundle.mNextActions);
    EmitJsonFieldSizeT("phase_count", Bundle.mPhases.size());

    // Phase summary — compact index/status/scope
    std::cout << "\"phase_summary\":[";
    for (size_t I = 0; I < Bundle.mPhases.size(); ++I)
    {
        const FPhaseRecord &Phase = Bundle.mPhases[I];
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonFieldSizeT("index", I);
        EmitJsonField("status", ToString(Phase.mLifecycle.mStatus));
        // Truncate scope to 120 chars for compactness
        std::string Scope = Phase.mScope;
        if (Scope.size() > 120)
            Scope = Scope.substr(0, 117) + "...";
        EmitJsonField("scope", Scope, false);
        std::cout << "}";
    }
    std::cout << "],";

    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Topic Get — Human output
// ---------------------------------------------------------------------------

static int RunTopicGetHuman(const fs::path &InRepoRoot,
                            const FTopicGetOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << kColorBold << Bundle.mTopicKey << kColorReset
              << "  status=" << ColorizeStatus(ToString(Bundle.mStatus))
              << "  phases=" << kColorOrange << Bundle.mPhases.size()
              << kColorReset << "\n";
    std::cout << kColorDim << Bundle.mMetadata.mTitle << kColorReset << "\n\n";

    auto PrintField = [](const char *InLabel, const std::string &InVal)
    {
        if (!InVal.empty())
        {
            std::cout << kColorBold << InLabel << kColorReset << "\n"
                      << kColorDim << InVal << kColorReset << "\n\n";
        }
    };

    PrintField("Summary", Bundle.mMetadata.mSummary);
    PrintField("Goals", Bundle.mMetadata.mGoals);
    PrintField("Non-Goals", Bundle.mMetadata.mNonGoals);
    PrintField("Risks", Bundle.mMetadata.mRisks);
    PrintField("Acceptance Criteria", Bundle.mMetadata.mAcceptanceCriteria);
    PrintField("Problem Statement", Bundle.mMetadata.mProblemStatement);
    PrintField("Baseline Audit", Bundle.mMetadata.mBaselineAudit);
    PrintField("Execution Strategy", Bundle.mMetadata.mExecutionStrategy);
    PrintField("Locked Decisions", Bundle.mMetadata.mLockedDecisions);
    PrintField("Source References", Bundle.mMetadata.mSourceReferences);
    PrintField("Next Actions", Bundle.mNextActions);

    // Validation commands table
    if (!Bundle.mMetadata.mValidationCommands.empty())
    {
        std::cout << kColorBold << "Validation Commands" << kColorReset << "\n";
        HumanTable VCTable;
        VCTable.mHeaders = {"Platform", "Command", "Description"};
        for (const FValidationCommand &C : Bundle.mMetadata.mValidationCommands)
        {
            VCTable.AddRow({ToString(C.mPlatform), C.mCommand, C.mDescription});
        }
        VCTable.Print();
        std::cout << "\n";
    }

    // Dependencies table
    if (!Bundle.mMetadata.mDependencies.empty())
    {
        std::cout << kColorBold << "Dependencies" << kColorReset << "\n";
        HumanTable DepTable;
        DepTable.mHeaders = {"Kind", "Topic", "Phase", "Path", "Note"};
        for (const FBundleReference &R : Bundle.mMetadata.mDependencies)
        {
            const std::string PhaseCell =
                R.mPhase < 0 ? "" : std::to_string(R.mPhase);
            DepTable.AddRow(
                {ToString(R.mKind), R.mTopic, PhaseCell, R.mPath, R.mNote});
        }
        DepTable.Print();
        std::cout << "\n";
    }

    // Phase table
    std::cout << kColorBold << "Phases" << kColorReset << "\n";
    HumanTable Table;
    Table.mHeaders = {"Index", "Status", "Scope"};
    for (size_t I = 0; I < Bundle.mPhases.size(); ++I)
    {
        const FPhaseRecord &Phase = Bundle.mPhases[I];
        std::string Scope = Phase.mScope;
        if (Scope.size() > 80)
            Scope = Scope.substr(0, 77) + "...";
        Table.AddRow({std::to_string(I),
                      ColorizeStatus(ToString(Phase.mLifecycle.mStatus)),
                      kColorDim + Scope + kColorReset});
    }
    Table.Print();
    return 0;
}

// ---------------------------------------------------------------------------
// RunTopicCommand — dispatch topic list / topic get
// ---------------------------------------------------------------------------

int RunTopicCommand(const std::vector<std::string> &InArgs,
                    const std::string &InRepoRoot)
{
    if (InArgs.empty() || ContainsHelpFlag(InArgs))
    {
        throw UsageError("topic requires a subcommand: list, get, set, "
                         "start, complete, block, status");
    }

    const std::string Sub = InArgs[0];
    const std::vector<std::string> SubArgs(InArgs.begin() + 1, InArgs.end());

    if (Sub == "list")
    {
        const FTopicListOptions Options = ParseTopicListOptions(SubArgs);
        const fs::path RepoRoot = NormalizeRepoRootPath(
            Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
        if (Options.mbHuman)
            return RunTopicListHuman(RepoRoot, Options);
        return RunTopicListJson(RepoRoot, Options);
    }

    if (Sub == "get")
    {
        const FTopicGetOptions Options = ParseTopicGetOptions(SubArgs);
        const fs::path RepoRoot = NormalizeRepoRootPath(
            Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
        if (Options.mbHuman)
            return RunTopicGetHuman(RepoRoot, Options);
        return RunTopicGetJson(RepoRoot, Options);
    }

    if (Sub == "set")
    {
        return RunTopicSetCommand(SubArgs, InRepoRoot);
    }

    // Semantic topic commands
    if (Sub == "start")
        return RunTopicStartCommand(SubArgs, InRepoRoot);
    if (Sub == "complete")
        return RunTopicCompleteCommand(SubArgs, InRepoRoot);
    if (Sub == "block")
        return RunTopicBlockCommand(SubArgs, InRepoRoot);
    if (Sub == "status")
        return RunTopicStatusCommand(SubArgs, InRepoRoot);

    throw UsageError("Unknown topic subcommand: " + Sub +
                     ". Expected: list, get, set, start, complete, block, "
                     "status");
}

// ===================================================================
// Batch 3: phase list + phase get
// ===================================================================

// ---------------------------------------------------------------------------
// Phase List — JSON
// ---------------------------------------------------------------------------

static int RunBundlePhaseListJson(const fs::path &InRepoRoot,
                                  const FPhaseListOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const bool bFilter = (InOptions.mStatus != "all");
    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kPhaseListSchemaV2, UTC, InRepoRoot.string());
    EmitJsonField("topic", Bundle.mTopicKey);

    // Build filtered indices
    std::vector<size_t> Indices;
    for (size_t I = 0; I < Bundle.mPhases.size(); ++I)
    {
        const std::string Status =
            ToString(Bundle.mPhases[I].mLifecycle.mStatus);
        if (!bFilter || Status == InOptions.mStatus)
            Indices.push_back(I);
    }

    if (bFilter)
        EmitJsonField("status_filter", InOptions.mStatus);
    EmitJsonFieldSizeT("count", Indices.size());
    std::cout << "\"phases\":[";
    for (size_t N = 0; N < Indices.size(); ++N)
    {
        const size_t I = Indices[N];
        const FPhaseRecord &Phase = Bundle.mPhases[I];
        PrintJsonSep(N);
        std::cout << "{";
        EmitJsonFieldSizeT("index", I);
        EmitJsonField("status", ToString(Phase.mLifecycle.mStatus));
        std::string Scope = Phase.mScope;
        if (Scope.size() > 120)
            Scope = Scope.substr(0, 117) + "...";
        EmitJsonField("scope", Scope);
        EmitJsonFieldSizeT("lane_count", Phase.mLanes.size());
        EmitJsonFieldSizeT("job_count", Phase.mJobs.size());
        size_t TaskCount = 0;
        for (const FJobRecord &J : Phase.mJobs)
            TaskCount += J.mTasks.size();
        EmitJsonFieldSizeT("task_count", TaskCount, false);
        std::cout << "}";
    }
    std::cout << "],";
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Phase List — Human
// ---------------------------------------------------------------------------

static int RunBundlePhaseListHuman(const fs::path &InRepoRoot,
                                   const FPhaseListOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const bool bFilter = (InOptions.mStatus != "all");
    std::cout << kColorBold << "Phases" << kColorReset
              << " topic=" << kColorOrange << Bundle.mTopicKey << kColorReset;
    if (bFilter)
        std::cout << " [status=" << InOptions.mStatus << "]";
    std::cout << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Index", "Status", "Jobs", "Tasks", "Scope"};
    for (size_t I = 0; I < Bundle.mPhases.size(); ++I)
    {
        const FPhaseRecord &Phase = Bundle.mPhases[I];
        const std::string Status = ToString(Phase.mLifecycle.mStatus);
        if (bFilter && Status != InOptions.mStatus)
            continue;
        size_t TaskCount = 0;
        for (const FJobRecord &J : Phase.mJobs)
            TaskCount += J.mTasks.size();
        std::string Scope = Phase.mScope;
        if (Scope.size() > 60)
            Scope = Scope.substr(0, 57) + "...";
        Table.AddRow({std::to_string(I), ColorizeStatus(Status),
                      std::to_string(Phase.mJobs.size()),
                      std::to_string(TaskCount),
                      kColorDim + Scope + kColorReset});
    }
    Table.Print();
    return 0;
}

// ---------------------------------------------------------------------------
// Phase Get — JSON
// ---------------------------------------------------------------------------

// Compute next actionable job index: first non-completed job
// in the lowest incomplete wave.
static int ComputeNextJob(const FPhaseRecord &InPhase)
{
    if (InPhase.mJobs.empty())
        return -1;
    // Find lowest incomplete wave
    int LowestWave = INT_MAX;
    for (const FJobRecord &Job : InPhase.mJobs)
    {
        if (Job.mStatus != EExecutionStatus::Completed &&
            Job.mWave < LowestWave)
            LowestWave = Job.mWave;
    }
    if (LowestWave == INT_MAX)
        return -1;
    // First non-completed job in that wave
    for (size_t I = 0; I < InPhase.mJobs.size(); ++I)
    {
        if (InPhase.mJobs[I].mWave == LowestWave &&
            InPhase.mJobs[I].mStatus != EExecutionStatus::Completed)
            return static_cast<int>(I);
    }
    return -1;
}

// Emit progress object: jobs/tasks completed/total/percent
static void EmitProgress(const FPhaseRecord &InPhase)
{
    size_t JobsDone = 0, TasksDone = 0, TasksTotal = 0;
    for (const FJobRecord &Job : InPhase.mJobs)
    {
        if (Job.mStatus == EExecutionStatus::Completed)
            JobsDone++;
        for (const FTaskRecord &Task : Job.mTasks)
        {
            TasksTotal++;
            if (Task.mStatus == EExecutionStatus::Completed)
                TasksDone++;
        }
    }
    const size_t JobsTotal = InPhase.mJobs.size();
    const int Percent =
        JobsTotal > 0 ? static_cast<int>(JobsDone * 100 / JobsTotal) : 0;
    std::cout << "\"progress\":{";
    EmitJsonFieldSizeT("jobs_completed", JobsDone);
    EmitJsonFieldSizeT("jobs_total", JobsTotal);
    EmitJsonFieldSizeT("tasks_completed", TasksDone);
    EmitJsonFieldSizeT("tasks_total", TasksTotal);
    EmitJsonFieldInt("percent", Percent, false);
    std::cout << "},";
}

static int RunBundlePhaseGetJson(const fs::path &InRepoRoot,
                                 const FPhaseGetOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (InOptions.mPhaseIndex < 0 ||
        static_cast<size_t>(InOptions.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index " << InOptions.mPhaseIndex
                  << " out of range (0.." << Bundle.mPhases.size() - 1 << ")\n";
        return 1;
    }

    const FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(InOptions.mPhaseIndex)];
    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kPhaseGetSchema, UTC, InRepoRoot.string());
    EmitJsonField("topic", Bundle.mTopicKey);
    EmitJsonFieldInt("phase_index", InOptions.mPhaseIndex);
    EmitJsonField("status", ToString(Phase.mLifecycle.mStatus));
    EmitJsonFieldNullable("scope", Phase.mScope);

    // --brief: compact view for session resume (~500 tokens)
    if (InOptions.mbBrief)
    {
        std::string Done = Phase.mLifecycle.mDone;
        if (Done.size() > 200)
            Done = Done.substr(0, 197) + "...";
        EmitJsonFieldNullable("done", Done);
        EmitJsonFieldNullable("remaining", Phase.mLifecycle.mRemaining);
        EmitJsonFieldNullable("blockers", Phase.mLifecycle.mBlockers);
        EmitJsonFieldNullable("agent_context", Phase.mLifecycle.mAgentContext);
        EmitJsonFieldSizeT("job_count", Phase.mJobs.size());
        // Job summary — index + status + scope only
        std::cout << "\"job_summary\":[";
        for (size_t I = 0; I < Phase.mJobs.size(); ++I)
        {
            PrintJsonSep(I);
            std::cout << "{";
            EmitJsonFieldSizeT("index", I);
            EmitJsonField("status", ToString(Phase.mJobs[I].mStatus));
            std::string Scope = Phase.mJobs[I].mScope;
            if (Scope.size() > 80)
                Scope = Scope.substr(0, 77) + "...";
            EmitJsonField("scope", Scope, false);
            std::cout << "}";
        }
        std::cout << "],";
        EmitJsonFieldInt("next_job", ComputeNextJob(Phase));
        EmitProgress(Phase);
        std::vector<std::string> Warnings;
        PrintJsonClose(Warnings);
        return 0;
    }

    // --reference: design material only
    if (InOptions.mbReference)
    {
        EmitJsonFieldNullable("readiness_gate", Phase.mDesign.mReadinessGate);
        EmitJsonFieldNullable("investigation", Phase.mDesign.mInvestigation);
        EmitDependenciesJson("dependencies", Phase.mDesign.mDependencies);
        EmitJsonFieldNullable("code_entity_contract",
                              Phase.mDesign.mCodeEntityContract);
        EmitJsonFieldNullable("code_snippets", Phase.mDesign.mCodeSnippets);
        EmitJsonFieldNullable("best_practices", Phase.mDesign.mBestPractices);
        EmitJsonFieldNullable("multi_platforming",
                              Phase.mDesign.mMultiPlatforming);
        EmitJsonFieldNullable("handoff", Phase.mDesign.mHandoff);
        EmitValidationCommandsJson("validation_commands",
                                   Phase.mDesign.mValidationCommands);
        std::vector<std::string> Warnings;
        PrintJsonClose(Warnings);
        return 0;
    }

    // Full or --execution mode
    EmitJsonFieldNullable("output", Phase.mOutput);
    EmitJsonFieldNullable("done", Phase.mLifecycle.mDone);
    EmitJsonFieldNullable("remaining", Phase.mLifecycle.mRemaining);
    EmitJsonFieldNullable("blockers", Phase.mLifecycle.mBlockers);
    EmitJsonFieldNullable("started_at", Phase.mLifecycle.mStartedAt);
    EmitJsonFieldNullable("completed_at", Phase.mLifecycle.mCompletedAt);
    EmitJsonFieldNullable("agent_context", Phase.mLifecycle.mAgentContext);

    if (!InOptions.mbExecution)
    {
        // Full mode includes reference fields
        EmitJsonFieldNullable("readiness_gate", Phase.mDesign.mReadinessGate);
        EmitJsonFieldNullable("investigation", Phase.mDesign.mInvestigation);
        EmitDependenciesJson("dependencies", Phase.mDesign.mDependencies);
        EmitJsonFieldNullable("handoff", Phase.mDesign.mHandoff);
        EmitValidationCommandsJson("validation_commands",
                                   Phase.mDesign.mValidationCommands);
        EmitJsonFieldNullable("best_practices", Phase.mDesign.mBestPractices);
        EmitJsonFieldNullable("code_entity_contract",
                              Phase.mDesign.mCodeEntityContract);
        EmitJsonFieldNullable("code_snippets", Phase.mDesign.mCodeSnippets);
        EmitJsonFieldNullable("multi_platforming",
                              Phase.mDesign.mMultiPlatforming);
    }

    // Lanes
    std::cout << "\"lanes\":[";
    for (size_t I = 0; I < Phase.mLanes.size(); ++I)
    {
        const FLaneRecord &Lane = Phase.mLanes[I];
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonFieldSizeT("index", I);
        EmitJsonField("status", ToString(Lane.mStatus));
        EmitJsonField("scope", Lane.mScope);
        EmitJsonField("exit_criteria", Lane.mExitCriteria, false);
        std::cout << "}";
    }
    std::cout << "],";

    // Jobs with nested tasks
    std::cout << "\"jobs\":[";
    for (size_t I = 0; I < Phase.mJobs.size(); ++I)
    {
        const FJobRecord &Job = Phase.mJobs[I];
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonFieldSizeT("index", I);
        EmitJsonFieldInt("wave", Job.mWave);
        EmitJsonFieldInt("lane", Job.mLane);
        EmitJsonField("status", ToString(Job.mStatus));
        EmitJsonField("scope", Job.mScope);
        EmitJsonFieldNullable("output", Job.mOutput);
        EmitJsonField("exit_criteria", Job.mExitCriteria);
        EmitJsonFieldNullable("started_at", Job.mStartedAt);
        EmitJsonFieldNullable("completed_at", Job.mCompletedAt);
        std::cout << "\"tasks\":[";
        for (size_t T = 0; T < Job.mTasks.size(); ++T)
        {
            const FTaskRecord &Task = Job.mTasks[T];
            PrintJsonSep(T);
            std::cout << "{";
            EmitJsonFieldSizeT("index", T);
            EmitJsonField("status", ToString(Task.mStatus));
            EmitJsonField("description", Task.mDescription);
            EmitJsonFieldNullable("evidence", Task.mEvidence);
            EmitJsonFieldNullable("notes", Task.mNotes);
            EmitJsonFieldNullable("completed_at", Task.mCompletedAt, false);
            std::cout << "}";
        }
        std::cout << "]";
        std::cout << "}";
    }
    std::cout << "],";

    EmitJsonFieldInt("next_job", ComputeNextJob(Phase));
    EmitProgress(Phase);

    if (!InOptions.mbExecution)
    {
        // Full mode: include testing + file_manifest
        std::cout << "\"testing\":[";
        for (size_t I = 0; I < Phase.mTesting.size(); ++I)
        {
            const FTestingRecord &Test = Phase.mTesting[I];
            PrintJsonSep(I);
            std::cout << "{";
            EmitJsonField("session", Test.mSession);
            EmitJsonField("actor", ToString(Test.mActor));
            EmitJsonField("step", Test.mStep);
            EmitJsonField("action", Test.mAction);
            EmitJsonField("expected", Test.mExpected);
            EmitJsonField("evidence", Test.mEvidence, false);
            std::cout << "}";
        }
        std::cout << "],";

        std::cout << "\"file_manifest\":[";
        for (size_t I = 0; I < Phase.mFileManifest.size(); ++I)
        {
            const FFileManifestItem &Item = Phase.mFileManifest[I];
            PrintJsonSep(I);
            std::cout << "{";
            EmitJsonField("file", Item.mFilePath);
            EmitJsonField("action", ToString(Item.mAction));
            EmitJsonField("description", Item.mDescription, false);
            std::cout << "}";
        }
        std::cout << "],";
    }

    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Phase Get — Human
// ---------------------------------------------------------------------------

static int RunBundlePhaseGetHuman(const fs::path &InRepoRoot,
                                  const FPhaseGetOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (InOptions.mPhaseIndex < 0 ||
        static_cast<size_t>(InOptions.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index " << InOptions.mPhaseIndex
                  << " out of range (0.." << Bundle.mPhases.size() - 1 << ")\n";
        return 1;
    }

    const FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(InOptions.mPhaseIndex)];

    std::cout << kColorBold << Bundle.mTopicKey << " phases["
              << InOptions.mPhaseIndex << "]" << kColorReset << "  status="
              << ColorizeStatus(ToString(Phase.mLifecycle.mStatus)) << "\n\n";

    auto PrintField = [](const char *InLabel, const std::string &InVal)
    {
        if (!InVal.empty())
        {
            std::cout << kColorBold << InLabel << kColorReset << "\n"
                      << kColorDim << InVal << kColorReset << "\n\n";
        }
    };

    PrintField("Scope", Phase.mScope);
    PrintField("Output", Phase.mOutput);
    PrintField("Done", Phase.mLifecycle.mDone);
    PrintField("Remaining", Phase.mLifecycle.mRemaining);
    PrintField("Blockers", Phase.mLifecycle.mBlockers);
    PrintField("Agent Context", Phase.mLifecycle.mAgentContext);
    PrintField("Readiness Gate", Phase.mDesign.mReadinessGate);
    PrintField("Handoff", Phase.mDesign.mHandoff);
    PrintField("Multi Platforming", Phase.mDesign.mMultiPlatforming);

    // Dependencies table — typed vector of FBundleReference.
    if (!Phase.mDesign.mDependencies.empty())
    {
        std::cout << kColorBold << "Dependencies" << kColorReset << "\n";
        HumanTable DepTable;
        DepTable.mHeaders = {"Kind", "Topic", "Phase", "Path", "Note"};
        for (const FBundleReference &R : Phase.mDesign.mDependencies)
        {
            const std::string PhaseCell =
                R.mPhase < 0 ? "" : std::to_string(R.mPhase);
            DepTable.AddRow(
                {ToString(R.mKind), R.mTopic, PhaseCell, R.mPath, R.mNote});
        }
        DepTable.Print();
        std::cout << "\n";
    }

    // Validation commands table — typed vector of FValidationCommand.
    if (!Phase.mDesign.mValidationCommands.empty())
    {
        std::cout << kColorBold << "Validation Commands" << kColorReset << "\n";
        HumanTable VCTable;
        VCTable.mHeaders = {"Platform", "Command", "Description"};
        for (const FValidationCommand &C : Phase.mDesign.mValidationCommands)
        {
            VCTable.AddRow({ToString(C.mPlatform), C.mCommand, C.mDescription});
        }
        VCTable.Print();
        std::cout << "\n";
    }

    // Lanes table
    if (!Phase.mLanes.empty())
    {
        std::cout << kColorBold << "Lanes" << kColorReset << "\n";
        HumanTable LaneTable;
        LaneTable.mHeaders = {"L", "Status", "Scope", "Exit Criteria"};
        for (size_t I = 0; I < Phase.mLanes.size(); ++I)
        {
            const FLaneRecord &Lane = Phase.mLanes[I];
            LaneTable.AddRow({std::to_string(I),
                              ColorizeStatus(ToString(Lane.mStatus)),
                              Lane.mScope, Lane.mExitCriteria});
        }
        LaneTable.Print();
        std::cout << "\n";
    }

    // Jobs table
    if (!Phase.mJobs.empty())
    {
        std::cout << kColorBold << "Jobs" << kColorReset << "\n";
        HumanTable JobTable;
        JobTable.mHeaders = {"J", "W", "L", "Status", "Scope", "Tasks"};
        for (size_t I = 0; I < Phase.mJobs.size(); ++I)
        {
            const FJobRecord &Job = Phase.mJobs[I];
            JobTable.AddRow({std::to_string(I), std::to_string(Job.mWave),
                             std::to_string(Job.mLane),
                             ColorizeStatus(ToString(Job.mStatus)), Job.mScope,
                             std::to_string(Job.mTasks.size())});
        }
        JobTable.Print();
        std::cout << "\n";

        // Print tasks per job
        for (size_t I = 0; I < Phase.mJobs.size(); ++I)
        {
            const FJobRecord &Job = Phase.mJobs[I];
            if (Job.mTasks.empty())
                continue;
            std::cout << kColorBold << "  J" << I << " Tasks" << kColorReset
                      << "\n";
            for (size_t T = 0; T < Job.mTasks.size(); ++T)
            {
                const FTaskRecord &Task = Job.mTasks[T];
                std::cout << "    [" << T << "] "
                          << ColorizeStatus(ToString(Task.mStatus)) << "  "
                          << Task.mDescription << "\n";
            }
            std::cout << "\n";
        }
    }

    PrintField("Investigation", Phase.mDesign.mInvestigation);
    PrintField("Code Entity Contract", Phase.mDesign.mCodeEntityContract);
    PrintField("Code Snippets", Phase.mDesign.mCodeSnippets);
    PrintField("Best Practices", Phase.mDesign.mBestPractices);

    // File manifest table
    if (!Phase.mFileManifest.empty())
    {
        std::cout << kColorBold << "File Manifest" << kColorReset << "\n";
        HumanTable ManTable;
        ManTable.mHeaders = {"#", "Action", "File", "Description"};
        for (size_t I = 0; I < Phase.mFileManifest.size(); ++I)
        {
            const FFileManifestItem &FM = Phase.mFileManifest[I];
            ManTable.AddRow({std::to_string(I), ToString(FM.mAction),
                             FM.mFilePath, FM.mDescription});
        }
        ManTable.Print();
        std::cout << "\n";
    }

    // Testing table
    if (!Phase.mTesting.empty())
    {
        std::cout << kColorBold << "Testing" << kColorReset << "\n";
        HumanTable TestTable;
        TestTable.mHeaders = {"#",      "Actor",    "Session", "Step",
                              "Action", "Expected", "Evidence"};
        for (size_t I = 0; I < Phase.mTesting.size(); ++I)
        {
            const FTestingRecord &T = Phase.mTesting[I];
            TestTable.AddRow({std::to_string(I), ToString(T.mActor), T.mSession,
                              T.mStep, T.mAction, T.mExpected, T.mEvidence});
        }
        TestTable.Print();
        std::cout << "\n";
    }

    return 0;
}

// ---------------------------------------------------------------------------
// RunBundlePhaseCommand — dispatch phase list / phase get
// ---------------------------------------------------------------------------

int RunBundlePhaseCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot)
{
    if (InArgs.empty() || ContainsHelpFlag(InArgs))
    {
        throw UsageError("phase requires a subcommand: list, get, set, add, "
                         "remove, start, complete, block, unblock, progress, "
                         "complete-jobs, log, verify, next, readiness, "
                         "wave-status");
    }

    const std::string Sub = InArgs[0];
    const std::vector<std::string> SubArgs(InArgs.begin() + 1, InArgs.end());

    if (Sub == "list")
    {
        const FPhaseListOptions Options = ParsePhaseListOptions(SubArgs);
        const fs::path RepoRoot = NormalizeRepoRootPath(
            Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
        if (Options.mbHuman)
            return RunBundlePhaseListHuman(RepoRoot, Options);
        return RunBundlePhaseListJson(RepoRoot, Options);
    }

    if (Sub == "get")
    {
        const FPhaseGetOptions Options = ParsePhaseGetOptions(SubArgs);
        const fs::path RepoRoot = NormalizeRepoRootPath(
            Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
        if (Options.mbHuman)
            return RunBundlePhaseGetHuman(RepoRoot, Options);
        return RunBundlePhaseGetJson(RepoRoot, Options);
    }

    if (Sub == "set")
    {
        return RunPhaseSetCommand(SubArgs, InRepoRoot);
    }

    if (Sub == "remove")
    {
        return RunPhaseRemoveCommand(SubArgs, InRepoRoot);
    }

    if (Sub == "add")
    {
        return RunPhaseAddCommand(SubArgs, InRepoRoot);
    }

    if (Sub == "normalize")
    {
        return RunPhaseNormalizeCommand(SubArgs, InRepoRoot);
    }

    // Semantic phase commands
    if (Sub == "start")
        return RunPhaseStartCommand(SubArgs, InRepoRoot);
    if (Sub == "complete")
        return RunPhaseCompleteCommand(SubArgs, InRepoRoot);
    if (Sub == "block")
        return RunPhaseBlockCommand(SubArgs, InRepoRoot);
    if (Sub == "unblock")
        return RunPhaseUnblockCommand(SubArgs, InRepoRoot);
    if (Sub == "progress")
        return RunPhaseProgressCommand(SubArgs, InRepoRoot);
    if (Sub == "complete-jobs")
        return RunPhaseCompleteJobsCommand(SubArgs, InRepoRoot);
    if (Sub == "log")
        return RunPhaseLogCommand(SubArgs, InRepoRoot);
    if (Sub == "verify")
        return RunPhaseVerifyCommand(SubArgs, InRepoRoot);
    if (Sub == "next")
        return RunPhaseNextCommand(SubArgs, InRepoRoot);
    if (Sub == "readiness")
        return RunPhaseReadinessCommand(SubArgs, InRepoRoot);
    if (Sub == "wave-status")
        return RunPhaseWaveStatusCommand(SubArgs, InRepoRoot);

    throw UsageError("Unknown phase subcommand: " + Sub +
                     ". Expected: list, get, set, add, remove, start, "
                     "complete, block, unblock, progress, complete-jobs, "
                     "log, verify, next, readiness, wave-status");
}

// ===================================================================
// Batch 4: changelog + verification + timeline + blockers
// ===================================================================

// ---------------------------------------------------------------------------
// Changelog — JSON
// ---------------------------------------------------------------------------

// Resolve phase index to human-readable label
static std::string ResolvePhaseLabel(int InPhase,
                                     const std::vector<FPhaseRecord> &InPhases)
{
    if (InPhase < 0)
        return "";
    const size_t Idx = static_cast<size_t>(InPhase);
    if (Idx < InPhases.size())
    {
        std::string Scope = InPhases[Idx].mScope;
        if (Scope.size() > 60)
            Scope = Scope.substr(0, 57) + "...";
        return "phases[" + std::to_string(InPhase) + "] " + Scope;
    }
    return std::to_string(InPhase);
}

// Sort key for changelog entries: -1 (topic-level) first, then ascending
static int PhaseSortKey(int InPhase)
{
    return InPhase; // -1 sorts before 0, 1, 2, ...
}

// Sort scope: plan < implementation < numeric ascending
// ScopeSortKey for verification entries (still string-based)
static int ScopeSortKey(const std::string &InScope)
{
    if (InScope.empty() || InScope == "plan")
        return -1;
    bool bIsDigit = !InScope.empty();
    for (char C : InScope)
    {
        if (!std::isdigit(static_cast<unsigned char>(C)))
        {
            bIsDigit = false;
            break;
        }
    }
    if (bIsDigit)
        return std::atoi(InScope.c_str());
    return 9999;
}

static int RunBundleChangelogJson(const fs::path &InRepoRoot,
                                  const FBundleChangelogOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // Filter by phase
    const int PhaseFilter = InOptions.mbHasScopeFilter
                                ? std::atoi(InOptions.mScopeFilter.c_str())
                                : -2;
    std::vector<const FChangeLogEntry *> Filtered;
    for (const FChangeLogEntry &Entry : Bundle.mChangeLogs)
    {
        if (PhaseFilter != -2 && Entry.mPhase != PhaseFilter)
            continue;
        Filtered.push_back(&Entry);
    }

    // Sort: topic-level (-1) first, then ascending phase, then date desc
    std::sort(Filtered.begin(), Filtered.end(),
              [](const FChangeLogEntry *A, const FChangeLogEntry *B)
              {
                  if (A->mPhase != B->mPhase)
                      return A->mPhase < B->mPhase;
                  return A->mDate > B->mDate;
              });

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kChangelogSchemaV2, UTC, InRepoRoot.string());
    EmitJsonField("topic", Bundle.mTopicKey);
    if (InOptions.mbHasScopeFilter)
        EmitJsonFieldInt("phase_filter", PhaseFilter);
    EmitJsonFieldSizeT("count", Filtered.size());
    std::cout << "\"entries\":[";
    for (size_t I = 0; I < Filtered.size(); ++I)
    {
        const FChangeLogEntry &Entry = *Filtered[I];
        PrintJsonSep(I);
        std::cout << "{";
        if (Entry.mPhase < 0)
            std::cout << "\"phase\":null,";
        else
            EmitJsonFieldInt("phase", Entry.mPhase);
        EmitJsonField("phase_label",
                      ResolvePhaseLabel(Entry.mPhase, Bundle.mPhases));
        EmitJsonField("date", Entry.mDate);
        EmitJsonField("change", Entry.mChange);
        EmitJsonFieldNullable("affected", Entry.mAffected);
        EmitJsonField("type", ToString(Entry.mType));
        EmitJsonField("actor", ToString(Entry.mActor), false);
        std::cout << "}";
    }
    std::cout << "],";
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Changelog — Human
// ---------------------------------------------------------------------------

static int RunBundleChangelogHuman(const fs::path &InRepoRoot,
                                   const FBundleChangelogOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const int PhaseFilter = InOptions.mbHasScopeFilter
                                ? std::atoi(InOptions.mScopeFilter.c_str())
                                : -2;
    std::vector<const FChangeLogEntry *> Filtered;
    for (const FChangeLogEntry &Entry : Bundle.mChangeLogs)
    {
        if (PhaseFilter != -2 && Entry.mPhase != PhaseFilter)
            continue;
        Filtered.push_back(&Entry);
    }

    // Sort: topic-level (-1) first, then ascending phase, then date desc
    std::sort(Filtered.begin(), Filtered.end(),
              [](const FChangeLogEntry *A, const FChangeLogEntry *B)
              {
                  if (A->mPhase != B->mPhase)
                      return A->mPhase < B->mPhase;
                  return A->mDate > B->mDate;
              });

    std::cout << kColorBold << "Changelog" << kColorReset
              << " topic=" << kColorOrange << Bundle.mTopicKey << kColorReset
              << " count=" << Filtered.size() << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Phase", "Date", "Type", "Actor", "Affected", "Change"};
    for (const FChangeLogEntry *rpEntry : Filtered)
    {
        std::string Change = rpEntry->mChange;
        if (Change.size() > 80)
            Change = Change.substr(0, 77) + "...";
        const std::string Label =
            ResolvePhaseLabel(rpEntry->mPhase, Bundle.mPhases);
        std::string PhaseDisplay = Label;
        if (PhaseDisplay.size() > 40)
            PhaseDisplay = PhaseDisplay.substr(0, 37) + "...";
        std::string Affected = rpEntry->mAffected;
        if (Affected.size() > 40)
            Affected = Affected.substr(0, 37) + "...";
        Table.AddRow({PhaseDisplay, rpEntry->mDate, ToString(rpEntry->mType),
                      ToString(rpEntry->mActor), Affected,
                      kColorDim + Change + kColorReset});
    }
    Table.Print();
    return 0;
}

// ---------------------------------------------------------------------------
// RunBundleChangelogCommand
// ---------------------------------------------------------------------------

int RunBundleChangelogCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot)
{
    // Check for "add" subcommand
    if (!InArgs.empty() && InArgs[0] == "add")
    {
        const std::vector<std::string> SubArgs(InArgs.begin() + 1,
                                               InArgs.end());
        return RunChangelogAddCommand(SubArgs, InRepoRoot);
    }
    if (!InArgs.empty() && InArgs[0] == "set")
    {
        const std::vector<std::string> SubArgs(InArgs.begin() + 1,
                                               InArgs.end());
        return RunChangelogSetCommand(SubArgs, InRepoRoot);
    }

    const FBundleChangelogOptions Options = ParseBundleChangelogOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
    if (Options.mbHuman)
        return RunBundleChangelogHuman(RepoRoot, Options);
    return RunBundleChangelogJson(RepoRoot, Options);
}

// ---------------------------------------------------------------------------
// Verification — JSON
// ---------------------------------------------------------------------------

static int
RunBundleVerificationJson(const fs::path &InRepoRoot,
                          const FBundleVerificationOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const int PhaseFilter = InOptions.mbHasScopeFilter
                                ? std::atoi(InOptions.mScopeFilter.c_str())
                                : -2;
    std::vector<const FVerificationEntry *> Filtered;
    for (const FVerificationEntry &Entry : Bundle.mVerifications)
    {
        if (PhaseFilter != -2 && Entry.mPhase != PhaseFilter)
            continue;
        Filtered.push_back(&Entry);
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kVerificationSchemaV2, UTC, InRepoRoot.string());
    EmitJsonField("topic", Bundle.mTopicKey);
    if (InOptions.mbHasScopeFilter)
        EmitJsonFieldInt("phase_filter", PhaseFilter);
    EmitJsonFieldSizeT("count", Filtered.size());
    std::cout << "\"entries\":[";
    for (size_t I = 0; I < Filtered.size(); ++I)
    {
        const FVerificationEntry &Entry = *Filtered[I];
        PrintJsonSep(I);
        std::cout << "{";
        if (Entry.mPhase < 0)
            std::cout << "\"phase\":null,";
        else
            EmitJsonFieldInt("phase", Entry.mPhase);
        EmitJsonField("date", Entry.mDate);
        EmitJsonField("check", Entry.mCheck);
        EmitJsonFieldNullable("result", Entry.mResult);
        EmitJsonFieldNullable("detail", Entry.mDetail, false);
        std::cout << "}";
    }
    std::cout << "],";
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Verification — Human
// ---------------------------------------------------------------------------

static int
RunBundleVerificationHuman(const fs::path &InRepoRoot,
                           const FBundleVerificationOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const int VPhaseFilter = InOptions.mbHasScopeFilter
                                 ? std::atoi(InOptions.mScopeFilter.c_str())
                                 : -2;
    std::vector<const FVerificationEntry *> Filtered;
    for (const FVerificationEntry &Entry : Bundle.mVerifications)
    {
        if (VPhaseFilter != -2 && Entry.mPhase != VPhaseFilter)
            continue;
        Filtered.push_back(&Entry);
    }

    std::cout << kColorBold << "Verification" << kColorReset
              << " topic=" << kColorOrange << Bundle.mTopicKey << kColorReset
              << " count=" << Filtered.size() << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Phase", "Date", "Check", "Result", "Detail"};
    for (const FVerificationEntry *rpEntry : Filtered)
    {
        std::string Check = rpEntry->mCheck;
        if (Check.size() > 60)
            Check = Check.substr(0, 57) + "...";
        std::string Detail = rpEntry->mDetail;
        if (Detail.size() > 60)
            Detail = Detail.substr(0, 57) + "...";
        const std::string PhaseDisplay =
            rpEntry->mPhase < 0 ? "(topic)" : std::to_string(rpEntry->mPhase);
        Table.AddRow({PhaseDisplay, rpEntry->mDate,
                      kColorDim + Check + kColorReset, rpEntry->mResult,
                      Detail});
    }
    Table.Print();
    return 0;
}

// ---------------------------------------------------------------------------
// RunBundleVerificationCommand
// ---------------------------------------------------------------------------

int RunBundleVerificationCommand(const std::vector<std::string> &InArgs,
                                 const std::string &InRepoRoot)
{
    // Check for "add" subcommand
    if (!InArgs.empty() && InArgs[0] == "add")
    {
        const std::vector<std::string> SubArgs(InArgs.begin() + 1,
                                               InArgs.end());
        return RunVerificationAddCommand(SubArgs, InRepoRoot);
    }
    if (!InArgs.empty() && InArgs[0] == "set")
    {
        const std::vector<std::string> SubArgs(InArgs.begin() + 1,
                                               InArgs.end());
        return RunVerificationSetCommand(SubArgs, InRepoRoot);
    }

    const FBundleVerificationOptions Options =
        ParseBundleVerificationOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
    if (Options.mbHuman)
        return RunBundleVerificationHuman(RepoRoot, Options);
    return RunBundleVerificationJson(RepoRoot, Options);
}

// ---------------------------------------------------------------------------
// Timeline — JSON (changelogs + verifications sorted by date)
// ---------------------------------------------------------------------------

static int RunBundleTimelineJson(const fs::path &InRepoRoot,
                                 const FBundleTimelineOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // Merge changelogs and verifications into timeline entries
    struct FTimelineEntry
    {
        std::string mDate;
        std::string mKind; // "changelog" or "verification"
        int mPhase;        // phase index or -1 for topic-level
        std::string mText;
        std::string mType;
    };

    std::vector<FTimelineEntry> Entries;
    for (const FChangeLogEntry &CL : Bundle.mChangeLogs)
    {
        if (!InOptions.mSince.empty() && CL.mDate < InOptions.mSince)
            continue;
        if (InOptions.mbHasPhaseFilter && CL.mPhase != InOptions.mPhaseFilter)
            continue;
        Entries.push_back(
            {CL.mDate, "changelog", CL.mPhase, CL.mChange, ToString(CL.mType)});
    }
    for (const FVerificationEntry &VE : Bundle.mVerifications)
    {
        if (!InOptions.mSince.empty() && VE.mDate < InOptions.mSince)
            continue;
        if (InOptions.mbHasPhaseFilter && VE.mPhase != InOptions.mPhaseFilter)
            continue;
        Entries.push_back({VE.mDate, "verification", VE.mPhase, VE.mCheck, ""});
    }

    // Sort by date descending (newest first)
    std::sort(Entries.begin(), Entries.end(),
              [](const FTimelineEntry &A, const FTimelineEntry &B)
              { return A.mDate > B.mDate; });

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kTimelineSchema, UTC, InRepoRoot.string());
    EmitJsonField("topic", Bundle.mTopicKey);
    if (!InOptions.mSince.empty())
        EmitJsonField("since", InOptions.mSince);
    if (InOptions.mbHasPhaseFilter)
        EmitJsonFieldInt("phase_filter", InOptions.mPhaseFilter);
    EmitJsonFieldSizeT("count", Entries.size());
    std::cout << "\"entries\":[";
    for (size_t I = 0; I < Entries.size(); ++I)
    {
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonField("date", Entries[I].mDate);
        EmitJsonField("kind", Entries[I].mKind);
        if (Entries[I].mPhase < 0)
            std::cout << "\"phase\":null,";
        else
            EmitJsonFieldInt("phase", Entries[I].mPhase);
        EmitJsonField("phase_label",
                      ResolvePhaseLabel(Entries[I].mPhase, Bundle.mPhases));
        EmitJsonFieldNullable("text", Entries[I].mText);
        EmitJsonFieldNullable("type", Entries[I].mType, false);
        std::cout << "}";
    }
    std::cout << "],";
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Timeline — Human
// ---------------------------------------------------------------------------

static int RunBundleTimelineHuman(const fs::path &InRepoRoot,
                                  const FBundleTimelineOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    struct FTimelineEntry
    {
        std::string mDate;
        std::string mKind;
        int mPhase; // phase index or -1 for topic-level
        std::string mType;
        std::string mText;
    };

    std::vector<FTimelineEntry> Entries;
    for (const FChangeLogEntry &CL : Bundle.mChangeLogs)
    {
        if (!InOptions.mSince.empty() && CL.mDate < InOptions.mSince)
            continue;
        if (InOptions.mbHasPhaseFilter && CL.mPhase != InOptions.mPhaseFilter)
            continue;
        Entries.push_back(
            {CL.mDate, "changelog", CL.mPhase, ToString(CL.mType), CL.mChange});
    }
    for (const FVerificationEntry &VE : Bundle.mVerifications)
    {
        if (!InOptions.mSince.empty() && VE.mDate < InOptions.mSince)
            continue;
        if (InOptions.mbHasPhaseFilter && VE.mPhase != InOptions.mPhaseFilter)
            continue;
        Entries.push_back(
            {VE.mDate, "verification", VE.mPhase, VE.mResult, VE.mCheck});
    }
    std::sort(Entries.begin(), Entries.end(),
              [](const FTimelineEntry &A, const FTimelineEntry &B)
              { return A.mDate > B.mDate; });

    std::cout << kColorBold << "Timeline" << kColorReset
              << " topic=" << kColorOrange << Bundle.mTopicKey << kColorReset
              << " count=" << Entries.size() << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Date", "Kind", "Phase", "Type/Result", "Text"};
    for (const FTimelineEntry &E : Entries)
    {
        std::string Text = E.mText;
        if (Text.size() > 60)
            Text = Text.substr(0, 57) + "...";
        const std::string PhaseDisplay =
            E.mPhase < 0 ? "(topic)" : std::to_string(E.mPhase);
        Table.AddRow({E.mDate, E.mKind, PhaseDisplay, E.mType,
                      kColorDim + Text + kColorReset});
    }
    Table.Print();
    return 0;
}

// ---------------------------------------------------------------------------
// RunBundleTimelineCommand
// ---------------------------------------------------------------------------

int RunBundleTimelineCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot)
{
    const FBundleTimelineOptions Options = ParseBundleTimelineOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
    if (Options.mbHuman)
        return RunBundleTimelineHuman(RepoRoot, Options);
    return RunBundleTimelineJson(RepoRoot, Options);
}

// ---------------------------------------------------------------------------
// Blockers — JSON
// ---------------------------------------------------------------------------

static int RunBundleBlockersJson(const fs::path &InRepoRoot,
                                 const FBundleBlockersOptions &InOptions)
{
    std::vector<std::string> Warnings;
    std::vector<FTopicBundle> Bundles;

    if (!InOptions.mTopic.empty())
    {
        FTopicBundle Bundle;
        std::string Error;
        if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
        {
            std::cerr << Error << "\n";
            return 1;
        }
        Bundles.push_back(std::move(Bundle));
    }
    else
    {
        Bundles = LoadAllBundles(InRepoRoot, Warnings);
    }

    struct FBlockerEntry
    {
        std::string mTopic;
        size_t mPhaseIndex;
        std::string mScope;
        std::string mBlockers;
    };

    auto HasRealBlocker = [](const std::string &InText) -> bool
    {
        if (InText.empty())
            return false;
        std::string Lower;
        for (char C : InText)
            Lower +=
                static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
        return Lower != "none" && Lower != "none." && Lower != "n/a" &&
               Lower != "-";
    };

    std::vector<FBlockerEntry> BlockerEntries;
    for (const FTopicBundle &Bundle : Bundles)
    {
        for (size_t I = 0; I < Bundle.mPhases.size(); ++I)
        {
            const FPhaseRecord &Phase = Bundle.mPhases[I];
            if (Phase.mLifecycle.mStatus == EExecutionStatus::Blocked ||
                HasRealBlocker(Phase.mLifecycle.mBlockers))
            {
                BlockerEntries.push_back({Bundle.mTopicKey, I, Phase.mScope,
                                          Phase.mLifecycle.mBlockers});
            }
        }
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kBlockersSchema, UTC, InRepoRoot.string());
    EmitJsonFieldSizeT("count", BlockerEntries.size());
    std::cout << "\"blockers\":[";
    for (size_t I = 0; I < BlockerEntries.size(); ++I)
    {
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonField("topic", BlockerEntries[I].mTopic);
        EmitJsonFieldSizeT("phase_index", BlockerEntries[I].mPhaseIndex);
        EmitJsonField("scope", BlockerEntries[I].mScope);
        EmitJsonField("blockers", BlockerEntries[I].mBlockers, false);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Blockers — Human
// ---------------------------------------------------------------------------

static int RunBundleBlockersHuman(const fs::path &InRepoRoot,
                                  const FBundleBlockersOptions &InOptions)
{
    std::vector<std::string> Warnings;
    std::vector<FTopicBundle> Bundles;

    if (!InOptions.mTopic.empty())
    {
        FTopicBundle Bundle;
        std::string Error;
        if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
        {
            std::cerr << Error << "\n";
            return 1;
        }
        Bundles.push_back(std::move(Bundle));
    }
    else
    {
        Bundles = LoadAllBundles(InRepoRoot, Warnings);
    }

    struct FBlockerEntry
    {
        std::string mTopic;
        size_t mPhaseIndex;
        std::string mScope;
        std::string mBlockers;
    };

    auto HasRealBlocker = [](const std::string &InText) -> bool
    {
        if (InText.empty())
            return false;
        std::string Lower;
        for (char C : InText)
            Lower +=
                static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
        return Lower != "none" && Lower != "none." && Lower != "n/a" &&
               Lower != "-";
    };

    std::vector<FBlockerEntry> BlockerEntries;
    for (const FTopicBundle &Bundle : Bundles)
    {
        for (size_t I = 0; I < Bundle.mPhases.size(); ++I)
        {
            const FPhaseRecord &Phase = Bundle.mPhases[I];
            if (Phase.mLifecycle.mStatus == EExecutionStatus::Blocked ||
                HasRealBlocker(Phase.mLifecycle.mBlockers))
            {
                BlockerEntries.push_back({Bundle.mTopicKey, I, Phase.mScope,
                                          Phase.mLifecycle.mBlockers});
            }
        }
    }

    std::cout << kColorBold << "Blockers" << kColorReset
              << " count=" << BlockerEntries.size() << "\n\n";

    if (BlockerEntries.empty())
    {
        std::cout << kColorDim << "No blocked phases." << kColorReset << "\n";
        return 0;
    }

    HumanTable Table;
    Table.mHeaders = {"Topic", "Phase", "Scope", "Blockers"};
    for (const FBlockerEntry &E : BlockerEntries)
    {
        std::string Scope = E.mScope;
        if (Scope.size() > 40)
            Scope = Scope.substr(0, 37) + "...";
        Table.AddRow(
            {E.mTopic, std::to_string(E.mPhaseIndex), Scope, E.mBlockers});
    }
    Table.Print();
    return 0;
}

// ---------------------------------------------------------------------------
// RunBundleBlockersCommand
// ---------------------------------------------------------------------------

int RunBundleBlockersCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot)
{
    const FBundleBlockersOptions Options = ParseBundleBlockersOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
    if (Options.mbHuman)
        return RunBundleBlockersHuman(RepoRoot, Options);
    return RunBundleBlockersJson(RepoRoot, Options);
}

// ===================================================================
// Batch 5: validate
// ===================================================================

// ---------------------------------------------------------------------------
// Validate — validates .Plan.json against schema constraints
// ---------------------------------------------------------------------------

// Resolve mLine for every ValidateCheck by consulting the raw JSON text of
// the owning bundle. Each bundle's text is scanned at most once.
static void ResolveIssueLines(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &InOutChecks)
{
    std::unordered_map<std::string, FJsonLineIndex> IndexByTopic;
    const auto GetIndex =
        [&](const std::string &InTopic) -> const FJsonLineIndex *
    {
        auto It = IndexByTopic.find(InTopic);
        if (It != IndexByTopic.end())
            return &It->second;
        for (const FTopicBundle &B : InBundles)
        {
            if (B.mTopicKey != InTopic)
                continue;
            if (B.mBundlePath.empty())
                break;
            std::ifstream Stream(B.mBundlePath);
            if (!Stream)
                break;
            const std::string Text((std::istreambuf_iterator<char>(Stream)),
                                   std::istreambuf_iterator<char>());
            FJsonLineIndex Index;
            Index.Build(Text);
            It = IndexByTopic.emplace(InTopic, std::move(Index)).first;
            return &It->second;
        }
        return nullptr;
    };

    for (ValidateCheck &C : InOutChecks)
    {
        if (C.mLine >= 0)
            continue;
        if (C.mTopic.empty() || C.mPath.empty())
            continue;
        const FJsonLineIndex *rpIndex = GetIndex(C.mTopic);
        if (rpIndex == nullptr)
            continue;
        C.mLine = rpIndex->LineFor(C.mPath);
    }
}

static int RunBundleValidateJson(const fs::path &InRepoRoot,
                                 const FBundleValidateOptions &InOptions)
{
    std::vector<std::string> BundleWarnings;
    std::vector<FTopicBundle> Bundles;
    if (!InOptions.mTopic.empty())
    {
        FTopicBundle Bundle;
        std::string Error;
        if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
        {
            const std::string UTC = GetUtcNow();
            PrintJsonHeader(kValidateSchema, UTC, InRepoRoot.string());
            EmitJsonField("topic", InOptions.mTopic);
            EmitJsonFieldBool("valid", false);
            std::cout << "\"issues\":[{";
            EmitJsonField("id", "load_failure");
            EmitJsonField("severity", "error_major");
            EmitJsonFieldBool("ok", false);
            EmitJsonField("detail", Error, false);
            std::cout << "}],";
            PrintJsonClose(BundleWarnings);
            return 1;
        }
        Bundles.push_back(std::move(Bundle));
    }
    else
    {
        Bundles = LoadAllBundles(InRepoRoot, BundleWarnings);
    }

    std::vector<ValidateCheck> Checks = ValidateAllBundles(Bundles);
    ResolveIssueLines(Bundles, Checks);

    int ErrorMajorCount = 0;
    int ErrorMinorCount = 0;
    int WarningCount = 0;
    bool bValid = true;
    for (const ValidateCheck &C : Checks)
    {
        if (!C.mbOk)
        {
            if (C.mSeverity == EValidationSeverity::ErrorMajor)
            {
                ErrorMajorCount++;
                bValid = false;
            }
            else if (C.mSeverity == EValidationSeverity::ErrorMinor)
                ErrorMinorCount++;
            else
                WarningCount++;
        }
    }

    // --strict promotes ErrorMinor and Warning into the bValid gate.
    if (InOptions.mbStrict && (ErrorMinorCount > 0 || WarningCount > 0))
        bValid = false;

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kValidateSchema, UTC, InRepoRoot.string());
    if (!InOptions.mTopic.empty())
        EmitJsonField("topic", InOptions.mTopic);
    EmitJsonFieldSizeT("bundle_count", Bundles.size());
    EmitJsonFieldBool("valid", bValid);
    EmitJsonFieldInt("error_major", ErrorMajorCount);
    EmitJsonFieldInt("error_minor", ErrorMinorCount);
    EmitJsonFieldInt("warnings", WarningCount);

    // Only emit failures
    std::cout << "\"issues\":[";
    bool bFirst = true;
    for (const ValidateCheck &C : Checks)
    {
        if (C.mbOk)
            continue;
        if (!bFirst)
            std::cout << ",";
        bFirst = false;
        std::cout << "{";
        EmitJsonField("id", C.mID);
        EmitJsonField("severity", ToString(C.mSeverity));
        EmitJsonFieldNullable("topic", C.mTopic);
        EmitJsonFieldNullable("path", C.mPath);
        if (C.mLine > 0)
            EmitJsonFieldInt("line", C.mLine);
        else
            std::cout << "\"line\":null,";
        EmitJsonField("detail", C.mDetail, false);
        std::cout << "}";
    }
    std::cout << "],";
    PrintJsonClose(BundleWarnings);
    return bValid ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Validate — Human
// ---------------------------------------------------------------------------

static int RunBundleValidateHuman(const fs::path &InRepoRoot,
                                  const FBundleValidateOptions &InOptions)
{
    std::vector<std::string> BundleWarnings;
    std::vector<FTopicBundle> Bundles;
    if (!InOptions.mTopic.empty())
    {
        FTopicBundle Bundle;
        std::string Error;
        if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
        {
            std::cerr << kColorRed << "FAIL" << kColorReset << " "
                      << InOptions.mTopic << ": " << Error << "\n";
            return 1;
        }
        Bundles.push_back(std::move(Bundle));
    }
    else
    {
        Bundles = LoadAllBundles(InRepoRoot, BundleWarnings);
    }

    std::vector<ValidateCheck> Checks = ValidateAllBundles(Bundles);
    ResolveIssueLines(Bundles, Checks);

    int ErrorMajorCount = 0;
    int ErrorMinorCount = 0;
    int WarningCount = 0;
    for (const ValidateCheck &C : Checks)
    {
        if (!C.mbOk)
        {
            if (C.mSeverity == EValidationSeverity::ErrorMajor)
                ErrorMajorCount++;
            else if (C.mSeverity == EValidationSeverity::ErrorMinor)
                ErrorMinorCount++;
            else
                WarningCount++;
        }
    }

    const int TotalFailed = ErrorMajorCount + ErrorMinorCount + WarningCount;
    std::cout << kColorBold << "Validate" << kColorReset << "  "
              << Bundles.size() << " bundles";
    if (TotalFailed == 0)
    {
        std::cout << "  " << kColorGreen << "PASS" << kColorReset << "\n";
    }
    else
    {
        std::cout << "  " << kColorRed << TotalFailed << " issues"
                  << kColorReset << " (";
        if (ErrorMajorCount > 0)
            std::cout << kColorRed << ErrorMajorCount << " major"
                      << kColorReset;
        if (ErrorMinorCount > 0)
        {
            if (ErrorMajorCount > 0)
                std::cout << ", ";
            std::cout << kColorYellow << ErrorMinorCount << " minor"
                      << kColorReset;
        }
        if (WarningCount > 0)
        {
            if (ErrorMajorCount > 0 || ErrorMinorCount > 0)
                std::cout << ", ";
            std::cout << kColorDim << WarningCount << " warning" << kColorReset;
        }
        std::cout << ")\n";
    }
    std::cout << "\n";

    // Only show failures
    if (TotalFailed > 0)
    {
        HumanTable Table;
        Table.mHeaders = {"Severity", "Topic", "Line", "Path", "Detail"};
        for (const ValidateCheck &C : Checks)
        {
            if (C.mbOk)
                continue;
            std::string Sev;
            if (C.mSeverity == EValidationSeverity::ErrorMajor)
                Sev = kColorRed + std::string("ERROR") + kColorReset;
            else if (C.mSeverity == EValidationSeverity::ErrorMinor)
                Sev = kColorYellow + std::string("error") + kColorReset;
            else
                Sev = kColorDim + std::string("warn") + kColorReset;

            std::string Detail = C.mDetail;
            if (Detail.size() > 50)
                Detail = Detail.substr(0, 47) + "...";
            std::string Path = C.mPath;
            if (Path.size() > 35)
                Path = Path.substr(0, 32) + "...";
            const std::string LineCell =
                C.mLine > 0 ? std::to_string(C.mLine) : "-";
            Table.AddRow({Sev, C.mTopic, LineCell,
                          kColorDim + Path + kColorReset, Detail});
        }
        Table.Print();
    }

    const bool bStrictFail =
        InOptions.mbStrict && (ErrorMinorCount > 0 || WarningCount > 0);
    return (ErrorMajorCount > 0 || bStrictFail) ? 1 : 0;
}

// ---------------------------------------------------------------------------
// RunBundleValidateCommand
// ---------------------------------------------------------------------------

int RunBundleValidateCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot)
{
    const FBundleValidateOptions Options = ParseBundleValidateOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
    if (Options.mbHuman)
        return RunBundleValidateHuman(RepoRoot, Options);
    return RunBundleValidateJson(RepoRoot, Options);
}

// ===================================================================
// Mutation commands
// ===================================================================

// Shared: emit mutation JSON response
static void EmitMutationJson(
    const std::string &InTopic, const std::string &InTarget,
    const std::vector<
        std::pair<std::string, std::pair<std::string, std::string>>> &InChanges,
    bool InAutoChangelog)
{
    std::cout << "{\"schema\":" << JsonQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", InTopic);
    EmitJsonField("target", InTarget);
    std::cout << "\"changes\":[";
    for (size_t I = 0; I < InChanges.size(); ++I)
    {
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonField("field", InChanges[I].first);
        EmitJsonField("old", InChanges[I].second.first);
        EmitJsonField("new", InChanges[I].second.second, false);
        std::cout << "}";
    }
    std::cout << "],";
    EmitJsonFieldBool("auto_changelog", InAutoChangelog, false);
    std::cout << "}\n";
}

// Shared: auto-append a changelog entry for a mutation
static void AppendAutoChangelog(FTopicBundle &InOutBundle,
                                const std::string &InTarget,
                                const std::string &InDescription)
{
    FChangeLogEntry Entry;
    // Extract phase index only if target is phase-scoped (matches canonical
    // "phases[N]..." form). Top-level targets like "verifications[N]",
    // "changelogs[N]", or "plan" have no phase ref (-1).
    static constexpr const char *kPhasePrefix = "phases[";
    static constexpr size_t kPhasePrefixLen = 7;
    if (InTarget.compare(0, kPhasePrefixLen, kPhasePrefix) == 0)
    {
        const size_t Close = InTarget.find(']', kPhasePrefixLen);
        if (Close != std::string::npos)
            Entry.mPhase = std::atoi(
                InTarget.substr(kPhasePrefixLen, Close - kPhasePrefixLen)
                    .c_str());
        else
            Entry.mPhase = -1;
    }
    else
    {
        Entry.mPhase = -1;
    }
    // Date = today UTC
    const std::string UTC = GetUtcNow();
    Entry.mDate = UTC.substr(0, 10); // YYYY-MM-DD
    Entry.mChange = InDescription;
    Entry.mAffected = InTarget;
    Entry.mType = EChangeType::Chore;
    Entry.mActor = ETestingActor::AI;
    InOutBundle.mChangeLogs.push_back(std::move(Entry));
}

// Shared: write bundle back to its source path
static int WriteBundleBack(const FTopicBundle &InBundle,
                           const fs::path &InRepoRoot, std::string &OutError)
{
    if (InBundle.mBundlePath.empty())
    {
        OutError = "Bundle has no source path (was it loaded via "
                   "TryLoadBundleByTopic?)";
        return 1;
    }
    return TryWriteTopicBundle(InBundle, InBundle.mBundlePath, OutError) ? 0
                                                                         : 1;
}

// ---------------------------------------------------------------------------
// topic set
// ---------------------------------------------------------------------------

int RunTopicSetCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot)
{
    const FTopicSetOptions Options = ParseTopicSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;
    std::string Desc;

    if (!Options.mStatus.empty())
    {
        ETopicStatus NewTopicStatus;
        if (!TopicStatusFromString(Options.mStatus, NewTopicStatus))
        {
            std::cerr << "Invalid topic status: " << Options.mStatus << "\n";
            return 1;
        }
        Changes.push_back(
            {"status", {ToString(Bundle.mStatus), Options.mStatus}});
        Desc = "Status → " + Options.mStatus;
        Bundle.mStatus = NewTopicStatus;
    }
    if (!Options.mNextActions.empty())
    {
        Changes.push_back(
            {"next_actions", {Bundle.mNextActions, Options.mNextActions}});
        if (Desc.empty())
            Desc = "Updated next_actions";
        Bundle.mNextActions = Options.mNextActions;
    }

    // Metadata fields — apply each non-empty option to Bundle.mMetadata
    auto ApplyMeta = [&](const std::string &InFlag, std::string &InOutField,
                         const std::string &InNewValue)
    {
        if (!InNewValue.empty())
        {
            Changes.push_back({InFlag, {InOutField, InNewValue}});
            InOutField = InNewValue;
            if (Desc.empty())
                Desc = "Updated " + InFlag;
        }
    };
    ApplyMeta("summary", Bundle.mMetadata.mSummary, Options.mSummary);
    ApplyMeta("goals", Bundle.mMetadata.mGoals, Options.mGoals);
    ApplyMeta("non_goals", Bundle.mMetadata.mNonGoals, Options.mNonGoals);
    ApplyMeta("risks", Bundle.mMetadata.mRisks, Options.mRisks);
    ApplyMeta("acceptance_criteria", Bundle.mMetadata.mAcceptanceCriteria,
              Options.mAcceptanceCriteria);
    ApplyMeta("problem_statement", Bundle.mMetadata.mProblemStatement,
              Options.mProblemStatement);

    // validation_commands (typed vector) — --validation-clear empties the
    // existing set, --validation-add appends new entries. Either or both
    // triggers a change record.
    if (Options.mbValidationClear || !Options.mValidationAdd.empty())
    {
        std::string OldDesc =
            std::to_string(Bundle.mMetadata.mValidationCommands.size()) +
            " entries";
        if (Options.mbValidationClear)
            Bundle.mMetadata.mValidationCommands.clear();
        for (const FValidationCommand &C : Options.mValidationAdd)
            Bundle.mMetadata.mValidationCommands.push_back(C);
        std::string NewDesc =
            std::to_string(Bundle.mMetadata.mValidationCommands.size()) +
            " entries";
        Changes.push_back({"validation_commands", {OldDesc, NewDesc}});
        if (Desc.empty())
            Desc = "Updated validation_commands";
    }
    ApplyMeta("baseline_audit", Bundle.mMetadata.mBaselineAudit,
              Options.mBaselineAudit);
    ApplyMeta("execution_strategy", Bundle.mMetadata.mExecutionStrategy,
              Options.mExecutionStrategy);
    ApplyMeta("locked_decisions", Bundle.mMetadata.mLockedDecisions,
              Options.mLockedDecisions);
    ApplyMeta("source_references", Bundle.mMetadata.mSourceReferences,
              Options.mSourceReferences);

    // dependencies (typed vector) — mirrors validation_commands semantics.
    if (Options.mbDependencyClear || !Options.mDependencyAdd.empty())
    {
        const std::string OldDesc =
            std::to_string(Bundle.mMetadata.mDependencies.size()) + " entries";
        if (Options.mbDependencyClear)
            Bundle.mMetadata.mDependencies.clear();
        for (const FBundleReference &R : Options.mDependencyAdd)
            Bundle.mMetadata.mDependencies.push_back(R);
        const std::string NewDesc =
            std::to_string(Bundle.mMetadata.mDependencies.size()) + " entries";
        Changes.push_back({"dependencies", {OldDesc, NewDesc}});
        if (Desc.empty())
            Desc = "Updated dependencies";
    }

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, kTargetPlan, Desc);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, "plan", Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// phase set
// ---------------------------------------------------------------------------

int RunPhaseSetCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot)
{
    const FPhaseSetOptions Options = ParsePhaseSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];
    const std::string Target = MakePhaseTarget(Options.mPhaseIndex);

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;
    std::string Desc;

    if (!Options.mStatus.empty())
    {
        const std::string Old = ToString(Phase.mLifecycle.mStatus);
        EExecutionStatus NewStatus;
        if (!ExecutionStatusFromString(Options.mStatus, NewStatus))
        {
            std::cerr << "Invalid status: " << Options.mStatus << "\n";
            return 1;
        }
        Changes.push_back({"status", {Old, Options.mStatus}});
        Phase.mLifecycle.mStatus = NewStatus;
        Desc = Target + " → " + Options.mStatus;

        // Auto-set timestamps
        const std::string UTC = GetUtcNow();
        if (NewStatus == EExecutionStatus::InProgress &&
            Phase.mLifecycle.mStartedAt.empty())
        {
            Phase.mLifecycle.mStartedAt = UTC;
            Changes.push_back({"started_at", {"", UTC}});
        }
        if (NewStatus == EExecutionStatus::Completed &&
            Phase.mLifecycle.mCompletedAt.empty())
        {
            Phase.mLifecycle.mCompletedAt = UTC;
            Changes.push_back({"completed_at", {"", UTC}});
        }
    }
    if (!Options.mDone.empty())
    {
        Changes.push_back({"done", {Phase.mLifecycle.mDone, Options.mDone}});
        Phase.mLifecycle.mDone = Options.mDone;
        if (Desc.empty())
            Desc = Target + " updated done";
    }
    if (!Options.mRemaining.empty())
    {
        Changes.push_back(
            {"remaining", {Phase.mLifecycle.mRemaining, Options.mRemaining}});
        Phase.mLifecycle.mRemaining = Options.mRemaining;
    }
    if (!Options.mBlockers.empty())
    {
        Changes.push_back(
            {"blockers", {Phase.mLifecycle.mBlockers, Options.mBlockers}});
        Phase.mLifecycle.mBlockers = Options.mBlockers;
    }
    if (!Options.mContext.empty())
    {
        Changes.push_back({"agent_context",
                           {Phase.mLifecycle.mAgentContext, Options.mContext}});
        Phase.mLifecycle.mAgentContext = Options.mContext;
    }

    // Phase-level fields
    auto ApplyPhase = [&](const std::string &InFlag, std::string &InOutField,
                          const std::string &InNewValue)
    {
        if (!InNewValue.empty())
        {
            Changes.push_back({InFlag, {InOutField, InNewValue}});
            InOutField = InNewValue;
            if (Desc.empty())
                Desc = Target + " updated " + InFlag;
        }
    };
    ApplyPhase("scope", Phase.mScope, Options.mScope);
    ApplyPhase("output", Phase.mOutput, Options.mOutput);

    // Design material fields
    ApplyPhase("investigation", Phase.mDesign.mInvestigation,
               Options.mInvestigation);
    ApplyPhase("code_entity_contract", Phase.mDesign.mCodeEntityContract,
               Options.mCodeEntityContract);
    ApplyPhase("code_snippets", Phase.mDesign.mCodeSnippets,
               Options.mCodeSnippets);
    ApplyPhase("best_practices", Phase.mDesign.mBestPractices,
               Options.mBestPractices);
    ApplyPhase("multi_platforming", Phase.mDesign.mMultiPlatforming,
               Options.mMultiPlatforming);
    ApplyPhase("readiness_gate", Phase.mDesign.mReadinessGate,
               Options.mReadinessGate);
    ApplyPhase("handoff", Phase.mDesign.mHandoff, Options.mHandoff);

    // validation_commands (typed vector) — see FTopicSetCommand above for
    // the --validation-clear + --validation-add semantics.
    if (Options.mbValidationClear || !Options.mValidationAdd.empty())
    {
        std::string OldDesc =
            std::to_string(Phase.mDesign.mValidationCommands.size()) +
            " entries";
        if (Options.mbValidationClear)
            Phase.mDesign.mValidationCommands.clear();
        for (const FValidationCommand &C : Options.mValidationAdd)
            Phase.mDesign.mValidationCommands.push_back(C);
        std::string NewDesc =
            std::to_string(Phase.mDesign.mValidationCommands.size()) +
            " entries";
        Changes.push_back({"validation_commands", {OldDesc, NewDesc}});
        if (Desc.empty())
            Desc = "Updated validation_commands";
    }
    // dependencies (typed vector) — see FTopicSetCommand above for semantics.
    if (Options.mbDependencyClear || !Options.mDependencyAdd.empty())
    {
        const std::string OldDesc =
            std::to_string(Phase.mDesign.mDependencies.size()) + " entries";
        if (Options.mbDependencyClear)
            Phase.mDesign.mDependencies.clear();
        for (const FBundleReference &R : Options.mDependencyAdd)
            Phase.mDesign.mDependencies.push_back(R);
        const std::string NewDesc =
            std::to_string(Phase.mDesign.mDependencies.size()) + " entries";
        Changes.push_back({"dependencies", {OldDesc, NewDesc}});
        if (Desc.empty())
            Desc = Target + " updated dependencies";
    }

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    // Build the changelog message from the full field list so repeated
    // set calls with different field subsets produce distinct entries.
    // Previously only the last-changed field ended up in Desc, which
    // caused no_duplicate_changelog warnings whenever the same single
    // field was set twice.
    std::string FieldList;
    for (const auto &C : Changes)
    {
        if (!FieldList.empty())
            FieldList += ", ";
        FieldList += C.first;
    }
    AppendAutoChangelog(Bundle, Target, Target + " updated: " + FieldList);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// phase remove — delete the trailing phase of a topic
//
// Safety gates:
//   - phase index must refer to the last phase of the topic (no index-shift
//     semantics; callers who want to remove an intermediate phase must
//     reorder first)
//   - phase.status must be not_started (refuse in_progress / completed /
//     blocked — removing executed work would falsify history)
//   - no changelog or verification entry may reference the phase (those
//     entries would otherwise be orphaned)
//
// Emits an auto-changelog entry describing the removal. Does not cascade
// into jobs/lanes/tasks — those are embedded in the phase record being
// erased and disappear with it.
// ---------------------------------------------------------------------------

int RunPhaseRemoveCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot)
{
    const FPhaseGetOptions Options = ParsePhaseGetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (Options.mPhaseIndex < 0 ||
        static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    const size_t Idx = static_cast<size_t>(Options.mPhaseIndex);
    const size_t Last = Bundle.mPhases.size() - 1;
    if (Idx != Last)
    {
        std::cerr << "phase remove only supports the trailing phase ("
                  << "phases[" << Last << "]); got phases[" << Idx << "]\n";
        return 1;
    }

    const FPhaseRecord &Phase = Bundle.mPhases[Idx];
    if (Phase.mLifecycle.mStatus != EExecutionStatus::NotStarted)
    {
        std::cerr << "phase remove refuses status="
                  << ToString(Phase.mLifecycle.mStatus)
                  << "; only not_started phases may be removed\n";
        return 1;
    }

    for (const FChangeLogEntry &C : Bundle.mChangeLogs)
    {
        if (C.mPhase == static_cast<int>(Idx))
        {
            std::cerr << "phase remove refused: changelogs[] references "
                      << "phases[" << Idx << "]\n";
            return 1;
        }
    }
    for (const FVerificationEntry &V : Bundle.mVerifications)
    {
        if (V.mPhase == static_cast<int>(Idx))
        {
            std::cerr << "phase remove refused: verifications[] references "
                      << "phases[" << Idx << "]\n";
            return 1;
        }
    }

    const std::string OldScope = Phase.mScope;
    Bundle.mPhases.pop_back();

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;
    Changes.push_back(
        {"phases[" + std::to_string(Idx) + "]", {OldScope, "(removed)"}});

    const std::string Target = MakePhaseTarget(static_cast<int>(Idx));
    const std::string Desc =
        "Removed " + Target + (OldScope.empty() ? "" : ": " + OldScope);
    // File removal changelog against the topic, not the now-gone phase
    // index — otherwise AppendAutoChangelog sets mPhase to Idx and leaves
    // a dangling changelogs[*].phase reference flagged by
    // changelog_phase_ref.
    AppendAutoChangelog(Bundle, kTargetPlan, Desc);

    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// phase add — append a new trailing phase to a topic
//
// Creates a new phase at index phases.size() (trailing). Default status is
// not_started; --scope / --output / --status are optional bootstrap fields.
// Any other per-phase content is added afterwards via `phase set` or the
// corresponding lane/job/testing/manifest commands.
//
// Auto-changelog entry is filed topic-scoped ("Added phases[N]...") rather
// than phase-scoped — a phase that was just created has no prior history
// to correlate against, and topic-scoping keeps the new phase's own
// changelogs[] slice clean.
// ---------------------------------------------------------------------------

int RunPhaseAddCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot)
{
    const FPhaseAddOptions Options = ParsePhaseAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    EExecutionStatus NewStatus = EExecutionStatus::NotStarted;
    if (!Options.mStatus.empty() &&
        !ExecutionStatusFromString(Options.mStatus, NewStatus))
    {
        std::cerr << "Invalid status: " << Options.mStatus << "\n";
        return 1;
    }

    FPhaseRecord NewPhase;
    NewPhase.mScope = Options.mScope;
    NewPhase.mOutput = Options.mOutput;
    NewPhase.mLifecycle.mStatus = NewStatus;
    if (NewStatus == EExecutionStatus::InProgress)
        NewPhase.mLifecycle.mStartedAt = GetUtcNow();
    else if (NewStatus == EExecutionStatus::Completed)
    {
        const std::string UTC = GetUtcNow();
        NewPhase.mLifecycle.mStartedAt = UTC;
        NewPhase.mLifecycle.mCompletedAt = UTC;
    }

    const size_t NewIdx = Bundle.mPhases.size();
    Bundle.mPhases.push_back(std::move(NewPhase));

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;
    Changes.push_back(
        {"phases[" + std::to_string(NewIdx) + "]", {"(new)", Options.mScope}});

    const std::string Target = MakePhaseTarget(static_cast<int>(NewIdx));
    const std::string Desc =
        "Added " + Target +
        (Options.mScope.empty() ? "" : ": " + Options.mScope);
    // File topic-scoped: a just-created phase has no prior timeline, so
    // auto-changelog filed against kTargetPlan keeps phases[NewIdx] own
    // changelog slice empty until the first real mutation lands there.
    AppendAutoChangelog(Bundle, kTargetPlan, Desc);

    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// phase normalize
//
// Sweep every prose field on a phase and replace common Unicode format
// artifacts with ASCII equivalents (em/en/figure dash + horizontal bar ->
// "-", smart quotes -> straight quotes, NBSP -> regular space). Fields
// covered: top-level scope/output, lifecycle done/remaining/blockers,
// agent_context, and all design fields (investigation,
// code_entity_contract, code_snippets, best_practices,
// multi_platforming, readiness_gate, handoff). Also sweeps each
// jobs[*] scope/output/exit_criteria, lanes[*] scope/exit_criteria,
// testing[*] step/action/expected/evidence/session, and
// file_manifest[*] description.
//
// Closes the CLI gap that forced callers to reach for direct JSON
// edits when content with smart quotes or dashes tripped
// no_smart_quotes. All writes go through the typed mutation pipeline.
//
// --dry-run reports what would change without writing. Auto-changelog
// is filed phase-scoped with a "Normalized phases[N] prose (N
// replacements in N fields)" entry.
// ---------------------------------------------------------------------------

int RunPhaseNormalizeCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot)
{
    const FPhaseNormalizeOptions Options = ParsePhaseNormalizeOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase = Bundle.mPhases[Options.mPhaseIndex];

    size_t TotalReplacements = 0;
    size_t FieldsChanged = 0;
    auto Sweep = [&](std::string &InOutField)
    {
        if (Options.mbDryRun)
        {
            std::string Copy = InOutField;
            const size_t R = NormalizeSmartChars(Copy);
            if (R > 0)
            {
                ++FieldsChanged;
                TotalReplacements += R;
            }
            return;
        }
        const size_t R = NormalizeSmartChars(InOutField);
        if (R > 0)
        {
            ++FieldsChanged;
            TotalReplacements += R;
        }
    };

    // Top-level phase text fields
    Sweep(Phase.mScope);
    Sweep(Phase.mOutput);
    Sweep(Phase.mLifecycle.mDone);
    Sweep(Phase.mLifecycle.mRemaining);
    Sweep(Phase.mLifecycle.mBlockers);
    Sweep(Phase.mLifecycle.mAgentContext);
    // Design material fields
    Sweep(Phase.mDesign.mInvestigation);
    Sweep(Phase.mDesign.mCodeEntityContract);
    Sweep(Phase.mDesign.mCodeSnippets);
    Sweep(Phase.mDesign.mBestPractices);
    Sweep(Phase.mDesign.mMultiPlatforming);
    Sweep(Phase.mDesign.mReadinessGate);
    Sweep(Phase.mDesign.mHandoff);
    // Lanes
    for (FLaneRecord &Lane : Phase.mLanes)
    {
        Sweep(Lane.mScope);
        Sweep(Lane.mExitCriteria);
    }
    // Jobs + tasks
    for (FJobRecord &Job : Phase.mJobs)
    {
        Sweep(Job.mScope);
        Sweep(Job.mOutput);
        Sweep(Job.mExitCriteria);
        for (FTaskRecord &Task : Job.mTasks)
        {
            Sweep(Task.mDescription);
            Sweep(Task.mEvidence);
            Sweep(Task.mNotes);
        }
    }
    // Testing
    for (FTestingRecord &T : Phase.mTesting)
    {
        Sweep(T.mSession);
        Sweep(T.mStep);
        Sweep(T.mAction);
        Sweep(T.mExpected);
        Sweep(T.mEvidence);
    }
    // File manifest
    for (FFileManifestItem &FM : Phase.mFileManifest)
    {
        Sweep(FM.mDescription);
    }

    const std::string Target = MakePhaseTarget(Options.mPhaseIndex);
    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;
    Changes.push_back({"normalized",
                       {std::to_string(FieldsChanged) + " fields",
                        std::to_string(TotalReplacements) + " replacements"}});

    if (Options.mbDryRun || TotalReplacements == 0)
    {
        EmitMutationJson(Options.mTopic, Target, Changes, true);
        return 0;
    }

    const std::string Desc = "Normalized " + Target + " prose (" +
                             std::to_string(TotalReplacements) +
                             " replacements in " +
                             std::to_string(FieldsChanged) + " fields)";
    AppendAutoChangelog(Bundle, Target, Desc);

    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// job set
// ---------------------------------------------------------------------------

int RunJobSetCommand(const std::vector<std::string> &InArgs,
                     const std::string &InRepoRoot)
{
    const FJobSetOptions Options = ParseJobSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size() ||
        static_cast<size_t>(Options.mJobIndex) >=
            Bundle.mPhases[Options.mPhaseIndex].mJobs.size())
    {
        std::cerr << "Phase or job index out of range\n";
        return 1;
    }

    FJobRecord &Job =
        Bundle.mPhases[Options.mPhaseIndex].mJobs[Options.mJobIndex];
    const std::string Target =
        MakeJobTarget(Options.mPhaseIndex, Options.mJobIndex);

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;
    std::string Desc;

    if (!Options.mStatus.empty())
    {
        const std::string Old = ToString(Job.mStatus);
        EExecutionStatus NewStatus;
        if (!ExecutionStatusFromString(Options.mStatus, NewStatus))
        {
            std::cerr << "Invalid status: " << Options.mStatus << "\n";
            return 1;
        }
        Changes.push_back({"status", {Old, Options.mStatus}});
        Job.mStatus = NewStatus;
        Desc = Target + " → " + Options.mStatus;

        const std::string UTC = GetUtcNow();
        if (NewStatus == EExecutionStatus::InProgress && Job.mStartedAt.empty())
        {
            Job.mStartedAt = UTC;
            Changes.push_back({"started_at", {"", UTC}});
        }
        if (NewStatus == EExecutionStatus::Completed &&
            Job.mCompletedAt.empty())
        {
            Job.mCompletedAt = UTC;
            Changes.push_back({"completed_at", {"", UTC}});
        }
    }
    if (!Options.mScope.empty())
    {
        Changes.push_back({"scope", {Job.mScope, Options.mScope}});
        Job.mScope = Options.mScope;
        if (Desc.empty())
            Desc = Target + " updated scope";
    }
    if (!Options.mOutput.empty())
    {
        Changes.push_back({"output", {Job.mOutput, Options.mOutput}});
        Job.mOutput = Options.mOutput;
    }
    if (!Options.mExitCriteria.empty())
    {
        Changes.push_back(
            {"exit_criteria", {Job.mExitCriteria, Options.mExitCriteria}});
        Job.mExitCriteria = Options.mExitCriteria;
    }
    if (Options.mLaneIndex >= 0)
    {
        if (static_cast<size_t>(Options.mLaneIndex) >=
            Bundle.mPhases[Options.mPhaseIndex].mLanes.size())
        {
            std::cerr << "Lane index out of range: " << Options.mLaneIndex
                      << "\n";
            return 1;
        }
        Changes.push_back(
            {"lane",
             {std::to_string(Job.mLane), std::to_string(Options.mLaneIndex)}});
        Job.mLane = Options.mLaneIndex;
        if (Desc.empty())
            Desc = Target + " reassigned lane";
    }
    if (Options.mWave >= 0)
    {
        Changes.push_back(
            {"wave",
             {std::to_string(Job.mWave), std::to_string(Options.mWave)}});
        Job.mWave = Options.mWave;
        if (Desc.empty())
            Desc = Target + " reassigned wave";
    }

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    // Build the changelog message from the full field list so repeated
    // set calls with different field subsets produce distinct entries.
    // Previously only the last-changed field ended up in Desc, which
    // caused no_duplicate_changelog warnings whenever the same single
    // field was set twice.
    std::string FieldList;
    for (const auto &C : Changes)
    {
        if (!FieldList.empty())
            FieldList += ", ";
        FieldList += C.first;
    }
    AppendAutoChangelog(Bundle, Target, Target + " updated: " + FieldList);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// task set
// ---------------------------------------------------------------------------

int RunTaskSetCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot)
{
    const FTaskSetOptions Options = ParseTaskSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size() ||
        static_cast<size_t>(Options.mJobIndex) >=
            Bundle.mPhases[Options.mPhaseIndex].mJobs.size() ||
        static_cast<size_t>(Options.mTaskIndex) >=
            Bundle.mPhases[Options.mPhaseIndex]
                .mJobs[Options.mJobIndex]
                .mTasks.size())
    {
        std::cerr << "Phase, job, or task index out of range\n";
        return 1;
    }

    FTaskRecord &Task = Bundle.mPhases[Options.mPhaseIndex]
                            .mJobs[Options.mJobIndex]
                            .mTasks[Options.mTaskIndex];
    const std::string Target = MakeTaskTarget(
        Options.mPhaseIndex, Options.mJobIndex, Options.mTaskIndex);

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    if (!Options.mStatus.empty())
    {
        const std::string Old = ToString(Task.mStatus);
        EExecutionStatus NewStatus;
        if (!ExecutionStatusFromString(Options.mStatus, NewStatus))
        {
            std::cerr << "Invalid status: " << Options.mStatus << "\n";
            return 1;
        }
        Changes.push_back({"status", {Old, Options.mStatus}});
        Task.mStatus = NewStatus;

        if (NewStatus == EExecutionStatus::Completed &&
            Task.mCompletedAt.empty())
        {
            const std::string UTC = GetUtcNow();
            Task.mCompletedAt = UTC;
            Changes.push_back({"completed_at", {"", UTC}});
        }
    }
    if (!Options.mEvidence.empty())
    {
        Changes.push_back({"evidence", {Task.mEvidence, Options.mEvidence}});
        Task.mEvidence = Options.mEvidence;
    }
    if (!Options.mNotes.empty())
    {
        Changes.push_back({"notes", {Task.mNotes, Options.mNotes}});
        Task.mNotes = Options.mNotes;
    }

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, Target, Target + " updated");

    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, false);
    return 0;
}

// ---------------------------------------------------------------------------
// changelog add
// ---------------------------------------------------------------------------

int RunChangelogAddCommand(const std::vector<std::string> &InArgs,
                           const std::string &InRepoRoot)
{
    const FChangelogAddOptions Options = ParseChangelogAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    FChangeLogEntry Entry;
    Entry.mPhase =
        Options.mScope.empty() ? -1 : std::atoi(Options.mScope.c_str());
    Entry.mDate = GetUtcNow().substr(0, 10);
    Entry.mChange = Options.mChange;
    Entry.mAffected = Options.mAffected;
    Entry.mActor = ETestingActor::AI;

    EChangeType Type;
    if (ChangeTypeFromString(Options.mType, Type))
        Entry.mType = Type;

    Bundle.mChangeLogs.push_back(std::move(Entry));

    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JsonQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", kTargetChangelogs);
    EmitJsonFieldSizeT("entry_index", Bundle.mChangeLogs.size() - 1, false);
    std::cout << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// verification add
// ---------------------------------------------------------------------------

int RunVerificationAddCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot)
{
    const FVerificationAddOptions Options = ParseVerificationAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    FVerificationEntry Entry;
    Entry.mPhase =
        Options.mScope.empty() ? -1 : std::atoi(Options.mScope.c_str());
    Entry.mDate = GetUtcNow().substr(0, 10);
    Entry.mCheck = Options.mCheck;
    Entry.mResult = Options.mResult;
    Entry.mDetail = Options.mDetail;

    Bundle.mVerifications.push_back(std::move(Entry));

    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JsonQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", kTargetVerifications);
    EmitJsonFieldSizeT("entry_index", Bundle.mVerifications.size() - 1, false);
    std::cout << "}\n";
    return 0;
}

// ===================================================================
// Tier 4: Query helpers (read-only)
// ===================================================================

// ---------------------------------------------------------------------------
// phase next — find first not_started phase with readiness report
// ---------------------------------------------------------------------------

int RunPhaseNextCommand(const std::vector<std::string> &InArgs,
                        const std::string &InRepoRoot)
{
    const FTopicGetOptions Options = ParseTopicGetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    int NextIndex = -1;
    for (size_t I = 0; I < Bundle.mPhases.size(); ++I)
    {
        if (Bundle.mPhases[I].mLifecycle.mStatus ==
            EExecutionStatus::NotStarted)
        {
            NextIndex = static_cast<int>(I);
            break;
        }
    }

    if (NextIndex < 0)
    {
        if (Options.mbHuman)
        {
            std::cout << kColorDim << "All phases started or completed\n"
                      << kColorReset;
            return 0;
        }
        std::cout << "{\"schema\":" << JsonQuote(kMutationSchema) << ",";
        EmitJsonFieldBool("ok", true);
        EmitJsonField("topic", Options.mTopic);
        EmitJsonFieldInt("phase_index", -1);
        EmitJsonField("scope", "");
        EmitJsonFieldBool("ready", false);
        std::cout << "\"missing_fields\":[]}\n";
        return 0;
    }

    const FPhaseRecord &Phase = Bundle.mPhases[static_cast<size_t>(NextIndex)];

    // Check readiness gates
    std::vector<std::string> MissingFields;
    if (Phase.mDesign.mInvestigation.empty())
        MissingFields.push_back("investigation");
    if (Phase.mDesign.mCodeEntityContract.empty())
        MissingFields.push_back("code_entity_contract");
    if (Phase.mDesign.mBestPractices.empty())
        MissingFields.push_back("best_practices");
    if (Phase.mDesign.mMultiPlatforming.empty())
        MissingFields.push_back("multi_platforming");
    if (Phase.mTesting.empty())
        MissingFields.push_back("testing");

    const bool Ready = MissingFields.empty();

    if (Options.mbHuman)
    {
        std::cout << kColorBold << "Next phase: " << kColorReset << NextIndex
                  << " — " << Phase.mScope << "\n";
        std::cout << "Ready: "
                  << (Ready ? (std::string(kColorGreen) + "yes")
                            : (std::string(kColorRed) + "no"))
                  << kColorReset << "\n";
        if (!MissingFields.empty())
        {
            std::cout << "Missing: ";
            for (size_t I = 0; I < MissingFields.size(); ++I)
            {
                if (I > 0)
                    std::cout << ", ";
                std::cout << MissingFields[I];
            }
            std::cout << "\n";
        }
        return 0;
    }

    std::cout << "{\"schema\":" << JsonQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonFieldInt("phase_index", NextIndex);
    EmitJsonField("scope", Phase.mScope);
    EmitJsonFieldBool("ready", Ready);
    std::cout << "\"missing_fields\":[";
    for (size_t I = 0; I < MissingFields.size(); ++I)
    {
        PrintJsonSep(I);
        std::cout << JsonQuote(MissingFields[I]);
    }
    std::cout << "]}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// phase readiness — gate-by-gate status for a specific phase
// ---------------------------------------------------------------------------

int RunPhaseReadinessCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot)
{
    const FPhaseQueryOptions Options = ParsePhaseQueryOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    const FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    struct FGateCheck
    {
        const char *mName;
        bool mbPass;
    };

    const FGateCheck Gates[] = {
        {"investigation", !Phase.mDesign.mInvestigation.empty()},
        {"code_entity_contract", !Phase.mDesign.mCodeEntityContract.empty()},
        {"code_snippets", !Phase.mDesign.mCodeSnippets.empty()},
        {"best_practices", !Phase.mDesign.mBestPractices.empty()},
        {"multi_platforming", !Phase.mDesign.mMultiPlatforming.empty()},
        {"testing", !Phase.mTesting.empty()},
    };

    bool AllPass = true;
    for (const auto &Gate : Gates)
    {
        if (!Gate.mbPass)
            AllPass = false;
    }

    if (Options.mbHuman)
    {
        std::cout << kColorBold << "Phase " << Options.mPhaseIndex
                  << " readiness" << kColorReset << "\n";
        std::cout << "Scope: " << Phase.mScope << "\n";
        std::cout << "Ready: "
                  << (AllPass ? (std::string(kColorGreen) + "yes")
                              : (std::string(kColorRed) + "no"))
                  << kColorReset << "\n\n";

        HumanTable Table;
        Table.mHeaders = {"Gate", "Status"};
        for (const auto &Gate : Gates)
        {
            Table.AddRow(
                {Gate.mName, ColorizeStatus(Gate.mbPass ? "pass" : "fail")});
        }
        Table.Print();
        return 0;
    }

    std::cout << "{\"schema\":" << JsonQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonFieldInt("phase_index", Options.mPhaseIndex);
    EmitJsonFieldBool("ready", AllPass);
    std::cout << "\"gates\":[";
    constexpr size_t GateCount = sizeof(Gates) / sizeof(Gates[0]);
    for (size_t I = 0; I < GateCount; ++I)
    {
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonField("name", Gates[I].mName);
        EmitJsonField("status", Gates[I].mbPass ? "pass" : "fail", false);
        std::cout << "}";
    }
    std::cout << "]}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// topic status — overview of all topics with active phases
// ---------------------------------------------------------------------------

int RunTopicStatusCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot)
{
    BaseOptions Options;
    const auto Remaining = ConsumeCommonOptions(InArgs, Options, false);
    for (size_t I = 0; I < Remaining.size(); ++I)
    {
        throw UsageError("Unknown option for topic status: " + Remaining[I]);
    }
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    std::vector<std::string> Warnings;
    const std::vector<FTopicBundle> Bundles =
        LoadAllBundles(RepoRoot, Warnings);

    int NotStarted = 0, InProgress = 0, Completed = 0, Blocked = 0,
        Canceled = 0;
    struct FActiveEntry
    {
        std::string mTopicKey;
        int mPhaseIndex;
        int mPhasesCompleted;
        int mPhasesTotal;
    };
    std::vector<FActiveEntry> Active;

    for (const auto &Bundle : Bundles)
    {
        switch (Bundle.mStatus)
        {
        case ETopicStatus::NotStarted:
            ++NotStarted;
            break;
        case ETopicStatus::InProgress:
            ++InProgress;
            break;
        case ETopicStatus::Completed:
            ++Completed;
            break;
        case ETopicStatus::Blocked:
            ++Blocked;
            break;
        case ETopicStatus::Canceled:
            ++Canceled;
            break;
        }

        if (Bundle.mStatus == ETopicStatus::InProgress)
        {
            int ActivePhase = -1;
            int PhasesCompleted = 0;
            for (size_t PI = 0; PI < Bundle.mPhases.size(); ++PI)
            {
                if (Bundle.mPhases[PI].mLifecycle.mStatus ==
                    EExecutionStatus::Completed)
                    ++PhasesCompleted;
                if (ActivePhase < 0 && Bundle.mPhases[PI].mLifecycle.mStatus ==
                                           EExecutionStatus::InProgress)
                    ActivePhase = static_cast<int>(PI);
            }
            Active.push_back({Bundle.mTopicKey, ActivePhase, PhasesCompleted,
                              static_cast<int>(Bundle.mPhases.size())});
        }
    }

    const int Total = static_cast<int>(Bundles.size());

    if (Options.mbHuman)
    {
        std::cout << kColorBold << "Topic Status Overview" << kColorReset
                  << "\n";
        std::cout << "Total: " << Total << "\n\n";

        HumanTable CountTable;
        CountTable.mHeaders = {"Status", "Count"};
        CountTable.AddRow({"not_started", std::to_string(NotStarted)});
        CountTable.AddRow({"in_progress", std::to_string(InProgress)});
        CountTable.AddRow({"completed", std::to_string(Completed)});
        CountTable.AddRow({"blocked", std::to_string(Blocked)});
        CountTable.AddRow({"canceled", std::to_string(Canceled)});
        CountTable.Print();

        if (!Active.empty())
        {
            std::cout << "\n"
                      << kColorBold << "Active Topics" << kColorReset << "\n";
            HumanTable ActiveTable;
            ActiveTable.mHeaders = {"Topic", "Phase", "Progress"};
            for (const auto &Entry : Active)
            {
                const std::string PhaseStr =
                    Entry.mPhaseIndex >= 0 ? std::to_string(Entry.mPhaseIndex)
                                           : "—";
                const std::string Progress =
                    std::to_string(Entry.mPhasesCompleted) + "/" +
                    std::to_string(Entry.mPhasesTotal);
                ActiveTable.AddRow({Entry.mTopicKey, PhaseStr, Progress});
            }
            ActiveTable.Print();
        }
        return 0;
    }

    std::cout << "{\"schema\":" << JsonQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonFieldInt("total", Total);
    std::cout << "\"counts\":{";
    EmitJsonFieldInt("not_started", NotStarted);
    EmitJsonFieldInt("in_progress", InProgress);
    EmitJsonFieldInt("completed", Completed);
    EmitJsonFieldInt("blocked", Blocked);
    EmitJsonFieldInt("canceled", Canceled, false);
    std::cout << "},\"active\":[";
    for (size_t I = 0; I < Active.size(); ++I)
    {
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonField("topic", Active[I].mTopicKey);
        EmitJsonFieldInt("phase_index", Active[I].mPhaseIndex);
        std::cout << "\"progress\":{";
        EmitJsonFieldInt("completed", Active[I].mPhasesCompleted);
        EmitJsonFieldInt("total", Active[I].mPhasesTotal, false);
        std::cout << "}}";
    }
    std::cout << "]}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// phase wave-status — per-wave job completion
// ---------------------------------------------------------------------------

int RunPhaseWaveStatusCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot)
{
    const FPhaseQueryOptions Options = ParsePhaseQueryOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    const FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    // Group jobs by wave
    struct FWaveInfo
    {
        int mTotal = 0;
        int mCompleted = 0;
    };
    std::map<int, FWaveInfo> WaveMap;
    for (const auto &Job : Phase.mJobs)
    {
        FWaveInfo &Info = WaveMap[Job.mWave];
        ++Info.mTotal;
        if (Job.mStatus == EExecutionStatus::Completed)
            ++Info.mCompleted;
    }

    // Current wave = first wave with incomplete jobs
    int CurrentWave = -1;
    for (const auto &Pair : WaveMap)
    {
        if (Pair.second.mCompleted < Pair.second.mTotal)
        {
            CurrentWave = Pair.first;
            break;
        }
    }

    if (Options.mbHuman)
    {
        std::cout << kColorBold << "Phase " << Options.mPhaseIndex
                  << " wave status" << kColorReset << "\n";
        if (CurrentWave >= 0)
            std::cout << "Current wave: W" << CurrentWave << "\n";
        else
            std::cout << "All waves complete\n";
        std::cout << "\n";

        HumanTable Table;
        Table.mHeaders = {"Wave", "Done", "Total", "Status"};
        for (const auto &Pair : WaveMap)
        {
            const std::string WaveLabel = "W" + std::to_string(Pair.first);
            const std::string Done = std::to_string(Pair.second.mCompleted);
            const std::string Total = std::to_string(Pair.second.mTotal);
            const bool AllDone = Pair.second.mCompleted == Pair.second.mTotal;
            const std::string Status =
                ColorizeStatus(AllDone ? "completed" : "in_progress");
            Table.AddRow({WaveLabel, Done, Total, Status});
        }
        Table.Print();
        return 0;
    }

    std::cout << "{\"schema\":" << JsonQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonFieldInt("phase_index", Options.mPhaseIndex);
    EmitJsonFieldInt("current_wave", CurrentWave);
    std::cout << "\"waves\":[";
    size_t WI = 0;
    for (const auto &Pair : WaveMap)
    {
        PrintJsonSep(WI++);
        std::cout << "{";
        EmitJsonFieldInt("wave", Pair.first);
        EmitJsonFieldInt("total", Pair.second.mTotal);
        EmitJsonFieldInt("completed", Pair.second.mCompleted, false);
        std::cout << "}";
    }
    std::cout << "]}\n";
    return 0;
}

// ===================================================================
// Tier 1: Phase lifecycle semantic commands
// ===================================================================

// ---------------------------------------------------------------------------
// phase start — claim a phase with gate enforcement
// ---------------------------------------------------------------------------

int RunPhaseStartCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot)
{
    const FPhaseStartOptions Options = ParsePhaseStartOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];
    const std::string Target = MakePhaseTarget(Options.mPhaseIndex);

    // Gate: phase must be not_started
    if (Phase.mLifecycle.mStatus != EExecutionStatus::NotStarted)
    {
        std::cerr << "Cannot start phase " << Options.mPhaseIndex
                  << ": status is " << ToString(Phase.mLifecycle.mStatus)
                  << ", expected not_started\n";
        return 1;
    }

    // Gate: design material non-empty
    if (Phase.mDesign.mInvestigation.empty() &&
        Phase.mDesign.mCodeEntityContract.empty())
    {
        std::cerr << "Cannot start phase " << Options.mPhaseIndex
                  << ": design material is empty "
                  << "(populate investigation or "
                  << "code_entity_contract first)\n";
        return 1;
    }

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    // Set phase → in_progress
    const std::string UTC = GetUtcNow();
    Changes.push_back({"status", {"not_started", "in_progress"}});
    Phase.mLifecycle.mStatus = EExecutionStatus::InProgress;
    Phase.mLifecycle.mStartedAt = UTC;
    Changes.push_back({"started_at", {"", UTC}});

    // Set agent_context if provided
    if (!Options.mContext.empty())
    {
        Changes.push_back({"agent_context",
                           {Phase.mLifecycle.mAgentContext, Options.mContext}});
        Phase.mLifecycle.mAgentContext = Options.mContext;
    }

    // Auto-cascade: if topic is not_started, set to in_progress
    if (Bundle.mStatus == ETopicStatus::NotStarted)
    {
        Bundle.mStatus = ETopicStatus::InProgress;
        AppendAutoChangelog(Bundle, kTargetPlan,
                            "Topic auto-started (phase " +
                                std::to_string(Options.mPhaseIndex) +
                                " claimed)");
    }

    AppendAutoChangelog(Bundle, Target,
                        "Phase " + std::to_string(Options.mPhaseIndex) +
                            " started");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// phase complete — close a phase with evidence
// ---------------------------------------------------------------------------

int RunPhaseCompleteCommand(const std::vector<std::string> &InArgs,
                            const std::string &InRepoRoot)
{
    const FPhaseCompleteOptions Options = ParsePhaseCompleteOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];
    const std::string Target = MakePhaseTarget(Options.mPhaseIndex);

    // Gate: phase must be in_progress
    if (Phase.mLifecycle.mStatus != EExecutionStatus::InProgress)
    {
        std::cerr << "Cannot complete phase " << Options.mPhaseIndex
                  << ": status is " << ToString(Phase.mLifecycle.mStatus)
                  << ", expected in_progress\n";
        return 1;
    }

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    const std::string UTC = GetUtcNow();
    Changes.push_back({"status", {"in_progress", "completed"}});
    Phase.mLifecycle.mStatus = EExecutionStatus::Completed;
    Phase.mLifecycle.mCompletedAt = UTC;
    Changes.push_back({"completed_at", {"", UTC}});
    Changes.push_back({"done", {Phase.mLifecycle.mDone, Options.mDone}});
    Phase.mLifecycle.mDone = Options.mDone;
    Changes.push_back({"remaining", {Phase.mLifecycle.mRemaining, ""}});
    Phase.mLifecycle.mRemaining.clear();

    AppendAutoChangelog(Bundle, Target,
                        "Phase " + std::to_string(Options.mPhaseIndex) +
                            " completed: " + Options.mDone);

    // Optional verification
    if (!Options.mVerification.empty())
    {
        FVerificationEntry VEntry;
        VEntry.mPhase = Options.mPhaseIndex;
        VEntry.mDate = UTC.substr(0, 10);
        VEntry.mCheck = Options.mVerification;
        VEntry.mResult = "pass";
        Bundle.mVerifications.push_back(std::move(VEntry));
    }

    // Auto-cascade: if ALL phases completed → topic completed
    bool AllCompleted = true;
    for (const auto &P : Bundle.mPhases)
    {
        if (P.mLifecycle.mStatus != EExecutionStatus::Completed)
        {
            AllCompleted = false;
            break;
        }
    }
    if (AllCompleted)
    {
        Bundle.mStatus = ETopicStatus::Completed;
        AppendAutoChangelog(Bundle, kTargetPlan,
                            "Topic auto-completed (all phases done)");
    }

    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// phase block — block a phase with reason
// ---------------------------------------------------------------------------

int RunPhaseBlockCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot)
{
    const FPhaseBlockOptions Options = ParsePhaseBlockOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];
    const std::string Target = MakePhaseTarget(Options.mPhaseIndex);

    // Gate: phase must be in_progress
    if (Phase.mLifecycle.mStatus != EExecutionStatus::InProgress)
    {
        std::cerr << "Cannot block phase " << Options.mPhaseIndex
                  << ": status is " << ToString(Phase.mLifecycle.mStatus)
                  << ", expected in_progress\n";
        return 1;
    }

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    Changes.push_back({"status", {"in_progress", "blocked"}});
    Phase.mLifecycle.mStatus = EExecutionStatus::Blocked;
    Changes.push_back(
        {"blockers", {Phase.mLifecycle.mBlockers, Options.mReason}});
    Phase.mLifecycle.mBlockers = Options.mReason;

    AppendAutoChangelog(Bundle, Target,
                        "Phase " + std::to_string(Options.mPhaseIndex) +
                            " blocked: " + Options.mReason);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// phase unblock — resume a blocked phase
// ---------------------------------------------------------------------------

int RunPhaseUnblockCommand(const std::vector<std::string> &InArgs,
                           const std::string &InRepoRoot)
{
    const FPhaseUnblockOptions Options = ParsePhaseUnblockOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];
    const std::string Target = MakePhaseTarget(Options.mPhaseIndex);

    // Gate: phase must be blocked
    if (Phase.mLifecycle.mStatus != EExecutionStatus::Blocked)
    {
        std::cerr << "Cannot unblock phase " << Options.mPhaseIndex
                  << ": status is " << ToString(Phase.mLifecycle.mStatus)
                  << ", expected blocked\n";
        return 1;
    }

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    Changes.push_back({"status", {"blocked", "in_progress"}});
    Phase.mLifecycle.mStatus = EExecutionStatus::InProgress;
    Changes.push_back({"blockers", {Phase.mLifecycle.mBlockers, ""}});
    Phase.mLifecycle.mBlockers.clear();

    AppendAutoChangelog(Bundle, Target,
                        "Phase " + std::to_string(Options.mPhaseIndex) +
                            " unblocked");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// phase progress — update done/remaining without status change
// ---------------------------------------------------------------------------

int RunPhaseProgressCommand(const std::vector<std::string> &InArgs,
                            const std::string &InRepoRoot)
{
    const FPhaseProgressOptions Options = ParsePhaseProgressOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];
    const std::string Target = MakePhaseTarget(Options.mPhaseIndex);

    // Gate: phase must be in_progress
    if (Phase.mLifecycle.mStatus != EExecutionStatus::InProgress)
    {
        std::cerr << "Cannot update progress for phase " << Options.mPhaseIndex
                  << ": status is " << ToString(Phase.mLifecycle.mStatus)
                  << ", expected in_progress\n";
        return 1;
    }

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    Changes.push_back({"done", {Phase.mLifecycle.mDone, Options.mDone}});
    Phase.mLifecycle.mDone = Options.mDone;
    Changes.push_back(
        {"remaining", {Phase.mLifecycle.mRemaining, Options.mRemaining}});
    Phase.mLifecycle.mRemaining = Options.mRemaining;

    AppendAutoChangelog(Bundle, Target,
                        "Phase " + std::to_string(Options.mPhaseIndex) +
                            " progress updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// phase complete-jobs — bulk-complete all incomplete jobs
// ---------------------------------------------------------------------------

int RunPhaseCompleteJobsCommand(const std::vector<std::string> &InArgs,
                                const std::string &InRepoRoot)
{
    const FPhaseCompleteJobsOptions Options =
        ParsePhaseCompleteJobsOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];
    const std::string Target = MakePhaseTarget(Options.mPhaseIndex);

    // Gate: phase must be in_progress
    if (Phase.mLifecycle.mStatus != EExecutionStatus::InProgress)
    {
        std::cerr << "Cannot complete-jobs for phase " << Options.mPhaseIndex
                  << ": status is " << ToString(Phase.mLifecycle.mStatus)
                  << ", expected in_progress\n";
        return 1;
    }

    const std::string UTC = GetUtcNow();
    int BulkCount = 0;
    for (auto &Job : Phase.mJobs)
    {
        if (Job.mStatus != EExecutionStatus::Completed)
        {
            Job.mStatus = EExecutionStatus::Completed;
            Job.mCompletedAt = UTC;
            ++BulkCount;
        }
    }

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;
    Changes.push_back(
        {"jobs_completed", {"", std::to_string(BulkCount) + " jobs"}});

    AppendAutoChangelog(Bundle, Target,
                        "Phase " + std::to_string(Options.mPhaseIndex) + ": " +
                            std::to_string(BulkCount) + " jobs bulk-completed");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ===================================================================
// Tier 2: Topic lifecycle semantic commands
// ===================================================================

// ---------------------------------------------------------------------------
// topic start — start a topic with gate enforcement
// ---------------------------------------------------------------------------

int RunTopicStartCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot)
{
    const FTopicStartOptions Options = ParseTopicStartOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // Gate: topic must be not_started
    if (Bundle.mStatus != ETopicStatus::NotStarted)
    {
        std::cerr << "Cannot start topic " << Options.mTopic << ": status is "
                  << ToString(Bundle.mStatus) << ", expected not_started\n";
        return 1;
    }

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    Changes.push_back({"status", {"not_started", "in_progress"}});
    Bundle.mStatus = ETopicStatus::InProgress;

    AppendAutoChangelog(Bundle, kTargetPlan, "Topic started");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, "plan", Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// topic complete — complete a topic with all-phases gate
// ---------------------------------------------------------------------------

int RunTopicCompleteCommand(const std::vector<std::string> &InArgs,
                            const std::string &InRepoRoot)
{
    const FTopicCompleteOptions Options = ParseTopicCompleteOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // Gate: all phases must be completed
    std::vector<int> NonCompleted;
    for (size_t I = 0; I < Bundle.mPhases.size(); ++I)
    {
        if (Bundle.mPhases[I].mLifecycle.mStatus != EExecutionStatus::Completed)
            NonCompleted.push_back(static_cast<int>(I));
    }
    if (!NonCompleted.empty())
    {
        std::cerr << "Cannot complete topic " << Options.mTopic << ": "
                  << NonCompleted.size() << " phase(s) not completed: [";
        for (size_t I = 0; I < NonCompleted.size(); ++I)
        {
            if (I > 0)
                std::cerr << ", ";
            std::cerr << NonCompleted[I];
        }
        std::cerr << "]\n";
        return 1;
    }

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    const std::string Old = ToString(Bundle.mStatus);
    Changes.push_back({"status", {Old, "completed"}});
    Bundle.mStatus = ETopicStatus::Completed;

    AppendAutoChangelog(Bundle, kTargetPlan, "Topic completed");

    // Optional verification
    if (!Options.mVerification.empty())
    {
        FVerificationEntry VEntry;
        VEntry.mPhase = -1;
        VEntry.mDate = GetUtcNow().substr(0, 10);
        VEntry.mCheck = Options.mVerification;
        VEntry.mResult = "pass";
        Bundle.mVerifications.push_back(std::move(VEntry));
    }

    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, "plan", Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// topic block — block a topic with reason
// ---------------------------------------------------------------------------

int RunTopicBlockCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot)
{
    const FTopicBlockOptions Options = ParseTopicBlockOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // Gate: topic must be in_progress
    if (Bundle.mStatus != ETopicStatus::InProgress)
    {
        std::cerr << "Cannot block topic " << Options.mTopic << ": status is "
                  << ToString(Bundle.mStatus) << ", expected in_progress\n";
        return 1;
    }

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    Changes.push_back({"status", {"in_progress", "blocked"}});
    Changes.push_back({"reason", {"", Options.mReason}});
    Bundle.mStatus = ETopicStatus::Blocked;

    AppendAutoChangelog(Bundle, kTargetPlan,
                        "Topic blocked: " + Options.mReason);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, "plan", Changes, true);
    return 0;
}

// ===================================================================
// Tier 3: Evidence shortcuts
// ===================================================================

// ---------------------------------------------------------------------------
// phase log — changelog add scoped to a phase (with bounds check)
// ---------------------------------------------------------------------------

int RunPhaseLogCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot)
{
    const FChangelogAddOptions Options = ParsePhaseLogOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // Bounds check on phase index
    const int PhaseIndex = std::atoi(Options.mScope.c_str());
    if (PhaseIndex < 0 ||
        static_cast<size_t>(PhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range: " << PhaseIndex << "\n";
        return 1;
    }

    FChangeLogEntry Entry;
    Entry.mPhase = PhaseIndex;
    Entry.mDate = GetUtcNow().substr(0, 10);
    Entry.mChange = Options.mChange;
    Entry.mAffected = Options.mAffected;
    Entry.mActor = ETestingActor::AI;

    EChangeType Type;
    if (ChangeTypeFromString(Options.mType, Type))
        Entry.mType = Type;

    Bundle.mChangeLogs.push_back(std::move(Entry));

    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JsonQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", kTargetChangelogs);
    EmitJsonFieldSizeT("entry_index", Bundle.mChangeLogs.size() - 1, false);
    std::cout << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// phase verify — verification add scoped to a phase (with bounds check)
// ---------------------------------------------------------------------------

int RunPhaseVerifyCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot)
{
    const FVerificationAddOptions Options = ParsePhaseVerifyOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // Bounds check on phase index
    const int PhaseIndex = std::atoi(Options.mScope.c_str());
    if (PhaseIndex < 0 ||
        static_cast<size_t>(PhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range: " << PhaseIndex << "\n";
        return 1;
    }

    FVerificationEntry Entry;
    Entry.mPhase = PhaseIndex;
    Entry.mDate = GetUtcNow().substr(0, 10);
    Entry.mCheck = Options.mCheck;
    Entry.mResult = Options.mResult;
    Entry.mDetail = Options.mDetail;

    Bundle.mVerifications.push_back(std::move(Entry));

    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JsonQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", kTargetVerifications);
    EmitJsonFieldSizeT("entry_index", Bundle.mVerifications.size() - 1, false);
    std::cout << "}\n";
    return 0;
}

// ===================================================================
// Tier 5: Missing entity coverage
// ===================================================================

// ---------------------------------------------------------------------------
// lane set — set lane status
// ---------------------------------------------------------------------------

int RunLaneSetCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot)
{
    const FLaneSetOptions Options = ParseLaneSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];
    if (static_cast<size_t>(Options.mLaneIndex) >= Phase.mLanes.size())
    {
        std::cerr << "Lane index out of range\n";
        return 1;
    }

    FLaneRecord &Lane = Phase.mLanes[static_cast<size_t>(Options.mLaneIndex)];
    const std::string Target =
        MakeLaneTarget(Options.mPhaseIndex, Options.mLaneIndex);

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;
    std::string Desc;

    if (!Options.mStatus.empty())
    {
        EExecutionStatus NewStatus;
        if (!ExecutionStatusFromString(Options.mStatus, NewStatus))
        {
            std::cerr << "Invalid lane status: " << Options.mStatus << "\n";
            return 1;
        }
        Changes.push_back(
            {"status", {ToString(Lane.mStatus), Options.mStatus}});
        Lane.mStatus = NewStatus;
        Desc = Target + " → " + Options.mStatus;
    }
    if (!Options.mScope.empty())
    {
        Changes.push_back({"scope", {Lane.mScope, Options.mScope}});
        Lane.mScope = Options.mScope;
        if (Desc.empty())
            Desc = Target + " updated scope";
    }
    if (!Options.mExitCriteria.empty())
    {
        Changes.push_back(
            {"exit_criteria", {Lane.mExitCriteria, Options.mExitCriteria}});
        Lane.mExitCriteria = Options.mExitCriteria;
    }

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    // Build the changelog message from the full field list so repeated
    // set calls with different field subsets produce distinct entries.
    // Previously only the last-changed field ended up in Desc, which
    // caused no_duplicate_changelog warnings whenever the same single
    // field was set twice.
    std::string FieldList;
    for (const auto &C : Changes)
    {
        if (!FieldList.empty())
            FieldList += ", ";
        FieldList += C.first;
    }
    AppendAutoChangelog(Bundle, Target, Target + " updated: " + FieldList);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// testing add — append a testing record to a phase
// ---------------------------------------------------------------------------

int RunTestingAddCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot)
{
    const FTestingAddOptions Options = ParseTestingAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    FTestingRecord Record;
    Record.mSession = Options.mSession;
    if (!Options.mActor.empty())
    {
        ETestingActor Actor;
        if (!TestingActorFromString(Options.mActor, Actor))
        {
            std::cerr << "Invalid actor: " << Options.mActor
                      << " (expected: human, ai, automated)\n";
            return 1;
        }
        Record.mActor = Actor;
    }
    Record.mStep = Options.mStep;
    Record.mAction = Options.mAction;
    Record.mExpected = Options.mExpected;
    Record.mEvidence = Options.mEvidence;

    Phase.mTesting.push_back(std::move(Record));

    const std::string Target =
        MakePhaseTarget(Options.mPhaseIndex) + ".testing";

    const size_t NewTestingIndex = Phase.mTesting.size() - 1;
    AppendAutoChangelog(Bundle, Target,
                        "testing[" + std::to_string(NewTestingIndex) +
                            "] added to phases[" +
                            std::to_string(Options.mPhaseIndex) + "]");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JsonQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", Target);
    EmitJsonFieldSizeT("entry_index", Phase.mTesting.size() - 1, false);
    std::cout << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// manifest add — append a file manifest item to a phase
// ---------------------------------------------------------------------------

int RunManifestAddCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot)
{
    const FManifestAddOptions Options = ParseManifestAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    EFileAction Action;
    if (!FileActionFromString(Options.mAction, Action))
    {
        std::cerr << "Invalid action: " << Options.mAction
                  << " (expected: create, modify, delete)\n";
        return 1;
    }

    FFileManifestItem Item;
    Item.mFilePath = Options.mFile;
    Item.mAction = Action;
    Item.mDescription = Options.mDescription;

    Phase.mFileManifest.push_back(std::move(Item));

    const std::string Target =
        MakePhaseTarget(Options.mPhaseIndex) + ".file_manifest";

    const size_t NewManifestIndex = Phase.mFileManifest.size() - 1;
    AppendAutoChangelog(Bundle, Target,
                        "file_manifest[" + std::to_string(NewManifestIndex) +
                            "] added to phases[" +
                            std::to_string(Options.mPhaseIndex) +
                            "]: " + Options.mFile);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JsonQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", Target);
    EmitJsonFieldSizeT("entry_index", Phase.mFileManifest.size() - 1, false);
    std::cout << "}\n";
    return 0;
}

// ---------------------------------------------------------------------------
// testing set — modify an existing testing record by index
// ---------------------------------------------------------------------------

int RunTestingSetCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot)
{
    const FTestingSetOptions Options = ParseTestingSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    if (static_cast<size_t>(Options.mIndex) >= Phase.mTesting.size())
    {
        std::cerr << "Testing index out of range: " << Options.mIndex
                  << " (size " << Phase.mTesting.size() << ")\n";
        return 1;
    }

    FTestingRecord &Record =
        Phase.mTesting[static_cast<size_t>(Options.mIndex)];
    const std::string Target =
        MakeTestingTarget(Options.mPhaseIndex, Options.mIndex);

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    if (!Options.mSession.empty())
    {
        Changes.push_back({"session", {Record.mSession, Options.mSession}});
        Record.mSession = Options.mSession;
    }
    if (!Options.mActor.empty())
    {
        ETestingActor NewActor;
        if (!TestingActorFromString(Options.mActor, NewActor))
        {
            std::cerr << "Invalid actor: " << Options.mActor
                      << " (expected: human, ai)\n";
            return 1;
        }
        Changes.push_back({"actor", {ToString(Record.mActor), Options.mActor}});
        Record.mActor = NewActor;
    }
    if (!Options.mStep.empty())
    {
        Changes.push_back({"step", {Record.mStep, Options.mStep}});
        Record.mStep = Options.mStep;
    }
    if (!Options.mAction.empty())
    {
        Changes.push_back({"action", {Record.mAction, Options.mAction}});
        Record.mAction = Options.mAction;
    }
    if (!Options.mExpected.empty())
    {
        Changes.push_back({"expected", {Record.mExpected, Options.mExpected}});
        Record.mExpected = Options.mExpected;
    }
    if (!Options.mEvidence.empty())
    {
        Changes.push_back({"evidence", {Record.mEvidence, Options.mEvidence}});
        Record.mEvidence = Options.mEvidence;
    }

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, Target, Target + " updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// verification set — modify an existing verification by index
// ---------------------------------------------------------------------------

int RunVerificationSetCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot)
{
    const FVerificationSetOptions Options = ParseVerificationSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mIndex) >= Bundle.mVerifications.size())
    {
        std::cerr << "Verification index out of range: " << Options.mIndex
                  << " (size " << Bundle.mVerifications.size() << ")\n";
        return 1;
    }

    FVerificationEntry &Entry =
        Bundle.mVerifications[static_cast<size_t>(Options.mIndex)];
    const std::string Target = MakeVerificationTarget(Options.mIndex);

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    if (!Options.mCheck.empty())
    {
        Changes.push_back({"check", {Entry.mCheck, Options.mCheck}});
        Entry.mCheck = Options.mCheck;
    }
    if (!Options.mResult.empty())
    {
        Changes.push_back({"result", {Entry.mResult, Options.mResult}});
        Entry.mResult = Options.mResult;
    }
    if (!Options.mDetail.empty())
    {
        Changes.push_back({"detail", {Entry.mDetail, Options.mDetail}});
        Entry.mDetail = Options.mDetail;
    }

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, Target, Target + " updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// manifest set — modify an existing file manifest entry by index
// ---------------------------------------------------------------------------

int RunManifestSetCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot)
{
    const FManifestSetOptions Options = ParseManifestSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    if (static_cast<size_t>(Options.mIndex) >= Phase.mFileManifest.size())
    {
        std::cerr << "Manifest index out of range: " << Options.mIndex
                  << " (size " << Phase.mFileManifest.size() << ")\n";
        return 1;
    }

    FFileManifestItem &Item =
        Phase.mFileManifest[static_cast<size_t>(Options.mIndex)];
    const std::string Target =
        MakeManifestTarget(Options.mPhaseIndex, Options.mIndex);

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    if (!Options.mFile.empty())
    {
        Changes.push_back({"file_path", {Item.mFilePath, Options.mFile}});
        Item.mFilePath = Options.mFile;
    }
    if (!Options.mAction.empty())
    {
        EFileAction NewAction;
        if (!FileActionFromString(Options.mAction, NewAction))
        {
            std::cerr << "Invalid action: " << Options.mAction
                      << " (expected: create, modify, delete)\n";
            return 1;
        }
        Changes.push_back(
            {"action", {ToString(Item.mAction), Options.mAction}});
        Item.mAction = NewAction;
    }
    if (!Options.mDescription.empty())
    {
        Changes.push_back(
            {"description", {Item.mDescription, Options.mDescription}});
        Item.mDescription = Options.mDescription;
    }

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, Target, Target + " updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// manifest remove - drop a file manifest entry by index
//
// Needed to undo bad manifest adds (e.g., invented file paths that
// don't exist on disk). No trailing-only restriction - unlike phase
// remove, file_manifest entries are not referenced by other entities
// so removing any index is safe. Auto-changelog is filed phase-scoped
// (not targeted at the removed index) so the removed-index path does
// not dangle.
// ---------------------------------------------------------------------------

int RunManifestRemoveCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot)
{
    const FManifestRemoveOptions Options = ParseManifestRemoveOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    if (static_cast<size_t>(Options.mIndex) >= Phase.mFileManifest.size())
    {
        std::cerr << "Manifest index out of range: " << Options.mIndex
                  << " (size " << Phase.mFileManifest.size() << ")\n";
        return 1;
    }

    const std::string RemovedFile =
        Phase.mFileManifest[static_cast<size_t>(Options.mIndex)].mFilePath;
    Phase.mFileManifest.erase(Phase.mFileManifest.begin() + Options.mIndex);

    const std::string PhaseTarget = MakePhaseTarget(Options.mPhaseIndex);
    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;
    Changes.push_back({"file_manifest[" + std::to_string(Options.mIndex) + "]",
                       {RemovedFile, "(removed)"}});

    AppendAutoChangelog(Bundle, PhaseTarget,
                        "file_manifest[" + std::to_string(Options.mIndex) +
                            "] removed from phases[" +
                            std::to_string(Options.mPhaseIndex) +
                            "]: " + RemovedFile);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, PhaseTarget, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// changelog set — modify an existing changelog entry by index
// ---------------------------------------------------------------------------

int RunChangelogSetCommand(const std::vector<std::string> &InArgs,
                           const std::string &InRepoRoot)
{
    const FChangelogSetOptions Options = ParseChangelogSetOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mIndex) >= Bundle.mChangeLogs.size())
    {
        std::cerr << "Changelog index out of range: " << Options.mIndex
                  << " (size " << Bundle.mChangeLogs.size() << ")\n";
        return 1;
    }

    FChangeLogEntry &Entry =
        Bundle.mChangeLogs[static_cast<size_t>(Options.mIndex)];
    const std::string Target = MakeChangelogTarget(Options.mIndex);

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    if (Options.mPhase != -2)
    {
        if (Options.mPhase >= 0 &&
            static_cast<size_t>(Options.mPhase) >= Bundle.mPhases.size())
        {
            std::cerr << "Phase out of range: " << Options.mPhase << "\n";
            return 1;
        }
        Changes.push_back(
            {"phase",
             {std::to_string(Entry.mPhase), std::to_string(Options.mPhase)}});
        Entry.mPhase = Options.mPhase;
    }
    if (!Options.mDate.empty())
    {
        Changes.push_back({"date", {Entry.mDate, Options.mDate}});
        Entry.mDate = Options.mDate;
    }
    if (!Options.mChange.empty())
    {
        Changes.push_back({"change", {Entry.mChange, Options.mChange}});
        Entry.mChange = Options.mChange;
    }
    if (!Options.mType.empty())
    {
        EChangeType NewType;
        if (!ChangeTypeFromString(Options.mType, NewType))
        {
            std::cerr << "Invalid type: " << Options.mType << "\n";
            return 1;
        }
        Changes.push_back({"type", {ToString(Entry.mType), Options.mType}});
        Entry.mType = NewType;
    }
    if (!Options.mAffected.empty())
    {
        Changes.push_back({"affected", {Entry.mAffected, Options.mAffected}});
        Entry.mAffected = Options.mAffected;
    }

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, Target, Target + " updated");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ---------------------------------------------------------------------------
// lane add — append a new lane to a phase
// ---------------------------------------------------------------------------

int RunLaneAddCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot)
{
    const FLaneAddOptions Options = ParseLaneAddOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    if (static_cast<size_t>(Options.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index out of range\n";
        return 1;
    }

    FPhaseRecord &Phase =
        Bundle.mPhases[static_cast<size_t>(Options.mPhaseIndex)];

    FLaneRecord Lane;
    if (!Options.mStatus.empty())
    {
        if (!ExecutionStatusFromString(Options.mStatus, Lane.mStatus))
        {
            std::cerr << "Invalid status: " << Options.mStatus << "\n";
            return 1;
        }
    }
    Lane.mScope = Options.mScope;
    Lane.mExitCriteria = Options.mExitCriteria;

    Phase.mLanes.push_back(std::move(Lane));

    const std::string Target = MakePhaseTarget(Options.mPhaseIndex) + ".lanes";

    AppendAutoChangelog(Bundle, Target,
                        "Lane added at index " +
                            std::to_string(Phase.mLanes.size() - 1));
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JsonQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", Target);
    EmitJsonFieldSizeT("lane_index", Phase.mLanes.size() - 1, false);
    std::cout << "}\n";
    return 0;
}

} // namespace UniPlan
