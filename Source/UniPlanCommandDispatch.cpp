#include "UniPlanCommandHelp.h"
#include "UniPlanRuntime.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#ifdef UPLAN_WATCH
#include "UniPlanWatchApp.h"
#endif
#include "UniPlanTypes.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Command dispatch table
//
// Replaces a 13-branch if/else chain on command name with a data-driven
// table. Each entry maps a command string to its handler function.
// Subcommand dispatchers are defined separately (job, task, lane,
// testing, manifest) and registered through the same table.
// ---------------------------------------------------------------------------

using FCommandHandler = int (*)(const std::vector<std::string> &,
                                const std::string &);

struct FCommandEntry
{
    const char *mName;
    FCommandHandler mpHandler;
};

template <size_t N>
static int DispatchSubcommand(const char *InGroupName,
                              const std::vector<std::string> &InArgs,
                              const std::string &InCwd,
                              const FCommandEntry (&InSubs)[N],
                              const char *InExpectedList)
{
    // 3-prologue --help handling (v0.85.0):
    //   (a) `uni-plan <group>`            → group help, exit 0
    //   (b) `uni-plan <group> --help`     → group help, exit 0
    //   (c) `uni-plan <group> <sub> --help|..|--help` → subcommand help, exit 0
    // Previously (a) threw UsageError (exit 2, stderr). The new behavior
    // keeps `--help` on a success path for AI agents that parse exit
    // codes. Unknown subcommands still throw UsageError below.
    if (InArgs.empty())
    {
        PrintCommandUsage(std::cout, InGroupName);
        return 0;
    }
    const std::string &Sub = InArgs[0];
    if (Sub == "--help" || Sub == "-h")
    {
        PrintCommandUsage(std::cout, InGroupName);
        return 0;
    }
    const std::vector<std::string> SubArgs(InArgs.begin() + 1, InArgs.end());
    if (ContainsHelpFlag(SubArgs))
    {
        PrintCommandUsage(std::cout, InGroupName, Sub);
        return 0;
    }
    for (size_t Index = 0; Index < N; ++Index)
    {
        if (Sub == InSubs[Index].mName)
        {
            return InSubs[Index].mpHandler(SubArgs, InCwd);
        }
    }
    (void)InExpectedList; // preserved for ABI, callers may still pass it
    throw UsageError("Unknown " + std::string(InGroupName) +
                     " subcommand: " + Sub);
}

static int DispatchJobCommand(const std::vector<std::string> &InArgs,
                              const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {{"add", &RunJobAddCommand},
                                          {"set", &RunJobSetCommand},
                                          {"remove", &RunJobRemoveCommand},
                                          {"list", &RunJobListCommand}};
    return DispatchSubcommand("job", InArgs, InCwd, kSubs,
                              "add, set, remove, list");
}

static int DispatchTaskCommand(const std::vector<std::string> &InArgs,
                               const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {{"add", &RunTaskAddCommand},
                                          {"set", &RunTaskSetCommand},
                                          {"remove", &RunTaskRemoveCommand},
                                          {"list", &RunTaskListCommand}};
    return DispatchSubcommand("task", InArgs, InCwd, kSubs,
                              "add, set, remove, list");
}

static int DispatchLaneCommand(const std::vector<std::string> &InArgs,
                               const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {{"add", &RunLaneAddCommand},
                                          {"set", &RunLaneSetCommand},
                                          {"complete", &RunLaneCompleteCommand},
                                          {"remove", &RunLaneRemoveCommand},
                                          {"list", &RunLaneListCommand}};
    return DispatchSubcommand("lane", InArgs, InCwd, kSubs,
                              "add, set, complete, remove, list");
}

static int DispatchTestingCommand(const std::vector<std::string> &InArgs,
                                  const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {{"add", &RunTestingAddCommand},
                                          {"set", &RunTestingSetCommand},
                                          {"remove", &RunTestingRemoveCommand},
                                          {"list", &RunTestingListCommand}};
    return DispatchSubcommand("testing", InArgs, InCwd, kSubs,
                              "add, set, remove, list");
}

static int DispatchManifestCommand(const std::vector<std::string> &InArgs,
                                   const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {
        {"add", &RunManifestAddCommand},
        {"set", &RunManifestSetCommand},
        {"remove", &RunManifestRemoveCommand},
        {"list", &RunManifestListCommand},
        {"suggest", &RunManifestSuggestCommand}};
    return DispatchSubcommand("manifest", InArgs, InCwd, kSubs,
                              "add, set, remove, list, suggest");
}

// v0.89.0 typed-array CLI groups. Each group exposes add/set/remove/list
// with the same shape as the existing `changelog` / `manifest` precedents
// above.
static int DispatchRiskCommand(const std::vector<std::string> &InArgs,
                               const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {{"add", &RunRiskAddCommand},
                                          {"set", &RunRiskSetCommand},
                                          {"remove", &RunRiskRemoveCommand},
                                          {"list", &RunRiskListCommand}};
    return DispatchSubcommand("risk", InArgs, InCwd, kSubs,
                              "add, set, remove, list");
}

static int DispatchNextActionCommand(const std::vector<std::string> &InArgs,
                                     const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {
        {"add", &RunNextActionAddCommand},
        {"set", &RunNextActionSetCommand},
        {"remove", &RunNextActionRemoveCommand},
        {"list", &RunNextActionListCommand}};
    return DispatchSubcommand("next-action", InArgs, InCwd, kSubs,
                              "add, set, remove, list");
}

static int
DispatchAcceptanceCriterionCommand(const std::vector<std::string> &InArgs,
                                   const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {
        {"add", &RunAcceptanceCriterionAddCommand},
        {"set", &RunAcceptanceCriterionSetCommand},
        {"remove", &RunAcceptanceCriterionRemoveCommand},
        {"list", &RunAcceptanceCriterionListCommand}};
    return DispatchSubcommand("acceptance-criterion", InArgs, InCwd, kSubs,
                              "add, set, remove, list");
}

// v0.98.0 typed-array CLI groups. Sidecar-replacement homes for priority
// groupings, runbooks, and residual risks.
static int
DispatchPriorityGroupingCommand(const std::vector<std::string> &InArgs,
                                const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {
        {"add", &RunPriorityGroupingAddCommand},
        {"set", &RunPriorityGroupingSetCommand},
        {"remove", &RunPriorityGroupingRemoveCommand},
        {"list", &RunPriorityGroupingListCommand}};
    return DispatchSubcommand("priority-grouping", InArgs, InCwd, kSubs,
                              "add, set, remove, list");
}

static int DispatchRunbookCommand(const std::vector<std::string> &InArgs,
                                  const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {{"add", &RunRunbookAddCommand},
                                          {"set", &RunRunbookSetCommand},
                                          {"remove", &RunRunbookRemoveCommand},
                                          {"list", &RunRunbookListCommand}};
    return DispatchSubcommand("runbook", InArgs, InCwd, kSubs,
                              "add, set, remove, list");
}

static int DispatchResidualRiskCommand(const std::vector<std::string> &InArgs,
                                       const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {
        {"add", &RunResidualRiskAddCommand},
        {"set", &RunResidualRiskSetCommand},
        {"remove", &RunResidualRiskRemoveCommand},
        {"list", &RunResidualRiskListCommand}};
    return DispatchSubcommand("residual-risk", InArgs, InCwd, kSubs,
                              "add, set, remove, list");
}

// ---------------------------------------------------------------------------
// Phase list all — collects phases from every plan in the inventory
// ---------------------------------------------------------------------------

void PrintUsage(std::ostream &Out)
{
    Out << "Usage:\n";
    Out << "  uni-plan --version\n";
    Out << "  uni-plan --help\n";
    Out << "\n";
    Out << "Query commands:\n";
    Out << "  uni-plan topic list [--status <filter>]\n";
    Out << "  uni-plan topic get --topic <T> [--sections <csv>]\n";
    Out << "  uni-plan topic status\n";
    Out << "  uni-plan phase list --topic <T> "
           "[--status <filter>]\n";
    Out << "  uni-plan phase get --topic <T> "
           "[--phase <N> | --phases <csv>] "
           "[--brief|--execution|--design]\n";
    Out << "  uni-plan phase next --topic <T>\n";
    Out << "  uni-plan phase readiness --topic <T> "
           "--phase <N>\n";
    Out << "  uni-plan phase wave-status --topic <T> "
           "--phase <N>\n";
    Out << "  uni-plan phase drift [--topic <T>]\n";
    Out << "  uni-plan changelog --topic <T> "
           "[--phase <N>]\n";
    Out << "  uni-plan verification --topic <T> "
           "[--phase <N>]\n";
    Out << "  uni-plan timeline --topic <T> "
           "[--since <date>]\n";
    Out << "  uni-plan blockers [--topic <T>]\n";
    Out << "  uni-plan validate [--topic <T>]\n";
    Out << "  uni-plan legacy-gap [--topic <T>] [--category <c>]\n";
    Out << "\n";
    Out << "Semantic lifecycle commands:\n";
    Out << "  uni-plan topic start --topic <T>\n";
    Out << "  uni-plan topic complete --topic <T> "
           "[--verification <text>]\n";
    Out << "  uni-plan topic block --topic <T> "
           "--reason <text>\n";
    Out << "  uni-plan phase start --topic <T> "
           "--phase <N> [--context <text>]\n";
    Out << "  uni-plan phase complete --topic <T> "
           "--phase <N> --done <text> "
           "[--verification <text>]\n";
    Out << "  uni-plan phase block --topic <T> "
           "--phase <N> --reason <text>\n";
    Out << "  uni-plan phase unblock --topic <T> "
           "--phase <N>\n";
    Out << "  uni-plan phase progress --topic <T> "
           "--phase <N> --done <text> "
           "--remaining <text>\n";
    Out << "  uni-plan phase complete-jobs --topic <T> "
           "--phase <N>\n";
    Out << "  uni-plan phase log --topic <T> "
           "--phase <N> --change <text> "
           "[--type <type>]\n";
    Out << "  uni-plan phase verify --topic <T> "
           "--phase <N> --check <text> "
           "[--result <text>]\n";
    Out << "\n";
    Out << "Raw mutation commands:\n";
    Out << "  uni-plan topic add --topic <T> --title <text> "
           "[--summary <t>] [--goals <t>] ...\n";
    Out << "  uni-plan topic set --topic <T> "
           "[--status <s>] [--summary <t>] "
           "[--goals <t>] ...\n";
    Out << "  uni-plan phase set --topic <T> "
           "--phase <N> [--status <s>] "
           "[--scope <t>] [--investigation <t>] ...\n";
    Out << "  uni-plan phase add --topic <T> "
           "[--scope <t>] [--output <t>] [--status <s>]\n";
    Out << "  uni-plan phase remove --topic <T> --phase <N>\n";
    Out << "  uni-plan phase normalize --topic <T> "
           "--phase <N> [--dry-run]\n";
    Out << "  uni-plan topic normalize --topic <T> [--dry-run]\n";
    Out << "  uni-plan job add|set|remove|list --topic <T> "
           "--phase <N> [--job <N>] ...\n";
    Out << "  uni-plan task add|set|remove|list --topic <T> "
           "--phase <N> --job <N> [--task <N>] ...\n";
    Out << "  uni-plan changelog add --topic <T> "
           "--change <text> [--phase <N>] "
           "[--type <type>]\n";
    Out << "  uni-plan changelog set --topic <T> "
           "--index <N> [--phase <N|topic>] "
           "[--date <d>] [--change <t>] [--type <t>]\n";
    Out << "  uni-plan changelog remove --topic <T> --index <N>\n";
    Out << "  uni-plan verification add --topic <T> "
           "--check <text> [--phase <N>] "
           "[--result <text>]\n";
    Out << "  uni-plan verification set --topic <T> "
           "--index <N> [--check <t>] [--result <t>] "
           "[--detail <t>]\n";
    Out << "  uni-plan lane add|set|remove|list --topic <T> "
           "--phase <N> [--lane <N>] ...\n";
    Out << "  uni-plan testing add|set|remove|list --topic <T> "
           "--phase <N> [--index <N>] ...\n";
    Out << "  uni-plan manifest add --topic <T> "
           "--phase <N> --file <path> "
           "--action <a> --description <text>\n";
    Out << "  uni-plan manifest remove --topic <T> "
           "--phase <N> --index <N>\n";
    Out << "  uni-plan manifest list "
           "[--topic <T>] [--phase <N>] [--missing-only]\n";
    Out << "  uni-plan manifest set --topic <T> "
           "--phase <N> --index <N> [--file <t>] "
           "[--action <t>] [--description <t>]\n";
    Out << "  uni-plan risk add|set|remove|list --topic <T> "
           "[--statement <t>] [--severity <s>] "
           "[--status <s>] [--index <N>]\n";
    Out << "  uni-plan next-action add|set|remove|list --topic <T> "
           "[--statement <t>] [--order <N>] "
           "[--status <s>] [--index <N>]\n";
    Out << "  uni-plan acceptance-criterion "
           "add|set|remove|list --topic <T> "
           "[--statement <t>] [--status <s>] [--index <N>]\n";
    Out << "  uni-plan priority-grouping "
           "add|set|remove|list --topic <T> "
           "[--id <t>] [--phase-indices <csv>] [--rule <t>] [--index <N>]\n";
    Out << "  uni-plan runbook "
           "add|set|remove|list --topic <T> "
           "[--name <t>] [--trigger <t>] [--command <t> ...] "
           "[--description <t>] [--index <N>]\n";
    Out << "  uni-plan residual-risk "
           "add|set|remove|list --topic <T> "
           "[--area <t>] [--observation <t>] [--why-deferred <t>] "
           "[--target-phase <t>] [--index <N>]\n";
    Out << "  uni-plan graph [--topic <T>] [--depth <N>]\n";
    Out << "\n";
    Out << "Utility:\n";
    Out << "  uni-plan cache [info|clear|config]\n";
    Out << "  uni-plan watch [--repo-root <path>]\n";
    Out << "\n";
    Out << "Output is JSON by default.\n";
    Out << "Global options:\n";
    Out << "  --human       Formatted ANSI tables\n";
    Out << "  --repo-root   Override repository root\n";
}

// kCommandHelp registry, kFileFlagFooter, and PrintCommandUsage moved
// to UniPlanCommandHelp.cpp (v0.85.0). The hierarchical help registry
// + subcommand-aware PrintCommandUsage live there so every dispatcher
// can delegate `--help` through a single include.

int RunMain(const int InArgc, char *InArgv[])
{
    std::vector<std::string> Tokens;
    for (int Index = 1; Index < InArgc; ++Index)
    {
        Tokens.emplace_back(InArgv[Index]);
    }

    bool UseCache = true;
    std::vector<std::string> FilteredTokens;
    FilteredTokens.reserve(Tokens.size());
    for (const std::string &Token : Tokens)
    {
        if (Token == "--no-cache")
        {
            UseCache = false;
            continue;
        }
        FilteredTokens.push_back(Token);
    }
    Tokens = std::move(FilteredTokens);

    const DocConfig Config = LoadConfig(ResolveExecutableDirectory());

    if (!Config.mbCacheEnabled)
    {
        UseCache = false;
    }

    if (Tokens.empty())
    {
        PrintUsage(std::cout);
        return 0;
    }

    if (Tokens.size() == 1 && (Tokens[0] == "--help" || Tokens[0] == "-h"))
    {
        PrintUsage(std::cout);
        return 0;
    }

    if (Tokens.size() == 1 && Tokens[0] == "--version")
    {
        std::cout << "uni-plan " << kCliVersion << "\n";
        return 0;
    }

    const std::string Command = Tokens[0];
    try
    {
        // V4 bundle-native commands
        const std::string CWD = fs::current_path().string();
        const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());

        static const FCommandEntry kCommands[] = {
            {"topic", &RunTopicCommand},
            {"phase", &RunBundlePhaseCommand},
            {"changelog", &RunBundleChangelogCommand},
            {"verification", &RunBundleVerificationCommand},
            {"timeline", &RunBundleTimelineCommand},
            {"blockers", &RunBundleBlockersCommand},
            {"validate", &RunBundleValidateCommand},
            {"legacy-gap", &RunLegacyGapCommand},
            {"job", &DispatchJobCommand},
            {"task", &DispatchTaskCommand},
            {"lane", &DispatchLaneCommand},
            {"testing", &DispatchTestingCommand},
            {"manifest", &DispatchManifestCommand},
            {"risk", &DispatchRiskCommand},
            {"next-action", &DispatchNextActionCommand},
            {"acceptance-criterion", &DispatchAcceptanceCriterionCommand},
            // v0.98.0 typed-array CLI groups + graph read command.
            {"priority-grouping", &DispatchPriorityGroupingCommand},
            {"runbook", &DispatchRunbookCommand},
            {"residual-risk", &DispatchResidualRiskCommand},
            {"graph", &RunGraphCommand},
            {"migrate", &RunMigrateCommand},
            {"_catalog", &RunCatalogCommand},
        };

        for (const FCommandEntry &Entry : kCommands)
        {
            if (Command == Entry.mName)
            {
                return Entry.mpHandler(Args, CWD);
            }
        }

        if (Command == "cache")
        {
            // v0.85.0: cache's group-level --help is handled by the
            // standard kCommandHelp path (cache entry lives in
            // UniPlanCommandHelp.cpp). Below we also honor subcommand-
            // level --help (cache info/clear/config --help) via the
            // FSubcommandHelpEntry lookup.
            if (Tokens.size() >= 2 &&
                (Tokens[1] == "--help" || Tokens[1] == "-h"))
            {
                PrintCommandUsage(std::cout, "cache");
                return 0;
            }

            // Default subcommand: info (bare "uni-plan cache" → "uni-plan cache
            // info")
            std::string Subcommand = "info";
            size_t ArgsStart = 1;
            if (Tokens.size() >= 2)
            {
                const std::string Candidate = ToLower(Tokens[1]);
                if (Candidate == "info" || Candidate == "clear" ||
                    Candidate == "config")
                {
                    Subcommand = Candidate;
                    ArgsStart = 2;
                }
            }

            // Subcommand --help (cache info/clear/config --help).
            {
                const std::vector<std::string> HelpProbe(
                    Tokens.begin() + static_cast<std::ptrdiff_t>(ArgsStart),
                    Tokens.end());
                if (ContainsHelpFlag(HelpProbe))
                {
                    PrintCommandUsage(std::cout, "cache", Subcommand);
                    return 0;
                }
            }

            const std::vector<std::string> Args(
                Tokens.begin() + static_cast<std::ptrdiff_t>(ArgsStart),
                Tokens.end());

            if (Subcommand == "info")
            {
                const CacheInfoOptions Options = ParseCacheInfoOptions(Args);
                const fs::path RepoRoot =
                    NormalizeRepoRootPath(Options.mRepoRoot);
                const CacheInfoResult Result =
                    BuildCacheInfo(Options.mRepoRoot, Config);
                if (Options.mbHuman)
                {
                    return RunCacheInfoHuman(Result);
                }
                if (!Options.mbJson)
                {
                    return RunCacheInfoText(Result);
                }
                return RunCacheInfoJson(ToGenericPath(RepoRoot), Result);
            }

            if (Subcommand == "clear")
            {
                const CacheClearOptions Options = ParseCacheClearOptions(Args);
                const fs::path RepoRoot =
                    NormalizeRepoRootPath(Options.mRepoRoot);
                const CacheClearResult Result =
                    ClearCache(Options.mRepoRoot, Config);
                if (Options.mbHuman)
                {
                    return RunCacheClearHuman(Result);
                }
                if (!Options.mbJson)
                {
                    return RunCacheClearText(Result);
                }
                return RunCacheClearJson(ToGenericPath(RepoRoot), Result);
            }

            if (Subcommand == "config")
            {
                const CacheConfigOptions Options =
                    ParseCacheConfigOptions(Args);
                const fs::path RepoRoot =
                    NormalizeRepoRootPath(Options.mRepoRoot);
                const CacheConfigResult Result =
                    WriteCacheConfig(Options.mRepoRoot, Options, Config);
                if (Options.mbHuman)
                {
                    return RunCacheConfigHuman(Result);
                }
                if (!Options.mbJson)
                {
                    return RunCacheConfigText(Result);
                }
                return RunCacheConfigJson(ToGenericPath(RepoRoot), Result);
            }

            throw UsageError("Unknown cache subcommand: " + Subcommand);
        }

        // bundle command removed — .md→.json migration is
        // complete. .Plan.json files are the sole source of truth.

#ifdef UPLAN_WATCH
        if (Command == "watch")
        {
            const std::vector<std::string> Args(Tokens.begin() + 1,
                                                Tokens.end());
            if (ContainsHelpFlag(Args))
            {
                PrintCommandUsage(std::cout, "watch");
                return 0;
            }
            BaseOptions WatchOptions;
            ConsumeCommonOptions(Args, WatchOptions, true);
            const std::string WatchRoot =
                WatchOptions.mRepoRoot.empty() ? "." : WatchOptions.mRepoRoot;
            return RunDocWatch(WatchRoot, Config);
        }
#endif

        throw UsageError("Unknown command: " + Command);
    }
    catch (const UsageError &InError)
    {
        std::cerr << kColorRed << "error: " << InError.what() << kColorReset
                  << "\n\n";
        std::cerr << kColorDim;
        PrintCommandUsage(std::cerr, Command);
        std::cerr << kColorReset;
        return 2;
    }
}

} // namespace UniPlan
