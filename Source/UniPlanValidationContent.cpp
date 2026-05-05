#include "UniPlanEnums.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanPhaseKind.h"
#include "UniPlanProcessHelpers.h"
#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace UniPlan
{

static bool IsPlanPathTokenChar(const char InChar)
{
    const unsigned char Value = static_cast<unsigned char>(InChar);
    return std::isalnum(Value) != 0 || InChar == '_' || InChar == '.' ||
           InChar == '-';
}

static bool IsDevUserNameChar(const char InChar)
{
    const unsigned char Value = static_cast<unsigned char>(InChar);
    return std::isalnum(Value) != 0 || InChar == '_' || InChar == '.' ||
           InChar == '-';
}

static bool IsRegexWordChar(const char InChar)
{
    const unsigned char Value = static_cast<unsigned char>(InChar);
    return std::isalnum(Value) != 0 || InChar == '_';
}

static bool StringEndsWith(const std::string &InValue,
                           const std::string &InSuffix)
{
    return InValue.size() >= InSuffix.size() &&
           InValue.compare(InValue.size() - InSuffix.size(), InSuffix.size(),
                           InSuffix) == 0;
}

static bool TryFindImpossiblePlanPathRef(const std::string &InContent,
                                         std::string &OutMatch)
{
    static constexpr std::array<const char *, 2> kBadPrefixes = {
        "Docs/Implementation/", "Docs/Playbooks/"};
    static const std::string kSuffix = ".Plan.json";

    for (const char *rpPrefix : kBadPrefixes)
    {
        const std::string Prefix(rpPrefix);
        size_t SearchFrom = 0;
        while (SearchFrom < InContent.size())
        {
            const size_t PrefixPos = InContent.find(Prefix, SearchFrom);
            if (PrefixPos == std::string::npos)
            {
                break;
            }

            size_t End = PrefixPos + Prefix.size();
            if (End < InContent.size() && InContent[End] == '`')
            {
                ++End;
            }

            const size_t NameStart = End;
            while (End < InContent.size() &&
                   IsPlanPathTokenChar(InContent[End]))
            {
                ++End;
            }
            while (End > NameStart)
            {
                const unsigned char Last =
                    static_cast<unsigned char>(InContent[End - 1]);
                if (std::isalnum(Last) != 0)
                {
                    break;
                }
                --End;
            }

            if (End > NameStart)
            {
                const std::string Filename =
                    InContent.substr(NameStart, End - NameStart);
                if (StringEndsWith(Filename, kSuffix))
                {
                    OutMatch = InContent.substr(PrefixPos, End - PrefixPos);
                    return true;
                }
            }

            SearchFrom = PrefixPos + Prefix.size();
        }
    }

    return false;
}

static bool TryFindUnixDevAbsolutePath(const std::string &InContent,
                                       const std::string &InPrefix,
                                       std::string &OutMatch)
{
    size_t SearchFrom = 0;
    while (SearchFrom < InContent.size())
    {
        const size_t PrefixPos = InContent.find(InPrefix, SearchFrom);
        if (PrefixPos == std::string::npos)
        {
            break;
        }

        size_t End = PrefixPos + InPrefix.size();
        if (End >= InContent.size())
        {
            break;
        }

        const unsigned char First = static_cast<unsigned char>(InContent[End]);
        if (std::islower(First) == 0)
        {
            SearchFrom = End + 1;
            continue;
        }

        while (End < InContent.size() && IsDevUserNameChar(InContent[End]))
        {
            ++End;
        }
        if (End < InContent.size() && InContent[End] == '/')
        {
            ++End;
            OutMatch = InContent.substr(PrefixPos, End - PrefixPos);
            return true;
        }

        SearchFrom = End + 1;
    }
    return false;
}

static bool TryFindWindowsDevAbsolutePath(const std::string &InContent,
                                          std::string &OutMatch)
{
    static const std::string kNeedle = ":\\Users\\";

    size_t SearchFrom = 0;
    while (SearchFrom < InContent.size())
    {
        const size_t NeedlePos = InContent.find(kNeedle, SearchFrom);
        if (NeedlePos == std::string::npos)
        {
            break;
        }
        if (NeedlePos == 0)
        {
            SearchFrom = NeedlePos + kNeedle.size();
            continue;
        }

        const size_t DrivePos = NeedlePos - 1;
        const unsigned char Drive =
            static_cast<unsigned char>(InContent[DrivePos]);
        if (std::isupper(Drive) == 0)
        {
            SearchFrom = NeedlePos + kNeedle.size();
            continue;
        }

        size_t End = NeedlePos + kNeedle.size();
        const size_t NameStart = End;
        while (End < InContent.size() && IsDevUserNameChar(InContent[End]))
        {
            ++End;
        }
        if (End > NameStart)
        {
            OutMatch = InContent.substr(DrivePos, End - DrivePos);
            return true;
        }

        SearchFrom = NeedlePos + kNeedle.size();
    }
    return false;
}

static bool TryFindDevAbsolutePath(const std::string &InContent,
                                   std::string &OutMatch)
{
    return TryFindUnixDevAbsolutePath(InContent, "/Users/", OutMatch) ||
           TryFindUnixDevAbsolutePath(InContent, "/home/", OutMatch) ||
           TryFindWindowsDevAbsolutePath(InContent, OutMatch);
}

static bool HasWordMarkerAt(const std::string &InContent, const size_t InPos,
                            const std::string &InMarker)
{
    if (InPos + InMarker.size() > InContent.size())
    {
        return false;
    }
    if (InContent.compare(InPos, InMarker.size(), InMarker) != 0)
    {
        return false;
    }
    if (InPos > 0 && IsRegexWordChar(InContent[InPos - 1]))
    {
        return false;
    }
    const size_t After = InPos + InMarker.size();
    return After >= InContent.size() || !IsRegexWordChar(InContent[After]);
}

static bool TryFindUnresolvedMarker(const std::string &InContent,
                                    std::string &OutMatch)
{
    static constexpr std::array<const char *, 4> kWordMarkers = {
        "TODO", "FIXME", "XXX", "HACK"};

    for (size_t Pos = 0; Pos < InContent.size(); ++Pos)
    {
        if (Pos + 3 <= InContent.size() &&
            InContent.compare(Pos, 3, "???") == 0)
        {
            OutMatch = "???";
            return true;
        }
        for (const char *rpMarker : kWordMarkers)
        {
            const std::string Marker(rpMarker);
            if (HasWordMarkerAt(InContent, Pos, Marker))
            {
                OutMatch = Marker;
                return true;
            }
        }
    }

    return false;
}

static bool HasWordBoundaryBefore(const std::string &InContent,
                                  const size_t InPos)
{
    return InPos == 0 || !IsRegexWordChar(InContent[InPos - 1]);
}

static bool HasWordBoundaryAfter(const std::string &InContent,
                                 const size_t InPos)
{
    return InPos >= InContent.size() || !IsRegexWordChar(InContent[InPos]);
}

static bool TryReadDigits(const std::string &InContent, size_t &InOutPos)
{
    const size_t Start = InOutPos;
    while (InOutPos < InContent.size())
    {
        const unsigned char Value =
            static_cast<unsigned char>(InContent[InOutPos]);
        if (std::isdigit(Value) == 0)
        {
            break;
        }
        ++InOutPos;
    }
    return InOutPos > Start;
}

static bool TryFindLocalhostEndpoint(const std::string &InContent,
                                     std::string &OutMatch)
{
    static const std::string kPrefix = "localhost:";

    size_t SearchFrom = 0;
    while (SearchFrom < InContent.size())
    {
        const size_t Pos = InContent.find(kPrefix, SearchFrom);
        if (Pos == std::string::npos)
        {
            break;
        }
        if (!HasWordBoundaryBefore(InContent, Pos))
        {
            SearchFrom = Pos + kPrefix.size();
            continue;
        }

        size_t End = Pos + kPrefix.size();
        if (TryReadDigits(InContent, End) &&
            HasWordBoundaryAfter(InContent, End))
        {
            OutMatch = InContent.substr(Pos, End - Pos);
            return true;
        }
        SearchFrom = Pos + kPrefix.size();
    }
    return false;
}

static bool TryFindWordLiteral(const std::string &InContent,
                               const std::string &InLiteral,
                               std::string &OutMatch)
{
    size_t SearchFrom = 0;
    while (SearchFrom < InContent.size())
    {
        const size_t Pos = InContent.find(InLiteral, SearchFrom);
        if (Pos == std::string::npos)
        {
            break;
        }
        const size_t End = Pos + InLiteral.size();
        if (HasWordBoundaryBefore(InContent, Pos) &&
            HasWordBoundaryAfter(InContent, End))
        {
            OutMatch = InLiteral;
            return true;
        }
        SearchFrom = End;
    }
    return false;
}

static bool TryFindPrivateIpv4Prefix(const std::string &InContent,
                                     const std::string &InPrefix,
                                     const int InRemainingOctets,
                                     std::string &OutMatch)
{
    size_t SearchFrom = 0;
    while (SearchFrom < InContent.size())
    {
        const size_t Pos = InContent.find(InPrefix, SearchFrom);
        if (Pos == std::string::npos)
        {
            break;
        }
        if (!HasWordBoundaryBefore(InContent, Pos))
        {
            SearchFrom = Pos + InPrefix.size();
            continue;
        }

        size_t End = Pos + InPrefix.size();
        bool bMatched = true;
        for (int Octet = 0; Octet < InRemainingOctets; ++Octet)
        {
            if (!TryReadDigits(InContent, End))
            {
                bMatched = false;
                break;
            }
            if (Octet + 1 < InRemainingOctets)
            {
                if (End >= InContent.size() || InContent[End] != '.')
                {
                    bMatched = false;
                    break;
                }
                ++End;
            }
        }
        if (bMatched && HasWordBoundaryAfter(InContent, End))
        {
            OutMatch = InContent.substr(Pos, End - Pos);
            return true;
        }
        SearchFrom = Pos + InPrefix.size();
    }
    return false;
}

static bool TryFindHardcodedEndpoint(const std::string &InContent,
                                     std::string &OutMatch)
{
    return TryFindLocalhostEndpoint(InContent, OutMatch) ||
           TryFindWordLiteral(InContent, "127.0.0.1", OutMatch) ||
           TryFindPrivateIpv4Prefix(InContent, "192.168.", 2, OutMatch) ||
           TryFindPrivateIpv4Prefix(InContent, "10.", 3, OutMatch);
}

static bool EqualAsciiCaseInsensitive(const std::string &InContent,
                                      const size_t InPos,
                                      const std::string &InNeedle)
{
    if (InPos + InNeedle.size() > InContent.size())
    {
        return false;
    }
    for (size_t I = 0; I < InNeedle.size(); ++I)
    {
        const unsigned char L =
            static_cast<unsigned char>(InContent[InPos + I]);
        const unsigned char R = static_cast<unsigned char>(InNeedle[I]);
        if (std::tolower(L) != std::tolower(R))
        {
            return false;
        }
    }
    return true;
}

static bool TryMatchHtmlTagAt(const std::string &InContent, const size_t InPos,
                              std::string &OutMatch)
{
    if (InContent[InPos] != '<')
    {
        return false;
    }

    const size_t NamePos = InPos + 1;
    static constexpr std::array<const char *, 4> kNames = {"br", "div", "span",
                                                           "p"};
    for (const char *rpName : kNames)
    {
        const std::string Name(rpName);
        const size_t After = NamePos + Name.size();
        if (EqualAsciiCaseInsensitive(InContent, NamePos, Name) &&
            HasWordBoundaryAfter(InContent, After))
        {
            OutMatch = InContent.substr(InPos, After - InPos);
            return true;
        }
    }

    if (NamePos + 2 <= InContent.size() &&
        (InContent[NamePos] == 'h' || InContent[NamePos] == 'H') &&
        InContent[NamePos + 1] >= '1' && InContent[NamePos + 1] <= '6' &&
        HasWordBoundaryAfter(InContent, NamePos + 2))
    {
        OutMatch = InContent.substr(InPos, 3);
        return true;
    }

    return false;
}

static bool TryFindHtmlTagInProse(const std::string &InContent,
                                  std::string &OutMatch)
{
    size_t SearchFrom = 0;
    while (SearchFrom < InContent.size())
    {
        const size_t Pos = InContent.find('<', SearchFrom);
        if (Pos == std::string::npos)
        {
            break;
        }
        if (TryMatchHtmlTagAt(InContent, Pos, OutMatch))
        {
            return true;
        }
        SearchFrom = Pos + 1;
    }
    return false;
}

static std::string TrimAsciiWhitespace(const std::string &InValue)
{
    size_t Start = 0;
    while (Start < InValue.size())
    {
        const unsigned char Value = static_cast<unsigned char>(InValue[Start]);
        if (std::isspace(Value) == 0)
        {
            break;
        }
        ++Start;
    }

    size_t End = InValue.size();
    while (End > Start)
    {
        const unsigned char Value =
            static_cast<unsigned char>(InValue[End - 1]);
        if (std::isspace(Value) == 0)
        {
            break;
        }
        --End;
    }
    return InValue.substr(Start, End - Start);
}

static std::string ToLowerAscii(const std::string &InValue)
{
    std::string Result = InValue;
    for (char &Char : Result)
    {
        const unsigned char Value = static_cast<unsigned char>(Char);
        Char = static_cast<char>(std::tolower(Value));
    }
    return Result;
}

static bool TryFindClassicPlaceholderLiteral(const std::string &InContent,
                                             std::string &OutMatch)
{
    const std::string Trimmed = TrimAsciiWhitespace(InContent);
    static constexpr std::array<const char *, 7> kPlaceholders = {
        "None", "N/A", "n/a", "none", "TBD", "tbd", "-"};
    for (const char *rpPlaceholder : kPlaceholders)
    {
        if (Trimmed == rpPlaceholder)
        {
            OutMatch = Trimmed;
            return true;
        }
    }
    return false;
}

static bool IsStatusSeparator(const char InChar)
{
    const unsigned char Value = static_cast<unsigned char>(InChar);
    return InChar == '_' || std::isspace(Value) != 0;
}

static bool EqualsTwoPartStatus(const std::string &InValue,
                                const std::string &InLeft,
                                const std::string &InRight)
{
    const size_t ExpectedSize = InLeft.size() + 1 + InRight.size();
    return InValue.size() == ExpectedSize &&
           InValue.compare(0, InLeft.size(), InLeft) == 0 &&
           IsStatusSeparator(InValue[InLeft.size()]) &&
           InValue.compare(InLeft.size() + 1, InRight.size(), InRight) == 0;
}

static bool IsStatusQuoteChar(const char InChar)
{
    return InChar == '`' || InChar == '"' || InChar == '\'';
}

static bool TryFindStatusPlaceholderLiteral(const std::string &InContent,
                                            std::string &OutMatch)
{
    std::string Trimmed = TrimAsciiWhitespace(InContent);
    while (!Trimmed.empty() && IsStatusQuoteChar(Trimmed.front()))
    {
        Trimmed.erase(Trimmed.begin());
    }
    while (!Trimmed.empty() && IsStatusQuoteChar(Trimmed.back()))
    {
        Trimmed.pop_back();
    }
    Trimmed = TrimAsciiWhitespace(Trimmed);
    const std::string Lower = ToLowerAscii(Trimmed);

    static constexpr std::array<const char *, 6> kStatusPlaceholders = {
        "complete", "completed", "blocked", "pending", "wip", "done"};
    for (const char *rpPlaceholder : kStatusPlaceholders)
    {
        if (Lower == rpPlaceholder)
        {
            OutMatch = Trimmed;
            return true;
        }
    }
    if (EqualsTwoPartStatus(Lower, "in", "progress") ||
        EqualsTwoPartStatus(Lower, "not", "started"))
    {
        OutMatch = Trimmed;
        return true;
    }
    return false;
}

static void FindTopicPlanJsonReferences(const std::string &InContent,
                                        std::set<std::string> &OutReferences)
{
    static const std::string kSuffix = ".Plan.json";
    size_t SearchFrom = 0;
    while (SearchFrom < InContent.size())
    {
        const size_t SuffixPos = InContent.find(kSuffix, SearchFrom);
        if (SuffixPos == std::string::npos)
        {
            break;
        }

        size_t Start = SuffixPos;
        while (Start > 0)
        {
            const unsigned char Prev =
                static_cast<unsigned char>(InContent[Start - 1]);
            if (std::isalnum(Prev) == 0)
            {
                break;
            }
            --Start;
        }

        const size_t Length = SuffixPos - Start;
        if (Length >= 2)
        {
            const unsigned char First =
                static_cast<unsigned char>(InContent[Start]);
            if (std::isupper(First) != 0)
            {
                OutReferences.insert(InContent.substr(Start, Length));
            }
        }

        SearchFrom = SuffixPos + kSuffix.size();
    }
}

static bool TryFindSmartTypography(const std::string &InContent,
                                   std::string &OutMatch)
{
    static constexpr std::array<const char *, 6> kSequences = {
        "\xE2\x80\x98", "\xE2\x80\x99", "\xE2\x80\x9C",
        "\xE2\x80\x9D", "\xE2\x80\x93", "\xE2\x80\x94"};
    for (size_t Pos = 0; Pos + 3 <= InContent.size(); ++Pos)
    {
        if (static_cast<unsigned char>(InContent[Pos]) != 0xE2)
        {
            continue;
        }
        for (const char *rpSequence : kSequences)
        {
            if (InContent.compare(Pos, 3, rpSequence) == 0)
            {
                OutMatch = InContent.substr(Pos, 3);
                return true;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Content-hygiene helper: scan one prose string and emit a single failure
// when the pattern hits. High-volume simple checks route through
// deterministic string scanners first; the regex fallback is reserved for
// lower-volume patterns whose matching semantics are not a startup hotspot.
// ---------------------------------------------------------------------------

static void
ScanProseField(const std::string &InTopic, const std::string &InPath,
               const std::string &InContent, const std::regex &InPattern,
               const std::string &InCheckID, EValidationSeverity InSeverity,
               const std::string &InDetailPrefix,
               std::vector<ValidateCheck> &OutChecks)
{
    if (InContent.empty())
        return;
    if (InCheckID == "no_dev_absolute_path")
    {
        std::string Match;
        if (TryFindDevAbsolutePath(InContent, Match))
        {
            Fail(OutChecks, InCheckID, InSeverity, InTopic, InPath,
                 InDetailPrefix + Match);
        }
        return;
    }
    if (InCheckID == "no_unresolved_marker")
    {
        std::string Match;
        if (TryFindUnresolvedMarker(InContent, Match))
        {
            Fail(OutChecks, InCheckID, InSeverity, InTopic, InPath,
                 InDetailPrefix + Match);
        }
        return;
    }
    if (InCheckID == "no_hardcoded_endpoint")
    {
        std::string Match;
        if (TryFindHardcodedEndpoint(InContent, Match))
        {
            Fail(OutChecks, InCheckID, InSeverity, InTopic, InPath,
                 InDetailPrefix + Match);
        }
        return;
    }
    if (InCheckID == "no_smart_quotes")
    {
        std::string Match;
        if (TryFindSmartTypography(InContent, Match))
        {
            Fail(OutChecks, InCheckID, InSeverity, InTopic, InPath,
                 InDetailPrefix + Match);
        }
        return;
    }
    if (InCheckID == "no_html_in_prose")
    {
        std::string Match;
        if (TryFindHtmlTagInProse(InContent, Match))
        {
            Fail(OutChecks, InCheckID, InSeverity, InTopic, InPath,
                 InDetailPrefix + Match);
        }
        return;
    }
    if (InCheckID == "no_empty_placeholder_literal")
    {
        std::string Match;
        const bool bMatched =
            InDetailPrefix == "status-word placeholder: "
                ? TryFindStatusPlaceholderLiteral(InContent, Match)
                : TryFindClassicPlaceholderLiteral(InContent, Match);
        if (bMatched)
        {
            Fail(OutChecks, InCheckID, InSeverity, InTopic, InPath,
                 InDetailPrefix + Match);
        }
        return;
    }
    std::smatch Match;
    if (std::regex_search(InContent, Match, InPattern))
    {
        // v0.97.0 no-truncation contract: embed the full regex match
        // in the issue detail. Callers that triage validator output
        // can still filter by check id / path / severity before
        // reading detail.
        Fail(OutChecks, InCheckID, InSeverity, InTopic, InPath,
             InDetailPrefix + Match.str());
    }
}

// ---------------------------------------------------------------------------
// Content-hygiene helper: scan every topic-level prose field.
// ---------------------------------------------------------------------------

static void ScanTopicProse(const FTopicBundle &InBundle,
                           const std::regex &InPattern,
                           const std::string &InCheckID,
                           EValidationSeverity InSeverity,
                           const std::string &InDetailPrefix,
                           std::vector<ValidateCheck> &OutChecks)
{
    const FPlanMetadata &M = InBundle.mMetadata;
    const std::string &Key = InBundle.mTopicKey;
    const auto Scan = [&](const std::string &InField, const std::string &InVal)
    {
        ScanProseField(Key, InField, InVal, InPattern, InCheckID, InSeverity,
                       InDetailPrefix, OutChecks);
    };
    Scan("summary", M.mSummary);
    Scan("goals", M.mGoals);
    Scan("non_goals", M.mNonGoals);
    // risks is a typed vector — scan each entry's prose fields individually
    // so path references like `risks[3].statement` surface in the issue
    // output.
    for (size_t I = 0; I < M.mRisks.size(); ++I)
    {
        const FRiskEntry &R = M.mRisks[I];
        const std::string Base = "risks[" + std::to_string(I) + "]";
        Scan(Base + ".statement", R.mStatement);
        Scan(Base + ".mitigation", R.mMitigation);
        Scan(Base + ".notes", R.mNotes);
    }
    // acceptance_criteria is a typed vector — same per-entry scanning.
    for (size_t I = 0; I < M.mAcceptanceCriteria.size(); ++I)
    {
        const FAcceptanceCriterionEntry &C = M.mAcceptanceCriteria[I];
        const std::string Base =
            "acceptance_criteria[" + std::to_string(I) + "]";
        Scan(Base + ".statement", C.mStatement);
        Scan(Base + ".measure", C.mMeasure);
        Scan(Base + ".evidence", C.mEvidence);
    }
    Scan("problem_statement", M.mProblemStatement);
    // validation_commands is a typed vector now — scan each element's
    // command + description prose individually so path references like
    // `validation_commands[3].command` surface in the issue output.
    for (size_t I = 0; I < M.mValidationCommands.size(); ++I)
    {
        const FValidationCommand &C = M.mValidationCommands[I];
        const std::string Base =
            "validation_commands[" + std::to_string(I) + "]";
        Scan(Base + ".command", C.mCommand);
        Scan(Base + ".description", C.mDescription);
    }
    Scan("baseline_audit", M.mBaselineAudit);
    Scan("execution_strategy", M.mExecutionStrategy);
    Scan("locked_decisions", M.mLockedDecisions);
    Scan("source_references", M.mSourceReferences);
    // dependencies is a typed vector — scan each record's prose fields.
    for (size_t I = 0; I < M.mDependencies.size(); ++I)
    {
        const FBundleReference &R = M.mDependencies[I];
        const std::string Base = "dependencies[" + std::to_string(I) + "]";
        Scan(Base + ".path", R.mPath);
        Scan(Base + ".note", R.mNote);
    }
    // next_actions is a typed vector — scan per-entry prose fields.
    for (size_t I = 0; I < InBundle.mNextActions.size(); ++I)
    {
        const FNextActionEntry &A = InBundle.mNextActions[I];
        const std::string Base = "next_actions[" + std::to_string(I) + "]";
        Scan(Base + ".statement", A.mStatement);
        Scan(Base + ".rationale", A.mRationale);
        Scan(Base + ".owner", A.mOwner);
        Scan(Base + ".target_date", A.mTargetDate);
    }
}

// ---------------------------------------------------------------------------
// Phase-field classification for content-hygiene scans.
//
// Phase prose is a mix of prescriptive contract (what the phase will do) and
// evidence (what ran / what proved it). The two classes carry different
// hygiene contracts:
//
//   - Prescriptive fields describe forward-looking intent. They must be
//     V4-clean and agent-parseable regardless of whether the phase has run
//     yet. Scanning is status-agnostic.
//
//   - Evidence fields capture historical execution. V3 vocabulary or legacy
//     CLI references in completed-phase evidence are legitimate records of
//     what actually happened at that time. Scanning evidence on completed
//     phases for drift produces perpetual false positives. Scanning evidence
//     on not-started / in-progress phases is still useful — the author is
//     still drafting that prose, so drift is still correctable.
//
//   - Unresolved-marker semantics invert: TODO markers in evidence on a
//     completed phase signal premature closure, so that check scans evidence
//     only on completed phases.
//
// EPhaseEvidenceScope lets each check declare which phase-status slice of
// evidence prose it cares about. ScanPhaseLifecycleProse keeps its own
// InOnlyCompleted flag for historical reasons; evidence sub-fields that
// live inside the design section flow through ScanPhaseDesignEvidenceProse
// with an explicit scope.
// ---------------------------------------------------------------------------

enum class EPhaseEvidenceScope
{
    AllPhases,     // format checks: scan evidence regardless of status
    NotCompleted,  // drift checks: skip completed phases (historical log)
    CompletedOnly, // no_unresolved_marker: only flag post-closure residue
};

// ---------------------------------------------------------------------------
// ScanPhaseDesignPrescriptiveProse — scans the forward-looking contract
// fields of every phase. Always scans every phase regardless of status;
// prescriptive contracts must stay V4-clean whether the phase has executed
// or not.
//
// Evidence sub-fields (tasks[].evidence, tasks[].notes, testing[].evidence)
// are handled separately by ScanPhaseDesignEvidenceProse; lifecycle fields
// (done/remaining/blockers) by ScanPhaseLifecycleProse.
// ---------------------------------------------------------------------------

static void ScanPhaseDesignPrescriptiveProse(
    const FTopicBundle &InBundle, const std::regex &InPattern,
    const std::string &InCheckID, EValidationSeverity InSeverity,
    const std::string &InDetailPrefix, std::vector<ValidateCheck> &OutChecks)
{
    const std::string &Key = InBundle.mTopicKey;
    for (size_t PI = 0; PI < InBundle.mPhases.size(); ++PI)
    {
        const FPhaseRecord &P = InBundle.mPhases[PI];
        const std::string Base = "phases[" + std::to_string(PI) + "]";
        const auto Scan =
            [&](const std::string &InSuffix, const std::string &InVal)
        {
            ScanProseField(Key, Base + "." + InSuffix, InVal, InPattern,
                           InCheckID, InSeverity, InDetailPrefix, OutChecks);
        };
        Scan("scope", P.mScope);
        Scan("output", P.mOutput);
        Scan("investigation", P.mDesign.mInvestigation);
        Scan("code_snippets", P.mDesign.mCodeSnippets);
        for (size_t DI = 0; DI < P.mDesign.mDependencies.size(); ++DI)
        {
            const FBundleReference &R = P.mDesign.mDependencies[DI];
            const std::string DBase =
                "dependencies[" + std::to_string(DI) + "]";
            Scan(DBase + ".path", R.mPath);
            Scan(DBase + ".note", R.mNote);
        }
        Scan("readiness_gate", P.mDesign.mReadinessGate);
        Scan("handoff", P.mDesign.mHandoff);
        Scan("code_entity_contract", P.mDesign.mCodeEntityContract);
        Scan("best_practices", P.mDesign.mBestPractices);
        for (size_t CI = 0; CI < P.mDesign.mValidationCommands.size(); ++CI)
        {
            const FValidationCommand &C = P.mDesign.mValidationCommands[CI];
            const std::string VCBase =
                "validation_commands[" + std::to_string(CI) + "]";
            Scan(VCBase + ".command", C.mCommand);
            Scan(VCBase + ".description", C.mDescription);
        }
        Scan("multi_platforming", P.mDesign.mMultiPlatforming);
        for (size_t LI = 0; LI < P.mLanes.size(); ++LI)
        {
            const FLaneRecord &L = P.mLanes[LI];
            const std::string LBase =
                Base + ".lanes[" + std::to_string(LI) + "]";
            ScanProseField(Key, LBase + ".scope", L.mScope, InPattern,
                           InCheckID, InSeverity, InDetailPrefix, OutChecks);
            ScanProseField(Key, LBase + ".exit_criteria", L.mExitCriteria,
                           InPattern, InCheckID, InSeverity, InDetailPrefix,
                           OutChecks);
        }
        for (size_t JI = 0; JI < P.mJobs.size(); ++JI)
        {
            const FJobRecord &J = P.mJobs[JI];
            const std::string JBase =
                Base + ".jobs[" + std::to_string(JI) + "]";
            ScanProseField(Key, JBase + ".scope", J.mScope, InPattern,
                           InCheckID, InSeverity, InDetailPrefix, OutChecks);
            ScanProseField(Key, JBase + ".output", J.mOutput, InPattern,
                           InCheckID, InSeverity, InDetailPrefix, OutChecks);
            ScanProseField(Key, JBase + ".exit_criteria", J.mExitCriteria,
                           InPattern, InCheckID, InSeverity, InDetailPrefix,
                           OutChecks);
            for (size_t TI = 0; TI < J.mTasks.size(); ++TI)
            {
                const FTaskRecord &T = J.mTasks[TI];
                const std::string TBase =
                    JBase + ".tasks[" + std::to_string(TI) + "]";
                ScanProseField(Key, TBase + ".description", T.mDescription,
                               InPattern, InCheckID, InSeverity, InDetailPrefix,
                               OutChecks);
            }
        }
        for (size_t TI = 0; TI < P.mTesting.size(); ++TI)
        {
            const FTestingRecord &TR = P.mTesting[TI];
            const std::string TBase =
                Base + ".testing[" + std::to_string(TI) + "]";
            ScanProseField(Key, TBase + ".step", TR.mStep, InPattern, InCheckID,
                           InSeverity, InDetailPrefix, OutChecks);
            ScanProseField(Key, TBase + ".action", TR.mAction, InPattern,
                           InCheckID, InSeverity, InDetailPrefix, OutChecks);
            ScanProseField(Key, TBase + ".expected", TR.mExpected, InPattern,
                           InCheckID, InSeverity, InDetailPrefix, OutChecks);
        }
    }
}

// ---------------------------------------------------------------------------
// ScanPhaseDesignEvidenceProse — scans the evidence sub-fields embedded in
// the design section: tasks[].evidence, tasks[].notes, testing[].evidence.
// These fields record execution rather than contract, so drift checks should
// treat completed-phase content as historical log.
//
// Callers declare the slice they care about via EPhaseEvidenceScope.
// ---------------------------------------------------------------------------

static void ScanPhaseDesignEvidenceProse(const FTopicBundle &InBundle,
                                         const std::regex &InPattern,
                                         const std::string &InCheckID,
                                         EValidationSeverity InSeverity,
                                         const std::string &InDetailPrefix,
                                         std::vector<ValidateCheck> &OutChecks,
                                         EPhaseEvidenceScope InScope)
{
    const std::string &Key = InBundle.mTopicKey;
    for (size_t PI = 0; PI < InBundle.mPhases.size(); ++PI)
    {
        const FPhaseRecord &P = InBundle.mPhases[PI];
        const bool bCompleted =
            P.mLifecycle.mStatus == EExecutionStatus::Completed;
        switch (InScope)
        {
        case EPhaseEvidenceScope::AllPhases:
            break;
        case EPhaseEvidenceScope::NotCompleted:
            if (bCompleted)
                continue;
            break;
        case EPhaseEvidenceScope::CompletedOnly:
            if (!bCompleted)
                continue;
            break;
        }
        const std::string Base = "phases[" + std::to_string(PI) + "]";
        for (size_t JI = 0; JI < P.mJobs.size(); ++JI)
        {
            const FJobRecord &J = P.mJobs[JI];
            const std::string JBase =
                Base + ".jobs[" + std::to_string(JI) + "]";
            for (size_t TI = 0; TI < J.mTasks.size(); ++TI)
            {
                const FTaskRecord &T = J.mTasks[TI];
                const std::string TBase =
                    JBase + ".tasks[" + std::to_string(TI) + "]";
                ScanProseField(Key, TBase + ".evidence", T.mEvidence, InPattern,
                               InCheckID, InSeverity, InDetailPrefix,
                               OutChecks);
                ScanProseField(Key, TBase + ".notes", T.mNotes, InPattern,
                               InCheckID, InSeverity, InDetailPrefix,
                               OutChecks);
            }
        }
        for (size_t TI = 0; TI < P.mTesting.size(); ++TI)
        {
            const FTestingRecord &TR = P.mTesting[TI];
            const std::string TBase =
                Base + ".testing[" + std::to_string(TI) + "]";
            ScanProseField(Key, TBase + ".evidence", TR.mEvidence, InPattern,
                           InCheckID, InSeverity, InDetailPrefix, OutChecks);
        }
    }
}

// ---------------------------------------------------------------------------
// Content-hygiene helper: scan per-phase *lifecycle* (historical evidence)
// prose — done/remaining/blockers only. agent_context is intentionally
// excluded (it's scratch context, not plan data).
//
// Uses EPhaseEvidenceScope to express status filtering uniformly with
// ScanPhaseDesignEvidenceProse:
//   - AllPhases      — format checks that apply regardless of status.
//   - NotCompleted   — drift checks; completed-phase lifecycle is a
//                      historical log and legitimate V3 vocabulary there
//                      records what actually happened.
//   - CompletedOnly  — no_unresolved_marker; TODO markers in a closed
//                      phase's lifecycle signal premature closure.
// ---------------------------------------------------------------------------

static void ScanPhaseLifecycleProse(const FTopicBundle &InBundle,
                                    const std::regex &InPattern,
                                    const std::string &InCheckID,
                                    EValidationSeverity InSeverity,
                                    const std::string &InDetailPrefix,
                                    std::vector<ValidateCheck> &OutChecks,
                                    EPhaseEvidenceScope InScope)
{
    const std::string &Key = InBundle.mTopicKey;
    for (size_t PI = 0; PI < InBundle.mPhases.size(); ++PI)
    {
        const FPhaseRecord &P = InBundle.mPhases[PI];
        const bool bCompleted =
            P.mLifecycle.mStatus == EExecutionStatus::Completed;
        switch (InScope)
        {
        case EPhaseEvidenceScope::AllPhases:
            break;
        case EPhaseEvidenceScope::NotCompleted:
            if (bCompleted)
                continue;
            break;
        case EPhaseEvidenceScope::CompletedOnly:
            if (!bCompleted)
                continue;
            break;
        }
        const std::string Base = "phases[" + std::to_string(PI) + "]";
        ScanProseField(Key, Base + ".done", P.mLifecycle.mDone, InPattern,
                       InCheckID, InSeverity, InDetailPrefix, OutChecks);
        ScanProseField(Key, Base + ".remaining", P.mLifecycle.mRemaining,
                       InPattern, InCheckID, InSeverity, InDetailPrefix,
                       OutChecks);
        ScanProseField(Key, Base + ".blockers", P.mLifecycle.mBlockers,
                       InPattern, InCheckID, InSeverity, InDetailPrefix,
                       OutChecks);
    }
}

// ---------------------------------------------------------------------------
// V4 Bundle Validation — evaluators
// ---------------------------------------------------------------------------

// 1. required_fields (ErrorMajor)
void EvalPathResolves(const std::vector<FTopicBundle> &InBundles,
                      std::vector<ValidateCheck> &OutChecks)
{
    const auto Scan = [&](const std::string &InTopic, const std::string &InPath,
                          const std::string &InContent)
    {
        if (InContent.empty())
            return;
        std::string Match;
        if (TryFindImpossiblePlanPathRef(InContent, Match))
        {
            Fail(OutChecks, "path_resolves", EValidationSeverity::ErrorMinor,
                 InTopic, InPath,
                 "impossible path ref (.Plan.json lives at Docs/Plans/, "
                 "not Docs/Implementation|Playbooks/): " +
                     Match);
        }
    };

    const auto ScanDeps = [&](const std::string &InTopic,
                              const std::string &InBase,
                              const std::vector<FBundleReference> &InDeps)
    {
        for (size_t I = 0; I < InDeps.size(); ++I)
        {
            const FBundleReference &R = InDeps[I];
            const std::string B = InBase + "[" + std::to_string(I) + "]";
            Scan(InTopic, B + ".path", R.mPath);
            Scan(InTopic, B + ".note", R.mNote);
        }
    };
    for (const FTopicBundle &B : InBundles)
    {
        const FPlanMetadata &M = B.mMetadata;
        ScanDeps(B.mTopicKey, "dependencies", M.mDependencies);
        Scan(B.mTopicKey, "source_references", M.mSourceReferences);
        Scan(B.mTopicKey, "execution_strategy", M.mExecutionStrategy);
        Scan(B.mTopicKey, "locked_decisions", M.mLockedDecisions);
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &P = B.mPhases[PI];
            const std::string Base = "phases[" + std::to_string(PI) + "]";
            ScanDeps(B.mTopicKey, Base + ".dependencies",
                     P.mDesign.mDependencies);
            Scan(B.mTopicKey, Base + ".investigation",
                 P.mDesign.mInvestigation);
            Scan(B.mTopicKey, Base + ".readiness_gate",
                 P.mDesign.mReadinessGate);
            Scan(B.mTopicKey, Base + ".handoff", P.mDesign.mHandoff);
            Scan(B.mTopicKey, Base + ".code_entity_contract",
                 P.mDesign.mCodeEntityContract);
        }
    }
}

// 22. no_dev_absolute_path (ErrorMinor) — dev-machine absolute paths leak
// PII and break for other agents.
void EvalNoDevAbsolutePath(const std::vector<FTopicBundle> &InBundles,
                           std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex Pattern(
        R"(/Users/[a-z][\w.-]*/|/home/[a-z][\w.-]*/|[A-Z]:\\Users\\[\w.-]+)");
    for (const FTopicBundle &B : InBundles)
    {
        ScanTopicProse(B, Pattern, "no_dev_absolute_path",
                       EValidationSeverity::ErrorMinor,
                       "dev-machine absolute path: ", OutChecks);
        ScanPhaseDesignPrescriptiveProse(
            B, Pattern, "no_dev_absolute_path", EValidationSeverity::ErrorMinor,
            "dev-machine absolute path: ", OutChecks);
        ScanPhaseDesignEvidenceProse(B, Pattern, "no_dev_absolute_path",
                                     EValidationSeverity::ErrorMinor,
                                     "dev-machine absolute path: ", OutChecks,
                                     EPhaseEvidenceScope::AllPhases);
    }
}

// 23. no_hardcoded_endpoint (Warning) — localhost/LAN IPs steer agents to
// developer-local network state that won't exist in CI.
void EvalNoHardcodedEndpoint(const std::vector<FTopicBundle> &InBundles,
                             std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex Pattern(
        R"(\blocalhost:\d+\b|\b127\.0\.0\.1\b|\b192\.168\.\d+\.\d+\b|\b10\.\d+\.\d+\.\d+\b)");
    for (const FTopicBundle &B : InBundles)
    {
        // Topic-level only — changelogs may legitimately record historical
        // endpoint state; governance prose must not.
        ScanTopicProse(B, Pattern, "no_hardcoded_endpoint",
                       EValidationSeverity::Warning,
                       "hardcoded endpoint: ", OutChecks);
        // Per-phase: design material + evidence on all phases. Lifecycle is
        // historical and remains unscanned for this format check; a
        // hardcoded endpoint in lifecycle text is a historical record of a
        // dev-only endpoint that was used, not governance drift.
        ScanPhaseDesignPrescriptiveProse(B, Pattern, "no_hardcoded_endpoint",
                                         EValidationSeverity::Warning,
                                         "hardcoded endpoint: ", OutChecks);
        ScanPhaseDesignEvidenceProse(
            B, Pattern, "no_hardcoded_endpoint", EValidationSeverity::Warning,
            "hardcoded endpoint: ", OutChecks, EPhaseEvidenceScope::AllPhases);
    }
}

// validation_command_platform_consistency (Warning) — structural check:
// a FValidationCommand whose mCommand text contains Windows-specific
// backslash path segments (`\Windows-x64\`, `\Debug\`, `\Tools\`) must
// have mPlatform == Windows. A non-Windows (or Any) platform paired with
// a Windows-only command is a mis-tagged record.
//
// Replaces the deleted `platform_path_sep_free` workaround, which
// line-sniffed for `Windows |` prefixes in the old string form. The
// typed record makes this check structural: mPlatform is the source of
// truth, not a prose prefix.
void EvalValidationCommandPlatformConsistency(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex WindowsPath(
        R"([A-Za-z]+\\(?:Windows-x64|Debug|Release|Tools|FIE_Doc)\\)");

    const auto Scan = [&](const std::string &InTopic,
                          const std::string &InPathBase,
                          const std::vector<FValidationCommand> &InCommands)
    {
        for (size_t I = 0; I < InCommands.size(); ++I)
        {
            const FValidationCommand &C = InCommands[I];
            if (C.mPlatform == EPlatformScope::Windows)
                continue; // explicitly Windows — backslash paths OK
            std::smatch Match;
            if (std::regex_search(C.mCommand, Match, WindowsPath))
            {
                Fail(OutChecks, "validation_command_platform_consistency",
                     EValidationSeverity::Warning, InTopic,
                     InPathBase + "[" + std::to_string(I) + "]",
                     "Windows path `" + Match.str() +
                         "` in non-Windows command; set platform=windows");
            }
        }
    };

    for (const FTopicBundle &B : InBundles)
    {
        Scan(B.mTopicKey, "validation_commands",
             B.mMetadata.mValidationCommands);
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            Scan(B.mTopicKey,
                 "phases[" + std::to_string(PI) + "].validation_commands",
                 B.mPhases[PI].mDesign.mValidationCommands);
        }
    }
}

// 25. no_smart_quotes (Warning) — Unicode curly quotes/dashes break shell
// commands copy-pasted from bundle content.
void EvalNoSmartQuotes(const std::vector<FTopicBundle> &InBundles,
                       std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex Pattern("\xE2\x80\x98|\xE2\x80\x99|\xE2\x80\x9C|"
                                    "\xE2\x80\x9D|\xE2\x80\x93|\xE2\x80\x94");
    for (const FTopicBundle &B : InBundles)
    {
        ScanTopicProse(B, Pattern, "no_smart_quotes",
                       EValidationSeverity::Warning,
                       "unicode smart char: ", OutChecks);
        ScanPhaseDesignPrescriptiveProse(B, Pattern, "no_smart_quotes",
                                         EValidationSeverity::Warning,
                                         "unicode smart char: ", OutChecks);
        ScanPhaseDesignEvidenceProse(
            B, Pattern, "no_smart_quotes", EValidationSeverity::Warning,
            "unicode smart char: ", OutChecks, EPhaseEvidenceScope::AllPhases);
    }
}

// 26. no_html_in_prose (Warning) — HTML tags break markdown rendering in
// watch mode.
void EvalNoHtmlInProse(const std::vector<FTopicBundle> &InBundles,
                       std::vector<ValidateCheck> &OutChecks)
{
    static const std::regex Pattern(R"(<(?:br|div|span|p|h[1-6])\b)",
                                    std::regex_constants::icase);
    for (const FTopicBundle &B : InBundles)
    {
        ScanTopicProse(B, Pattern, "no_html_in_prose",
                       EValidationSeverity::Warning,
                       "HTML tag in prose: ", OutChecks);
        ScanPhaseDesignPrescriptiveProse(B, Pattern, "no_html_in_prose",
                                         EValidationSeverity::Warning,
                                         "HTML tag in prose: ", OutChecks);
        ScanPhaseDesignEvidenceProse(
            B, Pattern, "no_html_in_prose", EValidationSeverity::Warning,
            "HTML tag in prose: ", OutChecks, EPhaseEvidenceScope::AllPhases);
    }
}

// 27. no_empty_placeholder_literal (Warning) — literal "None"/"N/A"/"TBD"/"-"
// /status-word placeholders should be empty string.
//
// The status-word branch catches migration/column-shift defects where a
// table's Status column leaked into a prose field — producing `scope =
// "Completed"` or `done = "`completed`"`. These values are grammatically
// short placeholders, not real content, and silently satisfy presence
// checks elsewhere in the validator (e.g. `no_hollow_completed_phase`
// which only fires on empty fields). Catching them here forces callers
// to populate real prose.
//
// The scope of scanned fields mirrors `FPhaseLifecycle.mDone/mRemaining/
// mBlockers` plus the phase-level `scope`/`output` and the most common
// design-material prose (`investigation`). Other design fields are
// scanned by `no_unresolved_marker` and `no_smart_quotes` if/when they
// contain prose drift; a literal status word inside e.g. `code_snippets`
// would be meaningful content, not corruption.
void EvalNoEmptyPlaceholderLiteral(const std::vector<FTopicBundle> &InBundles,
                                   std::vector<ValidateCheck> &OutChecks)
{
    // The classic branch — the original None/N/A/TBD/- pattern, anchored
    // so it only fires on whole-field matches (not e.g. "ISO-like-TBD
    // banner").
    static const std::regex ClassicPattern(
        R"(^\s*(?:None|N/A|n/a|none|TBD|tbd|-)\s*$)");
    // The status-word branch — whole-field matches of bare status literals
    // with optional surrounding quotes/backticks, any case. Matches values
    // like `Completed`, `` `completed` ``, `"In progress"`, `not_started`,
    // `WIP`, etc. Multi-word tokens allow space or underscore between words
    // ("In progress" == "in_progress").
    static const std::regex StatusWordPattern(
        R"(^\s*[`"']*(?:complete[d]?|in[\s_]?progress|not[\s_]?started|blocked|pending|wip|done)[`"']*\s*$)",
        std::regex::icase);
    const auto Scan = [&](const std::string &InTopic, const std::string &InPath,
                          const std::string &InVal)
    {
        ScanProseField(InTopic, InPath, InVal, ClassicPattern,
                       "no_empty_placeholder_literal",
                       EValidationSeverity::Warning,
                       "placeholder literal: ", OutChecks);
        ScanProseField(InTopic, InPath, InVal, StatusWordPattern,
                       "no_empty_placeholder_literal",
                       EValidationSeverity::Warning,
                       "status-word placeholder: ", OutChecks);
    };
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &P = B.mPhases[PI];
            const std::string Base = "phases[" + std::to_string(PI) + "]";
            // Lifecycle prose (original coverage).
            Scan(B.mTopicKey, Base + ".blockers", P.mLifecycle.mBlockers);
            Scan(B.mTopicKey, Base + ".remaining", P.mLifecycle.mRemaining);
            Scan(B.mTopicKey, Base + ".done", P.mLifecycle.mDone);
            // Phase-level prose — catches the known column-shift defect
            // where V3 "Status" leaked into V4 `scope` / `output`.
            Scan(B.mTopicKey, Base + ".scope", P.mScope);
            Scan(B.mTopicKey, Base + ".output", P.mOutput);
            // Design-material prose — catches "Delivered:... Scope:
            // Completed" style leaks where a status line landed in
            // investigation.
            Scan(B.mTopicKey, Base + ".investigation",
                 P.mDesign.mInvestigation);
            // dependencies is a typed vector — an empty vector means "no
            // dependencies" (structural), not a placeholder literal.
            // Degenerate typed entries are caught by
            // `no_degenerate_dependency_entry`.
        }
    }
}

// 27a. topic_fields_not_identical (Warning) — semantically-distinct
// topic-level prose fields should not be byte-identical to each other.
// Byte-identical values signal a migration bug (one field copied verbatim
// into another) rather than intentional overlap: governance writers
// naturally paraphrase when they repeat a point.
//
// Pairs checked:
//   summary vs goals         — summary is the "why", goals are "what"
//   summary vs problem_statement
//   goals vs non_goals       — non_goals is inversion-by-definition
//
// Other fields (risks, acceptance_criteria, baseline_audit) are typed
// tables or long-form lists; equality there is extraordinarily unlikely
// and would already surface via other structural checks if it happened.
void EvalTopicFieldsNotIdentical(const std::vector<FTopicBundle> &InBundles,
                                 std::vector<ValidateCheck> &OutChecks)
{
    const auto CheckPair = [&](const FTopicBundle &B, const std::string &LName,
                               const std::string &LVal,
                               const std::string &RName,
                               const std::string &RVal)
    {
        // Both must be non-empty; two empty strings are trivially equal and
        // caught by required_fields if required.
        if (LVal.empty() || RVal.empty())
            return;
        if (LVal != RVal)
            return;
        // Short values can legitimately overlap (e.g. one-word titles).
        // Require at least 40 characters of content before flagging — this
        // is long enough to catch migration copy-paste of summary prose
        // into goals, short enough to not gate single-sentence topics.
        if (LVal.size() < 40)
            return;
        Fail(OutChecks, "topic_fields_not_identical",
             EValidationSeverity::Warning, B.mTopicKey, LName + "==" + RName,
             "topic fields are byte-identical (" + std::to_string(LVal.size()) +
                 " chars): " + LName + " and " + RName);
    };
    for (const FTopicBundle &B : InBundles)
    {
        const FPlanMetadata &M = B.mMetadata;
        CheckPair(B, "summary", M.mSummary, "goals", M.mGoals);
        CheckPair(B, "summary", M.mSummary, "problem_statement",
                  M.mProblemStatement);
        CheckPair(B, "goals", M.mGoals, "non_goals", M.mNonGoals);
    }
}

// 27b. no_degenerate_dependency_entry (Warning) — every typed dependency
// row must have real content. A row with empty kind (parsed as default
// Bundle) AND empty topic AND empty path is a shell that migrated
// free-prose into the `note` field without ever populating the structured
// reference fields. Downstream queries (`topic_ref_integrity`,
// `path_resolves`) silently skip such rows because the structure says
// "nothing to check", which hides the migration defect instead of
// surfacing it.
//
// Rule: kind is always valid (enum-typed), so we check whether the row
// carries any resolution-bearing value — a topic key (for Bundle/Phase),
// a path (for Governance/External), or — at minimum — a non-empty note
// that describes what the row is for. A row with topic="" + path="" +
// note="" is unambiguously empty and fails.
//
// Stricter variant: for Bundle/Phase kinds, topic is the primary key;
// missing topic is a structural break.
void EvalNoDegenerateDependencyEntry(const std::vector<FTopicBundle> &InBundles,
                                     std::vector<ValidateCheck> &OutChecks)
{
    // Severity promoted from Warning to ErrorMinor in v0.73.1 — the
    // check is at zero residuals across the mm-factory corpus post-
    // repair, and the structural invariant (every typed dependency
    // carries enough content to resolve) is cheap for producers to
    // satisfy. Regressions should block CI, not pass silently as
    // warnings.
    const auto CheckRef = [&](const std::string &InTopicKey,
                              const std::string &InPath,
                              const FBundleReference &InRef)
    {
        const bool bHasTopic = !InRef.mTopic.empty();
        const bool bHasPath = !InRef.mPath.empty();
        const bool bHasNote = !InRef.mNote.empty();
        if (!bHasTopic && !bHasPath && !bHasNote)
        {
            Fail(OutChecks, "no_degenerate_dependency_entry",
                 EValidationSeverity::ErrorMinor, InTopicKey, InPath,
                 "dependency row has empty topic, path, and note");
            return;
        }
        // Bundle/Phase dependencies must resolve to a topic. A note-only
        // Bundle-kind row is the classic migration shell: the free-prose
        // "Upstream dependency" column was wrapped in an empty-kind
        // envelope.
        if ((InRef.mKind == EDependencyKind::Bundle ||
             InRef.mKind == EDependencyKind::Phase) &&
            !bHasTopic)
        {
            Fail(OutChecks, "no_degenerate_dependency_entry",
                 EValidationSeverity::ErrorMinor, InTopicKey, InPath,
                 "dependency kind=" + std::string(ToString(InRef.mKind)) +
                     " requires a topic key");
        }
        // Governance/External dependencies must point at a path.
        if ((InRef.mKind == EDependencyKind::Governance ||
             InRef.mKind == EDependencyKind::External) &&
            !bHasPath)
        {
            Fail(OutChecks, "no_degenerate_dependency_entry",
                 EValidationSeverity::ErrorMinor, InTopicKey, InPath,
                 "dependency kind=" + std::string(ToString(InRef.mKind)) +
                     " requires a path");
        }
    };
    for (const FTopicBundle &B : InBundles)
    {
        const FPlanMetadata &M = B.mMetadata;
        for (size_t I = 0; I < M.mDependencies.size(); ++I)
        {
            CheckRef(B.mTopicKey, "dependencies[" + std::to_string(I) + "]",
                     M.mDependencies[I]);
        }
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const auto &Deps = B.mPhases[PI].mDesign.mDependencies;
            for (size_t DI = 0; DI < Deps.size(); ++DI)
            {
                CheckRef(B.mTopicKey,
                         "phases[" + std::to_string(PI) + "].dependencies[" +
                             std::to_string(DI) + "]",
                         Deps[DI]);
            }
        }
    }
}

// 28. no_unresolved_marker (Warning) — TODO/FIXME/TBD/unresolved markers
// in prescriptive governance prose signal design drift (the plan itself
// still has open questions). Completed-phase evidence and lifecycle fields
// are also checked: if a phase claims done but its evidence log still
// contains TODO, the historical record is self-contradictory.
//
// Scope structurally split:
//   topic prose                    — always scanned (prescriptive)
//   phase design prescriptive      — always scanned (prescriptive)
//   phase design evidence fields   — only when phase is completed
//   phase lifecycle prose          — only when phase is completed
//
// No status-based skip workaround — each scope scans the fields that
// semantically belong to it, not "scan everything and filter by status".
void EvalNoUnresolvedMarker(const std::vector<FTopicBundle> &InBundles,
                            std::vector<ValidateCheck> &OutChecks)
{
    // Match only bare TODO-style markers, not prose like "todo list".
    static const std::regex Pattern(
        R"(\bTODO\b|\bFIXME\b|\bXXX\b|\bHACK\b|\?\?\?)");
    for (const FTopicBundle &B : InBundles)
    {
        ScanTopicProse(B, Pattern, "no_unresolved_marker",
                       EValidationSeverity::Warning,
                       "unresolved marker: ", OutChecks);
        ScanPhaseDesignPrescriptiveProse(B, Pattern, "no_unresolved_marker",
                                         EValidationSeverity::Warning,
                                         "unresolved marker: ", OutChecks);
        ScanPhaseDesignEvidenceProse(
            B, Pattern, "no_unresolved_marker", EValidationSeverity::Warning,
            "unresolved marker in completed phase evidence: ", OutChecks,
            EPhaseEvidenceScope::CompletedOnly);
        ScanPhaseLifecycleProse(
            B, Pattern, "no_unresolved_marker", EValidationSeverity::Warning,
            "unresolved marker in completed phase: ", OutChecks,
            EPhaseEvidenceScope::CompletedOnly);
    }
}

// 29. topic_ref_integrity (ErrorMinor) — `<X>.Plan.json` prose references
// must resolve to a real topic key in the loaded bundle set.
void EvalTopicRefIntegrity(const std::vector<FTopicBundle> &InBundles,
                           const std::vector<FTopicBundle> &InReferenceBundles,
                           std::vector<ValidateCheck> &OutChecks)
{
    std::set<std::string> KnownKeys;
    for (const FTopicBundle &B : InReferenceBundles)
        KnownKeys.insert(B.mTopicKey);

    const auto Walk = [&](const std::string &InTopic, const std::string &InPath,
                          const std::string &InContent)
    {
        if (InContent.empty())
            return;
        std::set<std::string> References;
        FindTopicPlanJsonReferences(InContent, References);
        for (const std::string &Ref : References)
        {
            if (Ref == InTopic)
                continue; // self-reference ok
            if (KnownKeys.count(Ref) > 0)
                continue;
            Fail(OutChecks, "topic_ref_integrity",
                 EValidationSeverity::ErrorMinor, InTopic, InPath,
                 "unknown topic reference: '" + Ref + ".Plan.json'");
        }
    };

    // Structural check: typed FBundleReference.mTopic must resolve.
    // Scans mNote/mPath for stray references using the legacy regex walk.
    const auto CheckDeps = [&](const FTopicBundle &InB,
                               const std::string &InBase,
                               const std::vector<FBundleReference> &InDeps)
    {
        for (size_t I = 0; I < InDeps.size(); ++I)
        {
            const FBundleReference &R = InDeps[I];
            const std::string B = InBase + "[" + std::to_string(I) + "]";
            if ((R.mKind == EDependencyKind::Bundle ||
                 R.mKind == EDependencyKind::Phase) &&
                !R.mTopic.empty() && R.mTopic != InB.mTopicKey &&
                KnownKeys.count(R.mTopic) == 0)
            {
                Fail(OutChecks, "topic_ref_integrity",
                     EValidationSeverity::ErrorMinor, InB.mTopicKey,
                     B + ".topic",
                     "unknown topic reference: '" + R.mTopic + "'");
            }
            Walk(InB.mTopicKey, B + ".path", R.mPath);
            Walk(InB.mTopicKey, B + ".note", R.mNote);
        }
    };
    for (const FTopicBundle &B : InBundles)
    {
        const FPlanMetadata &M = B.mMetadata;
        Walk(B.mTopicKey, "summary", M.mSummary);
        CheckDeps(B, "dependencies", M.mDependencies);
        Walk(B.mTopicKey, "source_references", M.mSourceReferences);
        Walk(B.mTopicKey, "execution_strategy", M.mExecutionStrategy);
        Walk(B.mTopicKey, "locked_decisions", M.mLockedDecisions);
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &P = B.mPhases[PI];
            const std::string Base = "phases[" + std::to_string(PI) + "]";
            CheckDeps(B, Base + ".dependencies", P.mDesign.mDependencies);
            Walk(B.mTopicKey, Base + ".scope", P.mScope);
            Walk(B.mTopicKey, Base + ".output", P.mOutput);
        }
    }
}

void EvalTopicRefIntegrity(const std::vector<FTopicBundle> &InBundles,
                           std::vector<ValidateCheck> &OutChecks)
{
    EvalTopicRefIntegrity(InBundles, InBundles, OutChecks);
}

// 31. no_duplicate_changelog (Warning) — same (phase, change text) pair
// recorded more than once distorts the audit timeline.
void EvalNoDuplicateChangelog(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        std::map<std::pair<int, std::string>, size_t> FirstSeen;
        for (size_t I = 0; I < B.mChangeLogs.size(); ++I)
        {
            const FChangeLogEntry &C = B.mChangeLogs[I];
            if (C.mChange.empty())
                continue;
            const auto Key = std::make_pair(C.mPhase, C.mChange);
            auto Found = FirstSeen.find(Key);
            if (Found == FirstSeen.end())
            {
                FirstSeen.emplace(Key, I);
                continue;
            }
            // v0.97.0 no-truncation contract: embed the full change
            // text so callers can reconstruct the duplicate pair.
            Fail(OutChecks, "no_duplicate_changelog",
                 EValidationSeverity::Warning, B.mTopicKey,
                 "changelogs[" + std::to_string(I) + "].change",
                 "duplicate of changelogs[" + std::to_string(Found->second) +
                     "]: '" + C.mChange + "'");
        }
    }
}

// no_duplicate_phase_field (Warning) — two or more phases in the same
// bundle share byte-identical non-empty content in a prescriptive field
// that should vary per phase (`scope`, `output`, `handoff`,
// `readiness_gate`, `investigation`, `code_entity_contract`,
// `code_snippets`, `best_practices`).
//
// Catches migration-stamp artifacts where a template string was copied
// unchanged into many phases (the signature pattern of a broken V3→V4
// extractor). Short stubs (<20 chars) are ignored because they tend to
// be legitimately-terse labels like `N/A`.
void EvalNoDuplicatePhaseField(const std::vector<FTopicBundle> &InBundles,
                               std::vector<ValidateCheck> &OutChecks)
{
    static const std::array<
        std::pair<const char *, std::string (*)(const FPhaseRecord &)>, 10>
        Fields = {
            {{"scope", [](const FPhaseRecord &P) { return P.mScope; }},
             {"output", [](const FPhaseRecord &P) { return P.mOutput; }},
             {"done", [](const FPhaseRecord &P) { return P.mLifecycle.mDone; }},
             {"remaining",
              [](const FPhaseRecord &P) { return P.mLifecycle.mRemaining; }},
             {"handoff",
              [](const FPhaseRecord &P) { return P.mDesign.mHandoff; }},
             {"readiness_gate",
              [](const FPhaseRecord &P) { return P.mDesign.mReadinessGate; }},
             {"investigation",
              [](const FPhaseRecord &P) { return P.mDesign.mInvestigation; }},
             {"code_entity_contract", [](const FPhaseRecord &P)
              { return P.mDesign.mCodeEntityContract; }},
             {"code_snippets",
              [](const FPhaseRecord &P) { return P.mDesign.mCodeSnippets; }},
             {"best_practices",
              [](const FPhaseRecord &P) { return P.mDesign.mBestPractices; }}}};

    for (const FTopicBundle &B : InBundles)
    {
        for (const auto &Field : Fields)
        {
            std::map<std::string, size_t> FirstSeen;
            for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
            {
                const std::string Value = Field.second(B.mPhases[PI]);
                if (Value.size() < 20)
                    continue;
                auto Found = FirstSeen.find(Value);
                if (Found == FirstSeen.end())
                {
                    FirstSeen.emplace(Value, PI);
                    continue;
                }
                // v0.97.0 no-truncation contract: embed the full
                // identical value so callers see the exact duplicate.
                Fail(OutChecks, "no_duplicate_phase_field",
                     EValidationSeverity::Warning, B.mTopicKey,
                     "phases[" + std::to_string(PI) + "]." + Field.first,
                     "identical to phases[" + std::to_string(Found->second) +
                         "]." + Field.first + ": '" + Value + "'");
            }
        }
    }
}

// no_hollow_completed_phase (Warning) — a phase is marked `completed` but
// has no execution evidence: no jobs, no testing records, no file manifest
// entries, AND insufficient design prose
// (`ComputePhaseDesignChars < kPhaseHollowChars`, i.e. < 3000 chars ≈ 5-7
// design fields populated with 1-3 sentences each). This is the signature
// of a migration script that copied the `status=completed` marker from a
// legacy tracker without harvesting the underlying execution content, or
// of a post-hoc phase entry that was marked done without ever being
// filled in.
//
// Structural — looks only at array sizes and a unified char-count measure
// (same `ComputePhaseDesignChars` used by `legacy-gap` and the watch TUI
// `Design` column, so all three tools agree on what "hollow" means).
//
// v0.82.0 tightened this check from the prior v0.79.0 / v0.80.0 rule that
// only inspected two specific design fields for emptiness
// (`code_snippets` and `investigation`). The old rule let trivially-
// filled phases pass — e.g. a completed phase with "TBD" in one of those
// fields and nothing else. The chars-threshold form catches those
// honestly: a phase must carry ≥ kPhaseHollowChars of authored prose
// across scope/output/design to escape the check on the prose side, OR
// must have any non-empty jobs/testing/manifest array on the execution
// side. v0.83.0 recalibrated kPhaseHollowChars from 4000 to 3000
// (user-ratified, V4 schema-semantic derivation — see
// UniPlanTopicTypes.h).
void EvalNoHollowCompletedPhase(const std::vector<FTopicBundle> &InBundles,
                                std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &Phase = B.mPhases[PI];
            if (Phase.mLifecycle.mStatus != EExecutionStatus::Completed)
                continue;
            if (!Phase.mJobs.empty())
                continue;
            if (!Phase.mTesting.empty())
                continue;
            if (!Phase.mFileManifest.empty())
                continue;
            if (ComputePhaseDesignChars(Phase) >= kPhaseHollowChars)
                continue;
            Fail(OutChecks, "no_hollow_completed_phase",
                 EValidationSeverity::Warning, B.mTopicKey,
                 "phases[" + std::to_string(PI) + "]",
                 "completed but has no jobs, no testing records, no file "
                 "manifest, and design prose totals < " +
                     std::to_string(kPhaseHollowChars) +
                     " chars (hollow threshold)");
        }
    }
}

// ---------------------------------------------------------------------------
// no_duplicate_lane_scope — catch literal clone-and-never-cleaned-up lanes.
// Flags lanes within the same phase whose `scope` prose normalizes to the
// same string (lowercase + whitespace-collapsed + edge-punctuation stripped).
// Conservative: exact-normalized match only. Fires on the later lane; detail
// points back to the original. Empty scopes are left to EvalLaneRequiredFields.
// Added v0.84.0 after ISeeThroughYou phase 6/9/15 audit surfaced the
// clone-and-forget pattern during a manual investigation.
// ---------------------------------------------------------------------------

static std::string NormalizeLaneScope(const std::string &InScope)
{
    std::string Out;
    Out.reserve(InScope.size());
    bool bInWhitespace = false;
    for (char C : InScope)
    {
        const unsigned char U = static_cast<unsigned char>(C);
        if (std::isspace(U))
        {
            if (!bInWhitespace && !Out.empty())
                Out.push_back(' ');
            bInWhitespace = true;
        }
        else
        {
            Out.push_back(static_cast<char>(std::tolower(U)));
            bInWhitespace = false;
        }
    }
    while (!Out.empty() && Out.back() == ' ')
        Out.pop_back();
    const auto IsEdgePunct = [](char C)
    {
        return C == '.' || C == '!' || C == '?' || C == ',' || C == ';' ||
               C == ':' || C == '-';
    };
    while (!Out.empty() && IsEdgePunct(Out.front()))
        Out.erase(Out.begin());
    while (!Out.empty() && IsEdgePunct(Out.back()))
        Out.pop_back();
    return Out;
}

void EvalNoDuplicateLaneScope(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &Phase = B.mPhases[PI];
            if (Phase.mLanes.size() < 2)
                continue;
            std::vector<std::string> Normalized;
            Normalized.reserve(Phase.mLanes.size());
            for (const FLaneRecord &L : Phase.mLanes)
                Normalized.push_back(NormalizeLaneScope(L.mScope));
            for (size_t J = 1; J < Normalized.size(); ++J)
            {
                if (Normalized[J].empty())
                    continue;
                for (size_t I = 0; I < J; ++I)
                {
                    if (Normalized[I].empty())
                        continue;
                    if (Normalized[I] != Normalized[J])
                        continue;
                    Fail(OutChecks, "no_duplicate_lane_scope",
                         EValidationSeverity::Warning, B.mTopicKey,
                         "phases[" + std::to_string(PI) + "].lanes[" +
                             std::to_string(J) + "]",
                         "duplicate scope (normalized) of phases[" +
                             std::to_string(PI) + "].lanes[" +
                             std::to_string(I) + "]");
                    break;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// file_manifest_required_for_code_phases — close the authoring gap that
// leaves ~36% of plans without manifest evidence. Predicate: a phase is
// "code-bearing" when it declared itself so via populated design fields
// (mCodeEntityContract OR mCodeSnippets non-empty). For each such phase
// with an empty file_manifest AND no explicit opt-out (mbNoFileManifest),
// emit a Warning. v0.86.0 ships at Warning severity to surface ~9
// retrofit candidates without breaking CI; v0.87.0 promotes to
// ErrorMinor once `manifest suggest` has bought the migration window.
// ---------------------------------------------------------------------------

// IsCodeBearingPhase is provided by UniPlanPhaseKind.h as the single-
// source-of-truth predicate for every consumer (phase readiness,
// phase next, phase complete mutation-gate, and this validator). The
// earlier static-local copy was removed in v0.96.0 to eliminate drift
// risk; re-adding it here is a regression.

void EvalFileManifestRequiredForCodePhases(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &Phase = B.mPhases[PI];
            if (!IsCodeBearingPhase(Phase))
                continue;
            if (Phase.mLifecycle.mStatus == EExecutionStatus::Canceled)
                continue; // superseded phase; never executed, never will
            if (!Phase.mFileManifest.empty())
                continue;
            if (Phase.mbNoFileManifest)
                continue; // explicit opt-out; reason is enforced by parser
            // Severity v0.86.0: Warning (advisory). v0.87.0: promoted
            // to ErrorMinor — `manifest suggest` (v0.86.0) gives authors
            // the migration path; the 30-day advisory window is over.
            // Now gates --strict so CI can refuse drift PRs.
            Fail(OutChecks, "file_manifest_required_for_code_phases",
                 EValidationSeverity::ErrorMinor, B.mTopicKey,
                 "phases[" + std::to_string(PI) + "].file_manifest",
                 "code-bearing phase (code_entity_contract or code_snippets "
                 "populated) has empty file_manifest; either run `uni-plan "
                 "manifest suggest --topic " +
                     B.mTopicKey + " --phase " + std::to_string(PI) +
                     " --apply` to backfill from git history, or set "
                     "no_file_manifest=true with a documented reason via "
                     "`phase set --no-file-manifest=true "
                     "--no-file-manifest-reason \"...\"`");
        }
    }
}

// ---------------------------------------------------------------------------
// stale_mislabeled_modify (v0.87.0) — close the PerformanceCulling-class
// blind spot. For each manifest entry with action=modify, look up the
// file's first-commit timestamp via git history; if it post-dates the
// phase's started_at, the action should have been `create` instead.
//
// One git invocation per validate call (cached in a per-call map),
// scanning the entire repo history. Skipped silently when:
//   * Repo root is empty (caller didn't pass it — e.g. watch TUI).
//   * Git is unavailable (popen returns non-zero).
//   * Phase has no started_at (no comparison possible).
//   * Manifest entry's file is missing from history (probably outside
//     the repo or removed before any commit recorded it).
// Severity: Warning — fuzzy signal that can false-fire on history
// rewrites, cherry-picks, or files that were renamed across phases.
// ---------------------------------------------------------------------------

// Build map: file_path -> ISO 8601 timestamp of the FIRST commit that
// added the file. Walks `git log --reverse --name-only --pretty=format:%aI`
// once and records the earliest seen timestamp per path. Excluded paths:
// the bundle files themselves (they record manifest changes, not the
// underlying code creation).
static bool BuildFirstCommitMap(const fs::path &InRepoRoot,
                                std::map<std::string, std::string> &OutMap)
{
    std::ostringstream Cmd;
    Cmd << "git -C \"" << InRepoRoot.string() << "\" log --reverse "
        << "--name-only --pretty=format:%aI " << ShellStderrToNull();
    FILE *rpPipe = OpenReadPipe(Cmd.str());
    if (rpPipe == nullptr)
        return false;
    char Buffer[8192];
    std::string CurrentTimestamp;
    static const std::regex IsoLine(
        R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}.*$)");
    while (std::fgets(Buffer, sizeof(Buffer), rpPipe) != nullptr)
    {
        std::string Line(Buffer);
        while (!Line.empty() && (Line.back() == '\n' || Line.back() == '\r'))
            Line.pop_back();
        if (Line.empty())
            continue;
        if (std::regex_match(Line, IsoLine))
        {
            CurrentTimestamp = Line;
            continue;
        }
        // First-seen timestamp wins because --reverse processes oldest
        // first. Skip bundle files: their git history reflects manifest
        // mutations, not the underlying code creation we're auditing.
        if (Line.find(".Plan.json") != std::string::npos)
            continue;
        if (OutMap.find(Line) == OutMap.end())
            OutMap[Line] = CurrentTimestamp;
    }
    const int ExitCode = CloseReadPipe(rpPipe);
    return ExitCode == 0;
}

// Cheap ISO 8601 lexicographic comparison. Both sides MUST be valid
// ISO 8601 (YYYY-MM-DDTHH:MM:SS...) — same charset and order, so string
// compare matches chronological order. Caller validates inputs (the
// phase fixture round-trip enforces ISO format on started_at; git emits
// canonical ISO for %aI).
static bool IsTimestampStrictlyAfter(const std::string &InCandidate,
                                     const std::string &InReference)
{
    return InCandidate > InReference;
}

void EvalStaleMislabeledModify(const std::vector<FTopicBundle> &InBundles,
                               std::vector<ValidateCheck> &OutChecks,
                               const fs::path &InRepoRoot)
{
    // Quick exit if no bundle has any modify entries — avoids the git
    // call entirely on corpora without manifest discipline yet.
    bool bAnyModify = false;
    for (const FTopicBundle &B : InBundles)
    {
        for (const FPhaseRecord &P : B.mPhases)
        {
            if (P.mLifecycle.mStartedAt.empty())
                continue;
            for (const FFileManifestItem &FM : P.mFileManifest)
            {
                if (FM.mAction == EFileAction::Modify)
                {
                    bAnyModify = true;
                    break;
                }
            }
            if (bAnyModify)
                break;
        }
        if (bAnyModify)
            break;
    }
    if (!bAnyModify)
        return;

    std::map<std::string, std::string> FirstCommit;
    if (!BuildFirstCommitMap(InRepoRoot, FirstCommit))
        return; // git unavailable; silently skip

    for (const FTopicBundle &B : InBundles)
    {
        for (size_t PI = 0; PI < B.mPhases.size(); ++PI)
        {
            const FPhaseRecord &Phase = B.mPhases[PI];
            const std::string &Started = Phase.mLifecycle.mStartedAt;
            if (Started.empty())
                continue;
            for (size_t MI = 0; MI < Phase.mFileManifest.size(); ++MI)
            {
                const FFileManifestItem &FM = Phase.mFileManifest[MI];
                if (FM.mAction != EFileAction::Modify)
                    continue;
                const auto It = FirstCommit.find(FM.mFilePath);
                if (It == FirstCommit.end())
                    continue; // file has no recorded git history
                const std::string &Born = It->second;
                if (!IsTimestampStrictlyAfter(Born, Started))
                    continue; // file pre-existed the phase — modify is OK
                Fail(OutChecks, "stale_mislabeled_modify",
                     EValidationSeverity::Warning, B.mTopicKey,
                     "phases[" + std::to_string(PI) + "].file_manifest[" +
                         std::to_string(MI) + "]",
                     "action=modify but file was first committed at " + Born +
                         " (after phase started_at=" + Started +
                         "); the action should have been `create`. Update "
                         "via `manifest set --topic <T> --phase " +
                         std::to_string(PI) + " --index " + std::to_string(MI) +
                         " --action create`.");
            }
        }
    }
}

// ===========================================================================
// v0.89.0 typed-array evaluators
// ===========================================================================

// ---------------------------------------------------------------------------
// scope_and_non_scope_populated — closes the VoGame watch blind spot.
// Topics in active governance states (`in_progress`, `completed`,
// `blocked`) must have both `goals` and `non_goals` populated — the
// watch TUI PLAN DETAIL panel reads these typed fields to render Scope
// and Non-Scope rows, and renders `(none)` when either is empty.
// Severity is Warning by default; promoted to ErrorMinor under --strict.
// Skipped for `not_started` (still being authored) and `canceled`.
// ---------------------------------------------------------------------------

void EvalScopeAndNonScopePopulated(const std::vector<FTopicBundle> &InBundles,
                                   std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        if (B.mStatus == ETopicStatus::NotStarted ||
            B.mStatus == ETopicStatus::Canceled)
            continue;
        const bool bGoalsEmpty = B.mMetadata.mGoals.empty();
        const bool bNonGoalsEmpty = B.mMetadata.mNonGoals.empty();
        if (!bGoalsEmpty && !bNonGoalsEmpty)
            continue;
        std::string Path = "plan";
        std::string Detail;
        if (bGoalsEmpty && bNonGoalsEmpty)
            Detail = "goals and non_goals are both empty";
        else if (bGoalsEmpty)
            Detail = "goals is empty";
        else
            Detail = "non_goals is empty";
        Detail += "; populate via `uni-plan topic set --topic " + B.mTopicKey +
                  " --goals <text> --non-goals <text>` so watch TUI PLAN "
                  "DETAIL can render Scope / Non-Scope panels";
        Fail(OutChecks, "scope_and_non_scope_populated",
             EValidationSeverity::Warning, B.mTopicKey, Path, Detail);
    }
}

// ---------------------------------------------------------------------------
// Per-entry well-formedness checks for the three v0.89.0 typed arrays.
// Each fires ErrorMinor so they surface without --strict but don't block
// load — broken entries still load (with defaults) but the validator
// immediately points at the defect.
// ---------------------------------------------------------------------------

void EvalRiskEntryWellformed(const std::vector<FTopicBundle> &InBundles,
                             std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mMetadata.mRisks.size(); ++I)
        {
            const FRiskEntry &R = B.mMetadata.mRisks[I];
            if (R.mStatement.empty())
            {
                Fail(OutChecks, "risk_entry_wellformed",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     "risks[" + std::to_string(I) + "].statement",
                     "statement is empty");
            }
        }
    }
}

void EvalRiskSeverityPopulatedForHighImpact(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mMetadata.mRisks.size(); ++I)
        {
            const FRiskEntry &R = B.mMetadata.mRisks[I];
            const bool bHighImpact = (R.mSeverity == ERiskSeverity::High ||
                                      R.mSeverity == ERiskSeverity::Critical);
            if (!bHighImpact)
                continue;
            if (R.mStatus == ERiskStatus::Accepted ||
                R.mStatus == ERiskStatus::Closed)
                continue;
            if (!R.mMitigation.empty())
                continue;
            Fail(OutChecks, "risk_severity_populated_for_high_impact",
                 EValidationSeverity::Warning, B.mTopicKey,
                 "risks[" + std::to_string(I) + "]",
                 std::string("severity=") + ToString(R.mSeverity) +
                     " risk missing mitigation (set --mitigation or move "
                     "status to accepted/closed)");
        }
    }
}

void EvalRiskIdUnique(const std::vector<FTopicBundle> &InBundles,
                      std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        std::map<std::string, size_t> Seen;
        for (size_t I = 0; I < B.mMetadata.mRisks.size(); ++I)
        {
            const FRiskEntry &R = B.mMetadata.mRisks[I];
            if (R.mId.empty())
                continue;
            const auto It = Seen.find(R.mId);
            if (It != Seen.end())
            {
                Fail(OutChecks, "risk_id_unique",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     "risks[" + std::to_string(I) + "].id",
                     "id '" + R.mId + "' duplicates risks[" +
                         std::to_string(It->second) + "].id");
                continue;
            }
            Seen[R.mId] = I;
        }
    }
}

void EvalNextActionWellformed(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mNextActions.size(); ++I)
        {
            const FNextActionEntry &A = B.mNextActions[I];
            if (A.mStatement.empty())
            {
                Fail(OutChecks, "next_action_wellformed",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     "next_actions[" + std::to_string(I) + "].statement",
                     "statement is empty");
            }
            if (A.mOrder <= 0)
            {
                Fail(OutChecks, "next_action_wellformed",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     "next_actions[" + std::to_string(I) + "].order",
                     "order must be >= 1 (got " + std::to_string(A.mOrder) +
                         ")");
            }
        }
    }
}

void EvalNextActionOrderUnique(const std::vector<FTopicBundle> &InBundles,
                               std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        std::map<int, size_t> Seen;
        for (size_t I = 0; I < B.mNextActions.size(); ++I)
        {
            const FNextActionEntry &A = B.mNextActions[I];
            if (A.mOrder <= 0)
                continue; // caught by next_action_wellformed
            const auto It = Seen.find(A.mOrder);
            if (It != Seen.end())
            {
                Fail(OutChecks, "next_action_order_unique",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     "next_actions[" + std::to_string(I) + "].order",
                     "order=" + std::to_string(A.mOrder) +
                         " duplicates next_actions[" +
                         std::to_string(It->second) + "].order");
                continue;
            }
            Seen[A.mOrder] = I;
        }
    }
}

void EvalNextActionHasEntries(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        if (B.mStatus != ETopicStatus::InProgress &&
            B.mStatus != ETopicStatus::Blocked)
            continue;
        if (!B.mNextActions.empty())
            continue;
        Fail(OutChecks, "next_action_has_entries", EValidationSeverity::Warning,
             B.mTopicKey, "next_actions",
             std::string("active topic (status=") + ToString(B.mStatus) +
                 ") has no next_actions — add at least one via "
                 "`uni-plan next-action add --topic " +
                 B.mTopicKey + " --statement <text>`");
    }
}

void EvalAcceptanceCriterionWellformed(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mMetadata.mAcceptanceCriteria.size(); ++I)
        {
            const FAcceptanceCriterionEntry &C =
                B.mMetadata.mAcceptanceCriteria[I];
            if (C.mStatement.empty())
            {
                Fail(OutChecks, "acceptance_criterion_wellformed",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     "acceptance_criteria[" + std::to_string(I) + "].statement",
                     "statement is empty");
            }
        }
    }
}

void EvalAcceptanceCriterionIdUnique(const std::vector<FTopicBundle> &InBundles,
                                     std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        std::map<std::string, size_t> Seen;
        for (size_t I = 0; I < B.mMetadata.mAcceptanceCriteria.size(); ++I)
        {
            const FAcceptanceCriterionEntry &C =
                B.mMetadata.mAcceptanceCriteria[I];
            if (C.mId.empty())
                continue;
            const auto It = Seen.find(C.mId);
            if (It != Seen.end())
            {
                Fail(OutChecks, "acceptance_criterion_id_unique",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     "acceptance_criteria[" + std::to_string(I) + "].id",
                     "id '" + C.mId + "' duplicates acceptance_criteria[" +
                         std::to_string(It->second) + "].id");
                continue;
            }
            Seen[C.mId] = I;
        }
    }
}

void EvalAcceptanceCriteriaHasEntries(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        if (B.mStatus != ETopicStatus::Completed)
            continue;
        if (!B.mMetadata.mAcceptanceCriteria.empty())
            continue;
        Fail(OutChecks, "acceptance_criteria_has_entries",
             EValidationSeverity::ErrorMinor, B.mTopicKey,
             "acceptance_criteria",
             "completed topic has no acceptance_criteria — a completed "
             "topic with zero criteria provides no audit trail for what "
             "was delivered; backfill via `uni-plan acceptance-criterion "
             "add --topic " +
                 B.mTopicKey + " --statement <text>`");
    }
}

void EvalCompletedTopicCriteriaAllMet(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        if (B.mStatus != ETopicStatus::Completed)
            continue;
        for (size_t I = 0; I < B.mMetadata.mAcceptanceCriteria.size(); ++I)
        {
            const FAcceptanceCriterionEntry &C =
                B.mMetadata.mAcceptanceCriteria[I];
            if (C.mStatus == ECriterionStatus::Met ||
                C.mStatus == ECriterionStatus::NotApplicable)
                continue;
            Fail(OutChecks, "completed_topic_criteria_all_met",
                 EValidationSeverity::Warning, B.mTopicKey,
                 "acceptance_criteria[" + std::to_string(I) + "].status",
                 std::string("completed topic has acceptance_criteria[") +
                     std::to_string(I) + "] with status=" +
                     ToString(C.mStatus) + " (expected met or not_applicable)");
        }
    }
}

// ---------------------------------------------------------------------------
// v0.98.0 typed-array evaluators — priority_groupings / runbooks /
// residual_risks. Well-formedness, uniqueness, phase-index-in-range, and
// closure-sha format checks. These fire only when the arrays contain
// entries; empty arrays are the valid default (authors opt in).
// ---------------------------------------------------------------------------

void EvalPriorityGroupingWellformed(const std::vector<FTopicBundle> &InBundles,
                                    std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mMetadata.mPriorityGroupings.size(); ++I)
        {
            const FPriorityGrouping &G = B.mMetadata.mPriorityGroupings[I];
            const std::string Base =
                "priority_groupings[" + std::to_string(I) + "]";
            if (G.mID.empty())
                Fail(OutChecks, "priority_grouping_wellformed",
                     EValidationSeverity::ErrorMinor, B.mTopicKey, Base + ".id",
                     "id is empty");
            if (G.mPhaseIndices.empty())
                Fail(OutChecks, "priority_grouping_wellformed",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     Base + ".phase_indices",
                     "phase_indices is empty (at least one phase required)");
            if (G.mRule.empty())
                Fail(OutChecks, "priority_grouping_wellformed",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     Base + ".rule", "rule is empty");
        }
    }
}

void EvalPriorityGroupingPhaseIndexValid(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        const int PhaseCount = static_cast<int>(B.mPhases.size());
        for (size_t I = 0; I < B.mMetadata.mPriorityGroupings.size(); ++I)
        {
            const FPriorityGrouping &G = B.mMetadata.mPriorityGroupings[I];
            for (size_t J = 0; J < G.mPhaseIndices.size(); ++J)
            {
                const int PI = G.mPhaseIndices[J];
                if (PI < 0 || PI >= PhaseCount)
                {
                    Fail(OutChecks, "priority_grouping_phase_index_valid",
                         EValidationSeverity::ErrorMinor, B.mTopicKey,
                         "priority_groupings[" + std::to_string(I) +
                             "].phase_indices[" + std::to_string(J) + "]",
                         "phase index " + std::to_string(PI) +
                             " out of range (topic has " +
                             std::to_string(PhaseCount) + " phases)");
                }
            }
        }
    }
}

void EvalPriorityGroupingIdUnique(const std::vector<FTopicBundle> &InBundles,
                                  std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        std::map<std::string, size_t> Seen;
        for (size_t I = 0; I < B.mMetadata.mPriorityGroupings.size(); ++I)
        {
            const FPriorityGrouping &G = B.mMetadata.mPriorityGroupings[I];
            if (G.mID.empty())
                continue; // caught by priority_grouping_wellformed
            const auto It = Seen.find(G.mID);
            if (It != Seen.end())
            {
                Fail(OutChecks, "priority_grouping_id_unique",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     "priority_groupings[" + std::to_string(I) + "].id",
                     "id '" + G.mID + "' duplicates priority_groupings[" +
                         std::to_string(It->second) + "].id");
                continue;
            }
            Seen[G.mID] = I;
        }
    }
}

void EvalRunbookWellformed(const std::vector<FTopicBundle> &InBundles,
                           std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mMetadata.mRunbooks.size(); ++I)
        {
            const FRunbookProcedure &R = B.mMetadata.mRunbooks[I];
            const std::string Base = "runbooks[" + std::to_string(I) + "]";
            if (R.mName.empty())
                Fail(OutChecks, "runbook_wellformed",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     Base + ".name", "name is empty");
            if (R.mTrigger.empty())
                Fail(OutChecks, "runbook_wellformed",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     Base + ".trigger", "trigger is empty");
            if (R.mCommands.empty())
                Fail(OutChecks, "runbook_wellformed",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     Base + ".commands",
                     "commands is empty (at least one command required)");
        }
    }
}

void EvalRunbookNameUnique(const std::vector<FTopicBundle> &InBundles,
                           std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        std::map<std::string, size_t> Seen;
        for (size_t I = 0; I < B.mMetadata.mRunbooks.size(); ++I)
        {
            const FRunbookProcedure &R = B.mMetadata.mRunbooks[I];
            if (R.mName.empty())
                continue; // caught by runbook_wellformed
            const auto It = Seen.find(R.mName);
            if (It != Seen.end())
            {
                Fail(OutChecks, "runbook_name_unique",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     "runbooks[" + std::to_string(I) + "].name",
                     "name '" + R.mName + "' duplicates runbooks[" +
                         std::to_string(It->second) + "].name");
                continue;
            }
            Seen[R.mName] = I;
        }
    }
}

void EvalResidualRiskWellformed(const std::vector<FTopicBundle> &InBundles,
                                std::vector<ValidateCheck> &OutChecks)
{
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mMetadata.mResidualRisks.size(); ++I)
        {
            const FResidualRiskEntry &R = B.mMetadata.mResidualRisks[I];
            const std::string Base =
                "residual_risks[" + std::to_string(I) + "]";
            if (R.mArea.empty())
                Fail(OutChecks, "residual_risk_wellformed",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     Base + ".area", "area is empty");
            if (R.mObservation.empty())
                Fail(OutChecks, "residual_risk_wellformed",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     Base + ".observation", "observation is empty");
            if (R.mWhyDeferred.empty())
                Fail(OutChecks, "residual_risk_wellformed",
                     EValidationSeverity::ErrorMinor, B.mTopicKey,
                     Base + ".why_deferred", "why_deferred is empty");
        }
    }
}

void EvalResidualRiskClosureShaFormat(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks)
{
    // Git SHA: 7-40 hex chars (short or full). Enforced only when the
    // field is non-empty — empty means "not yet closed" and is normal.
    const std::regex kShaRegex("^[0-9a-f]{7,40}$");
    for (const FTopicBundle &B : InBundles)
    {
        for (size_t I = 0; I < B.mMetadata.mResidualRisks.size(); ++I)
        {
            const FResidualRiskEntry &R = B.mMetadata.mResidualRisks[I];
            if (R.mClosureSha.empty())
                continue;
            if (!std::regex_match(R.mClosureSha, kShaRegex))
            {
                Fail(OutChecks, "residual_risk_closure_sha_format",
                     EValidationSeverity::Warning, B.mTopicKey,
                     "residual_risks[" + std::to_string(I) + "].closure_sha",
                     "closure_sha '" + R.mClosureSha +
                         "' is not a 7-40 char lowercase hex git SHA");
            }
        }
    }
}

} // namespace UniPlan
