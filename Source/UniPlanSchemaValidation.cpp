#include "UniPlanSchemaValidation.h"

#include <algorithm>
#include <cctype>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Internal: normalize raw string for lenient matching
// ---------------------------------------------------------------------------

static std::string NormalizeLower(const std::string &InValue)
{
    std::string Result;
    for (char C : InValue)
    {
        if (C == '_' || C == '-')
            Result += ' ';
        else if (C == '`')
            continue;
        else
            Result +=
                static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
    }
    // Trim
    const size_t Start = Result.find_first_not_of(' ');
    if (Start == std::string::npos)
        return "";
    const size_t End = Result.find_last_not_of(' ');
    return Result.substr(Start, End - Start + 1);
}

// ---------------------------------------------------------------------------
// RequireString
// ---------------------------------------------------------------------------

bool RequireString(const JsonValue &InJson, const std::string &InKey,
                   std::string &OutValue, const std::string &InContext,
                   std::string &OutError)
{
    if (!InJson.contains(InKey))
    {
        OutError = InContext + ": missing required field '" + InKey + "'";
        return false;
    }
    if (!InJson[InKey].is_string())
    {
        OutError = InContext + "." + InKey + ": expected string, got " +
                   std::string(InJson[InKey].type_name());
        return false;
    }
    OutValue = InJson[InKey].get<std::string>();
    return true;
}

// ---------------------------------------------------------------------------
// RequireArray
// ---------------------------------------------------------------------------

bool RequireArray(const JsonValue &InJson, const std::string &InKey,
                  const std::string &InContext, std::string &OutError)
{
    if (!InJson.contains(InKey))
    {
        OutError = InContext + ": missing required field '" + InKey + "'";
        return false;
    }
    if (!InJson[InKey].is_array())
    {
        OutError = InContext + "." + InKey + ": expected array, got " +
                   std::string(InJson[InKey].type_name());
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// RequireStringArray
// ---------------------------------------------------------------------------

bool RequireStringArray(const JsonValue &InJson, const std::string &InKey,
                        std::vector<std::string> &OutValue,
                        const std::string &InContext, std::string &OutError)
{
    if (!RequireArray(InJson, InKey, InContext, OutError))
        return false;
    OutValue.clear();
    for (const auto &Item : InJson[InKey])
    {
        if (Item.is_string())
            OutValue.push_back(Item.get<std::string>());
    }
    return true;
}

// ---------------------------------------------------------------------------
// OptionalString
// ---------------------------------------------------------------------------

bool OptionalString(const JsonValue &InJson, const std::string &InKey,
                    std::string &OutValue)
{
    if (InJson.contains(InKey) && InJson[InKey].is_string())
    {
        OutValue = InJson[InKey].get<std::string>();
        return true;
    }
    OutValue.clear();
    return false;
}

// ---------------------------------------------------------------------------
// OptionalStringArray
// ---------------------------------------------------------------------------

bool OptionalStringArray(const JsonValue &InJson, const std::string &InKey,
                         std::vector<std::string> &OutValue)
{
    OutValue.clear();
    if (InJson.contains(InKey) && InJson[InKey].is_array())
    {
        for (const auto &Item : InJson[InKey])
        {
            if (Item.is_string())
                OutValue.push_back(Item.get<std::string>());
        }
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// RequireExecutionStatus
// ---------------------------------------------------------------------------

bool RequireExecutionStatus(const JsonValue &InJson, const std::string &InKey,
                            EExecutionStatus &OutValue,
                            const std::string &InContext, std::string &OutError)
{
    std::string Raw;
    if (!RequireString(InJson, InKey, Raw, InContext, OutError))
        return false;
    if (!ExecutionStatusFromString(Raw, OutValue))
    {
        OutError = InContext + "." + InKey + ": invalid value '" + Raw +
                   "', expected "
                   "not_started|in_progress|completed|blocked";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// RequireTopicStatus
// ---------------------------------------------------------------------------

bool RequireTopicStatus(const JsonValue &InJson, const std::string &InKey,
                        ETopicStatus &OutValue, const std::string &InContext,
                        std::string &OutError)
{
    std::string Raw;
    if (!RequireString(InJson, InKey, Raw, InContext, OutError))
        return false;
    if (!TopicStatusFromString(Raw, OutValue))
    {
        OutError = InContext + "." + InKey + ": invalid value '" + Raw +
                   "', expected "
                   "not_started|in_progress|completed|blocked|canceled";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// RequireFileAction
// ---------------------------------------------------------------------------

bool RequireFileAction(const JsonValue &InJson, const std::string &InKey,
                       EFileAction &OutValue, const std::string &InContext,
                       std::string &OutError)
{
    std::string Raw;
    if (!RequireString(InJson, InKey, Raw, InContext, OutError))
        return false;
    if (!FileActionFromString(Raw, OutValue))
    {
        OutError = InContext + "." + InKey + ": invalid value '" + Raw +
                   "', expected create|modify|delete";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// RequireTestingActor
// ---------------------------------------------------------------------------

bool RequireTestingActor(const JsonValue &InJson, const std::string &InKey,
                         ETestingActor &OutValue, const std::string &InContext,
                         std::string &OutError)
{
    std::string Raw;
    if (!RequireString(InJson, InKey, Raw, InContext, OutError))
        return false;
    if (!TestingActorFromString(Raw, OutValue))
    {
        OutError = InContext + "." + InKey + ": invalid value '" + Raw +
                   "', expected human|ai|automated";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// ParseExecutionStatusLenient — V1/V2 backward compat
// ---------------------------------------------------------------------------

EExecutionStatus ParseExecutionStatusLenient(const std::string &InRaw)
{
    // Try strict match first
    EExecutionStatus Status;
    if (ExecutionStatusFromString(InRaw, Status))
        return Status;

    // Normalize and match patterns
    const std::string N = NormalizeLower(InRaw);
    if (N.find("complete") != std::string::npos || N == "done" || N == "closed")
        return EExecutionStatus::Completed;
    if (N.find("progress") != std::string::npos || N == "active")
        return EExecutionStatus::InProgress;
    if (N.find("block") != std::string::npos)
        return EExecutionStatus::Blocked;
    return EExecutionStatus::NotStarted;
}

// ---------------------------------------------------------------------------
// ParseFileActionLenient
// ---------------------------------------------------------------------------

EFileAction ParseFileActionLenient(const std::string &InRaw)
{
    EFileAction Action;
    if (FileActionFromString(InRaw, Action))
        return Action;

    const std::string N = NormalizeLower(InRaw);
    if (N.find("modif") != std::string::npos || N == "update" || N == "edit" ||
        N == "change")
        return EFileAction::Modify;
    if (N.find("delet") != std::string::npos || N == "remove")
        return EFileAction::Delete;
    return EFileAction::Create;
}

// ---------------------------------------------------------------------------
// ParseTestingActorLenient
// ---------------------------------------------------------------------------

ETestingActor ParseTestingActorLenient(const std::string &InRaw)
{
    ETestingActor Actor;
    if (TestingActorFromString(InRaw, Actor))
        return Actor;

    const std::string N = NormalizeLower(InRaw);
    if (N == "ai" || N == "claude" || N == "llm" || N == "agent")
        return ETestingActor::AI;
    if (N == "auto" || N.find("automat") != std::string::npos || N == "ci" ||
        N == "script")
        return ETestingActor::Automated;
    return ETestingActor::Human;
}

// ---------------------------------------------------------------------------
// ParseChangeTypeLenient
// ---------------------------------------------------------------------------

EChangeType ParseChangeTypeLenient(const std::string &InRaw)
{
    EChangeType Type;
    if (ChangeTypeFromString(InRaw, Type))
        return Type;

    const std::string N = NormalizeLower(InRaw);
    if (N == "feature" || N == "add" || N == "new")
        return EChangeType::Feat;
    if (N == "bugfix" || N == "patch" || N == "hotfix")
        return EChangeType::Fix;
    if (N == "cleanup" || N == "restructure")
        return EChangeType::Refactor;
    return EChangeType::Chore;
}

// ---------------------------------------------------------------------------
// RequireChangeType
// ---------------------------------------------------------------------------

bool RequireChangeType(const JsonValue &InJson, const std::string &InKey,
                       EChangeType &OutValue, const std::string &InContext,
                       std::string &OutError)
{
    if (!InJson.contains(InKey) || !InJson[InKey].is_string())
    {
        OutError = InContext + "." + InKey + ": missing or not a string";
        return false;
    }
    const std::string Raw = InJson[InKey].get<std::string>();
    if (!ChangeTypeFromString(Raw, OutValue))
    {
        OutError = InContext + "." + InKey + ": invalid value '" + Raw +
                   "', expected feat|fix|refactor|chore";
        return false;
    }
    return true;
}

} // namespace UniPlan
