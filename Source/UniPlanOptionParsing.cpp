#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace UniPlan
{

bool IsOptionToken(const std::string &InToken)
{
    return !InToken.empty() && InToken[0] == '-';
}

std::vector<std::string>
ConsumeCommonOptions(const std::vector<std::string> &InTokens,
                     BaseOptions &OutOptions,
                     const bool InAllowPositionalRoot = false)
{
    OutOptions.mRepoRoot = fs::current_path().string();
    std::vector<std::string> Remaining;
    bool HasExplicitRoot = false;

    for (size_t Index = 0; Index < InTokens.size(); ++Index)
    {
        const std::string &Token = InTokens[Index];
        if (Token == "--repo" || Token == "--repo-root" || Token == "--root")
        {
            if (Index + 1 >= InTokens.size())
            {
                throw UsageError("Missing value for --repo-root");
            }
            OutOptions.mRepoRoot = InTokens[Index + 1];
            HasExplicitRoot = true;
            Index += 1;
            continue;
        }
        if (Token == "--text")
        {
            throw UsageError(
                "`--text` was removed in doc 3.0.0. Use default JSON "
                "output or `--human`.");
        }
        if (Token == "--human")
        {
            OutOptions.mbHuman = true;
            OutOptions.mbJson = false;
            continue;
        }
        if (InAllowPositionalRoot && !HasExplicitRoot && !IsOptionToken(Token))
        {
            OutOptions.mRepoRoot = Token;
            HasExplicitRoot = true;
            continue;
        }
        Remaining.push_back(Token);
    }
    return Remaining;
}

std::string ConsumeValuedOption(const std::vector<std::string> &InTokens,
                                size_t &InOutIndex,
                                const std::string &InOptionName)
{
    if (InOutIndex + 1 >= InTokens.size())
    {
        throw UsageError("Missing value for " + InOptionName);
    }
    InOutIndex += 1;
    return InTokens[InOutIndex];
}

bool ContainsHelpFlag(const std::vector<std::string> &InTokens)
{
    for (const std::string &Token : InTokens)
    {
        if (Token == "--help" || Token == "-h")
        {
            return true;
        }
    }
    return false;
}

std::string ValidateAndNormalizeStatusFilter(
    const std::string &InRawStatus, const std::string &InSupportedHint,
    const std::set<std::string> *InAllowedValues = nullptr)
{
    if (InRawStatus == "all")
    {
        return InRawStatus;
    }

    bool bNegate = false;
    std::string RawStatus = InRawStatus;
    if (!RawStatus.empty() && RawStatus[0] == '!')
    {
        bNegate = true;
        RawStatus = RawStatus.substr(1);
    }

    std::string Normalized;
    if (InAllowedValues != nullptr)
    {
        if (InAllowedValues->count(RawStatus) == 0)
        {
            throw UsageError("Unsupported status '" + InRawStatus + "'. " +
                             InSupportedHint);
        }
        Normalized = RawStatus;
    }
    else
    {
        Normalized = NormalizeStatusValue(RawStatus);
        if (Normalized == "unknown")
        {
            const std::string LoweredInput = ToLower(Trim(RawStatus));
            if (LoweredInput != "unknown")
            {
                throw UsageError("Unsupported status '" + InRawStatus + "'. " +
                                 InSupportedHint);
            }
        }
    }
    return bNegate ? ("!" + Normalized) : Normalized;
}

ListOptions ParseListOptions(const std::vector<std::string> &InTokens)
{
    ListOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--type")
        {
            Options.mKind =
                ToLower(ConsumeValuedOption(Remaining, Index, "--type"));
            continue;
        }
        if (Token == "--status" || Token == "--state")
        {
            Options.mStatus =
                ToLower(Trim(ConsumeValuedOption(Remaining, Index, Token)));
            continue;
        }
        throw UsageError("Unknown option for list: " + Token);
    }

    if (Options.mKind.empty())
    {
        throw UsageError("Missing required option --type");
    }

    static const std::set<std::string> AllowedKinds = {"implementation", "pair",
                                                       "plan", "playbook"};
    if (AllowedKinds.count(Options.mKind) == 0)
    {
        throw UsageError("Invalid value for --type: " + Options.mKind);
    }

    Options.mStatus = ValidateAndNormalizeStatusFilter(
        Options.mStatus, "Supported values: all | not_started | in_progress | "
                         "completed | closed | blocked | canceled | unknown "
                         "(prefix with ! to exclude)");

    return Options;
}

int ParsePositiveInteger(const std::string &InValue,
                         const std::string &InOptionName)
{
    try
    {
        size_t ParsedLength = 0;
        const int ParsedValue = std::stoi(InValue, &ParsedLength);
        if (ParsedLength != InValue.size() || ParsedValue <= 0)
        {
            throw UsageError("Invalid value for " + InOptionName + ": " +
                             InValue);
        }
        return ParsedValue;
    }
    catch (const std::invalid_argument &)
    {
        throw UsageError("Invalid value for " + InOptionName + ": " + InValue);
    }
    catch (const std::out_of_range &)
    {
        throw UsageError("Value out of range for " + InOptionName + ": " +
                         InValue);
    }
}

int ParseNonNegativeInteger(const std::string &InValue,
                            const std::string &InOptionName)
{
    try
    {
        size_t ParsedLength = 0;
        const int ParsedValue = std::stoi(InValue, &ParsedLength);
        if (ParsedLength != InValue.size() || ParsedValue < 0)
        {
            throw UsageError("Invalid value for " + InOptionName + ": " +
                             InValue);
        }
        return ParsedValue;
    }
    catch (const std::invalid_argument &)
    {
        throw UsageError("Invalid value for " + InOptionName + ": " + InValue);
    }
    catch (const std::out_of_range &)
    {
        throw UsageError("Value out of range for " + InOptionName + ": " +
                         InValue);
    }
}

ValidateOptions ParseValidateOptions(const std::vector<std::string> &InTokens)
{
    ValidateOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);

    for (const std::string &Token : Remaining)
    {
        if (Token == "--strict")
        {
            Options.mbStrict = true;
            continue;
        }
        throw UsageError("Unknown option for validate: " + Token);
    }
    return Options;
}

TimelineOptions ParseTimelineOptions(const std::vector<std::string> &InTokens)
{
    TimelineOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            Options.mTopic = ConsumeValuedOption(Remaining, Index, "--topic");
            continue;
        }
        if (Token == "--since")
        {
            Options.mSince = ConsumeValuedOption(Remaining, Index, "--since");
            continue;
        }
        throw UsageError("Unknown option for timeline: " + Token);
    }

    if (Trim(Options.mTopic).empty())
    {
        throw UsageError("Missing required option --topic");
    }
    if (!Options.mSince.empty() && !LooksLikeIsoDate(Options.mSince))
    {
        throw UsageError("Invalid value for --since (expected YYYY-MM-DD): " +
                         Options.mSince);
    }
    return Options;
}

BlockersOptions ParseBlockersOptions(const std::vector<std::string> &InTokens)
{
    BlockersOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--status")
        {
            Options.mStatus =
                ToLower(ConsumeValuedOption(Remaining, Index, "--status"));
            continue;
        }
        throw UsageError("Unknown option for blockers: " + Token);
    }

    static const std::set<std::string> AllowedStatus = {"open", "blocked"};
    Options.mStatus = ValidateAndNormalizeStatusFilter(
        Options.mStatus,
        "Supported values: all | open | blocked (prefix with ! to exclude)",
        &AllowedStatus);
    return Options;
}

CacheInfoOptions ParseCacheInfoOptions(const std::vector<std::string> &InTokens)
{
    CacheInfoOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        throw UsageError("Unknown option for cache info: " + Remaining[Index]);
    }
    return Options;
}

CacheClearOptions
ParseCacheClearOptions(const std::vector<std::string> &InTokens)
{
    CacheClearOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        throw UsageError("Unknown option for cache clear: " + Remaining[Index]);
    }
    return Options;
}

CacheConfigOptions
ParseCacheConfigOptions(const std::vector<std::string> &InTokens)
{
    CacheConfigOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);
    bool bHasAnyField = false;

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--dir")
        {
            Options.mDir = ConsumeValuedOption(Remaining, Index, "--dir");
            Options.mbDirSet = true;
            bHasAnyField = true;
            continue;
        }
        if (Token == "--enabled")
        {
            Options.mEnabled =
                ConsumeValuedOption(Remaining, Index, "--enabled");
            std::string Val = Options.mEnabled;
            std::transform(Val.begin(), Val.end(), Val.begin(), ::tolower);
            if (Val != "true" && Val != "false")
            {
                throw UsageError(
                    "Invalid value for --enabled (expected true|false): " +
                    Options.mEnabled);
            }
            Options.mEnabled = Val;
            bHasAnyField = true;
            continue;
        }
        if (Token == "--verbose")
        {
            Options.mVerbose =
                ConsumeValuedOption(Remaining, Index, "--verbose");
            std::string Val = Options.mVerbose;
            std::transform(Val.begin(), Val.end(), Val.begin(), ::tolower);
            if (Val != "true" && Val != "false")
            {
                throw UsageError(
                    "Invalid value for --verbose (expected true|false): " +
                    Options.mVerbose);
            }
            Options.mVerbose = Val;
            bHasAnyField = true;
            continue;
        }
        throw UsageError("Unknown option for cache config: " + Token);
    }

    if (!bHasAnyField)
    {
        throw UsageError("cache config requires at least one of --dir, "
                         "--enabled, or --verbose");
    }

    return Options;
}

// ---------------------------------------------------------------------------
// V4 bundle-native option parsers
// ---------------------------------------------------------------------------

FTopicListOptions
ParseTopicListOptions(const std::vector<std::string> &InTokens)
{
    FTopicListOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--status")
        {
            Options.mStatus = ConsumeValuedOption(Remaining, Index, "--status");
            continue;
        }
        throw UsageError("Unknown option for topic list: " + Token);
    }
    return Options;
}

FTopicGetOptions ParseTopicGetOptions(const std::vector<std::string> &InTokens)
{
    FTopicGetOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            Options.mTopic = ConsumeValuedOption(Remaining, Index, "--topic");
            continue;
        }
        throw UsageError("Unknown option for topic get: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("topic get requires --topic <topic>");
    return Options;
}

FPhaseListOptions
ParsePhaseListOptions(const std::vector<std::string> &InTokens)
{
    FPhaseListOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            Options.mTopic = ConsumeValuedOption(Remaining, Index, "--topic");
            continue;
        }
        if (Token == "--status")
        {
            Options.mStatus = ConsumeValuedOption(Remaining, Index, "--status");
            continue;
        }
        throw UsageError("Unknown option for phase list: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase list requires --topic <topic>");
    return Options;
}

FPhaseGetOptions ParsePhaseGetOptions(const std::vector<std::string> &InTokens)
{
    FPhaseGetOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            Options.mTopic = ConsumeValuedOption(Remaining, Index, "--topic");
            continue;
        }
        if (Token == "--phase")
        {
            const std::string Val =
                ConsumeValuedOption(Remaining, Index, "--phase");
            if (!Val.empty() &&
                !std::isdigit(static_cast<unsigned char>(Val[0])))
            {
                throw UsageError("Phase must be an integer index (e.g. 6), "
                                 "not a key (e.g. " +
                                 Val + ")");
            }
            Options.mPhaseIndex = std::atoi(Val.c_str());
            continue;
        }
        if (Token == "--brief")
        {
            Options.mbBrief = true;
            continue;
        }
        if (Token == "--execution")
        {
            Options.mbExecution = true;
            continue;
        }
        if (Token == "--reference")
        {
            Options.mbReference = true;
            continue;
        }
        throw UsageError("Unknown option for phase get: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase get requires --topic <topic>");
    if (Options.mPhaseIndex < 0)
        throw UsageError("phase get requires --phase <index>");
    return Options;
}

FBundleChangelogOptions
ParseBundleChangelogOptions(const std::vector<std::string> &InTokens)
{
    FBundleChangelogOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            Options.mTopic = ConsumeValuedOption(Remaining, Index, "--topic");
            continue;
        }
        if (Token == "--phase" || Token == "--scope")
        {
            Options.mScopeFilter = ConsumeValuedOption(Remaining, Index, Token);
            Options.mbHasScopeFilter = true;
            continue;
        }
        throw UsageError("Unknown option for changelog: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("changelog requires --topic <topic>");
    return Options;
}

FBundleVerificationOptions
ParseBundleVerificationOptions(const std::vector<std::string> &InTokens)
{
    FBundleVerificationOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            Options.mTopic = ConsumeValuedOption(Remaining, Index, "--topic");
            continue;
        }
        if (Token == "--phase" || Token == "--scope")
        {
            Options.mScopeFilter = ConsumeValuedOption(Remaining, Index, Token);
            Options.mbHasScopeFilter = true;
            continue;
        }
        throw UsageError("Unknown option for verification: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("verification requires --topic <topic>");
    return Options;
}

FBundleTimelineOptions
ParseBundleTimelineOptions(const std::vector<std::string> &InTokens)
{
    FBundleTimelineOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            Options.mTopic = ConsumeValuedOption(Remaining, Index, "--topic");
            continue;
        }
        if (Token == "--since")
        {
            Options.mSince = ConsumeValuedOption(Remaining, Index, "--since");
            continue;
        }
        if (Token == "--phase")
        {
            const std::string Val =
                ConsumeValuedOption(Remaining, Index, "--phase");
            Options.mPhaseFilter = std::atoi(Val.c_str());
            Options.mbHasPhaseFilter = true;
            continue;
        }
        throw UsageError("Unknown option for timeline: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("timeline requires --topic <topic>");
    return Options;
}

FBundleBlockersOptions
ParseBundleBlockersOptions(const std::vector<std::string> &InTokens)
{
    FBundleBlockersOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            Options.mTopic = ConsumeValuedOption(Remaining, Index, "--topic");
            continue;
        }
        throw UsageError("Unknown option for blockers: " + Token);
    }
    return Options;
}

FBundleValidateOptions
ParseBundleValidateOptions(const std::vector<std::string> &InTokens)
{
    FBundleValidateOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            Options.mTopic = ConsumeValuedOption(Remaining, Index, "--topic");
            continue;
        }
        if (Token == "--strict")
        {
            Options.mbStrict = true;
            continue;
        }
        throw UsageError("Unknown option for validate: " + Token);
    }
    return Options;
}

// ---------------------------------------------------------------------------
// Mutation option parsers
// ---------------------------------------------------------------------------

static void ParseRequiredTopic(const std::vector<std::string> &InRemaining,
                               size_t &InOutIndex, std::string &OutTopic)
{
    const std::string &Token = InRemaining[InOutIndex];
    if (Token == "--topic")
    {
        OutTopic = ConsumeValuedOption(InRemaining, InOutIndex, "--topic");
    }
}

static void ParseRequiredPhaseIndex(const std::vector<std::string> &InRemaining,
                                    size_t &InOutIndex, int &OutIndex)
{
    const std::string Val =
        ConsumeValuedOption(InRemaining, InOutIndex, "--phase");
    if (!Val.empty() && !std::isdigit(static_cast<unsigned char>(Val[0])))
    {
        throw UsageError("Phase must be an integer index (e.g. 6), "
                         "not a key (e.g. " +
                         Val + ")");
    }
    OutIndex = std::atoi(Val.c_str());
}

static void ParseRequiredIntIndex(const std::vector<std::string> &InRemaining,
                                  size_t &InOutIndex, const std::string &InFlag,
                                  int &OutIndex)
{
    const std::string Val =
        ConsumeValuedOption(InRemaining, InOutIndex, InFlag);
    OutIndex = std::atoi(Val.c_str());
}

FTopicSetOptions ParseTopicSetOptions(const std::vector<std::string> &InTokens)
{
    FTopicSetOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--status")
        {
            Options.mStatus = ConsumeValuedOption(Remaining, Index, "--status");
            continue;
        }
        if (Token == "--next-actions")
        {
            Options.mNextActions =
                ConsumeValuedOption(Remaining, Index, "--next-actions");
            continue;
        }
        if (Token == "--summary")
        {
            Options.mSummary =
                ConsumeValuedOption(Remaining, Index, "--summary");
            continue;
        }
        if (Token == "--goals")
        {
            Options.mGoals = ConsumeValuedOption(Remaining, Index, "--goals");
            continue;
        }
        if (Token == "--non-goals")
        {
            Options.mNonGoals =
                ConsumeValuedOption(Remaining, Index, "--non-goals");
            continue;
        }
        if (Token == "--risks")
        {
            Options.mRisks = ConsumeValuedOption(Remaining, Index, "--risks");
            continue;
        }
        if (Token == "--acceptance-criteria")
        {
            Options.mAcceptanceCriteria =
                ConsumeValuedOption(Remaining, Index, "--acceptance-criteria");
            continue;
        }
        if (Token == "--problem-statement")
        {
            Options.mProblemStatement =
                ConsumeValuedOption(Remaining, Index, "--problem-statement");
            continue;
        }
        if (Token == "--validation-clear")
        {
            Options.mbValidationClear = true;
            continue;
        }
        if (Token == "--validation-add")
        {
            // Parse "<platform>|<command>|<description>" (pipe-delimited).
            // Platform field may be empty (→ Any). Command is required.
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--validation-add");
            FValidationCommand C;
            const size_t Pipe1 = Raw.find('|');
            const std::string Plat =
                Pipe1 == std::string::npos ? "" : Raw.substr(0, Pipe1);
            if (!PlatformScopeFromString(Plat, C.mPlatform))
                C.mPlatform = EPlatformScope::Any;
            if (Pipe1 == std::string::npos)
            {
                C.mCommand = Raw;
            }
            else
            {
                const size_t Pipe2 = Raw.find('|', Pipe1 + 1);
                if (Pipe2 == std::string::npos)
                {
                    C.mCommand = Raw.substr(Pipe1 + 1);
                }
                else
                {
                    C.mCommand = Raw.substr(Pipe1 + 1, Pipe2 - Pipe1 - 1);
                    C.mDescription = Raw.substr(Pipe2 + 1);
                }
            }
            if (C.mCommand.empty())
                throw UsageError(
                    "--validation-add requires a non-empty <command> "
                    "segment; format: "
                    "'<platform>|<command>|<description>'");
            Options.mValidationAdd.push_back(std::move(C));
            continue;
        }
        if (Token == "--baseline-audit")
        {
            Options.mBaselineAudit =
                ConsumeValuedOption(Remaining, Index, "--baseline-audit");
            continue;
        }
        if (Token == "--execution-strategy")
        {
            Options.mExecutionStrategy =
                ConsumeValuedOption(Remaining, Index, "--execution-strategy");
            continue;
        }
        if (Token == "--locked-decisions")
        {
            Options.mLockedDecisions =
                ConsumeValuedOption(Remaining, Index, "--locked-decisions");
            continue;
        }
        if (Token == "--source-references")
        {
            Options.mSourceReferences =
                ConsumeValuedOption(Remaining, Index, "--source-references");
            continue;
        }
        if (Token == "--dependency-clear")
        {
            Options.mbDependencyClear = true;
            continue;
        }
        if (Token == "--dependency-add")
        {
            // Parse "<kind>|<topic>|<phase>|<path>|<note>" (pipe-delimited).
            // All fields optional except topic (Bundle/Phase) or path
            // (Governance/External). Kind defaults to bundle.
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--dependency-add");
            FBundleReference R;
            std::vector<std::string> Segments;
            std::string Current;
            for (char C : Raw)
            {
                if (C == '|')
                {
                    Segments.push_back(std::move(Current));
                    Current.clear();
                }
                else
                {
                    Current += C;
                }
            }
            Segments.push_back(std::move(Current));
            const std::string Kind = Segments.size() > 0 ? Segments[0] : "";
            if (!DependencyKindFromString(Kind, R.mKind))
                throw UsageError(
                    "--dependency-add: invalid kind '" + Kind +
                    "' (expected bundle|phase|governance|external)");
            if (Segments.size() > 1)
                R.mTopic = Segments[1];
            if (Segments.size() > 2 && !Segments[2].empty())
            {
                try
                {
                    R.mPhase = std::stoi(Segments[2]);
                }
                catch (const std::exception &)
                {
                    throw UsageError(
                        "--dependency-add: phase segment must be an integer");
                }
            }
            if (Segments.size() > 3)
                R.mPath = Segments[3];
            if (Segments.size() > 4)
            {
                R.mNote = Segments[4];
                for (size_t I = 5; I < Segments.size(); ++I)
                    R.mNote += "|" + Segments[I];
            }
            if ((R.mKind == EDependencyKind::Bundle ||
                 R.mKind == EDependencyKind::Phase) &&
                R.mTopic.empty())
                throw UsageError(
                    "--dependency-add: kind=bundle|phase requires non-empty "
                    "<topic> segment; format: "
                    "'<kind>|<topic>|<phase>|<path>|<note>'");
            Options.mDependencyAdd.push_back(std::move(R));
            continue;
        }
        throw UsageError("Unknown option for topic set: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("topic set requires --topic <topic>");
    return Options;
}

FPhaseSetOptions ParsePhaseSetOptions(const std::vector<std::string> &InTokens)
{
    FPhaseSetOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        if (Token == "--status")
        {
            Options.mStatus = ConsumeValuedOption(Remaining, Index, "--status");
            continue;
        }
        if (Token == "--done")
        {
            Options.mDone = ConsumeValuedOption(Remaining, Index, "--done");
            continue;
        }
        if (Token == "--remaining")
        {
            Options.mRemaining =
                ConsumeValuedOption(Remaining, Index, "--remaining");
            continue;
        }
        if (Token == "--blockers")
        {
            Options.mBlockers =
                ConsumeValuedOption(Remaining, Index, "--blockers");
            continue;
        }
        if (Token == "--context")
        {
            Options.mContext =
                ConsumeValuedOption(Remaining, Index, "--context");
            continue;
        }
        if (Token == "--scope")
        {
            Options.mScope = ConsumeValuedOption(Remaining, Index, "--scope");
            continue;
        }
        if (Token == "--output")
        {
            Options.mOutput = ConsumeValuedOption(Remaining, Index, "--output");
            continue;
        }
        if (Token == "--investigation")
        {
            Options.mInvestigation =
                ConsumeValuedOption(Remaining, Index, "--investigation");
            continue;
        }
        if (Token == "--code-entity-contract")
        {
            Options.mCodeEntityContract =
                ConsumeValuedOption(Remaining, Index, "--code-entity-contract");
            continue;
        }
        if (Token == "--code-snippets")
        {
            Options.mCodeSnippets =
                ConsumeValuedOption(Remaining, Index, "--code-snippets");
            continue;
        }
        if (Token == "--best-practices")
        {
            Options.mBestPractices =
                ConsumeValuedOption(Remaining, Index, "--best-practices");
            continue;
        }
        if (Token == "--multi-platforming")
        {
            Options.mMultiPlatforming =
                ConsumeValuedOption(Remaining, Index, "--multi-platforming");
            continue;
        }
        if (Token == "--readiness-gate")
        {
            Options.mReadinessGate =
                ConsumeValuedOption(Remaining, Index, "--readiness-gate");
            continue;
        }
        if (Token == "--handoff")
        {
            Options.mHandoff =
                ConsumeValuedOption(Remaining, Index, "--handoff");
            continue;
        }
        if (Token == "--validation-clear")
        {
            Options.mbValidationClear = true;
            continue;
        }
        if (Token == "--validation-add")
        {
            // Parse "<platform>|<command>|<description>" (pipe-delimited).
            // Platform field may be empty (→ Any). Command is required.
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--validation-add");
            FValidationCommand C;
            const size_t Pipe1 = Raw.find('|');
            const std::string Plat =
                Pipe1 == std::string::npos ? "" : Raw.substr(0, Pipe1);
            if (!PlatformScopeFromString(Plat, C.mPlatform))
                C.mPlatform = EPlatformScope::Any;
            if (Pipe1 == std::string::npos)
            {
                C.mCommand = Raw;
            }
            else
            {
                const size_t Pipe2 = Raw.find('|', Pipe1 + 1);
                if (Pipe2 == std::string::npos)
                {
                    C.mCommand = Raw.substr(Pipe1 + 1);
                }
                else
                {
                    C.mCommand = Raw.substr(Pipe1 + 1, Pipe2 - Pipe1 - 1);
                    C.mDescription = Raw.substr(Pipe2 + 1);
                }
            }
            if (C.mCommand.empty())
                throw UsageError(
                    "--validation-add requires a non-empty <command> "
                    "segment; format: "
                    "'<platform>|<command>|<description>'");
            Options.mValidationAdd.push_back(std::move(C));
            continue;
        }
        if (Token == "--dependency-clear")
        {
            Options.mbDependencyClear = true;
            continue;
        }
        if (Token == "--dependency-add")
        {
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--dependency-add");
            FBundleReference R;
            std::vector<std::string> Segments;
            std::string Current;
            for (char C : Raw)
            {
                if (C == '|')
                {
                    Segments.push_back(std::move(Current));
                    Current.clear();
                }
                else
                {
                    Current += C;
                }
            }
            Segments.push_back(std::move(Current));
            const std::string Kind = Segments.size() > 0 ? Segments[0] : "";
            if (!DependencyKindFromString(Kind, R.mKind))
                throw UsageError(
                    "--dependency-add: invalid kind '" + Kind +
                    "' (expected bundle|phase|governance|external)");
            if (Segments.size() > 1)
                R.mTopic = Segments[1];
            if (Segments.size() > 2 && !Segments[2].empty())
            {
                try
                {
                    R.mPhase = std::stoi(Segments[2]);
                }
                catch (const std::exception &)
                {
                    throw UsageError(
                        "--dependency-add: phase segment must be an integer");
                }
            }
            if (Segments.size() > 3)
                R.mPath = Segments[3];
            if (Segments.size() > 4)
            {
                R.mNote = Segments[4];
                for (size_t I = 5; I < Segments.size(); ++I)
                    R.mNote += "|" + Segments[I];
            }
            if ((R.mKind == EDependencyKind::Bundle ||
                 R.mKind == EDependencyKind::Phase) &&
                R.mTopic.empty())
                throw UsageError(
                    "--dependency-add: kind=bundle|phase requires non-empty "
                    "<topic> segment");
            Options.mDependencyAdd.push_back(std::move(R));
            continue;
        }
        throw UsageError("Unknown option for phase set: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase set requires --topic <topic>");
    if (Options.mPhaseIndex < 0)
        throw UsageError("phase set requires --phase <index>");
    return Options;
}

FJobSetOptions ParseJobSetOptions(const std::vector<std::string> &InTokens)
{
    FJobSetOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        if (Token == "--job")
        {
            ParseRequiredIntIndex(Remaining, Index, "--job", Options.mJobIndex);
            continue;
        }
        if (Token == "--status")
        {
            Options.mStatus = ConsumeValuedOption(Remaining, Index, "--status");
            continue;
        }
        if (Token == "--scope")
        {
            Options.mScope = ConsumeValuedOption(Remaining, Index, "--scope");
            continue;
        }
        if (Token == "--output")
        {
            Options.mOutput = ConsumeValuedOption(Remaining, Index, "--output");
            continue;
        }
        if (Token == "--exit-criteria")
        {
            Options.mExitCriteria =
                ConsumeValuedOption(Remaining, Index, "--exit-criteria");
            continue;
        }
        if (Token == "--lane")
        {
            ParseRequiredIntIndex(Remaining, Index, "--lane",
                                  Options.mLaneIndex);
            continue;
        }
        if (Token == "--wave")
        {
            ParseRequiredIntIndex(Remaining, Index, "--wave", Options.mWave);
            continue;
        }
        throw UsageError("Unknown option for job set: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("job set requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("job set requires --phase");
    if (Options.mJobIndex < 0)
        throw UsageError("job set requires --job");
    return Options;
}

FTaskSetOptions ParseTaskSetOptions(const std::vector<std::string> &InTokens)
{
    FTaskSetOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        if (Token == "--job")
        {
            ParseRequiredIntIndex(Remaining, Index, "--job", Options.mJobIndex);
            continue;
        }
        if (Token == "--task")
        {
            ParseRequiredIntIndex(Remaining, Index, "--task",
                                  Options.mTaskIndex);
            continue;
        }
        if (Token == "--status")
        {
            Options.mStatus = ConsumeValuedOption(Remaining, Index, "--status");
            continue;
        }
        if (Token == "--evidence")
        {
            Options.mEvidence =
                ConsumeValuedOption(Remaining, Index, "--evidence");
            continue;
        }
        if (Token == "--notes")
        {
            Options.mNotes = ConsumeValuedOption(Remaining, Index, "--notes");
            continue;
        }
        throw UsageError("Unknown option for task set: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("task set requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("task set requires --phase");
    if (Options.mJobIndex < 0)
        throw UsageError("task set requires --job");
    if (Options.mTaskIndex < 0)
        throw UsageError("task set requires --task");
    return Options;
}

FChangelogAddOptions
ParseChangelogAddOptions(const std::vector<std::string> &InTokens)
{
    FChangelogAddOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--scope" || Token == "--phase")
        {
            Options.mScope = ConsumeValuedOption(Remaining, Index, Token);
            continue;
        }
        if (Token == "--change")
        {
            Options.mChange = ConsumeValuedOption(Remaining, Index, "--change");
            continue;
        }
        if (Token == "--type")
        {
            Options.mType = ConsumeValuedOption(Remaining, Index, "--type");
            continue;
        }
        if (Token == "--affected")
        {
            Options.mAffected =
                ConsumeValuedOption(Remaining, Index, "--affected");
            continue;
        }
        throw UsageError("Unknown option for changelog add: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("changelog add requires --topic");
    if (Options.mChange.empty())
        throw UsageError("changelog add requires --change");
    return Options;
}

FVerificationAddOptions
ParseVerificationAddOptions(const std::vector<std::string> &InTokens)
{
    FVerificationAddOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--scope" || Token == "--phase")
        {
            Options.mScope = ConsumeValuedOption(Remaining, Index, Token);
            continue;
        }
        if (Token == "--check")
        {
            Options.mCheck = ConsumeValuedOption(Remaining, Index, "--check");
            continue;
        }
        if (Token == "--result")
        {
            Options.mResult = ConsumeValuedOption(Remaining, Index, "--result");
            continue;
        }
        if (Token == "--detail")
        {
            Options.mDetail = ConsumeValuedOption(Remaining, Index, "--detail");
            continue;
        }
        throw UsageError("Unknown option for verification add: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("verification add requires --topic");
    if (Options.mCheck.empty())
        throw UsageError("verification add requires --check");
    return Options;
}

// ---------------------------------------------------------------------------
// Semantic command option parsers
// ---------------------------------------------------------------------------

// Tier 1 — Phase lifecycle

FPhaseStartOptions
ParsePhaseStartOptions(const std::vector<std::string> &InTokens)
{
    FPhaseStartOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        if (Token == "--context")
        {
            Options.mContext =
                ConsumeValuedOption(Remaining, Index, "--context");
            continue;
        }
        throw UsageError("Unknown option for phase start: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase start requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("phase start requires --phase");
    return Options;
}

FPhaseCompleteOptions
ParsePhaseCompleteOptions(const std::vector<std::string> &InTokens)
{
    FPhaseCompleteOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        if (Token == "--done")
        {
            Options.mDone = ConsumeValuedOption(Remaining, Index, "--done");
            continue;
        }
        if (Token == "--verification")
        {
            Options.mVerification =
                ConsumeValuedOption(Remaining, Index, "--verification");
            continue;
        }
        throw UsageError("Unknown option for phase complete: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase complete requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("phase complete requires --phase");
    if (Options.mDone.empty())
        throw UsageError("phase complete requires --done");
    return Options;
}

FPhaseBlockOptions
ParsePhaseBlockOptions(const std::vector<std::string> &InTokens)
{
    FPhaseBlockOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        if (Token == "--reason")
        {
            Options.mReason = ConsumeValuedOption(Remaining, Index, "--reason");
            continue;
        }
        throw UsageError("Unknown option for phase block: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase block requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("phase block requires --phase");
    if (Options.mReason.empty())
        throw UsageError("phase block requires --reason");
    return Options;
}

FPhaseUnblockOptions
ParsePhaseUnblockOptions(const std::vector<std::string> &InTokens)
{
    FPhaseUnblockOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        throw UsageError("Unknown option for phase unblock: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase unblock requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("phase unblock requires --phase");
    return Options;
}

FPhaseProgressOptions
ParsePhaseProgressOptions(const std::vector<std::string> &InTokens)
{
    FPhaseProgressOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        if (Token == "--done")
        {
            Options.mDone = ConsumeValuedOption(Remaining, Index, "--done");
            continue;
        }
        if (Token == "--remaining")
        {
            Options.mRemaining =
                ConsumeValuedOption(Remaining, Index, "--remaining");
            continue;
        }
        throw UsageError("Unknown option for phase progress: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase progress requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("phase progress requires --phase");
    if (Options.mDone.empty())
        throw UsageError("phase progress requires --done");
    if (Options.mRemaining.empty())
        throw UsageError("phase progress requires --remaining");
    return Options;
}

FPhaseCompleteJobsOptions
ParsePhaseCompleteJobsOptions(const std::vector<std::string> &InTokens)
{
    FPhaseCompleteJobsOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        throw UsageError("Unknown option for phase complete-jobs: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase complete-jobs requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("phase complete-jobs requires --phase");
    return Options;
}

// Tier 2 — Topic lifecycle

FTopicStartOptions
ParseTopicStartOptions(const std::vector<std::string> &InTokens)
{
    FTopicStartOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        throw UsageError("Unknown option for topic start: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("topic start requires --topic");
    return Options;
}

FTopicCompleteOptions
ParseTopicCompleteOptions(const std::vector<std::string> &InTokens)
{
    FTopicCompleteOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--verification")
        {
            Options.mVerification =
                ConsumeValuedOption(Remaining, Index, "--verification");
            continue;
        }
        throw UsageError("Unknown option for topic complete: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("topic complete requires --topic");
    return Options;
}

FTopicBlockOptions
ParseTopicBlockOptions(const std::vector<std::string> &InTokens)
{
    FTopicBlockOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--reason")
        {
            Options.mReason = ConsumeValuedOption(Remaining, Index, "--reason");
            continue;
        }
        throw UsageError("Unknown option for topic block: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("topic block requires --topic");
    if (Options.mReason.empty())
        throw UsageError("topic block requires --reason");
    return Options;
}

// Tier 3 — Evidence shortcuts (return existing option types)

FChangelogAddOptions
ParsePhaseLogOptions(const std::vector<std::string> &InTokens)
{
    FChangelogAddOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            Options.mScope = ConsumeValuedOption(Remaining, Index, "--phase");
            continue;
        }
        if (Token == "--change")
        {
            Options.mChange = ConsumeValuedOption(Remaining, Index, "--change");
            continue;
        }
        if (Token == "--type")
        {
            Options.mType = ConsumeValuedOption(Remaining, Index, "--type");
            continue;
        }
        if (Token == "--affected")
        {
            Options.mAffected =
                ConsumeValuedOption(Remaining, Index, "--affected");
            continue;
        }
        throw UsageError("Unknown option for phase log: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase log requires --topic");
    if (Options.mScope.empty())
        throw UsageError("phase log requires --phase");
    if (Options.mChange.empty())
        throw UsageError("phase log requires --change");
    return Options;
}

FVerificationAddOptions
ParsePhaseVerifyOptions(const std::vector<std::string> &InTokens)
{
    FVerificationAddOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            Options.mScope = ConsumeValuedOption(Remaining, Index, "--phase");
            continue;
        }
        if (Token == "--check")
        {
            Options.mCheck = ConsumeValuedOption(Remaining, Index, "--check");
            continue;
        }
        if (Token == "--result")
        {
            Options.mResult = ConsumeValuedOption(Remaining, Index, "--result");
            continue;
        }
        if (Token == "--detail")
        {
            Options.mDetail = ConsumeValuedOption(Remaining, Index, "--detail");
            continue;
        }
        throw UsageError("Unknown option for phase verify: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase verify requires --topic");
    if (Options.mScope.empty())
        throw UsageError("phase verify requires --phase");
    if (Options.mCheck.empty())
        throw UsageError("phase verify requires --check");
    return Options;
}

// Tier 4 — Query helpers

FPhaseQueryOptions
ParsePhaseQueryOptions(const std::vector<std::string> &InTokens)
{
    FPhaseQueryOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        throw UsageError("Unknown option: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("Requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("Requires --phase");
    return Options;
}

// Tier 5 — Missing entity coverage

FLaneSetOptions ParseLaneSetOptions(const std::vector<std::string> &InTokens)
{
    FLaneSetOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        if (Token == "--lane")
        {
            ParseRequiredIntIndex(Remaining, Index, "--lane",
                                  Options.mLaneIndex);
            continue;
        }
        if (Token == "--status")
        {
            Options.mStatus = ConsumeValuedOption(Remaining, Index, "--status");
            continue;
        }
        if (Token == "--scope")
        {
            Options.mScope = ConsumeValuedOption(Remaining, Index, "--scope");
            continue;
        }
        if (Token == "--exit-criteria")
        {
            Options.mExitCriteria =
                ConsumeValuedOption(Remaining, Index, "--exit-criteria");
            continue;
        }
        throw UsageError("Unknown option for lane set: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("lane set requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("lane set requires --phase");
    if (Options.mLaneIndex < 0)
        throw UsageError("lane set requires --lane");
    return Options;
}

FTestingAddOptions
ParseTestingAddOptions(const std::vector<std::string> &InTokens)
{
    FTestingAddOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        if (Token == "--session")
        {
            Options.mSession =
                ConsumeValuedOption(Remaining, Index, "--session");
            continue;
        }
        if (Token == "--actor")
        {
            Options.mActor = ConsumeValuedOption(Remaining, Index, "--actor");
            continue;
        }
        if (Token == "--step")
        {
            Options.mStep = ConsumeValuedOption(Remaining, Index, "--step");
            continue;
        }
        if (Token == "--action")
        {
            Options.mAction = ConsumeValuedOption(Remaining, Index, "--action");
            continue;
        }
        if (Token == "--expected")
        {
            Options.mExpected =
                ConsumeValuedOption(Remaining, Index, "--expected");
            continue;
        }
        if (Token == "--evidence")
        {
            Options.mEvidence =
                ConsumeValuedOption(Remaining, Index, "--evidence");
            continue;
        }
        throw UsageError("Unknown option for testing add: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("testing add requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("testing add requires --phase");
    if (Options.mStep.empty())
        throw UsageError("testing add requires --step");
    if (Options.mAction.empty())
        throw UsageError("testing add requires --action");
    if (Options.mExpected.empty())
        throw UsageError("testing add requires --expected");
    return Options;
}

FManifestAddOptions
ParseManifestAddOptions(const std::vector<std::string> &InTokens)
{
    FManifestAddOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        if (Token == "--file")
        {
            Options.mFile = ConsumeValuedOption(Remaining, Index, "--file");
            continue;
        }
        if (Token == "--action")
        {
            Options.mAction = ConsumeValuedOption(Remaining, Index, "--action");
            continue;
        }
        if (Token == "--description")
        {
            Options.mDescription =
                ConsumeValuedOption(Remaining, Index, "--description");
            continue;
        }
        throw UsageError("Unknown option for manifest add: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("manifest add requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("manifest add requires --phase");
    if (Options.mFile.empty())
        throw UsageError("manifest add requires --file");
    if (Options.mAction.empty())
        throw UsageError("manifest add requires --action");
    if (Options.mDescription.empty())
        throw UsageError("manifest add requires --description");
    return Options;
}

// ---------------------------------------------------------------------------
// Modify-existing array entries (set semantics, target by --index)
// ---------------------------------------------------------------------------

FTestingSetOptions
ParseTestingSetOptions(const std::vector<std::string> &InTokens)
{
    FTestingSetOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        if (Token == "--index")
        {
            ParseRequiredIntIndex(Remaining, Index, "--index", Options.mIndex);
            continue;
        }
        if (Token == "--session")
        {
            Options.mSession =
                ConsumeValuedOption(Remaining, Index, "--session");
            continue;
        }
        if (Token == "--actor")
        {
            Options.mActor = ConsumeValuedOption(Remaining, Index, "--actor");
            continue;
        }
        if (Token == "--step")
        {
            Options.mStep = ConsumeValuedOption(Remaining, Index, "--step");
            continue;
        }
        if (Token == "--action")
        {
            Options.mAction = ConsumeValuedOption(Remaining, Index, "--action");
            continue;
        }
        if (Token == "--expected")
        {
            Options.mExpected =
                ConsumeValuedOption(Remaining, Index, "--expected");
            continue;
        }
        if (Token == "--evidence")
        {
            Options.mEvidence =
                ConsumeValuedOption(Remaining, Index, "--evidence");
            continue;
        }
        throw UsageError("Unknown option for testing set: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("testing set requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("testing set requires --phase");
    if (Options.mIndex < 0)
        throw UsageError("testing set requires --index");
    return Options;
}

FVerificationSetOptions
ParseVerificationSetOptions(const std::vector<std::string> &InTokens)
{
    FVerificationSetOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--index")
        {
            ParseRequiredIntIndex(Remaining, Index, "--index", Options.mIndex);
            continue;
        }
        if (Token == "--check")
        {
            Options.mCheck = ConsumeValuedOption(Remaining, Index, "--check");
            continue;
        }
        if (Token == "--result")
        {
            Options.mResult = ConsumeValuedOption(Remaining, Index, "--result");
            continue;
        }
        if (Token == "--detail")
        {
            Options.mDetail = ConsumeValuedOption(Remaining, Index, "--detail");
            continue;
        }
        throw UsageError("Unknown option for verification set: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("verification set requires --topic");
    if (Options.mIndex < 0)
        throw UsageError("verification set requires --index");
    return Options;
}

FManifestSetOptions
ParseManifestSetOptions(const std::vector<std::string> &InTokens)
{
    FManifestSetOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        if (Token == "--index")
        {
            ParseRequiredIntIndex(Remaining, Index, "--index", Options.mIndex);
            continue;
        }
        if (Token == "--file")
        {
            Options.mFile = ConsumeValuedOption(Remaining, Index, "--file");
            continue;
        }
        if (Token == "--action")
        {
            Options.mAction = ConsumeValuedOption(Remaining, Index, "--action");
            continue;
        }
        if (Token == "--description")
        {
            Options.mDescription =
                ConsumeValuedOption(Remaining, Index, "--description");
            continue;
        }
        throw UsageError("Unknown option for manifest set: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("manifest set requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("manifest set requires --phase");
    if (Options.mIndex < 0)
        throw UsageError("manifest set requires --index");
    return Options;
}

FChangelogSetOptions
ParseChangelogSetOptions(const std::vector<std::string> &InTokens)
{
    FChangelogSetOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--index")
        {
            ParseRequiredIntIndex(Remaining, Index, "--index", Options.mIndex);
            continue;
        }
        if (Token == "--phase")
        {
            const std::string V =
                ConsumeValuedOption(Remaining, Index, "--phase");
            if (V == "topic" || V == "-1")
                Options.mPhase = -1;
            else
                Options.mPhase = std::atoi(V.c_str());
            continue;
        }
        if (Token == "--date")
        {
            Options.mDate = ConsumeValuedOption(Remaining, Index, "--date");
            continue;
        }
        if (Token == "--change")
        {
            Options.mChange = ConsumeValuedOption(Remaining, Index, "--change");
            continue;
        }
        if (Token == "--type")
        {
            Options.mType = ConsumeValuedOption(Remaining, Index, "--type");
            continue;
        }
        if (Token == "--affected")
        {
            Options.mAffected =
                ConsumeValuedOption(Remaining, Index, "--affected");
            continue;
        }
        throw UsageError("Unknown option for changelog set: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("changelog set requires --topic");
    if (Options.mIndex < 0)
        throw UsageError("changelog set requires --index");
    return Options;
}

FLaneAddOptions ParseLaneAddOptions(const std::vector<std::string> &InTokens)
{
    FLaneAddOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--phase")
        {
            ParseRequiredPhaseIndex(Remaining, Index, Options.mPhaseIndex);
            continue;
        }
        if (Token == "--status")
        {
            Options.mStatus = ConsumeValuedOption(Remaining, Index, "--status");
            continue;
        }
        if (Token == "--scope")
        {
            Options.mScope = ConsumeValuedOption(Remaining, Index, "--scope");
            continue;
        }
        if (Token == "--exit-criteria")
        {
            Options.mExitCriteria =
                ConsumeValuedOption(Remaining, Index, "--exit-criteria");
            continue;
        }
        throw UsageError("Unknown option for lane add: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("lane add requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("lane add requires --phase");
    return Options;
}

FPhaseAddOptions ParsePhaseAddOptions(const std::vector<std::string> &InTokens)
{
    FPhaseAddOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        if (Token == "--scope")
        {
            Options.mScope = ConsumeValuedOption(Remaining, Index, "--scope");
            continue;
        }
        if (Token == "--output")
        {
            Options.mOutput = ConsumeValuedOption(Remaining, Index, "--output");
            continue;
        }
        if (Token == "--status")
        {
            Options.mStatus = ConsumeValuedOption(Remaining, Index, "--status");
            continue;
        }
        throw UsageError("Unknown option for phase add: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase add requires --topic");
    return Options;
}

} // namespace UniPlan
