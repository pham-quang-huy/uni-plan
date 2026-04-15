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
