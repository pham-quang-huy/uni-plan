#include "UniPlanCommandHelp.h"
#include "UniPlanEnums.h"
#include "UniPlanFileHelpers.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanPhaseMetrics.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>
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
        // v0.97.0 no-truncation contract: emit the verbatim stored scope.
        EmitJsonField("scope", Phase.mScope);
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
        const size_t DesignChars = ComputePhaseDesignChars(Phase);
        // v0.97.0 no-truncation contract.
        Table.AddRow({std::to_string(I), ColorizeStatus(Status),
                      std::to_string(Phase.mJobs.size()),
                      std::to_string(TaskCount),
                      ColorizeDesignChars(DesignChars),
                      kColorDim + Phase.mScope + kColorReset});
    }
    Table.Print();
    return 0;
}

// ---------------------------------------------------------------------------
// Phase Metric — JSON / Human
// ---------------------------------------------------------------------------

static void EmitMetricThresholdRangeJson(const char *InName,
                                         const size_t InHollow,
                                         const size_t InRich,
                                         const bool InTrailingComma = true)
{
    std::cout << "\"" << InName << "\":{";
    EmitJsonFieldSizeT("hollow", InHollow);
    EmitJsonFieldSizeT("rich", InRich, false);
    std::cout << "}";
    if (InTrailingComma)
    {
        std::cout << ",";
    }
}

static void EmitPhaseMetricThresholdsJson()
{
    std::cout << "\"thresholds\":{";
    EmitMetricThresholdRangeJson("design_chars", kPhaseHollowChars,
                                 kPhaseRichMinChars);
    EmitMetricThresholdRangeJson("solid_words", kPhaseMetricSolidHollowWords,
                                 kPhaseMetricSolidRichWords);
    EmitMetricThresholdRangeJson("recursive_words",
                                 kPhaseMetricRecursiveHollowWords,
                                 kPhaseMetricRecursiveRichWords);
    EmitMetricThresholdRangeJson("field_coverage_percent",
                                 kPhaseMetricFieldCoverageHollowPercent,
                                 kPhaseMetricFieldCoverageRichPercent);
    EmitMetricThresholdRangeJson("work_items", kPhaseMetricWorkHollowItems,
                                 kPhaseMetricWorkRichItems);
    EmitMetricThresholdRangeJson("testing_records",
                                 kPhaseMetricTestingHollowRecords,
                                 kPhaseMetricTestingRichRecords);
    EmitMetricThresholdRangeJson("file_manifest_entries",
                                 kPhaseMetricFileManifestHollowEntries,
                                 kPhaseMetricFileManifestRichEntries);
    EmitMetricThresholdRangeJson("evidence_items",
                                 kPhaseMetricEvidenceHollowItems,
                                 kPhaseMetricEvidenceRichItems);
    EmitJsonFieldSizeT("design_bloat_reference_chars",
                       kPhaseMetricDesignBloatReferenceChars, false);
    std::cout << "},";
}

static void EmitPhaseMetricObjectJson(const size_t InIndex,
                                      const FPhaseRecord &InPhase,
                                      const FPhaseRuntimeMetrics &InMetrics)
{
    std::cout << "{";
    EmitJsonFieldSizeT("index", InIndex);
    EmitJsonField("status", ToString(InPhase.mLifecycle.mStatus));
    EmitJsonFieldSizeT("design_chars", InMetrics.mDesignChars);
    EmitJsonFieldSizeT("largest_design_field_chars",
                       InMetrics.mLargestDesignFieldChars);
    EmitJsonFieldSizeT("repeated_design_block_count",
                       InMetrics.mRepeatedDesignBlockCount);
    EmitJsonFieldSizeT("design_bloat_ratio", InMetrics.mDesignBloatRatio);
    EmitJsonFieldSizeT("solid_words", InMetrics.mSolidWordCount);
    EmitJsonFieldSizeT("recursive_words", InMetrics.mRecursiveWordCount);
    EmitJsonFieldSizeT("authored_fields", InMetrics.mAuthoredFieldCount);
    EmitJsonFieldSizeT("authored_fields_total", InMetrics.mAuthoredFieldTotal);
    EmitJsonFieldSizeT("field_coverage_percent",
                       InMetrics.mFieldCoveragePercent);
    EmitJsonFieldSizeT("lane_count", InMetrics.mLaneCount);
    EmitJsonFieldSizeT("job_count", InMetrics.mJobCount);
    EmitJsonFieldSizeT("task_count", InMetrics.mTaskCount);
    EmitJsonFieldSizeT("work_items", InMetrics.mWorkItemCount);
    EmitJsonFieldSizeT("testing_records", InMetrics.mTestingRecordCount);
    EmitJsonFieldSizeT("file_manifest_entries", InMetrics.mFileManifestCount);
    EmitJsonFieldSizeT("evidence_items", InMetrics.mEvidenceItemCount);
    EmitJsonFieldSizeT("verification_count", InMetrics.mVerificationCount);
    EmitJsonFieldSizeT("changelog_count", InMetrics.mChangelogCount, false);
    std::cout << "}";
}

static std::string PhaseMetricRangeText(const size_t InPhaseCount)
{
    if (InPhaseCount == 0)
    {
        return " (topic has no phases)";
    }
    return " (0.." + std::to_string(InPhaseCount - 1) + ")";
}

static bool TryAppendPhaseMetricIndex(const FTopicBundle &InBundle,
                                      const FPhaseMetricOptions &InOptions,
                                      const int InIndex,
                                      std::vector<size_t> &OutIndices)
{
    if (InIndex < 0 || static_cast<size_t>(InIndex) >= InBundle.mPhases.size())
    {
        std::cerr << "Phase index " << InIndex << " out of range"
                  << PhaseMetricRangeText(InBundle.mPhases.size()) << "\n";
        return false;
    }

    const FPhaseRecord &Phase = InBundle.mPhases[static_cast<size_t>(InIndex)];
    const std::string Status = ToString(Phase.mLifecycle.mStatus);
    if (InOptions.mStatus == "all" || InOptions.mStatus == Status)
    {
        OutIndices.push_back(static_cast<size_t>(InIndex));
    }
    return true;
}

static bool TryCollectPhaseMetricIndices(const FTopicBundle &InBundle,
                                         const FPhaseMetricOptions &InOptions,
                                         std::vector<size_t> &OutIndices)
{
    if (InOptions.mPhaseIndex >= 0)
    {
        return TryAppendPhaseMetricIndex(InBundle, InOptions,
                                         InOptions.mPhaseIndex, OutIndices);
    }
    if (!InOptions.mPhaseIndices.empty())
    {
        for (const int Index : InOptions.mPhaseIndices)
        {
            if (!TryAppendPhaseMetricIndex(InBundle, InOptions, Index,
                                           OutIndices))
            {
                return false;
            }
        }
        return true;
    }
    for (size_t Index = 0; Index < InBundle.mPhases.size(); ++Index)
    {
        if (!TryAppendPhaseMetricIndex(InBundle, InOptions,
                                       static_cast<int>(Index), OutIndices))
        {
            return false;
        }
    }
    return true;
}

static int RunBundlePhaseMetricJson(const fs::path &InRepoRoot,
                                    const FPhaseMetricOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::vector<size_t> Indices;
    if (!TryCollectPhaseMetricIndices(Bundle, InOptions, Indices))
    {
        return 1;
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kPhaseMetricSchema, UTC, InRepoRoot.string());
    EmitJsonField("topic", Bundle.mTopicKey);
    EmitJsonField("status_filter", InOptions.mStatus);
    EmitPhaseMetricThresholdsJson();
    EmitJsonFieldSizeT("count", Indices.size());
    std::cout << "\"phases\":[";
    for (size_t Offset = 0; Offset < Indices.size(); ++Offset)
    {
        const size_t Index = Indices[Offset];
        PrintJsonSep(Offset);
        EmitPhaseMetricObjectJson(Index, Bundle.mPhases[Index],
                                  ComputePhaseDepthMetrics(Bundle, Index));
    }
    std::cout << "],";
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

static const char *ColorForMetric(const size_t InValue, const size_t InHollow,
                                  const size_t InRich)
{
    if (InValue < InHollow)
    {
        return kColorRed;
    }
    if (InValue < InRich)
    {
        return kColorYellow;
    }
    return kColorGreen;
}

static std::string HumanMetricBar(const size_t InValue, const size_t InHollow,
                                  const size_t InRich,
                                  const std::string &InLabel)
{
    static constexpr size_t kWidth = 8;
    const size_t Filled =
        InRich == 0
            ? 0
            : (std::min)(kWidth, (InValue * kWidth + InRich - 1) / InRich);
    const std::string Bar = "[" + std::string(Filled, '#') +
                            std::string(kWidth - Filled, '.') + "] " + InLabel;
    return Colorize(ColorForMetric(InValue, InHollow, InRich), Bar);
}

static int RunBundlePhaseMetricHuman(const fs::path &InRepoRoot,
                                     const FPhaseMetricOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::vector<size_t> Indices;
    if (!TryCollectPhaseMetricIndices(Bundle, InOptions, Indices))
    {
        return 1;
    }

    std::cout << kColorBold << "Phase Metrics" << kColorReset
              << " topic=" << kColorOrange << Bundle.mTopicKey << kColorReset;
    if (InOptions.mStatus != "all")
    {
        std::cout << " [status=" << InOptions.mStatus << "]";
    }
    std::cout << "\n\n";

    HumanTable Table;
    Table.mHeaders = {"Index",  "Status", "Design", "Bloat", "SOLID",
                      "Words",  "Fields", "Work",   "Tests", "Files",
                      "Evidence"};
    for (const size_t Index : Indices)
    {
        const FPhaseRecord &Phase = Bundle.mPhases[Index];
        const FPhaseRuntimeMetrics Metrics =
            ComputePhaseDepthMetrics(Bundle, Index);
        Table.AddRow({
            std::to_string(Index),
            ColorizeStatus(ToString(Phase.mLifecycle.mStatus)),
            HumanMetricBar(Metrics.mDesignChars, kPhaseHollowChars,
                           kPhaseRichMinChars,
                           std::to_string(Metrics.mDesignChars)),
            std::to_string(Metrics.mLargestDesignFieldChars) + " max / " +
                std::to_string(Metrics.mRepeatedDesignBlockCount) + " rep / " +
                std::to_string(Metrics.mDesignBloatRatio) + "%",
            HumanMetricBar(Metrics.mSolidWordCount,
                           kPhaseMetricSolidHollowWords,
                           kPhaseMetricSolidRichWords,
                           std::to_string(Metrics.mSolidWordCount)),
            HumanMetricBar(Metrics.mRecursiveWordCount,
                           kPhaseMetricRecursiveHollowWords,
                           kPhaseMetricRecursiveRichWords,
                           std::to_string(Metrics.mRecursiveWordCount)),
            HumanMetricBar(Metrics.mFieldCoveragePercent,
                           kPhaseMetricFieldCoverageHollowPercent,
                           kPhaseMetricFieldCoverageRichPercent,
                           std::to_string(Metrics.mFieldCoveragePercent) + "%"),
            HumanMetricBar(Metrics.mWorkItemCount, kPhaseMetricWorkHollowItems,
                           kPhaseMetricWorkRichItems,
                           std::to_string(Metrics.mWorkItemCount)),
            HumanMetricBar(Metrics.mTestingRecordCount,
                           kPhaseMetricTestingHollowRecords,
                           kPhaseMetricTestingRichRecords,
                           std::to_string(Metrics.mTestingRecordCount)),
            HumanMetricBar(Metrics.mFileManifestCount,
                           kPhaseMetricFileManifestHollowEntries,
                           kPhaseMetricFileManifestRichEntries,
                           std::to_string(Metrics.mFileManifestCount)),
            HumanMetricBar(Metrics.mEvidenceItemCount,
                           kPhaseMetricEvidenceHollowItems,
                           kPhaseMetricEvidenceRichItems,
                           std::to_string(Metrics.mEvidenceItemCount)),
        });
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

// Emit the stream of per-phase fields that follow the `topic` field in
// the v1 single-phase output. Ends with a trailing comma on the last
// field so the caller can chain additional top-level fields (single
// phase: "warnings":[] via PrintJsonClose; batch: absorbed + trimmed
// before the closing `}` of the per-phase object). Extracted v0.84.0
// to support the `--phases 1,3,5` batch mode without duplicating the
// four emission branches (--brief / --design / --execution / full).
static void EmitPhaseGetFieldsJson(const FTopicBundle &InBundle,
                                   size_t InPhaseIndex,
                                   const FPhaseGetOptions &InOptions);

static int RunBundlePhaseGetBatchJson(const fs::path &InRepoRoot,
                                      const FPhaseGetOptions &InOptions);

static int RunBundlePhaseGetJson(const fs::path &InRepoRoot,
                                 const FPhaseGetOptions &InOptions)
{
    // Batch mode (`--phases 1,3,5` or v0.105.0+ `--all-phases`) dispatches
    // to the wrapped batch renderer. `--all-phases` is sugar for "every
    // phase index": expand to mPhaseIndices once the bundle is loaded so
    // the existing batch emitter path handles the iteration unchanged.
    if (InOptions.mbAllPhases)
    {
        FTopicBundle BundleForCount;
        std::string BundleErr;
        if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, BundleForCount,
                                  BundleErr))
        {
            std::cerr << BundleErr << "\n";
            return 1;
        }
        FPhaseGetOptions Expanded = InOptions;
        Expanded.mPhaseIndices.clear();
        for (size_t I = 0; I < BundleForCount.mPhases.size(); ++I)
        {
            Expanded.mPhaseIndices.push_back(static_cast<int>(I));
        }
        // Empty-phase bundles emit an empty phases[] array via the batch
        // renderer — that is the correct behavior for --all-phases on a
        // newly-created bundle with no phases yet.
        return RunBundlePhaseGetBatchJson(InRepoRoot, Expanded);
    }
    if (!InOptions.mPhaseIndices.empty())
        return RunBundlePhaseGetBatchJson(InRepoRoot, InOptions);

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

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kPhaseGetSchema, UTC, InRepoRoot.string());
    EmitJsonField("topic", Bundle.mTopicKey);
    EmitPhaseGetFieldsJson(Bundle, static_cast<size_t>(InOptions.mPhaseIndex),
                           InOptions);
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Extracted per-phase field emission. Called by both the single-phase
// JSON renderer (above) and the batch renderer (below). Emits fields
// with trailing commas (the caller appends `"warnings":[]}` or absorbs
// the trailing comma when wrapping this body in a `{...}` array element).
// ---------------------------------------------------------------------------
static void EmitPhaseGetFieldsJson(const FTopicBundle &InBundle,
                                   size_t InPhaseIndex,
                                   const FPhaseGetOptions &InOptions)
{
    const FPhaseRecord &Phase = InBundle.mPhases[InPhaseIndex];
    EmitJsonFieldSizeT("phase_index", InPhaseIndex);
    EmitJsonField("status", ToString(Phase.mLifecycle.mStatus));
    // Unified design-depth measure — surfaced at top level of every
    // `phase get` mode (brief / reference / full / execution) so agents
    // can gate readiness checks without a second command round-trip.
    EmitJsonFieldSizeT("design_chars", ComputePhaseDesignChars(Phase));
    // Governance-phase opt-out signal — surfaced in every mode so
    // auditors can see, in one query, whether a phase is exempt from
    // code-bearing gates (file_manifest, code_entity_contract,
    // code_snippets, multi_platforming). When mbNoFileManifest is
    // true the JSON deserializer guarantees mFileManifestSkipReason
    // is non-empty, so the reason string is always usable when the
    // bool is true. Added v0.96.0 to close the `phase get` CLI gap.
    EmitJsonFieldBool("no_file_manifest", Phase.mbNoFileManifest);
    EmitJsonFieldNullable("file_manifest_skip_reason",
                          Phase.mFileManifestSkipReason);
    EmitJsonFieldNullable("scope", Phase.mScope);

    // --brief: compact view for session resume. v0.97.0+ the view is
    // "fewer fields" not "truncated fields" — every emitted field
    // carries its byte-identical stored value; callers that want a
    // token-budgeted preview are responsible for trimming on their end.
    if (InOptions.mbBrief)
    {
        EmitJsonFieldNullable("done", Phase.mLifecycle.mDone);
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
            EmitJsonField("scope", Phase.mJobs[I].mScope, false);
            std::cout << "}";
        }
        std::cout << "],";
        EmitJsonFieldInt("next_job", ComputeNextJob(Phase));
        EmitProgress(Phase);
        // Caller handles the closing brace + `warnings` field.
        return;
    }

    // --design: exactly the fields that contribute to `design_chars`
    // (scope + output + 7 design material prose fields). Structural
    // gate info — dependencies, validation_commands — lives in
    // --execution (v0.83.0 realignment). Renamed from --reference.
    if (InOptions.mbDesign)
    {
        EmitJsonFieldNullable("output", Phase.mOutput);
        EmitJsonFieldNullable("readiness_gate", Phase.mDesign.mReadinessGate);
        EmitJsonFieldNullable("investigation", Phase.mDesign.mInvestigation);
        EmitJsonFieldNullable("code_entity_contract",
                              Phase.mDesign.mCodeEntityContract);
        EmitJsonFieldNullable("code_snippets", Phase.mDesign.mCodeSnippets);
        EmitJsonFieldNullable("best_practices", Phase.mDesign.mBestPractices);
        EmitJsonFieldNullable("multi_platforming",
                              Phase.mDesign.mMultiPlatforming);
        EmitJsonFieldNullable("handoff", Phase.mDesign.mHandoff);
        return;
    }

    // Full or --execution mode
    EmitJsonFieldNullable("output", Phase.mOutput);
    EmitJsonFieldNullable("done", Phase.mLifecycle.mDone);
    EmitJsonFieldNullable("remaining", Phase.mLifecycle.mRemaining);
    EmitJsonFieldNullable("blockers", Phase.mLifecycle.mBlockers);
    EmitJsonFieldNullable("started_at", Phase.mLifecycle.mStartedAt);
    EmitJsonFieldNullable("completed_at", Phase.mLifecycle.mCompletedAt);
    EmitJsonFieldNullable("agent_context", Phase.mLifecycle.mAgentContext);

    // Structural gate info — emitted in both --execution and full mode.
    // dependencies + validation_commands are contracts/pre-requisites,
    // not prose, so they moved out of --design in v0.83.0.
    EmitDependenciesJson("dependencies", Phase.mDesign.mDependencies);
    EmitValidationCommandsJson("validation_commands",
                               Phase.mDesign.mValidationCommands);

    if (!InOptions.mbExecution)
    {
        // Full mode also includes design material prose
        EmitJsonFieldNullable("readiness_gate", Phase.mDesign.mReadinessGate);
        EmitJsonFieldNullable("investigation", Phase.mDesign.mInvestigation);
        EmitJsonFieldNullable("handoff", Phase.mDesign.mHandoff);
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

    // Full / --execution path exits here; caller closes the envelope.
}

// ---------------------------------------------------------------------------
// Phase Get — Batch JSON (v0.84.0)
// Renders N phase objects under a wrapped `phases` array using the v2
// schema (`uni-plan-phase-get-v2`). Each inner object is the same field
// shape as the v1 single-phase top-level (minus the envelope fields),
// captured through a std::cout redirect so every existing mode branch
// (--brief / --design / --execution / full) reuses the shared emitter.
// Trailing comma on the last field is trimmed before the per-phase
// closing `}`.
// ---------------------------------------------------------------------------
static int RunBundlePhaseGetBatchJson(const fs::path &InRepoRoot,
                                      const FPhaseGetOptions &InOptions)
{
    FTopicBundle Bundle;
    std::string Error;
    if (!TryLoadBundleByTopic(InRepoRoot, InOptions.mTopic, Bundle, Error))
    {
        std::cerr << Error << "\n";
        return 1;
    }

    // Bounds-check each requested index before emitting anything.
    for (const int Idx : InOptions.mPhaseIndices)
    {
        if (Idx < 0 || static_cast<size_t>(Idx) >= Bundle.mPhases.size())
        {
            std::cerr << "Phase index " << Idx << " out of range (0.."
                      << Bundle.mPhases.size() - 1 << ")\n";
            return 1;
        }
    }

    const std::string UTC = GetUtcNow();
    PrintJsonHeader(kPhaseGetBatchSchema, UTC, InRepoRoot.string());
    EmitJsonField("topic", Bundle.mTopicKey);
    std::cout << "\"phases\":[";
    for (size_t N = 0; N < InOptions.mPhaseIndices.size(); ++N)
    {
        const size_t PI = static_cast<size_t>(InOptions.mPhaseIndices[N]);
        // Capture the per-phase field stream to trim the trailing comma
        // left by the shared emitter before wrapping in `{...}`.
        std::ostringstream Buffer;
        auto *rpOldStream = std::cout.rdbuf(Buffer.rdbuf());
        EmitPhaseGetFieldsJson(Bundle, PI, InOptions);
        std::cout.rdbuf(rpOldStream);
        std::string Body = Buffer.str();
        while (!Body.empty() &&
               (Body.back() == ',' ||
                std::isspace(static_cast<unsigned char>(Body.back()))))
            Body.pop_back();
        PrintJsonSep(N);
        std::cout << "{" << Body << "}";
    }
    std::cout << "],";
    EmitJsonFieldSizeT("phase_count", InOptions.mPhaseIndices.size());
    std::vector<std::string> Warnings;
    PrintJsonClose(Warnings);
    return 0;
}

// ---------------------------------------------------------------------------
// Phase Get — Human
// ---------------------------------------------------------------------------

// Emit per-phase human rendering for a single phase index. Extracted
// v0.84.0 so the batch path (`--phases 1,3,5`) can reuse the same per-
// phase output with a visible separator between entries.
static void EmitPhaseGetHuman(const FTopicBundle &InBundle, size_t InPhaseIndex,
                              const FPhaseGetOptions &InOptions)
{
    const FPhaseRecord &Phase = InBundle.mPhases[InPhaseIndex];

    const size_t DesignChars = ComputePhaseDesignChars(Phase);
    std::cout << kColorBold << InBundle.mTopicKey << " phases[" << InPhaseIndex
              << "]" << kColorReset << "  status="
              << ColorizeStatus(ToString(Phase.mLifecycle.mStatus))
              << "  design=" << ColorizeDesignChars(DesignChars) << " "
              << kColorDim << "(" << GetDesignDepthLabel(DesignChars) << ")"
              << kColorReset << "\n\n";
    (void)InOptions; // current implementation emits full view; flag-driven
                     // subsetting in --brief/--design/--execution is only
                     // applied in the JSON path today.

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
}

// Thin dispatcher for the human renderer. Single-phase calls through to
// the per-phase emitter once; batch mode (--phases) loops and prints a
// separator line between entries.
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

    // --all-phases (v0.105.0+): expand to mPhaseIndices so the existing
    // batch-render loop below handles every phase uniformly. Empty-phase
    // bundles fall through as an empty batch — the loop renders nothing
    // and exits 0.
    std::vector<int> AllIndices;
    const std::vector<int> *EffectiveIndices = &InOptions.mPhaseIndices;
    if (InOptions.mbAllPhases)
    {
        AllIndices.reserve(Bundle.mPhases.size());
        for (size_t I = 0; I < Bundle.mPhases.size(); ++I)
        {
            AllIndices.push_back(static_cast<int>(I));
        }
        EffectiveIndices = &AllIndices;
    }

    if (!EffectiveIndices->empty())
    {
        // Batch mode: bounds-check all indices up front before emitting
        // any output, then render each with a visible separator.
        for (const int Idx : *EffectiveIndices)
        {
            if (Idx < 0 || static_cast<size_t>(Idx) >= Bundle.mPhases.size())
            {
                std::cerr << "Phase index " << Idx << " out of range (0.."
                          << Bundle.mPhases.size() - 1 << ")\n";
                return 1;
            }
        }
        for (size_t N = 0; N < EffectiveIndices->size(); ++N)
        {
            if (N > 0)
                std::cout << kColorDim << "--------------------------------\n\n"
                          << kColorReset;
            EmitPhaseGetHuman(
                Bundle, static_cast<size_t>((*EffectiveIndices)[N]), InOptions);
        }
        return 0;
    }

    // Single-phase path.
    if (InOptions.mPhaseIndex < 0 ||
        static_cast<size_t>(InOptions.mPhaseIndex) >= Bundle.mPhases.size())
    {
        std::cerr << "Phase index " << InOptions.mPhaseIndex
                  << " out of range (0.." << Bundle.mPhases.size() - 1 << ")\n";
        return 1;
    }
    EmitPhaseGetHuman(Bundle, static_cast<size_t>(InOptions.mPhaseIndex),
                      InOptions);
    return 0;
}

// ---------------------------------------------------------------------------
// RunBundlePhaseCommand — dispatch phase list / phase get
// ---------------------------------------------------------------------------

int RunBundlePhaseCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot)
{
    // 3-prologue --help handling (v0.85.0). See DispatchSubcommand<N> in
    // UniPlanCommandDispatch.cpp for the canonical pattern this mirrors.
    if (InArgs.empty())
    {
        PrintCommandUsage(std::cout, "phase");
        return 0;
    }
    const std::string Sub = InArgs[0];
    if (Sub == "--help" || Sub == "-h")
    {
        PrintCommandUsage(std::cout, "phase");
        return 0;
    }
    const std::vector<std::string> SubArgs(InArgs.begin() + 1, InArgs.end());
    if (ContainsHelpFlag(SubArgs))
    {
        PrintCommandUsage(std::cout, "phase", Sub);
        return 0;
    }

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

    if (Sub == "metric")
    {
        const FPhaseMetricOptions Options = ParsePhaseMetricOptions(SubArgs);
        const fs::path RepoRoot = NormalizeRepoRootPath(
            Options.mRepoRoot.empty() ? InRepoRoot : Options.mRepoRoot);
        if (Options.mbHuman)
        {
            return RunBundlePhaseMetricHuman(RepoRoot, Options);
        }
        return RunBundlePhaseMetricJson(RepoRoot, Options);
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

    if (Sub == "board-replace")
    {
        return RunPhaseBoardReplaceCommand(SubArgs, InRepoRoot);
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
    if (Sub == "cancel")
        return RunPhaseCancelCommand(SubArgs, InRepoRoot);
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
    if (Sub == "drift")
        return RunPhaseDriftCommand(SubArgs, InRepoRoot);
    if (Sub == "sync-execution")
        return RunPhaseSyncExecutionCommand(SubArgs, InRepoRoot);

    throw UsageError("Unknown phase subcommand: " + Sub +
                     ". Expected: list, get, set, add, remove, normalize, "
                     "board-replace, start, complete, block, unblock, "
                     "cancel, progress, complete-jobs, log, verify, next, "
                     "readiness, wave-status, drift, sync-execution");
}

} // namespace UniPlan
