#include "UniPlanJsonIO.h"
#include "UniPlanJson.h"
#include "UniPlanSchemaValidation.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// JSON access helpers
// ---------------------------------------------------------------------------

static std::string GetString(const JsonValue &InJson, const std::string &InKey,
                             const std::string &InDefault = "")
{
    if (InJson.contains(InKey) && InJson[InKey].is_string())
        return InJson[InKey].get<std::string>();
    return InDefault;
}

// ---------------------------------------------------------------------------
// Serialization helpers: C++ structs -> JSON
// ---------------------------------------------------------------------------

static JsonValue SerializeChangeLogEntry(const FChangeLogEntry &InEntry)
{
    JsonValue Entry = JsonValue::object();
    if (InEntry.mPhase < 0)
        Entry["phase"] = nullptr;
    else
        Entry["phase"] = InEntry.mPhase;
    Entry["date"] = InEntry.mDate;
    Entry["change"] = InEntry.mChange;
    Entry["affected"] = InEntry.mAffected;
    Entry["type"] = ToString(InEntry.mType);
    Entry["actor"] = ToString(InEntry.mActor);
    return Entry;
}

static JsonValue SerializeVerificationEntry(const FVerificationEntry &InEntry)
{
    JsonValue Entry = JsonValue::object();
    if (InEntry.mPhase < 0)
        Entry["phase"] = nullptr;
    else
        Entry["phase"] = InEntry.mPhase;
    Entry["date"] = InEntry.mDate;
    Entry["check"] = InEntry.mCheck;
    Entry["result"] = InEntry.mResult;
    Entry["detail"] = InEntry.mDetail;
    return Entry;
}

// ---------------------------------------------------------------------------
// V3 domain-driven serialization (plan-v3)
// All record fields always written. No field omission.
// Tasks nested inside jobs. Tracking inline in phases.
// Changelogs/verifications as flat arrays with scope.
// ---------------------------------------------------------------------------

static JsonValue SerializeStringArray(const std::vector<std::string> &InArray)
{
    JsonValue Arr = JsonValue::array();
    for (const std::string &Item : InArray)
        Arr.push_back(Item);
    return Arr;
}

static JsonValue SerializeValidationCommand(const FValidationCommand &InEntry)
{
    JsonValue Entry = JsonValue::object();
    Entry["platform"] = ToString(InEntry.mPlatform);
    Entry["command"] = InEntry.mCommand;
    Entry["description"] = InEntry.mDescription;
    return Entry;
}

static JsonValue
SerializeValidationCommandArray(const std::vector<FValidationCommand> &InArray)
{
    JsonValue Arr = JsonValue::array();
    for (const FValidationCommand &C : InArray)
        Arr.push_back(SerializeValidationCommand(C));
    return Arr;
}

// Backward-compat deserializer: accepts either
//   (a) an array of objects [{platform, command, description}, ...] — canonical
//   (b) a string — legacy markdown-table form; stored as a single
//       FValidationCommand with mPlatform=Any, mCommand=<raw>, mDescription=""
//
// The string form exists so bundles written by older uni-plan versions
// continue to load. The migration script converts string form to array
// form; the CLI always writes array form going forward.
static void
DeserializeValidationCommands(const JsonValue &InParent,
                              const std::string &InKey,
                              std::vector<FValidationCommand> &OutArray)
{
    OutArray.clear();
    if (!InParent.contains(InKey))
        return;
    const JsonValue &V = InParent[InKey];
    if (V.is_array())
    {
        for (const JsonValue &E : V)
        {
            if (!E.is_object())
                continue;
            FValidationCommand C;
            std::string Platform = GetString(E, "platform", "any");
            if (!PlatformScopeFromString(Platform, C.mPlatform))
                C.mPlatform = EPlatformScope::Any;
            C.mCommand = GetString(E, "command");
            C.mDescription = GetString(E, "description");
            OutArray.push_back(std::move(C));
        }
        return;
    }
    if (V.is_string())
    {
        // Legacy string form — preserve raw text as a single record.
        FValidationCommand C;
        C.mPlatform = EPlatformScope::Any;
        C.mCommand = V.get<std::string>();
        OutArray.push_back(std::move(C));
    }
}

static JsonValue SerializeLaneRecord(const FLaneRecord &InLane)
{
    JsonValue Lane = JsonValue::object();
    Lane["status"] = ToString(InLane.mStatus);
    Lane["scope"] = InLane.mScope;
    Lane["exit_criteria"] = InLane.mExitCriteria;
    return Lane;
}

static JsonValue SerializeTaskRecord(const FTaskRecord &InTask)
{
    JsonValue Task = JsonValue::object();
    Task["status"] = ToString(InTask.mStatus);
    Task["description"] = InTask.mDescription;
    Task["evidence"] = InTask.mEvidence;
    Task["notes"] = InTask.mNotes;
    Task["completed_at"] = InTask.mCompletedAt;
    return Task;
}

static JsonValue SerializeJobRecord(const FJobRecord &InJob)
{
    JsonValue Job = JsonValue::object();
    Job["wave"] = InJob.mWave;
    Job["lane"] = InJob.mLane;
    Job["status"] = ToString(InJob.mStatus);
    Job["scope"] = InJob.mScope;
    Job["output"] = InJob.mOutput;
    Job["exit_criteria"] = InJob.mExitCriteria;
    JsonValue Tasks = JsonValue::array();
    for (const FTaskRecord &Task : InJob.mTasks)
        Tasks.push_back(SerializeTaskRecord(Task));
    Job["tasks"] = std::move(Tasks);
    Job["started_at"] = InJob.mStartedAt;
    Job["completed_at"] = InJob.mCompletedAt;
    return Job;
}

static JsonValue SerializeTestingRecord(const FTestingRecord &InTest)
{
    JsonValue Test = JsonValue::object();
    Test["session"] = InTest.mSession;
    Test["actor"] = ToString(InTest.mActor);
    Test["step"] = InTest.mStep;
    Test["action"] = InTest.mAction;
    Test["expected"] = InTest.mExpected;
    Test["evidence"] = InTest.mEvidence;
    return Test;
}

static JsonValue SerializeFileManifestItem(const FFileManifestItem &InItem)
{
    JsonValue Item = JsonValue::object();
    Item["file"] = InItem.mFilePath;
    Item["action"] = ToString(InItem.mAction);
    Item["description"] = InItem.mDescription;
    return Item;
}

static JsonValue SerializePhaseRecord(const FPhaseRecord &InPhase)
{
    JsonValue Phase = JsonValue::object();
    Phase["scope"] = InPhase.mScope;
    Phase["output"] = InPhase.mOutput;

    // Lifecycle
    const FPhaseLifecycle &LC = InPhase.mLifecycle;
    Phase["status"] = ToString(LC.mStatus);
    Phase["done"] = LC.mDone;
    Phase["remaining"] = LC.mRemaining;
    Phase["blockers"] = LC.mBlockers;
    Phase["started_at"] = LC.mStartedAt;
    Phase["completed_at"] = LC.mCompletedAt;
    Phase["agent_context"] = LC.mAgentContext;

    // Execution taxonomy
    JsonValue Lanes = JsonValue::array();
    for (const FLaneRecord &Lane : InPhase.mLanes)
        Lanes.push_back(SerializeLaneRecord(Lane));
    Phase["lanes"] = std::move(Lanes);

    JsonValue Jobs = JsonValue::array();
    for (const FJobRecord &Job : InPhase.mJobs)
        Jobs.push_back(SerializeJobRecord(Job));
    Phase["jobs"] = std::move(Jobs);

    // Evidence
    JsonValue Testing = JsonValue::array();
    for (const FTestingRecord &Test : InPhase.mTesting)
        Testing.push_back(SerializeTestingRecord(Test));
    Phase["testing"] = std::move(Testing);

    JsonValue Manifest = JsonValue::array();
    for (const FFileManifestItem &Item : InPhase.mFileManifest)
        Manifest.push_back(SerializeFileManifestItem(Item));
    Phase["file_manifest"] = std::move(Manifest);

    // Design material
    const FPhaseDesignMaterial &DM = InPhase.mDesign;
    Phase["investigation"] = DM.mInvestigation;
    Phase["code_snippets"] = DM.mCodeSnippets;
    Phase["dependencies"] = DM.mDependencies;
    Phase["readiness_gate"] = DM.mReadinessGate;
    Phase["handoff"] = DM.mHandoff;
    Phase["code_entity_contract"] = DM.mCodeEntityContract;
    Phase["best_practices"] = DM.mBestPractices;
    Phase["validation_commands"] =
        SerializeValidationCommandArray(DM.mValidationCommands);
    Phase["multi_platforming"] = DM.mMultiPlatforming;

    return Phase;
}

static JsonValue SerializeTopicBundleV4(const FTopicBundle &InBundle)
{
    JsonValue Root = JsonValue::object();
    Root["$schema"] = "plan-v4";
    Root["topic"] = InBundle.mTopicKey;
    Root["status"] = ToString(InBundle.mStatus);

    // Plan metadata at root
    const FPlanMetadata &Meta = InBundle.mMetadata;
    Root["title"] = Meta.mTitle;
    Root["summary"] = Meta.mSummary;
    Root["goals"] = Meta.mGoals;
    Root["non_goals"] = Meta.mNonGoals;
    Root["risks"] = Meta.mRisks;
    Root["acceptance_criteria"] = Meta.mAcceptanceCriteria;
    Root["problem_statement"] = Meta.mProblemStatement;
    Root["validation_commands"] =
        SerializeValidationCommandArray(Meta.mValidationCommands);
    Root["baseline_audit"] = Meta.mBaselineAudit;
    Root["execution_strategy"] = Meta.mExecutionStrategy;
    Root["locked_decisions"] = Meta.mLockedDecisions;
    Root["source_references"] = Meta.mSourceReferences;
    Root["dependencies"] = Meta.mDependencies;

    // Phases (with inline tracking, index-based)
    JsonValue Phases = JsonValue::array();
    for (const FPhaseRecord &Phase : InBundle.mPhases)
        Phases.push_back(SerializePhaseRecord(Phase));
    Root["phases"] = std::move(Phases);

    Root["next_actions"] = InBundle.mNextActions;

    // Changelogs (flat array with scope)
    JsonValue ChangeLogs = JsonValue::array();
    for (const FChangeLogEntry &Entry : InBundle.mChangeLogs)
        ChangeLogs.push_back(SerializeChangeLogEntry(Entry));
    Root["changelogs"] = std::move(ChangeLogs);

    // Verifications (flat array with scope)
    JsonValue Verifications = JsonValue::array();
    for (const FVerificationEntry &Entry : InBundle.mVerifications)
        Verifications.push_back(SerializeVerificationEntry(Entry));
    Root["verifications"] = std::move(Verifications);

    return Root;
}

static FChangeLogEntry DeserializeChangeLogEntry(const JsonValue &InJson)
{
    FChangeLogEntry Entry;
    if (InJson.count("phase") && InJson["phase"].is_number())
        Entry.mPhase = InJson["phase"].get<int>();
    Entry.mDate = GetString(InJson, "date");
    Entry.mChange = GetString(InJson, "change");
    Entry.mAffected = GetString(InJson, "affected");
    Entry.mType = ParseChangeTypeLenient(GetString(InJson, "type"));
    Entry.mActor = ParseTestingActorLenient(GetString(InJson, "actor"));
    return Entry;
}

static FVerificationEntry DeserializeVerificationEntry(const JsonValue &InJson)
{
    FVerificationEntry Entry;
    if (InJson.count("phase") && InJson["phase"].is_number())
        Entry.mPhase = InJson["phase"].get<int>();
    Entry.mDate = GetString(InJson, "date");
    Entry.mCheck = GetString(InJson, "check");
    Entry.mResult = GetString(InJson, "result");
    Entry.mDetail = GetString(InJson, "detail");
    return Entry;
}

static bool DeserializeLaneRecordStrict(const JsonValue &InJson,
                                        FLaneRecord &OutLane,
                                        const std::string &InContext,
                                        std::string &OutError)
{
    if (!RequireExecutionStatus(InJson, "status", OutLane.mStatus, InContext,
                                OutError))
        return false;
    if (!RequireString(InJson, "scope", OutLane.mScope, InContext, OutError))
        return false;
    if (!RequireString(InJson, "exit_criteria", OutLane.mExitCriteria,
                       InContext, OutError))
        return false;
    return true;
}

static bool DeserializeTaskRecordStrict(const JsonValue &InJson,
                                        FTaskRecord &OutTask,
                                        const std::string &InContext,
                                        std::string &OutError)
{
    if (!RequireExecutionStatus(InJson, "status", OutTask.mStatus, InContext,
                                OutError))
        return false;
    if (!RequireString(InJson, "description", OutTask.mDescription, InContext,
                       OutError))
        return false;
    if (!RequireString(InJson, "evidence", OutTask.mEvidence, InContext,
                       OutError))
        return false;
    OptionalString(InJson, "notes", OutTask.mNotes);
    OptionalString(InJson, "completed_at", OutTask.mCompletedAt);
    return true;
}

static bool DeserializeJobRecordStrict(const JsonValue &InJson,
                                       FJobRecord &OutJob,
                                       const std::string &InContext,
                                       std::string &OutError)
{
    if (InJson.contains("wave") && InJson["wave"].is_number())
        OutJob.mWave = InJson["wave"].get<int>();
    else
    {
        OutError = InContext + ".wave: expected integer";
        return false;
    }
    if (InJson.contains("lane") && InJson["lane"].is_number())
        OutJob.mLane = InJson["lane"].get<int>();
    else
    {
        OutError = InContext + ".lane: expected integer";
        return false;
    }
    if (!RequireExecutionStatus(InJson, "status", OutJob.mStatus, InContext,
                                OutError))
        return false;
    if (!RequireString(InJson, "scope", OutJob.mScope, InContext, OutError))
        return false;
    OptionalString(InJson, "output", OutJob.mOutput);
    if (!RequireString(InJson, "exit_criteria", OutJob.mExitCriteria, InContext,
                       OutError))
        return false;
    if (!RequireArray(InJson, "tasks", InContext, OutError))
        return false;
    for (size_t I = 0; I < InJson["tasks"].size(); ++I)
    {
        const auto &TJ = InJson["tasks"][I];
        if (!TJ.is_object())
        {
            OutError = InContext + ".tasks[" + std::to_string(I) +
                       "]: expected object";
            return false;
        }
        FTaskRecord Task;
        const std::string TaskCtx =
            InContext + ".tasks[" + std::to_string(I) + "]";
        if (!DeserializeTaskRecordStrict(TJ, Task, TaskCtx, OutError))
            return false;
        OutJob.mTasks.push_back(std::move(Task));
    }
    OptionalString(InJson, "started_at", OutJob.mStartedAt);
    OptionalString(InJson, "completed_at", OutJob.mCompletedAt);
    return true;
}

static bool DeserializeTestingRecordStrict(const JsonValue &InJson,
                                           FTestingRecord &OutTest,
                                           const std::string &InContext,
                                           std::string &OutError)
{
    if (!RequireString(InJson, "session", OutTest.mSession, InContext,
                       OutError))
        return false;
    if (!RequireTestingActor(InJson, "actor", OutTest.mActor, InContext,
                             OutError))
        return false;
    if (!RequireString(InJson, "step", OutTest.mStep, InContext, OutError))
        return false;
    if (!RequireString(InJson, "action", OutTest.mAction, InContext, OutError))
        return false;
    if (!RequireString(InJson, "expected", OutTest.mExpected, InContext,
                       OutError))
        return false;
    if (!RequireString(InJson, "evidence", OutTest.mEvidence, InContext,
                       OutError))
        return false;
    return true;
}

static bool DeserializeFileManifestStrict(const JsonValue &InJson,
                                          FFileManifestItem &OutItem,
                                          const std::string &InContext,
                                          std::string &OutError)
{
    if (!RequireString(InJson, "file", OutItem.mFilePath, InContext, OutError))
        return false;
    if (!RequireFileAction(InJson, "action", OutItem.mAction, InContext,
                           OutError))
        return false;
    if (!RequireString(InJson, "description", OutItem.mDescription, InContext,
                       OutError))
        return false;
    return true;
}

static bool DeserializeChangeLogStrict(const JsonValue &InJson,
                                       FChangeLogEntry &OutEntry,
                                       const std::string &InContext,
                                       std::string &OutError)
{
    if (InJson.count("phase") && InJson["phase"].is_number())
        OutEntry.mPhase = InJson["phase"].get<int>();
    if (!RequireString(InJson, "date", OutEntry.mDate, InContext, OutError))
        return false;
    if (!RequireString(InJson, "change", OutEntry.mChange, InContext, OutError))
        return false;
    OptionalString(InJson, "affected", OutEntry.mAffected);
    OutEntry.mType = ParseChangeTypeLenient(GetString(InJson, "type"));
    OutEntry.mActor = ParseTestingActorLenient(GetString(InJson, "actor"));
    return true;
}

static bool DeserializeVerificationStrict(const JsonValue &InJson,
                                          FVerificationEntry &OutEntry,
                                          const std::string &InContext,
                                          std::string &OutError)
{
    if (InJson.count("phase") && InJson["phase"].is_number())
        OutEntry.mPhase = InJson["phase"].get<int>();
    if (!RequireString(InJson, "date", OutEntry.mDate, InContext, OutError))
        return false;
    if (!RequireString(InJson, "check", OutEntry.mCheck, InContext, OutError))
        return false;
    if (!RequireString(InJson, "result", OutEntry.mResult, InContext, OutError))
        return false;
    if (!RequireString(InJson, "detail", OutEntry.mDetail, InContext, OutError))
        return false;
    return true;
}

static bool DeserializePhaseRecordStrict(const JsonValue &InJson,
                                         FPhaseRecord &OutPhase,
                                         const std::string &InContext,
                                         std::string &OutError)
{
    // Lifecycle — required fields
    FPhaseLifecycle &LC = OutPhase.mLifecycle;
    if (!RequireExecutionStatus(InJson, "status", LC.mStatus, InContext,
                                OutError))
        return false;
    if (!RequireString(InJson, "done", LC.mDone, InContext, OutError))
        return false;
    if (!RequireString(InJson, "remaining", LC.mRemaining, InContext, OutError))
        return false;
    if (!RequireString(InJson, "blockers", LC.mBlockers, InContext, OutError))
        return false;

    // Lifecycle — optional fields
    OptionalString(InJson, "started_at", LC.mStartedAt);
    OptionalString(InJson, "completed_at", LC.mCompletedAt);
    OptionalString(InJson, "agent_context", LC.mAgentContext);

    // Phase core
    OptionalString(InJson, "scope", OutPhase.mScope);
    OptionalString(InJson, "output", OutPhase.mOutput);

    // Design material
    FPhaseDesignMaterial &DM = OutPhase.mDesign;
    OptionalString(InJson, "investigation", DM.mInvestigation);
    OptionalString(InJson, "code_snippets", DM.mCodeSnippets);
    OptionalString(InJson, "dependencies", DM.mDependencies);
    OptionalString(InJson, "readiness_gate", DM.mReadinessGate);
    OptionalString(InJson, "handoff", DM.mHandoff);
    OptionalString(InJson, "code_entity_contract", DM.mCodeEntityContract);
    OptionalString(InJson, "best_practices", DM.mBestPractices);
    DeserializeValidationCommands(InJson, "validation_commands",
                                  DM.mValidationCommands);
    OptionalString(InJson, "multi_platforming", DM.mMultiPlatforming);

    // Optional arrays with strict record validation
    if (InJson.contains("lanes") && InJson["lanes"].is_array())
    {
        for (size_t I = 0; I < InJson["lanes"].size(); ++I)
        {
            const std::string Ctx =
                InContext + ".lanes[" + std::to_string(I) + "]";
            FLaneRecord Lane;
            if (!DeserializeLaneRecordStrict(InJson["lanes"][I], Lane, Ctx,
                                             OutError))
                return false;
            OutPhase.mLanes.push_back(std::move(Lane));
        }
    }

    if (InJson.contains("jobs") && InJson["jobs"].is_array())
    {
        for (size_t I = 0; I < InJson["jobs"].size(); ++I)
        {
            const std::string Ctx =
                InContext + ".jobs[" + std::to_string(I) + "]";
            FJobRecord Job;
            if (!DeserializeJobRecordStrict(InJson["jobs"][I], Job, Ctx,
                                            OutError))
                return false;
            OutPhase.mJobs.push_back(std::move(Job));
        }
    }

    if (InJson.contains("testing") && InJson["testing"].is_array())
    {
        for (size_t I = 0; I < InJson["testing"].size(); ++I)
        {
            const std::string Ctx =
                InContext + ".testing[" + std::to_string(I) + "]";
            FTestingRecord Test;
            if (!DeserializeTestingRecordStrict(InJson["testing"][I], Test, Ctx,
                                                OutError))
                return false;
            OutPhase.mTesting.push_back(std::move(Test));
        }
    }

    if (InJson.contains("file_manifest") && InJson["file_manifest"].is_array())
    {
        for (size_t I = 0; I < InJson["file_manifest"].size(); ++I)
        {
            const std::string Ctx =
                InContext + ".file_manifest[" + std::to_string(I) + "]";
            FFileManifestItem Item;
            if (!DeserializeFileManifestStrict(InJson["file_manifest"][I], Item,
                                               Ctx, OutError))
                return false;
            OutPhase.mFileManifest.push_back(std::move(Item));
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// DeserializeTopicBundleV4 — strict plan-v4 deserializer.
// Rejects missing required fields, invalid enum values, wrong types.
// ---------------------------------------------------------------------------

static bool DeserializeTopicBundleV4(const JsonValue &InRoot,
                                     FTopicBundle &OutBundle,
                                     std::string &OutError)
{
    const std::string Ctx = "plan-v4";

    // Required root fields
    if (!RequireString(InRoot, "topic", OutBundle.mTopicKey, Ctx, OutError))
        return false;
    {
        std::string StatusStr;
        if (!RequireString(InRoot, "status", StatusStr, Ctx, OutError))
            return false;
        if (!TopicStatusFromString(StatusStr, OutBundle.mStatus))
        {
            OutError = "plan-v4.status: invalid value '" + StatusStr +
                       "', expected "
                       "not_started|in_progress|completed|blocked|canceled";
            return false;
        }
    }
    if (!RequireString(InRoot, "title", OutBundle.mMetadata.mTitle, Ctx,
                       OutError))
        return false;

    // Required arrays
    if (!RequireArray(InRoot, "phases", Ctx, OutError))
        return false;
    if (!RequireArray(InRoot, "changelogs", Ctx, OutError))
        return false;
    if (!RequireArray(InRoot, "verifications", Ctx, OutError))
        return false;

    // Optional plan metadata
    FPlanMetadata &Meta = OutBundle.mMetadata;
    OptionalString(InRoot, "summary", Meta.mSummary);
    OptionalString(InRoot, "goals", Meta.mGoals);
    OptionalString(InRoot, "non_goals", Meta.mNonGoals);
    OptionalString(InRoot, "risks", Meta.mRisks);
    OptionalString(InRoot, "acceptance_criteria", Meta.mAcceptanceCriteria);
    OptionalString(InRoot, "problem_statement", Meta.mProblemStatement);
    DeserializeValidationCommands(InRoot, "validation_commands",
                                  Meta.mValidationCommands);
    OptionalString(InRoot, "baseline_audit", Meta.mBaselineAudit);
    OptionalString(InRoot, "execution_strategy", Meta.mExecutionStrategy);
    OptionalString(InRoot, "locked_decisions", Meta.mLockedDecisions);
    OptionalString(InRoot, "source_references", Meta.mSourceReferences);
    OptionalString(InRoot, "dependencies", Meta.mDependencies);
    OptionalString(InRoot, "next_actions", OutBundle.mNextActions);

    // Phases — strict validation per record
    for (size_t I = 0; I < InRoot["phases"].size(); ++I)
    {
        const auto &PJ = InRoot["phases"][I];
        if (!PJ.is_object())
        {
            OutError = "phases[" + std::to_string(I) + "]: expected object";
            return false;
        }
        FPhaseRecord Phase;
        const std::string PhaseCtx = "phases[" + std::to_string(I) + "]";
        if (!DeserializePhaseRecordStrict(PJ, Phase, PhaseCtx, OutError))
            return false;
        OutBundle.mPhases.push_back(std::move(Phase));
    }

    // Changelogs — strict validation per entry
    for (size_t I = 0; I < InRoot["changelogs"].size(); ++I)
    {
        const auto &CJ = InRoot["changelogs"][I];
        if (!CJ.is_object())
        {
            OutError = "changelogs[" + std::to_string(I) + "]: expected object";
            return false;
        }
        FChangeLogEntry Entry;
        const std::string EntryCtx = "changelogs[" + std::to_string(I) + "]";
        if (!DeserializeChangeLogStrict(CJ, Entry, EntryCtx, OutError))
            return false;
        OutBundle.mChangeLogs.push_back(std::move(Entry));
    }

    // Verifications — strict validation per entry
    for (size_t I = 0; I < InRoot["verifications"].size(); ++I)
    {
        const auto &VJ = InRoot["verifications"][I];
        if (!VJ.is_object())
        {
            OutError =
                "verifications[" + std::to_string(I) + "]: expected object";
            return false;
        }
        FVerificationEntry Entry;
        const std::string EntryCtx = "verifications[" + std::to_string(I) + "]";
        if (!DeserializeVerificationStrict(VJ, Entry, EntryCtx, OutError))
            return false;
        OutBundle.mVerifications.push_back(std::move(Entry));
    }

    return true;
}

// ---------------------------------------------------------------------------
// Public API: Topic bundle
// ---------------------------------------------------------------------------

bool TryWriteTopicBundle(const FTopicBundle &InBundle, const fs::path &InPath,
                         std::string &OutError)
{
    try
    {
        const JsonValue Root = SerializeTopicBundleV4(InBundle);
        const std::string Content = Root.dump(2);

        std::ofstream Stream(InPath, std::ios::out | std::ios::trunc);
        if (!Stream.is_open())
        {
            OutError = "Failed to open file for writing: " + InPath.string();
            return false;
        }
        Stream << Content << "\n";
        Stream.close();
        if (Stream.fail())
        {
            OutError = "Write failed: " + InPath.string();
            return false;
        }
        return true;
    }
    catch (const std::exception &Ex)
    {
        OutError = std::string("Bundle write error: ") + Ex.what();
        return false;
    }
}

bool TryReadTopicBundle(const fs::path &InPath, FTopicBundle &OutBundle,
                        std::string &OutError)
{
    try
    {
        std::ifstream Stream(InPath);
        if (!Stream.is_open())
        {
            OutError = "Failed to open file: " + InPath.string();
            return false;
        }

        std::ostringstream Buffer;
        Buffer << Stream.rdbuf();
        const std::string Content = Buffer.str();

        if (Content.empty())
        {
            OutError = "File is empty: " + InPath.string();
            return false;
        }

        const JsonValue Root = JsonValue::parse(Content);

        if (!Root.is_object())
        {
            OutError = "Root is not a JSON object: " + InPath.string();
            return false;
        }

        // Only plan-v4 is supported
        const std::string Schema = GetString(Root, "$schema");
        if (Schema != "plan-v4")
        {
            OutError = "Unsupported schema '" + Schema +
                       "' (expected plan-v4): " + InPath.string();
            return false;
        }
        return DeserializeTopicBundleV4(Root, OutBundle, OutError);
    }
    catch (const JsonValue::parse_error &Ex)
    {
        OutError = std::string("Bundle parse error: ") + Ex.what();
        return false;
    }
    catch (const std::exception &Ex)
    {
        OutError = std::string("Bundle read error: ") + Ex.what();
        return false;
    }
}

// ---------------------------------------------------------------------------
// V1 → V2 domain extraction
// ---------------------------------------------------------------------------

} // namespace UniPlan
