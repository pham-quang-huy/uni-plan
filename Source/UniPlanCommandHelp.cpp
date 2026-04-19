#include "UniPlanCommandHelp.h"
#include "UniPlanTypes.h"

#include <cstring>
#include <set>
#include <string>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Shared footer text — appended to the options block of every command
// that accepts prose-setter flags. Documents the `--<field>-file <path>`
// sibling shape introduced in v0.76.0 that bypasses the shell-double-
// quote expansion hazard for prose containing $VAR / backticks / double
// quotes.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// kCommandHelp — data-driven group-level help registry.
//
// Each entry describes one top-level command group. Subcommand-level
// help (FSubcommandHelpEntry arrays) lands in Commit 2 (v0.85.0 content
// split). In Commit 1 the registry is migrated verbatim from
// UniPlanCommandDispatch.cpp so `--help` routing changes don't alter
// the printed text.
// ---------------------------------------------------------------------------
static const FCommandHelpEntry kCommandHelp[] = {
    {"topic",
     "Usage:\n"
     "  uni-plan topic list [--status <filter>]\n"
     "  uni-plan topic get --topic <T> [--sections <csv>]\n"
     "  uni-plan topic set --topic <T> [--status <s>] "
     "[--summary <t>] [--goals <t>] ...\n"
     "  uni-plan topic start --topic <T>\n"
     "  uni-plan topic complete --topic <T> "
     "[--verification <text>]\n"
     "  uni-plan topic block --topic <T> --reason <text>\n"
     "  uni-plan topic status\n\n",
     "List, inspect, or update plan topics.\n\n", nullptr,
     "  --status <filter>       Filter or set status\n"
     "  --sections <csv>        (get) Filter top-level output to named "
     "sections.\n"
     "                          Valid names: summary, goals, non_goals, "
     "risks,\n"
     "                          acceptance_criteria, problem_statement,\n"
     "                          validation_commands, baseline_audit,\n"
     "                          execution_strategy, locked_decisions,\n"
     "                          source_references, dependencies, "
     "next_actions,\n"
     "                          phases. Identity fields always emitted.\n"
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
     "  uni-plan topic get --topic X --sections summary,phases\n"
     "  uni-plan topic start --topic MultiPlatforming\n"
     "  uni-plan topic complete --topic X\n"
     "  uni-plan topic status --human\n"
     "  uni-plan topic set --topic X "
     "--summary-file summary.txt\n"},
    {"phase",
     "Usage:\n"
     "  uni-plan phase list --topic <T> [--status <filter>]\n"
     "  uni-plan phase get --topic <T> "
     "[--phase <N> | --phases <csv>] "
     "[--brief|--execution|--design]\n"
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
     "--phase <N>\n"
     "  uni-plan phase drift [--topic <T>]\n\n",
     "List, inspect, or update phases within a topic.\n\n",
     "Required:\n"
     "  --topic <T>             Topic key\n"
     "  --phase <N>             Phase index (integer)\n\n",
     "  --brief                 Compact view for session resume\n"
     "  --execution             Jobs / tasks / lanes + dependencies + "
     "validation_commands\n"
     "  --design                Exactly the fields that contribute to "
     "design_chars\n"
     "                          (scope + output + 7 design material prose "
     "fields).\n"
     "                          Renamed from --reference in v0.83.0.\n"
     "  --phases <csv>          Batch mode: emit wrapped v2 schema with "
     "phases\n"
     "                          under a `phases` array. Mutually exclusive "
     "with --phase.\n"
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
     "  uni-plan phase get --topic X --phases 0,2,4 --design\n"
     "  uni-plan phase drift --topic X --human\n"
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
     "  legacy_rich          legacy >= 150 LOC, V4 < 3000 chars (rebuild)\n"
     "  legacy_rich_matched  legacy >= 150 LOC, V4 >= 10000 chars\n"
     "  legacy_thin          legacy 50-149 LOC\n"
     "  legacy_stub          legacy < 50 LOC\n"
     "  legacy_absent        no legacy playbook\n"
     "  v4_only              V4 is rich (>= 10000 chars, >= 3 jobs) and "
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

// ---------------------------------------------------------------------------
// PrintCommandUsage — look up the registry entry and emit the block.
//
// InSubcommand routing:
//   empty                → print group block (or leaf block for single-
//                          command groups whose entire entry is the leaf).
//   non-empty match in   → print per-subcommand FSubcommandHelpEntry.
//   mpSubcommands array
//   non-empty with no    → fall through to group block (safe degradation
//   matching entry         during Commit 1 when leaf arrays are null).
// Unknown command        → fall back to PrintUsage() global.
// ---------------------------------------------------------------------------
static void PrintSubcommandBlock(std::ostream &Out,
                                 const FSubcommandHelpEntry &InSub);

void PrintCommandUsage(std::ostream &Out, const std::string &InCommand,
                       const std::string &InSubcommand)
{
    // Special-case: cache (multi-subcommand layout predates the
    // FCommandHelpEntry registry; keeping inline for Commit 1).
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
        // Subcommand-level lookup — only if the entry has a subcommand
        // table registered (Commit 2 populates these).
        if (!InSubcommand.empty() && Entry.mpSubcommands != nullptr)
        {
            for (size_t I = 0; I < Entry.mSubcommandCount; ++I)
            {
                if (InSubcommand == Entry.mpSubcommands[I].mName)
                {
                    PrintSubcommandBlock(Out, Entry.mpSubcommands[I]);
                    return;
                }
            }
            // Non-matching subcommand — fall through to group block so
            // the user at least sees the subcommand index.
        }
        // Group block (or leaf block for single-command groups).
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
        // Null-guard: `job` and `task` entries carry mHumanLabel =
        // nullptr (no --human renderer). Pre-v0.85.0 callers never
        // exercised PrintCommandUsage with "job"/"task" so this was
        // latent UB; the new test suite hits it.
        if (Entry.mHumanLabel != nullptr)
        {
            Out << Entry.mHumanLabel;
        }
        Out << "  --repo-root <path>      Override repository root\n\n";
        if (Entry.mExamples != nullptr)
        {
            Out << Entry.mExamples;
        }
        return;
    }
    // Unknown command — fall back to global usage
    PrintUsage(Out);
}

// ---------------------------------------------------------------------------
// PrintSubcommandBlock — render one FSubcommandHelpEntry as a complete
// targeted help block. Empty sections are omitted.
// ---------------------------------------------------------------------------
static void PrintSubcommandBlock(std::ostream &Out,
                                 const FSubcommandHelpEntry &InSub)
{
    Out << InSub.mUsageLine;
    Out << InSub.mDescription;
    if (InSub.mRequiredOptions != nullptr)
    {
        Out << InSub.mRequiredOptions;
    }
    if (InSub.mModes != nullptr)
    {
        Out << "Modes (mutually exclusive):\n" << InSub.mModes << "\n";
    }
    if (InSub.mSpecificOptions != nullptr)
    {
        Out << "Options:\n" << InSub.mSpecificOptions;
    }
    if (InSub.mbIsProseCommand)
    {
        Out << kFileFlagFooter;
    }
    // Common options — always shown.
    Out << "\nCommon options:\n";
    if (InSub.mbSupportsHuman)
    {
        Out << kHumanTable;
    }
    Out << "  --repo-root <path>      Override repository root\n\n";
    if (InSub.mOutputSchema != nullptr)
    {
        Out << "Output (JSON):\n"
            << "  schema: " << InSub.mOutputSchema << "\n\n";
    }
    Out << "Exit codes:\n"
        << "  0   Success\n"
        << "  1   Runtime error (I/O, missing bundle, etc.)\n"
        << "  2   Usage error (missing required flag, invalid enum, etc.)\n\n";
    if (InSub.mExamples != nullptr)
    {
        Out << InSub.mExamples;
    }
}

} // namespace UniPlan
