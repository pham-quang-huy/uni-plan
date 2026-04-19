#include "UniPlanJSONIO.h"
#include "UniPlanJSON.h"
#include "UniPlanSchemaValidation.h"

#include <fstream>
#include <sstream>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// JSON access helpers
// ---------------------------------------------------------------------------

static std::string GetString(const JSONValue &InJson, const std::string &InKey,
                             const std::string &InDefault = "")
{
    if (InJson.contains(InKey) && InJson[InKey].is_string())
        return InJson[InKey].get<std::string>();
    return InDefault;
}

// ---------------------------------------------------------------------------
// Serialization helpers: C++ structs -> JSON
// ---------------------------------------------------------------------------

static JSONValue SerializeChangeLogEntry(const FChangeLogEntry &InEntry)
{
    JSONValue Entry = JSONValue::object();
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

static JSONValue SerializeVerificationEntry(const FVerificationEntry &InEntry)
{
    JSONValue Entry = JSONValue::object();
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

static JSONValue SerializeStringArray(const std::vector<std::string> &InArray)
{
    JSONValue Arr = JSONValue::array();
    for (const std::string &Item : InArray)
        Arr.push_back(Item);
    return Arr;
}

static JSONValue SerializeValidationCommand(const FValidationCommand &InEntry)
{
    JSONValue Entry = JSONValue::object();
    Entry["platform"] = ToString(InEntry.mPlatform);
    Entry["command"] = InEntry.mCommand;
    Entry["description"] = InEntry.mDescription;
    return Entry;
}

static JSONValue
SerializeValidationCommandArray(const std::vector<FValidationCommand> &InArray)
{
    JSONValue Arr = JSONValue::array();
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
DeserializeValidationCommands(const JSONValue &InParent,
                              const std::string &InKey,
                              std::vector<FValidationCommand> &OutArray)
{
    OutArray.clear();
    if (!InParent.contains(InKey))
        return;
    const JSONValue &V = InParent[InKey];
    if (V.is_array())
    {
        for (const JSONValue &E : V)
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

// ---------------------------------------------------------------------------
// FBundleReference serialization — typed dependency records.
// Same shape as FValidationCommand: serialize as array-of-objects;
// deserialize accepts canonical array OR legacy string form (string form
// is preserved as a single External record with mNote=<raw>).
// ---------------------------------------------------------------------------

static JSONValue SerializeBundleReference(const FBundleReference &InEntry)
{
    JSONValue Entry = JSONValue::object();
    Entry["kind"] = ToString(InEntry.mKind);
    Entry["topic"] = InEntry.mTopic;
    if (InEntry.mPhase < 0)
        Entry["phase"] = nullptr;
    else
        Entry["phase"] = InEntry.mPhase;
    Entry["path"] = InEntry.mPath;
    Entry["note"] = InEntry.mNote;
    return Entry;
}

static JSONValue
SerializeBundleReferenceArray(const std::vector<FBundleReference> &InArray)
{
    JSONValue Arr = JSONValue::array();
    for (const FBundleReference &R : InArray)
        Arr.push_back(SerializeBundleReference(R));
    return Arr;
}

static void DeserializeBundleReferences(const JSONValue &InParent,
                                        const std::string &InKey,
                                        std::vector<FBundleReference> &OutArray)
{
    OutArray.clear();
    if (!InParent.contains(InKey))
        return;
    const JSONValue &V = InParent[InKey];
    if (V.is_array())
    {
        for (const JSONValue &E : V)
        {
            if (!E.is_object())
                continue;
            FBundleReference R;
            std::string Kind = GetString(E, "kind", "bundle");
            if (!DependencyKindFromString(Kind, R.mKind))
                R.mKind = EDependencyKind::Bundle;
            R.mTopic = GetString(E, "topic");
            if (E.contains("phase") && E["phase"].is_number())
                R.mPhase = E["phase"].get<int>();
            else
                R.mPhase = -1;
            R.mPath = GetString(E, "path");
            R.mNote = GetString(E, "note");
            OutArray.push_back(std::move(R));
        }
        return;
    }
    if (V.is_string())
    {
        const std::string Raw = V.get<std::string>();
        if (Raw.empty())
            return;
        FBundleReference R;
        R.mKind = EDependencyKind::External;
        R.mNote = Raw;
        OutArray.push_back(std::move(R));
    }
}

static JSONValue SerializeLaneRecord(const FLaneRecord &InLane)
{
    JSONValue Lane = JSONValue::object();
    Lane["status"] = ToString(InLane.mStatus);
    Lane["scope"] = InLane.mScope;
    Lane["exit_criteria"] = InLane.mExitCriteria;
    return Lane;
}

static JSONValue SerializeTaskRecord(const FTaskRecord &InTask)
{
    JSONValue Task = JSONValue::object();
    Task["status"] = ToString(InTask.mStatus);
    Task["description"] = InTask.mDescription;
    Task["evidence"] = InTask.mEvidence;
    Task["notes"] = InTask.mNotes;
    Task["completed_at"] = InTask.mCompletedAt;
    return Task;
}

static JSONValue SerializeJobRecord(const FJobRecord &InJob)
{
    JSONValue Job = JSONValue::object();
    Job["wave"] = InJob.mWave;
    Job["lane"] = InJob.mLane;
    Job["status"] = ToString(InJob.mStatus);
    Job["scope"] = InJob.mScope;
    Job["output"] = InJob.mOutput;
    Job["exit_criteria"] = InJob.mExitCriteria;
    JSONValue Tasks = JSONValue::array();
    for (const FTaskRecord &Task : InJob.mTasks)
        Tasks.push_back(SerializeTaskRecord(Task));
    Job["tasks"] = std::move(Tasks);
    Job["started_at"] = InJob.mStartedAt;
    Job["completed_at"] = InJob.mCompletedAt;
    return Job;
}

static JSONValue SerializeTestingRecord(const FTestingRecord &InTest)
{
    JSONValue Test = JSONValue::object();
    Test["session"] = InTest.mSession;
    Test["actor"] = ToString(InTest.mActor);
    Test["step"] = InTest.mStep;
    Test["action"] = InTest.mAction;
    Test["expected"] = InTest.mExpected;
    Test["evidence"] = InTest.mEvidence;
    return Test;
}

static JSONValue SerializeFileManifestItem(const FFileManifestItem &InItem)
{
    JSONValue Item = JSONValue::object();
    Item["file"] = InItem.mFilePath;
    Item["action"] = ToString(InItem.mAction);
    Item["description"] = InItem.mDescription;
    return Item;
}

static JSONValue SerializePhaseRecord(const FPhaseRecord &InPhase)
{
    JSONValue Phase = JSONValue::object();
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
    JSONValue Lanes = JSONValue::array();
    for (const FLaneRecord &Lane : InPhase.mLanes)
        Lanes.push_back(SerializeLaneRecord(Lane));
    Phase["lanes"] = std::move(Lanes);

    JSONValue Jobs = JSONValue::array();
    for (const FJobRecord &Job : InPhase.mJobs)
        Jobs.push_back(SerializeJobRecord(Job));
    Phase["jobs"] = std::move(Jobs);

    // Evidence
    JSONValue Testing = JSONValue::array();
    for (const FTestingRecord &Test : InPhase.mTesting)
        Testing.push_back(SerializeTestingRecord(Test));
    Phase["testing"] = std::move(Testing);

    JSONValue Manifest = JSONValue::array();
    for (const FFileManifestItem &Item : InPhase.mFileManifest)
        Manifest.push_back(SerializeFileManifestItem(Item));
    Phase["file_manifest"] = std::move(Manifest);

    // Design material
    const FPhaseDesignMaterial &DM = InPhase.mDesign;
    Phase["investigation"] = DM.mInvestigation;
    Phase["code_snippets"] = DM.mCodeSnippets;
    Phase["dependencies"] = SerializeBundleReferenceArray(DM.mDependencies);
    Phase["readiness_gate"] = DM.mReadinessGate;
    Phase["handoff"] = DM.mHandoff;
    Phase["code_entity_contract"] = DM.mCodeEntityContract;
    Phase["best_practices"] = DM.mBestPractices;
    Phase["validation_commands"] =
        SerializeValidationCommandArray(DM.mValidationCommands);
    Phase["multi_platforming"] = DM.mMultiPlatforming;

    return Phase;
}

static JSONValue SerializeTopicBundleV4(const FTopicBundle &InBundle)
{
    JSONValue Root = JSONValue::object();
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
    Root["dependencies"] = SerializeBundleReferenceArray(Meta.mDependencies);

    // Phases (with inline tracking, index-based)
    JSONValue Phases = JSONValue::array();
    for (const FPhaseRecord &Phase : InBundle.mPhases)
        Phases.push_back(SerializePhaseRecord(Phase));
    Root["phases"] = std::move(Phases);

    Root["next_actions"] = InBundle.mNextActions;

    // Changelogs (flat array with scope)
    JSONValue ChangeLogs = JSONValue::array();
    for (const FChangeLogEntry &Entry : InBundle.mChangeLogs)
        ChangeLogs.push_back(SerializeChangeLogEntry(Entry));
    Root["changelogs"] = std::move(ChangeLogs);

    // Verifications (flat array with scope)
    JSONValue Verifications = JSONValue::array();
    for (const FVerificationEntry &Entry : InBundle.mVerifications)
        Verifications.push_back(SerializeVerificationEntry(Entry));
    Root["verifications"] = std::move(Verifications);

    return Root;
}

static bool DeserializeLaneRecordStrict(const JSONValue &InJson,
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

static bool DeserializeTaskRecordStrict(const JSONValue &InJson,
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

static bool DeserializeJobRecordStrict(const JSONValue &InJson,
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

static bool DeserializeTestingRecordStrict(const JSONValue &InJson,
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

static bool DeserializeFileManifestStrict(const JSONValue &InJson,
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

static bool DeserializeChangeLogStrict(const JSONValue &InJson,
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
    if (!OptionalChangeType(InJson, "type", OutEntry.mType, InContext,
                            OutError))
        return false;
    if (!OptionalTestingActor(InJson, "actor", OutEntry.mActor, InContext,
                              OutError))
        return false;
    return true;
}

static bool DeserializeVerificationStrict(const JSONValue &InJson,
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

static bool DeserializePhaseRecordStrict(const JSONValue &InJson,
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
    DeserializeBundleReferences(InJson, "dependencies", DM.mDependencies);
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

static bool DeserializeTopicBundleV4(const JSONValue &InRoot,
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
    // topic_category (V3-era field) is silently ignored if present in
    // legacy bundle files; it is no longer interpreted.

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
    DeserializeBundleReferences(InRoot, "dependencies", Meta.mDependencies);
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
        const JSONValue Root = SerializeTopicBundleV4(InBundle);
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

        const JSONValue Root = JSONValue::parse(Content);

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
    catch (const JSONValue::parse_error &Ex)
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
