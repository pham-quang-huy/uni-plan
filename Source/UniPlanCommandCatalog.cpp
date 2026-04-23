#include "UniPlanCliConstants.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanJSONHelpers.h"
#include "UniPlanOptionTypes.h"
#include "UniPlanStringHelpers.h"

#include <iostream>
#include <string>
#include <vector>

namespace UniPlan
{

// ===================================================================
// _catalog — machine-readable CLI surface (v0.93.0)
//
// Emits the full verb/subcommand/flag/schema matrix as JSON so external
// tooling can cross-check skill recipes, help strings, and documented
// flags against the shipping CLI without parsing human-oriented help
// prose. Primary consumer: FIE's `fie_skill_recipe_lint.py` — the
// mechanical guard against the silent-workaround pattern that
// `.claude/rules/cli-gap-discipline.md` codifies.
//
// Why the catalog is hand-maintained: the parser-side flag list is
// scattered across ~30 ParseXxxOptions functions, each with bespoke
// required-check logic. Extracting flags mechanically from the parsers
// would require a non-trivial AST pass or a macro-registration shim.
// The catalog below is the single source of truth for "what flags
// ship"; every CLI surface addition should land a catalog entry in
// the same commit. The `catalog_vs_dispatch_smoke` test locks the
// top-level verb list against UniPlanCommandDispatch.cpp.
// ===================================================================

// One subcommand entry: name + schema + required/optional flag names
// (WITHOUT the leading `--`). Parsers accept paired `--<field>` and
// `--<field>-file` for every prose-style flag; the catalog lists only
// the base name and records which fields accept the `-file` variant
// via mProseFlags.
struct FCatalogSub
{
    const char *mName;
    const char *mOutputSchema;
    // Required flags (without leading --). Empty = no required flag
    // beyond --topic (most queries) or none at all (utility cmds).
    std::vector<std::string> mRequiredFlags;
    // Optional flags (without leading --). May overlap with prose flags
    // below when the flag also accepts a --<name>-file variant.
    std::vector<std::string> mOptionalFlags;
    // Subset of mOptionalFlags (or mRequiredFlags) that accept the
    // `-file` suffix variant per TryConsumeStringOrFileOption. Listed
    // here so the linter can resolve `--<name>-file` → valid without a
    // full shape rewrite.
    std::vector<std::string> mProseFlags;
    // Exit codes the subcommand emits. Convention:
    //   0 = success, 1 = runtime error, 2 = usage error.
    std::vector<int> mExitCodes;
};

struct FCatalogVerb
{
    const char *mName;
    const char *mDescription;
    std::vector<FCatalogSub> mSubs;
};

// Emit a JSON array of strings WITHOUT a trailing comma on the last
// element. Used for flag lists and exit-code lists inside each
// FCatalogSub record. Separated so both helpers share the same logic.
static void EmitStringArray(const std::vector<std::string> &InItems)
{
    std::cout << "[";
    for (size_t I = 0; I < InItems.size(); ++I)
    {
        if (I > 0)
            std::cout << ",";
        std::cout << JSONQuote(InItems[I]);
    }
    std::cout << "]";
}

static void EmitIntArray(const std::vector<int> &InItems)
{
    std::cout << "[";
    for (size_t I = 0; I < InItems.size(); ++I)
    {
        if (I > 0)
            std::cout << ",";
        std::cout << InItems[I];
    }
    std::cout << "]";
}

// Register every verb + sub that the dispatcher in
// UniPlanCommandDispatch.cpp routes to. Keep this ordered identically
// to kCommands[] there so the smoke-test line-by-line diff stays
// trivial. Add new entries at the bottom.
static std::vector<FCatalogVerb> BuildCatalog()
{
    std::vector<FCatalogVerb> Verbs;

    // ---- topic ----
    Verbs.push_back(
        {"topic",
         "List, inspect, and mutate plan topics.",
         {{"list", kTopicListSchema, {}, {"status", "human"}, {}, {0, 2}},
          {"get",
           kTopicGetSchema,
           {"topic"},
           {"sections", "human"},
           {},
           {0, 2}},
          {"status", kTopicListSchema, {}, {"human"}, {}, {0, 2}},
          // v0.94.0 — create a brand-new .Plan.json bundle file.
          {"add",
           kMutationSchema,
           {"topic", "title"},
           {"status", "summary", "goals", "non-goals", "problem-statement",
            "baseline-audit", "execution-strategy", "locked-decisions",
            "source-references"},
           {"title", "summary", "goals", "non-goals", "problem-statement",
            "baseline-audit", "execution-strategy", "locked-decisions",
            "source-references"},
           {0, 1, 2}},
          {"set",
           kMutationSchema,
           {"topic"},
           {"status", "summary", "goals", "non-goals", "problem-statement",
            "validation-commands", "validation-clear", "validation-add",
            "baseline-audit", "execution-strategy", "locked-decisions",
            "source-references", "dependency-clear", "dependency-add"},
           {"summary", "goals", "non-goals", "problem-statement",
            "validation-commands", "baseline-audit", "execution-strategy",
            "locked-decisions", "source-references"},
           {0, 1, 2}},
          {"normalize", kMutationSchema, {"topic"}, {"dry-run"}, {}, {0, 1, 2}},
          {"start", kMutationSchema, {"topic"}, {}, {}, {0, 1, 2}},
          {"complete",
           kMutationSchema,
           {"topic"},
           {"verification"},
           {"verification"},
           {0, 1, 2}},
          {"block",
           kMutationSchema,
           {"topic"},
           {"reason"},
           {"reason"},
           {0, 1, 2}}}});

    // ---- phase ----
    Verbs.push_back(
        {"phase",
         "List, inspect, and mutate phases within a topic.",
         {{"list",
           kPhaseListSchemaV2,
           {"topic"},
           {"status", "human"},
           {},
           {0, 2}},
          {"get",
           kPhaseGetSchema,
           {"topic"},
           {"phase", "phases", "brief", "design", "execution", "human"},
           {},
           {0, 2}},
          {"metric",
           kPhaseMetricSchema,
           {"topic"},
           {"phase", "phases", "status", "human"},
           {},
           {0, 1, 2}},
          {"next", kPhaseGetSchema, {"topic"}, {}, {}, {0, 2}},
          {"readiness", kPhaseGetSchema, {"topic", "phase"}, {}, {}, {0, 2}},
          {"wave-status", kPhaseGetSchema, {"topic", "phase"}, {}, {}, {0, 2}},
          {"drift", kPhaseDriftSchema, {}, {"topic"}, {}, {0, 2}},
          {"set",
           kMutationSchema,
           {"topic", "phase"},
           {"status",
            "context",
            "done",
            "done-clear",
            "remaining",
            "remaining-clear",
            "blockers",
            "blockers-clear",
            "scope",
            "output",
            "investigation",
            "code-entity-contract",
            "code-snippets",
            "best-practices",
            "multi-platforming",
            "readiness-gate",
            "handoff",
            "validation-commands",
            "validation-clear",
            "validation-add",
            "dependency-clear",
            "dependency-add",
            "started-at",
            "completed-at",
            "origin",
            "no-file-manifest",
            "no-file-manifest-reason",
            "no-file-manifest-reason-clear"},
           {"context", "done", "remaining", "blockers", "scope", "output",
            "investigation", "code-entity-contract", "code-snippets",
            "best-practices", "multi-platforming", "readiness-gate", "handoff",
            "validation-commands", "no-file-manifest-reason"},
           {0, 1, 2}},
          {"add",
           kMutationSchema,
           {"topic"},
           {"scope", "output", "status"},
           {"scope", "output"},
           {0, 1, 2}},
          {"remove", kMutationSchema, {"topic", "phase"}, {}, {}, {0, 1, 2}},
          {"normalize",
           kMutationSchema,
           {"topic", "phase"},
           {"dry-run"},
           {},
           {0, 1, 2}},
          {"start",
           kMutationSchema,
           {"topic", "phase"},
           {"context"},
           {"context"},
           {0, 1, 2}},
          {"complete",
           kMutationSchema,
           {"topic", "phase"},
           {"done", "verification"},
           {"done", "verification"},
           {0, 1, 2}},
          {"block",
           kMutationSchema,
           {"topic", "phase"},
           {"reason"},
           {"reason"},
           {0, 1, 2}},
          {"unblock", kMutationSchema, {"topic", "phase"}, {}, {}, {0, 1, 2}},
          {"cancel",
           kMutationSchema,
           {"topic", "phase"},
           {"reason"},
           {"reason"},
           {0, 1, 2}},
          {"progress",
           kMutationSchema,
           {"topic", "phase", "done", "remaining"},
           {},
           {"done", "remaining"},
           {0, 1, 2}},
          {"complete-jobs",
           kMutationSchema,
           {"topic", "phase"},
           {},
           {},
           {0, 1, 2}},
          {"log",
           kMutationSchema,
           {"topic", "phase", "change"},
           {"type", "affected"},
           {"change", "affected"},
           {0, 1, 2}},
          {"verify",
           kMutationSchema,
           {"topic", "phase", "check"},
           {"result", "detail"},
           {"check", "result", "detail"},
           {0, 1, 2}}}});

    // ---- changelog ----
    Verbs.push_back(
        {"changelog",
         "Query or mutate changelog entries.",
         {{"query",
           kChangelogSchemaV2,
           {"topic"},
           {"phase", "human"},
           {},
           {0, 2}},
          {"add",
           kMutationSchema,
           {"topic", "change"},
           {"phase", "type", "affected"},
           {"change", "affected"},
           {0, 1, 2}},
          {"set",
           kMutationSchema,
           {"topic", "index"},
           {"phase", "date", "change", "type", "affected"},
           {"change", "affected"},
           {0, 1, 2}},
          {"remove", kMutationSchema, {"topic", "index"}, {}, {}, {0, 1, 2}}}});

    // ---- verification ----
    Verbs.push_back({"verification",
                     "Query or mutate verification entries.",
                     {{"query",
                       kVerificationSchemaV2,
                       {"topic"},
                       {"phase", "human"},
                       {},
                       {0, 2}},
                      {"add",
                       kMutationSchema,
                       {"topic", "check"},
                       {"phase", "result", "detail"},
                       {"check", "result", "detail"},
                       {0, 1, 2}},
                      {"set",
                       kMutationSchema,
                       {"topic", "index"},
                       {"check", "result", "detail"},
                       {"check", "result", "detail"},
                       {0, 1, 2}}}});

    // ---- timeline / blockers / validate / legacy-gap ----
    Verbs.push_back({"timeline",
                     "Chronological merge of changelog + verification.",
                     {{"query",
                       kTimelineSchema,
                       {"topic"},
                       {"phase", "since", "human"},
                       {},
                       {0, 2}}}});
    Verbs.push_back(
        {"blockers",
         "List phases with blocked status or non-empty blockers prose.",
         {{"query", kBlockersSchema, {}, {"topic", "human"}, {}, {0, 2}}}});
    Verbs.push_back(
        {"validate",
         "Validate every bundle against schema + content-hygiene evaluators.",
         {{"query",
           kValidateSchema,
           {},
           {"topic", "strict", "human"},
           {},
           {0, 1, 2}}}});
    Verbs.push_back({"legacy-gap",
                     "Stateless V3 <-> V4 parity audit (historical).",
                     {{"query",
                       kLegacyGapSchema,
                       {},
                       {"topic", "category", "human"},
                       {},
                       {0, 2}}}});

    // ---- job ----
    Verbs.push_back(
        {"job",
         "Manage jobs within a phase.",
         {{"add",
           kMutationSchema,
           {"topic", "phase"},
           {"status", "scope", "output", "exit-criteria", "lane", "wave"},
           {"scope", "output", "exit-criteria"},
           {0, 1, 2}},
          {"set",
           kMutationSchema,
           {"topic", "phase", "job"},
           {"status", "scope", "output", "exit-criteria", "lane", "wave"},
           {"scope", "output", "exit-criteria"},
           {0, 1, 2}},
          {"remove",
           kMutationSchema,
           {"topic", "phase", "job"},
           {},
           {},
           {0, 1, 2}},
          {"list", kListSchema, {"topic"}, {"phase"}, {}, {0, 2}}}});

    // ---- task ----
    Verbs.push_back(
        {"task",
         "Manage tasks within a job.",
         {{"add",
           kMutationSchema,
           {"topic", "phase", "job", "description"},
           {"status", "evidence", "notes"},
           {"description", "evidence", "notes"},
           {0, 1, 2}},
          {"set",
           kMutationSchema,
           {"topic", "phase", "job", "task"},
           {"status", "evidence", "notes"},
           {"evidence", "notes"},
           {0, 1, 2}},
          {"remove",
           kMutationSchema,
           {"topic", "phase", "job", "task"},
           {},
           {},
           {0, 1, 2}},
          {"list", kListSchema, {"topic"}, {"phase", "job"}, {}, {0, 2}}}});

    // ---- lane ----
    Verbs.push_back(
        {"lane",
         "Manage lanes within a phase.",
         {{"add",
           kMutationSchema,
           {"topic", "phase"},
           {"status", "scope", "exit-criteria"},
           {"scope", "exit-criteria"},
           {0, 1, 2}},
          {"set",
           kMutationSchema,
           {"topic", "phase", "lane"},
           {"status", "scope", "exit-criteria"},
           {"scope", "exit-criteria"},
           {0, 1, 2}},
          {"remove",
           kMutationSchema,
           {"topic", "phase", "lane"},
           {},
           {},
           {0, 1, 2}},
          {"list", kListSchema, {"topic"}, {"phase"}, {}, {0, 2}}}});

    // ---- testing ----
    Verbs.push_back(
        {"testing",
         "Manage testing records within a phase.",
         {{"add",
           kMutationSchema,
           {"topic", "phase", "session", "step", "action", "expected"},
           {"actor", "evidence"},
           {"session", "step", "action", "expected", "evidence"},
           {0, 1, 2}},
          {"set",
           kMutationSchema,
           {"topic", "phase", "index"},
           {"session", "actor", "step", "action", "expected", "evidence"},
           {"session", "step", "action", "expected", "evidence"},
           {0, 1, 2}},
          {"remove",
           kMutationSchema,
           {"topic", "phase", "index"},
           {},
           {},
           {0, 1, 2}},
          {"list", kListSchema, {"topic"}, {"phase"}, {}, {0, 2}}}});

    // ---- manifest ----
    Verbs.push_back(
        {"manifest",
         "Manage file_manifest entries (declared file touches per phase).",
         {{"add",
           kMutationSchema,
           {"topic", "phase", "file", "action", "description"},
           {},
           {"description"},
           {0, 1, 2}},
          {"set",
           kMutationSchema,
           {"topic", "phase", "index"},
           {"file", "action", "description"},
           {"description"},
           {0, 1, 2}},
          {"remove",
           kMutationSchema,
           {"topic", "phase", "index"},
           {},
           {},
           {0, 1, 2}},
          {"list",
           kListSchema,
           {},
           {"topic", "phase", "missing-only", "stale-plan", "human"},
           {},
           {0, 2}},
          {"suggest",
           kManifestSuggestSchema,
           {"topic", "phase"},
           {"apply"},
           {},
           {0, 1, 2}}}});

    // ---- risk / next-action / acceptance-criterion ----
    Verbs.push_back(
        {"risk",
         "Manage typed risk entries.",
         {{"add",
           kMutationSchema,
           {"topic", "statement"},
           {"id", "mitigation", "severity", "status", "notes"},
           {"statement", "mitigation", "notes"},
           {0, 1, 2}},
          {"set",
           kMutationSchema,
           {"topic", "index"},
           {"id", "statement", "mitigation", "severity", "status", "notes"},
           {"statement", "mitigation", "notes"},
           {0, 1, 2}},
          {"remove", kMutationSchema, {"topic", "index"}, {}, {}, {0, 1, 2}},
          {"list",
           kListSchema,
           {"topic"},
           {"severity", "status"},
           {},
           {0, 2}}}});
    Verbs.push_back(
        {"next-action",
         "Manage typed next-action entries.",
         {{"add",
           kMutationSchema,
           {"topic", "statement"},
           {"order", "rationale", "owner", "status", "target-date"},
           {"statement", "rationale"},
           {0, 1, 2}},
          {"set",
           kMutationSchema,
           {"topic", "index"},
           {"order", "statement", "rationale", "owner", "status",
            "target-date"},
           {"statement", "rationale"},
           {0, 1, 2}},
          {"remove", kMutationSchema, {"topic", "index"}, {}, {}, {0, 1, 2}},
          {"list", kListSchema, {"topic"}, {"status"}, {}, {0, 2}}}});
    Verbs.push_back(
        {"acceptance-criterion",
         "Manage typed acceptance-criterion entries.",
         {{"add",
           kMutationSchema,
           {"topic", "statement"},
           {"id", "status", "measure", "evidence"},
           {"statement", "measure", "evidence"},
           {0, 1, 2}},
          {"set",
           kMutationSchema,
           {"topic", "index"},
           {"id", "statement", "status", "measure", "evidence"},
           {"statement", "measure", "evidence"},
           {0, 1, 2}},
          {"remove", kMutationSchema, {"topic", "index"}, {}, {}, {0, 1, 2}},
          {"list", kListSchema, {"topic"}, {"status"}, {}, {0, 2}}}});

    // ---- migrate / cache / watch / _catalog ----
    Verbs.push_back(
        {"migrate",
         "Eager bundle normalization after typed-array schema change.",
         {{"query", kMutationSchema, {}, {"topic"}, {}, {0, 1, 2}}}});
    Verbs.push_back({"cache",
                     "Cache directory / size / config.",
                     {{"info", kCacheInfoSchema, {}, {"human"}, {}, {0, 2}},
                      {"clear", kCacheClearSchema, {}, {"human"}, {}, {0, 2}},
                      {"config",
                       kCacheConfigSchema,
                       {},
                       {"dir", "enabled", "verbose", "human"},
                       {},
                       {0, 2}}}});
    Verbs.push_back(
        {"watch",
         "Terminal dashboard of every topic's current state.",
         {{"query", kMutationSchema, {}, {"repo-root"}, {}, {0, 1, 2}}}});
    Verbs.push_back({"_catalog",
                     "Machine-readable CLI surface dump (v0.93.0+).",
                     {{"query", kCatalogSchema, {}, {}, {}, {0}}}});

    return Verbs;
}

int RunCatalogCommand(const std::vector<std::string> &InArgs,
                      const std::string & /*InRepoRoot*/)
{
    // --help is already captured by DispatchSubcommand's --help probe,
    // but _catalog also accepts --json (a no-op today since JSON is the
    // only output form). Reject anything else.
    for (const std::string &Token : InArgs)
    {
        if (Token == "--json")
            continue;
        throw UsageError("Unknown option for _catalog: " + Token);
    }

    const std::vector<FCatalogVerb> Verbs = BuildCatalog();
    const std::string UTC = GetUtcNow();

    std::cout << "{";
    EmitJsonField("schema", kCatalogSchema);
    EmitJsonFieldInt("catalog_schema_version", kCatalogSchemaVersion);
    EmitJsonField("cli_version", kCliVersion);
    EmitJsonField("generated_utc", UTC);
    std::cout << "\"verbs\":[";
    for (size_t V = 0; V < Verbs.size(); ++V)
    {
        if (V > 0)
            std::cout << ",";
        const FCatalogVerb &Verb = Verbs[V];
        std::cout << "{";
        EmitJsonField("name", Verb.mName);
        EmitJsonField("description", Verb.mDescription);
        std::cout << "\"subcommands\":[";
        for (size_t S = 0; S < Verb.mSubs.size(); ++S)
        {
            if (S > 0)
                std::cout << ",";
            const FCatalogSub &Sub = Verb.mSubs[S];
            std::cout << "{";
            EmitJsonField("name", Sub.mName);
            EmitJsonField("output_schema_name",
                          Sub.mOutputSchema ? Sub.mOutputSchema : "");
            std::cout << "\"required_flags\":";
            EmitStringArray(Sub.mRequiredFlags);
            std::cout << ",\"optional_flags\":";
            EmitStringArray(Sub.mOptionalFlags);
            std::cout << ",\"prose_flags\":";
            EmitStringArray(Sub.mProseFlags);
            std::cout << ",\"exit_codes\":";
            EmitIntArray(Sub.mExitCodes);
            std::cout << "}";
        }
        std::cout << "]}";
    }
    std::cout << "]}\n";
    return 0;
}

} // namespace UniPlan
