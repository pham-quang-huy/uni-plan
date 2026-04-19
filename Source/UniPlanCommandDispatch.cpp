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
    if (InArgs.empty())
    {
        throw UsageError(std::string(InGroupName) +
                         " requires subcommand: " + InExpectedList);
    }
    for (size_t Index = 0; Index < N; ++Index)
    {
        if (InArgs[0] == InSubs[Index].mName)
        {
            const std::vector<std::string> SubArgs(InArgs.begin() + 1,
                                                   InArgs.end());
            return InSubs[Index].mpHandler(SubArgs, InCwd);
        }
    }
    throw UsageError("Unknown " + std::string(InGroupName) +
                     " subcommand: " + InArgs[0]);
}

static int DispatchJobCommand(const std::vector<std::string> &InArgs,
                              const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {{"set", &RunJobSetCommand}};
    return DispatchSubcommand("job", InArgs, InCwd, kSubs, "set");
}

static int DispatchTaskCommand(const std::vector<std::string> &InArgs,
                               const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {{"set", &RunTaskSetCommand}};
    return DispatchSubcommand("task", InArgs, InCwd, kSubs, "set");
}

static int DispatchLaneCommand(const std::vector<std::string> &InArgs,
                               const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {{"set", &RunLaneSetCommand},
                                          {"add", &RunLaneAddCommand}};
    return DispatchSubcommand("lane", InArgs, InCwd, kSubs, "set, add");
}

static int DispatchTestingCommand(const std::vector<std::string> &InArgs,
                                  const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {{"add", &RunTestingAddCommand},
                                          {"set", &RunTestingSetCommand}};
    return DispatchSubcommand("testing", InArgs, InCwd, kSubs, "add, set");
}

static int DispatchManifestCommand(const std::vector<std::string> &InArgs,
                                   const std::string &InCwd)
{
    static const FCommandEntry kSubs[] = {{"add", &RunManifestAddCommand},
                                          {"set", &RunManifestSetCommand},
                                          {"remove", &RunManifestRemoveCommand},
                                          {"list", &RunManifestListCommand}};
    return DispatchSubcommand("manifest", InArgs, InCwd, kSubs,
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
    Out << "  uni-plan topic get --topic <T>\n";
    Out << "  uni-plan topic status\n";
    Out << "  uni-plan phase list --topic <T> "
           "[--status <filter>]\n";
    Out << "  uni-plan phase get --topic <T> "
           "--phase <N> [--brief|--execution|"
           "--reference]\n";
    Out << "  uni-plan phase next --topic <T>\n";
    Out << "  uni-plan phase readiness --topic <T> "
           "--phase <N>\n";
    Out << "  uni-plan phase wave-status --topic <T> "
           "--phase <N>\n";
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
    Out << "  uni-plan job set --topic <T> "
           "--phase <N> --job <N> [--status <s>] "
           "[--scope <t>] ...\n";
    Out << "  uni-plan task set --topic <T> "
           "--phase <N> --job <N> --task <N> "
           "[--status <s>]\n";
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
    Out << "  uni-plan lane set --topic <T> "
           "--phase <N> --lane <N> [--status <s>] "
           "[--scope <t>] ...\n";
    Out << "  uni-plan lane add --topic <T> "
           "--phase <N> [--status <s>] "
           "[--scope <t>] [--exit-criteria <t>]\n";
    Out << "  uni-plan testing add --topic <T> "
           "--phase <N> --step <text> "
           "--action <text> --expected <text>\n";
    Out << "  uni-plan testing set --topic <T> "
           "--phase <N> --index <N> [--session <t>] "
           "[--actor <t>] [--step <t>] ...\n";
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

// FCommandHelpEntry + kHuman* constants moved to DocTypes.h

// Shared footer text — appended to the options block of every command that
// accepts prose-setter flags. Documents the `--<field>-file <path>` sibling
// shape introduced in v0.75.1 that bypasses the shell-double-quote expansion
// hazard for prose containing $VAR / backticks / double quotes.
static constexpr const char *kFileFlagFooter =
    "\n"
    "File-based prose input (v0.76.0+):\n"
    "  Every --<field> <text> flag above also accepts "
    "--<field>-file <path>.\n"
    "  The CLI reads the file as raw bytes — no shell expansion, so "
    "`$VAR`,\n"
    "  backticks, and double quotes round-trip safely. Prefer the file "
    "form\n"
    "  for long content or anything containing shell metachars.\n";

static const FCommandHelpEntry kCommandHelp[] = {
    {"topic",
     "Usage:\n"
     "  uni-plan topic list [--status <filter>]\n"
     "  uni-plan topic get --topic <T>\n"
     "  uni-plan topic set --topic <T> [--status <s>] "
     "[--summary <t>] [--goals <t>] ...\n"
     "  uni-plan topic start --topic <T>\n"
     "  uni-plan topic complete --topic <T> "
     "[--verification <text>]\n"
     "  uni-plan topic block --topic <T> --reason <text>\n"
     "  uni-plan topic status\n\n",
     "List, inspect, or update plan topics.\n\n", nullptr,
     "  --status <filter>       Filter or set status\n"
     "  --next-actions <text>   Set next actions\n"
     "  --summary <text>        Set plan summary\n"
     "  --goals <text>          Set plan goals\n"
     "  --non-goals <text>      Set non-goals\n"
     "  --risks <text>          Set risks\n"
     "  --acceptance-criteria   Set acceptance criteria\n"
     "  --problem-statement     Set problem statement\n"
     "  --validation-commands   Set validation commands\n"
     "  --baseline-audit        Set baseline audit\n"
     "  --execution-strategy    Set execution strategy\n"
     "  --locked-decisions      Set locked decisions\n"
     "  --source-references     Set source references\n"
     "  --dependency-clear      Empty dependencies array before "
     "--dependency-add\n"
     "  --dependency-add <spec> Add typed dependency; spec = "
     "'<kind>|<topic>|<phase>|<path>|<note>'\n"
     "                          <kind>: bundle | phase | governance "
     "| external\n"
     "  --reason <text>         Block reason (block only)\n"
     "  --verification <text>   Verification note (complete "
     "only)\n",
     kHumanTable,
     "Examples:\n"
     "  uni-plan topic list --status in_progress\n"
     "  uni-plan topic start --topic MultiPlatforming\n"
     "  uni-plan topic complete --topic X\n"
     "  uni-plan topic status --human\n"
     "  uni-plan topic set --topic X "
     "--summary-file summary.txt\n"},
    {"phase",
     "Usage:\n"
     "  uni-plan phase list --topic <T> [--status <filter>]\n"
     "  uni-plan phase get --topic <T> --phase <N> "
     "[--brief|--execution|--reference]\n"
     "  uni-plan phase set --topic <T> --phase <N> "
     "[--status <s>] [--done <text>] ...\n"
     "  uni-plan phase start --topic <T> --phase <N> "
     "[--context <text>]\n"
     "  uni-plan phase complete --topic <T> --phase <N> "
     "--done <text> [--verification <text>]\n"
     "  uni-plan phase block --topic <T> --phase <N> "
     "--reason <text>\n"
     "  uni-plan phase unblock --topic <T> --phase <N>\n"
     "  uni-plan phase progress --topic <T> --phase <N> "
     "--done <text> --remaining <text>\n"
     "  uni-plan phase complete-jobs --topic <T> "
     "--phase <N>\n"
     "  uni-plan phase log --topic <T> --phase <N> "
     "--change <text> [--type <type>]\n"
     "  uni-plan phase verify --topic <T> --phase <N> "
     "--check <text> [--result <text>]\n"
     "  uni-plan phase next --topic <T>\n"
     "  uni-plan phase readiness --topic <T> --phase <N>\n"
     "  uni-plan phase wave-status --topic <T> "
     "--phase <N>\n\n",
     "List, inspect, or update phases within a topic.\n\n",
     "Required:\n"
     "  --topic <T>             Topic key\n"
     "  --phase <N>             Phase index (integer)\n\n",
     "  --brief                 Compact view for session resume\n"
     "  --execution             Jobs/tasks only\n"
     "  --reference             Design material only\n"
     "  --context <text>        Agent continuation prompt\n"
     "  --done <text>           Completed work summary\n"
     "  --done-clear            Clear done field (for reverting a phase "
     "whose\n"
     "                          done text is stale placeholder prose)\n"
     "  --started-at <iso>      Explicit started_at override (set only, "
     "for migration/repair)\n"
     "  --completed-at <iso>    Explicit completed_at override (set only, "
     "for migration/repair)\n"
     "  --remaining <text>      Remaining work\n"
     "  --remaining-clear       Clear remaining field\n"
     "  --blockers-clear        Clear blockers field (prefer `phase "
     "unblock` for\n"
     "                          the normal blocked->in_progress flow)\n"
     "  --reason <text>         Block reason\n"
     "  --verification <text>   Verification note\n"
     "  --change <text>         Changelog entry (log only)\n"
     "  --check <text>          Verification check (verify "
     "only)\n"
     "  --scope <text>          Set phase scope (set only)\n"
     "  --output <text>         Set phase output (set only)\n"
     "  --investigation <text>  Set investigation notes\n"
     "  --code-entity-contract  Set code entity contract\n"
     "  --code-snippets <text>  Set code snippets\n"
     "  --best-practices <text> Set best practices\n"
     "  --multi-platforming     Set multi-platforming notes\n"
     "  --readiness-gate <text> Set readiness gate\n"
     "  --handoff <text>        Set handoff notes\n"
     "  --validation-commands   Set validation commands\n"
     "  --dependency-clear      Empty dependencies array before "
     "--dependency-add\n"
     "  --dependency-add <spec> Add typed dependency; spec = "
     "'<kind>|<topic>|<phase>|<path>|<note>'\n"
     "                          <kind>: bundle | phase | governance "
     "| external\n"
     "  --origin <value>        Stamp phase provenance: native_v4 or "
     "v3_migration\n"
     "                          (idempotent; empty = no change)\n",
     kHumanTable,
     "Examples:\n"
     "  uni-plan phase start --topic X --phase 6\n"
     "  uni-plan phase complete --topic X --phase 6 "
     "--done \"Implemented\"\n"
     "  uni-plan phase next --topic X --human\n"
     "  uni-plan phase readiness --topic X --phase 6\n"
     "  uni-plan phase set --topic X --phase 0 --origin v3_migration\n"
     "  uni-plan phase set --topic X --phase 6 "
     "--investigation-file inv.txt\n"},
    {"job",
     "Usage:\n"
     "  uni-plan job set --topic <topic> --phase <N> --job "
     "<J> [--status <s>] [--scope <t>] [--output <t>] "
     "[--exit-criteria <t>]\n\n",
     "Update job fields within a phase.\n\n",
     "Required:\n"
     "  --topic, --phase, --job\n\n",
     "  --status <s>            Set execution status\n"
     "  --scope <text>          Set job scope\n"
     "  --output <text>         Set job output\n"
     "  --exit-criteria <text>  Set exit criteria\n",
     nullptr,
     "Examples:\n"
     "  uni-plan job set --topic X --phase 2 --job 0 "
     "--status completed\n"
     "  uni-plan job set --topic X --phase 0 --job 0 "
     "--scope \"Implement core logic\"\n"},
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
     "<text> [--phase <N>] [--type "
     "<feat|fix|refactor|chore>] [--affected <text>]\n\n",
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
     "Usage: uni-plan timeline --topic <topic> [--phase "
     "<N>] [--since <yyyy-mm-dd>]\n\n",
     "Chronological timeline of changelog + verification "
     "entries.\n\n",
     "Required:\n"
     "  --topic <topic>         Topic key\n\n",
     "  --phase <N>             Filter by phase index\n"
     "  --since <date>          Only show entries after "
     "this date\n",
     kHumanTable,
     "Examples:\n"
     "  uni-plan timeline --topic MultiPlatforming\n"
     "  uni-plan timeline --topic X --phase 6\n"
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
    {"legacy-gap",
     "Usage: uni-plan legacy-gap [--topic <topic>] [--category <c>]\n\n",
     "Stateless per-phase parity audit between V3 .md artifacts and V4 "
     "bundles.\n"
     "Discovers legacy .md files on disk at invoke time via filename "
     "convention\n"
     "(<Topic>.Plan.md, <Topic>.<PhaseKey>.Playbook.md, sidecars). Bundles "
     "carry\n"
     "no legacy path index — after the .md corpus is deleted every phase "
     "falls\n"
     "into legacy_absent / v4_only which is the correct steady state.\n\n"
     "Each phase is bucketed into one of 8 categories:\n"
     "  legacy_rich          legacy >= 200 LOC, V4 < 4000 chars (rebuild)\n"
     "  legacy_rich_matched  legacy >= 200 LOC, V4 >= 16000 chars\n"
     "  legacy_thin          legacy 50-199 LOC\n"
     "  legacy_stub          legacy < 50 LOC\n"
     "  legacy_absent        no legacy playbook\n"
     "  v4_only              V4 is rich (>= 16000 chars, >= 3 jobs) and "
     "no legacy\n"
     "  hollow_both          legacy stub AND V4 hollow on a completed phase\n"
     "  drift                reserved for future semantic-overlap "
     "detection\n\n",
     nullptr,
     "  --topic <topic>         Audit only this topic\n"
     "  --category <c>          Emit only rows matching this category\n",
     kHumanTable,
     "Examples:\n"
     "  uni-plan legacy-gap --human\n"
     "  uni-plan legacy-gap --topic CycleRefactor --human\n"
     "  uni-plan legacy-gap --category legacy_rich --human\n"},
};

void PrintCommandUsage(std::ostream &Out, const std::string &InCommand)
{
    // Removed: section, table (dead .md commands)
    // Special-case: cache (multi-subcommand layout)
    if (InCommand == "cache")
    {
        Out << "Manage the persisted inventory cache.\n\n"
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
               "~/.uni-plan/cache/<repo-hash>/inventory.cache\n"
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
        Out << Entry.mUsageLine << Entry.mDescription;
        if (Entry.mRequiredOptions != nullptr)
        {
            Out << Entry.mRequiredOptions;
        }
        Out << "Options:\n";
        if (Entry.mSpecificOptions != nullptr)
        {
            Out << Entry.mSpecificOptions;
        }
        // Append the shared --<field>-file footer for commands that
        // expose prose-setter flags. Single source of truth via
        // kFileFlagFooter so the note stays consistent across every
        // mutation-bearing help block.
        // Names that match a kCommandHelp entry AND expose any
        // prose-setter flag. lane / testing / manifest route through
        // per-subcommand dispatchers rather than kCommandHelp, so they
        // don't appear here — their `-file` flags still work, the note
        // just lives in the repo-level docs rather than in --help output.
        static const std::set<std::string> kProseCommands = {
            "topic", "phase", "job", "task", "changelog", "verification"};
        if (kProseCommands.count(Entry.mName) > 0)
        {
            Out << kFileFlagFooter;
        }
        Out << Entry.mHumanLabel
            << "  --repo-root <path>      Override repository root\n\n"
            << Entry.mExamples;
        return;
    }
    // Unknown command — fall back to global usage
    PrintUsage(Out);
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
            if (ContainsHelpFlag(
                    std::vector<std::string>(Tokens.begin() + 1, Tokens.end())))
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
        std::cerr << kColorDim;
        PrintCommandUsage(std::cerr, Command);
        std::cerr << kColorReset;
        return 2;
    }
}

} // namespace UniPlan
