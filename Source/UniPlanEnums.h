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
// Closed set: no Unknown, no Canceled (those are topic-level concerns).
// ---------------------------------------------------------------------------

enum class EExecutionStatus : uint8_t
{
    NotStarted,
    InProgress,
    Completed,
    Blocked
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

} // namespace UniPlan
