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


} // namespace UniPlan
