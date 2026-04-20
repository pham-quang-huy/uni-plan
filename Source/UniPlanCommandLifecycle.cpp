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

namespace fs = std::filesystem;

namespace UniPlan
{

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

    // v0.88.0 lifecycle gate: code-bearing phase cannot complete with
    // an empty file_manifest unless the explicit opt-out is set. Closes
    // the authorship-discipline gap at the mutation surface, so drift
    // can't accumulate going forward — every new completed phase has
    // either real manifest evidence or a documented exemption.
    //
    // Predicate matches `EvalFileManifestRequiredForCodePhases`: a phase
    // is "code-bearing" when it declared itself so via populated
    // code_entity_contract OR code_snippets design fields. The gate
    // honors `mbNoFileManifest=true` (with required reason) as the
    // sanctioned escape hatch for taxonomy/doc/governance phases.
    const bool bCodeBearing = !Phase.mDesign.mCodeEntityContract.empty() ||
                              !Phase.mDesign.mCodeSnippets.empty();
    if (bCodeBearing && Phase.mFileManifest.empty() &&
        !Phase.mbNoFileManifest)
    {
        std::cerr
            << "Cannot complete phase " << Options.mPhaseIndex
            << ": code-bearing phase (code_entity_contract or "
               "code_snippets populated) has empty file_manifest. "
               "Backfill via `uni-plan manifest suggest --topic "
            << Options.mTopic << " --phase " << Options.mPhaseIndex
            << " --apply`, or set the explicit opt-out via `phase set "
               "--no-file-manifest=true --no-file-manifest-reason "
               "\"<justification>\"` before re-running phase complete.\n";
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

    // Auto-cascade: topic completes when every phase is terminal
    // (`Completed` or `Canceled` v0.89.0+) AND at least one phase actually
    // shipped (`Completed`). A topic where every phase was canceled
    // delivered nothing, so the caller should decide — don't auto-flip
    // that case to Completed.
    bool AllTerminal = true;
    bool AnyCompleted = false;
    for (const auto &P : Bundle.mPhases)
    {
        const EExecutionStatus S = P.mLifecycle.mStatus;
        if (S != EExecutionStatus::Completed &&
            S != EExecutionStatus::Canceled)
        {
            AllTerminal = false;
            break;
        }
        if (S == EExecutionStatus::Completed)
            AnyCompleted = true;
    }
    if (AllTerminal && AnyCompleted)
    {
        Bundle.mStatus = ETopicStatus::Completed;
        AppendAutoChangelog(Bundle, kTargetPlan,
                            "Topic auto-completed (all phases terminal)");
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
// phase cancel — mark a phase as superseded / won't-execute. Terminal but
// not completed. Reason is REQUIRED and recorded both in the blockers
// field (why the phase is no longer active) and in the auto-changelog.
// Gates: phase must not already be completed or canceled.
// ---------------------------------------------------------------------------

int RunPhaseCancelCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot)
{
    const FPhaseCancelOptions Options = ParsePhaseCancelOptions(InArgs);
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

    // Gates: completed phases cannot be canceled (use `phase set` for
    // historical corrections, with the audit trail that implies). Already-
    // canceled phases are a no-op we treat as a usage error for
    // idempotency discipline — the caller should know the state.
    if (Phase.mLifecycle.mStatus == EExecutionStatus::Completed)
    {
        std::cerr << "Cannot cancel phase " << Options.mPhaseIndex
                  << ": status is completed. Completed work cannot be "
                     "retroactively canceled via the semantic command "
                     "(use raw `phase set --status canceled` with full "
                     "audit-trail awareness if this is truly required)\n";
        return 1;
    }
    if (Phase.mLifecycle.mStatus == EExecutionStatus::Canceled)
    {
        std::cerr << "Cannot cancel phase " << Options.mPhaseIndex
                  << ": status is already canceled\n";
        return 1;
    }

    using Change = std::pair<std::string, std::pair<std::string, std::string>>;
    std::vector<Change> Changes;

    const std::string FromStatus = ToString(Phase.mLifecycle.mStatus);
    Changes.push_back({"status", {FromStatus, "canceled"}});
    Phase.mLifecycle.mStatus = EExecutionStatus::Canceled;
    Changes.push_back(
        {"blockers", {Phase.mLifecycle.mBlockers, Options.mReason}});
    Phase.mLifecycle.mBlockers = Options.mReason;

    AppendAutoChangelog(Bundle, Target,
                        "Phase " + std::to_string(Options.mPhaseIndex) +
                            " canceled: " + Options.mReason);

    // Auto-cascade: mirrors `phase complete` — if canceling this phase
    // leaves every phase terminal (Completed/Canceled) AND at least one
    // shipped, flip the topic to Completed so it exits the "active" pane
    // in watch / `topic list --status in_progress`. If every phase is
    // Canceled (nothing ever shipped), leave the topic alone — the
    // caller should decide whether to `topic complete`, `topic block`,
    // or reactivate a phase.
    bool AllTerminal = true;
    bool AnyCompleted = false;
    for (const auto &P : Bundle.mPhases)
    {
        const EExecutionStatus S = P.mLifecycle.mStatus;
        if (S != EExecutionStatus::Completed &&
            S != EExecutionStatus::Canceled)
        {
            AllTerminal = false;
            break;
        }
        if (S == EExecutionStatus::Completed)
            AnyCompleted = true;
    }
    if (AllTerminal && AnyCompleted &&
        Bundle.mStatus != ETopicStatus::Completed)
    {
        Bundle.mStatus = ETopicStatus::Completed;
        AppendAutoChangelog(Bundle, kTargetPlan,
                            "Topic auto-completed (all phases terminal)");
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

    // Gate: every phase must be terminal (Completed or Canceled v0.89.0+).
    // Canceled phases count as terminal because they will never execute —
    // the topic has no pending work. A topic where every phase is canceled
    // (no Completed) still passes this gate; if that's semantically wrong
    // the caller can `topic block` or reactivate a phase instead.
    std::vector<int> NonTerminal;
    for (size_t I = 0; I < Bundle.mPhases.size(); ++I)
    {
        const EExecutionStatus S = Bundle.mPhases[I].mLifecycle.mStatus;
        if (S != EExecutionStatus::Completed &&
            S != EExecutionStatus::Canceled)
            NonTerminal.push_back(static_cast<int>(I));
    }
    if (!NonTerminal.empty())
    {
        std::cerr << "Cannot complete topic " << Options.mTopic << ": "
                  << NonTerminal.size()
                  << " phase(s) not terminal (completed or canceled): [";
        for (size_t I = 0; I < NonTerminal.size(); ++I)
        {
            if (I > 0)
                std::cerr << ", ";
            std::cerr << NonTerminal[I];
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


} // namespace UniPlan
