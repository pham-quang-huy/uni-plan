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
        throw UsageError("Unknown option for job set: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("job set requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("job set requires --phase");
    if (Options.mJobIndex < 0)
        throw UsageError("job set requires --job");
    if (Options.mStatus.empty())
        throw UsageError("job set requires --status");
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
        if (Token == "--scope")
        {
            Options.mScope = ConsumeValuedOption(Remaining, Index, "--scope");
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
        if (Token == "--scope")
        {
            Options.mScope = ConsumeValuedOption(Remaining, Index, "--scope");
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

} // namespace UniPlan
