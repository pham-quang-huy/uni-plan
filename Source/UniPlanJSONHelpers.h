#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace UniPlan
{

inline std::string JsonEscape(const std::string &InValue)
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
        default:
            Stream << Character;
            break;
        }
    }
    return Stream.str();
}

inline std::string JsonQuote(const std::string &InValue)
{
    return "\"" + JsonEscape(InValue) + "\"";
}

inline std::string JsonNullOrQuote(const std::string &InValue)
{
    return InValue.empty() ? "null" : JsonQuote(InValue);
}

inline void PrintJsonHeader(const char *InSchema, const std::string &InUtc,
                            const std::string &InRoot)
{
    std::cout << "{\"schema\":" << JsonQuote(InSchema)
              << ",\"generated_utc\":" << JsonQuote(InUtc)
              << ",\"repo_root\":" << JsonQuote(InRoot) << ",";
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
        std::cout << JsonQuote(InItems[Index]);
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
    std::cout << "\"" << InName << "\":" << JsonQuote(InValue);
    if (InTrailingComma)
    {
        std::cout << ",";
    }
}

inline void EmitJsonFieldNullable(const char *InName,
                                  const std::string &InValue,
                                  bool InTrailingComma = true)
{
    std::cout << "\"" << InName << "\":" << JsonNullOrQuote(InValue);
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
