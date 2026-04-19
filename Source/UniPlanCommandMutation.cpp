#include "UniPlanCommandMutationCommon.h"
#include "UniPlanEnums.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace UniPlan
{

// ===================================================================
// Mutation commands
// ===================================================================

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

    if (Options.opStatus.has_value())
    {
        const ETopicStatus NewTopicStatus = *Options.opStatus;
        const std::string NewStatusStr = ToString(NewTopicStatus);
        Changes.push_back({"status", {ToString(Bundle.mStatus), NewStatusStr}});
        Desc = "Status → " + NewStatusStr;
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
        const size_t OldCount = Bundle.mMetadata.mDependencies.size();
        const std::string OldDesc = std::to_string(OldCount) + " entries";
        if (Options.mbDependencyClear)
            Bundle.mMetadata.mDependencies.clear();
        for (const FBundleReference &R : Options.mDependencyAdd)
            Bundle.mMetadata.mDependencies.push_back(R);
        const size_t NewCount = Bundle.mMetadata.mDependencies.size();
        const std::string NewDesc = std::to_string(NewCount) + " entries";
        Changes.push_back({"dependencies", {OldDesc, NewDesc}});
        if (Desc.empty())
            Desc = "Updated dependencies (" + OldDesc + " -> " + NewDesc + ")";
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

    if (Options.opStatus.has_value())
    {
        const std::string Old = ToString(Phase.mLifecycle.mStatus);
        const EExecutionStatus NewStatus = *Options.opStatus;
        const std::string NewStatusStr = ToString(NewStatus);
        Changes.push_back({"status", {Old, NewStatusStr}});
        Phase.mLifecycle.mStatus = NewStatus;
        Desc = Target + " → " + NewStatusStr;

        // Resolve the timestamp to write: explicit override (validated
        // at parse time) wins over the auto-stamp (GetUtcNow). The
        // override is intended for migration/repair passes that backfill
        // historical timestamps; without it, `phase set --status` would
        // always stamp "now" and bury real history.
        const std::string AutoUTC = GetUtcNow();
        if (NewStatus == EExecutionStatus::InProgress &&
            Phase.mLifecycle.mStartedAt.empty())
        {
            const std::string &StartedAt =
                Options.mStartedAt.empty() ? AutoUTC : Options.mStartedAt;
            Phase.mLifecycle.mStartedAt = StartedAt;
            Changes.push_back({"started_at", {"", StartedAt}});
        }
        if (NewStatus == EExecutionStatus::Completed)
        {
            // Completed implies both started_at and completed_at. If the
            // phase never captured started_at, require the caller to
            // supply --started-at <iso> explicitly — fabricating one
            // from completed_at or "now" would invent historical data
            // the caller has not provided (Data Fix Gate violation).
            // The normal execution path (`phase start` → `phase
            // complete`, or `phase set --status in_progress` →
            // `... completed`) already stamps started_at at the
            // in_progress transition, so this gate only fires when
            // callers skip straight from not_started to completed.
            if (Phase.mLifecycle.mStartedAt.empty())
            {
                if (Options.mStartedAt.empty())
                {
                    throw UsageError(
                        "phase set --status completed requires --started-at "
                        "<iso> when the phase has no recorded started_at; "
                        "supply the real historical start time, or call "
                        "`phase set --status in_progress` first so the "
                        "started_at stamp is recorded truthfully");
                }
                Phase.mLifecycle.mStartedAt = Options.mStartedAt;
                Changes.push_back({"started_at", {"", Options.mStartedAt}});
            }
            // completed_at defaults to "now" because the caller is
            // invoking the transition right now; that stamp is
            // truthful. Callers backfilling a past completion supply
            // --completed-at explicitly.
            if (Phase.mLifecycle.mCompletedAt.empty())
            {
                const std::string &CompletedAt = Options.mCompletedAt.empty()
                                                     ? AutoUTC
                                                     : Options.mCompletedAt;
                Phase.mLifecycle.mCompletedAt = CompletedAt;
                Changes.push_back({"completed_at", {"", CompletedAt}});
            }
        }
    }

    // Allow timestamp overrides even without a status change — this is
    // the explicit repair path for phases already in the right status
    // but missing/incorrect timestamps. Each non-empty override replaces
    // the existing value unconditionally; operators invoking this path
    // know what they are doing. A single `phase set` call can fix any
    // combination of status + started_at + completed_at.
    if (!Options.mStartedAt.empty() &&
        Phase.mLifecycle.mStartedAt != Options.mStartedAt)
    {
        Changes.push_back(
            {"started_at", {Phase.mLifecycle.mStartedAt, Options.mStartedAt}});
        Phase.mLifecycle.mStartedAt = Options.mStartedAt;
        if (Desc.empty())
            Desc = Target + " updated started_at";
    }
    if (!Options.mCompletedAt.empty() &&
        Phase.mLifecycle.mCompletedAt != Options.mCompletedAt)
    {
        Changes.push_back(
            {"completed_at",
             {Phase.mLifecycle.mCompletedAt, Options.mCompletedAt}});
        Phase.mLifecycle.mCompletedAt = Options.mCompletedAt;
        if (Desc.empty())
            Desc = Target + " updated completed_at";
    }
    if (!Options.mDone.empty())
    {
        Changes.push_back({"done", {Phase.mLifecycle.mDone, Options.mDone}});
        Phase.mLifecycle.mDone = Options.mDone;
        if (Desc.empty())
            Desc = Target + " updated done";
    }
    else if (Options.mbDoneClear && !Phase.mLifecycle.mDone.empty())
    {
        Changes.push_back({"done", {Phase.mLifecycle.mDone, ""}});
        Phase.mLifecycle.mDone.clear();
        if (Desc.empty())
            Desc = Target + " cleared done";
    }
    if (!Options.mRemaining.empty())
    {
        Changes.push_back(
            {"remaining", {Phase.mLifecycle.mRemaining, Options.mRemaining}});
        Phase.mLifecycle.mRemaining = Options.mRemaining;
    }
    else if (Options.mbRemainingClear && !Phase.mLifecycle.mRemaining.empty())
    {
        Changes.push_back({"remaining", {Phase.mLifecycle.mRemaining, ""}});
        Phase.mLifecycle.mRemaining.clear();
        if (Desc.empty())
            Desc = Target + " cleared remaining";
    }
    if (!Options.mBlockers.empty())
    {
        Changes.push_back(
            {"blockers", {Phase.mLifecycle.mBlockers, Options.mBlockers}});
        Phase.mLifecycle.mBlockers = Options.mBlockers;
    }
    else if (Options.mbBlockersClear && !Phase.mLifecycle.mBlockers.empty())
    {
        Changes.push_back({"blockers", {Phase.mLifecycle.mBlockers, ""}});
        Phase.mLifecycle.mBlockers.clear();
        if (Desc.empty())
            Desc = Target + " cleared blockers";
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
        const size_t OldCount = Phase.mDesign.mDependencies.size();
        const std::string OldDesc = std::to_string(OldCount) + " entries";
        if (Options.mbDependencyClear)
            Phase.mDesign.mDependencies.clear();
        for (const FBundleReference &R : Options.mDependencyAdd)
            Phase.mDesign.mDependencies.push_back(R);
        const size_t NewCount = Phase.mDesign.mDependencies.size();
        const std::string NewDesc = std::to_string(NewCount) + " entries";
        Changes.push_back({"dependencies", {OldDesc, NewDesc}});
        if (Desc.empty())
            Desc = Target + " updated dependencies (" + OldDesc + " -> " +
                   NewDesc + ")";
    }
    // origin (durable provenance stamp). Only records a change when the
    // enum value actually differs from the current stamp; idempotent
    // `--origin <same>` calls emit no Change row.
    if (Options.opOrigin.has_value() && Phase.mOrigin != *Options.opOrigin)
    {
        const std::string Old = ToString(Phase.mOrigin);
        const std::string New = ToString(*Options.opOrigin);
        Changes.push_back({"origin", {Old, New}});
        Phase.mOrigin = *Options.opOrigin;
        if (Desc.empty())
            Desc = Target + " stamped origin=" + New;
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

    const EExecutionStatus NewStatus =
        Options.opStatus.value_or(EExecutionStatus::NotStarted);

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

    if (Options.opStatus.has_value())
    {
        const std::string Old = ToString(Job.mStatus);
        const EExecutionStatus NewStatus = *Options.opStatus;
        const std::string NewStatusStr = ToString(NewStatus);
        Changes.push_back({"status", {Old, NewStatusStr}});
        Job.mStatus = NewStatus;
        Desc = Target + " → " + NewStatusStr;

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

    if (Options.opStatus.has_value())
    {
        const std::string Old = ToString(Task.mStatus);
        const EExecutionStatus NewStatus = *Options.opStatus;
        const std::string NewStatusStr = ToString(NewStatus);
        Changes.push_back({"status", {Old, NewStatusStr}});
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
    Entry.mType = Options.mType;

    Bundle.mChangeLogs.push_back(std::move(Entry));

    if (WriteBundleBack(Bundle, RepoRoot, Error) != 0)
    {
        std::cerr << Error << "\n";
        return 1;
    }

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
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

    std::cout << "{\"schema\":" << JSONQuote(kMutationSchema) << ",";
    EmitJsonFieldBool("ok", true);
    EmitJsonField("topic", Options.mTopic);
    EmitJsonField("target", kTargetVerifications);
    EmitJsonFieldSizeT("entry_index", Bundle.mVerifications.size() - 1, false);
    std::cout << "}\n";
    return 0;
}

} // namespace UniPlan
