#include "UniPlanCommandHelp.h"
#include "UniPlanTypes.h"

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

// ===========================================================================
// Subcommand-level help registries (v0.85.0 Commit 2).
//
// One FSubcommandHelpEntry per leaf command. Group entries in
// kCommandHelp below reference these arrays via mpSubcommands /
// mSubcommandCount. PrintCommandUsage(Out, "phase", "get") → leaf
// block; PrintCommandUsage(Out, "phase", "") → group summary block.
//
// Fields that are consistent across every leaf and never vary:
//   --human         documented via mbSupportsHuman (emits kHumanTable)
//   --repo-root     emitted by PrintSubcommandBlock unconditionally
//   --<field>-file  footer appended when mbIsProseCommand = true
//   Exit codes      emitted by PrintSubcommandBlock unconditionally
// ===========================================================================

// ---------------------------------------------------------------------------
// topic subcommands
// ---------------------------------------------------------------------------
static const FSubcommandHelpEntry kTopicSubs[] = {
    {
        "list",
        "Usage: uni-plan topic list [--status <filter>] [--human]\n\n",
        "List every topic in the repo with status + phase count.\n\n",
        nullptr, // required
        "  --status <filter>       Filter by status: not_started|in_progress|\n"
        "                          completed|blocked|dropped|canceled|all\n"
        "                          (default: all)\n",
        nullptr, // modes
        "uni-plan-topic-list-v1",
        "Examples:\n"
        "  uni-plan topic list\n"
        "  uni-plan topic list --status in_progress --human\n",
        /*mbIsProseCommand*/ false,
        /*mbSupportsHuman*/ true,
    },
    {
        "get",
        "Usage: uni-plan topic get --topic <T> [--sections <csv>] "
        "[--human]\n\n",
        "Retrieve a single topic's full metadata + phase summary.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key (PascalCase bundle stem)\n\n",
        "  --sections <csv>        Filter output to named top-level sections.\n"
        "                          Valid names: summary, goals, non_goals,\n"
        "                          risks, acceptance_criteria,\n"
        "                          problem_statement, validation_commands,\n"
        "                          baseline_audit, execution_strategy,\n"
        "                          locked_decisions, source_references,\n"
        "                          dependencies, next_actions, phases.\n"
        "                          Identity fields (topic / status / title /\n"
        "                          phase_count) are always emitted. Unknown\n"
        "                          names fail at parse time (exit 2).\n",
        nullptr,
        "uni-plan-topic-get-v1",
        "Examples:\n"
        "  uni-plan topic get --topic MultiPlatforming\n"
        "  uni-plan topic get --topic X --sections summary,phases\n"
        "  uni-plan topic get --topic X --human\n",
        false,
        true,
    },
    {
        "set",
        "Usage: uni-plan topic set --topic <T> [options]\n\n",
        "Mutate topic-level fields. Appends an auto-changelog entry for\n"
        "every changed field. Prefer the semantic `topic start/complete/\n"
        "block` commands for lifecycle transitions over `--status`.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n\n",
        "  --status <s>            Set status: not_started | in_progress |\n"
        "                          completed | blocked | dropped | canceled\n"
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
        "  --dependency-clear      Empty dependencies array before\n"
        "                          --dependency-add\n"
        "  --dependency-add <spec> Add typed dependency; spec =\n"
        "                          '<kind>|<topic>|<phase>|<path>|<note>'\n"
        "                          <kind>: bundle | phase | governance |\n"
        "                                  external\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan topic set --topic X --summary-file summary.txt\n"
        "  uni-plan topic set --topic X --dependency-clear \\\n"
        "                     --dependency-add 'bundle|OtherTopic|||'\n",
        /*mbIsProseCommand*/ true,
        /*mbSupportsHuman*/ false,
    },
    {
        "start",
        "Usage: uni-plan topic start --topic <T>\n\n",
        "Transition topic from not_started to in_progress. Fails (exit 2)\n"
        "if the topic is already started.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n\n",
        nullptr, // options
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan topic start --topic MultiPlatforming\n",
        false,
        false,
    },
    {
        "complete",
        "Usage: uni-plan topic complete --topic <T> [--verification <text>]\n"
        "                                [--verification-file <path>]\n\n",
        "Transition topic to completed. Gate: every phase must be completed\n"
        "or canceled. Optional --verification appends a final verification\n"
        "entry.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n\n",
        "  --verification <text>   Optional verification note\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan topic complete --topic X\n"
        "  uni-plan topic complete --topic X --verification 'All phases "
        "green'\n",
        true,
        false,
    },
    {
        "block",
        "Usage: uni-plan topic block --topic <T> --reason <text>\n"
        "                            [--reason-file <path>]\n\n",
        "Transition topic to blocked. --reason records the blocker in the\n"
        "auto-changelog. Gate: topic must be in_progress.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --reason <text>         Why the topic is blocked\n\n",
        nullptr,
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan topic block --topic X --reason 'Upstream dep stalled'\n",
        true,
        false,
    },
    {
        "status",
        "Usage: uni-plan topic status [--human]\n\n",
        "Aggregate overview of every topic: counts, active phases, blockers.\n"
        "Useful for the start of a session to see where to pick up.\n\n",
        nullptr,
        nullptr,
        nullptr,
        "uni-plan-topic-list-v1",
        "Examples:\n"
        "  uni-plan topic status\n"
        "  uni-plan topic status --human\n",
        false,
        true,
    },
};

// ---------------------------------------------------------------------------
// phase subcommands
// ---------------------------------------------------------------------------
static const FSubcommandHelpEntry kPhaseSubs[] = {
    {
        "list",
        "Usage: uni-plan phase list --topic <T> [--status <filter>] [--human]\n"
        "\n",
        "List phases in a topic with per-phase status, scope preview, job/\n"
        "task/design-chars counts.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n\n",
        "  --status <filter>       Filter by status: not_started|in_progress|\n"
        "                          completed|blocked|dropped|canceled|all\n",
        nullptr,
        "uni-plan-phase-list-v2",
        "Examples:\n"
        "  uni-plan phase list --topic MultiPlatforming\n"
        "  uni-plan phase list --topic X --status in_progress --human\n",
        false,
        true,
    },
    {
        "get",
        "Usage: uni-plan phase get --topic <T> [--phase <N> | --phases <csv>]\n"
        "                          [--brief | --design | --execution]\n"
        "                          [--human]\n\n",
        "Retrieve one or more phases.\n\n"
        "Pick exactly one index selector:\n"
        "  --phase <N>             Single-phase mode. Emits flat v1 schema.\n"
        "  --phases <csv>          Batch mode (v0.84.0+): comma-separated\n"
        "                          non-negative indices (e.g. 0,2,4). Emits\n"
        "                          wrapped v2 schema with phase objects\n"
        "                          under a `phases` array. Sort+dedup is\n"
        "                          applied server-side.\n\n"
        "Pick at most one output-shape mode (all four shapes carry the\n"
        "common header: schema, topic, phase_index, status, design_chars,\n"
        "scope).\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  (one of) --phase <N>  |  --phases <csv>\n\n",
        nullptr, // mSpecificOptions — all the real options live in Modes
        "  --brief                 Compact view for session resume ~500 tok:\n"
        "                          done/remaining/blockers/agent_context +\n"
        "                          job_summary + next_job + progress.\n"
        "  --design                The 9 prose fields that feed design_chars:\n"
        "                          scope + output + 7 design-material fields\n"
        "                          (investigation / code_entity_contract /\n"
        "                          code_snippets / best_practices / multi_\n"
        "                          platforming / readiness_gate / handoff).\n"
        "                          Renamed from --reference in v0.83.0.\n"
        "  --execution             Structural surface: lanes / jobs (with\n"
        "                          nested tasks) / dependencies / validation\n"
        "                          _commands + lifecycle timestamps.\n"
        "  (no flag)               Full view: execution + design prose +\n"
        "                          testing + file_manifest.\n",
        "uni-plan-phase-get-v1 (single) / uni-plan-phase-get-v2 (batch)",
        "Examples:\n"
        "  uni-plan phase get --topic X --phase 0\n"
        "  uni-plan phase get --topic X --phase 0 --design\n"
        "  uni-plan phase get --topic X --phase 0 --brief\n"
        "  uni-plan phase get --topic X --phases 0,2,4 --execution\n"
        "  uni-plan phase get --topic X --phase 6 --human\n",
        false,
        true,
    },
    {
        "set",
        "Usage: uni-plan phase set --topic <T> --phase <N> [options]\n\n",
        "Low-level mutation of phase fields. Prefer the semantic commands\n"
        "(phase start / complete / block / unblock / progress / log /\n"
        "verify) when possible — they enforce lifecycle gates.\n\n"
        "Transitioning to --status completed requires both timestamps: the\n"
        "normal path (phase start → phase complete) stamps them. If\n"
        "skipping straight from not_started to completed, --started-at\n"
        "MUST be supplied (Data Fix Gate, v0.73.1).\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n\n",
        "  --status <s>            not_started|in_progress|completed|blocked|\n"
        "                          dropped|canceled\n"
        "  --context <text>        Agent continuation prompt\n"
        "  --done <text>           Completed-work summary\n"
        "  --done-clear            Clear done field (revert stale prose)\n"
        "  --remaining <text>      Remaining work\n"
        "  --remaining-clear       Clear remaining field\n"
        "  --blockers <text>       Blockers (prefer `phase block --reason`)\n"
        "  --blockers-clear        Clear blockers field\n"
        "  --scope <text>          Set phase scope\n"
        "  --output <text>         Set phase output\n"
        "  --investigation <text>  Set investigation notes\n"
        "  --code-entity-contract  Set code entity contract\n"
        "  --code-snippets <text>  Set code snippets\n"
        "  --best-practices <text> Set best practices\n"
        "  --multi-platforming     Set multi-platforming notes\n"
        "  --readiness-gate <text> Set readiness gate\n"
        "  --handoff <text>        Set handoff notes\n"
        "  --validation-commands   Set validation commands\n"
        "  --started-at <iso>      Explicit started_at override (ISO-8601;\n"
        "                          migration/repair only)\n"
        "  --completed-at <iso>    Explicit completed_at override\n"
        "  --origin <value>        Stamp phase provenance: native_v4 |\n"
        "                          v3_migration (idempotent; empty = skip)\n"
        "  --dependency-clear      Empty dependencies array before\n"
        "                          --dependency-add\n"
        "  --dependency-add <spec> Add typed dependency; spec =\n"
        "                          '<kind>|<topic>|<phase>|<path>|<note>'\n"
        "  --no-file-manifest <true|false>\n"
        "                          v0.86.0+: explicit-no-manifest opt-out.\n"
        "                          When true, the\n"
        "                          file_manifest_required_for_code_phases\n"
        "                          evaluator (and v0.88.0+ phase-complete\n"
        "                          gate) skip this phase. REQUIRES\n"
        "                          --no-file-manifest-reason in the same\n"
        "                          call when flipping to true.\n"
        "  --no-file-manifest-reason <text>\n"
        "                          Justification (e.g., 'taxonomy rollout,\n"
        "                          no code touched'). Stored on the phase;\n"
        "                          enforced as non-empty whenever\n"
        "                          no_file_manifest=true.\n"
        "  --no-file-manifest-reason-file <path>\n"
        "                          Read the justification from a file\n"
        "                          (avoids shell-quoting hazards).\n"
        "  --no-file-manifest-reason-clear\n"
        "                          Clear the justification (only valid when\n"
        "                          flipping no_file_manifest back to false\n"
        "                          in the same call).\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan phase set --topic X --phase 0 --origin v3_migration\n"
        "  uni-plan phase set --topic X --phase 6 \\\n"
        "                     --investigation-file inv.txt\n"
        "  uni-plan phase set --topic X --phase 0 --status completed \\\n"
        "                     --started-at 2026-03-01T00:00:00Z\n"
        "  uni-plan phase set --topic X --phase 0 --no-file-manifest true \\\n"
        "                     --no-file-manifest-reason \"Doc-only plan\"\n",
        true,
        false,
    },
    {
        "add",
        "Usage: uni-plan phase add --topic <T> [--scope <t>] [--output <t>]\n"
        "                          [--status <s>]\n\n",
        "Append a trailing phase. Default status = not_started. Use `phase\n"
        "set` to populate richer fields after creation.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n\n",
        "  --scope <text>          Phase scope\n"
        "  --output <text>         Phase output\n"
        "  --status <s>            Initial status (default: not_started)\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan phase add --topic X --scope 'Migration cleanup'\n",
        true,
        false,
    },
    {
        "remove",
        "Usage: uni-plan phase remove --topic <T> --phase <N>\n\n",
        "Remove a trailing phase. Gates: the phase must be (a) the last\n"
        "index in the array, (b) not_started, (c) carry no changelog or\n"
        "verification references. Fails (exit 2) otherwise.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index (must be the last one)\n\n",
        nullptr,
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan phase remove --topic X --phase 4\n",
        false,
        false,
    },
    {
        "normalize",
        "Usage: uni-plan phase normalize --topic <T> --phase <N> [--dry-run]\n"
        "\n",
        "Replace em/en/figure dashes with '-', curly quotes with straight\n"
        "quotes, non-breaking spaces with regular spaces across every prose\n"
        "field in the phase. Idempotent. Use after bulk imports to satisfy\n"
        "the no_smart_quotes validator.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n\n",
        "  --dry-run               Report replacements, do not write\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan phase normalize --topic X --phase 2 --dry-run\n"
        "  uni-plan phase normalize --topic X --phase 2\n",
        false,
        false,
    },
    {
        "start",
        "Usage: uni-plan phase start --topic <T> --phase <N>\n"
        "                            [--context <text>]\n"
        "                            [--context-file <path>]\n\n",
        "Transition phase from not_started to in_progress and stamp\n"
        "started_at = now (UTC). Gate: phase must be not_started AND must\n"
        "have non-empty design material (scope + at least one design\n"
        "field). Optional --context captures the agent continuation prompt.\n"
        "\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n\n",
        "  --context <text>        Agent continuation prompt for this phase\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan phase start --topic X --phase 6\n"
        "  uni-plan phase start --topic X --phase 6 \\\n"
        "                       --context 'Build on Windows first'\n",
        true,
        false,
    },
    {
        "complete",
        "Usage: uni-plan phase complete --topic <T> --phase <N>\n"
        "                               --done <text> [--done-file <path>]\n"
        "                               [--verification <text>]\n"
        "                               [--verification-file <path>]\n\n",
        "Transition phase to completed and stamp completed_at = now (UTC).\n"
        "Gate: phase must be in_progress. Optional --verification appends\n"
        "a verification entry capturing proof of completion.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n"
        "  --done <text>           Completion summary\n\n",
        "  --verification <text>   Optional verification note\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan phase complete --topic X --phase 6 --done 'Shipped'\n"
        "  uni-plan phase complete --topic X --phase 6 --done 'Shipped' \\\n"
        "                          --verification 'build.sh exit 0'\n",
        true,
        false,
    },
    {
        "block",
        "Usage: uni-plan phase block --topic <T> --phase <N>\n"
        "                            --reason <text> [--reason-file <path>]\n"
        "\n",
        "Transition phase to blocked. Records the reason in the auto-\n"
        "changelog and in the phase's blockers field. Gate: phase must be\n"
        "in_progress.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n"
        "  --reason <text>         Why the phase is blocked\n\n",
        nullptr,
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan phase block --topic X --phase 6 \\\n"
        "                       --reason 'Upstream API unstable'\n",
        true,
        false,
    },
    {
        "unblock",
        "Usage: uni-plan phase unblock --topic <T> --phase <N>\n\n",
        "Transition phase from blocked back to in_progress. Clears the\n"
        "blockers field. Gate: phase must be blocked.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n\n",
        nullptr,
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan phase unblock --topic X --phase 6\n",
        false,
        false,
    },
    {
        "progress",
        "Usage: uni-plan phase progress --topic <T> --phase <N>\n"
        "                               --done <text> --remaining <text>\n"
        "                               [--done-file <path>]\n"
        "                               [--remaining-file <path>]\n\n",
        "Update both `done` and `remaining` in a single atomic mutation.\n"
        "Gate: phase must be in_progress.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n"
        "  --done <text>           What's done so far\n"
        "  --remaining <text>      What's left\n\n",
        nullptr,
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan phase progress --topic X --phase 6 \\\n"
        "                          --done 'jobs 0-2' \\\n"
        "                          --remaining 'jobs 3-4 + testing'\n",
        true,
        false,
    },
    {
        "complete-jobs",
        "Usage: uni-plan phase complete-jobs --topic <T> --phase <N>\n\n",
        "Bulk-mark every job in the phase as completed. Convenience for\n"
        "finalizing a phase whose per-job transitions have already been\n"
        "recorded informally. Gate: phase must be in_progress.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n\n",
        nullptr,
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan phase complete-jobs --topic X --phase 6\n",
        false,
        false,
    },
    {
        "log",
        "Usage: uni-plan phase log --topic <T> --phase <N>\n"
        "                          --change <text> [--change-file <path>]\n"
        "                          [--type <feat|fix|refactor|chore>]\n"
        "                          [--affected <text>]\n"
        "                          [--affected-file <path>]\n\n",
        "Append a changelog entry scoped to this phase. Shortcut for\n"
        "`changelog add --topic X --phase N --change ...` with a phase\n"
        "bounds check.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n"
        "  --change <text>         What changed\n\n",
        "  --type <t>              feat | fix | refactor | chore\n"
        "  --affected <text>       Canonical target, e.g. phases[2].jobs[1]\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan phase log --topic X --phase 2 \\\n"
        "                     --change 'Implemented core loop' --type feat\n",
        true,
        false,
    },
    {
        "verify",
        "Usage: uni-plan phase verify --topic <T> --phase <N>\n"
        "                             --check <text> [--check-file <path>]\n"
        "                             [--result <text>]\n"
        "                             [--detail <text>]\n\n",
        "Append a verification entry scoped to this phase. Shortcut for\n"
        "`verification add --topic X --phase N --check ...`.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n"
        "  --check <text>          What was verified\n\n",
        "  --result <text>         pass | fail | partial | string result\n"
        "  --detail <text>         Optional long-form detail / command log\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan phase verify --topic X --phase 6 \\\n"
        "                       --check 'build.sh' --result pass\n",
        true,
        false,
    },
    {
        "next",
        "Usage: uni-plan phase next --topic <T> [--human]\n\n",
        "Find the next not_started phase in the topic and emit its\n"
        "readiness gates. Returns empty when no not_started phase exists.\n"
        "\n",
        "Required:\n"
        "  --topic <T>             Topic key\n\n",
        nullptr,
        nullptr,
        "uni-plan-phase-get-v1",
        "Examples:\n"
        "  uni-plan phase next --topic MultiPlatforming\n"
        "  uni-plan phase next --topic X --human\n",
        false,
        true,
    },
    {
        "readiness",
        "Usage: uni-plan phase readiness --topic <T> --phase <N> [--human]\n\n",
        "Report gate-by-gate readiness for a phase: design material, best-\n"
        "practice investigation, code-entity contract, code snippets,\n"
        "multi-platforming, testing, validation commands.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n\n",
        nullptr,
        nullptr,
        "uni-plan-phase-get-v1",
        "Examples:\n"
        "  uni-plan phase readiness --topic X --phase 6 --human\n",
        false,
        true,
    },
    {
        "wave-status",
        "Usage: uni-plan phase wave-status --topic <T> --phase <N> [--human]\n"
        "\n",
        "Per-wave job completion rollup — counts completed / total jobs\n"
        "per wave index. Use to check whether the next wave's readiness\n"
        "gate is met.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n\n",
        nullptr,
        nullptr,
        "uni-plan-phase-get-v1",
        "Examples:\n"
        "  uni-plan phase wave-status --topic X --phase 6 --human\n",
        false,
        true,
    },
    {
        "drift",
        "Usage: uni-plan phase drift [--topic <T>] [--human]\n\n",
        "Report phases where declared lifecycle status disagrees with the\n"
        "evidence stored elsewhere in the bundle. Added v0.84.0.\n\n"
        "Four drift kinds:\n"
        "  status_lag_lane       status=not_started but lanes progressed\n"
        "  status_lag_done       status=not_started but `done` substantive\n"
        "  status_lag_timestamp  status=not_started but completed_at set\n"
        "  completion_lag_lane   status=completed but some lane isn't\n\n"
        "Shared detection logic with the `phase_status_lane_alignment`\n"
        "validator so `validate` and `drift` never disagree.\n\n",
        nullptr, // no required flags — --topic is optional
        "  --topic <T>             Scope to a single topic (default: whole\n"
        "                          corpus)\n",
        nullptr,
        "uni-plan-phase-drift-v1",
        "Examples:\n"
        "  uni-plan phase drift\n"
        "  uni-plan phase drift --topic MultiPlatforming --human\n",
        false,
        true,
    },
};

// ---------------------------------------------------------------------------
// job / task subcommands
// ---------------------------------------------------------------------------
static const FSubcommandHelpEntry kJobSubs[] = {
    {
        "set",
        "Usage: uni-plan job set --topic <T> --phase <N> --job <J> [options]\n"
        "\n",
        "Update job fields within a phase.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n"
        "  --job <J>               Job index within the phase\n\n",
        "  --status <s>            not_started|in_progress|completed|blocked|\n"
        "                          dropped|canceled\n"
        "  --scope <text>          Job scope\n"
        "  --output <text>         Job output\n"
        "  --exit-criteria <text>  Job exit criteria\n"
        "  --lane <N>              Move job to lane index N\n"
        "  --wave <N>              Move job to wave index N\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan job set --topic X --phase 2 --job 0 --status completed\n"
        "  uni-plan job set --topic X --phase 0 --job 0 \\\n"
        "                   --scope 'Implement core logic'\n",
        true,
        false,
    },
};

static const FSubcommandHelpEntry kTaskSubs[] = {
    {
        "set",
        "Usage: uni-plan task set --topic <T> --phase <N> --job <J>\n"
        "                         --task <K> [options]\n\n",
        "Update task status and evidence within a job.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n"
        "  --job <J>               Job index\n"
        "  --task <K>              Task index\n\n",
        "  --status <s>            not_started|in_progress|completed|blocked|\n"
        "                          dropped|canceled\n"
        "  --evidence <text>       Completion proof (commit SHA, URL, log)\n"
        "  --notes <text>          Agent working notes\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan task set --topic X --phase 2 --job 0 --task 1 \\\n"
        "                    --status completed --evidence 'commit abc123'\n",
        true,
        false,
    },
};

// ---------------------------------------------------------------------------
// changelog subcommands
// ---------------------------------------------------------------------------
static const FSubcommandHelpEntry kChangelogSubs[] = {
    {
        "query",
        "Usage: uni-plan changelog --topic <T> [--phase <N>] [--human]\n\n",
        "Query changelog entries. Default subcommand when no add/set/\n"
        "remove is supplied.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n\n",
        "  --phase <N>             Filter to a specific phase index\n",
        nullptr,
        "uni-plan-changelog-v2",
        "Examples:\n"
        "  uni-plan changelog --topic X\n"
        "  uni-plan changelog --topic X --phase 2 --human\n",
        false,
        true,
    },
    {
        "add",
        "Usage: uni-plan changelog add --topic <T> --change <text>\n"
        "                              [--change-file <path>]\n"
        "                              [--phase <N>]\n"
        "                              [--type <feat|fix|refactor|chore>]\n"
        "                              [--affected <text>]\n"
        "                              [--affected-file <path>]\n\n",
        "Append a changelog entry to the bundle. Date auto-stamped.\n"
        "--affected follows the canonical target format (e.g.\n"
        "phases[2].jobs[1] or changelogs[5]).\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --change <text>         What changed\n\n",
        "  --phase <N>             Scope to phase N (omit for topic-level)\n"
        "  --type <t>              feat | fix | refactor | chore\n"
        "  --affected <text>       Canonical mutation target\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan changelog add --topic X --phase 2 \\\n"
        "                         --change 'Implemented feature' --type feat\n"
        "  uni-plan changelog add --topic X --change 'Topic-level note' \\\n"
        "                         --affected plan\n",
        true,
        false,
    },
    {
        "set",
        "Usage: uni-plan changelog set --topic <T> --index <I> [options]\n\n",
        "Mutate an existing changelog entry by array index. Use for rare\n"
        "repair operations; prefer `changelog add` for normal flow.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --index <I>             Zero-based index into changelogs[]\n\n",
        "  --phase <N|topic>       Re-scope to phase N or 'topic' (empty)\n"
        "  --date <YYYY-MM-DD>     Override date\n"
        "  --change <text>         Replace change text\n"
        "  --type <t>              feat | fix | refactor | chore\n"
        "  --affected <text>       Replace canonical target\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan changelog set --topic X --index 3 \\\n"
        "                         --change 'Corrected prior entry'\n",
        true,
        false,
    },
    {
        "remove",
        "Usage: uni-plan changelog remove --topic <T> --index <I>\n\n",
        "Remove a changelog entry by array index. Rare repair operation;\n"
        "changelogs[] is conceptually append-only — removal breaks the\n"
        "audit trail. Use only when an entry was created in error (v0.74+).\n"
        "\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --index <I>             Zero-based index into changelogs[]\n\n",
        nullptr,
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan changelog remove --topic X --index 7\n",
        false,
        false,
    },
};

// ---------------------------------------------------------------------------
// verification subcommands
// ---------------------------------------------------------------------------
static const FSubcommandHelpEntry kVerificationSubs[] = {
    {
        "query",
        "Usage: uni-plan verification --topic <T> [--phase <N>] [--human]\n\n",
        "Query verification entries. Default subcommand when no add/set is\n"
        "supplied.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n\n",
        "  --phase <N>             Filter to a specific phase index\n",
        nullptr,
        "uni-plan-verification-v2",
        "Examples:\n"
        "  uni-plan verification --topic X\n"
        "  uni-plan verification --topic X --phase 2 --human\n",
        false,
        true,
    },
    {
        "add",
        "Usage: uni-plan verification add --topic <T> --check <text>\n"
        "                                 [--check-file <path>]\n"
        "                                 [--phase <N>]\n"
        "                                 [--result <text>]\n"
        "                                 [--detail <text>]\n"
        "                                 [--detail-file <path>]\n\n",
        "Append a verification entry to the bundle. Date auto-stamped.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --check <text>          What was verified\n\n",
        "  --phase <N>             Scope to phase N (omit for topic-level)\n"
        "  --result <text>         pass | fail | partial | string result\n"
        "  --detail <text>         Optional long-form detail / command log\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan verification add --topic X --phase 6 \\\n"
        "                            --check 'build.sh' --result pass\n"
        "  uni-plan verification add --topic X --check 'Peer review' \\\n"
        "                            --result partial --detail 'Open Q on "
        "ABI'\n",
        true,
        false,
    },
    {
        "set",
        "Usage: uni-plan verification set --topic <T> --index <I> [options]\n"
        "\n",
        "Mutate an existing verification entry by array index.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --index <I>             Zero-based index into verifications[]\n\n",
        "  --check <text>          Replace check text\n"
        "  --result <text>         Replace result\n"
        "  --detail <text>         Replace detail\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan verification set --topic X --index 2 \\\n"
        "                            --result 'pass after retry'\n",
        true,
        false,
    },
};

// ---------------------------------------------------------------------------
// lane subcommands (v0.85.0 Commit 3)
// ---------------------------------------------------------------------------
static const FSubcommandHelpEntry kLaneSubs[] = {
    {
        "set",
        "Usage: uni-plan lane set --topic <T> --phase <N> --lane <L>\n"
        "                         [options]\n\n",
        "Update an existing lane within a phase. Lanes group jobs that can\n"
        "execute in parallel within a wave.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n"
        "  --lane <L>              Lane index within the phase\n\n",
        "  --status <s>            not_started|in_progress|completed|blocked|\n"
        "                          dropped|canceled\n"
        "  --scope <text>          Lane scope\n"
        "  --exit-criteria <text>  Lane exit criteria\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan lane set --topic X --phase 2 --lane 0 --status completed\n",
        true,
        false,
    },
    {
        "add",
        "Usage: uni-plan lane add --topic <T> --phase <N> [options]\n\n",
        "Append a trailing lane to a phase.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n\n",
        "  --status <s>            Initial status (default: not_started)\n"
        "  --scope <text>          Lane scope\n"
        "  --exit-criteria <text>  Lane exit criteria\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan lane add --topic X --phase 2 --scope 'Integration'\n",
        true,
        false,
    },
};

// ---------------------------------------------------------------------------
// testing subcommands
// ---------------------------------------------------------------------------
static const FSubcommandHelpEntry kTestingSubs[] = {
    {
        "add",
        "Usage: uni-plan testing add --topic <T> --phase <N>\n"
        "                            --session <text> --step <text>\n"
        "                            --action <text> --expected <text>\n"
        "                            [--actor <human|ai|automated>]\n"
        "                            [--evidence <text>]\n\n",
        "Append a testing record to a phase. One record per (session,\n"
        "step) pair; use multiple records for branching test plans.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n"
        "  --session <text>        Session label (groups related steps)\n"
        "  --step <text>           Step label\n"
        "  --action <text>         What the tester does\n"
        "  --expected <text>       Expected result\n\n",
        "  --actor <t>             human | ai | automated (default: human)\n"
        "  --evidence <text>       Execution proof (log, SHA, URL)\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan testing add --topic X --phase 2 --session smoke \\\n"
        "                       --step 'build' --action 'run build.sh' \\\n"
        "                       --expected '0 errors' --actor ai\n",
        true,
        false,
    },
    {
        "set",
        "Usage: uni-plan testing set --topic <T> --phase <N> --index <I>\n"
        "                            [options]\n\n",
        "Update an existing testing record by array index.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n"
        "  --index <I>             Zero-based index into testing[]\n\n",
        "  --session <text>        Replace session label\n"
        "  --actor <t>             Replace actor: human | ai | automated\n"
        "  --step <text>           Replace step\n"
        "  --action <text>         Replace action\n"
        "  --expected <text>       Replace expected\n"
        "  --evidence <text>       Replace evidence\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan testing set --topic X --phase 2 --index 0 \\\n"
        "                       --evidence 'build.log sha=abc123'\n",
        true,
        false,
    },
};

// ---------------------------------------------------------------------------
// manifest subcommands
// ---------------------------------------------------------------------------
static const FSubcommandHelpEntry kManifestSubs[] = {
    {
        "add",
        "Usage: uni-plan manifest add --topic <T> --phase <N>\n"
        "                             --file <path>\n"
        "                             --action <create|modify|delete>\n"
        "                             --description <text>\n\n",
        "Append a file_manifest entry declaring a file the phase will\n"
        "create, modify, or delete. Used by readiness/validate to check\n"
        "plan↔disk consistency.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n"
        "  --file <path>           Repo-relative file path\n"
        "  --action <a>            create | modify | delete\n"
        "  --description <text>    What this file change achieves\n\n",
        nullptr,
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan manifest add --topic X --phase 2 \\\n"
        "                        --file Source/Foo.cpp --action create \\\n"
        "                        --description 'New command entry point'\n",
        true,
        false,
    },
    {
        "remove",
        "Usage: uni-plan manifest remove --topic <T> --phase <N> --index <I>\n"
        "\n",
        "Remove a file_manifest entry by array index.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n"
        "  --index <I>             Zero-based index into file_manifest[]\n\n",
        nullptr,
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan manifest remove --topic X --phase 2 --index 3\n",
        false,
        false,
    },
    {
        "list",
        "Usage: uni-plan manifest list [--topic <T>] [--phase <N>]\n"
        "                              [--missing-only] [--stale-plan]\n"
        "                              [--human]\n\n",
        "Enumerate file_manifest entries across the corpus with disk-\n"
        "reality checks. --missing-only and --stale-plan are orthogonal\n"
        "AND predicates — when both are set, a row must satisfy both.\n\n"
        "Drift classification (--stale-plan):\n"
        "  stale_create      action=create, file already exists on disk\n"
        "  stale_delete      action=delete, file still exists on disk\n"
        "  dangling_modify   action=modify, file does not exist on disk\n\n",
        nullptr,
        "  --topic <T>             Scope to a single topic\n"
        "  --phase <N>             Scope to a single phase index\n"
        "  --missing-only          Only rows whose file_path does not resolve\n"
        "  --stale-plan            Only rows where plan intent and disk "
        "disagree\n",
        nullptr,
        "uni-plan-list-v1",
        "Examples:\n"
        "  uni-plan manifest list --missing-only\n"
        "  uni-plan manifest list --topic X --stale-plan --human\n"
        "  uni-plan manifest list --topic X --missing-only --stale-plan\n",
        false,
        true,
    },
    {
        "set",
        "Usage: uni-plan manifest set --topic <T> --phase <N> --index <I>\n"
        "                             [options]\n\n",
        "Update an existing file_manifest entry by array index.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n"
        "  --index <I>             Zero-based index into file_manifest[]\n\n",
        "  --file <path>           Replace file path\n"
        "  --action <a>            create | modify | delete\n"
        "  --description <text>    Replace description\n",
        nullptr,
        "uni-plan-mutation-v1",
        "Examples:\n"
        "  uni-plan manifest set --topic X --phase 2 --index 0 \\\n"
        "                        --action modify\n",
        true,
        false,
    },
    {
        "suggest",
        "Usage: uni-plan manifest suggest --topic <T> --phase <N>\n"
        "                                 [--apply]\n\n",
        "Backfill helper (v0.86.0+). Scans `git log --name-status` over\n"
        "the phase's [started_at..completed_at] window and proposes\n"
        "file_manifest entries for files touched but not already\n"
        "recorded. Defaults to dry-run (read-only); pass --apply to\n"
        "actually invoke `manifest add` for each suggestion (reuses the\n"
        "auto-changelog + validation pipeline).\n\n"
        "Files already in the manifest are filtered out, so re-running\n"
        "after partial backfills is idempotent.\n\n"
        "Refuses with exit 1 when the phase has no started_at — backfill\n"
        "the lifecycle stamp first via `phase set --started-at <iso>` or\n"
        "run the phase through `phase start` / `phase complete`.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n"
        "  --phase <N>             Phase index\n\n",
        "  --apply                 Mutate the bundle: invoke `manifest add`\n"
        "                          for each suggestion. Default is dry-run\n"
        "                          (JSON output only, no bundle changes).\n",
        nullptr,
        "uni-plan-manifest-suggest-v1",
        "Examples:\n"
        "  uni-plan manifest suggest --topic X --phase 3            # dry-run\n"
        "  uni-plan manifest suggest --topic X --phase 3 --apply    # backfill\n",
        false,
        false,
    },
};

// ---------------------------------------------------------------------------
// cache subcommands — migrated from ad-hoc inline help block to the
// structured FSubcommandHelpEntry registry (v0.85.0 Commit 3).
// ---------------------------------------------------------------------------
static const FSubcommandHelpEntry kCacheSubs[] = {
    {
        "info",
        "Usage: uni-plan cache info [--human]\n\n",
        "Show cache directory, size, entry count, and configuration\n"
        "state. Default subcommand — `uni-plan cache` with no subcommand\n"
        "runs `cache info`.\n\n",
        nullptr,
        nullptr,
        nullptr,
        "uni-plan-cache-info-v1",
        "Examples:\n"
        "  uni-plan cache\n"
        "  uni-plan cache info --human\n",
        false,
        true,
    },
    {
        "clear",
        "Usage: uni-plan cache clear [--human]\n\n",
        "Remove all cached inventory data for all repositories. Safe to\n"
        "run; cache repopulates on the next invocation.\n\n",
        nullptr,
        nullptr,
        nullptr,
        "uni-plan-cache-clear-v1",
        "Examples:\n"
        "  uni-plan cache clear\n"
        "  uni-plan cache clear --human\n",
        false,
        true,
    },
    {
        "config",
        "Usage: uni-plan cache config [--dir <path>]\n"
        "                             [--enabled <true|false>]\n"
        "                             [--verbose <true|false>] [--human]\n"
        "\n",
        "Update cache settings in uni-plan.ini next to the binary.\n\n",
        nullptr,
        "  --dir <path>            Set cache directory (absolute, relative,\n"
        "                          or ${VAR})\n"
        "  --enabled <true|false>  Enable or disable inventory caching\n"
        "  --verbose <true|false>  Print cache hit/miss info to stderr\n",
        nullptr,
        "uni-plan-cache-config-v1",
        "Examples:\n"
        "  uni-plan cache config --dir /tmp/doc-cache\n"
        "  uni-plan cache config --enabled false\n"
        "  uni-plan cache config --verbose true\n",
        false,
        true,
    },
};

// ===========================================================================
// kCommandHelp — top-level group-level help registry.
//
// Group blocks now carry a compact subcommand index in mSpecificOptions
// (one line per subcommand with a one-liner description). Per-flag
// prose lives in the matching FSubcommandHelpEntry — agents reach it
// via `uni-plan <group> <sub> --help`. This closes the pre-v0.85.0
// "bleed" where `phase --help` listed all flags from every subcommand
// mashed together.
// ===========================================================================
static const FCommandHelpEntry kCommandHelp[] = {
    {
        "topic",
        "Usage:\n"
        "  uni-plan topic list [--status <filter>]\n"
        "  uni-plan topic get --topic <T> [--sections <csv>]\n"
        "  uni-plan topic set --topic <T> [options]\n"
        "  uni-plan topic start --topic <T>\n"
        "  uni-plan topic complete --topic <T> [--verification <text>]\n"
        "  uni-plan topic block --topic <T> --reason <text>\n"
        "  uni-plan topic status\n\n",
        "List, inspect, or update plan topics.\n\n",
        nullptr, // mRequiredOptions
        "  list          List topics with status + phase counts\n"
        "  get           Retrieve one topic's metadata + phase summary\n"
        "  set           Mutate topic-level fields\n"
        "  start         Transition topic to in_progress (semantic)\n"
        "  complete      Transition topic to completed (semantic)\n"
        "  block         Transition topic to blocked (semantic)\n"
        "  status        Aggregate overview of every topic\n\n"
        "Run `uni-plan topic <sub> --help` for per-subcommand detail.\n",
        kHumanTable, // mHumanLabel
        "Examples:\n"
        "  uni-plan topic list --status in_progress\n"
        "  uni-plan topic get --topic X --sections summary,phases\n"
        "  uni-plan topic status --human\n",
        kTopicSubs,
        sizeof(kTopicSubs) / sizeof(kTopicSubs[0]),
    },
    {
        "phase",
        "Usage:\n"
        "  uni-plan phase list --topic <T> [--status <filter>]\n"
        "  uni-plan phase get --topic <T> [--phase <N> | --phases <csv>]\n"
        "                                 [--brief|--design|--execution]\n"
        "  uni-plan phase set --topic <T> --phase <N> [options]\n"
        "  uni-plan phase add --topic <T> [--scope <t>] [--output <t>]\n"
        "  uni-plan phase remove --topic <T> --phase <N>\n"
        "  uni-plan phase normalize --topic <T> --phase <N> [--dry-run]\n"
        "  uni-plan phase start --topic <T> --phase <N> [--context <t>]\n"
        "  uni-plan phase complete --topic <T> --phase <N> --done <t>\n"
        "  uni-plan phase block --topic <T> --phase <N> --reason <t>\n"
        "  uni-plan phase unblock --topic <T> --phase <N>\n"
        "  uni-plan phase progress --topic <T> --phase <N>\n"
        "                         --done <t> --remaining <t>\n"
        "  uni-plan phase complete-jobs --topic <T> --phase <N>\n"
        "  uni-plan phase log --topic <T> --phase <N>\n"
        "                     --change <t> [--type <type>]\n"
        "  uni-plan phase verify --topic <T> --phase <N>\n"
        "                        --check <t> [--result <t>]\n"
        "  uni-plan phase next --topic <T>\n"
        "  uni-plan phase readiness --topic <T> --phase <N>\n"
        "  uni-plan phase wave-status --topic <T> --phase <N>\n"
        "  uni-plan phase drift [--topic <T>]\n\n",
        "List, inspect, or update phases within a topic.\n\n",
        nullptr,
        "  list             List phases with status + design-chars summary\n"
        "  get              Retrieve one or more phases (4 output modes)\n"
        "  set              Low-level field mutation\n"
        "  add              Append a trailing phase\n"
        "  remove           Remove the trailing phase (gated)\n"
        "  normalize        Replace dashes/quotes/NBSP in prose fields\n"
        "  start            Transition to in_progress + stamp started_at\n"
        "  complete         Transition to completed + stamp completed_at\n"
        "  block            Transition to blocked\n"
        "  unblock          Transition from blocked back to in_progress\n"
        "  progress         Update done + remaining atomically\n"
        "  complete-jobs    Bulk-mark every job as completed\n"
        "  log              Append a changelog entry scoped to the phase\n"
        "  verify           Append a verification entry scoped to the phase\n"
        "  next             Find the next not_started phase + readiness\n"
        "  readiness        Gate-by-gate readiness report\n"
        "  wave-status      Per-wave job completion rollup\n"
        "  drift            Report status-vs-evidence drift (v0.84.0+)\n\n"
        "Run `uni-plan phase <sub> --help` for per-subcommand detail.\n",
        kHumanTable,
        "Examples:\n"
        "  uni-plan phase list --topic X --status in_progress\n"
        "  uni-plan phase get --topic X --phase 6 --design\n"
        "  uni-plan phase get --topic X --phases 0,2,4 --execution\n"
        "  uni-plan phase start --topic X --phase 6\n"
        "  uni-plan phase drift --topic X --human\n",
        kPhaseSubs,
        sizeof(kPhaseSubs) / sizeof(kPhaseSubs[0]),
    },
    {
        "job",
        "Usage:\n"
        "  uni-plan job set --topic <T> --phase <N> --job <J> [options]\n\n",
        "Mutate job fields within a phase.\n\n",
        nullptr,
        "  set              Update job status / scope / output / criteria\n\n"
        "Run `uni-plan job set --help` for flag detail.\n",
        nullptr, // no --human renderer
        "Examples:\n"
        "  uni-plan job set --topic X --phase 2 --job 0 --status completed\n",
        kJobSubs,
        sizeof(kJobSubs) / sizeof(kJobSubs[0]),
    },
    {
        "task",
        "Usage:\n"
        "  uni-plan task set --topic <T> --phase <N> --job <J> --task <K>\n"
        "                    [options]\n\n",
        "Mutate task fields within a job.\n\n",
        nullptr,
        "  set              Update task status / evidence / notes\n\n"
        "Run `uni-plan task set --help` for flag detail.\n",
        nullptr,
        "Examples:\n"
        "  uni-plan task set --topic X --phase 2 --job 0 --task 1 \\\n"
        "                    --status completed --evidence 'commit abc'\n",
        kTaskSubs,
        sizeof(kTaskSubs) / sizeof(kTaskSubs[0]),
    },
    {
        "changelog",
        "Usage:\n"
        "  uni-plan changelog --topic <T> [--phase <N>]\n"
        "  uni-plan changelog add --topic <T> --change <t>\n"
        "                         [--phase <N>] [--type <t>] [--affected <t>]\n"
        "  uni-plan changelog set --topic <T> --index <I> [options]\n"
        "  uni-plan changelog remove --topic <T> --index <I>\n\n",
        "Query or mutate changelog entries.\n\n",
        nullptr,
        "  (default)        Query — list changelog entries\n"
        "  add              Append an entry (type: feat|fix|refactor|chore)\n"
        "  set              Mutate an entry by array index (rare repair)\n"
        "  remove           Delete an entry by array index (rare repair)\n\n"
        "Run `uni-plan changelog <sub> --help` for per-subcommand detail.\n",
        kHumanTable,
        "Examples:\n"
        "  uni-plan changelog --topic X --phase 2\n"
        "  uni-plan changelog add --topic X --phase 2 \\\n"
        "                         --change 'Implemented feature' --type feat\n",
        kChangelogSubs,
        sizeof(kChangelogSubs) / sizeof(kChangelogSubs[0]),
    },
    {
        "verification",
        "Usage:\n"
        "  uni-plan verification --topic <T> [--phase <N>]\n"
        "  uni-plan verification add --topic <T> --check <t>\n"
        "                            [--phase <N>] [--result <t>]\n"
        "                            [--detail <t>]\n"
        "  uni-plan verification set --topic <T> --index <I> [options]\n\n",
        "Query or mutate verification entries.\n\n",
        nullptr,
        "  (default)        Query — list verification entries\n"
        "  add              Append a verification entry\n"
        "  set              Mutate an entry by array index (rare repair)\n\n"
        "Run `uni-plan verification <sub> --help` for per-subcommand detail.\n",
        kHumanTable,
        "Examples:\n"
        "  uni-plan verification --topic X\n"
        "  uni-plan verification add --topic X --phase 6 \\\n"
        "                            --check 'build.sh' --result pass\n",
        kVerificationSubs,
        sizeof(kVerificationSubs) / sizeof(kVerificationSubs[0]),
    },
    {
        "timeline",
        "Usage: uni-plan timeline --topic <T> [--phase <N>]\n"
        "                         [--since <yyyy-mm-dd>]\n\n",
        "Chronological timeline of changelog + verification entries for a\n"
        "topic. Useful for progress reports and release notes.\n\n",
        "Required:\n"
        "  --topic <T>             Topic key\n\n",
        "  --phase <N>             Filter to a specific phase index\n"
        "  --since <date>          Emit only entries on/after this date\n",
        kHumanTable,
        "Examples:\n"
        "  uni-plan timeline --topic MultiPlatforming\n"
        "  uni-plan timeline --topic X --phase 6\n"
        "  uni-plan timeline --topic X --since 2026-04-01 --human\n",
    },
    {
        "blockers",
        "Usage: uni-plan blockers [--topic <T>] [--human]\n\n",
        "List phases with blocked status OR non-empty blockers prose.\n"
        "Scope is the whole corpus unless --topic is supplied.\n\n",
        nullptr,
        "  --topic <T>             Filter to a single topic\n",
        kHumanTable,
        "Examples:\n"
        "  uni-plan blockers\n"
        "  uni-plan blockers --topic MultiPlatforming --human\n",
    },
    {
        "validate",
        "Usage: uni-plan validate [--topic <T>] [--strict] [--human]\n\n",
        "Validate every .Plan.json bundle in the repo against the V4\n"
        "schema + content-hygiene evaluators (34 checks as of v0.75.0+).\n"
        "Emits per-issue JSON with topic / path / line / detail. Exit 1\n"
        "when ErrorMajor issues exist (or any ErrorMinor/Warning under\n"
        "--strict).\n\n",
        nullptr,
        "  --topic <T>             Validate a single topic only\n"
        "  --strict                Promote ErrorMinor + Warning to fail\n",
        kHumanTable,
        "Examples:\n"
        "  uni-plan validate\n"
        "  uni-plan validate --topic MultiPlatforming\n"
        "  uni-plan validate --strict --human\n",
    },
    {
        "legacy-gap",
        "Usage: uni-plan legacy-gap [--topic <T>] [--category <c>] [--human]\n"
        "\n",
        "Stateless per-phase parity audit between V3 .md artifacts and V4\n"
        "bundles. Discovers legacy .md files on disk at invoke time via\n"
        "filename convention (<Topic>.Plan.md, <Topic>.<PhaseKey>.\n"
        "Playbook.md, sidecars). Bundles carry no legacy path index —\n"
        "after the .md corpus is deleted every phase falls into\n"
        "legacy_absent / v4_only which is the correct steady state.\n\n"
        "Each phase is bucketed into one of 8 categories:\n"
        "  legacy_rich          legacy >= 150 LOC, V4 < 3000 chars (rebuild)\n"
        "  legacy_rich_matched  legacy >= 150 LOC, V4 >= 10000 chars\n"
        "  legacy_thin          legacy 50-149 LOC\n"
        "  legacy_stub          legacy < 50 LOC\n"
        "  legacy_absent        no legacy playbook\n"
        "  v4_only              V4 rich (>= 10000 chars, >= 3 jobs), no "
        "legacy\n"
        "  hollow_both          legacy stub AND V4 hollow on completed phase\n"
        "  drift                reserved for future semantic-overlap\n\n",
        nullptr,
        "  --topic <T>             Audit only this topic\n"
        "  --category <c>          Emit only rows matching this category\n",
        kHumanTable,
        "Examples:\n"
        "  uni-plan legacy-gap --human\n"
        "  uni-plan legacy-gap --topic CycleRefactor --human\n"
        "  uni-plan legacy-gap --category legacy_rich --human\n",
    },
    {
        "lane",
        "Usage:\n"
        "  uni-plan lane set --topic <T> --phase <N> --lane <L> [options]\n"
        "  uni-plan lane add --topic <T> --phase <N> [options]\n\n",
        "Manage lanes within a phase. Lanes group jobs that can execute\n"
        "in parallel within a wave.\n\n",
        nullptr,
        "  set              Update an existing lane's fields\n"
        "  add              Append a trailing lane to a phase\n\n"
        "Run `uni-plan lane <sub> --help` for flag detail.\n",
        nullptr,
        "Examples:\n"
        "  uni-plan lane add --topic X --phase 2 --scope 'Integration'\n",
        kLaneSubs,
        sizeof(kLaneSubs) / sizeof(kLaneSubs[0]),
    },
    {
        "testing",
        "Usage:\n"
        "  uni-plan testing add --topic <T> --phase <N>\n"
        "                       --session <t> --step <t>\n"
        "                       --action <t> --expected <t>\n"
        "                       [--actor <human|ai|automated>]\n"
        "                       [--evidence <t>]\n"
        "  uni-plan testing set --topic <T> --phase <N> --index <I> [options]\n"
        "\n",
        "Manage testing records within a phase.\n\n",
        nullptr,
        "  add              Append a new testing record\n"
        "  set              Update an existing testing record by index\n\n"
        "Run `uni-plan testing <sub> --help` for flag detail.\n",
        nullptr,
        "Examples:\n"
        "  uni-plan testing add --topic X --phase 2 --session smoke \\\n"
        "                       --step 'build' --action 'run build.sh' \\\n"
        "                       --expected '0 errors' --actor ai\n",
        kTestingSubs,
        sizeof(kTestingSubs) / sizeof(kTestingSubs[0]),
    },
    {
        "manifest",
        "Usage:\n"
        "  uni-plan manifest add --topic <T> --phase <N>\n"
        "                        --file <path> --action "
        "<create|modify|delete>\n"
        "                        --description <t>\n"
        "  uni-plan manifest remove --topic <T> --phase <N> --index <I>\n"
        "  uni-plan manifest list [--topic <T>] [--phase <N>]\n"
        "                         [--missing-only] [--stale-plan]\n"
        "  uni-plan manifest set --topic <T> --phase <N> --index <I> "
        "[options]\n"
        "\n",
        "Manage file_manifest entries declaring the files a phase will\n"
        "create, modify, or delete. Used by readiness/validate to check\n"
        "plan↔disk consistency.\n\n",
        nullptr,
        "  add              Append a new file_manifest entry\n"
        "  remove           Remove an entry by array index\n"
        "  list             Enumerate entries across the corpus (with\n"
        "                   --missing-only and --stale-plan drift filters)\n"
        "  set              Update an existing entry by index\n\n"
        "Run `uni-plan manifest <sub> --help` for flag detail.\n",
        kHumanTable,
        "Examples:\n"
        "  uni-plan manifest list --missing-only\n"
        "  uni-plan manifest list --topic X --stale-plan --human\n"
        "  uni-plan manifest add --topic X --phase 2 \\\n"
        "                        --file Source/Foo.cpp --action create \\\n"
        "                        --description 'New command entry point'\n",
        kManifestSubs,
        sizeof(kManifestSubs) / sizeof(kManifestSubs[0]),
    },
    {
        "cache",
        "Usage:\n"
        "  uni-plan cache [info]\n"
        "  uni-plan cache clear\n"
        "  uni-plan cache config [--dir <path>] [--enabled <true|false>]\n"
        "                        [--verbose <true|false>]\n\n",
        "Manage the persisted inventory cache.\n\n"
        "uni-plan scans the repository to build a documentation inventory.\n"
        "This scan is cached to avoid repeating it on every invocation.\n"
        "The cache is automatically invalidated when any markdown file is\n"
        "added, removed, resized, or modified (tracked via FNV-1a hash of\n"
        "file paths, sizes, and timestamps).\n\n"
        "Cache location: ~/.uni-plan/cache/<repo-hash>/inventory.cache\n"
        "Each repository gets its own entry keyed by a hash of the repo\n"
        "path.\n\n",
        nullptr,
        "  info             Show cache state (default — bare `cache` runs "
        "info)\n"
        "  clear            Remove all cached data for all repositories\n"
        "  config           Update settings in uni-plan.ini\n\n"
        "Run `uni-plan cache <sub> --help` for flag detail.\n",
        kHumanTable,
        "Examples:\n"
        "  uni-plan cache\n"
        "  uni-plan cache clear --human\n"
        "  uni-plan cache config --dir /tmp/doc-cache\n",
        kCacheSubs,
        sizeof(kCacheSubs) / sizeof(kCacheSubs[0]),
    },
#ifdef UPLAN_WATCH
    {
        "watch",
        "Usage: uni-plan watch [--repo-root <path>]\n\n",
        "Launch the FTXUI terminal dashboard (watch mode). Live view of\n"
        "active topics, phase timeline, blockers, and design-chars. Press\n"
        "q to quit.\n\n"
        "Built only when uni-plan is compiled with -DUPLAN_WATCH=1\n"
        "(default on).\n\n",
        nullptr, // required
        nullptr, // options (no per-command flags — only --repo-root)
        nullptr, // mHumanLabel — watch is always human
        "Examples:\n"
        "  uni-plan watch\n"
        "  uni-plan watch --repo-root /path/to/repo\n",
    },
#endif
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
//   matching entry         when the subcommand isn't in the registry yet).
// Unknown command        → fall back to PrintUsage() global.
// ---------------------------------------------------------------------------
static void PrintSubcommandBlock(std::ostream &Out,
                                 const FSubcommandHelpEntry &InSub);

void PrintCommandUsage(std::ostream &Out, const std::string &InCommand,
                       const std::string &InSubcommand)
{
    // Data-driven: standard command help
    for (const FCommandHelpEntry &Entry : kCommandHelp)
    {
        if (InCommand != Entry.mName)
        {
            continue;
        }
        // Subcommand-level lookup — only if the entry has a subcommand
        // table registered.
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
        // Group block (or leaf block for single-command groups:
        // timeline, blockers, validate, legacy-gap).
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
        static const std::set<std::string> kProseCommands = {
            "topic", "phase", "job", "task", "changelog", "verification"};
        if (kProseCommands.count(Entry.mName) > 0)
        {
            Out << kFileFlagFooter;
        }
        // Null-guard mHumanLabel: `job`/`task` carry nullptr.
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
