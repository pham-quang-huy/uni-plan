#include "UniPlanEnums.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanJsonIO.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

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
// TryLoadBundleByTopic — deterministic path lookup
// ---------------------------------------------------------------------------

bool TryLoadBundleByTopic(const fs::path &InRepoRoot,
                          const std::string &InTopicKey,
                          FTopicBundle &OutBundle, std::string &OutError)
{
    const fs::path BundlePath =
        InRepoRoot / "Docs" / "Plans" / (InTopicKey + ".Plan.json");
    if (!fs::exists(BundlePath))
    {
        OutError = "Bundle not found: " + BundlePath.string();
        return false;
    }
    return TryReadTopicBundle(BundlePath, OutBundle, OutError);
}

// ---------------------------------------------------------------------------
// LoadAllBundles — scan Docs/Plans/*.Plan.json
// ---------------------------------------------------------------------------

std::vector<FTopicBundle> LoadAllBundles(const fs::path &InRepoRoot,
                                         std::vector<std::string> &OutWarnings)
{
    std::vector<FTopicBundle> Bundles;
    const fs::path PlansDir = InRepoRoot / "Docs" / "Plans";
    if (!fs::is_directory(PlansDir))
    {
        OutWarnings.push_back("Plans directory not found: " +
                              PlansDir.string());
        return Bundles;
    }

    static const std::regex BundleRegex(R"(^([A-Za-z0-9]+)\.Plan\.json$)");

    for (const auto &Entry : fs::directory_iterator(PlansDir))
    {
        if (!Entry.is_regular_file())
            continue;
        const std::string Filename = Entry.path().filename().string();
        std::smatch Match;
        if (!std::regex_match(Filename, Match, BundleRegex))
            continue;
        FTopicBundle Bundle;
        std::string Error;
        if (TryReadTopicBundle(Entry.path(), Bundle, Error))
        {
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
    EmitJsonFieldNullable("validation_commands", Meta.mValidationCommands);
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
    PrintField("Next Actions", Bundle.mNextActions);

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
        throw UsageError("topic requires a subcommand: list, get, set");
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

    throw UsageError("Unknown topic subcommand: " + Sub +
                     ". Expected: list, get, set");
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
        EmitJsonFieldNullable("dependencies", Phase.mDesign.mDependencies);
        EmitJsonFieldNullable("code_entity_contract",
                              Phase.mDesign.mCodeEntityContract);
        EmitJsonFieldNullable("code_snippets", Phase.mDesign.mCodeSnippets);
        EmitJsonFieldNullable("best_practices", Phase.mDesign.mBestPractices);
        EmitJsonFieldNullable("handoff", Phase.mDesign.mHandoff);
        EmitJsonFieldNullable("validation_commands",
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
        EmitJsonFieldNullable("dependencies", Phase.mDesign.mDependencies);
        EmitJsonFieldNullable("handoff", Phase.mDesign.mHandoff);
        EmitJsonFieldNullable("validation_commands",
                              Phase.mDesign.mValidationCommands);
        EmitJsonFieldNullable("best_practices", Phase.mDesign.mBestPractices);
        EmitJsonFieldNullable("code_entity_contract",
                              Phase.mDesign.mCodeEntityContract);
        EmitJsonFieldNullable("code_snippets", Phase.mDesign.mCodeSnippets);
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

    std::cout << kColorBold << Bundle.mTopicKey << " phase["
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
    PrintField("Done", Phase.mLifecycle.mDone);
    PrintField("Remaining", Phase.mLifecycle.mRemaining);
    PrintField("Blockers", Phase.mLifecycle.mBlockers);
    PrintField("Readiness Gate", Phase.mDesign.mReadinessGate);
    PrintField("Dependencies", Phase.mDesign.mDependencies);
    PrintField("Handoff", Phase.mDesign.mHandoff);
    PrintField("Validation Commands", Phase.mDesign.mValidationCommands);

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
        throw UsageError("phase requires a subcommand: list, get, set");
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

    throw UsageError("Unknown phase subcommand: " + Sub +
                     ". Expected: list, get, set");
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
        return "phase[" + std::to_string(InPhase) + "] " + Scope;
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
    Table.mHeaders = {"Phase", "Date", "Type", "Change"};
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
        Table.AddRow({PhaseDisplay, rpEntry->mDate, ToString(rpEntry->mType),
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
    Table.mHeaders = {"Phase", "Date", "Check", "Result"};
    for (const FVerificationEntry *rpEntry : Filtered)
    {
        std::string Check = rpEntry->mCheck;
        if (Check.size() > 60)
            Check = Check.substr(0, 57) + "...";
        const std::string PhaseDisplay =
            rpEntry->mPhase < 0 ? "(topic)" : std::to_string(rpEntry->mPhase);
        Table.AddRow({PhaseDisplay, rpEntry->mDate,
                      kColorDim + Check + kColorReset, rpEntry->mResult});
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
        std::string mScope;
        std::string mText;
        std::string mType;
    };

    std::vector<FTimelineEntry> Entries;
    for (const FChangeLogEntry &CL : Bundle.mChangeLogs)
    {
        if (!InOptions.mSince.empty() && CL.mDate < InOptions.mSince)
            continue;
        const std::string PhaseStr =
            CL.mPhase < 0 ? "" : std::to_string(CL.mPhase);
        Entries.push_back(
            {CL.mDate, "changelog", PhaseStr, CL.mChange, ToString(CL.mType)});
    }
    for (const FVerificationEntry &VE : Bundle.mVerifications)
    {
        if (!InOptions.mSince.empty() && VE.mDate < InOptions.mSince)
            continue;
        const std::string VPhaseStr =
            VE.mPhase < 0 ? "" : std::to_string(VE.mPhase);
        Entries.push_back({VE.mDate, "verification", VPhaseStr, VE.mCheck, ""});
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
    EmitJsonFieldSizeT("count", Entries.size());
    std::cout << "\"entries\":[";
    for (size_t I = 0; I < Entries.size(); ++I)
    {
        PrintJsonSep(I);
        std::cout << "{";
        EmitJsonField("date", Entries[I].mDate);
        EmitJsonField("kind", Entries[I].mKind);
        EmitJsonField("scope", Entries[I].mScope);
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
        std::string mScope;
        std::string mText;
    };

    std::vector<FTimelineEntry> Entries;
    for (const FChangeLogEntry &CL : Bundle.mChangeLogs)
    {
        if (!InOptions.mSince.empty() && CL.mDate < InOptions.mSince)
            continue;
        const std::string PhaseStr =
            CL.mPhase < 0 ? "" : std::to_string(CL.mPhase);
        Entries.push_back({CL.mDate, "changelog", PhaseStr, CL.mChange});
    }
    for (const FVerificationEntry &VE : Bundle.mVerifications)
    {
        if (!InOptions.mSince.empty() && VE.mDate < InOptions.mSince)
            continue;
        const std::string VPhaseStr =
            VE.mPhase < 0 ? "" : std::to_string(VE.mPhase);
        Entries.push_back({VE.mDate, "verification", VPhaseStr, VE.mCheck});
    }
    std::sort(Entries.begin(), Entries.end(),
              [](const FTimelineEntry &A, const FTimelineEntry &B)
              { return A.mDate > B.mDate; });

    std::cout << kColorBold << "Timeline" << kColorReset
              << " topic=" << kColorOrange << Bundle.mTopicKey << kColorReset
              << " count=" << Entries.size() << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Date", "Kind", "Scope", "Text"};
    for (const FTimelineEntry &E : Entries)
    {
        std::string Text = E.mText;
        if (Text.size() > 60)
            Text = Text.substr(0, 57) + "...";
        Table.AddRow(
            {E.mDate, E.mKind, E.mScope, kColorDim + Text + kColorReset});
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

    const std::vector<ValidateCheck> Checks = ValidateAllBundles(Bundles);

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

    const std::vector<ValidateCheck> Checks = ValidateAllBundles(Bundles);

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
        Table.mHeaders = {"Severity", "Topic", "Path", "Detail"};
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
            Table.AddRow(
                {Sev, C.mTopic, kColorDim + Path + kColorReset, Detail});
        }
        Table.Print();
    }

    return ErrorMajorCount > 0 ? 1 : 0;
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
    // Extract scope from target: "phases[2].status" → "2"
    const size_t Open = InTarget.find('[');
    const size_t Close = InTarget.find(']');
    if (Open != std::string::npos && Close != std::string::npos)
        Entry.mPhase =
            std::atoi(InTarget.substr(Open + 1, Close - Open - 1).c_str());
    else
        Entry.mPhase = -1;
    // Date = today UTC
    const std::string UTC = GetUtcNow();
    Entry.mDate = UTC.substr(0, 10); // YYYY-MM-DD
    Entry.mChange = InDescription;
    Entry.mAffected = InTarget;
    Entry.mType = EChangeType::Chore;
    Entry.mActor = ETestingActor::AI;
    InOutBundle.mChangeLogs.push_back(std::move(Entry));
}

// Shared: write bundle to the deterministic path
static int WriteBundleBack(const FTopicBundle &InBundle,
                           const fs::path &InRepoRoot, std::string &OutError)
{
    const fs::path Path =
        InRepoRoot / "Docs" / "Plans" / (InBundle.mTopicKey + ".Plan.json");
    return TryWriteTopicBundle(InBundle, Path, OutError) ? 0 : 1;
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

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, "plan", Desc);
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
    const std::string Target =
        "phases[" + std::to_string(Options.mPhaseIndex) + "]";

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

    if (Changes.empty())
    {
        std::cerr << "No fields to update\n";
        return 1;
    }

    AppendAutoChangelog(Bundle, Target,
                        Desc.empty() ? Target + " updated" : Desc);
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
    const std::string Target = "phases[" + std::to_string(Options.mPhaseIndex) +
                               "].jobs[" + std::to_string(Options.mJobIndex) +
                               "]";

    const std::string Old = ToString(Job.mStatus);
    EExecutionStatus NewStatus;
    if (!ExecutionStatusFromString(Options.mStatus, NewStatus))
    {
        std::cerr << "Invalid status: " << Options.mStatus << "\n";
        return 1;
    }

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;
    Changes.push_back({"status", {Old, Options.mStatus}});
    Job.mStatus = NewStatus;

    const std::string UTC = GetUtcNow();
    if (NewStatus == EExecutionStatus::InProgress && Job.mStartedAt.empty())
    {
        Job.mStartedAt = UTC;
        Changes.push_back({"started_at", {"", UTC}});
    }
    if (NewStatus == EExecutionStatus::Completed && Job.mCompletedAt.empty())
    {
        Job.mCompletedAt = UTC;
        Changes.push_back({"completed_at", {"", UTC}});
    }

    AppendAutoChangelog(Bundle, Target, Target + " → " + Options.mStatus);
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
    const std::string Target = "phases[" + std::to_string(Options.mPhaseIndex) +
                               "].jobs[" + std::to_string(Options.mJobIndex) +
                               "].tasks[" + std::to_string(Options.mTaskIndex) +
                               "]";

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
    EmitJsonField("target", "changelogs");
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
    EmitJsonField("target", "verifications");
    EmitJsonFieldSizeT("entry_index", Bundle.mVerifications.size() - 1, false);
    std::cout << "}\n";
    return 0;
}

} // namespace UniPlan
