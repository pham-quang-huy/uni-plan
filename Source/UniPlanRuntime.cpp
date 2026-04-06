#include "UniPlanRuntime.h"
#include "UniPlanForwardDecls.h"
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

namespace UniPlan {

// kMarkdownPathRegex moved to DocParsing.cpp (declared in DocForwardDecls.h)

CacheConfigResult WriteCacheConfig(const std::string &InRepoRoot,
                                   const CacheConfigOptions &InOptions,
                                   const DocConfig &InCurrentConfig) {
  CacheConfigResult Result;
  Result.mGeneratedUtc = GetUtcNow();

  const fs::path ExeDir = ResolveExecutableDirectory();
  const fs::path IniPath = ExeDir / "uni-plan.ini";
  Result.mIniPath = IniPath.string();

  // Start with current effective values
  std::string EffectiveDir = InCurrentConfig.mCacheDir;
  std::string EffectiveEnabled =
      InCurrentConfig.mbCacheEnabled ? "true" : "false";
  std::string EffectiveVerbose =
      InCurrentConfig.mbCacheVerbose ? "true" : "false";

  // Merge only explicitly-set fields (mbDirSet distinguishes "not passed" from
  // "set to empty")
  if (InOptions.mbDirSet) {
    EffectiveDir = InOptions.mDir;
  }
  if (!InOptions.mEnabled.empty()) {
    EffectiveEnabled = InOptions.mEnabled;
  }
  if (!InOptions.mVerbose.empty()) {
    EffectiveVerbose = InOptions.mVerbose;
  }

  std::string WriteError;
  if (!TryWriteDocIni(IniPath, EffectiveDir, EffectiveEnabled, EffectiveVerbose,
                      WriteError)) {
    Result.mbSuccess = false;
    Result.mError = WriteError;
  }

  // Re-read to get effective config
  const DocConfig NewConfig = LoadConfig(ExeDir);
  Result.mDir = NewConfig.mCacheDir;
  Result.mbEnabled = NewConfig.mbCacheEnabled;
  Result.mbVerbose = NewConfig.mbCacheVerbose;

  return Result;
}

// ---------------------------------------------------------------------------
// Phase list all — collects phases from every plan in the inventory
// ---------------------------------------------------------------------------

void PrintUsage() {
  std::cout << "Usage:\n";
  std::cout << "  uni-plan --version\n";
  std::cout << "  uni-plan --help\n";
  std::cout << "  uni-plan list --type <plan|playbook|implementation|pair> "
               "[--status <[!]filter>] [--repo-root <path>]\n";
  std::cout << "  uni-plan phase [list] --topic <topic|path> [--status "
               "<[!]filter>] [--repo-root <path>]\n";
  std::cout
      << "  uni-plan phase list [--status <[!]filter>] [--repo-root <path>]\n";
  std::cout << "  uni-plan lint [--repo-root <path>] [--fail-on-warning]\n";
  std::cout << "  uni-plan inventory [--repo-root <path>]\n";
  std::cout << "  uni-plan orphan-check [--repo-root <path>]\n";
  std::cout
      << "  uni-plan artifacts --topic <topic|path> [--type "
         "<all|plan|playbook|implementation|sidecar>] [--repo-root <path>]\n";
  std::cout << "  uni-plan changelog --topic <topic|path> --for "
               "<plan|implementation|playbook> [--phase <phase-key>] "
               "[--repo-root <path>]\n";
  std::cout << "  uni-plan verification --topic <topic|path> --for "
               "<plan|implementation|playbook> [--phase <phase-key>] "
               "[--repo-root <path>]\n";
  std::cout << "  uni-plan schema --type <doc|plan|playbook|implementation> "
               "[--repo-root <path>]\n";
  std::cout << "  uni-plan rules [--repo-root <path>]\n";
  std::cout << "  uni-plan validate [--repo-root <path>] [--strict]\n";
  std::cout << "  uni-plan section resolve --doc <path> --section "
               "<section-id|heading> [--repo-root <path>]\n";
  std::cout << "  uni-plan section schema [--type "
               "<doc|plan|playbook|implementation>] [--repo-root <path>]\n";
  std::cout
      << "  uni-plan section content --doc <path> --id <section-id|heading> "
         "[--line-char-limit <n>] [--repo-root <path>]\n";
  std::cout << "  uni-plan section list [--doc <path>] [--count] [--repo-root "
               "<path>]\n";
  std::cout << "  uni-plan excerpt --doc <path> --section <section-id|heading> "
               "[--context-lines <n>] [--repo-root <path>]\n";
  std::cout << "  uni-plan table list --doc <path> [--repo-root <path>]\n";
  std::cout << "  uni-plan table get --doc <path> --table-id <id> [--repo-root "
               "<path>]\n";
  std::cout << "  uni-plan graph --topic <topic|path> [--depth <n>] "
               "[--repo-root <path>]\n";
  std::cout << "  uni-plan diagnose drift [--repo-root <path>]\n";
  std::cout << "  uni-plan timeline --topic <topic|path> [--since "
               "<yyyy-mm-dd>] [--repo-root <path>]\n";
  std::cout << "  uni-plan blockers [--status <open|blocked|all>] [--repo-root "
               "<path>]\n";
  std::cout << "  uni-plan cache [info] [--repo-root <path>]\n";
  std::cout << "  uni-plan cache clear [--repo-root <path>]\n";
  std::cout << "  uni-plan cache config --dir <path> [--enabled <true|false>] "
               "[--verbose <true|false>]\n";
  std::cout << "Output is JSON by default.\n";
  std::cout << "Global options:\n";
  std::cout << "  --human       Output formatted tables with ANSI color for "
               "terminal reading.\n";
  std::cout << "  --no-cache    Disable persisted inventory cache for this "
               "invocation.\n";
}

// FCommandHelpEntry + kHuman* constants moved to DocTypes.h

static const FCommandHelpEntry kCommandHelp[] = {
    {"list",
     "Usage: uni-plan list --type <plan|playbook|implementation|pair> "
     "[options]\n\n",
     "List discovered documents of a given type.\n\n",
     "Required:\n"
     "  --type <type>           Document type: plan, playbook, implementation, "
     "pair\n\n",
     "  --status <filter>       Filter by status. Prefix with ! to exclude "
     "(e.g. !completed).\n"
     "                          Values: "
     "all|not_started|in_progress|completed|closed|blocked|canceled|unknown "
     "[default: all]\n",
     kHumanTable,
     "Examples:\n"
     "  uni-plan list --type plan\n"
     "  uni-plan list --type plan --status in_progress\n"
     "  uni-plan list --type plan --status !completed\n"
     "  uni-plan list --type pair --status completed\n"
     "  uni-plan list --type playbook --human\n"},
    // "phase" is special-cased in PrintCommandUsage (multi-mode layout)
    {"lint", "Usage: uni-plan lint [options]\n\n",
     "Run documentation lint checks on the repository.\n\n", nullptr,
     "  --fail-on-warning       Exit with code 1 if any warnings are found\n"
     "  --strict                Alias for --fail-on-warning\n",
     kHumanList,
     "Examples:\n"
     "  uni-plan lint\n"
     "  uni-plan lint --strict\n"},
    {"inventory", "Usage: uni-plan inventory [options]\n\n",
     "List all discovered documentation files with metadata.\n\n", nullptr,
     nullptr, kHumanTable,
     "Examples:\n"
     "  uni-plan inventory\n"
     "  uni-plan inventory\n"},
    {"orphan-check", "Usage: uni-plan orphan-check [options]\n\n",
     "Find documentation files not linked to any known topic.\n\n", nullptr,
     nullptr, kHumanList,
     "Examples:\n"
     "  uni-plan orphan-check\n"
     "  uni-plan orphan-check\n"},
    {"artifacts",
     "Usage: uni-plan artifacts --topic <topic|path> [options]\n\n",
     "Show all artifacts (plan, playbook, implementation, sidecar) for a "
     "topic.\n\n",
     "Required:\n"
     "  --topic <topic|path>    Topic key or plan file path\n\n",
     "  --type <filter>         Filter: "
     "all|plan|playbook|implementation|sidecar [default: all]\n",
     kHumanList,
     "Examples:\n"
     "  uni-plan artifacts --topic DOC-TOOL-CLI-SCHEMA\n"
     "  uni-plan artifacts --topic DOC-TOOL-CLI-SCHEMA --type playbook\n"},
    {"changelog",
     "Usage: uni-plan changelog --topic <topic|path> --for "
     "<plan|implementation|playbook> [options]\n\n",
     "Show changelog entries for a topic's document.\n\n",
     "Required:\n"
     "  --topic <topic|path>    Topic key or plan file path\n"
     "  --for <type>            Document type: plan, implementation, "
     "playbook\n\n",
     "  --phase <phase-key>     Required when --for playbook\n", kHumanList,
     "Examples:\n"
     "  uni-plan changelog --topic DOC-TOOL-CLI-SCHEMA --for plan\n"
     "  uni-plan changelog --topic DOC-TOOL-CLI-SCHEMA --for playbook --phase "
     "P9\n"},
    {"verification",
     "Usage: uni-plan verification --topic <topic|path> --for "
     "<plan|implementation|playbook> [options]\n\n",
     "Show verification entries for a topic's document.\n\n",
     "Required:\n"
     "  --topic <topic|path>    Topic key or plan file path\n"
     "  --for <type>            Document type: plan, implementation, "
     "playbook\n\n",
     "  --phase <phase-key>     Required when --for playbook\n", kHumanList,
     "Examples:\n"
     "  uni-plan verification --topic DOC-TOOL-CLI-SCHEMA --for plan\n"
     "  uni-plan verification --topic DOC-TOOL-CLI-SCHEMA --for playbook "
     "--phase P8\n"},
    {"schema",
     "Usage: uni-plan schema --type <doc|plan|playbook|implementation> "
     "[options]\n\n",
     "Show the expected section schema for a document type.\n\n",
     "Required:\n"
     "  --type <type>           Schema type: doc, plan, playbook, "
     "implementation\n\n",
     nullptr, kHumanList,
     "Examples:\n"
     "  uni-plan schema --type plan\n"
     "  uni-plan schema --type playbook --human\n"
     "  uni-plan schema --type doc\n"},
    {"rules", "Usage: uni-plan rules [options]\n\n",
     "List documentation governance rules with provenance.\n\n", nullptr,
     nullptr, kHumanTable,
     "Examples:\n"
     "  uni-plan rules\n"
     "  uni-plan rules\n"},
    {"validate", "Usage: uni-plan validate [options]\n\n",
     "Run validation checks on documentation structure and lifecycle.\n\n",
     nullptr, "  --strict                Fail (exit 1) on any check failure\n",
     kHumanTable,
     "Examples:\n"
     "  uni-plan validate\n"
     "  uni-plan validate --strict\n"},
    {"excerpt",
     "Usage: uni-plan excerpt --doc <path> --section <section-id|heading> "
     "[options]\n\n",
     "Extract the source text of a section from a document.\n\n",
     "Required:\n"
     "  --doc <path>            Document file path\n"
     "  --section <id|heading>  Section identifier or heading text\n\n",
     "  --context-lines <n>     Number of context lines around the section "
     "[default: 0]\n",
     kHumanDisplay,
     "Examples:\n"
     "  uni-plan excerpt --doc Tools/Doc/Docs/Plans/DocToolCliSchema.Plan.md "
     "--section summary\n"
     "  uni-plan excerpt --doc Tools/Doc/Docs/Plans/DocToolCliSchema.Plan.md "
     "--section summary\n"},
    {"graph", "Usage: uni-plan graph --topic <topic|path> [options]\n\n",
     "Show the document dependency graph for a topic.\n\n",
     "Required:\n"
     "  --topic <topic|path>    Topic key or plan file path\n\n",
     "  --depth <n>             Maximum traversal depth [default: unlimited]\n",
     kHumanTables,
     "Examples:\n"
     "  uni-plan graph --topic DOC-TOOL-CLI-SCHEMA\n"
     "  uni-plan graph --topic DOC-TOOL-CLI-SCHEMA --depth 2\n"},
    {"diagnose", "Usage: uni-plan diagnose drift [options]\n\n",
     "Detect documentation drift across the repository.\n\n", nullptr, nullptr,
     kHumanTable,
     "Examples:\n"
     "  uni-plan diagnose drift\n"
     "  uni-plan diagnose drift\n"},
    {"timeline", "Usage: uni-plan timeline --topic <topic|path> [options]\n\n",
     "Show the chronological timeline of changes for a topic.\n\n",
     "Required:\n"
     "  --topic <topic|path>    Topic key or plan file path\n\n",
     "  --since <yyyy-mm-dd>    Only show entries after this date\n",
     kHumanTable,
     "Examples:\n"
     "  uni-plan timeline --topic DOC-TOOL-CLI-SCHEMA\n"
     "  uni-plan timeline --topic DOC-TOOL-CLI-SCHEMA --since 2026-02-20\n"},
    {"blockers", "Usage: uni-plan blockers [options]\n\n",
     "List blocking items across all topics.\n\n", nullptr,
     "  --status <filter>       Filter by status. Prefix with ! to exclude "
     "(e.g. !open).\n"
     "                          Values: all|open|blocked [default: all]\n",
     kHumanTable,
     "Examples:\n"
     "  uni-plan blockers\n"
     "  uni-plan blockers --status !blocked\n"},
};

void PrintCommandUsage(const std::string &InCommand) {
  // Special-case: section (multi-subcommand layout)
  if (InCommand == "section") {
    std::cout
        << "Subcommands:\n"
           "  uni-plan section resolve --doc <path> --section <id|heading> "
           "[options]\n"
           "  uni-plan section schema  [--type "
           "<doc|plan|playbook|implementation>] [options]\n"
           "  uni-plan section content --doc <path> --id <id|heading> "
           "[--line-char-limit <n>] [options]\n"
           "  uni-plan section list    [--doc <path>] [--count] [options]\n\n"
           "section resolve: Resolve a section heading to its location in a "
           "document.\n"
           "section schema:  List canonical sections for a document type.\n"
           "section content: Extract section content with optional table "
           "rendering.\n"
           "section list:    List sections across all docs or for a single "
           "document.\n\n"
           "Options (resolve):\n"
           "  --doc <path>            Document file path (required)\n"
           "  --section <id|heading>  Section identifier or heading text "
           "(required)\n\n"
           "Options (schema):\n"
           "  --type <doc|plan|playbook|implementation> Schema type [default: "
           "doc]\n\n"
           "Options (content):\n"
           "  --doc <path>            Document file path (required)\n"
           "  --id <id|heading>       Section identifier or heading text "
           "(required)\n"
           "  --line-char-limit <n>   Truncate each line to n characters "
           "(0=unlimited)\n\n"
           "Options (list):\n"
           "  --doc <path>            Document file path (omit to scan all "
           "docs)\n"
           "  --count                 Show occurrence counts (inventory "
           "mode)\n\n"
           "Common options:\n"
           "  --human                 Output as formatted ANSI display "
           "(renders tables)\n"
           "  --repo-root <path>      Override repository root\n\n"
           "Examples:\n"
           "  uni-plan section resolve --doc Docs/Plans/AudioSFXSystem.Plan.md "
           "--section summary\n"
           "  uni-plan section schema --type playbook --human\n"
           "  uni-plan section schema --type plan --human\n"
           "  uni-plan section content --doc Docs/Plans/AudioSFXSystem.Plan.md "
           "--id summary --human\n"
           "  uni-plan section content --doc Docs/Plans/AudioSFXSystem.Plan.md "
           "--id implementation_phases --human --line-char-limit 80\n"
           "  uni-plan section list --count --human\n"
           "  uni-plan section list --doc Docs/Plans/AudioSFXSystem.Plan.md "
           "--human\n";
    return;
  }
  // Special-case: table (multi-subcommand layout)
  if (InCommand == "table") {
    std::cout
        << "Subcommands:\n"
           "  uni-plan table list --doc <path> [options]\n"
           "  uni-plan table get  --doc <path> --table-id <id> [options]\n\n"
           "table list: List all markdown tables in a document.\n"
           "table get:  Extract a specific table by ID.\n\n"
           "Required:\n"
           "  --doc <path>            Document file path\n"
           "  --table-id <id>         Table ID (for 'get' only)\n\n"
           "Options:\n"
        << kHumanTable
        << "  --repo-root <path>      Override repository root\n\n"
           "Examples:\n"
           "  uni-plan table list --doc "
           "Tools/Doc/Docs/Plans/DocToolCliSchema.Plan.md\n"
           "  uni-plan table get --doc "
           "Tools/Doc/Docs/Plans/DocToolCliSchema.Plan.md --table-id 1\n";
    return;
  }
  // Special-case: phase (two modes: single-plan vs all-plans)
  if (InCommand == "phase") {
    std::cout
        << "List implementation phases from plan implementation_phases "
           "tables.\n\n"
           "Modes:\n"
           "  uni-plan phase --topic <topic|path> [options]     Single plan\n"
           "  uni-plan phase list [options]                     All plans "
           "(tree view)\n\n"
           "Single-plan mode:\n"
           "  Show phases for one plan. --topic is required.\n\n"
           "All-plans mode:\n"
           "  List every plan that has phases, with its phases nested below.\n"
           "  Omit --topic to enter this mode.\n\n"
           "Options:\n"
           "  --topic <topic|path>    Topic key (e.g., DOC-TOOL-CLI-SCHEMA) or "
           "plan file path\n"
           "  --status <filter>       Filter by status. Prefix with ! to "
           "exclude (e.g. !completed).\n"
           "                          Values: "
           "all|not_started|in_progress|completed|closed|blocked|canceled|"
           "unknown [default: all]\n"
        << kHumanTable
        << "  --repo-root <path>      Override repository root\n\n"
           "Examples:\n"
           "  uni-plan phase --topic DOC-TOOL-CLI-SCHEMA\n"
           "  uni-plan phase --topic DOC-TOOL-CLI-SCHEMA --status !completed "
           "--human\n"
           "  uni-plan phase list\n"
           "  uni-plan phase list --human\n"
           "  uni-plan phase list --status completed\n";
    return;
  }
  // Special-case: cache (multi-subcommand layout)
  if (InCommand == "cache") {
    std::cout
        << "Manage the persisted inventory cache.\n\n"
           "uni-plan scans the repository for all plans, playbooks, "
           "implementations,\n"
           "and sidecar documents to build a documentation inventory. This "
           "scan is cached\n"
           "to avoid repeating it on every invocation. The cache is "
           "automatically\n"
           "invalidated when any markdown file is added, removed, resized, or "
           "modified\n"
           "(tracked via FNV-1a hash of file paths, sizes, and timestamps).\n\n"
           "Cache location: "
           "~/.codex/uni-plan/cache/<repo-hash>/inventory.cache\n"
           "Each repository gets its own cache entry keyed by a hash of the "
           "repo path.\n\n"
           "Subcommands:\n"
           "  uni-plan cache [info]   [options]\n"
           "  uni-plan cache clear    [options]\n"
           "  uni-plan cache config   --dir <path> [--enabled <true|false>] "
           "[--verbose <true|false>] [options]\n\n"
           "cache info:    Show cache directory, size, entry count, and "
           "configuration state.\n"
           "cache clear:   Remove all cached inventory data for all "
           "repositories.\n"
           "cache config:  Update cache settings in uni-plan.ini next to the "
           "binary.\n\n"
           "Options (config):\n"
           "  --dir <path>            Set cache directory (absolute, relative, "
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
  for (const FCommandHelpEntry &Entry : kCommandHelp) {
    if (InCommand != Entry.mName) {
      continue;
    }
    std::cout << Entry.mUsageLine << Entry.mDescription;
    if (Entry.mRequiredOptions != nullptr) {
      std::cout << Entry.mRequiredOptions;
    }
    std::cout << "Options:\n";
    if (Entry.mSpecificOptions != nullptr) {
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

const std::vector<DocumentRecord> &
ResolveRecordsByKind(const Inventory &InInventory, const std::string &InKind) {
  if (InKind == "plan") {
    return InInventory.mPlans;
  }
  if (InKind == "playbook") {
    return InInventory.mPlaybooks;
  }
  if (InKind == "implementation") {
    return InInventory.mImplementations;
  }
  throw UsageError("Invalid value for --type: " + InKind);
}

std::vector<const TopicPairRecord *>
FilterPairsByStatus(const std::vector<TopicPairRecord> &InPairs,
                    const std::string &InStatusFilter) {
  const bool bFilterActive = (InStatusFilter != "all");
  std::vector<const TopicPairRecord *> Result;
  for (const TopicPairRecord &Pair : InPairs) {
    if (bFilterActive &&
        !MatchesStatusFilter(InStatusFilter,
                             GetDisplayStatus(Pair.mOverallStatus))) {
      continue;
    }
    Result.push_back(&Pair);
  }
  return Result;
}

std::vector<const DocumentRecord *>
FilterRecordsByStatus(const std::vector<DocumentRecord> &InRecords,
                      const std::string &InStatusFilter) {
  const bool bFilterActive = (InStatusFilter != "all");
  std::vector<const DocumentRecord *> Result;
  for (const DocumentRecord &Record : InRecords) {
    if (bFilterActive &&
        !MatchesStatusFilter(InStatusFilter,
                             GetDisplayStatus(Record.mStatus))) {
      continue;
    }
    Result.push_back(&Record);
  }
  return Result;
}

std::vector<SchemaField>
ParseSchemaFields(const fs::path &InSchemaPath,
                  std::vector<std::string> &OutWarnings) {
  std::vector<std::string> Lines;
  std::string ReadError;
  if (!TryReadFileLines(InSchemaPath, Lines, ReadError)) {
    throw std::runtime_error("Failed to read schema file '" +
                             InSchemaPath.string() + "': " + ReadError);
  }

  const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
  const std::vector<MarkdownTableRecord> Tables =
      ParseMarkdownTables(Lines, Headings);
  std::vector<SchemaField> Fields;

  for (const MarkdownTableRecord &Table : Tables) {
    if (Table.mHeaders.size() < 2) {
      continue;
    }

    const std::string Header0 = ToLower(Trim(Table.mHeaders[0]));
    const std::string Header1 = ToLower(Trim(Table.mHeaders[1]));
    if (Header0 != "property" || Header1 != "value") {
      continue;
    }

    for (const std::vector<std::string> &Row : Table.mRows) {
      if (Row.size() < 2) {
        continue;
      }
      SchemaField Field;
      Field.mSectionId = Table.mSectionId;
      Field.mProperty = Row[0];
      Field.mValue = Row[1];
      Fields.push_back(std::move(Field));
    }
  }

  if (Fields.empty()) {
    AddWarning(OutWarnings, "No `Property|Value` schema tables found in '" +
                                ToGenericPath(InSchemaPath) + "'.");
  }
  return Fields;
}

bool IsSupportedSchemaType(const std::string &InType) {
  static const std::set<std::string> Supported = {"doc",
                                                  "plan",
                                                  "playbook",
                                                  "implementation",
                                                  "plan_changelog",
                                                  "plan_verification",
                                                  "impl_changelog",
                                                  "impl_verification",
                                                  "playbook_changelog",
                                                  "playbook_verification"};
  return Supported.count(InType) > 0;
}

static fs::path SchemaRelativePath(const std::string &InType) {
  if (InType == "doc") {
    return fs::path("Schemas/Doc.Schema.md");
  }
  if (InType == "playbook") {
    return fs::path("Schemas/Playbook.Schema.md");
  }
  if (InType == "implementation") {
    return fs::path("Schemas/Implementation.Schema.md");
  }
  if (InType == "plan_changelog") {
    return fs::path("Schemas/PlanChangeLog.Schema.md");
  }
  if (InType == "plan_verification") {
    return fs::path("Schemas/PlanVerification.Schema.md");
  }
  if (InType == "impl_changelog") {
    return fs::path("Schemas/ImplChangeLog.Schema.md");
  }
  if (InType == "impl_verification") {
    return fs::path("Schemas/ImplVerification.Schema.md");
  }
  if (InType == "playbook_changelog") {
    return fs::path("Schemas/PlaybookChangeLog.Schema.md");
  }
  if (InType == "playbook_verification") {
    return fs::path("Schemas/PlaybookVerification.Schema.md");
  }
  return fs::path("Schemas/Plan.Schema.md");
}

fs::path ResolveSchemaFilePath(const std::string &InType,
                               const fs::path &InRepoRoot) {
  const fs::path RelPath = SchemaRelativePath(InType);

  // 1. Try repo-local schema
  if (!InRepoRoot.empty()) {
    const fs::path RepoSchema = InRepoRoot / RelPath;
    if (fs::exists(RepoSchema)) {
      return RepoSchema;
    }
  }

  // 2. Try bundled schemas next to executable
  const fs::path ExeDir = GetExecutableDirectory();
  const fs::path BundledSchema = ExeDir / RelPath;
  if (fs::exists(BundledSchema)) {
    return BundledSchema;
  }

  // 3. Fallback: return repo-local path even if missing (callers handle
  // non-existence)
  if (!InRepoRoot.empty()) {
    return InRepoRoot / RelPath;
  }
  return RelPath;
}

std::vector<std::string> BuildSchemaExamples(const std::string & /*InType*/) {
  // Examples are repo-specific; return empty to avoid hardcoding paths
  return {};
}

std::string JoinMarkdownRowCells(const std::vector<std::string> &InRow) {
  std::ostringstream Stream;
  for (size_t Index = 0; Index < InRow.size(); ++Index) {
    if (Index > 0) {
      Stream << " | ";
    }
    Stream << Trim(InRow[Index]);
  }
  return Stream.str();
}

bool RowContainsAllTerms(const std::vector<std::string> &InRow,
                         const std::vector<std::string> &InTerms) {
  if (InTerms.empty()) {
    return true;
  }

  const std::string RowLower = ToLower(JoinMarkdownRowCells(InRow));
  for (const std::string &Term : InTerms) {
    if (RowLower.find(ToLower(Term)) == std::string::npos) {
      return false;
    }
  }
  return true;
}

bool TryResolveRuleProvenance(const fs::path &InRepoRoot,
                              const RuleProvenanceProbe &InProbe,
                              RuleEntry &InOutRule,
                              std::vector<std::string> &OutWarnings) {
  const fs::path AbsolutePath = InRepoRoot / fs::path(InProbe.mPath);
  std::vector<std::string> Lines;
  std::string ReadError;
  if (!TryReadFileLines(AbsolutePath, Lines, ReadError)) {
    AddWarning(OutWarnings, "Rule provenance read failed for '" +
                                InProbe.mPath + "' (`" + InOutRule.mId +
                                "`): " + ReadError);
    return false;
  }

  const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
  const std::vector<MarkdownTableRecord> Tables =
      ParseMarkdownTables(Lines, Headings);
  const std::string TargetSectionId = NormalizeSectionId(InProbe.mSectionId);

  for (const MarkdownTableRecord &Table : Tables) {
    if (!TargetSectionId.empty() && Table.mSectionId != TargetSectionId) {
      continue;
    }

    for (size_t RowIndex = 0; RowIndex < Table.mRows.size(); ++RowIndex) {
      const std::vector<std::string> &Row = Table.mRows[RowIndex];
      if (!RowContainsAllTerms(Row, InProbe.mRowTerms)) {
        continue;
      }

      InOutRule.mbSourceResolved = true;
      InOutRule.mSourcePath = InProbe.mPath;
      InOutRule.mSourceSectionId = Table.mSectionId;
      InOutRule.mSourceTableId = Table.mTableId;
      InOutRule.mSourceRowIndex = static_cast<int>(RowIndex) + 1;
      InOutRule.mSourceEvidence = JoinMarkdownRowCells(Row);
      if (!InOutRule.mSourceSectionId.empty()) {
        InOutRule.mSource =
            InOutRule.mSourcePath + "#" + InOutRule.mSourceSectionId;
      } else {
        InOutRule.mSource = InOutRule.mSourcePath;
      }
      return true;
    }
  }

  AddWarning(OutWarnings, "Rule provenance unresolved for `" + InOutRule.mId +
                              "` in '" + InProbe.mPath + "' section '" +
                              InProbe.mSectionId + "'.");
  return false;
}

std::vector<RuleEntry> BuildRules(const fs::path &InRepoRoot,
                                  std::vector<std::string> &OutWarnings) {
  std::vector<RuleEntry> Rules = {
      {"plan_impl_pairing",
       "Active `<TopicPascalCase>.Plan.md` docs must have paired "
       "`<TopicPascalCase>.Impl.md` trackers.",
       "CLAUDE.md#pairing_rules"},
      {"active_phase_playbook_required",
       "Every active plan phase must have a dedicated "
       "`<TopicPascalCase>.<PhaseKey>.Playbook.md`.",
       "CLAUDE.md#pairing_rules"},
      {"phase_entry_gate",
       "Before a phase is marked in progress, complete investigation and "
       "prepare the phase playbook execution lanes (and `testing` for testable "
       "phases).",
       "CLAUDE.md#pairing_rules"},
      {"artifact_role_boundary",
       "Keep artifact ownership explicit: playbook is procedure, "
       "implementation is outcomes, sidecars are evidence history.",
       "CLAUDE.md#pairing_rules"},
      {"canonical_naming",
       "Use canonical naming for plan, implementation, playbook, and sidecar "
       "docs.",
       "CLAUDE.md#document_type_naming"},
      {"canonical_placement",
       "Keep lifecycle artifacts in canonical scope folders (`Docs/Plans`, "
       "`Docs/Playbooks`, `Docs/Implementation`).",
       "PATTERNS.md#pattern-p-doc-plan-artifact-bundle"},
      {"detached_evidence_sidecars",
       "Keep long-running change/verification history in detached sidecars and "
       "keep core docs concise.",
       "PATTERNS.md#pattern-p-doc-plan-artifact-bundle"},
      {"doc_lint_required",
       "Run uni-plan lint after documentation tasks (`uni-plan lint`).",
       "CLAUDE.md#doc_lint_commands"}};

  const std::vector<RuleProvenanceProbe> Probes = {
      {"CLAUDE.md", "pairing_rules", {"paired artifacts"}},
      {"CLAUDE.md", "pairing_rules", {"playbook-first"}},
      {"CLAUDE.md", "pairing_rules", {"phase entry gates"}},
      {"CLAUDE.md", "pairing_rules", {"artifact boundary"}},
      {"CLAUDE.md", "document_type_naming", {"plan", ".plan.md"}},
      {"PATTERNS.md",
       "pattern_p_doc_plan_artifact_bundle",
       {"canonical scope folders"}},
      {"PATTERNS.md",
       "pattern_p_doc_plan_artifact_bundle",
       {"detached sidecars"}},
      {"CLAUDE.md", "doc_lint_commands", {"uni-plan lint", "yes"}}};

  if (Rules.size() != Probes.size()) {
    AddWarning(
        OutWarnings,
        "Rule probe configuration mismatch; provenance fallback is active.");
    return Rules;
  }

  for (size_t Index = 0; Index < Rules.size(); ++Index) {
    TryResolveRuleProvenance(InRepoRoot, Probes[Index], Rules[Index],
                             OutWarnings);
  }
  return Rules;
}

bool HasIndexedHeadingPrefix(const std::string &InHeadingText) {
  static const std::regex IndexedPrefixRegex(R"(^\s*\d+\s*[\.\)])");
  return std::regex_search(InHeadingText, IndexedPrefixRegex);
}

std::string BuildTopicPhaseIdentityNormalized(const std::string &InTopicKey,
                                              const std::string &InPhaseKey) {
  return ToLower(Trim(InTopicKey)) + "::" + ToLower(Trim(InPhaseKey));
}

std::string ExtractPhaseKeyFromCell(const std::string &InCellValue) {
  const std::string CellValue = Trim(InCellValue);
  const size_t TickStart = CellValue.find('`');
  if (TickStart != std::string::npos) {
    const size_t TickEnd = CellValue.find('`', TickStart + 1);
    if (TickEnd != std::string::npos && TickEnd > TickStart + 1) {
      return Trim(CellValue.substr(TickStart + 1, TickEnd - TickStart - 1));
    }
  }

  static const std::regex TokenPattern(R"(([A-Za-z][A-Za-z0-9_-]*))");
  std::smatch Match;
  if (std::regex_search(CellValue, Match, TokenPattern)) {
    return Trim(Match[1].str());
  }
  return "";
}

std::vector<ActivePhaseRecord>
CollectActivePhaseRecords(const fs::path &InRepoRoot,
                          const std::vector<DocumentRecord> &InPlans,
                          std::vector<std::string> &OutWarnings) {
  std::vector<ActivePhaseRecord> ActivePhases;
  for (const DocumentRecord &Plan : InPlans) {
    const fs::path AbsolutePath = InRepoRoot / fs::path(Plan.mPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsolutePath, Lines, ReadError)) {
      AddWarning(OutWarnings, "Active-phase parse skipped for '" + Plan.mPath +
                                  "': " + ReadError);
      continue;
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    const std::vector<MarkdownTableRecord> Tables =
        ParseMarkdownTables(Lines, Headings);
    for (const MarkdownTableRecord &Table : Tables) {
      if (Table.mSectionId != "implementation_phases") {
        continue;
      }

      const int PhaseIndex = FindHeaderIndex(Table.mHeaders, "phase");
      const int StatusIndex = FindHeaderIndex(Table.mHeaders, "status");
      if (PhaseIndex < 0 || StatusIndex < 0) {
        continue;
      }

      for (const std::vector<std::string> &Row : Table.mRows) {
        const size_t PhaseCellIndex = static_cast<size_t>(PhaseIndex);
        const size_t StatusCellIndex = static_cast<size_t>(StatusIndex);
        if (PhaseCellIndex >= Row.size() || StatusCellIndex >= Row.size()) {
          continue;
        }

        const std::string PhaseKey =
            ExtractPhaseKeyFromCell(Row[PhaseCellIndex]);
        if (PhaseKey.empty()) {
          continue;
        }

        const std::string StatusRaw = Trim(Row[StatusCellIndex]);
        const std::string Status = NormalizeStatusValue(StatusRaw);
        if (Status != "in_progress" && Status != "blocked") {
          continue;
        }

        ActivePhaseRecord Record;
        Record.mTopicKey = Plan.mTopicKey;
        Record.mPlanPath = Plan.mPath;
        Record.mPhaseKey = PhaseKey;
        Record.mStatusRaw = StatusRaw;
        Record.mStatus = Status;
        ActivePhases.push_back(std::move(Record));
      }
    }
  }
  return ActivePhases;
}

std::set<std::string>
BuildHeadingIdSet(const std::vector<HeadingRecord> &InHeadings) {
  std::set<std::string> HeadingIds;
  for (const HeadingRecord &Heading : InHeadings) {
    HeadingIds.insert(Heading.mSectionId);
  }
  return HeadingIds;
}

bool IsPlaybookPhaseEntryReady(const fs::path &InPlaybookAbsolutePath,
                               std::vector<std::string> &OutMissingSections,
                               std::string &OutReadError) {
  std::vector<std::string> Lines;
  if (!TryReadFileLines(InPlaybookAbsolutePath, Lines, OutReadError)) {
    return false;
  }

  const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
  const std::set<std::string> HeadingIds = BuildHeadingIdSet(Headings);
  const std::vector<std::string> RequiredSections = {
      "phase_binding", "investigation_baseline", "phase_entry_readiness_gate",
      "execution_lanes"};

  for (const std::string &RequiredSection : RequiredSections) {
    if (HeadingIds.count(RequiredSection) == 0) {
      OutMissingSections.push_back(RequiredSection);
    }
  }
  return true;
}

PhaseEntryGateResult
EvaluatePhaseEntryGate(const fs::path &InRepoRoot,
                       const std::vector<DocumentRecord> &InPlans,
                       const std::vector<DocumentRecord> &InPlaybooks,
                       std::vector<std::string> &OutWarnings) {
  PhaseEntryGateResult Result;
  const std::vector<ActivePhaseRecord> ActivePhases =
      CollectActivePhaseRecords(InRepoRoot, InPlans, OutWarnings);
  Result.mActivePhaseCount = static_cast<int>(ActivePhases.size());

  std::map<std::string, DocumentRecord> PlaybookByTopicPhase;
  for (const DocumentRecord &Playbook : InPlaybooks) {
    const std::string Identity = BuildTopicPhaseIdentityNormalized(
        Playbook.mTopicKey, Playbook.mPhaseKey);
    if (PlaybookByTopicPhase.count(Identity) == 0) {
      PlaybookByTopicPhase.emplace(Identity, Playbook);
    }
  }

  for (const ActivePhaseRecord &ActivePhase : ActivePhases) {
    const std::string Identity = BuildTopicPhaseIdentityNormalized(
        ActivePhase.mTopicKey, ActivePhase.mPhaseKey);
    const auto FoundPlaybook = PlaybookByTopicPhase.find(Identity);
    if (FoundPlaybook == PlaybookByTopicPhase.end()) {
      Result.mMissingPlaybookCount += 1;
      AddWarning(OutWarnings, "Active phase '" + ActivePhase.mPhaseKey +
                                  "' for topic '" + ActivePhase.mTopicKey +
                                  "' in '" + ActivePhase.mPlanPath +
                                  "' has no matching playbook.");
      continue;
    }

    std::vector<std::string> MissingSections;
    std::string ReadError;
    const fs::path PlaybookAbsolutePath =
        InRepoRoot / fs::path(FoundPlaybook->second.mPath);
    if (!IsPlaybookPhaseEntryReady(PlaybookAbsolutePath, MissingSections,
                                   ReadError)) {
      Result.mUnpreparedPlaybookCount += 1;
      AddWarning(OutWarnings,
                 "Active-phase playbook readiness parse failed for '" +
                     FoundPlaybook->second.mPath + "': " + ReadError);
      continue;
    }

    if (!MissingSections.empty()) {
      Result.mUnpreparedPlaybookCount += 1;
      std::ostringstream Missing;
      for (size_t Index = 0; Index < MissingSections.size(); ++Index) {
        if (Index > 0) {
          Missing << ", ";
        }
        Missing << MissingSections[Index];
      }
      AddWarning(OutWarnings,
                 "Active-phase playbook '" + FoundPlaybook->second.mPath +
                     "' is missing readiness sections: " + Missing.str() + ".");
    }
  }

  return Result;
}

ArtifactRoleBoundaryResult EvaluateArtifactRoleBoundaries(
    const fs::path &InRepoRoot, const std::vector<DocumentRecord> &InPlaybooks,
    const std::vector<DocumentRecord> &InImplementations,
    std::vector<std::string> &OutWarnings) {
  ArtifactRoleBoundaryResult Result;

  const std::vector<std::string> ForbiddenPlaybookSections = {
      "progress_summary", "phase_tracking", "implementation_phases"};
  for (const DocumentRecord &Playbook : InPlaybooks) {
    const fs::path AbsolutePath = InRepoRoot / fs::path(Playbook.mPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsolutePath, Lines, ReadError)) {
      AddWarning(OutWarnings,
                 "Artifact-role boundary parse skipped for playbook '" +
                     Playbook.mPath + "': " + ReadError);
      continue;
    }

    const std::set<std::string> HeadingIds =
        BuildHeadingIdSet(ParseHeadingRecords(Lines));
    std::vector<std::string> Violations;
    for (const std::string &Forbidden : ForbiddenPlaybookSections) {
      if (HeadingIds.count(Forbidden) > 0) {
        Violations.push_back(Forbidden);
      }
    }

    if (!Violations.empty()) {
      Result.mPlaybookViolationCount += 1;
      AddWarning(OutWarnings,
                 "Playbook role-boundary violation in '" + Playbook.mPath +
                     "' (implementation-owned sections detected).");
    }
  }

  const std::vector<std::string> ForbiddenImplementationSections = {
      "phase_entry_readiness_gate", "execution_lanes", "investigation_baseline",
      "lane_artifact_contract", "testing"};
  for (const DocumentRecord &Implementation : InImplementations) {
    const fs::path AbsolutePath = InRepoRoot / fs::path(Implementation.mPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsolutePath, Lines, ReadError)) {
      AddWarning(OutWarnings,
                 "Artifact-role boundary parse skipped for implementation '" +
                     Implementation.mPath + "': " + ReadError);
      continue;
    }

    const std::set<std::string> HeadingIds =
        BuildHeadingIdSet(ParseHeadingRecords(Lines));
    std::vector<std::string> Violations;
    for (const std::string &Forbidden : ForbiddenImplementationSections) {
      if (HeadingIds.count(Forbidden) > 0) {
        Violations.push_back(Forbidden);
      }
    }

    if (!Violations.empty()) {
      Result.mImplementationViolationCount += 1;
      AddWarning(OutWarnings, "Implementation role-boundary violation in '" +
                                  Implementation.mPath +
                                  "' (playbook-owned sections detected).");
    }
  }

  return Result;
}

PlaybookSchemaResult
EvaluatePlaybookSchema(const fs::path &InRepoRoot,
                       const std::vector<DocumentRecord> &InPlaybooks,
                       std::vector<std::string> &OutWarnings) {
  PlaybookSchemaResult Result;
  const std::vector<SectionSchemaEntry> SchemaEntries =
      BuildSectionSchemaEntries("playbook", InRepoRoot);
  std::vector<std::string> RequiredSectionIds;
  for (const SectionSchemaEntry &Entry : SchemaEntries) {
    if (Entry.mbRequired) {
      RequiredSectionIds.push_back(Entry.mSectionId);
    }
  }

  Result.mPlaybookCount = static_cast<int>(InPlaybooks.size());
  for (const DocumentRecord &Playbook : InPlaybooks) {
    const fs::path AbsolutePath = InRepoRoot / fs::path(Playbook.mPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsolutePath, Lines, ReadError)) {
      Result.mReadFailureCount += 1;
      AddWarning(OutWarnings, "Playbook-schema check skipped for '" +
                                  Playbook.mPath + "': " + ReadError);
      continue;
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    std::set<std::string> H2Ids;
    for (const HeadingRecord &Heading : Headings) {
      if (Heading.mLevel == 2) {
        H2Ids.insert(Heading.mSectionId);
      }
    }

    std::vector<std::string> MissingSections;
    for (const std::string &Required : RequiredSectionIds) {
      if (H2Ids.count(Required) == 0) {
        MissingSections.push_back(Required);
      }
    }
    if (!MissingSections.empty()) {
      Result.mMissingSectionPlaybookCount += 1;
      Result.mDiagnostics.push_back(
          "playbook=" + Playbook.mPath +
          " missing_required_sections=" + JoinCommaSeparated(MissingSections));
    }
  }

  return Result;
}

// ---------------------------------------------------------------------------
// Section Schema renderers
// ---------------------------------------------------------------------------

// SectionSchemaEntry moved to DocTypes.h

std::vector<SectionSchemaEntry>
BuildSectionSchemaEntries(const std::string &InType,
                          const fs::path &InRepoRoot) {
  // Resolve schema file: repo-local first, then bundled next to executable
  const fs::path SchemaPath = ResolveSchemaFilePath(InType, InRepoRoot);
  if (fs::exists(SchemaPath)) {
    std::vector<SectionSchemaEntry> Parsed =
        TryParseSectionSchemaFromFile(SchemaPath);
    if (!Parsed.empty()) {
      return Parsed;
    }
  }

  return {};
}

void AppendGraphEdgeUnique(std::vector<GraphEdge> &InOutEdges,
                           std::set<std::string> &InOutEdgeKeys,
                           const std::string &InFromNodeId,
                           const std::string &InToNodeId,
                           const std::string &InKind, const int InDepth) {
  const std::string Key = InFromNodeId + "|" + InToNodeId + "|" + InKind + "|" +
                          std::to_string(InDepth);
  if (InOutEdgeKeys.count(Key) > 0) {
    return;
  }
  InOutEdgeKeys.insert(Key);

  GraphEdge Edge;
  Edge.mFromNodeId = InFromNodeId;
  Edge.mToNodeId = InToNodeId;
  Edge.mKind = InKind;
  Edge.mDepth = InDepth;
  InOutEdges.push_back(std::move(Edge));
}

void AddDriftItem(std::vector<DriftItem> &InOutDrifts, const std::string &InId,
                  const std::string &InSeverity, const std::string &InTopicKey,
                  const std::string &InPath, const std::string &InMessage) {
  DriftItem Item;
  Item.mId = InId;
  Item.mSeverity = InSeverity;
  Item.mTopicKey = InTopicKey;
  Item.mPath = InPath;
  Item.mMessage = InMessage;
  InOutDrifts.push_back(std::move(Item));
}

int SeverityRank(const std::string &InSeverity) {
  if (InSeverity == "error") {
    return 0;
  }
  if (InSeverity == "warning") {
    return 1;
  }
  return 2;
}

int RunMain(const int InArgc, char *InArgv[]) {
  std::vector<std::string> Tokens;
  for (int Index = 1; Index < InArgc; ++Index) {
    Tokens.emplace_back(InArgv[Index]);
  }

  bool UseCache = true;
  std::vector<std::string> FilteredTokens;
  FilteredTokens.reserve(Tokens.size());
  for (const std::string &Token : Tokens) {
    if (Token == "--no-cache") {
      UseCache = false;
      continue;
    }
    FilteredTokens.push_back(Token);
  }
  Tokens = std::move(FilteredTokens);

  const DocConfig Config = LoadConfig(ResolveExecutableDirectory());

  if (!Config.mbCacheEnabled) {
    UseCache = false;
  }

  if (Tokens.empty()) {
    PrintUsage();
    return 0;
  }

  if (Tokens.size() == 1 && (Tokens[0] == "--help" || Tokens[0] == "-h")) {
    PrintUsage();
    return 0;
  }

  if (Tokens.size() == 1 && Tokens[0] == "--version") {
    std::cout << "uni-plan " << kCliVersion << "\n";
    return 0;
  }

  const std::string Command = Tokens[0];
  try {
    if (Command == "list") {
      const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());
      if (ContainsHelpFlag(Args)) {
        PrintCommandUsage("list");
        return 0;
      }
      const ListOptions Options = ParseListOptions(Args);
      const Inventory ParsedInventory = BuildInventory(
          Options.mRepoRoot, UseCache, Config.mCacheDir, Config.mbCacheVerbose);
      if (Options.mbHuman) {
        return RunListHuman(ParsedInventory, Options.mKind, Options.mStatus);
      }
      if (!Options.mbJson) {
        return RunListText(ParsedInventory, Options.mKind, Options.mStatus);
      }
      return RunListJson(ParsedInventory, Options.mKind, Options.mStatus);
    }

    if (Command == "phase") {
      if (ContainsHelpFlag(
              std::vector<std::string>(Tokens.begin() + 1, Tokens.end()))) {
        PrintCommandUsage("phase");
        return 0;
      }

      bool bListAll = false;
      size_t ArgsStartIndex = 1;
      if (Tokens.size() > 1) {
        const std::string MaybeSubcommand = ToLower(Tokens[1]);
        if (MaybeSubcommand == "list") {
          ArgsStartIndex = 2;
          bListAll = true;
        } else if (!IsOptionToken(Tokens[1])) {
          throw UsageError("Unknown phase subcommand: " + Tokens[1]);
        }
      }

      const std::vector<std::string> Args(
          Tokens.begin() + static_cast<std::ptrdiff_t>(ArgsStartIndex),
          Tokens.end());
      const PhaseOptions Options = ParsePhaseOptions(Args);

      // If --topic was provided, revert to single-plan mode
      if (!Options.mTopic.empty()) {
        bListAll = false;
      }

      const fs::path RepoRoot = NormalizeRepoRootPath(Options.mRepoRoot);
      const Inventory ParsedInventory = BuildInventory(
          Options.mRepoRoot, UseCache, Config.mCacheDir, Config.mbCacheVerbose);

      if (bListAll) {
        // All-plans mode: list every plan with its phases
        std::vector<std::string> Warnings = ParsedInventory.mWarnings;
        const std::vector<PhaseListAllEntry> Entries = BuildPhaseListAll(
            RepoRoot, ParsedInventory, Options.mStatus, Warnings);
        NormalizeWarnings(Warnings);

        if (Options.mbHuman) {
          return RunPhaseListAllHuman(Options.mStatus, Entries, Warnings);
        }
        if (!Options.mbJson) {
          return RunPhaseListAllText(Options.mStatus, Entries, Warnings);
        }
        return RunPhaseListAllJson(GetUtcNow(), ToGenericPath(RepoRoot),
                                   Options.mStatus, Entries, Warnings);
      }

      // Single-plan mode: requires --topic
      if (Options.mTopic.empty()) {
        throw UsageError("Missing required option --topic (or use 'uni-plan "
                         "phase list' for all plans)");
      }

      const std::string TopicKey =
          ResolveTopicKeyFromInventory(ParsedInventory, Options.mTopic);
      const DocumentRecord *Plan =
          FindSingleRecordByTopic(ParsedInventory.mPlans, TopicKey);
      if (Plan == nullptr) {
        throw UsageError("Resolved topic has no plan document: " + TopicKey);
      }

      std::vector<std::string> Warnings = ParsedInventory.mWarnings;
      const std::vector<PhaseItem> Items =
          CollectPhaseItemsFromPlan(RepoRoot, *Plan, ParsedInventory.mPlaybooks,
                                    Options.mStatus, Warnings);
      NormalizeWarnings(Warnings);

      if (Options.mbHuman) {
        return RunPhaseHuman(TopicKey, Plan->mPath, Options.mStatus, Items,
                             Warnings);
      }
      if (!Options.mbJson) {
        return RunPhaseText(TopicKey, Plan->mPath, Options.mStatus, Items,
                            Warnings);
      }
      return RunPhaseJson(GetUtcNow(), ToGenericPath(RepoRoot), TopicKey,
                          Plan->mPath, Options.mStatus, Items, Warnings);
    }

    if (Command == "lint") {
      const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());
      if (ContainsHelpFlag(Args)) {
        PrintCommandUsage("lint");
        return 0;
      }
      const LintOptions Options = ParseLintOptions(Args);
      const LintResult Result = BuildLintResult(Options.mRepoRoot);
      if (Options.mbHuman) {
        RunLintHuman(Result);
      } else if (!Options.mbJson) {
        RunLintText(Result);
      } else {
        RunLintJson(Result);
      }
      return (Options.mbFailOnWarning && Result.mWarningCount > 0) ? 1 : 0;
    }

    if (Command == "inventory") {
      const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());
      if (ContainsHelpFlag(Args)) {
        PrintCommandUsage("inventory");
        return 0;
      }
      const InventoryOptions Options = ParseInventoryOptions(Args);
      const InventoryResult Result = BuildDocInventoryResult(Options.mRepoRoot);
      if (Options.mbHuman) {
        return RunInventoryHuman(Result);
      }
      if (!Options.mbJson) {
        return RunInventoryText(Result);
      }
      return RunInventoryJson(Result);
    }

    if (Command == "orphan-check") {
      const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());
      if (ContainsHelpFlag(Args)) {
        PrintCommandUsage("orphan-check");
        return 0;
      }
      const OrphanCheckOptions Options = ParseOrphanCheckOptions(Args);
      const OrphanCheckResult Result =
          BuildOrphanCheckResult(Options.mRepoRoot);
      if (Options.mbHuman) {
        return RunOrphanCheckHuman(Result);
      }
      if (!Options.mbJson) {
        return RunOrphanCheckText(Result);
      }
      return RunOrphanCheckJson(Result);
    }

    if (Command == "artifacts") {
      const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());
      if (ContainsHelpFlag(Args)) {
        PrintCommandUsage("artifacts");
        return 0;
      }
      const ArtifactsOptions Options = ParseArtifactsOptions(Args);
      const Inventory ParsedInventory = BuildInventory(
          Options.mRepoRoot, UseCache, Config.mCacheDir, Config.mbCacheVerbose);
      const std::string TopicKey =
          ResolveTopicKeyFromInventory(ParsedInventory, Options.mTopic);
      if (Options.mbHuman) {
        return RunArtifactsHuman(ParsedInventory, TopicKey, Options.mKind);
      }
      if (!Options.mbJson) {
        return RunArtifactsText(ParsedInventory, TopicKey, Options.mKind);
      }
      return RunArtifactsJson(ParsedInventory, TopicKey, Options.mKind);
    }

    if (Command == "changelog" || Command == "verification") {
      const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());
      if (ContainsHelpFlag(Args)) {
        PrintCommandUsage(Command);
        return 0;
      }
      const bool IsChangelog = (Command == "changelog");
      const EvidenceOptions Options = ParseEvidenceOptions(Args, Command);
      const fs::path RepoRoot = NormalizeRepoRootPath(Options.mRepoRoot);
      const Inventory ParsedInventory = BuildInventory(
          Options.mRepoRoot, UseCache, Config.mCacheDir, Config.mbCacheVerbose);
      const std::string TopicKey =
          ResolveTopicKeyFromInventory(ParsedInventory, Options.mTopic);
      const std::vector<SidecarRecord> TopicSidecars =
          CollectSidecarsByTopic(ParsedInventory.mSidecars, TopicKey);
      const std::string SidecarDocKind =
          IsChangelog ? kSidecarChangeLog : kSidecarVerification;
      const std::vector<SidecarRecord> EvidenceSidecars =
          ResolveEvidenceSidecars(TopicSidecars, Options.mDocClass,
                                  Options.mPhaseKey, SidecarDocKind);

      std::vector<std::string> Warnings = ParsedInventory.mWarnings;
      if (EvidenceSidecars.empty()) {
        AddWarning(Warnings, "No " + Command + " sidecar found for topic '" +
                                 TopicKey + "' and doc class '" +
                                 Options.mDocClass + "'.");
      }

      std::vector<EvidenceEntry> Entries;
      for (const SidecarRecord &Sidecar : EvidenceSidecars) {
        const fs::path AbsolutePath = RepoRoot / fs::path(Sidecar.mPath);
        std::vector<EvidenceEntry> SidecarEntries =
            ParseEvidenceEntriesFromFile(AbsolutePath, Sidecar.mPath,
                                         Sidecar.mPhaseKey, Warnings);
        Entries.insert(Entries.end(), SidecarEntries.begin(),
                       SidecarEntries.end());
      }
      NormalizeWarnings(Warnings);

      const char *SchemaId =
          IsChangelog ? kChangelogSchema : kVerificationSchema;
      const std::string Label = IsChangelog ? "Changelog" : "Verification";
      if (Options.mbHuman) {
        return RunEvidenceHuman(Label, TopicKey, Options.mDocClass, Entries,
                                Warnings);
      }
      if (!Options.mbJson) {
        return RunEvidenceText(Label, TopicKey, Options.mDocClass, Entries,
                               Warnings);
      }
      return RunEvidenceJson(SchemaId, ParsedInventory.mGeneratedUtc,
                             ParsedInventory.mRepoRoot, TopicKey,
                             Options.mDocClass, Entries, Warnings);
    }

    if (Command == "schema") {
      const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());
      if (ContainsHelpFlag(Args)) {
        PrintCommandUsage("schema");
        return 0;
      }
      const SchemaOptions Options = ParseSchemaOptions(Args);
      const fs::path RepoRoot = NormalizeRepoRootPath(Options.mRepoRoot);
      const fs::path SchemaPath =
          ResolveSchemaFilePath(Options.mType, RepoRoot);
      std::vector<std::string> Warnings;
      const std::vector<SchemaField> Fields =
          ParseSchemaFields(SchemaPath, Warnings);
      const std::vector<std::string> Examples =
          BuildSchemaExamples(Options.mType);
      NormalizeWarnings(Warnings);

      const std::string GeneratedUtc = GetUtcNow();
      const std::string RepoRootPath = ToGenericPath(RepoRoot);
      if (Options.mbHuman) {
        return RunSchemaHuman(Options.mType, Fields, Examples, Warnings);
      }
      if (!Options.mbJson) {
        return RunSchemaText(Options.mType, Fields, Examples, Warnings);
      }
      return RunSchemaJson(GeneratedUtc, RepoRootPath, Options.mType, Fields,
                           Examples, Warnings);
    }

    if (Command == "rules") {
      const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());
      if (ContainsHelpFlag(Args)) {
        PrintCommandUsage("rules");
        return 0;
      }
      const RulesOptions Options = ParseRulesOptions(Args);
      const fs::path RepoRoot = NormalizeRepoRootPath(Options.mRepoRoot);
      std::vector<std::string> Warnings;
      const std::vector<RuleEntry> Rules = BuildRules(RepoRoot, Warnings);
      NormalizeWarnings(Warnings);
      if (Options.mbHuman) {
        return RunRulesHuman(Rules, Warnings);
      }
      if (!Options.mbJson) {
        return RunRulesText(Rules, Warnings);
      }
      return RunRulesJson(GetUtcNow(), ToGenericPath(RepoRoot), Rules,
                          Warnings);
    }

    if (Command == "validate") {
      const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());
      if (ContainsHelpFlag(Args)) {
        PrintCommandUsage("validate");
        return 0;
      }
      const ValidateOptions Options = ParseValidateOptions(Args);
      const Inventory ParsedInventory = BuildInventory(
          Options.mRepoRoot, UseCache, Config.mCacheDir, Config.mbCacheVerbose);
      const fs::path RepoRoot = NormalizeRepoRootPath(Options.mRepoRoot);

      std::vector<std::string> Errors;
      std::vector<std::string> Warnings;
      bool Ok = true;
      const std::vector<ValidateCheck> Checks = BuildValidateChecks(
          ParsedInventory, RepoRoot, Options.mbStrict, Errors, Warnings, Ok);
      if (Options.mbHuman) {
        RunValidateHuman(Options.mbStrict, Ok, Checks, Errors, Warnings);
      } else if (!Options.mbJson) {
        RunValidateText(Options.mbStrict, Ok, Checks, Errors, Warnings);
      } else {
        RunValidateJson(ParsedInventory.mGeneratedUtc,
                        ParsedInventory.mRepoRoot, Options.mbStrict, Ok, Checks,
                        Errors, Warnings);
      }
      return Ok ? 0 : 1;
    }

    if (Command == "section") {
      if (ContainsHelpFlag(
              std::vector<std::string>(Tokens.begin() + 1, Tokens.end()))) {
        PrintCommandUsage("section");
        return 0;
      }
      if (Tokens.size() < 2) {
        throw UsageError("Missing subcommand for section (expected: "
                         "resolve|schema|content|list)");
      }

      const std::string Subcommand = ToLower(Tokens[1]);

      if (Subcommand == "resolve") {
        const std::vector<std::string> Args(Tokens.begin() + 2, Tokens.end());
        const SectionResolveOptions Options = ParseSectionResolveOptions(Args);
        const ResolvedDocument Doc =
            ReadAndParseDocument(Options, Options.mDocPath);

        std::vector<std::string> Warnings;
        const SectionResolution Resolution =
            ResolveSectionByQuery(Doc.mLines, Doc.mHeadings, Options.mSection);
        if (!Resolution.mbFound) {
          AddWarning(Warnings,
                     "Section query did not match document headings: " +
                         Options.mSection);
        }
        NormalizeWarnings(Warnings);

        if (Options.mbHuman) {
          RunSectionResolveHuman(Doc.mRelativePath, Resolution, Warnings);
        } else if (!Options.mbJson) {
          RunSectionResolveText(Doc.mRelativePath, Resolution, Warnings);
        } else {
          RunSectionResolveJson(GetUtcNow(), ToGenericPath(Doc.mRepoRoot),
                                Doc.mRelativePath, Resolution, Warnings);
        }
        return Resolution.mbFound ? 0 : 1;
      }

      if (Subcommand == "schema") {
        const std::vector<std::string> Args(Tokens.begin() + 2, Tokens.end());
        const SectionSchemaOptions Options = ParseSectionSchemaOptions(Args);
        const fs::path RepoRoot = NormalizeRepoRootPath(Options.mRepoRoot);
        const std::vector<SectionSchemaEntry> Entries =
            BuildSectionSchemaEntries(Options.mType, RepoRoot);

        if (Options.mbHuman) {
          return RunSectionSchemaHuman(Options.mType, Entries);
        }
        if (!Options.mbJson) {
          return RunSectionSchemaText(Options.mType, Entries);
        }
        return RunSectionSchemaJson(GetUtcNow(), ToGenericPath(RepoRoot),
                                    Options.mType, Entries);
      }

      if (Subcommand == "content") {
        const std::vector<std::string> Args(Tokens.begin() + 2, Tokens.end());
        const SectionContentOptions Options = ParseSectionContentOptions(Args);
        const ResolvedDocument Doc =
            ReadAndParseDocument(Options, Options.mDocPath);
        const SectionResolution Resolution =
            ResolveSectionByQuery(Doc.mLines, Doc.mHeadings, Options.mSection);
        if (!Resolution.mbFound) {
          throw std::runtime_error("Section not found: " + Options.mSection);
        }

        // Extract content lines (skip the heading line itself).
        const int ContentStartLine = Resolution.mStartLine + 1;
        const int ContentEndLine = Resolution.mEndLine;
        std::vector<std::string> ContentLines;
        for (int LineNumber = ContentStartLine; LineNumber <= ContentEndLine;
             ++LineNumber) {
          ContentLines.push_back(
              Doc.mLines[static_cast<size_t>(LineNumber - 1)]);
        }

        // Parse tables within the content line range.
        // ParseMarkdownTables works on the full document, so we filter to
        // tables that fall within our section content range.
        const std::vector<MarkdownTableRecord> AllTables =
            ParseMarkdownTables(Doc.mLines, Doc.mHeadings);
        std::vector<MarkdownTableRecord> SectionTables;
        for (const MarkdownTableRecord &Table : AllTables) {
          if (Table.mStartLine >= ContentStartLine &&
              Table.mEndLine <= ContentEndLine) {
            SectionTables.push_back(Table);
          }
        }

        std::vector<std::string> Warnings;
        NormalizeWarnings(Warnings);

        if (Options.mbHuman) {
          return RunSectionContentHuman(
              Doc.mRelativePath, Resolution, Options.mLineCharLimit,
              ContentStartLine, ContentLines, SectionTables, Warnings);
        }
        if (!Options.mbJson) {
          return RunSectionContentText(Doc.mRelativePath, Resolution,
                                       Options.mLineCharLimit, ContentStartLine,
                                       ContentLines, Warnings);
        }
        return RunSectionContentJson(GetUtcNow(), ToGenericPath(Doc.mRepoRoot),
                                     Doc.mRelativePath, Resolution,
                                     Options.mLineCharLimit, ContentStartLine,
                                     ContentLines, SectionTables, Warnings);
      }

      if (Subcommand == "list") {
        const std::vector<std::string> Args(Tokens.begin() + 2, Tokens.end());
        const SectionListOptions Options = ParseSectionListOptions(Args);

        if (!Options.mDocPath.empty()) {
          // Single-doc mode: show heading tree
          const ResolvedDocument Doc =
              ReadAndParseDocument(Options, Options.mDocPath);
          if (Options.mbHuman) {
            return RunSectionListDocHuman(Doc.mRelativePath, Doc.mHeadings);
          }
          if (!Options.mbJson) {
            return RunSectionListDocText(Doc.mRelativePath, Doc.mHeadings);
          }
          return RunSectionListDocJson(GetUtcNow(),
                                       ToGenericPath(Doc.mRepoRoot),
                                       Doc.mRelativePath, Doc.mHeadings);
        }

        // Inventory mode: scan all docs, aggregate literal headings.
        const Inventory ParsedInventory =
            BuildInventory(Options.mRepoRoot, UseCache, Config.mCacheDir,
                           Config.mbCacheVerbose);
        const fs::path RepoRoot = NormalizeRepoRootPath(Options.mRepoRoot);

        std::map<std::string, SectionCount> SectionCounts;
        auto CollectSections =
            [&](const std::vector<DocumentRecord> &InRecords) {
              for (const DocumentRecord &Record : InRecords) {
                const fs::path AbsPath =
                    ResolveDocumentAbsolutePath(RepoRoot, Record.mPath);
                std::vector<std::string> Lines;
                std::string ReadError;
                if (!TryReadFileLines(AbsPath, Lines, ReadError)) {
                  continue;
                }
                const std::vector<HeadingRecord> Headings =
                    ParseHeadingRecords(Lines);
                for (const HeadingRecord &Heading : Headings) {
                  SectionCount &Item = SectionCounts[Heading.mText];
                  if (Item.mHeading.empty()) {
                    Item.mHeading = Heading.mText;
                  }
                  if (Item.mSectionId.empty()) {
                    Item.mSectionId = Heading.mSectionId;
                  }
                  Item.mCount += 1;
                }
              }
            };
        CollectSections(ParsedInventory.mPlans);
        CollectSections(ParsedInventory.mPlaybooks);
        CollectSections(ParsedInventory.mImplementations);

        std::vector<SectionCount> SortedCounts;
        SortedCounts.reserve(SectionCounts.size());
        for (const auto &Pair : SectionCounts) {
          SortedCounts.push_back(Pair.second);
        }
        std::sort(SortedCounts.begin(), SortedCounts.end(),
                  [](const SectionCount &InA, const SectionCount &InB) {
                    const std::string A = ToLower(InA.mHeading);
                    const std::string B = ToLower(InB.mHeading);
                    if (A == B) {
                      return InA.mSectionId < InB.mSectionId;
                    }
                    return A < B;
                  });

        if (Options.mbHuman) {
          return RunSectionListHuman(SortedCounts, Options.mbCount);
        }
        if (!Options.mbJson) {
          return RunSectionListText(SortedCounts, Options.mbCount);
        }
        return RunSectionListJson(GetUtcNow(), ToGenericPath(RepoRoot),
                                  SortedCounts, Options.mbCount);
      }

      throw UsageError("Unknown section subcommand: " + Subcommand);
    }

    if (Command == "excerpt") {
      const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());
      if (ContainsHelpFlag(Args)) {
        PrintCommandUsage("excerpt");
        return 0;
      }
      const ExcerptOptions Options = ParseExcerptOptions(Args);
      const ResolvedDocument Doc =
          ReadAndParseDocument(Options, Options.mDocPath);
      const SectionResolution Resolution =
          ResolveSectionByQuery(Doc.mLines, Doc.mHeadings, Options.mSection);
      if (!Resolution.mbFound) {
        throw std::runtime_error("Section not found for excerpt: " +
                                 Options.mSection);
      }

      const int ExcerptStartLine =
          (std::max)(1, Resolution.mStartLine - Options.mContextLines);
      const int ExcerptEndLine =
          (std::min)(static_cast<int>(Doc.mLines.size()),
                     Resolution.mEndLine + Options.mContextLines);
      std::vector<std::string> ExcerptLines;
      for (int LineNumber = ExcerptStartLine; LineNumber <= ExcerptEndLine;
           ++LineNumber) {
        ExcerptLines.push_back(Doc.mLines[static_cast<size_t>(LineNumber - 1)]);
      }

      std::vector<std::string> Warnings;
      if (Options.mbHuman) {
        return RunExcerptHuman(Doc.mRelativePath, Resolution, ExcerptStartLine,
                               ExcerptLines, Warnings);
      }
      if (!Options.mbJson) {
        return RunExcerptText(Doc.mRelativePath, Resolution, ExcerptStartLine,
                              ExcerptLines, Warnings);
      }
      return RunExcerptJson(GetUtcNow(), ToGenericPath(Doc.mRepoRoot),
                            Doc.mRelativePath, Resolution,
                            Options.mContextLines, ExcerptStartLine,
                            ExcerptLines, Warnings);
    }

    if (Command == "table") {
      if (ContainsHelpFlag(
              std::vector<std::string>(Tokens.begin() + 1, Tokens.end()))) {
        PrintCommandUsage("table");
        return 0;
      }
      if (Tokens.size() < 2) {
        throw UsageError("Missing subcommand for table (expected: list|get)");
      }

      const std::string Subcommand = ToLower(Tokens[1]);
      if (Subcommand == "list") {
        const std::vector<std::string> Args(Tokens.begin() + 2, Tokens.end());
        const TableListOptions Options = ParseTableListOptions(Args);
        const ResolvedDocument Doc =
            ReadAndParseDocument(Options, Options.mDocPath);
        const std::vector<MarkdownTableRecord> Tables =
            ParseMarkdownTables(Doc.mLines, Doc.mHeadings);
        std::vector<std::string> Warnings;
        if (Options.mbHuman) {
          return RunTableListHuman(Doc.mRelativePath, Tables, Warnings);
        }
        if (!Options.mbJson) {
          return RunTableListText(Doc.mRelativePath, Tables, Warnings);
        }
        return RunTableListJson(GetUtcNow(), ToGenericPath(Doc.mRepoRoot),
                                Doc.mRelativePath, Tables, Warnings);
      }

      if (Subcommand == "get") {
        const std::vector<std::string> Args(Tokens.begin() + 2, Tokens.end());
        const TableGetOptions Options = ParseTableGetOptions(Args);
        const ResolvedDocument Doc =
            ReadAndParseDocument(Options, Options.mDocPath);
        const std::vector<MarkdownTableRecord> Tables =
            ParseMarkdownTables(Doc.mLines, Doc.mHeadings);
        const auto FoundTable =
            std::find_if(Tables.begin(), Tables.end(),
                         [&Options](const MarkdownTableRecord &InTable) {
                           return InTable.mTableId == Options.mTableId;
                         });

        if (FoundTable == Tables.end()) {
          throw UsageError("Unknown table id for --table-id: " +
                           std::to_string(Options.mTableId));
        }

        std::vector<std::string> Warnings;
        if (Options.mbHuman) {
          return RunTableGetHuman(Doc.mRelativePath, *FoundTable, Warnings);
        }
        if (!Options.mbJson) {
          return RunTableGetText(Doc.mRelativePath, *FoundTable, Warnings);
        }
        return RunTableGetJson(GetUtcNow(), ToGenericPath(Doc.mRepoRoot),
                               Doc.mRelativePath, *FoundTable, Warnings);
      }

      throw UsageError("Unknown table subcommand: " + Subcommand);
    }

    if (Command == "graph") {
      const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());
      if (ContainsHelpFlag(Args)) {
        PrintCommandUsage("graph");
        return 0;
      }
      const GraphOptions Options = ParseGraphOptions(Args);
      const fs::path RepoRoot = NormalizeRepoRootPath(Options.mRepoRoot);
      const Inventory ParsedInventory = BuildInventory(
          Options.mRepoRoot, UseCache, Config.mCacheDir, Config.mbCacheVerbose);
      const std::string TopicKey =
          ResolveTopicKeyFromInventory(ParsedInventory, Options.mTopic);

      std::vector<std::string> Warnings = ParsedInventory.mWarnings;
      const std::vector<MarkdownDocument> Documents =
          EnumerateMarkdownDocuments(RepoRoot, Warnings);
      const std::map<std::string, fs::path> PathMap =
          BuildMarkdownPathMap(Documents);

      std::map<std::string, std::set<std::string>> Outgoing;
      std::map<std::string, std::set<std::string>> Incoming;
      BuildReferenceGraph(RepoRoot, PathMap, Warnings, Outgoing, Incoming);

      const std::vector<std::string> SeedPaths =
          CollectTopicSeedPaths(ParsedInventory, TopicKey);
      if (SeedPaths.empty()) {
        AddWarning(Warnings, "No plan/implementation/playbook/sidecar "
                             "artifacts found for topic '" +
                                 TopicKey + "'.");
      }

      std::map<std::string, int> DistanceByPath;
      std::queue<std::string> WorkQueue;
      for (const std::string &SeedPath : SeedPaths) {
        if (PathMap.count(SeedPath) == 0) {
          AddWarning(Warnings, "Seed path missing from markdown corpus: '" +
                                   SeedPath + "'.");
          continue;
        }
        if (DistanceByPath.count(SeedPath) > 0) {
          continue;
        }
        DistanceByPath[SeedPath] = 0;
        WorkQueue.push(SeedPath);
      }

      while (!WorkQueue.empty()) {
        const std::string CurrentPath = WorkQueue.front();
        WorkQueue.pop();
        const int CurrentDepth = DistanceByPath[CurrentPath];
        if (CurrentDepth >= Options.mDepth) {
          continue;
        }

        std::set<std::string> Neighbors;
        const auto OutgoingIt = Outgoing.find(CurrentPath);
        if (OutgoingIt != Outgoing.end()) {
          Neighbors.insert(OutgoingIt->second.begin(),
                           OutgoingIt->second.end());
        }
        const auto IncomingIt = Incoming.find(CurrentPath);
        if (IncomingIt != Incoming.end()) {
          Neighbors.insert(IncomingIt->second.begin(),
                           IncomingIt->second.end());
        }

        for (const std::string &NeighborPath : Neighbors) {
          if (DistanceByPath.count(NeighborPath) > 0) {
            continue;
          }
          DistanceByPath[NeighborPath] = CurrentDepth + 1;
          WorkQueue.push(NeighborPath);
        }
      }

      std::map<std::string, GraphNode> NodesById;
      for (const auto &DistanceEntry : DistanceByPath) {
        EnsureGraphNode(DistanceEntry.first, NodesById);
      }

      std::vector<GraphEdge> Edges;
      std::set<std::string> EdgeKeys;
      for (const auto &OutgoingEntry : Outgoing) {
        const std::string &SourcePath = OutgoingEntry.first;
        const auto SourceDistanceIt = DistanceByPath.find(SourcePath);
        if (SourceDistanceIt == DistanceByPath.end()) {
          continue;
        }

        const std::string SourceNodeId = EnsureGraphNode(SourcePath, NodesById);
        for (const std::string &TargetPath : OutgoingEntry.second) {
          const auto TargetDistanceIt = DistanceByPath.find(TargetPath);
          if (TargetDistanceIt == DistanceByPath.end()) {
            continue;
          }

          const std::string TargetNodeId =
              EnsureGraphNode(TargetPath, NodesById);
          const int EdgeDepth =
              (std::max)(SourceDistanceIt->second, TargetDistanceIt->second);
          AppendGraphEdgeUnique(Edges, EdgeKeys, SourceNodeId, TargetNodeId,
                                "references", EdgeDepth);
        }
      }

      std::vector<GraphNode> Nodes;
      Nodes.reserve(NodesById.size());
      for (const auto &NodeEntry : NodesById) {
        Nodes.push_back(NodeEntry.second);
      }
      std::sort(Nodes.begin(), Nodes.end(),
                [](const GraphNode &InLeft, const GraphNode &InRight) {
                  if (InLeft.mId != InRight.mId) {
                    return InLeft.mId < InRight.mId;
                  }
                  return InLeft.mPath < InRight.mPath;
                });
      std::sort(Edges.begin(), Edges.end(),
                [](const GraphEdge &InLeft, const GraphEdge &InRight) {
                  if (InLeft.mFromNodeId != InRight.mFromNodeId) {
                    return InLeft.mFromNodeId < InRight.mFromNodeId;
                  }
                  if (InLeft.mToNodeId != InRight.mToNodeId) {
                    return InLeft.mToNodeId < InRight.mToNodeId;
                  }
                  if (InLeft.mKind != InRight.mKind) {
                    return InLeft.mKind < InRight.mKind;
                  }
                  return InLeft.mDepth < InRight.mDepth;
                });
      NormalizeWarnings(Warnings);

      if (Options.mbHuman) {
        return RunGraphHuman(TopicKey, Options.mDepth, Nodes, Edges, Warnings);
      }
      if (!Options.mbJson) {
        return RunGraphText(TopicKey, Options.mDepth, Nodes, Edges, Warnings);
      }
      return RunGraphJson(GetUtcNow(), ToGenericPath(RepoRoot), TopicKey,
                          Options.mDepth, Nodes, Edges, Warnings);
    }

    if (Command == "diagnose") {
      if (ContainsHelpFlag(
              std::vector<std::string>(Tokens.begin() + 1, Tokens.end()))) {
        PrintCommandUsage("diagnose");
        return 0;
      }
      if (Tokens.size() < 2) {
        throw UsageError("Missing subcommand for diagnose (expected: drift)");
      }

      const std::string Subcommand = ToLower(Tokens[1]);
      if (Subcommand != "drift") {
        throw UsageError("Unknown diagnose subcommand: " + Subcommand);
      }

      const std::vector<std::string> Args(Tokens.begin() + 2, Tokens.end());
      const DiagnoseDriftOptions Options = ParseDiagnoseDriftOptions(Args);
      const fs::path RepoRoot = NormalizeRepoRootPath(Options.mRepoRoot);
      const Inventory ParsedInventory = BuildInventory(
          Options.mRepoRoot, UseCache, Config.mCacheDir, Config.mbCacheVerbose);

      std::vector<std::string> Warnings = ParsedInventory.mWarnings;
      const std::vector<MarkdownDocument> Documents =
          EnumerateMarkdownDocuments(RepoRoot, Warnings);
      const std::map<std::string, fs::path> PathMap =
          BuildMarkdownPathMap(Documents);

      std::map<std::string, std::set<std::string>> Outgoing;
      std::map<std::string, std::set<std::string>> Incoming;
      BuildReferenceGraph(RepoRoot, PathMap, Warnings, Outgoing, Incoming);

      std::vector<DriftItem> Drifts;
      const auto HasSidecar =
          [&ParsedInventory](const std::string &InOwnerKind,
                             const std::string &InTopicKey,
                             const std::string &InPhaseKey,
                             const std::string &InDocKind) -> bool {
        for (const SidecarRecord &Sidecar : ParsedInventory.mSidecars) {
          if (Sidecar.mOwnerKind == InOwnerKind &&
              Sidecar.mTopicKey == InTopicKey &&
              Sidecar.mPhaseKey == InPhaseKey &&
              Sidecar.mDocKind == InDocKind) {
            return true;
          }
        }
        return false;
      };

      for (const TopicPairRecord &Pair : ParsedInventory.mPairs) {
        if (Pair.mPairState == "paired") {
          continue;
        }

        std::string PairPath = Pair.mPlanPath;
        if (PairPath.empty()) {
          PairPath = Pair.mImplementationPath;
        }
        if (PairPath.empty() && !Pair.mPlaybooks.empty()) {
          PairPath = Pair.mPlaybooks.front().mPath;
        }

        AddDriftItem(Drifts, "pair_state_" + Pair.mPairState, "error",
                     Pair.mTopicKey, PairPath,
                     "Topic pair-state drift detected: " + Pair.mPairState);
      }

      for (const DocumentRecord &Plan : ParsedInventory.mPlans) {
        if (!HasSidecar("Plan", Plan.mTopicKey, "", kSidecarChangeLog)) {
          AddDriftItem(Drifts, "missing_plan_changelog_sidecar", "warning",
                       Plan.mTopicKey, Plan.mPath,
                       "Plan is missing detached changelog sidecar.");
        }
        if (!HasSidecar("Plan", Plan.mTopicKey, "", kSidecarVerification)) {
          AddDriftItem(Drifts, "missing_plan_verification_sidecar", "warning",
                       Plan.mTopicKey, Plan.mPath,
                       "Plan is missing detached verification sidecar.");
        }
      }

      for (const DocumentRecord &Implementation :
           ParsedInventory.mImplementations) {
        if (!HasSidecar("Impl", Implementation.mTopicKey, "",
                        kSidecarChangeLog)) {
          AddDriftItem(
              Drifts, "missing_implementation_changelog_sidecar", "warning",
              Implementation.mTopicKey, Implementation.mPath,
              "Implementation tracker is missing detached changelog sidecar.");
        }
        if (!HasSidecar("Impl", Implementation.mTopicKey, "",
                        kSidecarVerification)) {
          AddDriftItem(Drifts, "missing_implementation_verification_sidecar",
                       "warning", Implementation.mTopicKey,
                       Implementation.mPath,
                       "Implementation tracker is missing detached "
                       "verification sidecar.");
        }
      }

      for (const DocumentRecord &Playbook : ParsedInventory.mPlaybooks) {
        if (!HasSidecar("Playbook", Playbook.mTopicKey, Playbook.mPhaseKey,
                        kSidecarChangeLog)) {
          AddDriftItem(
              Drifts, "missing_playbook_changelog_sidecar", "warning",
              Playbook.mTopicKey, Playbook.mPath,
              "Playbook is missing detached changelog sidecar for phase '" +
                  Playbook.mPhaseKey + "'.");
        }
        if (!HasSidecar("Playbook", Playbook.mTopicKey, Playbook.mPhaseKey,
                        kSidecarVerification)) {
          AddDriftItem(
              Drifts, "missing_playbook_verification_sidecar", "warning",
              Playbook.mTopicKey, Playbook.mPath,
              "Playbook is missing detached verification sidecar for phase '" +
                  Playbook.mPhaseKey + "'.");
        }
      }

      const auto HasReference =
          [&Outgoing](const std::string &InSourcePath,
                      const std::string &InTargetPath) -> bool {
        const auto SourceIt = Outgoing.find(InSourcePath);
        if (SourceIt == Outgoing.end()) {
          return false;
        }
        return SourceIt->second.count(InTargetPath) > 0;
      };

      for (const TopicPairRecord &Pair : ParsedInventory.mPairs) {
        if (Pair.mPlanPath.empty() || Pair.mImplementationPath.empty()) {
          continue;
        }

        if (!HasReference(Pair.mPlanPath, Pair.mImplementationPath)) {
          AddDriftItem(
              Drifts, "plan_missing_implementation_link", "warning",
              Pair.mTopicKey, Pair.mPlanPath,
              "Plan does not reference paired implementation tracker path.");
        }
        if (!HasReference(Pair.mImplementationPath, Pair.mPlanPath)) {
          AddDriftItem(
              Drifts, "implementation_missing_plan_link", "warning",
              Pair.mTopicKey, Pair.mImplementationPath,
              "Implementation tracker does not reference paired plan path.");
        }

        for (const DocumentRecord &Playbook : Pair.mPlaybooks) {
          if (!HasReference(Playbook.mPath, Pair.mPlanPath)) {
            AddDriftItem(Drifts, "playbook_missing_plan_link", "warning",
                         Pair.mTopicKey, Playbook.mPath,
                         "Playbook does not reference paired plan path.");
          }
          if (!HasReference(Playbook.mPath, Pair.mImplementationPath)) {
            AddDriftItem(
                Drifts, "playbook_missing_implementation_link", "warning",
                Pair.mTopicKey, Playbook.mPath,
                "Playbook does not reference paired implementation path.");
          }
        }
      }

      const auto ResolveIndexPathForRecord =
          [](const DocumentRecord &InRecord) -> std::string {
        if (InRecord.mPath.rfind("Docs/", 0) == 0) {
          return "Docs/INDEX.md";
        }

        const size_t DocsMarker = InRecord.mPath.find("/Docs/");
        if (DocsMarker == std::string::npos) {
          return "";
        }
        return InRecord.mPath.substr(0, DocsMarker +
                                            std::string("/Docs/").size()) +
               "README.md";
      };

      const auto CheckIndexLink = [&](const DocumentRecord &InRecord) {
        const std::string IndexPath = ResolveIndexPathForRecord(InRecord);
        if (IndexPath.empty() || PathMap.count(IndexPath) == 0) {
          return;
        }
        if (HasReference(IndexPath, InRecord.mPath)) {
          return;
        }

        AddDriftItem(Drifts, "index_missing_doc_link", "warning",
                     InRecord.mTopicKey, IndexPath,
                     "Index/readme path does not reference core document '" +
                         InRecord.mPath + "'.");
      };

      for (const DocumentRecord &Plan : ParsedInventory.mPlans) {
        CheckIndexLink(Plan);
      }
      for (const DocumentRecord &Playbook : ParsedInventory.mPlaybooks) {
        CheckIndexLink(Playbook);
      }
      for (const DocumentRecord &Implementation :
           ParsedInventory.mImplementations) {
        CheckIndexLink(Implementation);
      }

      std::sort(Drifts.begin(), Drifts.end(),
                [](const DriftItem &InLeft, const DriftItem &InRight) {
                  const int LeftRank = SeverityRank(InLeft.mSeverity);
                  const int RightRank = SeverityRank(InRight.mSeverity);
                  if (LeftRank != RightRank) {
                    return LeftRank < RightRank;
                  }
                  if (InLeft.mTopicKey != InRight.mTopicKey) {
                    return InLeft.mTopicKey < InRight.mTopicKey;
                  }
                  if (InLeft.mPath != InRight.mPath) {
                    return InLeft.mPath < InRight.mPath;
                  }
                  if (InLeft.mId != InRight.mId) {
                    return InLeft.mId < InRight.mId;
                  }
                  return InLeft.mMessage < InRight.mMessage;
                });
      NormalizeWarnings(Warnings);
      const bool Ok = Drifts.empty();
      if (Options.mbHuman) {
        return RunDiagnoseDriftHuman(Ok, Drifts, Warnings);
      }
      if (!Options.mbJson) {
        return RunDiagnoseDriftText(Ok, Drifts, Warnings);
      }
      return RunDiagnoseDriftJson(GetUtcNow(), ToGenericPath(RepoRoot), Ok,
                                  Drifts, Warnings);
    }

    if (Command == "timeline") {
      const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());
      if (ContainsHelpFlag(Args)) {
        PrintCommandUsage("timeline");
        return 0;
      }
      const TimelineOptions Options = ParseTimelineOptions(Args);
      const fs::path RepoRoot = NormalizeRepoRootPath(Options.mRepoRoot);
      const Inventory ParsedInventory = BuildInventory(
          Options.mRepoRoot, UseCache, Config.mCacheDir, Config.mbCacheVerbose);
      const std::string TopicKey =
          ResolveTopicKeyFromInventory(ParsedInventory, Options.mTopic);
      const std::vector<SidecarRecord> TopicSidecars =
          CollectSidecarsByTopic(ParsedInventory.mSidecars, TopicKey);

      std::vector<std::string> Warnings = ParsedInventory.mWarnings;
      std::vector<TimelineItem> Items;
      bool HasChangeLogSidecar = false;
      for (const SidecarRecord &Sidecar : TopicSidecars) {
        if (Sidecar.mDocKind != kSidecarChangeLog) {
          continue;
        }
        HasChangeLogSidecar = true;

        const fs::path AbsolutePath = RepoRoot / fs::path(Sidecar.mPath);
        const std::vector<EvidenceEntry> Entries = ParseEvidenceEntriesFromFile(
            AbsolutePath, Sidecar.mPath, Sidecar.mPhaseKey, Warnings);
        for (const EvidenceEntry &Entry : Entries) {
          const std::map<std::string, std::string> FieldMap =
              BuildFieldMap(Entry.mFields);
          const std::string Date = GetFirstFieldValue(FieldMap, {"date"});
          if (!Options.mSince.empty()) {
            if (!Date.empty() && LooksLikeIsoDate(Date)) {
              if (Date < Options.mSince) {
                continue;
              }
            } else if (!Date.empty()) {
              AddWarning(Warnings, "Timeline row in '" + Sidecar.mPath +
                                       "' has non-ISO date '" + Date +
                                       "' (expected YYYY-MM-DD).");
            }
          }

          TimelineItem Item;
          Item.mDate = Date;
          Item.mDocClass = ToDocClassFromOwnerKind(Sidecar.mOwnerKind);
          Item.mPhaseKey = Sidecar.mPhaseKey;
          Item.mSourcePath = Sidecar.mPath;
          Item.mUpdate = GetFirstFieldValue(
              FieldMap, {"update", "changes", "change", "summary", "item"});
          if (Item.mUpdate.empty()) {
            Item.mUpdate = "Row " + std::to_string(Entry.mRowIndex) + " update";
          }
          Item.mEvidence = GetFirstFieldValue(
              FieldMap, {"evidence", "validation", "notes", "details"});
          Items.push_back(std::move(Item));
        }
      }

      if (!HasChangeLogSidecar) {
        AddWarning(Warnings,
                   "No changelog sidecars found for topic '" + TopicKey + "'.");
      }

      std::sort(Items.begin(), Items.end(),
                [](const TimelineItem &InLeft, const TimelineItem &InRight) {
                  const bool LeftHasDate = !InLeft.mDate.empty();
                  const bool RightHasDate = !InRight.mDate.empty();
                  if (LeftHasDate != RightHasDate) {
                    return LeftHasDate;
                  }
                  if (InLeft.mDate != InRight.mDate) {
                    return InLeft.mDate < InRight.mDate;
                  }
                  if (InLeft.mDocClass != InRight.mDocClass) {
                    return InLeft.mDocClass < InRight.mDocClass;
                  }
                  if (InLeft.mPhaseKey != InRight.mPhaseKey) {
                    return InLeft.mPhaseKey < InRight.mPhaseKey;
                  }
                  if (InLeft.mSourcePath != InRight.mSourcePath) {
                    return InLeft.mSourcePath < InRight.mSourcePath;
                  }
                  return InLeft.mUpdate < InRight.mUpdate;
                });
      NormalizeWarnings(Warnings);
      if (Options.mbHuman) {
        return RunTimelineHuman(TopicKey, Options.mSince, Items, Warnings);
      }
      if (!Options.mbJson) {
        return RunTimelineText(TopicKey, Options.mSince, Items, Warnings);
      }
      return RunTimelineJson(GetUtcNow(), ToGenericPath(RepoRoot), TopicKey,
                             Options.mSince, Items, Warnings);
    }

    if (Command == "blockers") {
      const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());
      if (ContainsHelpFlag(Args)) {
        PrintCommandUsage("blockers");
        return 0;
      }
      const BlockersOptions Options = ParseBlockersOptions(Args);
      const fs::path RepoRoot = NormalizeRepoRootPath(Options.mRepoRoot);
      const Inventory ParsedInventory = BuildInventory(
          Options.mRepoRoot, UseCache, Config.mCacheDir, Config.mbCacheVerbose);

      std::vector<std::string> Warnings = ParsedInventory.mWarnings;
      std::vector<BlockerItem> Items;
      const auto TryAppendItem = [&Items, &Options](const BlockerItem &InItem) {
        if (!MatchesBlockerStatusFilter(Options.mStatus, InItem.mStatus)) {
          return;
        }
        Items.push_back(InItem);
      };

      for (const TopicPairRecord &Pair : ParsedInventory.mPairs) {
        if (Pair.mPairState == "paired") {
          continue;
        }

        BlockerItem Item;
        Item.mTopicKey = Pair.mTopicKey;
        Item.mSourcePath = Pair.mPlanPath;
        if (Item.mSourcePath.empty()) {
          Item.mSourcePath = Pair.mImplementationPath;
        }
        if (Item.mSourcePath.empty() && !Pair.mPlaybooks.empty()) {
          Item.mSourcePath = Pair.mPlaybooks.front().mPath;
        }
        Item.mKind = "pair_state";
        Item.mStatus = "blocked";
        Item.mPriority = "high";
        Item.mAction = "Resolve pair-state drift.";
        if (Pair.mPairState == "missing_implementation") {
          Item.mAction = "Create paired implementation tracker.";
        } else if (Pair.mPairState == "missing_phase_playbook") {
          Item.mAction = "Create phase-scoped playbook for active phase.";
        } else if (Pair.mPairState == "orphan_implementation") {
          Item.mAction =
              "Create paired plan owner or relocate implementation tracker.";
        } else if (Pair.mPairState == "orphan_playbook") {
          Item.mAction =
              "Create paired plan and implementation for this playbook topic.";
        }
        Item.mNotes = "pair_state=" + Pair.mPairState;
        TryAppendItem(Item);
      }

      const auto AppendDocumentBlockers =
          [&RepoRoot, &Warnings,
           &TryAppendItem](const std::vector<DocumentRecord> &InRecords,
                           const std::string &InDocClass) {
            for (const DocumentRecord &Record : InRecords) {
              const std::vector<BlockerItem> DocumentItems =
                  CollectBlockerItemsFromDocument(RepoRoot, Record, InDocClass,
                                                  Warnings);
              for (const BlockerItem &Item : DocumentItems) {
                TryAppendItem(Item);
              }
            }
          };

      AppendDocumentBlockers(ParsedInventory.mPlans, "plan");
      AppendDocumentBlockers(ParsedInventory.mPlaybooks, "playbook");
      AppendDocumentBlockers(ParsedInventory.mImplementations,
                             "implementation");

      std::vector<BlockerItem> UniqueItems;
      std::set<std::string> ItemKeys;
      for (const BlockerItem &Item : Items) {
        const std::string Key =
            Item.mTopicKey + "|" + Item.mSourcePath + "|" + Item.mKind + "|" +
            Item.mStatus + "|" + Item.mPhaseKey + "|" + Item.mPriority + "|" +
            Item.mAction + "|" + Item.mOwner + "|" + Item.mNotes;
        if (ItemKeys.insert(Key).second) {
          UniqueItems.push_back(Item);
        }
      }

      std::sort(UniqueItems.begin(), UniqueItems.end(),
                [](const BlockerItem &InLeft, const BlockerItem &InRight) {
                  const int LeftRank = BlockerStatusRank(InLeft.mStatus);
                  const int RightRank = BlockerStatusRank(InRight.mStatus);
                  if (LeftRank != RightRank) {
                    return LeftRank < RightRank;
                  }
                  if (InLeft.mTopicKey != InRight.mTopicKey) {
                    return InLeft.mTopicKey < InRight.mTopicKey;
                  }
                  if (InLeft.mSourcePath != InRight.mSourcePath) {
                    return InLeft.mSourcePath < InRight.mSourcePath;
                  }
                  if (InLeft.mPhaseKey != InRight.mPhaseKey) {
                    return InLeft.mPhaseKey < InRight.mPhaseKey;
                  }
                  if (InLeft.mAction != InRight.mAction) {
                    return InLeft.mAction < InRight.mAction;
                  }
                  return InLeft.mKind < InRight.mKind;
                });
      NormalizeWarnings(Warnings);

      if (Options.mbHuman) {
        return RunBlockersHuman(Options.mStatus, UniqueItems, Warnings);
      }
      if (!Options.mbJson) {
        return RunBlockersText(Options.mStatus, UniqueItems, Warnings);
      }
      return RunBlockersJson(GetUtcNow(), ToGenericPath(RepoRoot),
                             Options.mStatus, UniqueItems, Warnings);
    }

    if (Command == "cache") {
      if (ContainsHelpFlag(
              std::vector<std::string>(Tokens.begin() + 1, Tokens.end()))) {
        PrintCommandUsage("cache");
        return 0;
      }

      // Default subcommand: info (bare "uni-plan cache" → "uni-plan cache
      // info")
      std::string Subcommand = "info";
      size_t ArgsStart = 1;
      if (Tokens.size() >= 2) {
        const std::string Candidate = ToLower(Tokens[1]);
        if (Candidate == "info" || Candidate == "clear" ||
            Candidate == "config") {
          Subcommand = Candidate;
          ArgsStart = 2;
        }
      }

      const std::vector<std::string> Args(
          Tokens.begin() + static_cast<std::ptrdiff_t>(ArgsStart),
          Tokens.end());

      if (Subcommand == "info") {
        const CacheInfoOptions Options = ParseCacheInfoOptions(Args);
        const fs::path RepoRoot = NormalizeRepoRootPath(Options.mRepoRoot);
        const CacheInfoResult Result =
            BuildCacheInfo(Options.mRepoRoot, Config);
        if (Options.mbHuman) {
          return RunCacheInfoHuman(Result);
        }
        if (!Options.mbJson) {
          return RunCacheInfoText(Result);
        }
        return RunCacheInfoJson(ToGenericPath(RepoRoot), Result);
      }

      if (Subcommand == "clear") {
        const CacheClearOptions Options = ParseCacheClearOptions(Args);
        const fs::path RepoRoot = NormalizeRepoRootPath(Options.mRepoRoot);
        const CacheClearResult Result = ClearCache(Options.mRepoRoot, Config);
        if (Options.mbHuman) {
          return RunCacheClearHuman(Result);
        }
        if (!Options.mbJson) {
          return RunCacheClearText(Result);
        }
        return RunCacheClearJson(ToGenericPath(RepoRoot), Result);
      }

      if (Subcommand == "config") {
        const CacheConfigOptions Options = ParseCacheConfigOptions(Args);
        const fs::path RepoRoot = NormalizeRepoRootPath(Options.mRepoRoot);
        const CacheConfigResult Result =
            WriteCacheConfig(Options.mRepoRoot, Options, Config);
        if (Options.mbHuman) {
          return RunCacheConfigHuman(Result);
        }
        if (!Options.mbJson) {
          return RunCacheConfigText(Result);
        }
        return RunCacheConfigJson(ToGenericPath(RepoRoot), Result);
      }

      throw UsageError("Unknown cache subcommand: " + Subcommand);
    }

#ifdef UPLAN_WATCH
    if (Command == "watch") {
      const std::vector<std::string> Args(Tokens.begin() + 1, Tokens.end());
      BaseOptions WatchOptions;
      ConsumeCommonOptions(Args, WatchOptions, true);
      const std::string WatchRoot =
          WatchOptions.mRepoRoot.empty() ? "." : WatchOptions.mRepoRoot;
      return RunDocWatch(WatchRoot, Config);
    }
#endif

    throw UsageError("Unknown command: " + Command);
  } catch (const UsageError &InError) {
    std::cerr << kColorRed << "error: " << InError.what() << kColorReset
              << "\n\n";
    std::cout << kColorDim;
    PrintCommandUsage(Command);
    std::cout << kColorReset;
    return 2;
  }
}
} // namespace UniPlan
