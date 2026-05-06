#pragma once

#include "UniPlanPathHelpers.h"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace UniPlan
{

// JSONEscape — produce the RFC 8259 §7 escaped form of an arbitrary
// std::string for embedding between JSON string double-quotes. Handles
// the six named escapes (\, ", \n, \r, \t, \b, \f) and emits `\u00XX`
// for every remaining control character (U+0000 through U+001F). Passing
// raw control bytes through would produce syntactically invalid JSON,
// so this helper is the single source of truth for escape behavior —
// do not inline or re-implement in consumers.
inline std::string JSONEscape(const std::string &InValue)
{
    std::ostringstream Stream;
    for (const char Character : InValue)
    {
        switch (Character)
        {
        case '\\':
            Stream << "\\\\";
            break;
        case '"':
            Stream << "\\\"";
            break;
        case '\n':
            Stream << "\\n";
            break;
        case '\r':
            Stream << "\\r";
            break;
        case '\t':
            Stream << "\\t";
            break;
        case '\b':
            Stream << "\\b";
            break;
        case '\f':
            Stream << "\\f";
            break;
        default:
            if (static_cast<unsigned char>(Character) < 0x20)
            {
                char Buffer[8];
                std::snprintf(Buffer, sizeof(Buffer), "\\u%04x",
                              static_cast<unsigned int>(
                                  static_cast<unsigned char>(Character)));
                Stream << Buffer;
            }
            else
            {
                Stream << Character;
            }
            break;
        }
    }
    return Stream.str();
}

inline std::string JSONQuote(const std::string &InValue)
{
    return "\"" + JSONEscape(InValue) + "\"";
}

inline std::string JSONNullOrQuote(const std::string &InValue)
{
    return InValue.empty() ? "null" : JSONQuote(InValue);
}

inline void PrintJsonHeader(const char *InSchema, const std::string &InUtc,
                            const std::string &InRoot)
{
    std::cout << "{\"schema\":" << JSONQuote(InSchema)
              << ",\"generated_utc\":" << JSONQuote(InUtc)
              << ",\"repo_root\":" << JSONQuote(FormatJSONRepoRoot(InRoot))
              << ",";
}

inline void PrintJsonSep(const size_t InIndex)
{
    if (InIndex > 0)
    {
        std::cout << ",";
    }
}

inline void PrintJsonStringArray(const char *InName,
                                 const std::vector<std::string> &InItems)
{
    std::cout << "\"" << InName << "\":[";
    for (size_t Index = 0; Index < InItems.size(); ++Index)
    {
        if (Index > 0)
        {
            std::cout << ",";
        }
        std::cout << JSONQuote(InItems[Index]);
    }
    std::cout << "]";
}

inline void PrintJsonWarnings(const std::vector<std::string> &InWarnings)
{
    PrintJsonStringArray("warnings", InWarnings);
}

inline void PrintJsonClose(const std::vector<std::string> &InWarnings)
{
    PrintJsonWarnings(InWarnings);
    std::cout << "}\n";
}

inline void EmitJsonField(const char *InName, const std::string &InValue,
                          bool InTrailingComma = true)
{
    std::cout << "\"" << InName << "\":" << JSONQuote(InValue);
    if (InTrailingComma)
    {
        std::cout << ",";
    }
}

inline void EmitJsonFieldNullable(const char *InName,
                                  const std::string &InValue,
                                  bool InTrailingComma = true)
{
    std::cout << "\"" << InName << "\":" << JSONNullOrQuote(InValue);
    if (InTrailingComma)
    {
        std::cout << ",";
    }
}

inline void EmitJsonFieldInt(const char *InName, int InValue,
                             bool InTrailingComma = true)
{
    std::cout << "\"" << InName << "\":" << InValue;
    if (InTrailingComma)
    {
        std::cout << ",";
    }
}

inline void EmitJsonFieldBool(const char *InName, bool InValue,
                              bool InTrailingComma = true)
{
    std::cout << "\"" << InName << "\":" << (InValue ? "true" : "false");
    if (InTrailingComma)
    {
        std::cout << ",";
    }
}

inline void EmitJsonFieldSizeT(const char *InName, size_t InValue,
                               bool InTrailingComma = true)
{
    std::cout << "\"" << InName << "\":" << InValue;
    if (InTrailingComma)
    {
        std::cout << ",";
    }
}

} // namespace UniPlan
