#pragma once

#include "UniPlanEnums.h"
#include "UniPlanJSON.h"

#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Strict schema validation helpers for V3 deserializer.
// These are shared between TryReadTopicBundle and uni-plan validate.
// All Require* functions return false and set OutError on failure.
// ---------------------------------------------------------------------------

bool RequireString(const JSONValue &InJson, const std::string &InKey,
                   std::string &OutValue, const std::string &InContext,
                   std::string &OutError);

bool RequireArray(const JSONValue &InJson, const std::string &InKey,
                  const std::string &InContext, std::string &OutError);

bool RequireStringArray(const JSONValue &InJson, const std::string &InKey,
                        std::vector<std::string> &OutValue,
                        const std::string &InContext, std::string &OutError);

bool OptionalString(const JSONValue &InJson, const std::string &InKey,
                    std::string &OutValue);

bool OptionalStringArray(const JSONValue &InJson, const std::string &InKey,
                         std::vector<std::string> &OutValue);

// ---------------------------------------------------------------------------
// Typed enum validation — field must exist, be a string, and parse
// to a valid enum value. Invalid values produce specific errors like:
//   "phases[2].status: invalid value 'done', expected
//    not_started|in_progress|completed|blocked"
// ---------------------------------------------------------------------------

bool RequireExecutionStatus(const JSONValue &InJson, const std::string &InKey,
                            EExecutionStatus &OutValue,
                            const std::string &InContext,
                            std::string &OutError);

bool RequireTopicStatus(const JSONValue &InJson, const std::string &InKey,
                        ETopicStatus &OutValue, const std::string &InContext,
                        std::string &OutError);

bool RequireFileAction(const JSONValue &InJson, const std::string &InKey,
                       EFileAction &OutValue, const std::string &InContext,
                       std::string &OutError);

bool RequireTestingActor(const JSONValue &InJson, const std::string &InKey,
                         ETestingActor &OutValue, const std::string &InContext,
                         std::string &OutError);

bool RequireChangeType(const JSONValue &InJson, const std::string &InKey,
                       EChangeType &OutValue, const std::string &InContext,
                       std::string &OutError);

// ---------------------------------------------------------------------------
// Optional enum parsing — returns false only when the field is present but
// invalid. Missing or empty values succeed with OutValue left untouched so
// the caller's struct default applies.
// ---------------------------------------------------------------------------

bool OptionalChangeType(const JSONValue &InJson, const std::string &InKey,
                        EChangeType &OutValue, const std::string &InContext,
                        std::string &OutError);

bool OptionalTestingActor(const JSONValue &InJson, const std::string &InKey,
                          ETestingActor &OutValue, const std::string &InContext,
                          std::string &OutError);

// ---------------------------------------------------------------------------
// Lenient execution status parsing for V3 playbook markdown table rows.
// Accepts loose column text like "done" / "closed" / "active" and falls
// back to EExecutionStatus::NotStarted when the cell is unparseable. Used
// only by the legacy .Playbook.md lane deserialization path.
// ---------------------------------------------------------------------------

EExecutionStatus ParseExecutionStatusLenient(const std::string &InRaw);

} // namespace UniPlan
