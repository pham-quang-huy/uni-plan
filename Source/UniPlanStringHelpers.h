#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace UniPlan
{

// Replace common Unicode format artifacts with their ASCII equivalents so
// prose round-trips cleanly through `no_smart_quotes`. Targets: em/en/figure
// dash + horizontal bar -> "-"; smart single/double quotes -> straight
// quotes; non-breaking space -> regular space. Returns the count of byte
// positions replaced (useful for "N normalizations" mutation reports).
inline size_t NormalizeSmartChars(std::string &InOutValue)
{
    struct FReplacement
    {
        const char *mFrom;
        const char *mTo;
    };
    static const FReplacement kMap[] = {
        {"\xE2\x80\x94", "-"},  // U+2014 em dash
        {"\xE2\x80\x93", "-"},  // U+2013 en dash
        {"\xE2\x80\x92", "-"},  // U+2012 figure dash
        {"\xE2\x80\x95", "-"},  // U+2015 horizontal bar
        {"\xE2\x80\x98", "'"},  // U+2018 left single quote
        {"\xE2\x80\x99", "'"},  // U+2019 right single quote
        {"\xE2\x80\x9C", "\""}, // U+201C left double quote
        {"\xE2\x80\x9D", "\""}, // U+201D right double quote
        {"\xC2\xA0", " "},      // U+00A0 non-breaking space
    };
    size_t Replacements = 0;
    for (const FReplacement &R : kMap)
    {
        const std::string From = R.mFrom;
        const std::string To = R.mTo;
        size_t Position = 0;
        while ((Position = InOutValue.find(From, Position)) !=
               std::string::npos)
        {
            InOutValue.replace(Position, From.size(), To);
            Position += To.size();
            ++Replacements;
        }
    }
    return Replacements;
}

inline std::string ToLower(std::string InValue)
{
    std::transform(InValue.begin(), InValue.end(), InValue.begin(),
                   [](const unsigned char InCharacter)
                   { return static_cast<char>(std::tolower(InCharacter)); });
    return InValue;
}

inline std::string Trim(const std::string &InValue)
{
    size_t Start = 0;
    while (Start < InValue.size() &&
           std::isspace(static_cast<unsigned char>(InValue[Start])) != 0)
    {
        ++Start;
    }
    size_t End = InValue.size();
    while (End > Start &&
           std::isspace(static_cast<unsigned char>(InValue[End - 1])) != 0)
    {
        --End;
    }
    return InValue.substr(Start, End - Start);
}

inline bool EndsWith(const std::string &InValue, const std::string &InSuffix)
{
    if (InSuffix.size() > InValue.size())
    {
        return false;
    }
    return InValue.compare(InValue.size() - InSuffix.size(), InSuffix.size(),
                           InSuffix) == 0;
}

inline std::string JoinCommaSeparated(const std::vector<std::string> &InValues)
{
    std::ostringstream Stream;
    for (size_t Index = 0; Index < InValues.size(); ++Index)
    {
        if (Index > 0)
        {
            Stream << ",";
        }
        Stream << InValues[Index];
    }
    return Stream.str();
}

inline std::string GetUtcNow()
{
    const auto Now = std::chrono::system_clock::now();
    const std::time_t Timestamp = std::chrono::system_clock::to_time_t(Now);
    std::tm UtcTime{};
#ifdef _WIN32
    gmtime_s(&UtcTime, &Timestamp);
#else
    gmtime_r(&Timestamp, &UtcTime);
#endif
    std::ostringstream Stream;
    Stream << std::put_time(&UtcTime, "%Y-%m-%dT%H:%M:%SZ");
    return Stream.str();
}

} // namespace UniPlan
