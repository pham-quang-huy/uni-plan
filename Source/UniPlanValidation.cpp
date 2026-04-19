#include "UniPlanFileHelpers.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <regex>
#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Lint (still .md-aware — used by watch mode)
// ---------------------------------------------------------------------------

LintResult BuildLintResult(const std::string &InRepoRoot, const bool InQuiet)
{
    const fs::path RepoRoot = NormalizeRepoRootPath(InRepoRoot);
    LintResult Result;
    Result.mGeneratedUtc = GetUtcNow();
    Result.mRepoRoot = ToGenericPath(RepoRoot);

    const std::vector<MarkdownDocument> Docs =
        EnumerateMarkdownDocuments(RepoRoot, Result.mWarnings);
    if (!InQuiet)
        PrintScanInfo(Docs.size());

    for (const MarkdownDocument &Doc : Docs)
    {
        const std::string Name = Doc.mAbsolutePath.filename().string();
        if (!IsAllowedLintFilename(Name))
        {
            AddWarning(Result.mWarnings,
                       "WARN name pattern: " + Doc.mRelativePath);
            Result.mNamePatternWarningCount += 1;
        }

        std::string H1Error;
        const bool HasH1 = HasFirstNonEmptyLineH1(Doc.mAbsolutePath, H1Error);
        if (!H1Error.empty())
        {
            AddWarning(Result.mWarnings,
                       "WARN read failure: " + Doc.mRelativePath + " (" +
                           H1Error + ")");
            continue;
        }
        if (!HasH1)
        {
            AddWarning(Result.mWarnings,
                       "WARN missing H1: " + Doc.mRelativePath);
            Result.mMissingH1WarningCount += 1;
        }
    }

    NormalizeWarnings(Result.mWarnings);
    Result.mWarningCount = static_cast<int>(Result.mWarnings.size());
    return Result;
}

// ---------------------------------------------------------------------------
// V4 Bundle Validation — helpers
// ---------------------------------------------------------------------------

static bool IsValidISODate(const std::string &InValue)
{
    static const std::regex Pattern(R"(^\d{4}-\d{2}-\d{2}$)");
    return std::regex_match(InValue, Pattern);
}

static bool IsValidISOTimestamp(const std::string &InValue)
{
    static const std::regex Pattern(
        R"(^\d{4}-\d{2}-\d{2}(T\d{2}:\d{2}:\d{2}.*)?)");
    return std::regex_match(InValue, Pattern);
}

static ValidateCheck MakeCheck(const std::string &InID,
                               EValidationSeverity InSeverity, bool InOk,
                               const std::string &InTopic,
                               const std::string &InPath,
                               const std::string &InDetail)
{
    ValidateCheck C;
    C.mID = InID;
    C.mSeverity = InSeverity;
    C.mbOk = InOk;
    C.mTopic = InTopic;
    C.mPath = InPath;
    C.mDetail = InDetail;
    return C;
}

void Fail(std::vector<ValidateCheck> &OutChecks, const std::string &InID,
          EValidationSeverity InSeverity, const std::string &InTopic,
          const std::string &InPath, const std::string &InDetail)
{
    OutChecks.push_back(
        MakeCheck(InID, InSeverity, false, InTopic, InPath, InDetail));
}

static void EvalRequiredFields(const std::vector<FTopicBundle> &InBundles,
                               std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        if (B.mTopicKey.empty())
            Fail(OutChecks, "required_fields", EValidationSeverity::ErrorMajor,
                 "", "", "missing topic key");
        else
        {
            if (B.mMetadata.mTitle.empty())
                Fail(OutChecks, "required_fields",
                     EValidationSeverity::ErrorMajor, B.mTopicKey, "title",
                     "missing title");
        }
    }
}

// 2. topic_status_enum — now enforced by ETopicStatus enum at
// deserialization time. No runtime check needed.

// 3. phases_present (ErrorMajor)
static void EvalPhasesPresent(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        if (B.mPhases.empty())
            Fail(OutChecks, "phases_present", EValidationSeverity::ErrorMajor,
                 B.mTopicKey, "phases", "no phases defined");
    }
}

// 4. phase_scope (ErrorMinor)
static void EvalPhaseScope(const std::vector<FTopicBundle> &InBundles,
                           std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mPhases.size(); ++I)
        {
            if (B.mPhases[I].mScope.empty())
                Fail(OutChecks, "phase_scope", EValidationSeverity::ErrorMinor,
                     B.mTopicKey, "phases[" + std::to_string(I) + "].scope",
                     "empty scope");
        }
    }
}

// 5. phase_status_enum (ErrorMinor)
// 6. job_required_fields (ErrorMinor)
static void EvalJobRequiredFields(const std::vector<FTopicBundle> &InBundles,
                                  std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &Phase = B.mPhases[PI];
            for (size_t JI = 0; JI < Phase.mJobs.size(); ++JI)
            {
                const FJobRecord &Job = Phase.mJobs[JI];
                const std::string Path = "phases[" + std::to_string(PI) +
                                         "].jobs[" + std::to_string(JI) + "]";
                if (Job.mScope.empty())
                    Fail(OutChecks, "job_required_fields",
                         EValidationSeverity::ErrorMinor, B.mTopicKey,
                         Path + ".scope", "empty scope");
                if (Job.mExitCriteria.empty())
                    Fail(OutChecks, "job_required_fields",
                         EValidationSeverity::ErrorMinor, B.mTopicKey,
                         Path + ".exit_criteria", "empty exit_criteria");
            }
        }
    }
}

// 7. job_lane_ref (ErrorMinor)
static void EvalJobLaneRef(const std::vector<FTopicBundle> &InBundles,
                           std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &Phase = B.mPhases[PI];
            if (Phase.mLanes.empty())
                continue;
            const int LaneCount = static_cast<int>(Phase.mLanes.size());
            for (size_t JI = 0; JI < Phase.mJobs.size(); ++JI)
            {
                const FJobRecord &Job = Phase.mJobs[JI];
                if (Job.mLane >= LaneCount)
                {
                    Fail(OutChecks, "job_lane_ref",
                         EValidationSeverity::ErrorMinor, B.mTopicKey,
                         "phases[" + std::to_string(PI) + "].jobs[" +
                             std::to_string(JI) + "].lane",
                         "lane " + std::to_string(Job.mLane) +
                             " out of bounds (max " +
                             std::to_string(LaneCount - 1) + ")");
                }
            }
        }
    }
}

// 8. task_required_fields (ErrorMinor)
static void EvalTaskRequiredFields(const std::vector<FTopicBundle> &InBundles,
                                   std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &Phase = B.mPhases[PI];
            for (size_t JI = 0; JI < Phase.mJobs.size(); ++JI)
            {
                const FJobRecord &Job = Phase.mJobs[JI];
                for (size_t TI = 0; TI < Job.mTasks.size(); ++TI)
                {
                    const FTaskRecord &Task = Job.mTasks[TI];
                    if (Task.mDescription.empty())
                    {
                        Fail(OutChecks, "task_required_fields",
                             EValidationSeverity::ErrorMinor, B.mTopicKey,
                             "phases[" + std::to_string(PI) + "].jobs[" +
                                 std::to_string(JI) + "].tasks[" +
                                 std::to_string(TI) + "].description",
                             "empty description");
                    }
                }
            }
        }
    }
}

// 9. lane_required_fields (ErrorMinor)
static void EvalLaneRequiredFields(const std::vector<FTopicBundle> &InBundles,
                                   std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &Phase = B.mPhases[PI];
            for (size_t LI = 0; LI < Phase.mLanes.size(); ++LI)
            {
                const FLaneRecord &Lane = Phase.mLanes[LI];
                const std::string Path = "phases[" + std::to_string(PI) +
                                         "].lanes[" + std::to_string(LI) + "]";
                if (Lane.mScope.empty())
                    Fail(OutChecks, "lane_required_fields",
                         EValidationSeverity::ErrorMinor, B.mTopicKey,
                         Path + ".scope", "empty scope");
                if (Lane.mExitCriteria.empty())
                    Fail(OutChecks, "lane_required_fields",
                         EValidationSeverity::ErrorMinor, B.mTopicKey,
                         Path + ".exit_criteria", "empty exit_criteria");
            }
        }
    }
}

// 10. changelog_phase_ref (ErrorMinor)
static void EvalChangelogPhaseRef(const std::vector<FTopicBundle> &InBundles,
                                  std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        const int PhaseCount = static_cast<int>(B.mPhases.size());
        for (size_t I = 0; I < B.mChangeLogs.size(); ++I)
        {
            const int Phase = B.mChangeLogs[I].mPhase;
            if (Phase >= 0 && Phase >= PhaseCount)
            {
                Fail(OutChecks, "changelog_phase_ref",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     "changelogs[" + std::to_string(I) + "].phase",
                     "phase " + std::to_string(Phase) + " out of bounds (max " +
                         std::to_string(PhaseCount - 1) + ")");
            }
        }
    }
}

// 11. changelog_required_fields (ErrorMinor)
static void
EvalChangelogRequiredFields(const std::vector<FTopicBundle> &InBundles,
                            std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mChangeLogs.size(); ++I)
        {
            const FChangeLogEntry &CL = B.mChangeLogs[I];
            const std::string Path = "changelogs[" + std::to_string(I) + "]";
            if (CL.mDate.empty())
                Fail(OutChecks, "changelog_required_fields",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     Path + ".date", "empty date");
            else if (!IsValidISODate(CL.mDate))
                Fail(OutChecks, "changelog_required_fields",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     Path + ".date", "invalid ISO date: " + CL.mDate);
            if (CL.mChange.empty())
                Fail(OutChecks, "changelog_required_fields",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     Path + ".change", "empty change");
        }
    }
}

// 12. verification_phase_ref (ErrorMinor)
static void EvalVerificationPhaseRef(const std::vector<FTopicBundle> &InBundles,
                                     std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        const int PhaseCount = static_cast<int>(B.mPhases.size());
        for (size_t I = 0; I < B.mVerifications.size(); ++I)
        {
            const int Phase = B.mVerifications[I].mPhase;
            if (Phase >= 0 && Phase >= PhaseCount)
            {
                Fail(OutChecks, "verification_phase_ref",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     "verifications[" + std::to_string(I) + "].phase",
                     "phase " + std::to_string(Phase) + " out of bounds (max " +
                         std::to_string(PhaseCount - 1) + ")");
            }
        }
    }
}

// 13. verification_required_fields (ErrorMinor)
static void
EvalVerificationRequiredFields(const std::vector<FTopicBundle> &InBundles,
                               std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mVerifications.size(); ++I)
        {
            const FVerificationEntry &VE = B.mVerifications[I];
            const std::string Path = "verifications[" + std::to_string(I) + "]";
            if (VE.mCheck.empty())
                Fail(OutChecks, "verification_required_fields",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     Path + ".check", "empty check");
            if (VE.mResult.empty())
                Fail(OutChecks, "verification_required_fields",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     Path + ".result", "empty result");
        }
    }
}

// 14. testing_record_fields (ErrorMinor)
static void EvalTestingRecordFields(const std::vector<FTopicBundle> &InBundles,
                                    std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &Phase = B.mPhases[PI];
            for (size_t TI = 0; TI < Phase.mTesting.size(); ++TI)
            {
                const FTestingRecord &TR = Phase.mTesting[TI];
                const std::string Path = "phases[" + std::to_string(PI) +
                                         "].testing[" + std::to_string(TI) +
                                         "]";
                if (TR.mSession.empty())
                    Fail(OutChecks, "testing_record_fields",
                         EValidationSeverity::ErrorMinor, B.mTopicKey,
                         Path + ".session", "empty session");
                if (TR.mStep.empty())
                    Fail(OutChecks, "testing_record_fields",
                         EValidationSeverity::ErrorMinor, B.mTopicKey,
                         Path + ".step", "empty step");
                if (TR.mAction.empty())
                    Fail(OutChecks, "testing_record_fields",
                         EValidationSeverity::ErrorMinor, B.mTopicKey,
                         Path + ".action", "empty action");
                if (TR.mExpected.empty())
                    Fail(OutChecks, "testing_record_fields",
                         EValidationSeverity::ErrorMinor, B.mTopicKey,
                         Path + ".expected", "empty expected");
            }
        }
    }
}

// 15. file_manifest_fields (ErrorMinor)
static void EvalFileManifestFields(const std::vector<FTopicBundle> &InBundles,
                                   std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &Phase = B.mPhases[PI];
            for (size_t FI = 0; FI < Phase.mFileManifest.size(); ++FI)
            {
                const FFileManifestItem &FM = Phase.mFileManifest[FI];
                const std::string Path = "phases[" + std::to_string(PI) +
                                         "].file_manifest[" +
                                         std::to_string(FI) + "]";
                if (FM.mFilePath.empty())
                    Fail(OutChecks, "file_manifest_fields",
                         EValidationSeverity::ErrorMinor, B.mTopicKey,
                         Path + ".file", "empty file path");
                if (FM.mDescription.empty())
                    Fail(OutChecks, "file_manifest_fields",
                         EValidationSeverity::ErrorMinor, B.mTopicKey,
                         Path + ".description", "empty description");
            }
        }
    }
}

// 16. timestamp_format (ErrorMinor)
static void EvalTimestampFormat(const std::vector<FTopicBundle> &InBundles,
                                std::vector<ValidateCheck> &OutChecks)
{
    auto CheckTS = [&](const std::string &InTopic, const std::string &InPath,
                       const std::string &InValue)
    {
        if (!InValue.empty() && !IsValidISOTimestamp(InValue))
            Fail(OutChecks, "timestamp_format", EValidationSeverity::ErrorMinor,
                 InTopic, InPath, "invalid timestamp: " + InValue);
    };

    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const std::string PP = "phases[" + std::to_string(PI) + "]";
            CheckTS(B.mTopicKey, PP + ".started_at",
                    B.mPhases[PI].mLifecycle.mStartedAt);
            CheckTS(B.mTopicKey, PP + ".completed_at",
                    B.mPhases[PI].mLifecycle.mCompletedAt);

            for (size_t JI = 0; JI < B.mPhases[PI].mJobs.size(); ++JI)
            {
                const std::string JP = PP + ".jobs[" + std::to_string(JI) + "]";
                CheckTS(B.mTopicKey, JP + ".started_at",
                        B.mPhases[PI].mJobs[JI].mStartedAt);
                CheckTS(B.mTopicKey, JP + ".completed_at",
                        B.mPhases[PI].mJobs[JI].mCompletedAt);

                for (size_t TI = 0; TI < B.mPhases[PI].mJobs[JI].mTasks.size();
                     ++TI)
                {
                    CheckTS(B.mTopicKey,
                            JP + ".tasks[" + std::to_string(TI) +
                                "].completed_at",
                            B.mPhases[PI].mJobs[JI].mTasks[TI].mCompletedAt);
                }
            }
        }
    }
}

// 17. phase_tracking (Warning)
static void EvalPhaseTracking(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mPhases.size(); ++I)
        {
            const FPhaseRecord &Phase = B.mPhases[I];
            if (Phase.mLifecycle.mStatus == EExecutionStatus::InProgress &&
                Phase.mLifecycle.mDone.empty() &&
                Phase.mLifecycle.mRemaining.empty())
            {
                Fail(OutChecks, "phase_tracking", EValidationSeverity::Warning,
                     B.mTopicKey, "phases[" + std::to_string(I) + "]",
                     "in_progress but done and remaining are both empty");
            }
        }
    }
}

// 17a. topic_phase_status_alignment (Warning) — topic-level status must be
// supportable by phase-level statuses. The lifecycle commands (`topic
// complete`) already gate this at mutation time, but direct JSON writes
// and legacy migrations can desync the two axes. This check catches that
// drift post-load, regardless of how the bundle was produced.
//
// Rules enforced:
//   topic.status == completed    => every phase.status must be completed
//   topic.status == not_started  => every phase.status must be not_started
//   topic.status == in_progress  => at least one phase must be started
//                                   (in_progress, completed, or blocked)
static void
EvalTopicPhaseStatusAlignment(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        if (B.mPhases.empty())
            continue;
        const ETopicStatus TopicStatus = B.mStatus;

        int CompletedCount = 0;
        int NotStartedCount = 0;
        int StartedCount = 0; // in_progress / completed / blocked
        for (const FPhaseRecord &P : B.mPhases)
        {
            switch (P.mLifecycle.mStatus)
            {
            case EExecutionStatus::Completed:
                ++CompletedCount;
                ++StartedCount;
                break;
            case EExecutionStatus::NotStarted:
                ++NotStartedCount;
                break;
            case EExecutionStatus::InProgress:
            case EExecutionStatus::Blocked:
                ++StartedCount;
                break;
            }
        }
        const int Total = static_cast<int>(B.mPhases.size());

        if (TopicStatus == ETopicStatus::Completed && CompletedCount != Total)
        {
            Fail(OutChecks, "topic_phase_status_alignment",
                 EValidationSeverity::Warning, B.mTopicKey, "status",
                 "topic status=completed but " +
                     std::to_string(Total - CompletedCount) + " of " +
                     std::to_string(Total) + " phase(s) are not completed (" +
                     std::to_string(NotStartedCount) + " not_started)");
        }
        else if (TopicStatus == ETopicStatus::NotStarted &&
                 NotStartedCount != Total)
        {
            Fail(OutChecks, "topic_phase_status_alignment",
                 EValidationSeverity::Warning, B.mTopicKey, "status",
                 "topic status=not_started but " +
                     std::to_string(Total - NotStartedCount) +
                     " phase(s) have progressed past not_started");
        }
        else if (TopicStatus == ETopicStatus::InProgress && StartedCount == 0)
        {
            Fail(OutChecks, "topic_phase_status_alignment",
                 EValidationSeverity::Warning, B.mTopicKey, "status",
                 "topic status=in_progress but no phase has been started");
        }
    }
}

// 17b. completed_phase_timestamp_required (Warning) — a phase marked
// completed must carry both started_at and completed_at, otherwise the
// evidence trail has a hole. `EvalTimestampFormat` validates format of
// non-empty timestamps but ignores null values — that exposes a gap
// where a migration can bulk-flip status without stamping timestamps.
// This check closes that gap.
//
// Symmetric rule for in_progress: started_at must be present. A phase
// cannot have progressed past not_started without its start stamp.
static void
EvalCompletedPhaseTimestampRequired(const std::vector<FTopicBundle> &InBundles,
                                    std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mPhases.size(); ++I)
        {
            const FPhaseRecord &Phase = B.mPhases[I];
            const std::string Base = "phases[" + std::to_string(I) + "]";
            if (Phase.mLifecycle.mStatus == EExecutionStatus::Completed)
            {
                if (Phase.mLifecycle.mStartedAt.empty())
                    Fail(OutChecks, "completed_phase_timestamp_required",
                         EValidationSeverity::Warning, B.mTopicKey,
                         Base + ".started_at",
                         "phase status=completed but started_at is empty");
                if (Phase.mLifecycle.mCompletedAt.empty())
                    Fail(OutChecks, "completed_phase_timestamp_required",
                         EValidationSeverity::Warning, B.mTopicKey,
                         Base + ".completed_at",
                         "phase status=completed but completed_at is empty");
            }
            else if (Phase.mLifecycle.mStatus == EExecutionStatus::InProgress ||
                     Phase.mLifecycle.mStatus == EExecutionStatus::Blocked)
            {
                if (Phase.mLifecycle.mStartedAt.empty())
                    Fail(OutChecks, "completed_phase_timestamp_required",
                         EValidationSeverity::Warning, B.mTopicKey,
                         Base + ".started_at",
                         "phase status=" +
                             std::string(ToString(Phase.mLifecycle.mStatus)) +
                             " but started_at is empty");
            }
        }
    }
}

// 18. testing_actor_coverage (Warning)
static void EvalTestingActorCoverage(const std::vector<FTopicBundle> &InBundles,
                                     std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &Phase = B.mPhases[PI];
            if (Phase.mTesting.empty())
                continue;
            if (Phase.mLifecycle.mStatus != EExecutionStatus::InProgress &&
                Phase.mLifecycle.mStatus != EExecutionStatus::Blocked)
            {
                continue;
            }

            bool bHasHuman = false;
            bool bHasAI = false;
            for (const FTestingRecord &TR : Phase.mTesting)
            {
                if (TR.mActor == ETestingActor::Human)
                    bHasHuman = true;
                else if (TR.mActor == ETestingActor::AI)
                    bHasAI = true;
                else if (TR.mActor == ETestingActor::Automated)
                {
                    bHasHuman = true;
                    bHasAI = true;
                }
            }

            std::string Missing;
            if (!bHasHuman)
                Missing += "human";
            if (!bHasAI)
            {
                if (!Missing.empty())
                    Missing += ",";
                Missing += "ai";
            }
            if (!Missing.empty())
            {
                Fail(OutChecks, "testing_actor_coverage",
                     EValidationSeverity::Warning, B.mTopicKey,
                     "phases[" + std::to_string(PI) + "].testing",
                     "missing actors: " + Missing);
            }
        }
    }
}

// canonical_entity_ref (ErrorMinor)
// Flag `affected` fields on changelog entries that use legacy plural
// container form (e.g. "phases[0]", "jobs[2]") or otherwise don't match
// the canonical path grammar. Empty `affected` is allowed (topic-level).
static bool IsCanonicalAffectedRef(const std::string &InValue)
{
    if (InValue.empty())
        return true;
    if (InValue == "plan")
        return true;
    // Canonical forms:
    //   phases[N](.lanes[M] | .jobs[M](.tasks[K])? | .testing[M] |
    //             .file_manifest[M])?(.<field>)?
    //   changelogs[N] | verifications[N]
    static const std::regex kCanonical(
        R"(^(plan|changelogs\[\d+\]|verifications\[\d+\]|phases\[\d+\](\.(lanes|jobs|testing|file_manifest)\[\d+\](\.tasks\[\d+\])?)?(\.[a-z_]+)?)$)");
    return std::regex_match(InValue, kCanonical);
}

static void EvalCanonicalEntityRef(const std::vector<FTopicBundle> &InBundles,
                                   std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mChangeLogs.size(); ++I)
        {
            const std::string &A = B.mChangeLogs[I].mAffected;
            if (!IsCanonicalAffectedRef(A))
            {
                std::string Detail = "non-canonical affected ref: '" + A + "'";
                // Legacy singular form: "phase[N]..." (not "phases[N]...").
                if (A.compare(0, 6, "phase[") == 0)
                    Detail += " (legacy singular form — use plural phases[N])";
                Fail(OutChecks, "canonical_entity_ref",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     "changelogs[" + std::to_string(I) + "].affected", Detail);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// V4 Bundle Validation — content-hygiene evaluators
//
// These 13 evaluators inspect the *content* of prose string fields (not just
// structural integrity). Each uses the ScanProseField / ScanTopicProse /
// ScanPhaseProse helpers above to emit one issue per offending field.
// ---------------------------------------------------------------------------

// validation_command_fields (ErrorMinor) — structural check that each
// FValidationCommand record has a non-empty mCommand. mDescription is
// advisory (Warning if empty) since a description aids humans but is not
// strictly required.
//
// Replaces the deleted `shell_syntax_sane` workaround. That workaround
// existed only to detect self-inflicted `\\ → /` corruption in the
// string-form field; the typed vector makes that class of corruption
// structurally impossible (mCommand is opaque to any bulk regex).
static void
EvalValidationCommandFields(const std::vector<FTopicBundle> &InBundles,
                            std::vector<ValidateCheck> &OutChecks)
{
    const auto Scan = [&](const std::string &InTopic,
                          const std::string &InPathBase,
                          const std::vector<FValidationCommand> &InCommands)
    {
        for (size_t I = 0; I < InCommands.size(); ++I)
        {
            const FValidationCommand &C = InCommands[I];
            const std::string Base = InPathBase + "[" + std::to_string(I) + "]";
            if (C.mCommand.empty())
                Fail(OutChecks, "validation_command_fields",
                     EValidationSeverity::ErrorMinor, InTopic,
                     Base + ".command", "empty command");
            if (C.mDescription.empty())
                Fail(OutChecks, "validation_command_fields",
                     EValidationSeverity::Warning, InTopic,
                     Base + ".description", "empty description");
        }
    };
    for (const FTopicBundle &B : InBundles)
    {
        Scan(B.mTopicKey, "validation_commands",
             B.mMetadata.mValidationCommands);
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            Scan(B.mTopicKey,
                 "phases[" + std::to_string(PI) + "].validation_commands",
                 B.mPhases[PI].mDesign.mValidationCommands);
        }
    }
}

// 33. path_resolves (ErrorMinor) — flags path references in prose that
// cannot possibly resolve. The most common residual case: V4 bundles
// live at `Docs/Plans/<Topic>.Plan.json`, but corrupted rewrites kept
// the old `Docs/Implementation/` or `Docs/Playbooks/` prefix with a
// `.Plan.json` suffix — pointing at files that do not exist.

// ---------------------------------------------------------------------------
// V4 Bundle Validation — orchestrator
// ---------------------------------------------------------------------------

// Emits one `legacy_source_path_resolves` warning per topic- or phase-level
// `legacy_sources[].path` that does not resolve on disk relative to
// `InRepoRoot`. When `InRepoRoot` is empty (e.g. fixture-only tests),
// skips the filesystem check entirely. Kept static-local because it
// depends on filesystem state the other evaluators do not use.
static void
EvalLegacySourcePathResolves(const std::vector<FTopicBundle> &InBundles,
                             const fs::path &InRepoRoot,
                             std::vector<ValidateCheck> &OutChecks)
{
    if (InRepoRoot.empty())
    {
        return;
    }
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mLegacySources.size(); ++I)
        {
            const FLegacyMdSource &S = B.mLegacySources[I];
            if (S.mPath.empty() || ManifestPathExists(InRepoRoot, S.mPath))
            {
                continue;
            }
            Fail(OutChecks, "legacy_source_path_resolves",
                 EValidationSeverity::Warning, B.mTopicKey,
                 "legacy_sources[" + std::to_string(I) + "].path",
                 "legacy source path does not resolve on disk (kind=" +
                     std::string(ToString(S.mKind)) + "): " + S.mPath);
        }
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &P = B.mPhases[PI];
            for (size_t I = 0; I < P.mLegacySources.size(); ++I)
            {
                const FLegacyMdSource &S = P.mLegacySources[I];
                if (S.mPath.empty() || ManifestPathExists(InRepoRoot, S.mPath))
                {
                    continue;
                }
                Fail(OutChecks, "legacy_source_path_resolves",
                     EValidationSeverity::Warning, B.mTopicKey,
                     "phases[" + std::to_string(PI) + "].legacy_sources[" +
                         std::to_string(I) + "].path",
                     "legacy source path does not resolve on disk (kind=" +
                         std::string(ToString(S.mKind)) + "): " + S.mPath);
            }
        }
    }
}

std::vector<ValidateCheck>
ValidateAllBundles(const std::vector<FTopicBundle> &InBundles,
                   const fs::path &InRepoRoot)
{
    std::vector<ValidateCheck> Checks;

    // ErrorMajor
    EvalRequiredFields(InBundles, Checks);
    // EvalTopicStatusEnum removed — enforced by ETopicStatus at parse time
    EvalPhasesPresent(InBundles, Checks);

    // ErrorMinor
    EvalPhaseScope(InBundles, Checks);
    // phase_status_enum removed in v0.69.0 - EExecutionStatus is enforced
    // at JSON parse time; the separate post-parse check was a no-op.
    EvalJobRequiredFields(InBundles, Checks);
    EvalJobLaneRef(InBundles, Checks);
    EvalTaskRequiredFields(InBundles, Checks);
    EvalLaneRequiredFields(InBundles, Checks);
    EvalChangelogPhaseRef(InBundles, Checks);
    EvalChangelogRequiredFields(InBundles, Checks);
    EvalVerificationPhaseRef(InBundles, Checks);
    EvalVerificationRequiredFields(InBundles, Checks);
    EvalTestingRecordFields(InBundles, Checks);
    EvalFileManifestFields(InBundles, Checks);
    EvalTimestampFormat(InBundles, Checks);

    // Warning
    EvalPhaseTracking(InBundles, Checks);
    EvalTopicPhaseStatusAlignment(InBundles, Checks);
    EvalCompletedPhaseTimestampRequired(InBundles, Checks);
    EvalTestingActorCoverage(InBundles, Checks);
    EvalCanonicalEntityRef(InBundles, Checks);

    // Content-hygiene (ErrorMinor + Warning)
    EvalNoDevAbsolutePath(InBundles, Checks);
    EvalTopicRefIntegrity(InBundles, Checks);
    EvalNoHardcodedEndpoint(InBundles, Checks);
    // Structural checks on typed FValidationCommand records (Phase A).
    EvalValidationCommandFields(InBundles, Checks);
    EvalValidationCommandPlatformConsistency(InBundles, Checks);
    EvalNoSmartQuotes(InBundles, Checks);
    EvalNoHtmlInProse(InBundles, Checks);
    EvalNoEmptyPlaceholderLiteral(InBundles, Checks);
    EvalTopicFieldsNotIdentical(InBundles, Checks);
    EvalNoDegenerateDependencyEntry(InBundles, Checks);
    EvalNoUnresolvedMarker(InBundles, Checks);
    EvalNoDuplicateChangelog(InBundles, Checks);
    EvalNoDuplicatePhaseField(InBundles, Checks);
    EvalNoHollowCompletedPhase(InBundles, Checks);
    EvalPathResolves(InBundles, Checks);
    EvalLegacySourcePathResolves(InBundles, InRepoRoot, Checks);

    return Checks;
}

} // namespace UniPlan
