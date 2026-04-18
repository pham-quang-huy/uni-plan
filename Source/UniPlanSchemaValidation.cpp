#include "UniPlanSchemaValidation.h"

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

// ---------------------------------------------------------------------------
// OptionalChangeType — missing/empty leaves OutValue untouched
// ---------------------------------------------------------------------------

bool OptionalChangeType(const JsonValue &InJson, const std::string &InKey,
                        EChangeType &OutValue, const std::string &InContext,
                        std::string &OutError)
{
    if (!InJson.contains(InKey) || !InJson[InKey].is_string())
        return true;
    const std::string Raw = InJson[InKey].get<std::string>();
    if (Raw.empty())
        return true;
    if (!ChangeTypeFromString(Raw, OutValue))
    {
        OutError = InContext + "." + InKey + ": invalid value '" + Raw +
                   "', expected feat|fix|refactor|chore";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// OptionalTestingActor — missing/empty leaves OutValue untouched
// ---------------------------------------------------------------------------

bool OptionalTestingActor(const JsonValue &InJson, const std::string &InKey,
                          ETestingActor &OutValue, const std::string &InContext,
                          std::string &OutError)
{
    if (!InJson.contains(InKey) || !InJson[InKey].is_string())
        return true;
    const std::string Raw = InJson[InKey].get<std::string>();
    if (Raw.empty())
        return true;
    if (!TestingActorFromString(Raw, OutValue))
    {
        OutError = InContext + "." + InKey + ": invalid value '" + Raw +
                   "', expected human|ai|automated";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// ParseExecutionStatusLenient — V3 playbook markdown table fallback
// ---------------------------------------------------------------------------

EExecutionStatus ParseExecutionStatusLenient(const std::string &InRaw)
{
    EExecutionStatus Status;
    if (ExecutionStatusFromString(InRaw, Status))
        return Status;

    const std::string N = NormalizeLower(InRaw);
    if (N.find("complete") != std::string::npos || N == "done" || N == "closed")
        return EExecutionStatus::Completed;
    if (N.find("progress") != std::string::npos || N == "active")
        return EExecutionStatus::InProgress;
    if (N.find("block") != std::string::npos)
        return EExecutionStatus::Blocked;
    return EExecutionStatus::NotStarted;
}

} // namespace UniPlan
