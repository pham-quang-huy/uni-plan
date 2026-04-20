#include "UniPlanFileHelpers.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanStatusHelpers.h"
#include "UniPlanStringHelpers.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <set>
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

// TryConsumeStringOrFileOption — unified handler for the paired
// `--<field>` (literal string value) and `--<field>-file <path>` (read
// file contents raw, no shell expansion) option shape used across every
// prose-setting mutation command.
//
// Returns true iff the token at InOutIndex matched either flag; on match
// the value is stored into OutValue and the caller should `continue`.
// Returns false when neither flag matched (caller moves on).
//
// The `-file` branch goes through TryReadFileToString, which slurps the
// whole file verbatim — no trimming, no line processing, no shell
// expansion. This is the single purpose of this helper: eliminate the
// `--<field> "$(cat file)"` silent-corruption hazard for prose fields
// that legitimately contain `$`, backticks, or double quotes.
//
// Empty-string semantics are preserved: a literal `--<field> ""` still
// reads as "no change" downstream (the mutation commands' long-standing
// convention). An empty file read via `--<field>-file empty.txt` also
// yields an empty string, matching the literal-empty behavior.
inline bool
TryConsumeStringOrFileOption(const std::vector<std::string> &InTokens,
                             size_t &InOutIndex, const char *InStringFlag,
                             const char *InFileFlag, std::string &OutValue)
{
    const std::string &Token = InTokens[InOutIndex];
    if (Token == InStringFlag)
    {
        OutValue = ConsumeValuedOption(InTokens, InOutIndex, InStringFlag);
        return true;
    }
    if (Token == InFileFlag)
    {
        const std::string Path =
            ConsumeValuedOption(InTokens, InOutIndex, InFileFlag);
        std::string Error;
        if (!TryReadFileToString(fs::path(Path), OutValue, Error))
        {
            throw UsageError(std::string(InFileFlag) + " '" + Path +
                             "': " + Error);
        }
        return true;
    }
    return false;
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

// Canonical `topic get` section names (v0.84.0). Matches the fields
// emitted by RunTopicGetJson/Human. Identity fields (topic/status/title,
// phase_count, schema envelope) are always emitted — they're not
// sectionable. Unknown names throw UsageError at parse time.
static bool IsValidTopicSection(const std::string &InName)
{
    static const char *const kValid[] = {
        "summary",           "goals",          "non_goals",
        "risks",             "acceptance_criteria",
        "problem_statement", "validation_commands",
        "baseline_audit",    "execution_strategy",
        "locked_decisions",  "source_references",
        "dependencies",      "next_actions",   "phases"};
    for (const char *V : kValid)
    {
        if (InName == V)
            return true;
    }
    return false;
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
        if (Token == "--sections")
        {
            const std::string Val =
                ConsumeValuedOption(Remaining, Index, "--sections");
            // Split CSV, trim each, validate against the canonical list.
            std::string Cur;
            const auto Emit = [&]()
            {
                const std::string Name = Trim(Cur);
                Cur.clear();
                if (Name.empty())
                    throw UsageError("--sections has an empty field");
                if (!IsValidTopicSection(Name))
                    throw UsageError("--sections unknown section: " + Name);
                Options.mSections.push_back(Name);
            };
            for (const char C : Val)
            {
                if (C == ',')
                    Emit();
                else
                    Cur.push_back(C);
            }
            Emit();
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
        if (Token == "--phases")
        {
            // Comma-separated batch mode (v0.84.0). Mutually exclusive
            // with --phase <N>; enforced after the loop so callers get a
            // clear "both flags set" error instead of a silent priority.
            const std::string Val =
                ConsumeValuedOption(Remaining, Index, "--phases");
            try
            {
                Options.mPhaseIndices = SplitCsvInts(Val);
            }
            catch (const std::invalid_argument &InError)
            {
                throw UsageError(
                    "--phases requires a comma-separated list of non-negative "
                    "integers (e.g. 1,3,5): " +
                    std::string(InError.what()));
            }
            // Dedupe + sort so callers can assume stable ordering in the
            // response. Preserves caller-visible semantics without forcing
            // input ordering on authors.
            std::sort(Options.mPhaseIndices.begin(),
                      Options.mPhaseIndices.end());
            Options.mPhaseIndices.erase(
                std::unique(Options.mPhaseIndices.begin(),
                            Options.mPhaseIndices.end()),
                Options.mPhaseIndices.end());
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
        if (Token == "--design")
        {
            Options.mbDesign = true;
            continue;
        }
        throw UsageError("Unknown option for phase get: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase get requires --topic <topic>");
    // Enforce mutual exclusion and presence: exactly one of --phase /
    // --phases must be provided.
    const bool bSingle = (Options.mPhaseIndex >= 0);
    const bool bBatch = !Options.mPhaseIndices.empty();
    if (bSingle && bBatch)
        throw UsageError("phase get: --phase and --phases are mutually "
                         "exclusive; pick one");
    if (!bSingle && !bBatch)
        throw UsageError("phase get requires --phase <index> or "
                         "--phases <csv>");
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
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--status");
            ETopicStatus Value;
            if (!TopicStatusFromString(Raw, Value))
            {
                throw UsageError(
                    "Invalid topic status '" + Raw +
                    "' (expected: not_started, in_progress, completed, "
                    "blocked, canceled)");
            }
            Options.opStatus = Value;
            continue;
        }
        if (TryConsumeStringOrFileOption(Remaining, Index, "--next-actions",
                                         "--next-actions-file",
                                         Options.mNextActions))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--summary",
                                         "--summary-file", Options.mSummary))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--goals",
                                         "--goals-file", Options.mGoals))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--non-goals",
                                         "--non-goals-file", Options.mNonGoals))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--risks",
                                         "--risks-file", Options.mRisks))
            continue;
        if (TryConsumeStringOrFileOption(
                Remaining, Index, "--acceptance-criteria",
                "--acceptance-criteria-file", Options.mAcceptanceCriteria))
            continue;
        if (TryConsumeStringOrFileOption(
                Remaining, Index, "--problem-statement",
                "--problem-statement-file", Options.mProblemStatement))
            continue;
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
        if (TryConsumeStringOrFileOption(Remaining, Index, "--baseline-audit",
                                         "--baseline-audit-file",
                                         Options.mBaselineAudit))
            continue;
        if (TryConsumeStringOrFileOption(
                Remaining, Index, "--execution-strategy",
                "--execution-strategy-file", Options.mExecutionStrategy))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--locked-decisions",
                                         "--locked-decisions-file",
                                         Options.mLockedDecisions))
            continue;
        if (TryConsumeStringOrFileOption(
                Remaining, Index, "--source-references",
                "--source-references-file", Options.mSourceReferences))
            continue;
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
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--status");
            EExecutionStatus Value;
            if (!ExecutionStatusFromString(Raw, Value))
            {
                throw UsageError(
                    "Invalid phase status '" + Raw +
                    "' (expected: not_started, in_progress, completed, "
                    "blocked)");
            }
            Options.opStatus = Value;
            continue;
        }
        if (TryConsumeStringOrFileOption(Remaining, Index, "--done",
                                         "--done-file", Options.mDone))
            continue;
        if (Token == "--done-clear")
        {
            Options.mbDoneClear = true;
            continue;
        }
        if (Token == "--remaining-clear")
        {
            Options.mbRemainingClear = true;
            continue;
        }
        if (Token == "--blockers-clear")
        {
            Options.mbBlockersClear = true;
            continue;
        }
        if (Token == "--started-at")
        {
            Options.mStartedAt =
                ConsumeValuedOption(Remaining, Index, "--started-at");
            // Validate at parse time so the usage error surfaces
            // alongside the other --status / --phase errors, not after
            // the bundle has been loaded.
            if (!Options.mStartedAt.empty() &&
                !IsValidISOTimestampValue(Options.mStartedAt))
            {
                throw UsageError(
                    "Invalid --started-at value: expected ISO timestamp "
                    "(YYYY-MM-DD or YYYY-MM-DDThh:mm:ssZ), got '" +
                    Options.mStartedAt + "'");
            }
            continue;
        }
        if (Token == "--completed-at")
        {
            Options.mCompletedAt =
                ConsumeValuedOption(Remaining, Index, "--completed-at");
            if (!Options.mCompletedAt.empty() &&
                !IsValidISOTimestampValue(Options.mCompletedAt))
            {
                throw UsageError(
                    "Invalid --completed-at value: expected ISO timestamp "
                    "(YYYY-MM-DD or YYYY-MM-DDThh:mm:ssZ), got '" +
                    Options.mCompletedAt + "'");
            }
            continue;
        }
        if (Token == "--origin")
        {
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--origin");
            EPhaseOrigin Parsed = EPhaseOrigin::NativeV4;
            if (!PhaseOriginFromString(Raw, Parsed))
            {
                throw UsageError("Invalid --origin value: '" + Raw +
                                 "', expected native_v4 or v3_migration");
            }
            // Empty string maps to NativeV4 inside PhaseOriginFromString,
            // but empty `--origin ""` should mean "no change" the same way
            // other prose flags do. Gate on Raw so the caller can
            // distinguish "unset" from an explicit native_v4 stamp.
            if (!Raw.empty())
            {
                Options.opOrigin = Parsed;
            }
            continue;
        }
        // v0.86.0: explicit-no-manifest opt-out. The two flags are
        // independent at parse time but the schema invariant
        // (no_file_manifest=true ⇒ non-empty reason) is enforced both
        // by the JSON deserializer and by the mutation handler before
        // the bundle is written, so a malformed combination always
        // produces a parse-time / write-time error rather than a
        // silently-corrupt bundle.
        if (Token == "--no-file-manifest")
        {
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--no-file-manifest");
            if (Raw == "true")
                Options.opNoFileManifest = true;
            else if (Raw == "false")
                Options.opNoFileManifest = false;
            else
                throw UsageError(
                    "Invalid --no-file-manifest value: '" + Raw +
                    "', expected 'true' or 'false'");
            continue;
        }
        if (TryConsumeStringOrFileOption(
                Remaining, Index, "--no-file-manifest-reason",
                "--no-file-manifest-reason-file",
                Options.mFileManifestSkipReason))
            continue;
        if (Token == "--no-file-manifest-reason-clear")
        {
            Options.mbFileManifestSkipReasonClear = true;
            continue;
        }
        if (TryConsumeStringOrFileOption(Remaining, Index, "--remaining",
                                         "--remaining-file",
                                         Options.mRemaining))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--blockers",
                                         "--blockers-file", Options.mBlockers))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--context",
                                         "--context-file", Options.mContext))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--scope",
                                         "--scope-file", Options.mScope))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--output",
                                         "--output-file", Options.mOutput))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--investigation",
                                         "--investigation-file",
                                         Options.mInvestigation))
            continue;
        if (TryConsumeStringOrFileOption(
                Remaining, Index, "--code-entity-contract",
                "--code-entity-contract-file", Options.mCodeEntityContract))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--code-snippets",
                                         "--code-snippets-file",
                                         Options.mCodeSnippets))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--best-practices",
                                         "--best-practices-file",
                                         Options.mBestPractices))
            continue;
        if (TryConsumeStringOrFileOption(
                Remaining, Index, "--multi-platforming",
                "--multi-platforming-file", Options.mMultiPlatforming))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--readiness-gate",
                                         "--readiness-gate-file",
                                         Options.mReadinessGate))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--handoff",
                                         "--handoff-file", Options.mHandoff))
            continue;
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
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--status");
            EExecutionStatus Value;
            if (!ExecutionStatusFromString(Raw, Value))
            {
                throw UsageError(
                    "Invalid job status '" + Raw +
                    "' (expected: not_started, in_progress, completed, "
                    "blocked)");
            }
            Options.opStatus = Value;
            continue;
        }
        if (TryConsumeStringOrFileOption(Remaining, Index, "--scope",
                                         "--scope-file", Options.mScope))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--output",
                                         "--output-file", Options.mOutput))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--exit-criteria",
                                         "--exit-criteria-file",
                                         Options.mExitCriteria))
            continue;
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
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--status");
            EExecutionStatus Value;
            if (!ExecutionStatusFromString(Raw, Value))
            {
                throw UsageError(
                    "Invalid task status '" + Raw +
                    "' (expected: not_started, in_progress, completed, "
                    "blocked)");
            }
            Options.opStatus = Value;
            continue;
        }
        if (TryConsumeStringOrFileOption(Remaining, Index, "--evidence",
                                         "--evidence-file", Options.mEvidence))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--notes",
                                         "--notes-file", Options.mNotes))
            continue;
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
        if (TryConsumeStringOrFileOption(Remaining, Index, "--change",
                                         "--change-file", Options.mChange))
            continue;
        if (Token == "--type")
        {
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--type");
            EChangeType Value;
            if (!ChangeTypeFromString(Raw, Value))
            {
                throw UsageError("Invalid changelog type '" + Raw +
                                 "' (expected: feat, fix, refactor, chore)");
            }
            Options.mType = Value;
            continue;
        }
        if (TryConsumeStringOrFileOption(Remaining, Index, "--affected",
                                         "--affected-file", Options.mAffected))
            continue;
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
        if (TryConsumeStringOrFileOption(Remaining, Index, "--check",
                                         "--check-file", Options.mCheck))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--result",
                                         "--result-file", Options.mResult))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--detail",
                                         "--detail-file", Options.mDetail))
            continue;
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
        if (TryConsumeStringOrFileOption(Remaining, Index, "--context",
                                         "--context-file", Options.mContext))
            continue;
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
        if (TryConsumeStringOrFileOption(Remaining, Index, "--done",
                                         "--done-file", Options.mDone))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--verification",
                                         "--verification-file",
                                         Options.mVerification))
            continue;
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
        if (TryConsumeStringOrFileOption(Remaining, Index, "--reason",
                                         "--reason-file", Options.mReason))
            continue;
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

FPhaseCancelOptions
ParsePhaseCancelOptions(const std::vector<std::string> &InTokens)
{
    FPhaseCancelOptions Options;
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
        if (TryConsumeStringOrFileOption(Remaining, Index, "--reason",
                                         "--reason-file", Options.mReason))
            continue;
        throw UsageError("Unknown option for phase cancel: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase cancel requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("phase cancel requires --phase");
    if (Options.mReason.empty())
        throw UsageError("phase cancel requires --reason");
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
        if (TryConsumeStringOrFileOption(Remaining, Index, "--done",
                                         "--done-file", Options.mDone))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--remaining",
                                         "--remaining-file",
                                         Options.mRemaining))
            continue;
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
        if (TryConsumeStringOrFileOption(Remaining, Index, "--verification",
                                         "--verification-file",
                                         Options.mVerification))
            continue;
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
        if (TryConsumeStringOrFileOption(Remaining, Index, "--reason",
                                         "--reason-file", Options.mReason))
            continue;
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
            if (TryConsumeStringOrFileOption(Remaining, Index, "--change",
                                             "--change-file", Options.mChange))
                continue;
        if (Token == "--type")
        {
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--type");
            EChangeType Value;
            if (!ChangeTypeFromString(Raw, Value))
            {
                throw UsageError("Invalid changelog type '" + Raw +
                                 "' (expected: feat, fix, refactor, chore)");
            }
            Options.mType = Value;
            continue;
        }
        if (TryConsumeStringOrFileOption(Remaining, Index, "--affected",
                                         "--affected-file", Options.mAffected))
            continue;
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
        if (TryConsumeStringOrFileOption(Remaining, Index, "--check",
                                         "--check-file", Options.mCheck))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--result",
                                         "--result-file", Options.mResult))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--detail",
                                         "--detail-file", Options.mDetail))
            continue;
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
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--status");
            EExecutionStatus Value;
            if (!ExecutionStatusFromString(Raw, Value))
            {
                throw UsageError(
                    "Invalid lane status '" + Raw +
                    "' (expected: not_started, in_progress, completed, "
                    "blocked)");
            }
            Options.opStatus = Value;
            continue;
        }
        if (TryConsumeStringOrFileOption(Remaining, Index, "--scope",
                                         "--scope-file", Options.mScope))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--exit-criteria",
                                         "--exit-criteria-file",
                                         Options.mExitCriteria))
            continue;
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
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--actor");
            ETestingActor Value;
            if (!TestingActorFromString(Raw, Value))
            {
                throw UsageError("Invalid actor '" + Raw +
                                 "' (expected: human, ai, automated)");
            }
            Options.opActor = Value;
            continue;
        }
        if (TryConsumeStringOrFileOption(Remaining, Index, "--step",
                                         "--step-file", Options.mStep))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--action",
                                         "--action-file", Options.mAction))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--expected",
                                         "--expected-file", Options.mExpected))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--evidence",
                                         "--evidence-file", Options.mEvidence))
            continue;
        throw UsageError("Unknown option for testing add: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("testing add requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("testing add requires --phase");
    if (Options.mSession.empty())
        throw UsageError("testing add requires --session");
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
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--action");
            EFileAction Value;
            if (!FileActionFromString(Raw, Value))
            {
                throw UsageError("Invalid file action '" + Raw +
                                 "' (expected: create, modify, delete)");
            }
            Options.opAction = Value;
            continue;
        }
        if (TryConsumeStringOrFileOption(Remaining, Index, "--description",
                                         "--description-file",
                                         Options.mDescription))
            continue;
        throw UsageError("Unknown option for manifest add: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("manifest add requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("manifest add requires --phase");
    if (Options.mFile.empty())
        throw UsageError("manifest add requires --file");
    if (!Options.opAction.has_value())
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
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--actor");
            ETestingActor Value;
            if (!TestingActorFromString(Raw, Value))
            {
                throw UsageError("Invalid actor '" + Raw +
                                 "' (expected: human, ai, automated)");
            }
            Options.opActor = Value;
            continue;
        }
        if (TryConsumeStringOrFileOption(Remaining, Index, "--step",
                                         "--step-file", Options.mStep))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--action",
                                         "--action-file", Options.mAction))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--expected",
                                         "--expected-file", Options.mExpected))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--evidence",
                                         "--evidence-file", Options.mEvidence))
            continue;
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
        if (TryConsumeStringOrFileOption(Remaining, Index, "--check",
                                         "--check-file", Options.mCheck))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--result",
                                         "--result-file", Options.mResult))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--detail",
                                         "--detail-file", Options.mDetail))
            continue;
        throw UsageError("Unknown option for verification set: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("verification set requires --topic");
    if (Options.mIndex < 0)
        throw UsageError("verification set requires --index");
    return Options;
}

FManifestListOptions
ParseManifestListOptions(const std::vector<std::string> &InTokens)
{
    FManifestListOptions Options;
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
        if (Token == "--missing-only")
        {
            Options.mbMissingOnly = true;
            continue;
        }
        if (Token == "--stale-plan")
        {
            Options.mbStalePlan = true;
            continue;
        }
        throw UsageError("Unknown option for manifest list: " + Token);
    }
    // All arguments optional — no post-validation.
    return Options;
}

// manifest suggest — backfill helper that scans git history for the
// phase's started_at..completed_at window and proposes file_manifest
// entries. Defaults to dry-run; --apply actually invokes manifest add
// for each suggestion. Added v0.86.0.
FManifestSuggestOptions
ParseManifestSuggestOptions(const std::vector<std::string> &InTokens)
{
    FManifestSuggestOptions Options;
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
        if (Token == "--apply")
        {
            Options.mbApply = true;
            continue;
        }
        throw UsageError("Unknown option for manifest suggest: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("manifest suggest requires --topic <topic>");
    if (Options.mPhaseIndex < 0)
        throw UsageError("manifest suggest requires --phase <index>");
    return Options;
}

// phase drift — report phases where declared status lags the evidence
// stored elsewhere in the bundle. --topic is optional; omit to scan all
// topics. Added v0.84.0.
FPhaseDriftOptions
ParsePhaseDriftOptions(const std::vector<std::string> &InTokens)
{
    FPhaseDriftOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            ParseRequiredTopic(Remaining, Index, Options.mTopic);
            continue;
        }
        throw UsageError("Unknown option for phase drift: " + Token);
    }
    return Options;
}

FManifestRemoveOptions
ParseManifestRemoveOptions(const std::vector<std::string> &InTokens)
{
    FManifestRemoveOptions Options;
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
        throw UsageError("Unknown option for manifest remove: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("manifest remove requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("manifest remove requires --phase");
    if (Options.mIndex < 0)
        throw UsageError("manifest remove requires --index");
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
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--action");
            EFileAction Value;
            if (!FileActionFromString(Raw, Value))
            {
                throw UsageError("Invalid file action '" + Raw +
                                 "' (expected: create, modify, delete)");
            }
            Options.opAction = Value;
            continue;
        }
        if (TryConsumeStringOrFileOption(Remaining, Index, "--description",
                                         "--description-file",
                                         Options.mDescription))
            continue;
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
        if (TryConsumeStringOrFileOption(Remaining, Index, "--change",
                                         "--change-file", Options.mChange))
            continue;
        if (Token == "--type")
        {
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--type");
            EChangeType Value;
            if (!ChangeTypeFromString(Raw, Value))
            {
                throw UsageError("Invalid changelog type '" + Raw +
                                 "' (expected: feat, fix, refactor, chore)");
            }
            Options.opType = Value;
            continue;
        }
        if (TryConsumeStringOrFileOption(Remaining, Index, "--affected",
                                         "--affected-file", Options.mAffected))
            continue;
        throw UsageError("Unknown option for changelog set: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("changelog set requires --topic");
    if (Options.mIndex < 0)
        throw UsageError("changelog set requires --index");
    return Options;
}

FChangelogRemoveOptions
ParseChangelogRemoveOptions(const std::vector<std::string> &InTokens)
{
    FChangelogRemoveOptions Options;
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
        throw UsageError("Unknown option for changelog remove: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("changelog remove requires --topic");
    if (Options.mIndex < 0)
        throw UsageError("changelog remove requires --index");
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
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--status");
            EExecutionStatus Value;
            if (!ExecutionStatusFromString(Raw, Value))
            {
                throw UsageError(
                    "Invalid lane status '" + Raw +
                    "' (expected: not_started, in_progress, completed, "
                    "blocked)");
            }
            Options.opStatus = Value;
            continue;
        }
        if (TryConsumeStringOrFileOption(Remaining, Index, "--scope",
                                         "--scope-file", Options.mScope))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--exit-criteria",
                                         "--exit-criteria-file",
                                         Options.mExitCriteria))
            continue;
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
        if (TryConsumeStringOrFileOption(Remaining, Index, "--scope",
                                         "--scope-file", Options.mScope))
            continue;
        if (TryConsumeStringOrFileOption(Remaining, Index, "--output",
                                         "--output-file", Options.mOutput))
            continue;
        if (Token == "--status")
        {
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--status");
            EExecutionStatus Value;
            if (!ExecutionStatusFromString(Raw, Value))
            {
                throw UsageError(
                    "Invalid phase status '" + Raw +
                    "' (expected: not_started, in_progress, completed, "
                    "blocked)");
            }
            Options.opStatus = Value;
            continue;
        }
        throw UsageError("Unknown option for phase add: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase add requires --topic");
    return Options;
}

FPhaseNormalizeOptions
ParsePhaseNormalizeOptions(const std::vector<std::string> &InTokens)
{
    FPhaseNormalizeOptions Options;
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
            Options.mPhaseIndex =
                std::stoi(ConsumeValuedOption(Remaining, Index, "--phase"));
            continue;
        }
        if (Token == "--dry-run")
        {
            Options.mbDryRun = true;
            continue;
        }
        throw UsageError("Unknown option for phase normalize: " + Token);
    }
    if (Options.mTopic.empty())
        throw UsageError("phase normalize requires --topic");
    if (Options.mPhaseIndex < 0)
        throw UsageError("phase normalize requires --phase");
    return Options;
}

// ---------------------------------------------------------------------------
// Legacy-gap option parser (stateless V3 <-> V4 parity audit)
// ---------------------------------------------------------------------------

FLegacyGapOptions
ParseLegacyGapOptions(const std::vector<std::string> &InTokens)
{
    FLegacyGapOptions Options;
    const auto Remaining = ConsumeCommonOptions(InTokens, Options);
    for (size_t Index = 0; Index < Remaining.size(); ++Index)
    {
        const std::string &Token = Remaining[Index];
        if (Token == "--topic")
        {
            Options.mTopic = ConsumeValuedOption(Remaining, Index, "--topic");
            continue;
        }
        if (Token == "--category")
        {
            const std::string Raw =
                ConsumeValuedOption(Remaining, Index, "--category");
            EPhaseGapCategory Cat = EPhaseGapCategory::LegacyAbsent;
            if (!PhaseGapCategoryFromString(Raw, Cat))
            {
                throw UsageError(
                    "legacy-gap --category: invalid value '" + Raw +
                    "', expected one of: "
                    "legacy_rich|legacy_rich_matched|legacy_thin|legacy_stub|"
                    "legacy_absent|v4_only|hollow_both|drift");
            }
            Options.opCategory = Cat;
            continue;
        }
        throw UsageError("Unknown option for legacy-gap: " + Token);
    }
    return Options;
}

} // namespace UniPlan
