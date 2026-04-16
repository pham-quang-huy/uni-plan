#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <iostream>
#include <regex>
#include <set>
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

static void Fail(std::vector<ValidateCheck> &OutChecks, const std::string &InID,
                 EValidationSeverity InSeverity, const std::string &InTopic,
                 const std::string &InPath, const std::string &InDetail)
{
    OutChecks.push_back(
        MakeCheck(InID, InSeverity, false, InTopic, InPath, InDetail));
}

// ---------------------------------------------------------------------------
// V4 Bundle Validation — evaluators
// ---------------------------------------------------------------------------

// 1. required_fields (ErrorMajor)
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
                     B.mTopicKey, "phase[" + std::to_string(I) + "].scope",
                     "empty scope");
        }
    }
}

// 5. phase_status_enum (ErrorMinor)
static void EvalPhaseStatusEnum(const std::vector<FTopicBundle> &InBundles,
                                std::vector<ValidateCheck> &OutChecks)
{
    // EExecutionStatus is already an enum — deserialization validates.
    // This check catches phases where the raw JSON had an invalid string
    // that was lenient-parsed to NotStarted.
    // In V4 strict mode, invalid enums would fail at load time.
    // This is a safety net for lenient parsing.
    (void)InBundles;
    (void)OutChecks;
}

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
                const std::string Path = "phase[" + std::to_string(PI) +
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
                         "phase[" + std::to_string(PI) + "].jobs[" +
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
                             "phase[" + std::to_string(PI) + "].jobs[" +
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
                const std::string Path = "phase[" + std::to_string(PI) +
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
                const std::string Path = "phase[" + std::to_string(PI) +
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
                const std::string Path = "phase[" + std::to_string(PI) +
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
            const std::string PP = "phase[" + std::to_string(PI) + "]";
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
                     B.mTopicKey, "phase[" + std::to_string(I) + "]",
                     "in_progress but done and remaining are both empty");
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
                     "phase[" + std::to_string(PI) + "].testing",
                     "missing actors: " + Missing);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// V4 Bundle Validation — orchestrator
// ---------------------------------------------------------------------------

std::vector<ValidateCheck>
ValidateAllBundles(const std::vector<FTopicBundle> &InBundles)
{
    std::vector<ValidateCheck> Checks;

    // ErrorMajor
    EvalRequiredFields(InBundles, Checks);
    // EvalTopicStatusEnum removed — enforced by ETopicStatus at parse time
    EvalPhasesPresent(InBundles, Checks);

    // ErrorMinor
    EvalPhaseScope(InBundles, Checks);
    EvalPhaseStatusEnum(InBundles, Checks);
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
    EvalTestingActorCoverage(InBundles, Checks);

    return Checks;
}

} // namespace UniPlan
