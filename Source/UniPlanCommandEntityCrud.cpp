#include "UniPlanCommandMutationCommon.h"
#include "UniPlanEnums.h"
#include "UniPlanFileHelpers.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanJSON.h"
#include "UniPlanJSONHelpers.h"
#include "UniPlanStringHelpers.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace UniPlan
{

// ===================================================================
// v0.93.0 CRUD symmetry — fills the add / remove / list gaps that the
// original Tier 5 entity surface left open. These handlers mirror the
// existing `RunLaneAddCommand` + `RunManifestRemoveCommand` +
// `RunManifestListCommand` patterns from UniPlanCommandEntity.cpp so the
// emitted mutation-JSON shape and auto-changelog convention stay
// identical across every FTopicBundle entity type.
//
// Why a new file: UniPlanCommandEntity.cpp is already the largest command
// file in the repo. Splitting the v0.93.0 additions here keeps both
// files under the monolith threshold and keeps the CRUD-completion
// diff reviewable as a unit.
// ===================================================================

// ---------------------------------------------------------------------------
// Helper — map EExecutionStatus to wire-level string once so handlers
// don't re-inline the enum-to-string path below. Reuses the existing
// ToString() overload for EExecutionStatus; the wrapper is here just to
// keep the Change vector push sites one line each.
// ---------------------------------------------------------------------------
static std::string StatusOrDefault(const std::optional<EExecutionStatus> &InOp)
{
    return ToString(InOp.value_or(EExecutionStatus::NotStarted));
}

static bool TryGetBoardString(const JSONValue &InObject, const char *InField,
                              const std::string &InContext,
                              const bool InRequired, std::string &OutValue,
                              std::string &OutError)
{
    if (!InObject.contains(InField))
    {
        if (InRequired)
        {
            OutError = InContext + "." + InField + ": missing required string";
            return false;
        }
        return true;
    }
    if (!InObject[InField].is_string())
    {
        OutError = InContext + "." + InField + ": expected string";
        return false;
    }
    OutValue = InObject[InField].get<std::string>();
    return true;
}

static bool TryGetBoardInt(const JSONValue &InObject, const char *InField,
                           const std::string &InContext, int &OutValue,
                           std::string &OutError)
{
    if (!InObject.contains(InField) || !InObject[InField].is_number_integer())
    {
        OutError = InContext + "." + InField + ": expected integer";
        return false;
    }
    OutValue = InObject[InField].get<int>();
    return true;
}

static bool TryGetBoardStatus(const JSONValue &InObject, const char *InField,
                              const std::string &InContext,
                              EExecutionStatus &OutStatus,
                              std::string &OutError)
{
    OutStatus = EExecutionStatus::NotStarted;
    if (!InObject.contains(InField))
    {
        return true;
    }
    if (!InObject[InField].is_string())
    {
        OutError = InContext + "." + InField + ": expected status string";
        return false;
    }
    const std::string Raw = InObject[InField].get<std::string>();
    if (!ExecutionStatusFromString(Raw, OutStatus))
    {
        OutError = InContext + "." + InField +
                   ": invalid status '" + Raw + "'";
        return false;
    }
    return true;
}

static bool IsBoardUnevidenced(const FPhaseRecord &InPhase,
                               std::string &OutReason)
{
    for (size_t LaneIndex = 0; LaneIndex < InPhase.mLanes.size(); ++LaneIndex)
    {
        if (InPhase.mLanes[LaneIndex].mStatus !=
            EExecutionStatus::NotStarted)
        {
            OutReason = "lanes[" + std::to_string(LaneIndex) +
                        "] status is " +
                        ToString(InPhase.mLanes[LaneIndex].mStatus);
            return false;
        }
    }
    for (size_t JobIndex = 0; JobIndex < InPhase.mJobs.size(); ++JobIndex)
    {
        const FJobRecord &Job = InPhase.mJobs[JobIndex];
        if (Job.mStatus != EExecutionStatus::NotStarted)
        {
            OutReason = "jobs[" + std::to_string(JobIndex) +
                        "] status is " + ToString(Job.mStatus);
            return false;
        }
        if (!Job.mStartedAt.empty() || !Job.mCompletedAt.empty())
        {
            OutReason = "jobs[" + std::to_string(JobIndex) +
                        "] carries lifecycle timestamps";
            return false;
        }
        for (size_t TaskIndex = 0; TaskIndex < Job.mTasks.size(); ++TaskIndex)
        {
            const FTaskRecord &Task = Job.mTasks[TaskIndex];
            if (Task.mStatus != EExecutionStatus::NotStarted)
            {
                OutReason = "jobs[" + std::to_string(JobIndex) +
                            "].tasks[" + std::to_string(TaskIndex) +
                            "] status is " + ToString(Task.mStatus);
                return false;
            }
            if (!Task.mEvidence.empty() || !Task.mNotes.empty() ||
                !Task.mCompletedAt.empty())
            {
                OutReason = "jobs[" + std::to_string(JobIndex) +
                            "].tasks[" + std::to_string(TaskIndex) +
                            "] carries evidence, notes, or completed_at";
                return false;
            }
        }
    }
    return true;
}

static bool TryParseBoardLane(const JSONValue &InJSON,
                              const std::string &InContext,
                              FLaneRecord &OutLane, std::string &OutError)
{
    if (!InJSON.is_object())
    {
        OutError = InContext + ": expected object";
        return false;
    }
    if (!TryGetBoardStatus(InJSON, "status", InContext, OutLane.mStatus,
                           OutError))
    {
        return false;
    }
    if (!TryGetBoardString(InJSON, "scope", InContext, true, OutLane.mScope,
                           OutError))
    {
        return false;
    }
    if (!TryGetBoardString(InJSON, "exit_criteria", InContext, true,
                           OutLane.mExitCriteria, OutError))
    {
        return false;
    }
    return true;
}

static bool TryParseBoardTask(const JSONValue &InJSON,
                              const std::string &InContext,
                              FTaskRecord &OutTask, std::string &OutError)
{
    if (!InJSON.is_object())
    {
        OutError = InContext + ": expected object";
        return false;
    }
    if (!TryGetBoardStatus(InJSON, "status", InContext, OutTask.mStatus,
                           OutError))
    {
        return false;
    }
    if (!TryGetBoardString(InJSON, "description", InContext, true,
                           OutTask.mDescription, OutError))
    {
        return false;
    }
    if (!TryGetBoardString(InJSON, "evidence", InContext, false,
                           OutTask.mEvidence, OutError))
    {
        return false;
    }
    if (!TryGetBoardString(InJSON, "notes", InContext, false, OutTask.mNotes,
                           OutError))
    {
        return false;
    }
    if (!TryGetBoardString(InJSON, "completed_at", InContext, false,
                           OutTask.mCompletedAt, OutError))
    {
        return false;
    }
    return true;
}

static bool TryParseBoardJob(const JSONValue &InJSON,
                             const std::string &InContext,
                             FJobRecord &OutJob, std::string &OutError)
{
    if (!InJSON.is_object())
    {
        OutError = InContext + ": expected object";
        return false;
    }
    if (!TryGetBoardInt(InJSON, "wave", InContext, OutJob.mWave, OutError))
    {
        return false;
    }
    if (!TryGetBoardInt(InJSON, "lane", InContext, OutJob.mLane, OutError))
    {
        return false;
    }
    if (OutJob.mWave < 0 || OutJob.mLane < 0)
    {
        OutError = InContext + ": wave and lane must be non-negative";
        return false;
    }
    if (!TryGetBoardStatus(InJSON, "status", InContext, OutJob.mStatus,
                           OutError))
    {
        return false;
    }
    if (!TryGetBoardString(InJSON, "scope", InContext, true, OutJob.mScope,
                           OutError))
    {
        return false;
    }
    if (!TryGetBoardString(InJSON, "output", InContext, false, OutJob.mOutput,
                           OutError))
    {
        return false;
    }
    if (!TryGetBoardString(InJSON, "exit_criteria", InContext, true,
                           OutJob.mExitCriteria, OutError))
    {
        return false;
    }
    if (!TryGetBoardString(InJSON, "started_at", InContext, false,
                           OutJob.mStartedAt, OutError))
    {
        return false;
    }
    if (!TryGetBoardString(InJSON, "completed_at", InContext, false,
                           OutJob.mCompletedAt, OutError))
    {
        return false;
    }
    if (!InJSON.contains("tasks") || !InJSON["tasks"].is_array())
    {
        OutError = InContext + ".tasks: expected array";
        return false;
    }
    const JSONValue &Tasks = InJSON["tasks"];
    for (size_t TaskIndex = 0; TaskIndex < Tasks.size(); ++TaskIndex)
    {
        FTaskRecord Task;
        const std::string TaskContext =
            InContext + ".tasks[" + std::to_string(TaskIndex) + "]";
        if (!TryParseBoardTask(Tasks[TaskIndex], TaskContext, Task, OutError))
        {
            return false;
        }
        OutJob.mTasks.push_back(std::move(Task));
    }
    return true;
}

static bool TryParsePhaseBoardJSON(const std::string &InBytes,
                                   FPhaseRecord &OutBoard,
                                   std::string &OutError)
{
    JSONValue Root;
    try
    {
        Root = JSONValue::parse(InBytes);
    }
    catch (const std::exception &InError)
    {
        OutError = std::string("invalid board JSON: ") + InError.what();
        return false;
    }

    if (!Root.is_object())
    {
        OutError = "board JSON root must be an object";
        return false;
    }
    if (!Root.contains("lanes") || !Root["lanes"].is_array())
    {
        OutError = "board JSON requires lanes[]";
        return false;
    }
    if (!Root.contains("jobs") || !Root["jobs"].is_array())
    {
        OutError = "board JSON requires jobs[]";
        return false;
    }

    const JSONValue &Lanes = Root["lanes"];
    for (size_t LaneIndex = 0; LaneIndex < Lanes.size(); ++LaneIndex)
    {
        FLaneRecord Lane;
        const std::string LaneContext =
            "lanes[" + std::to_string(LaneIndex) + "]";
        if (!TryParseBoardLane(Lanes[LaneIndex], LaneContext, Lane, OutError))
        {
            return false;
        }
        OutBoard.mLanes.push_back(std::move(Lane));
    }

    const JSONValue &Jobs = Root["jobs"];
    for (size_t JobIndex = 0; JobIndex < Jobs.size(); ++JobIndex)
    {
        FJobRecord Job;
        const std::string JobContext =
            "jobs[" + std::to_string(JobIndex) + "]";
        if (!TryParseBoardJob(Jobs[JobIndex], JobContext, Job, OutError))
        {
            return false;
        }
        if (static_cast<size_t>(Job.mLane) >= OutBoard.mLanes.size())
        {
            OutError = JobContext + ".lane: index " +
                       std::to_string(Job.mLane) +
                       " out of range for lanes_count=" +
                       std::to_string(OutBoard.mLanes.size());
            return false;
        }
        OutBoard.mJobs.push_back(std::move(Job));
    }

    std::string Reason;
    if (!IsBoardUnevidenced(OutBoard, Reason))
    {
        OutError = "phase board-replace accepts only not_started, "
                   "unevidenced board input; " +
                   Reason;
        return false;
    }
    return true;
}

int RunPhaseBoardReplaceCommand(const std::vector<std::string> &InArgs,
                                const std::string &InRepoRoot)
{
    const FPhaseBoardReplaceOptions Options =
        ParsePhaseBoardReplaceOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    std::string BoardJSONBytes;
    std::string Error;
    if (!TryReadFileToString(Options.mBoardJSONFile, BoardJSONBytes, Error))
    {
        std::cerr << "phase board-replace: " << Error << "\n";
        return 1;
    }

    FPhaseRecord ParsedBoard;
    if (!TryParsePhaseBoardJSON(BoardJSONBytes, ParsedBoard, Error))
    {
        std::cerr << "phase board-replace: " << Error << "\n";
        return 1;
    }

    FTopicBundle Bundle;
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
    std::string Reason;
    if (!IsBoardUnevidenced(Phase, Reason))
    {
        std::cerr << "phase board-replace refuses to overwrite evidenced or "
                     "progressed board: "
                  << Reason << "\n";
        return 1;
    }

    const size_t OldLaneCount = Phase.mLanes.size();
    const size_t OldJobCount = Phase.mJobs.size();
    size_t OldTaskCount = 0;
    for (const FJobRecord &Job : Phase.mJobs)
    {
        OldTaskCount += Job.mTasks.size();
    }

    size_t NewTaskCount = 0;
    for (const FJobRecord &Job : ParsedBoard.mJobs)
    {
        NewTaskCount += Job.mTasks.size();
    }

    Phase.mLanes = std::move(ParsedBoard.mLanes);
    Phase.mJobs = std::move(ParsedBoard.mJobs);

    const std::string Target = MakePhaseTarget(Options.mPhaseIndex);
    AppendAutoChangelog(
        Bundle, Target,
        Target + " execution board replaced from JSON file: lanes " +
            std::to_string(OldLaneCount) + "->" +
            std::to_string(Phase.mLanes.size()) + ", jobs " +
            std::to_string(OldJobCount) + "->" +
            std::to_string(Phase.mJobs.size()) + ", tasks " +
            std::to_string(OldTaskCount) + "->" +
            std::to_string(NewTaskCount));

    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    const std::vector<Change> Changes = {
        {"lanes", {std::to_string(OldLaneCount),
                   std::to_string(Phase.mLanes.size())}},
        {"jobs",
         {std::to_string(OldJobCount), std::to_string(Phase.mJobs.size())}},
        {"tasks", {std::to_string(OldTaskCount),
                   std::to_string(NewTaskCount)}}};
    EmitMutationJson(Options.mTopic, Target, Changes, true,
                     Options.mbAckOnly);
    return 0;
}

// ===================================================================
// job add — append a new FJobRecord to a phase's jobs array
// ===================================================================

int RunJobAddCommand(const std::vector<std::string> &InArgs,
                     const std::string &InRepoRoot)
{
    const FJobAddOptions Options = ParseJobAddOptions(InArgs);
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

    if (Options.mLaneIndex >= 0 &&
        static_cast<size_t>(Options.mLaneIndex) >= Phase.mLanes.size())
    {
        std::cerr << "Lane index out of range: " << Options.mLaneIndex
                  << " (phase has " << Phase.mLanes.size() << " lanes)\n";
        return 1;
    }

    FJobRecord Job;
    if (Options.opStatus.has_value())
        Job.mStatus = *Options.opStatus;
    Job.mScope = Options.mScope;
    Job.mOutput = Options.mOutput;
    Job.mExitCriteria = Options.mExitCriteria;
    if (Options.mLaneIndex >= 0)
        Job.mLane = Options.mLaneIndex;
    if (Options.mWave >= 0)
        Job.mWave = Options.mWave;

    Phase.mJobs.push_back(std::move(Job));
    const size_t NewIndex = Phase.mJobs.size() - 1;

    const std::string PhaseTarget = MakePhaseTarget(Options.mPhaseIndex);
    const std::string JobTarget =
        MakeJobTarget(Options.mPhaseIndex, static_cast<int>(NewIndex));

    AppendAutoChangelog(Bundle, PhaseTarget,
                        "Job added at " + JobTarget);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", JobTarget);
    EmitJsonFieldSizeT("job_index", NewIndex, false);
    std::cout << "}\n";
    return 0;
}

// ===================================================================
// job remove — erase a job by index, shifting later entries down
// ===================================================================
//
// No referential cleanup needed beyond the shift: tasks live INSIDE the
// job entry (mTasks) so they're erased with it; lanes/waves are integer
// hints, not reverse references. Callers rewriting wave layout after a
// remove should follow up with `uni-plan job set --wave <W>` on the
// shifted siblings.
int RunJobRemoveCommand(const std::vector<std::string> &InArgs,
                        const std::string &InRepoRoot)
{
    const FJobRemoveOptions Options = ParseJobRemoveOptions(InArgs);
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
    if (static_cast<size_t>(Options.mJobIndex) >= Phase.mJobs.size())
    {
        std::cerr << "Job index out of range: " << Options.mJobIndex
                  << " (phase has " << Phase.mJobs.size() << " jobs)\n";
        return 1;
    }

    const FJobRecord Removed =
        Phase.mJobs[static_cast<size_t>(Options.mJobIndex)];
    Phase.mJobs.erase(Phase.mJobs.begin() +
                      static_cast<std::ptrdiff_t>(Options.mJobIndex));

    const std::string JobTarget =
        MakeJobTarget(Options.mPhaseIndex, Options.mJobIndex);
    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes = {
        {"scope", {Removed.mScope, "(removed)"}},
        {"status", {ToString(Removed.mStatus), "(removed)"}}};

    AppendAutoChangelog(Bundle, MakePhaseTarget(Options.mPhaseIndex),
                        JobTarget + " removed: '" + Removed.mScope + "'");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, JobTarget, Changes, true);
    return 0;
}

// ===================================================================
// job list — emit the jobs across every phase (or filtered to --phase)
// ===================================================================
//
// Read-only aggregate query. `--topic` is required (the caller asked for
// "which jobs in this plan"); omitting `--phase` scans every phase.
int RunJobListCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot)
{
    const FJobListOptions Options = ParseJobListOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kListSchema, UTC, RepoRoot.string());
    EmitJsonField("topic", Options.mTopic);
    std::cout << "\"entries\":[";
    bool bFirst = true;
    size_t TotalEntries = 0;
    for (size_t PI = 0; PI < Bundle.mPhases.size(); ++PI)
    {
        if (Options.mPhaseIndex >= 0 &&
            PI != static_cast<size_t>(Options.mPhaseIndex))
            continue;
        const FPhaseRecord &Phase = Bundle.mPhases[PI];
        for (size_t JI = 0; JI < Phase.mJobs.size(); ++JI)
        {
            const FJobRecord &Job = Phase.mJobs[JI];
            if (!bFirst)
                std::cout << ",";
            bFirst = false;
            ++TotalEntries;
            std::cout << "{";
            EmitJsonFieldSizeT("phase_index", PI);
            EmitJsonFieldSizeT("job_index", JI);
            EmitJsonField("status", ToString(Job.mStatus));
            EmitJsonField("scope", Job.mScope);
            EmitJsonField("output", Job.mOutput);
            EmitJsonField("exit_criteria", Job.mExitCriteria);
            EmitJsonFieldInt("lane", Job.mLane);
            EmitJsonFieldInt("wave", Job.mWave);
            EmitJsonFieldSizeT("task_count", Job.mTasks.size(), false);
            std::cout << "}";
        }
    }
    std::cout << "],";
    EmitJsonFieldSizeT("entry_count", TotalEntries);
    PrintJsonClose({});
    return 0;
}

// ===================================================================
// task add — append a new FTaskRecord to a job's tasks array
// ===================================================================

int RunTaskAddCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot)
{
    const FTaskAddOptions Options = ParseTaskAddOptions(InArgs);
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

    FTaskRecord Task;
    if (Options.opStatus.has_value())
        Task.mStatus = *Options.opStatus;
    Task.mDescription = Options.mDescription;
    Task.mEvidence = Options.mEvidence;
    Task.mNotes = Options.mNotes;

    Job.mTasks.push_back(std::move(Task));
    const size_t NewIndex = Job.mTasks.size() - 1;

    const std::string TaskTarget =
        MakeTaskTarget(Options.mPhaseIndex, Options.mJobIndex,
                       static_cast<int>(NewIndex));
    const std::string JobTarget =
        MakeJobTarget(Options.mPhaseIndex, Options.mJobIndex);

    AppendAutoChangelog(Bundle, JobTarget, "Task added at " + TaskTarget);
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", TaskTarget);
    EmitJsonFieldSizeT("task_index", NewIndex, false);
    std::cout << "}\n";
    return 0;
}

// ===================================================================
// task remove — erase a task by index, shifting later entries down
// ===================================================================

int RunTaskRemoveCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot)
{
    const FTaskRemoveOptions Options = ParseTaskRemoveOptions(InArgs);
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

    FJobRecord &Job =
        Bundle.mPhases[Options.mPhaseIndex].mJobs[Options.mJobIndex];
    const FTaskRecord Removed =
        Job.mTasks[static_cast<size_t>(Options.mTaskIndex)];
    Job.mTasks.erase(Job.mTasks.begin() +
                     static_cast<std::ptrdiff_t>(Options.mTaskIndex));

    const std::string TaskTarget = MakeTaskTarget(
        Options.mPhaseIndex, Options.mJobIndex, Options.mTaskIndex);
    const std::string JobTarget =
        MakeJobTarget(Options.mPhaseIndex, Options.mJobIndex);
    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes = {
        {"description", {Removed.mDescription, "(removed)"}}};

    AppendAutoChangelog(Bundle, JobTarget,
                        TaskTarget + " removed: '" + Removed.mDescription +
                            "'");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, TaskTarget, Changes, true);
    return 0;
}

// ===================================================================
// task list — emit the tasks across every phase/job (or filtered)
// ===================================================================

int RunTaskListCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot)
{
    const FTaskListOptions Options = ParseTaskListOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kListSchema, UTC, RepoRoot.string());
    EmitJsonField("topic", Options.mTopic);
    std::cout << "\"entries\":[";
    bool bFirst = true;
    size_t TotalEntries = 0;
    for (size_t PI = 0; PI < Bundle.mPhases.size(); ++PI)
    {
        if (Options.mPhaseIndex >= 0 &&
            PI != static_cast<size_t>(Options.mPhaseIndex))
            continue;
        const FPhaseRecord &Phase = Bundle.mPhases[PI];
        for (size_t JI = 0; JI < Phase.mJobs.size(); ++JI)
        {
            if (Options.mJobIndex >= 0 &&
                JI != static_cast<size_t>(Options.mJobIndex))
                continue;
            const FJobRecord &Job = Phase.mJobs[JI];
            for (size_t TI = 0; TI < Job.mTasks.size(); ++TI)
            {
                const FTaskRecord &Task = Job.mTasks[TI];
                if (!bFirst)
                    std::cout << ",";
                bFirst = false;
                ++TotalEntries;
                std::cout << "{";
                EmitJsonFieldSizeT("phase_index", PI);
                EmitJsonFieldSizeT("job_index", JI);
                EmitJsonFieldSizeT("task_index", TI);
                EmitJsonField("status", ToString(Task.mStatus));
                EmitJsonField("description", Task.mDescription);
                EmitJsonField("evidence", Task.mEvidence);
                EmitJsonField("notes", Task.mNotes, false);
                std::cout << "}";
            }
        }
    }
    std::cout << "],";
    EmitJsonFieldSizeT("entry_count", TotalEntries);
    PrintJsonClose({});
    return 0;
}

// ===================================================================
// lane remove — erase a lane by index. Refuses the remove when any
// job still references the lane (by index or by a later-shifted
// index), forcing the caller to reassign or remove the dependent
// jobs first. Prevents dangling-reference drift that would surface as
// lane_index out-of-range after the shift.
// ===================================================================

int RunLaneRemoveCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot)
{
    const FLaneRemoveOptions Options = ParseLaneRemoveOptions(InArgs);
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
        std::cerr << "Lane index out of range: " << Options.mLaneIndex
                  << " (phase has " << Phase.mLanes.size() << " lanes)\n";
        return 1;
    }

    // Count jobs referencing this lane. A later-shifted remove would
    // misalign jobs that reference index > mLaneIndex too, but the
    // canonical fix is "reassign jobs first, then remove" — we surface
    // the full count either way so the caller knows the scope.
    size_t RefCount = 0;
    for (const FJobRecord &Job : Phase.mJobs)
    {
        if (Job.mLane == Options.mLaneIndex)
            ++RefCount;
    }
    if (RefCount > 0)
    {
        std::cerr << "Refusing lane remove: " << RefCount
                  << " job(s) in this phase reference lane "
                  << Options.mLaneIndex
                  << ". Reassign or remove them first via "
                  << "`uni-plan job set --lane <new>` or "
                  << "`uni-plan job remove`.\n";
        return 1;
    }

    const FLaneRecord Removed =
        Phase.mLanes[static_cast<size_t>(Options.mLaneIndex)];
    Phase.mLanes.erase(Phase.mLanes.begin() +
                       static_cast<std::ptrdiff_t>(Options.mLaneIndex));

    // Any job with mLane > removed index now has a stale higher reference;
    // shift those down by one so the bundle remains self-consistent. Jobs
    // whose mLane == removed were already refused above.
    for (FJobRecord &Job : Phase.mJobs)
    {
        if (Job.mLane > Options.mLaneIndex)
            --Job.mLane;
    }

    const std::string LaneTarget =
        MakeLaneTarget(Options.mPhaseIndex, Options.mLaneIndex);
    const std::string PhaseTarget = MakePhaseTarget(Options.mPhaseIndex);
    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes = {{"scope", {Removed.mScope, "(removed)"}}};

    AppendAutoChangelog(Bundle, PhaseTarget,
                        LaneTarget + " removed: '" + Removed.mScope + "'");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, LaneTarget, Changes, true);
    return 0;
}

// ===================================================================
// lane list — emit the lanes across every phase (or filtered)
// ===================================================================

int RunLaneListCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot)
{
    const FLaneListOptions Options = ParseLaneListOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kListSchema, UTC, RepoRoot.string());
    EmitJsonField("topic", Options.mTopic);
    std::cout << "\"entries\":[";
    bool bFirst = true;
    size_t TotalEntries = 0;
    for (size_t PI = 0; PI < Bundle.mPhases.size(); ++PI)
    {
        if (Options.mPhaseIndex >= 0 &&
            PI != static_cast<size_t>(Options.mPhaseIndex))
            continue;
        const FPhaseRecord &Phase = Bundle.mPhases[PI];
        for (size_t LI = 0; LI < Phase.mLanes.size(); ++LI)
        {
            const FLaneRecord &Lane = Phase.mLanes[LI];
            size_t JobCount = 0;
            for (const FJobRecord &Job : Phase.mJobs)
            {
                if (Job.mLane == static_cast<int>(LI))
                    ++JobCount;
            }
            if (!bFirst)
                std::cout << ",";
            bFirst = false;
            ++TotalEntries;
            std::cout << "{";
            EmitJsonFieldSizeT("phase_index", PI);
            EmitJsonFieldSizeT("lane_index", LI);
            EmitJsonField("status", ToString(Lane.mStatus));
            EmitJsonField("scope", Lane.mScope);
            EmitJsonField("exit_criteria", Lane.mExitCriteria);
            EmitJsonFieldSizeT("job_count", JobCount, false);
            std::cout << "}";
        }
    }
    std::cout << "],";
    EmitJsonFieldSizeT("entry_count", TotalEntries);
    PrintJsonClose({});
    return 0;
}

// ===================================================================
// testing remove — erase a testing entry by index
// ===================================================================

int RunTestingRemoveCommand(const std::vector<std::string> &InArgs,
                            const std::string &InRepoRoot)
{
    const FTestingRemoveOptions Options = ParseTestingRemoveOptions(InArgs);
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
                  << " (phase has " << Phase.mTesting.size()
                  << " testing entries)\n";
        return 1;
    }

    const FTestingRecord Removed =
        Phase.mTesting[static_cast<size_t>(Options.mIndex)];
    Phase.mTesting.erase(Phase.mTesting.begin() +
                         static_cast<std::ptrdiff_t>(Options.mIndex));

    const std::string Target =
        MakeTestingTarget(Options.mPhaseIndex, Options.mIndex);
    const std::string PhaseTarget = MakePhaseTarget(Options.mPhaseIndex);
    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes = {{"step", {Removed.mStep, "(removed)"}}};

    AppendAutoChangelog(Bundle, PhaseTarget,
                        Target + " removed: '" + Removed.mStep + "'");
    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }
    EmitMutationJson(Options.mTopic, Target, Changes, true);
    return 0;
}

// ===================================================================
// testing list — emit the testing records across every phase (or filtered)
// ===================================================================

int RunTestingListCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot)
{
    const FTestingListOptions Options = ParseTestingListOptions(InArgs);
    const fs::path RepoRoot = NormalizeRepoRootPath(
        Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);

    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(RepoRoot, Options.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kListSchema, UTC, RepoRoot.string());
    EmitJsonField("topic", Options.mTopic);
    std::cout << "\"entries\":[";
    bool bFirst = true;
    size_t TotalEntries = 0;
    for (size_t PI = 0; PI < Bundle.mPhases.size(); ++PI)
    {
        if (Options.mPhaseIndex >= 0 &&
            PI != static_cast<size_t>(Options.mPhaseIndex))
            continue;
        const FPhaseRecord &Phase = Bundle.mPhases[PI];
        for (size_t TI = 0; TI < Phase.mTesting.size(); ++TI)
        {
            const FTestingRecord &T = Phase.mTesting[TI];
            if (!bFirst)
                std::cout << ",";
            bFirst = false;
            ++TotalEntries;
            std::cout << "{";
            EmitJsonFieldSizeT("phase_index", PI);
            EmitJsonFieldSizeT("testing_index", TI);
            EmitJsonField("actor", ToString(T.mActor));
            EmitJsonField("session", T.mSession);
            EmitJsonField("step", T.mStep);
            EmitJsonField("action", T.mAction);
            EmitJsonField("expected", T.mExpected);
            EmitJsonField("evidence", T.mEvidence, false);
            std::cout << "}";
        }
    }
    std::cout << "],";
    EmitJsonFieldSizeT("entry_count", TotalEntries);
    PrintJsonClose({});
    return 0;
}

// Suppress "unused static function" warnings for helpers reserved for
// future use. StatusOrDefault is exported here for potential reuse by
// tests and downstream tooling that emit add-command previews.
static void SuppressUnusedWarning_CrudHelpers()
{
    (void)StatusOrDefault;
}

} // namespace UniPlan
