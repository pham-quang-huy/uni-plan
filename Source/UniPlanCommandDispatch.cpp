#include "UniPlanRuntime.h"
#include "UniPlanDocumentStore.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanJsonIO.h"
#include "UniPlanTopicTypes.h"
#ifdef UPLAN_WATCH
#include "UniPlanWatchApp.h"
#endif
#include "UniPlanHelpers.h"
#include "UniPlanTypes.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Phase list all — collects phases from every plan in the inventory
// ---------------------------------------------------------------------------

void PrintUsage()
{
    std::cout << "Usage:\n";
    std::cout << "  uni-plan --version\n";
    std::cout << "  uni-plan --help\n";
    std::cout << "\n";
    std::cout << "Query commands (read from .Plan.json "
                 "bundles):\n";
    std::cout << "  uni-plan topic list [--status <filter>] "
                 "[--repo-root <path>]\n";
    std::cout << "  uni-plan topic get --topic <topic> "
                 "[--repo-root <path>]\n";
    std::cout << "  uni-plan phase list --topic <topic> "
                 "[--status <filter>] [--repo-root <path>]\n";
    std::cout << "  uni-plan phase get --topic <topic> "
                 "--phase <index> [--brief|--execution|"
                 "--reference] [--repo-root <path>]\n";
    std::cout << "  uni-plan changelog --topic <topic> "
                 "[--phase <N>] "
                 "[--repo-root <path>]\n";
    std::cout << "  uni-plan verification --topic <topic> "
                 "[--phase <N>] "
                 "[--repo-root <path>]\n";
    std::cout << "  uni-plan timeline --topic <topic> "
                 "[--since <yyyy-mm-dd>] "
                 "[--repo-root <path>]\n";
    std::cout << "  uni-plan blockers [--topic <topic>] "
                 "[--repo-root <path>]\n";
    std::cout << "  uni-plan validate [--topic <topic>] "
                 "[--repo-root <path>]\n";
    std::cout << "\n";
    std::cout << "Mutation commands (write to .Plan.json "
                 "bundles):\n";
    std::cout << "  uni-plan topic set --topic <topic> "
                 "[--status <status>] [--next-actions "
                 "<text>]\n";
    std::cout << "  uni-plan phase set --topic <topic> "
                 "--phase <index> [--status <status>] "
                 "[--done <text>] [--remaining <text>] "
                 "[--blockers <text>] [--context <text>]\n";
    std::cout << "  uni-plan job set --topic <topic> "
                 "--phase <index> --job <index> --status "
                 "<status>\n";
    std::cout << "  uni-plan task set --topic <topic> "
                 "--phase <index> --job <index> --task "
                 "<index> [--status <status>] [--evidence "
                 "<text>] [--notes <text>]\n";
    std::cout << "  uni-plan changelog add --topic <topic> "
                 "--change <text> [--scope <N>] "
                 "[--type <feat|fix|refactor|chore>] "
                 "[--evidence <text>]\n";
    std::cout << "  uni-plan verification add --topic "
                 "<topic> --check <text> [--scope "
                 "<N>] [--result <text>] [--detail "
                 "<text>]\n";
    std::cout << "\n";
    std::cout << "Utility:\n";
    std::cout << "  uni-plan cache [info|clear|config] "
                 "[--repo-root <path>]\n";
    std::cout << "  uni-plan watch [--repo-root <path>]\n";
    std::cout << "\n";
    std::cout << "Output is JSON by default.\n";
    std::cout << "Global options:\n";
    std::cout << "  --human       Formatted ANSI tables\n";
    std::cout << "  --repo-root   Override repository root "
                 "path\n";
}

// FCommandHelpEntry + kHuman* constants moved to DocTypes.h

static const FCommandHelpEntry kCommandHelp[] = {
    {"topic",
     "Usage:\n"
     "  uni-plan topic list [--status <filter>]\n"
     "  uni-plan topic get --topic <topic>\n"
     "  uni-plan topic set --topic <topic> [--status <s>] "
     "[--next-actions <text>]\n\n",
     "List, inspect, or update plan topics.\n\n", nullptr,
     "  --status <filter>       Filter topics by status\n"
     "  --next-actions <text>   Set next actions (set only)\n",
     kHumanTable,
     "Examples:\n"
     "  uni-plan topic list --status in_progress\n"
     "  uni-plan topic get --topic MultiPlatforming\n"
     "  uni-plan topic set --topic X --status completed\n"},
    {"phase",
     "Usage:\n"
     "  uni-plan phase list --topic <topic> [--status <filter>]\n"
     "  uni-plan phase get --topic <topic> --phase <N> "
     "[--brief|--execution|--reference]\n"
     "  uni-plan phase set --topic <topic> --phase <N> "
     "[--status <s>] [--done <text>] [--remaining <text>] "
     "[--blockers <text>] [--context <text>]\n\n",
     "List, inspect, or update phases within a topic.\n\n",
     "Required:\n"
     "  --topic <topic>         Topic key\n"
     "  --phase <N>             Phase index (integer)\n\n",
     "  --brief                 Compact view for session resume\n"
     "  --execution             Jobs/tasks only (no reference "
     "material)\n"
     "  --reference             Design material only (no "
     "execution state)\n"
     "  --context <text>        Agent continuation prompt\n",
     kHumanTable,
     "Examples:\n"
     "  uni-plan phase list --topic MultiPlatforming\n"
     "  uni-plan phase get --topic X --phase 6 --brief\n"
     "  uni-plan phase set --topic X --phase 6 --status "
     "in_progress\n"},
    {"job",
     "Usage:\n"
     "  uni-plan job set --topic <topic> --phase <N> --job "
     "<J> --status <status>\n\n",
     "Update job status within a phase.\n\n",
     "Required:\n"
     "  --topic, --phase, --job, --status\n\n",
     nullptr, nullptr,
     "Examples:\n"
     "  uni-plan job set --topic X --phase 2 --job 0 "
     "--status completed\n"},
    {"task",
     "Usage:\n"
     "  uni-plan task set --topic <topic> --phase <N> --job "
     "<J> --task <T> [--status <s>] [--evidence <text>] "
     "[--notes <text>]\n\n",
     "Update task status and evidence within a job.\n\n",
     "Required:\n"
     "  --topic, --phase, --job, --task\n\n",
     "  --evidence <text>       Completion proof\n"
     "  --notes <text>          Agent working notes\n",
     nullptr,
     "Examples:\n"
     "  uni-plan task set --topic X --phase 2 --job 0 "
     "--task 1 --status completed --evidence 'commit "
     "abc'\n"},
    {"changelog",
     "Usage:\n"
     "  uni-plan changelog --topic <topic> [--phase "
     "<N>]\n"
     "  uni-plan changelog add --topic <topic> --change "
     "<text> [--scope <N>] [--type "
     "<feat|fix|refactor|chore>] [--evidence <text>]\n\n",
     "Query or append changelog entries.\n\n",
     "Required:\n"
     "  --topic <topic>         Topic key\n\n",
     "  --phase/--scope         Filter by phase index or "
     "'plan' or phase index\n"
     "  --type                  Change type (add only)\n",
     kHumanTable,
     "Examples:\n"
     "  uni-plan changelog --topic X --phase 2\n"
     "  uni-plan changelog add --topic X --scope 2 "
     "--change 'Implemented feature' --type feat\n"},
    {"verification",
     "Usage:\n"
     "  uni-plan verification --topic <topic> [--phase "
     "<N>]\n"
     "  uni-plan verification add --topic <topic> --check "
     "<text> [--scope <N>] [--result <text>]\n\n",
     "Query or append verification entries.\n\n",
     "Required:\n"
     "  --topic <topic>         Topic key\n\n",
     "  --scope                 Filter by phase index or "
     "'plan'\n",
     kHumanTable,
     "Examples:\n"
     "  uni-plan verification --topic X\n"
     "  uni-plan verification add --topic X --scope 2 "
     "--check 'build passes' --result pass\n"},
    {"timeline",
     "Usage: uni-plan timeline --topic <topic> [--since "
     "<yyyy-mm-dd>]\n\n",
     "Chronological timeline of changelog + verification "
     "entries.\n\n",
     "Required:\n"
     "  --topic <topic>         Topic key\n\n",
     "  --since <date>          Only show entries after "
     "this date\n",
     kHumanTable,
     "Examples:\n"
     "  uni-plan timeline --topic MultiPlatforming\n"
     "  uni-plan timeline --topic X --since 2026-04-01\n"},
    {"blockers", "Usage: uni-plan blockers [--topic <topic>]\n\n",
     "List phases with blocked status or non-trivial "
     "blockers.\n\n",
     nullptr, "  --topic <topic>         Filter to single topic\n", kHumanTable,
     "Examples:\n"
     "  uni-plan blockers\n"
     "  uni-plan blockers --topic MultiPlatforming\n"},
    {"validate", "Usage: uni-plan validate [--topic <topic>]\n\n",
     "Validate .Plan.json bundles against schema.\n\n", nullptr,
     "  --topic <topic>         Validate single topic\n", kHumanTable,
     "Examples:\n"
     "  uni-plan validate\n"
     "  uni-plan validate --topic MultiPlatforming\n"},
};

void PrintCommandUsage(const std::string &InCommand)
{
    // Removed: section, table (dead .md commands)
    // Special-case: cache (multi-subcommand layout)
    if (InCommand == "cache")
    {
        std::cout
            << "Manage the persisted inventory cache.\n\n"
               "uni-plan scans the repository for all plans, playbooks, "
               "implementations,\n"
               "and sidecar documents to build a documentation inventory. This "
               "scan is cached\n"
               "to avoid repeating it on every invocation. The cache is "
               "automatically\n"
               "invalidated when any markdown file is added, removed, resized, "
               "or "
               "modified\n"
               "(tracked via FNV-1a hash of file paths, sizes, and "
               "timestamps).\n\n"
               "Cache location: "
               "~/.codex/uni-plan/cache/<repo-hash>/inventory.cache\n"
               "Each repository gets its own cache entry keyed by a hash of "
               "the "
               "repo path.\n\n"
               "Subcommands:\n"
               "  uni-plan cache [info]   [options]\n"
               "  uni-plan cache clear    [options]\n"
               "  uni-plan cache config   --dir <path> [--enabled "
               "<true|false>] "
               "[--verbose <true|false>] [options]\n\n"
               "cache info:    Show cache directory, size, entry count, and "
               "configuration state.\n"
               "cache clear:   Remove all cached inventory data for all "
               "repositories.\n"
               "cache config:  Update cache settings in uni-plan.ini next to "
               "the "
               "binary.\n\n"
               "Options (config):\n"
               "  --dir <path>            Set cache directory (absolute, "
               "relative, "
               "or ${VAR})\n"
               "  --enabled <true|false>  Enable or disable inventory caching "
               "globally\n"
               "  --verbose <true|false>  Print cache hit/miss information to "
               "stderr\n\n"
               "Common options:\n"
               "  --human                 Output as formatted ANSI display\n"
               "  --repo-root <path>      Override repository root\n\n"
               "Examples:\n"
               "  uni-plan cache\n"
               "  uni-plan cache info\n"
               "  uni-plan cache clear --human\n"
               "  uni-plan cache config --dir /tmp/doc-cache\n"
               "  uni-plan cache config --enabled false\n"
               "  uni-plan cache config --verbose true\n";
        return;
    }
    // Data-driven: standard command help
    for (const FCommandHelpEntry &Entry : kCommandHelp)
    {
        if (InCommand != Entry.mName)
        {
            continue;
        }
        std::cout << Entry.mUsageLine << Entry.mDescription;
        if (Entry.mRequiredOptions != nullptr)
        {
            std::cout << Entry.mRequiredOptions;
        }
        std::cout << "Options:\n";
        if (Entry.mSpecificOptions != nullptr)
        {
            std::cout << Entry.mSpecificOptions;
        }
        std::cout << Entry.mHumanLabel
                  << "  --repo-root <path>      Override repository root\n\n"
                  << Entry.mExamples;
        return;
    }
    // Unknown command — fall back to global usage
    PrintUsage();
}

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
        PrintUsage();
        return 0;
    }

    if (Tokens.size() == 1 && (Tokens[0] == "--help" || Tokens[0] == "-h"))
    {
        PrintUsage();
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

        if (Command == "topic")
        {
            const std::vector<std::string> Args(Tokens.begin() + 1,
                                                Tokens.end());
            return RunTopicCommand(Args, CWD);
        }

        if (Command == "phase")
        {
            const std::vector<std::string> Args(Tokens.begin() + 1,
                                                Tokens.end());
            return RunBundlePhaseCommand(Args, CWD);
        }

        if (Command == "changelog")
        {
            const std::vector<std::string> Args(Tokens.begin() + 1,
                                                Tokens.end());
            return RunBundleChangelogCommand(Args, CWD);
        }

        if (Command == "verification")
        {
            const std::vector<std::string> Args(Tokens.begin() + 1,
                                                Tokens.end());
            return RunBundleVerificationCommand(Args, CWD);
        }

        if (Command == "timeline")
        {
            const std::vector<std::string> Args(Tokens.begin() + 1,
                                                Tokens.end());
            return RunBundleTimelineCommand(Args, CWD);
        }

        if (Command == "blockers")
        {
            const std::vector<std::string> Args(Tokens.begin() + 1,
                                                Tokens.end());
            return RunBundleBlockersCommand(Args, CWD);
        }

        if (Command == "validate")
        {
            const std::vector<std::string> Args(Tokens.begin() + 1,
                                                Tokens.end());
            return RunBundleValidateCommand(Args, CWD);
        }

        if (Command == "job")
        {
            const std::vector<std::string> Args(Tokens.begin() + 1,
                                                Tokens.end());
            if (!Args.empty() && Args[0] == "set")
            {
                const std::vector<std::string> SubArgs(Args.begin() + 1,
                                                       Args.end());
                return RunJobSetCommand(SubArgs, CWD);
            }
            throw UsageError("job requires subcommand: set");
        }

        if (Command == "task")
        {
            const std::vector<std::string> Args(Tokens.begin() + 1,
                                                Tokens.end());
            if (!Args.empty() && Args[0] == "set")
            {
                const std::vector<std::string> SubArgs(Args.begin() + 1,
                                                       Args.end());
                return RunTaskSetCommand(SubArgs, CWD);
            }
            throw UsageError("task requires subcommand: set");
        }

        // --- Old .md-based commands removed ---
        // list, phase (old), lint, inventory, orphan-check,
        // artifacts, changelog (old), verification (old), schema,
        // rules, validate (old), section, excerpt, table, graph,
        // diagnose, timeline (old), blockers (old), plan, evidence,
        // tag, search — all replaced by V4 bundle-native commands.

        if (Command == "cache")
        {
            if (ContainsHelpFlag(
                    std::vector<std::string>(Tokens.begin() + 1, Tokens.end())))
            {
                PrintCommandUsage("cache");
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
        std::cout << kColorDim;
        PrintCommandUsage(Command);
        std::cout << kColorReset;
        return 2;
    }
}

} // namespace UniPlan
