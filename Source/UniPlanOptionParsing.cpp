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

PhaseOptions ParsePhaseOptions(const std::vector<std::string> &InTokens)
{
    PhaseOptions Options;
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
        if (Token == "--status" || Token == "--state")
        {
            Options.mStatus =
                ToLower(Trim(ConsumeValuedOption(Remaining, Index, Token)));
            continue;
        }
        throw UsageError("Unknown option for phase: " + Token);
    }

    Options.mStatus = ValidateAndNormalizeStatusFilter(
        Options.mStatus, "Supported values: all | not_started | in_progress | "
                         "completed | closed | blocked | canceled | unknown "
                         "(prefix with ! to exclude)");
    return Options;
}

LintOptions ParseLintOptions(const std::vector<std::string> &InTokens)
{
    LintOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options, true);

    for (const std::string &Token : Remaining)
    {
        if (Token == "--fail-on-warning" || Token == "--strict")
        {
            Options.mbFailOnWarning = true;
            continue;
        }
        throw UsageError("Unknown option for lint: " + Token);
    }
    return Options;
}

InventoryOptions ParseInventoryOptions(const std::vector<std::string> &InTokens)
{
    InventoryOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options, true);

    for (const std::string &Token : Remaining)
    {
        throw UsageError("Unknown option for inventory: " + Token);
    }
    return Options;
}

OrphanCheckOptions
ParseOrphanCheckOptions(const std::vector<std::string> &InTokens)
{
    OrphanCheckOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options, true);

    for (const std::string &Token : Remaining)
    {
        throw UsageError("Unknown option for orphan-check: " + Token);
    }
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

ArtifactsOptions ParseArtifactsOptions(const std::vector<std::string> &InTokens)
{
    ArtifactsOptions Options;
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
        if (Token == "--type")
        {
            Options.mKind =
                ToLower(ConsumeValuedOption(Remaining, Index, "--type"));
            continue;
        }
        throw UsageError("Unknown option for artifacts: " + Token);
    }

    if (Trim(Options.mTopic).empty())
    {
        throw UsageError("Missing required option --topic");
    }

    static const std::set<std::string> AllowedKinds = {
        "all", "implementation", "plan", "playbook", "sidecar"};
    if (AllowedKinds.count(Options.mKind) == 0)
    {
        throw UsageError("Invalid value for --type: " + Options.mKind);
    }
    return Options;
}

EvidenceOptions ParseEvidenceOptions(const std::vector<std::string> &InTokens,
                                     const std::string &InCommandName)
{
    EvidenceOptions Options;
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
        if (Token == "--for")
        {
            Options.mDocClass =
                ToLower(ConsumeValuedOption(Remaining, Index, "--for"));
            continue;
        }
        if (Token == "--phase")
        {
            Options.mPhaseKey =
                ConsumeValuedOption(Remaining, Index, "--phase");
            continue;
        }
        throw UsageError("Unknown option for " + InCommandName + ": " + Token);
    }

    if (Trim(Options.mTopic).empty())
    {
        throw UsageError("Missing required option --topic");
    }
    if (Trim(Options.mDocClass).empty())
    {
        throw UsageError("Missing required option --for");
    }

    static const std::set<std::string> AllowedDocClasses = {
        "plan", "implementation", "playbook"};
    if (AllowedDocClasses.count(Options.mDocClass) == 0)
    {
        throw UsageError("Invalid value for --for: " + Options.mDocClass);
    }
    if (Options.mDocClass != "playbook" && !Options.mPhaseKey.empty())
    {
        throw UsageError("--phase is only valid when --for playbook");
    }
    return Options;
}

SchemaOptions ParseSchemaOptions(const std::vector<std::string> &InTokens)
{
    SchemaOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--type")
        {
            Options.mType =
                ToLower(ConsumeValuedOption(Remaining, Index, "--type"));
            continue;
        }
        throw UsageError("Unknown option for schema: " + Token);
    }

    if (!IsSupportedSchemaType(Options.mType))
    {
        throw UsageError("Invalid value for --type: " + Options.mType +
                         ". Supported values: doc | plan | playbook | "
                         "implementation | changelog | verification");
    }
    return Options;
}

RulesOptions ParseRulesOptions(const std::vector<std::string> &InTokens)
{
    RulesOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);

    for (const std::string &Token : Remaining)
    {
        throw UsageError("Unknown option for rules: " + Token);
    }
    return Options;
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

SectionResolveOptions
ParseSectionResolveOptions(const std::vector<std::string> &InTokens)
{
    SectionResolveOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--doc")
        {
            Options.mDocPath = ConsumeValuedOption(Remaining, Index, "--doc");
            continue;
        }
        if (Token == "--section")
        {
            Options.mSection =
                ConsumeValuedOption(Remaining, Index, "--section");
            continue;
        }
        throw UsageError("Unknown option for section resolve: " + Token);
    }

    if (Trim(Options.mDocPath).empty())
    {
        throw UsageError("Missing required option --doc");
    }
    if (Trim(Options.mSection).empty())
    {
        throw UsageError("Missing required option --section");
    }
    return Options;
}

SectionSchemaOptions
ParseSectionSchemaOptions(const std::vector<std::string> &InTokens)
{
    SectionSchemaOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--type")
        {
            Options.mType =
                ToLower(ConsumeValuedOption(Remaining, Index, "--type"));
            continue;
        }
        throw UsageError("Unknown option for section schema: " + Token);
    }

    if (!IsSupportedSchemaType(Options.mType))
    {
        throw UsageError("Invalid value for --type: " + Options.mType +
                         ". Supported values: doc | plan | playbook | "
                         "implementation | changelog | verification");
    }
    return Options;
}

SectionListOptions
ParseSectionListOptions(const std::vector<std::string> &InTokens)
{
    SectionListOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--doc")
        {
            Options.mDocPath = ConsumeValuedOption(Remaining, Index, "--doc");
            continue;
        }
        if (Token == "--count")
        {
            Options.mbCount = true;
            continue;
        }
        throw UsageError("Unknown option for section list: " + Token);
    }
    return Options;
}

SectionContentOptions
ParseSectionContentOptions(const std::vector<std::string> &InTokens)
{
    SectionContentOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--doc")
        {
            Options.mDocPath = ConsumeValuedOption(Remaining, Index, "--doc");
            continue;
        }
        if (Token == "--id")
        {
            Options.mSection = ConsumeValuedOption(Remaining, Index, "--id");
            continue;
        }
        if (Token == "--line-char-limit")
        {
            const std::string Value =
                ConsumeValuedOption(Remaining, Index, "--line-char-limit");
            try
            {
                Options.mLineCharLimit = std::stoi(Value);
            }
            catch (...)
            {
                throw UsageError("Invalid value for --line-char-limit: " +
                                 Value);
            }
            if (Options.mLineCharLimit < 0)
            {
                throw UsageError("--line-char-limit must be non-negative");
            }
            continue;
        }
        throw UsageError("Unknown option for section content: " + Token);
    }

    if (Trim(Options.mDocPath).empty())
    {
        throw UsageError("Missing required option --doc");
    }
    if (Trim(Options.mSection).empty())
    {
        throw UsageError("Missing required option --id");
    }
    return Options;
}

ExcerptOptions ParseExcerptOptions(const std::vector<std::string> &InTokens)
{
    ExcerptOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--doc")
        {
            Options.mDocPath = ConsumeValuedOption(Remaining, Index, "--doc");
            continue;
        }
        if (Token == "--section")
        {
            Options.mSection =
                ConsumeValuedOption(Remaining, Index, "--section");
            continue;
        }
        if (Token == "--context-lines")
        {
            Options.mContextLines = ParseNonNegativeInteger(
                ConsumeValuedOption(Remaining, Index, "--context-lines"),
                "--context-lines");
            continue;
        }
        throw UsageError("Unknown option for excerpt: " + Token);
    }

    if (Trim(Options.mDocPath).empty())
    {
        throw UsageError("Missing required option --doc");
    }
    if (Trim(Options.mSection).empty())
    {
        throw UsageError("Missing required option --section");
    }
    return Options;
}

TableListOptions ParseTableListOptions(const std::vector<std::string> &InTokens)
{
    TableListOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--doc")
        {
            Options.mDocPath = ConsumeValuedOption(Remaining, Index, "--doc");
            continue;
        }
        throw UsageError("Unknown option for table list: " + Token);
    }

    if (Trim(Options.mDocPath).empty())
    {
        throw UsageError("Missing required option --doc");
    }
    return Options;
}

TableGetOptions ParseTableGetOptions(const std::vector<std::string> &InTokens)
{
    TableGetOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--doc")
        {
            Options.mDocPath = ConsumeValuedOption(Remaining, Index, "--doc");
            continue;
        }
        if (Token == "--table-id")
        {
            Options.mTableId = ParsePositiveInteger(
                ConsumeValuedOption(Remaining, Index, "--table-id"),
                "--table-id");
            continue;
        }
        throw UsageError("Unknown option for table get: " + Token);
    }

    if (Trim(Options.mDocPath).empty())
    {
        throw UsageError("Missing required option --doc");
    }
    if (Options.mTableId <= 0)
    {
        throw UsageError("Missing required option --table-id");
    }
    return Options;
}

GraphOptions ParseGraphOptions(const std::vector<std::string> &InTokens)
{
    GraphOptions Options;
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
        if (Token == "--depth")
        {
            Options.mDepth = ParseNonNegativeInteger(
                ConsumeValuedOption(Remaining, Index, "--depth"), "--depth");
            continue;
        }
        throw UsageError("Unknown option for graph: " + Token);
    }

    if (Trim(Options.mTopic).empty())
    {
        throw UsageError("Missing required option --topic");
    }
    return Options;
}

DiagnoseDriftOptions
ParseDiagnoseDriftOptions(const std::vector<std::string> &InTokens)
{
    DiagnoseDriftOptions Options;
    const std::vector<std::string> Remaining =
        ConsumeCommonOptions(InTokens, Options);

    for (const std::string &Token : Remaining)
    {
        throw UsageError("Unknown option for diagnose drift: " + Token);
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

MigrateOptions ParseMigrateOptions(const std::vector<std::string> &InTokens)
{
    MigrateOptions Options;
    if (InTokens.empty())
    {
        throw UsageError("Missing subcommand. Usage: uni-plan migrate "
                         "<md-to-json|verify|status> [options]");
    }

    Options.mSubcommand = InTokens[0];
    std::vector<std::string> Remaining(InTokens.begin() + 1, InTokens.end());
    ConsumeCommonOptions(Remaining, Options, true);

    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--doc" && Index + 1 < Remaining.size())
        {
            Options.mDocPath = Remaining[++Index];
        }
        else if (Token == "--topic" && Index + 1 < Remaining.size())
        {
            Options.mTopic = Remaining[++Index];
        }
        else if (Token == "--all")
        {
            Options.mbAll = true;
        }
        else if (Token == "--delete-source")
        {
            Options.mbDeleteSource = true;
        }
        else if (!Token.empty() && Token[0] != '-' && Options.mRepoRoot.empty())
        {
            Options.mRepoRoot = Token;
        }
    }

    return Options;
}

} // namespace UniPlan
