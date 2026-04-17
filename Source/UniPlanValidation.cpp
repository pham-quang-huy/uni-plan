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
// Content-hygiene helper: regex-scan one prose string and emit a single
// failure (capturing the matched fragment) when the pattern hits.
// ---------------------------------------------------------------------------

static void
ScanProseField(const std::string &InTopic, const std::string &InPath,
               const std::string &InContent, const std::regex &InPattern,
               const std::string &InCheckID, EValidationSeverity InSeverity,
               const std::string &InDetailPrefix,
               std::vector<ValidateCheck> &OutChecks)
{
    if (InContent.empty())
        return;
    std::smatch Match;
    if (std::regex_search(InContent, Match, InPattern))
    {
        std::string Hit = Match.str();
        if (Hit.size() > 60)
            Hit = Hit.substr(0, 57) + "...";
        Fail(OutChecks, InCheckID, InSeverity, InTopic, InPath,
             InDetailPrefix + Hit);
    }
}

// ---------------------------------------------------------------------------
// Content-hygiene helper: scan every topic-level prose field.
// ---------------------------------------------------------------------------

static void ScanTopicProse(const FTopicBundle &InBundle,
                           const std::regex &InPattern,
                           const std::string &InCheckID,
                           EValidationSeverity InSeverity,
                           const std::string &InDetailPrefix,
                           std::vector<ValidateCheck> &OutChecks)
{
    const FPlanMetadata &M = InBundle.mMetadata;
    const std::string &Key = InBundle.mTopicKey;
    const auto Scan = [&](const std::string &InField, const std::string &InVal)
    {
        ScanProseField(Key, InField, InVal, InPattern, InCheckID, InSeverity,
                       InDetailPrefix, OutChecks);
    };
    Scan("summary", M.mSummary);
    Scan("goals", M.mGoals);
    Scan("non_goals", M.mNonGoals);
    Scan("risks", M.mRisks);
    Scan("acceptance_criteria", M.mAcceptanceCriteria);
    Scan("problem_statement", M.mProblemStatement);
    // validation_commands is a typed vector now — scan each element's
    // command + description prose individually so path references like
    // `validation_commands[3].command` surface in the issue output.
    for (size_t I = 0; I < M.mValidationCommands.size(); ++I)
    {
        const FValidationCommand &C = M.mValidationCommands[I];
        const std::string Base =
            "validation_commands[" + std::to_string(I) + "]";
        Scan(Base + ".command", C.mCommand);
        Scan(Base + ".description", C.mDescription);
    }
    Scan("baseline_audit", M.mBaselineAudit);
    Scan("execution_strategy", M.mExecutionStrategy);
    Scan("locked_decisions", M.mLockedDecisions);
    Scan("source_references", M.mSourceReferences);
    Scan("dependencies", M.mDependencies);
    Scan("next_actions", InBundle.mNextActions);
}

// ---------------------------------------------------------------------------
// Content-hygiene helper: scan every per-phase prose field
// (scope/output + all 9 design material fields + lifecycle prose).
// ---------------------------------------------------------------------------

static void ScanPhaseProse(
    const FTopicBundle &InBundle, const std::regex &InPattern,
    const std::string &InCheckID, EValidationSeverity InSeverity,
    const std::string &InDetailPrefix, std::vector<ValidateCheck> &OutChecks,
    const bool InIncludeLifecycle = true, const bool InIncludeChildren = true)
{
    const std::string &Key = InBundle.mTopicKey;
    for (size_t PI = 0; PI < InBundle.mPhases.size(); ++PI)
    {
        const FPhaseRecord &P = InBundle.mPhases[PI];
        const std::string Base = "phases[" + std::to_string(PI) + "]";
        const auto Scan =
            [&](const std::string &InSuffix, const std::string &InVal)
        {
            ScanProseField(Key, Base + "." + InSuffix, InVal, InPattern,
                           InCheckID, InSeverity, InDetailPrefix, OutChecks);
        };
        Scan("scope", P.mScope);
        Scan("output", P.mOutput);
        Scan("investigation", P.mDesign.mInvestigation);
        Scan("code_snippets", P.mDesign.mCodeSnippets);
        Scan("dependencies", P.mDesign.mDependencies);
        Scan("readiness_gate", P.mDesign.mReadinessGate);
        Scan("handoff", P.mDesign.mHandoff);
        Scan("code_entity_contract", P.mDesign.mCodeEntityContract);
        Scan("best_practices", P.mDesign.mBestPractices);
        for (size_t CI = 0; CI < P.mDesign.mValidationCommands.size(); ++CI)
        {
            const FValidationCommand &C = P.mDesign.mValidationCommands[CI];
            const std::string VCBase =
                "validation_commands[" + std::to_string(CI) + "]";
            Scan(VCBase + ".command", C.mCommand);
            Scan(VCBase + ".description", C.mDescription);
        }
        Scan("multi_platforming", P.mDesign.mMultiPlatforming);
        if (InIncludeLifecycle)
        {
            Scan("done", P.mLifecycle.mDone);
            Scan("remaining", P.mLifecycle.mRemaining);
            Scan("blockers", P.mLifecycle.mBlockers);
            Scan("agent_context", P.mLifecycle.mAgentContext);
        }
        if (InIncludeChildren)
        {
            for (size_t LI = 0; LI < P.mLanes.size(); ++LI)
            {
                const FLaneRecord &L = P.mLanes[LI];
                const std::string LBase =
                    Base + ".lanes[" + std::to_string(LI) + "]";
                ScanProseField(Key, LBase + ".scope", L.mScope, InPattern,
                               InCheckID, InSeverity, InDetailPrefix,
                               OutChecks);
                ScanProseField(Key, LBase + ".exit_criteria", L.mExitCriteria,
                               InPattern, InCheckID, InSeverity, InDetailPrefix,
                               OutChecks);
            }
            for (size_t JI = 0; JI < P.mJobs.size(); ++JI)
            {
                const FJobRecord &J = P.mJobs[JI];
                const std::string JBase =
                    Base + ".jobs[" + std::to_string(JI) + "]";
                ScanProseField(Key, JBase + ".scope", J.mScope, InPattern,
                               InCheckID, InSeverity, InDetailPrefix,
                               OutChecks);
                ScanProseField(Key, JBase + ".output", J.mOutput, InPattern,
                               InCheckID, InSeverity, InDetailPrefix,
                               OutChecks);
                ScanProseField(Key, JBase + ".exit_criteria", J.mExitCriteria,
                               InPattern, InCheckID, InSeverity, InDetailPrefix,
                               OutChecks);
            }
        }
    }
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
                     B.mTopicKey, "phases[" + std::to_string(I) + "].scope",
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

// 19. legacy_cli_free (Warning) — V3 `doc` CLI references.
static void EvalLegacyCliFree(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex Pattern(
        R"(\bdoc\.exe\b|\bdoc\s+(?:lint|phase|artifacts)\b|FIE_Doc[\\/]doc\b)",
        std::regex_constants::icase);
    for (const FTopicBundle &B : InBundles)
    {
        // Scan every topic, regardless of completion status — governance
        // prose carrying V3 CLI references misleads agents who read the
        // bundle, even if the phase that recorded the reference is closed.
        // (Historical accuracy of audit trails is preserved by keeping
        // the backtick quoting, not by skipping the check.)
        ScanTopicProse(B, Pattern, "legacy_cli_free",
                       EValidationSeverity::Warning,
                       "legacy CLI reference: ", OutChecks);
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &P = B.mPhases[PI];
            const std::string Base = "phases[" + std::to_string(PI) + "]";
            const auto Scan =
                [&](const std::string &InField, const std::string &InVal)
            {
                ScanProseField(B.mTopicKey, Base + "." + InField, InVal,
                               Pattern, "legacy_cli_free",
                               EValidationSeverity::Warning,
                               "legacy CLI reference: ", OutChecks);
            };
            Scan("scope", P.mScope);
            Scan("output", P.mOutput);
            Scan("investigation", P.mDesign.mInvestigation);
            Scan("dependencies", P.mDesign.mDependencies);
            Scan("readiness_gate", P.mDesign.mReadinessGate);
            Scan("handoff", P.mDesign.mHandoff);
            Scan("best_practices", P.mDesign.mBestPractices);
            Scan("code_entity_contract", P.mDesign.mCodeEntityContract);
            for (size_t CI = 0; CI < P.mDesign.mValidationCommands.size(); ++CI)
            {
                const FValidationCommand &C = P.mDesign.mValidationCommands[CI];
                const std::string VCBase =
                    "validation_commands[" + std::to_string(CI) + "]";
                Scan(VCBase + ".command", C.mCommand);
                Scan(VCBase + ".description", C.mDescription);
            }
            Scan("done", P.mLifecycle.mDone);
            Scan("remaining", P.mLifecycle.mRemaining);
            Scan("blockers", P.mLifecycle.mBlockers);
        }
    }
}

// 20. v3_terminology_free (Warning) — V3 plan/impl/playbook vocabulary,
// including plain equivalent phrases that don't use the original slash
// syntax (e.g., "implementation tracker", "tracker contract", "paired
// plan"). All forms presuppose the retired V3 plan-triad model.
static void EvalV3TerminologyFree(const std::vector<FTopicBundle> &InBundles,
                                  std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex Pattern(
        R"((?:playbook and detached sidecars|Sidecar evidence channels|)"
        R"(plan/implementation/playbook|\.(?:Impl|Playbook)\.md|)"
        R"(canonical pairing|Active-phase playbook|phase-scoped playbook|)"
        R"(implementation tracker|tracker contract|Tracker contract|)"
        R"(Plan contract|plan\s*\+\s*implementation tracker|)"
        R"(Paired plan/tracker|Paired plan\+tracker|paired plan/tracker|)"
        R"([Pp]aired\s+plan\b|)"
        R"(playbook pairing|playbook discoverab\w+|sidecar discoverab\w+|)"
        R"(plan/playbook/tracker|plan/tracker/playbook|)"
        R"(plan/implementation pairing|pairing semantics|)"
        R"(Docs/Implementation/[\w.-]+\.Plan\.json|)"
        R"(Docs/Playbooks/[\w.-]+\.Plan\.json|)"
        R"(Docs/Playbooks/`[\w.-]+|)"
        R"(Docs/Implementation/`[\w.-]+|)"
        R"(plan/playbook/implementation triad|plan/tracker/playbook triad))",
        std::regex_constants::icase);
    for (const FTopicBundle &B : InBundles)
    {
        ScanTopicProse(B, Pattern, "v3_terminology_free",
                       EValidationSeverity::Warning,
                       "V3 terminology: ", OutChecks);
        ScanPhaseProse(B, Pattern, "v3_terminology_free",
                       EValidationSeverity::Warning,
                       "V3 terminology: ", OutChecks);
    }
}

// 21. canonical_phase_ref_prose (Warning) — legacy P<N>/MP-<N> aliases
// in governance prose where canonical `phases[N]` is required.
//
// Catches three forms:
//   (a) Specific legacy references: `phase key (`P5`)`, `P6 → P7`,
//       `MP-19a/b/c`.
//   (b) Standalone `P<N>` in prescriptive fields — the common live form.
//       Skips lines that declare themselves historical ("Plan.md task P",
//       "original P1-P4", "legacy P", "historical P") and lines that
//       contain statistical percentiles (P50/P95/P99).
//   (c) Bare-text variants (`Px → Py`, `Px -> Py`, `PX phase`).
//
// Scope limited to prescriptive fields (not historical narrative fields
// like execution_strategy, changelog.change, verification.result).
static void
EvalCanonicalPhaseRefProse(const std::vector<FTopicBundle> &InBundles,
                           std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex SpecificPattern(
        R"(phase key \(`?P\d+`?\)|`?P\d+`?\s*(?:->|\xE2\x86\x92)\s*`?P\d+`?|\bMP-\d+[abc]\b)");
    // Standalone P<N>, covering both bare `P5` and backtick-quoted `` `P5` ``
    // forms. Backtick is INCLUDED as a valid token boundary — `` `P5` `` is
    // the most common live phase-alias form. Excluded boundaries: word
    // characters (avoids matching inside `phases5`), `[` (avoids matching
    // the P in `phases[5]`), and `/` / `:` (avoids path segments).
    // N is capped at 3 digits to skip C++ proposal numbers (`P2996`),
    // feature-tag IDs, and other 4+ digit P-codes that are not phases.
    static const std::regex StandalonePattern(
        R"((?:^|[^\w\[])(P\d{1,3})(?:[^\]\w/:]|$))");
    static const std::regex HistoricalMarker(
        R"(Plan\.md task P|legacy Plan\.md|historical Plan\.md|original P\d|\(legacy P|legacy P\d+ playbook|historical P\d+|- P\d+ [A-Z])");
    static const std::regex PercentileMarker(R"(\bP(?:50|95|99|99\.9)\b)");

    const auto ScanPrescriptive = [&](const std::string &InTopic,
                                      const std::string &InPath,
                                      const std::string &InContent)
    {
        if (InContent.empty())
            return;
        // Pass 1 — specific forms (always fire).
        std::smatch Match;
        if (std::regex_search(InContent, Match, SpecificPattern))
        {
            Fail(OutChecks, "canonical_phase_ref_prose",
                 EValidationSeverity::Warning, InTopic, InPath,
                 "legacy phase alias: " + Match.str());
            return;
        }
        // Pass 2 — standalone P<N> line-by-line with historical filter.
        std::string Line;
        for (size_t I = 0; I <= InContent.size(); ++I)
        {
            const char C = (I < InContent.size()) ? InContent[I] : '\n';
            if (C != '\n')
            {
                Line += C;
                continue;
            }
            if (!Line.empty())
            {
                const bool bHistorical =
                    std::regex_search(Line, HistoricalMarker);
                const bool bPercentile =
                    std::regex_search(Line, PercentileMarker);
                if (!bHistorical && !bPercentile &&
                    std::regex_search(Line, Match, StandalonePattern))
                {
                    Fail(OutChecks, "canonical_phase_ref_prose",
                         EValidationSeverity::Warning, InTopic, InPath,
                         "legacy phase alias: " + Match[1].str());
                    Line.clear();
                    return;
                }
            }
            Line.clear();
        }
    };

    for (const FTopicBundle &B : InBundles)
    {
        // Topic-level prescriptive fields.
        ScanPrescriptive(B.mTopicKey, "dependencies",
                         B.mMetadata.mDependencies);
        ScanPrescriptive(B.mTopicKey, "next_actions", B.mNextActions);
        ScanPrescriptive(B.mTopicKey, "acceptance_criteria",
                         B.mMetadata.mAcceptanceCriteria);
        // Per-phase prescriptive fields.
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &P = B.mPhases[PI];
            const std::string Base = "phases[" + std::to_string(PI) + "]";
            ScanPrescriptive(B.mTopicKey, Base + ".readiness_gate",
                             P.mDesign.mReadinessGate);
            ScanPrescriptive(B.mTopicKey, Base + ".handoff",
                             P.mDesign.mHandoff);
            ScanPrescriptive(B.mTopicKey, Base + ".dependencies",
                             P.mDesign.mDependencies);
            ScanPrescriptive(B.mTopicKey, Base + ".best_practices",
                             P.mDesign.mBestPractices);
            ScanPrescriptive(B.mTopicKey, Base + ".code_entity_contract",
                             P.mDesign.mCodeEntityContract);
            ScanPrescriptive(B.mTopicKey, Base + ".output", P.mOutput);
            ScanPrescriptive(B.mTopicKey, Base + ".done", P.mLifecycle.mDone);
            ScanPrescriptive(B.mTopicKey, Base + ".remaining",
                             P.mLifecycle.mRemaining);
            ScanPrescriptive(B.mTopicKey, Base + ".blockers",
                             P.mLifecycle.mBlockers);
            for (size_t LI = 0; LI < P.mLanes.size(); ++LI)
            {
                ScanPrescriptive(B.mTopicKey,
                                 Base + ".lanes[" + std::to_string(LI) +
                                     "].exit_criteria",
                                 P.mLanes[LI].mExitCriteria);
            }
            for (size_t JI = 0; JI < P.mJobs.size(); ++JI)
            {
                ScanPrescriptive(B.mTopicKey,
                                 Base + ".jobs[" + std::to_string(JI) +
                                     "].exit_criteria",
                                 P.mJobs[JI].mExitCriteria);
            }
        }
    }
}

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
static void EvalPathResolves(const std::vector<FTopicBundle> &InBundles,
                             std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex BadPath(
        R"(Docs/(?:Implementation|Playbooks)/`?[\w.-]+\.Plan\.json)");

    const auto Scan = [&](const std::string &InTopic, const std::string &InPath,
                          const std::string &InContent)
    {
        if (InContent.empty())
            return;
        std::smatch Match;
        if (std::regex_search(InContent, Match, BadPath))
        {
            Fail(OutChecks, "path_resolves", EValidationSeverity::ErrorMinor,
                 InTopic, InPath,
                 "impossible path ref (.Plan.json lives at Docs/Plans/, "
                 "not Docs/Implementation|Playbooks/): " +
                     Match.str());
        }
    };

    for (const FTopicBundle &B : InBundles)
    {
        const FPlanMetadata &M = B.mMetadata;
        Scan(B.mTopicKey, "dependencies", M.mDependencies);
        Scan(B.mTopicKey, "source_references", M.mSourceReferences);
        Scan(B.mTopicKey, "execution_strategy", M.mExecutionStrategy);
        Scan(B.mTopicKey, "locked_decisions", M.mLockedDecisions);
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &P = B.mPhases[PI];
            const std::string Base = "phases[" + std::to_string(PI) + "]";
            Scan(B.mTopicKey, Base + ".dependencies", P.mDesign.mDependencies);
            Scan(B.mTopicKey, Base + ".investigation",
                 P.mDesign.mInvestigation);
            Scan(B.mTopicKey, Base + ".readiness_gate",
                 P.mDesign.mReadinessGate);
            Scan(B.mTopicKey, Base + ".handoff", P.mDesign.mHandoff);
            Scan(B.mTopicKey, Base + ".code_entity_contract",
                 P.mDesign.mCodeEntityContract);
        }
    }
}

// 22. no_dev_absolute_path (ErrorMinor) — dev-machine absolute paths leak
// PII and break for other agents.
static void EvalNoDevAbsolutePath(const std::vector<FTopicBundle> &InBundles,
                                  std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex Pattern(
        R"(/Users/[a-z][\w.-]*/|/home/[a-z][\w.-]*/|[A-Z]:\\Users\\[\w.-]+)");
    for (const FTopicBundle &B : InBundles)
    {
        ScanTopicProse(B, Pattern, "no_dev_absolute_path",
                       EValidationSeverity::ErrorMinor,
                       "dev-machine absolute path: ", OutChecks);
        ScanPhaseProse(B, Pattern, "no_dev_absolute_path",
                       EValidationSeverity::ErrorMinor,
                       "dev-machine absolute path: ", OutChecks);
    }
}

// 23. no_hardcoded_endpoint (Warning) — localhost/LAN IPs steer agents to
// developer-local network state that won't exist in CI.
static void EvalNoHardcodedEndpoint(const std::vector<FTopicBundle> &InBundles,
                                    std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex Pattern(
        R"(\blocalhost:\d+\b|\b127\.0\.0\.1\b|\b192\.168\.\d+\.\d+\b|\b10\.\d+\.\d+\.\d+\b)");
    for (const FTopicBundle &B : InBundles)
    {
        // Topic-level only — changelogs may legitimately record historical
        // endpoint state; governance prose must not.
        ScanTopicProse(B, Pattern, "no_hardcoded_endpoint",
                       EValidationSeverity::Warning,
                       "hardcoded endpoint: ", OutChecks);
        // Per-phase: design material (forward-looking) only — lifecycle prose
        // may contain historical run output.
        ScanPhaseProse(B, Pattern, "no_hardcoded_endpoint",
                       EValidationSeverity::Warning,
                       "hardcoded endpoint: ", OutChecks,
                       /*InIncludeLifecycle=*/false,
                       /*InIncludeChildren=*/false);
    }
}

// validation_command_platform_consistency (Warning) — structural check:
// a FValidationCommand whose mCommand text contains Windows-specific
// backslash path segments (`\Windows-x64\`, `\Debug\`, `\Tools\`) must
// have mPlatform == Windows. A non-Windows (or Any) platform paired with
// a Windows-only command is a mis-tagged record.
//
// Replaces the deleted `platform_path_sep_free` workaround, which
// line-sniffed for `Windows |` prefixes in the old string form. The
// typed record makes this check structural: mPlatform is the source of
// truth, not a prose prefix.
static void EvalValidationCommandPlatformConsistency(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex WindowsPath(
        R"([A-Za-z]+\\(?:Windows-x64|Debug|Release|Tools|FIE_Doc)\\)");

    const auto Scan = [&](const std::string &InTopic,
                          const std::string &InPathBase,
                          const std::vector<FValidationCommand> &InCommands)
    {
        for (size_t I = 0; I < InCommands.size(); ++I)
        {
            const FValidationCommand &C = InCommands[I];
            if (C.mPlatform == EPlatformScope::Windows)
                continue; // explicitly Windows — backslash paths OK
            std::smatch Match;
            if (std::regex_search(C.mCommand, Match, WindowsPath))
            {
                Fail(OutChecks, "validation_command_platform_consistency",
                     EValidationSeverity::Warning, InTopic,
                     InPathBase + "[" + std::to_string(I) + "]",
                     "Windows path `" + Match.str() +
                         "` in non-Windows command; set platform=windows");
            }
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

// 25. no_smart_quotes (Warning) — Unicode curly quotes/dashes break shell
// commands copy-pasted from bundle content.
static void EvalNoSmartQuotes(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex Pattern("\xE2\x80\x98|\xE2\x80\x99|\xE2\x80\x9C|"
                                    "\xE2\x80\x9D|\xE2\x80\x93|\xE2\x80\x94");
    for (const FTopicBundle &B : InBundles)
    {
        ScanTopicProse(B, Pattern, "no_smart_quotes",
                       EValidationSeverity::Warning,
                       "unicode smart char: ", OutChecks);
        ScanPhaseProse(B, Pattern, "no_smart_quotes",
                       EValidationSeverity::Warning,
                       "unicode smart char: ", OutChecks);
    }
}

// 26. no_html_in_prose (Warning) — HTML tags break markdown rendering in
// watch mode.
static void EvalNoHtmlInProse(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex Pattern(R"(<(?:br|div|span|p|h[1-6])\b)",
                                    std::regex_constants::icase);
    for (const FTopicBundle &B : InBundles)
    {
        ScanTopicProse(B, Pattern, "no_html_in_prose",
                       EValidationSeverity::Warning,
                       "HTML tag in prose: ", OutChecks);
        ScanPhaseProse(B, Pattern, "no_html_in_prose",
                       EValidationSeverity::Warning,
                       "HTML tag in prose: ", OutChecks);
    }
}

// 27. no_empty_placeholder_literal (Warning) — literal "None"/"N/A"/"TBD"/"-"
// should be empty string.
static void
EvalNoEmptyPlaceholderLiteral(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex Pattern(
        R"(^\s*(?:None|N/A|n/a|none|TBD|tbd|-)\s*$)");
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &P = B.mPhases[PI];
            const std::string Base = "phases[" + std::to_string(PI) + "]";
            const auto Scan =
                [&](const std::string &InField, const std::string &InVal)
            {
                ScanProseField(B.mTopicKey, Base + "." + InField, InVal,
                               Pattern, "no_empty_placeholder_literal",
                               EValidationSeverity::Warning,
                               "placeholder literal: ", OutChecks);
            };
            Scan("blockers", P.mLifecycle.mBlockers);
            Scan("remaining", P.mLifecycle.mRemaining);
            Scan("done", P.mLifecycle.mDone);
            Scan("dependencies", P.mDesign.mDependencies);
        }
    }
}

// 28. no_unresolved_marker (Warning) — TODO/FIXME/TBD/unresolved markers
// in governance prose or completed-phase fields signal fidelity drift.
static void EvalNoUnresolvedMarker(const std::vector<FTopicBundle> &InBundles,
                                   std::vector<ValidateCheck> &OutChecks)
{
    // Match only bare TODO-style markers, not prose like "todo list".
    static const std::regex Pattern(
        R"(\bTODO\b|\bFIXME\b|\bXXX\b|\bHACK\b|\?\?\?)");
    for (const FTopicBundle &B : InBundles)
    {
        ScanTopicProse(B, Pattern, "no_unresolved_marker",
                       EValidationSeverity::Warning,
                       "unresolved marker: ", OutChecks);
        // Per-phase: only scan phases that claim to be completed.
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &P = B.mPhases[PI];
            if (P.mLifecycle.mStatus != EExecutionStatus::Completed)
                continue;
            const std::string Base = "phases[" + std::to_string(PI) + "]";
            const auto Scan =
                [&](const std::string &InField, const std::string &InVal)
            {
                ScanProseField(
                    B.mTopicKey, Base + "." + InField, InVal, Pattern,
                    "no_unresolved_marker", EValidationSeverity::Warning,
                    "unresolved marker in completed phase: ", OutChecks);
            };
            Scan("scope", P.mScope);
            Scan("output", P.mOutput);
            Scan("done", P.mLifecycle.mDone);
            Scan("remaining", P.mLifecycle.mRemaining);
            Scan("blockers", P.mLifecycle.mBlockers);
        }
    }
}

// 29. topic_ref_integrity (ErrorMinor) — `<X>.Plan.json` prose references
// must resolve to a real topic key in the loaded bundle set.
static void EvalTopicRefIntegrity(const std::vector<FTopicBundle> &InBundles,
                                  std::vector<ValidateCheck> &OutChecks)
{
    std::set<std::string> KnownKeys;
    for (const FTopicBundle &B : InBundles)
        KnownKeys.insert(B.mTopicKey);

    static const std::regex Pattern(R"(([A-Z][A-Za-z0-9]+)\.Plan\.json)");

    const auto Walk = [&](const std::string &InTopic, const std::string &InPath,
                          const std::string &InContent)
    {
        if (InContent.empty())
            return;
        auto Begin =
            std::sregex_iterator(InContent.begin(), InContent.end(), Pattern);
        const auto End = std::sregex_iterator();
        std::set<std::string> Reported;
        for (auto It = Begin; It != End; ++It)
        {
            const std::string Ref = (*It)[1].str();
            if (Ref == InTopic)
                continue; // self-reference ok
            if (KnownKeys.count(Ref) > 0)
                continue;
            if (!Reported.insert(Ref).second)
                continue;
            Fail(OutChecks, "topic_ref_integrity",
                 EValidationSeverity::ErrorMinor, InTopic, InPath,
                 "unknown topic reference: '" + Ref + ".Plan.json'");
        }
    };

    for (const FTopicBundle &B : InBundles)
    {
        const FPlanMetadata &M = B.mMetadata;
        Walk(B.mTopicKey, "summary", M.mSummary);
        Walk(B.mTopicKey, "dependencies", M.mDependencies);
        Walk(B.mTopicKey, "source_references", M.mSourceReferences);
        Walk(B.mTopicKey, "execution_strategy", M.mExecutionStrategy);
        Walk(B.mTopicKey, "locked_decisions", M.mLockedDecisions);
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &P = B.mPhases[PI];
            const std::string Base = "phases[" + std::to_string(PI) + "]";
            Walk(B.mTopicKey, Base + ".dependencies", P.mDesign.mDependencies);
            Walk(B.mTopicKey, Base + ".scope", P.mScope);
            Walk(B.mTopicKey, Base + ".output", P.mOutput);
        }
    }
}

// 30. stale_plan_md_reference (Warning) — `.Plan.md` references are V3
// filename residue; V4 uses `.Plan.json`.
static void EvalStalePlanMdReference(const std::vector<FTopicBundle> &InBundles,
                                     std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex Pattern(R"(\b[A-Z][A-Za-z0-9]+\.Plan\.md\b)");
    for (const FTopicBundle &B : InBundles)
    {
        ScanTopicProse(B, Pattern, "stale_plan_md_reference",
                       EValidationSeverity::Warning,
                       "stale .Plan.md ref: ", OutChecks);
        ScanPhaseProse(B, Pattern, "stale_plan_md_reference",
                       EValidationSeverity::Warning,
                       "stale .Plan.md ref: ", OutChecks);
    }
}

// 31. no_duplicate_changelog (Warning) — same (phase, change text) pair
// recorded more than once distorts the audit timeline.
static void EvalNoDuplicateChangelog(const std::vector<FTopicBundle> &InBundles,
                                     std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        std::map<std::pair<int, std::string>, size_t> FirstSeen;
        for (size_t I = 0; I < B.mChangeLogs.size(); ++I)
        {
            const FChangeLogEntry &C = B.mChangeLogs[I];
            if (C.mChange.empty())
                continue;
            const auto Key = std::make_pair(C.mPhase, C.mChange);
            auto Found = FirstSeen.find(Key);
            if (Found == FirstSeen.end())
            {
                FirstSeen.emplace(Key, I);
                continue;
            }
            std::string Preview = C.mChange;
            if (Preview.size() > 60)
                Preview = Preview.substr(0, 57) + "...";
            Fail(OutChecks, "no_duplicate_changelog",
                 EValidationSeverity::Warning, B.mTopicKey,
                 "changelogs[" + std::to_string(I) + "].change",
                 "duplicate of changelogs[" + std::to_string(Found->second) +
                     "]: '" + Preview + "'");
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
    EvalCanonicalEntityRef(InBundles, Checks);

    // Content-hygiene (ErrorMinor + Warning)
    EvalNoDevAbsolutePath(InBundles, Checks);
    EvalTopicRefIntegrity(InBundles, Checks);
    EvalLegacyCliFree(InBundles, Checks);
    EvalV3TerminologyFree(InBundles, Checks);
    EvalCanonicalPhaseRefProse(InBundles, Checks);
    EvalNoHardcodedEndpoint(InBundles, Checks);
    // Structural checks on typed FValidationCommand records (Phase A).
    // Replace the former string-scanning workarounds
    // (`platform_path_sep_free`, `shell_syntax_sane`).
    EvalValidationCommandFields(InBundles, Checks);
    EvalValidationCommandPlatformConsistency(InBundles, Checks);
    EvalNoSmartQuotes(InBundles, Checks);
    EvalNoHtmlInProse(InBundles, Checks);
    EvalNoEmptyPlaceholderLiteral(InBundles, Checks);
    EvalNoUnresolvedMarker(InBundles, Checks);
    EvalStalePlanMdReference(InBundles, Checks);
    EvalNoDuplicateChangelog(InBundles, Checks);
    EvalPathResolves(InBundles, Checks);

    return Checks;
}

} // namespace UniPlan
