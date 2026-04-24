#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace UniPlan
{

// v0.105.0+ helper shared by every mutation command that supports the
// --<field>-append-file flag family. Given the existing stored value
// for a prose field, the new value the caller supplied, and whether
// the caller asked for append semantics (via the field name being
// present in the caller's mAppendFields set), return the value that
// should be written to the bundle.
//
// Append semantics:
//   - If the existing value is empty, append is equivalent to replace
//     (no leading "\n\n" seam). This matches the intuition "append to
//     nothing = just set".
//   - Otherwise, the result is existing + "\n\n" + new. The seam is
//     always exactly one blank line (double-newline), regardless of
//     trailing whitespace on the existing value or leading whitespace
//     on the new value. Authors who want a different seam use
//     pull + local-edit + --<field>-file replace.
//
// When InAppend is false the helper is a pure passthrough of InNew —
// the replace path. Handler call sites thus become one uniform line:
//     NewValue = ComputeAppendOrReplace(Existing, Options.mX,
//                   Options.mBase.mAppendFields.count("x") > 0);
inline std::string ComputeAppendOrReplace(const std::string &InExisting,
                                          const std::string &InNew,
                                          bool InAppend)
{
    if (!InAppend)
    {
        return InNew;
    }
    if (InExisting.empty())
    {
        return InNew;
    }
    return InExisting + "\n\n" + InNew;
}

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
    std::tm UTCTime{};
#ifdef _WIN32
    gmtime_s(&UTCTime, &Timestamp);
#else
    gmtime_r(&Timestamp, &UTCTime);
#endif
    std::ostringstream Stream;
    Stream << std::put_time(&UTCTime, "%Y-%m-%dT%H:%M:%SZ");
    return Stream.str();
}

// Shared with the `timestamp_format` validator in UniPlanValidation.cpp.
// Anchored to the start of the value; an optional time suffix is permitted.
// Accepts both bare ISO dates ("2026-04-19") and full ISO timestamps
// ("2026-04-19T12:34:56Z").
inline bool IsValidISOTimestampValue(const std::string &InValue)
{
    static const std::regex Pattern(
        R"(^\d{4}-\d{2}-\d{2}(T\d{2}:\d{2}:\d{2}.*)?)");
    return std::regex_match(InValue, Pattern);
}

// Parse a comma-separated list of non-negative integers. Each field is
// Trim()-ed; empty fields, negative values, and non-numeric tokens throw
// std::invalid_argument. Duplicates are preserved by this helper — the
// caller is responsible for deduplication (see ParsePhaseGetOptions for
// the `--phases` flag, where dedupe + sort happens post-parse).
//
// Added v0.84.0 for the `phase get --phases 1,3,5` batch mode. Kept
// free-function + header-inline so additional CSV-flag consumers can
// reuse it without pulling in heavyweight dependencies.
inline std::vector<int> SplitCsvInts(const std::string &InValue)
{
    std::vector<int> Out;
    std::string Cur;
    const auto Emit = [&]()
    {
        const std::string Token = Trim(Cur);
        Cur.clear();
        if (Token.empty())
            throw std::invalid_argument("empty field in integer list");
        for (const char C : Token)
        {
            if (!std::isdigit(static_cast<unsigned char>(C)))
                throw std::invalid_argument("non-numeric token: " + Token);
        }
        Out.push_back(std::atoi(Token.c_str()));
    };
    for (const char C : InValue)
    {
        if (C == ',')
            Emit();
        else
            Cur.push_back(C);
    }
    Emit();
    return Out;
}

// LegacyMdContentLineCount lives in UniPlanFileHelpers.h — it reads from disk,
// so it belongs with the other filesystem helpers rather than this
// pure-string-operations header.

} // namespace UniPlan
