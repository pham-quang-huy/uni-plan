#pragma once

#include "UniPlanEnums.h"
#include "UniPlanJson.h"

#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Strict schema validation helpers for V3 deserializer.
// These are shared between TryReadTopicBundle and uni-plan validate.
// All Require* functions return false and set OutError on failure.
// ---------------------------------------------------------------------------

bool RequireString(const JsonValue &InJson, const std::string &InKey,
                   std::string &OutValue, const std::string &InContext,
                   std::string &OutError);

bool RequireArray(const JsonValue &InJson, const std::string &InKey,
                  const std::string &InContext, std::string &OutError);

bool RequireStringArray(const JsonValue &InJson, const std::string &InKey,
                        std::vector<std::string> &OutValue,
                        const std::string &InContext, std::string &OutError);

bool OptionalString(const JsonValue &InJson, const std::string &InKey,
                    std::string &OutValue);

bool OptionalStringArray(const JsonValue &InJson, const std::string &InKey,
                         std::vector<std::string> &OutValue);

// ---------------------------------------------------------------------------
// Typed enum validation — field must exist, be a string, and parse
// to a valid enum value. Invalid values produce specific errors like:
//   "phases[2].status: invalid value 'done', expected
//    not_started|in_progress|completed|blocked"
// ---------------------------------------------------------------------------

bool RequireExecutionStatus(const JsonValue &InJson, const std::string &InKey,
                            EExecutionStatus &OutValue,
                            const std::string &InContext,
                            std::string &OutError);

bool RequireTopicStatus(const JsonValue &InJson, const std::string &InKey,
                        ETopicStatus &OutValue, const std::string &InContext,
                        std::string &OutError);

bool RequireFileAction(const JsonValue &InJson, const std::string &InKey,
                       EFileAction &OutValue, const std::string &InContext,
                       std::string &OutError);

bool RequireTestingActor(const JsonValue &InJson, const std::string &InKey,
                         ETestingActor &OutValue, const std::string &InContext,
                         std::string &OutError);

// ---------------------------------------------------------------------------
// Lenient enum parsing for V1/V2 backward compatibility.
// Handles aliases ("done" → Completed, "active" → InProgress, etc.).
// Never fails — returns a sensible default.
// ---------------------------------------------------------------------------

EExecutionStatus ParseExecutionStatusLenient(const std::string &InRaw);
EFileAction ParseFileActionLenient(const std::string &InRaw);
ETestingActor ParseTestingActorLenient(const std::string &InRaw);
EChangeType ParseChangeTypeLenient(const std::string &InRaw);

bool RequireChangeType(const JsonValue &InJson, const std::string &InKey,
                       EChangeType &OutValue, const std::string &InContext,
                       std::string &OutError);

} // namespace UniPlan
