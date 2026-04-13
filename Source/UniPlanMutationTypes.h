#pragma once

#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Mutation result types for write commands (Phase 5+).
// Defines the structured output of any document mutation operation.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// FMutationChange — one discrete change within a mutation.
// ---------------------------------------------------------------------------

struct FMutationChange
{
    std::string mField;
    std::string mOldValue;
    std::string mNewValue;
};

// ---------------------------------------------------------------------------
// FMutationResult — outcome of a write/mutation command.
// ---------------------------------------------------------------------------

struct FMutationResult
{
    std::string mType;
    std::string mTargetPath;
    std::string mTargetEntity;
    std::vector<FMutationChange> mChanges;
    bool mbChangeLogAppended = false;
    std::string mChangeLogPath;
    bool mbPreCheckPassed = true;
    bool mbPostCheckPassed = true;
    std::vector<std::string> mChecksRun;
    std::vector<std::string> mWarnings;
};

} // namespace UniPlan
