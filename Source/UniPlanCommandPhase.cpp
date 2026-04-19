#include "UniPlanEnums.h"
#include "UniPlanFileHelpers.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace UniPlan
{

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
        EmitJsonFieldSizeT("task_count", TaskCount);
        // Unified design-depth measure (same `ComputePhaseDesignChars`
        // used by `legacy-gap.v4_design_chars`, `validate.summary.
        // design_chars`, and the watch TUI `Design` column).
        EmitJsonFieldSizeT("design_chars", ComputePhaseDesignChars(Phase),
                           false);
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
    // `Design` = `ComputePhaseDesignChars(Phase)` — same measure used by
    // watch TUI, legacy-gap, and validate. Colored by hollow / thin /
    // rich thresholds (see ColorizeDesignChars).
    Table.mHeaders = {"Index", "Status", "Jobs", "Tasks", "Design", "Scope"};
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
        const size_t DesignChars = ComputePhaseDesignChars(Phase);
        Table.AddRow({std::to_string(I), ColorizeStatus(Status),
                      std::to_string(Phase.mJobs.size()),
                      std::to_string(TaskCount),
                      ColorizeDesignChars(DesignChars),
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
    // Unified design-depth measure — surfaced at top level of every
    // `phase get` mode (brief / reference / full / execution) so agents
    // can gate readiness checks without a second command round-trip.
    EmitJsonFieldSizeT("design_chars", ComputePhaseDesignChars(Phase));
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

    const size_t DesignChars = ComputePhaseDesignChars(Phase);
    std::cout << kColorBold << Bundle.mTopicKey << " phases["
              << InOptions.mPhaseIndex << "]" << kColorReset << "  status="
              << ColorizeStatus(ToString(Phase.mLifecycle.mStatus))
              << "  design=" << ColorizeDesignChars(DesignChars) << " "
              << kColorDim << "(" << GetDesignDepthLabel(DesignChars) << ")"
              << kColorReset << "\n\n";

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

} // namespace UniPlan
