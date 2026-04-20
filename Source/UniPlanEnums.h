#pragma once

#include <cstdint>
#include <string>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// EDocumentType — classifies all document kinds including sidecars.
// Replaces EDocumentKind (3 values) + string-based sidecar kinds.
// ---------------------------------------------------------------------------

enum class EDocumentType : uint8_t
{
    Plan,
    Playbook,
    Implementation,
    ChangeLog,
    Verification
};

inline const char *ToString(EDocumentType InValue)
{
    switch (InValue)
    {
    case EDocumentType::Plan:
        return "plan";
    case EDocumentType::Playbook:
        return "playbook";
    case EDocumentType::Implementation:
        return "implementation";
    case EDocumentType::ChangeLog:
        return "changelog";
    case EDocumentType::Verification:
        return "verification";
    }
    return "unknown";
}

inline EDocumentType DocumentTypeFromString(const std::string &InValue,
                                            bool *OutValid = nullptr)
{
    if (OutValid)
        *OutValid = true;
    if (InValue == "plan")
        return EDocumentType::Plan;
    if (InValue == "playbook")
        return EDocumentType::Playbook;
    if (InValue == "implementation" || InValue == "impl")
        return EDocumentType::Implementation;
    if (InValue == "changelog")
        return EDocumentType::ChangeLog;
    if (InValue == "verification")
        return EDocumentType::Verification;
    if (OutValid)
        *OutValid = false;
    return EDocumentType::Plan;
}

// ---------------------------------------------------------------------------
// EPhaseStatus — normalized phase/document status values.
// Replaces raw status strings ("not_started", "in_progress", etc.).
// ---------------------------------------------------------------------------

enum class EPhaseStatus : uint8_t
{
    NotStarted,
    InProgress,
    Completed,
    Closed,
    Blocked,
    Canceled,
    Unknown
};

inline const char *ToString(EPhaseStatus InValue)
{
    switch (InValue)
    {
    case EPhaseStatus::NotStarted:
        return "not_started";
    case EPhaseStatus::InProgress:
        return "in_progress";
    case EPhaseStatus::Completed:
        return "completed";
    case EPhaseStatus::Closed:
        return "closed";
    case EPhaseStatus::Blocked:
        return "blocked";
    case EPhaseStatus::Canceled:
        return "canceled";
    case EPhaseStatus::Unknown:
        return "unknown";
    }
    return "unknown";
}

inline EPhaseStatus PhaseStatusFromString(const std::string &InValue)
{
    if (InValue == "not_started")
        return EPhaseStatus::NotStarted;
    if (InValue == "in_progress")
        return EPhaseStatus::InProgress;
    if (InValue == "completed")
        return EPhaseStatus::Completed;
    if (InValue == "closed")
        return EPhaseStatus::Closed;
    if (InValue == "blocked")
        return EPhaseStatus::Blocked;
    if (InValue == "canceled")
        return EPhaseStatus::Canceled;
    return EPhaseStatus::Unknown;
}

// ---------------------------------------------------------------------------
// EPairState — topic pairing state (plan + impl + playbooks).
// Replaces string-based pair state values.
// ---------------------------------------------------------------------------

enum class EPairState : uint8_t
{
    Paired,
    MissingImplementation,
    MissingPlan,
    OrphanImplementation,
    OrphanPlaybook,
    MissingPhasePlaybook,
    Unknown
};

inline const char *ToString(EPairState InValue)
{
    switch (InValue)
    {
    case EPairState::Paired:
        return "paired";
    case EPairState::MissingImplementation:
        return "missing_implementation";
    case EPairState::MissingPlan:
        return "missing_plan";
    case EPairState::OrphanImplementation:
        return "orphan_implementation";
    case EPairState::OrphanPlaybook:
        return "orphan_playbook";
    case EPairState::MissingPhasePlaybook:
        return "missing_phase_playbook";
    case EPairState::Unknown:
        return "unknown";
    }
    return "unknown";
}

inline EPairState PairStateFromString(const std::string &InValue)
{
    if (InValue == "paired")
        return EPairState::Paired;
    if (InValue == "missing_implementation")
        return EPairState::MissingImplementation;
    if (InValue == "missing_plan")
        return EPairState::MissingPlan;
    if (InValue == "orphan_implementation")
        return EPairState::OrphanImplementation;
    if (InValue == "orphan_playbook")
        return EPairState::OrphanPlaybook;
    if (InValue == "missing_phase_playbook")
        return EPairState::MissingPhasePlaybook;
    return EPairState::Unknown;
}

// ---------------------------------------------------------------------------
// EExecutionStatus — execution entity status (phase/lane/job/task).
// Canceled covers the "superseded / won't execute" terminal state (migration
// aliases, renumbered scopes) without implying the work was completed.
// ---------------------------------------------------------------------------

enum class EExecutionStatus : uint8_t
{
    NotStarted,
    InProgress,
    Completed,
    Blocked,
    Canceled
};

inline const char *ToString(EExecutionStatus InValue)
{
    switch (InValue)
    {
    case EExecutionStatus::NotStarted:
        return "not_started";
    case EExecutionStatus::InProgress:
        return "in_progress";
    case EExecutionStatus::Completed:
        return "completed";
    case EExecutionStatus::Blocked:
        return "blocked";
    case EExecutionStatus::Canceled:
        return "canceled";
    }
    return "not_started";
}

inline bool ExecutionStatusFromString(const std::string &InValue,
                                      EExecutionStatus &OutValue)
{
    if (InValue == "not_started")
    {
        OutValue = EExecutionStatus::NotStarted;
        return true;
    }
    if (InValue == "in_progress")
    {
        OutValue = EExecutionStatus::InProgress;
        return true;
    }
    if (InValue == "completed")
    {
        OutValue = EExecutionStatus::Completed;
        return true;
    }
    if (InValue == "blocked")
    {
        OutValue = EExecutionStatus::Blocked;
        return true;
    }
    if (InValue == "canceled")
    {
        OutValue = EExecutionStatus::Canceled;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ETopicStatus — topic-level status (superset of execution status).
// Includes Canceled for topics that are abandoned.
// ---------------------------------------------------------------------------

enum class ETopicStatus : uint8_t
{
    NotStarted,
    InProgress,
    Completed,
    Blocked,
    Canceled
};

inline const char *ToString(ETopicStatus InValue)
{
    switch (InValue)
    {
    case ETopicStatus::NotStarted:
        return "not_started";
    case ETopicStatus::InProgress:
        return "in_progress";
    case ETopicStatus::Completed:
        return "completed";
    case ETopicStatus::Blocked:
        return "blocked";
    case ETopicStatus::Canceled:
        return "canceled";
    }
    return "not_started";
}

inline bool TopicStatusFromString(const std::string &InValue,
                                  ETopicStatus &OutValue)
{
    if (InValue == "not_started")
    {
        OutValue = ETopicStatus::NotStarted;
        return true;
    }
    if (InValue == "in_progress")
    {
        OutValue = ETopicStatus::InProgress;
        return true;
    }
    if (InValue == "completed")
    {
        OutValue = ETopicStatus::Completed;
        return true;
    }
    if (InValue == "blocked")
    {
        OutValue = ETopicStatus::Blocked;
        return true;
    }
    if (InValue == "canceled")
    {
        OutValue = ETopicStatus::Canceled;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// EFileAction — file manifest action type.
// ---------------------------------------------------------------------------

enum class EFileAction : uint8_t
{
    Create,
    Modify,
    Delete
};

inline const char *ToString(EFileAction InValue)
{
    switch (InValue)
    {
    case EFileAction::Create:
        return "create";
    case EFileAction::Modify:
        return "modify";
    case EFileAction::Delete:
        return "delete";
    }
    return "create";
}

inline bool FileActionFromString(const std::string &InValue,
                                 EFileAction &OutValue)
{
    if (InValue == "create")
    {
        OutValue = EFileAction::Create;
        return true;
    }
    if (InValue == "modify")
    {
        OutValue = EFileAction::Modify;
        return true;
    }
    if (InValue == "delete")
    {
        OutValue = EFileAction::Delete;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ETestingActor — who performs a testing step.
// ---------------------------------------------------------------------------

enum class ETestingActor : uint8_t
{
    Human,
    AI,
    Automated
};

inline const char *ToString(ETestingActor InValue)
{
    switch (InValue)
    {
    case ETestingActor::Human:
        return "human";
    case ETestingActor::AI:
        return "ai";
    case ETestingActor::Automated:
        return "automated";
    }
    return "human";
}

inline bool TestingActorFromString(const std::string &InValue,
                                   ETestingActor &OutValue)
{
    if (InValue == "human")
    {
        OutValue = ETestingActor::Human;
        return true;
    }
    if (InValue == "ai")
    {
        OutValue = ETestingActor::AI;
        return true;
    }
    if (InValue == "automated")
    {
        OutValue = ETestingActor::Automated;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// EChangeType — changelog entry classification.
// ---------------------------------------------------------------------------

enum class EChangeType : uint8_t
{
    Feat,
    Fix,
    Refactor,
    Chore
};

inline const char *ToString(EChangeType InValue)
{
    switch (InValue)
    {
    case EChangeType::Feat:
        return "feat";
    case EChangeType::Fix:
        return "fix";
    case EChangeType::Refactor:
        return "refactor";
    case EChangeType::Chore:
        return "chore";
    }
    return "chore";
}

inline bool ChangeTypeFromString(const std::string &InValue,
                                 EChangeType &OutValue)
{
    if (InValue == "feat")
    {
        OutValue = EChangeType::Feat;
        return true;
    }
    if (InValue == "fix")
    {
        OutValue = EChangeType::Fix;
        return true;
    }
    if (InValue == "refactor")
    {
        OutValue = EChangeType::Refactor;
        return true;
    }
    if (InValue == "chore")
    {
        OutValue = EChangeType::Chore;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// EDriftSeverity — severity levels for drift diagnosis items.
// ---------------------------------------------------------------------------

enum class EDriftSeverity : uint8_t
{
    Info,
    Warning,
    Error,
    Critical
};

inline const char *ToString(EDriftSeverity InValue)
{
    switch (InValue)
    {
    case EDriftSeverity::Info:
        return "info";
    case EDriftSeverity::Warning:
        return "warning";
    case EDriftSeverity::Error:
        return "error";
    case EDriftSeverity::Critical:
        return "critical";
    }
    return "unknown";
}

inline EDriftSeverity DriftSeverityFromString(const std::string &InValue)
{
    if (InValue == "info")
        return EDriftSeverity::Info;
    if (InValue == "warning")
        return EDriftSeverity::Warning;
    if (InValue == "error")
        return EDriftSeverity::Error;
    if (InValue == "critical")
        return EDriftSeverity::Critical;
    return EDriftSeverity::Warning;
}

// ---------------------------------------------------------------------------
// EBlockerStatus — blocker resolution state.
// ---------------------------------------------------------------------------

enum class EBlockerStatus : uint8_t
{
    Open,
    Blocked,
    Resolved,
    Deferred
};

inline const char *ToString(EBlockerStatus InValue)
{
    switch (InValue)
    {
    case EBlockerStatus::Open:
        return "open";
    case EBlockerStatus::Blocked:
        return "blocked";
    case EBlockerStatus::Resolved:
        return "resolved";
    case EBlockerStatus::Deferred:
        return "deferred";
    }
    return "unknown";
}

inline EBlockerStatus BlockerStatusFromString(const std::string &InValue)
{
    if (InValue == "open")
        return EBlockerStatus::Open;
    if (InValue == "blocked")
        return EBlockerStatus::Blocked;
    if (InValue == "resolved" || InValue == "closed" || InValue == "completed")
        return EBlockerStatus::Resolved;
    if (InValue == "deferred")
        return EBlockerStatus::Deferred;
    return EBlockerStatus::Open;
}

// ---------------------------------------------------------------------------
// ESectionRequirement — whether a schema section is required.
// ---------------------------------------------------------------------------

enum class ESectionRequirement : uint8_t
{
    Required,
    Conditional,
    Optional
};

inline const char *ToString(ESectionRequirement InValue)
{
    switch (InValue)
    {
    case ESectionRequirement::Required:
        return "required";
    case ESectionRequirement::Conditional:
        return "conditional";
    case ESectionRequirement::Optional:
        return "optional";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// ETaxonomyLevel — execution taxonomy hierarchy levels.
// ---------------------------------------------------------------------------

enum class ETaxonomyLevel : uint8_t
{
    Phase,
    Wave,
    Lane,
    Job,
    Task
};

inline const char *ToString(ETaxonomyLevel InValue)
{
    switch (InValue)
    {
    case ETaxonomyLevel::Phase:
        return "phase";
    case ETaxonomyLevel::Wave:
        return "wave";
    case ETaxonomyLevel::Lane:
        return "lane";
    case ETaxonomyLevel::Job:
        return "job";
    case ETaxonomyLevel::Task:
        return "task";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// EValidationSeverity — validation check severity levels.
// ---------------------------------------------------------------------------

enum class EValidationSeverity : uint8_t
{
    ErrorMajor, // Bundle is broken — cannot be used by agents
    ErrorMinor, // Field violation — data quality issue
    Warning     // Advisory — not blocking but should be fixed
};

inline const char *ToString(EValidationSeverity InValue)
{
    switch (InValue)
    {
    case EValidationSeverity::ErrorMajor:
        return "error_major";
    case EValidationSeverity::ErrorMinor:
        return "error_minor";
    case EValidationSeverity::Warning:
        return "warning";
    }
    return "warning";
}

// ---------------------------------------------------------------------------
// EPlatformScope — platform targeting for a single validation command.
// `Any` means the command runs on every supported platform.
// ---------------------------------------------------------------------------

enum class EPlatformScope : uint8_t
{
    Any,
    MacOS,
    Windows,
    Linux
};

inline const char *ToString(EPlatformScope InValue)
{
    switch (InValue)
    {
    case EPlatformScope::Any:
        return "any";
    case EPlatformScope::MacOS:
        return "macos";
    case EPlatformScope::Windows:
        return "windows";
    case EPlatformScope::Linux:
        return "linux";
    }
    return "any";
}

inline bool PlatformScopeFromString(const std::string &InValue,
                                    EPlatformScope &OutValue)
{
    if (InValue == "any" || InValue.empty())
    {
        OutValue = EPlatformScope::Any;
        return true;
    }
    if (InValue == "macos" || InValue == "macOS")
    {
        OutValue = EPlatformScope::MacOS;
        return true;
    }
    if (InValue == "windows" || InValue == "Windows")
    {
        OutValue = EPlatformScope::Windows;
        return true;
    }
    if (InValue == "linux" || InValue == "Linux")
    {
        OutValue = EPlatformScope::Linux;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// EPhaseOrigin — semantic provenance of a phase record. Durable once
// stamped; independent of whether any legacy `.md` file exists on disk.
// Preserves migration history even after the V3 corpus is deleted.
//   NativeV4    — phase was authored directly against the V4 bundle
//                 schema (no V3 migration pass).
//   V3Migration — phase was produced by migrating a V3 markdown plan
//                 (Plan.md + Impl.md + Playbook.md). Indicates agents
//                 should expect `done`/`remaining` prose seeded from the
//                 V3 tracker rather than the V4 authoring workflow.
// Absence in JSON (backward-compat: bundles older than 0.75.0) maps to
// NativeV4 — stamp v3_migration explicitly during migration or on read
// of a bundle with known V3 heritage.
// ---------------------------------------------------------------------------

enum class EPhaseOrigin : uint8_t
{
    NativeV4,
    V3Migration
};

inline const char *ToString(EPhaseOrigin InValue)
{
    switch (InValue)
    {
    case EPhaseOrigin::NativeV4:
        return "native_v4";
    case EPhaseOrigin::V3Migration:
        return "v3_migration";
    }
    return "native_v4";
}

inline bool PhaseOriginFromString(const std::string &InValue,
                                  EPhaseOrigin &OutValue)
{
    if (InValue == "native_v4" || InValue.empty())
    {
        OutValue = EPhaseOrigin::NativeV4;
        return true;
    }
    if (InValue == "v3_migration")
    {
        OutValue = EPhaseOrigin::V3Migration;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// EPhaseGapCategory — outcome of a per-phase V3↔V4 parity check, as
// computed by `uni-plan legacy-gap`. Thresholds are documented next to
// the resolver in UniPlanCommandLegacyGap.cpp and derive from
// `kPhaseHollowChars` / `kPhaseRichMinChars` in UniPlanTopicTypes.h
// (3000 / 10000 chars, calibrated against V4 schema semantics — see
// that comment block for derivation).
//   LegacyRich         — legacy playbook >= 150 content LOC, V4 < 3000
//                        design chars. Rebuild V4 from legacy.
//   LegacyRichMatched  — legacy >= 150 LOC, V4 >= 10000 chars. Verify
//                        no drift.
//   LegacyThin         — legacy 50-149 LOC. Small uplift available.
//   LegacyStub         — legacy < 50 LOC (file exists but near-empty).
//                        Fall back to commit archaeology.
//   LegacyAbsent       — no legacy playbook file exists for this phase.
//   V4Only             — no legacy AND V4 already rich (>=10000 chars,
//                        >=3 jobs).
//   HollowBoth         — legacy < 50 LOC AND completed phase with V4
//                        < 3000 chars. Status likely wrong; demote
//                        from completed.
//   Drift              — reserved for future semantic-overlap detection.
//
// Version history:
//   v0.78.0 and earlier: 50 / 150 LOC, 500 / 2000 chars.
//   v0.80.0: bumped to 50 / 200 LOC, 4000 / 16000 chars (over-
//            translated V3 "200-line playbook" into V4 chars without
//            accounting for V3 `.md` format overhead).
//   v0.83.0: recalibrated to 50 / 150 LOC, 3000 / 10000 chars against
//            V4 schema semantics (user-ratified).
// ---------------------------------------------------------------------------

enum class EPhaseGapCategory : uint8_t
{
    LegacyRich,
    LegacyRichMatched,
    LegacyThin,
    LegacyStub,
    LegacyAbsent,
    V4Only,
    HollowBoth,
    Drift
};

inline const char *ToString(EPhaseGapCategory InValue)
{
    switch (InValue)
    {
    case EPhaseGapCategory::LegacyRich:
        return "legacy_rich";
    case EPhaseGapCategory::LegacyRichMatched:
        return "legacy_rich_matched";
    case EPhaseGapCategory::LegacyThin:
        return "legacy_thin";
    case EPhaseGapCategory::LegacyStub:
        return "legacy_stub";
    case EPhaseGapCategory::LegacyAbsent:
        return "legacy_absent";
    case EPhaseGapCategory::V4Only:
        return "v4_only";
    case EPhaseGapCategory::HollowBoth:
        return "hollow_both";
    case EPhaseGapCategory::Drift:
        return "drift";
    }
    return "legacy_absent";
}

inline bool PhaseGapCategoryFromString(const std::string &InValue,
                                       EPhaseGapCategory &OutValue)
{
    if (InValue == "legacy_rich")
    {
        OutValue = EPhaseGapCategory::LegacyRich;
        return true;
    }
    if (InValue == "legacy_rich_matched")
    {
        OutValue = EPhaseGapCategory::LegacyRichMatched;
        return true;
    }
    if (InValue == "legacy_thin")
    {
        OutValue = EPhaseGapCategory::LegacyThin;
        return true;
    }
    if (InValue == "legacy_stub")
    {
        OutValue = EPhaseGapCategory::LegacyStub;
        return true;
    }
    if (InValue == "legacy_absent")
    {
        OutValue = EPhaseGapCategory::LegacyAbsent;
        return true;
    }
    if (InValue == "v4_only")
    {
        OutValue = EPhaseGapCategory::V4Only;
        return true;
    }
    if (InValue == "hollow_both")
    {
        OutValue = EPhaseGapCategory::HollowBoth;
        return true;
    }
    if (InValue == "drift")
    {
        OutValue = EPhaseGapCategory::Drift;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// EDependencyKind — kind of dependency reference in FBundleReference.
//   Bundle     — another topic's `.Plan.json` (resolves against loaded bundles)
//   Phase      — a specific phase in this or another topic
//   Governance — non-bundle governance doc (CLAUDE.md, AGENTS.md, …)
//   External   — third-party / docs outside this repo
// ---------------------------------------------------------------------------

enum class EDependencyKind : uint8_t
{
    Bundle,
    Phase,
    Governance,
    External
};

inline const char *ToString(EDependencyKind InValue)
{
    switch (InValue)
    {
    case EDependencyKind::Bundle:
        return "bundle";
    case EDependencyKind::Phase:
        return "phase";
    case EDependencyKind::Governance:
        return "governance";
    case EDependencyKind::External:
        return "external";
    }
    return "bundle";
}

inline bool DependencyKindFromString(const std::string &InValue,
                                     EDependencyKind &OutValue)
{
    if (InValue == "bundle" || InValue.empty())
    {
        OutValue = EDependencyKind::Bundle;
        return true;
    }
    if (InValue == "phase")
    {
        OutValue = EDependencyKind::Phase;
        return true;
    }
    if (InValue == "governance")
    {
        OutValue = EDependencyKind::Governance;
        return true;
    }
    if (InValue == "external")
    {
        OutValue = EDependencyKind::External;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ERiskSeverity — severity classification for FRiskEntry.
// Default Medium so an unclassified risk still carries meaningful signal
// rather than silently slipping through as Low.
// ---------------------------------------------------------------------------

enum class ERiskSeverity : uint8_t
{
    Low,
    Medium,
    High,
    Critical
};

inline const char *ToString(ERiskSeverity InValue)
{
    switch (InValue)
    {
    case ERiskSeverity::Low:
        return "low";
    case ERiskSeverity::Medium:
        return "medium";
    case ERiskSeverity::High:
        return "high";
    case ERiskSeverity::Critical:
        return "critical";
    }
    return "medium";
}

inline bool RiskSeverityFromString(const std::string &InValue,
                                   ERiskSeverity &OutValue)
{
    if (InValue == "low")
    {
        OutValue = ERiskSeverity::Low;
        return true;
    }
    if (InValue == "medium" || InValue.empty())
    {
        OutValue = ERiskSeverity::Medium;
        return true;
    }
    if (InValue == "high")
    {
        OutValue = ERiskSeverity::High;
        return true;
    }
    if (InValue == "critical")
    {
        OutValue = ERiskSeverity::Critical;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ERiskStatus — lifecycle of one FRiskEntry.
//   Open       — risk is active and requires attention.
//   Mitigated  — mitigation implemented; residual risk is tolerable.
//   Accepted   — risk consciously accepted without mitigation.
//   Closed    — risk no longer applies (conditions changed).
// ---------------------------------------------------------------------------

enum class ERiskStatus : uint8_t
{
    Open,
    Mitigated,
    Accepted,
    Closed
};

inline const char *ToString(ERiskStatus InValue)
{
    switch (InValue)
    {
    case ERiskStatus::Open:
        return "open";
    case ERiskStatus::Mitigated:
        return "mitigated";
    case ERiskStatus::Accepted:
        return "accepted";
    case ERiskStatus::Closed:
        return "closed";
    }
    return "open";
}

inline bool RiskStatusFromString(const std::string &InValue,
                                 ERiskStatus &OutValue)
{
    if (InValue == "open" || InValue.empty())
    {
        OutValue = ERiskStatus::Open;
        return true;
    }
    if (InValue == "mitigated")
    {
        OutValue = ERiskStatus::Mitigated;
        return true;
    }
    if (InValue == "accepted")
    {
        OutValue = ERiskStatus::Accepted;
        return true;
    }
    if (InValue == "closed")
    {
        OutValue = ERiskStatus::Closed;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// EActionStatus — lifecycle of one FNextActionEntry.
//   Pending    — action is identified but not yet started.
//   InProgress — action is actively being worked.
//   Completed  — action has been executed; kept for audit trail.
//   Abandoned  — action will not be executed (superseded / scope changed).
// ---------------------------------------------------------------------------

enum class EActionStatus : uint8_t
{
    Pending,
    InProgress,
    Completed,
    Abandoned
};

inline const char *ToString(EActionStatus InValue)
{
    switch (InValue)
    {
    case EActionStatus::Pending:
        return "pending";
    case EActionStatus::InProgress:
        return "in_progress";
    case EActionStatus::Completed:
        return "completed";
    case EActionStatus::Abandoned:
        return "abandoned";
    }
    return "pending";
}

inline bool ActionStatusFromString(const std::string &InValue,
                                   EActionStatus &OutValue)
{
    if (InValue == "pending" || InValue.empty())
    {
        OutValue = EActionStatus::Pending;
        return true;
    }
    if (InValue == "in_progress")
    {
        OutValue = EActionStatus::InProgress;
        return true;
    }
    if (InValue == "completed")
    {
        OutValue = EActionStatus::Completed;
        return true;
    }
    if (InValue == "abandoned")
    {
        OutValue = EActionStatus::Abandoned;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ECriterionStatus — verification state of one FAcceptanceCriterionEntry.
//   NotMet        — criterion not yet verified met (default).
//   Met           — criterion verified, evidence recorded.
//   Partial       — partially met (quantified in mEvidence).
//   NotApplicable — criterion does not apply given current scope.
// ---------------------------------------------------------------------------

enum class ECriterionStatus : uint8_t
{
    NotMet,
    Met,
    Partial,
    NotApplicable
};

inline const char *ToString(ECriterionStatus InValue)
{
    switch (InValue)
    {
    case ECriterionStatus::NotMet:
        return "not_met";
    case ECriterionStatus::Met:
        return "met";
    case ECriterionStatus::Partial:
        return "partial";
    case ECriterionStatus::NotApplicable:
        return "not_applicable";
    }
    return "not_met";
}

inline bool CriterionStatusFromString(const std::string &InValue,
                                      ECriterionStatus &OutValue)
{
    if (InValue == "not_met" || InValue.empty() || InValue == "pending")
    {
        OutValue = ECriterionStatus::NotMet;
        return true;
    }
    if (InValue == "met" || InValue == "completed")
    {
        OutValue = ECriterionStatus::Met;
        return true;
    }
    if (InValue == "partial")
    {
        OutValue = ECriterionStatus::Partial;
        return true;
    }
    if (InValue == "not_applicable" || InValue == "n/a")
    {
        OutValue = ECriterionStatus::NotApplicable;
        return true;
    }
    return false;
}

} // namespace UniPlan
