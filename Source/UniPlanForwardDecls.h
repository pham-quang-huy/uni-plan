#pragma once

#include "UniPlanTopicTypes.h"
#include "UniPlanTypes.h"

#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace UniPlan
{

// Shared regex constant
extern const std::regex kMarkdownPathRegex;

// From UniPlanCommandTopic.cpp — shared topic-metadata JSON emitters used by
// both topic queries and phase queries.
void EmitValidationCommandsJson(
    const char *InName, const std::vector<FValidationCommand> &InCommands,
    bool InTrailingComma = true);
void EmitDependenciesJson(const char *InName,
                          const std::vector<FBundleReference> &InDependencies,
                          bool InTrailingComma = true);
void EmitRisksJson(const char *InName, const std::vector<FRiskEntry> &InRisks,
                   bool InTrailingComma = true,
                   const std::vector<size_t> *InOriginalIndices = nullptr);
void EmitNextActionsJson(
    const char *InName, const std::vector<FNextActionEntry> &InActions,
    bool InTrailingComma = true,
    const std::vector<size_t> *InOriginalIndices = nullptr);
void EmitAcceptanceCriteriaJson(
    const char *InName,
    const std::vector<FAcceptanceCriterionEntry> &InCriteria,
    bool InTrailingComma = true,
    const std::vector<size_t> *InOriginalIndices = nullptr);

// v0.98.0 typed-array emitters: priority_groupings / runbooks /
// residual_risks. Same shape as EmitRisksJson etc. — each emits an
// `index` field per entry so `<group> set --index <N>` targets the
// stable storage position even after filtering.
void EmitPriorityGroupingsJson(
    const char *InName, const std::vector<FPriorityGrouping> &InGroupings,
    bool InTrailingComma = true,
    const std::vector<size_t> *InOriginalIndices = nullptr);
void EmitRunbooksJson(const char *InName,
                      const std::vector<FRunbookProcedure> &InRunbooks,
                      bool InTrailingComma = true,
                      const std::vector<size_t> *InOriginalIndices = nullptr);
void EmitResidualRisksJson(
    const char *InName, const std::vector<FResidualRiskEntry> &InRisks,
    bool InTrailingComma = true,
    const std::vector<size_t> *InOriginalIndices = nullptr);

// From DocCache.cpp
fs::path ResolveExecutableDirectory();
std::string ExpandEnvVars(const std::string &InValue);
IniData ParseIniFile(const fs::path &InPath);
DocConfig LoadConfig(const fs::path &InExeDir);
// Fnv1aUpdateByte/String/Uint64 and ToHexString are now inline in
// UniPlanHashHelpers.h. Include that header directly where needed.
fs::path ResolveCacheRoot(const std::string &InConfigCacheDir);
fs::path BuildInventoryCachePath(const fs::path &InRepoRoot,
                                 const std::string &InConfigCacheDir);
bool TryComputeMarkdownCorpusSignature(const fs::path &InRepoRoot,
                                       uint64_t &OutSignature,
                                       std::string &OutError);
uint64_t ComputeDirectorySize(const fs::path &InPath);
int CountCacheEntries(const fs::path &InCacheRoot);
std::string FormatBytesHuman(uint64_t InBytes);
CacheInfoResult BuildCacheInfo(const std::string &InRepoRoot,
                               const DocConfig &InConfig);
CacheClearResult ClearCache(const std::string &InRepoRoot,
                            const DocConfig &InConfig);
bool TryWriteDocIni(const fs::path &InPath, const std::string &InCacheDir,
                    const std::string &InCacheEnabled,
                    const std::string &InCacheVerbose, std::string &OutError);

// From DocOptionParsing.cpp
bool IsOptionToken(const std::string &InToken);
std::vector<std::string>
ConsumeCommonOptions(const std::vector<std::string> &InTokens,
                     BaseOptions &OutOptions, const bool InAllowPositionalRoot);
std::string ConsumeValuedOption(const std::vector<std::string> &InTokens,
                                size_t &InOutIndex,
                                const std::string &InOptionName);
bool ContainsHelpFlag(const std::vector<std::string> &InTokens);
std::string
ValidateAndNormalizeStatusFilter(const std::string &InRawStatus,
                                 const std::string &InSupportedHint,
                                 const std::set<std::string> *InAllowedValues);
int ParsePositiveInteger(const std::string &InValue,
                         const std::string &InOptionName);
int ParseNonNegativeInteger(const std::string &InValue,
                            const std::string &InOptionName);
CacheInfoOptions
ParseCacheInfoOptions(const std::vector<std::string> &InTokens);
CacheClearOptions
ParseCacheClearOptions(const std::vector<std::string> &InTokens);
CacheConfigOptions
ParseCacheConfigOptions(const std::vector<std::string> &InTokens);

// V4 bundle-native option parsers
FTopicListOptions
ParseTopicListOptions(const std::vector<std::string> &InTokens);
FTopicGetOptions ParseTopicGetOptions(const std::vector<std::string> &InTokens);
FPhaseListOptions
ParsePhaseListOptions(const std::vector<std::string> &InTokens);
FPhaseGetOptions ParsePhaseGetOptions(const std::vector<std::string> &InTokens);
FBundleChangelogOptions
ParseBundleChangelogOptions(const std::vector<std::string> &InTokens);
FBundleVerificationOptions
ParseBundleVerificationOptions(const std::vector<std::string> &InTokens);
FBundleTimelineOptions
ParseBundleTimelineOptions(const std::vector<std::string> &InTokens);
FBundleBlockersOptions
ParseBundleBlockersOptions(const std::vector<std::string> &InTokens);
FBundleValidateOptions
ParseBundleValidateOptions(const std::vector<std::string> &InTokens);
FTopicAddOptions ParseTopicAddOptions(const std::vector<std::string> &InTokens);
FTopicSetOptions ParseTopicSetOptions(const std::vector<std::string> &InTokens);
FPhaseSetOptions ParsePhaseSetOptions(const std::vector<std::string> &InTokens);
FPhaseAddOptions ParsePhaseAddOptions(const std::vector<std::string> &InTokens);
FPhaseNormalizeOptions
ParsePhaseNormalizeOptions(const std::vector<std::string> &InTokens);
FJobSetOptions ParseJobSetOptions(const std::vector<std::string> &InTokens);
FTaskSetOptions ParseTaskSetOptions(const std::vector<std::string> &InTokens);
FChangelogAddOptions
ParseChangelogAddOptions(const std::vector<std::string> &InTokens);
FVerificationAddOptions
ParseVerificationAddOptions(const std::vector<std::string> &InTokens);

// Semantic command option parsers
FPhaseStartOptions
ParsePhaseStartOptions(const std::vector<std::string> &InTokens);
FPhaseCompleteOptions
ParsePhaseCompleteOptions(const std::vector<std::string> &InTokens);
FPhaseBlockOptions
ParsePhaseBlockOptions(const std::vector<std::string> &InTokens);
FPhaseUnblockOptions
ParsePhaseUnblockOptions(const std::vector<std::string> &InTokens);
FPhaseCancelOptions
ParsePhaseCancelOptions(const std::vector<std::string> &InTokens);
FPhaseProgressOptions
ParsePhaseProgressOptions(const std::vector<std::string> &InTokens);
FPhaseCompleteJobsOptions
ParsePhaseCompleteJobsOptions(const std::vector<std::string> &InTokens);
FTopicStartOptions
ParseTopicStartOptions(const std::vector<std::string> &InTokens);
FTopicCompleteOptions
ParseTopicCompleteOptions(const std::vector<std::string> &InTokens);
FTopicBlockOptions
ParseTopicBlockOptions(const std::vector<std::string> &InTokens);
FChangelogAddOptions
ParsePhaseLogOptions(const std::vector<std::string> &InTokens);
FVerificationAddOptions
ParsePhaseVerifyOptions(const std::vector<std::string> &InTokens);
FPhaseQueryOptions
ParsePhaseQueryOptions(const std::vector<std::string> &InTokens);
FLaneSetOptions ParseLaneSetOptions(const std::vector<std::string> &InTokens);
FLaneCompleteOptions
ParseLaneCompleteOptions(const std::vector<std::string> &InTokens);
FPhaseSyncExecutionOptions
ParsePhaseSyncExecutionOptions(const std::vector<std::string> &InTokens);
FTestingAddOptions
ParseTestingAddOptions(const std::vector<std::string> &InTokens);
FManifestAddOptions
ParseManifestAddOptions(const std::vector<std::string> &InTokens);
FTestingSetOptions
ParseTestingSetOptions(const std::vector<std::string> &InTokens);
FVerificationSetOptions
ParseVerificationSetOptions(const std::vector<std::string> &InTokens);
FManifestSetOptions
ParseManifestSetOptions(const std::vector<std::string> &InTokens);
FManifestRemoveOptions
ParseManifestRemoveOptions(const std::vector<std::string> &InTokens);
FManifestListOptions
ParseManifestListOptions(const std::vector<std::string> &InTokens);
FManifestSuggestOptions
ParseManifestSuggestOptions(const std::vector<std::string> &InTokens);
FPhaseDriftOptions
ParsePhaseDriftOptions(const std::vector<std::string> &InTokens);
FLaneAddOptions ParseLaneAddOptions(const std::vector<std::string> &InTokens);
FChangelogSetOptions
ParseChangelogSetOptions(const std::vector<std::string> &InTokens);
FChangelogRemoveOptions
ParseChangelogRemoveOptions(const std::vector<std::string> &InTokens);

// v0.93.0 CRUD symmetry — option parsers for the new entity CRUD surface.
FJobAddOptions ParseJobAddOptions(const std::vector<std::string> &InTokens);
FJobRemoveOptions
ParseJobRemoveOptions(const std::vector<std::string> &InTokens);
FJobListOptions ParseJobListOptions(const std::vector<std::string> &InTokens);
FTaskAddOptions ParseTaskAddOptions(const std::vector<std::string> &InTokens);
FTaskRemoveOptions
ParseTaskRemoveOptions(const std::vector<std::string> &InTokens);
FTaskListOptions ParseTaskListOptions(const std::vector<std::string> &InTokens);
FLaneRemoveOptions
ParseLaneRemoveOptions(const std::vector<std::string> &InTokens);
FLaneListOptions ParseLaneListOptions(const std::vector<std::string> &InTokens);
FTestingRemoveOptions
ParseTestingRemoveOptions(const std::vector<std::string> &InTokens);
FTestingListOptions
ParseTestingListOptions(const std::vector<std::string> &InTokens);
FTopicNormalizeOptions
ParseTopicNormalizeOptions(const std::vector<std::string> &InTokens);

// Legacy-gap option parser (stateless V3 <-> V4 parity audit, 0.75.0+)
FLegacyGapOptions
ParseLegacyGapOptions(const std::vector<std::string> &InTokens);

// From UniPlanOutputHuman.cpp
int RunCacheInfoHuman(const CacheInfoResult &InResult);
int RunCacheClearHuman(const CacheClearResult &InResult);
int RunCacheConfigHuman(const CacheConfigResult &InResult);

// From UniPlanOutputJson.cpp
int RunCacheInfoJson(const std::string &InRepoRoot,
                     const CacheInfoResult &InResult);
int RunCacheClearJson(const std::string &InRepoRoot,
                      const CacheClearResult &InResult);
int RunCacheConfigJson(const std::string &InRepoRoot,
                       const CacheConfigResult &InResult);

// From UniPlanOutputText.cpp
int RunCacheInfoText(const CacheInfoResult &InResult);
int RunCacheClearText(const CacheClearResult &InResult);
int RunCacheConfigText(const CacheConfigResult &InResult);

// From DocParsing.cpp
void SortRecords(std::vector<DocumentRecord> &InOutRecords);
void SortSidecars(std::vector<SidecarRecord> &InOutRecords);
std::string BuildPlaybookIdentity(const std::string &InTopicKey,
                                  const std::string &InPhaseKey);
std::string BuildSidecarIdentity(const SidecarRecord &InRecord);
std::vector<TopicPairRecord>
BuildTopicPairs(const std::vector<DocumentRecord> &InPlans,
                const std::vector<DocumentRecord> &InPlaybooks,
                const std::vector<DocumentRecord> &InImplementations,
                std::vector<std::string> &OutWarnings);
bool ShouldSkipRecursionDirectory(const fs::path &InPath);
fs::path NormalizeRepoRootPath(const std::string &InRepoRoot);
bool IsExcludedDocsScriptPath(const std::string &InRelativePath);
std::vector<MarkdownDocument>
EnumerateMarkdownDocuments(const fs::path &InRepoRoot,
                           std::vector<std::string> &OutWarnings);
bool IsAllowedLintFilename(const std::string &InName);
bool HasFirstNonEmptyLineH1(const fs::path &InPath, std::string &OutError);
bool IsMarkdownTableLine(const std::string &InTrimmedLine);
std::string NormalizeSectionID(const std::string &InHeadingText);
bool ParseFenceDelimiterLine(const std::string &InLine, char &OutFenceChar,
                             size_t &OutFenceLength, std::string &OutRemainder);
std::vector<HeadingRecord>
ParseHeadingRecords(const std::vector<std::string> &InLines);
std::vector<MarkdownTableRecord>
ParseMarkdownTables(const std::vector<std::string> &InLines,
                    const std::vector<HeadingRecord> &InHeadings);
std::string NormalizeHeaderKey(const std::string &InValue);

// From DocValidation.cpp
LintResult BuildLintResult(const std::string &InRepoRoot,
                           const bool InQuiet = false);
std::vector<ValidateCheck>
ValidateAllBundles(const std::vector<FTopicBundle> &InBundles,
                   const fs::path &InRepoRoot = fs::path());

// Shared validation helper — emits a failure entry with the given id,
// severity, topic, path, and detail. Defined in UniPlanValidation.cpp;
// called by both structural and content-hygiene evaluators.
void Fail(std::vector<ValidateCheck> &OutChecks, const std::string &InID,
          EValidationSeverity InSeverity, const std::string &InTopic,
          const std::string &InPath, const std::string &InDetail);

// Content-hygiene evaluators — defined in UniPlanValidationContent.cpp.
// Called by ValidateAllBundles in UniPlanValidation.cpp.
void EvalPathResolves(const std::vector<FTopicBundle> &InBundles,
                      std::vector<ValidateCheck> &OutChecks);
void EvalNoDevAbsolutePath(const std::vector<FTopicBundle> &InBundles,
                           std::vector<ValidateCheck> &OutChecks);
void EvalNoHardcodedEndpoint(const std::vector<FTopicBundle> &InBundles,
                             std::vector<ValidateCheck> &OutChecks);
void EvalValidationCommandPlatformConsistency(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks);
void EvalNoSmartQuotes(const std::vector<FTopicBundle> &InBundles,
                       std::vector<ValidateCheck> &OutChecks);
void EvalNoHtmlInProse(const std::vector<FTopicBundle> &InBundles,
                       std::vector<ValidateCheck> &OutChecks);
void EvalNoEmptyPlaceholderLiteral(const std::vector<FTopicBundle> &InBundles,
                                   std::vector<ValidateCheck> &OutChecks);
void EvalTopicFieldsNotIdentical(const std::vector<FTopicBundle> &InBundles,
                                 std::vector<ValidateCheck> &OutChecks);
void EvalNoDegenerateDependencyEntry(const std::vector<FTopicBundle> &InBundles,
                                     std::vector<ValidateCheck> &OutChecks);

// v0.89.0 typed-array evaluators.
// scope_and_non_scope_populated closes the watch PLAN DETAIL blind spot
// surfaced during the VoGame audit (Part A.1 of the v0.89.0 plan). The
// remaining 10 enforce well-formedness and completion-honesty on the
// three new typed arrays.
void EvalScopeAndNonScopePopulated(const std::vector<FTopicBundle> &InBundles,
                                   std::vector<ValidateCheck> &OutChecks);
void EvalRiskEntryWellformed(const std::vector<FTopicBundle> &InBundles,
                             std::vector<ValidateCheck> &OutChecks);
void EvalRiskSeverityPopulatedForHighImpact(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks);
void EvalRiskIdUnique(const std::vector<FTopicBundle> &InBundles,
                      std::vector<ValidateCheck> &OutChecks);
void EvalNextActionWellformed(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks);
void EvalNextActionOrderUnique(const std::vector<FTopicBundle> &InBundles,
                               std::vector<ValidateCheck> &OutChecks);
void EvalNextActionHasEntries(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks);
void EvalAcceptanceCriterionWellformed(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks);
void EvalAcceptanceCriterionIdUnique(const std::vector<FTopicBundle> &InBundles,
                                     std::vector<ValidateCheck> &OutChecks);
void EvalAcceptanceCriteriaHasEntries(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks);
void EvalCompletedTopicCriteriaAllMet(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks);
void EvalNoUnresolvedMarker(const std::vector<FTopicBundle> &InBundles,
                            std::vector<ValidateCheck> &OutChecks);
void EvalTopicRefIntegrity(const std::vector<FTopicBundle> &InBundles,
                           std::vector<ValidateCheck> &OutChecks);
void EvalNoDuplicateChangelog(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks);
void EvalNoDuplicatePhaseField(const std::vector<FTopicBundle> &InBundles,
                               std::vector<ValidateCheck> &OutChecks);
void EvalNoHollowCompletedPhase(const std::vector<FTopicBundle> &InBundles,
                                std::vector<ValidateCheck> &OutChecks);
void EvalNoDuplicateLaneScope(const std::vector<FTopicBundle> &InBundles,
                              std::vector<ValidateCheck> &OutChecks);
void EvalFileManifestRequiredForCodePhases(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks);
// v0.87.0: detect mislabeled `modify` manifest entries by comparing the
// file's first-commit timestamp against the phase's started_at. If the
// file was born AFTER the phase started, the action should have been
// `create`, not `modify`. Spawns one `git log --reverse --name-only
// --pretty=format:%aI` invocation per validate run; silently no-ops
// when git is unavailable or repo root is empty so validate remains
// usable in non-git checkouts and in the watch TUI snapshot path.
// Severity: Warning (permanent — fuzzy signal that can false-fire on
// history rewrites / cherry-picks).
void EvalStaleMislabeledModify(const std::vector<FTopicBundle> &InBundles,
                               std::vector<ValidateCheck> &OutChecks,
                               const fs::path &InRepoRoot);

// From UniPlanParsing.cpp
void AppendSidecarIntegrityWarnings(
    const std::vector<DocumentRecord> &InPlans,
    const std::vector<DocumentRecord> &InPlaybooks,
    const std::vector<DocumentRecord> &InImplementations,
    const std::vector<SidecarRecord> &InSidecars,
    std::vector<std::string> &OutWarnings);

// From UniPlanRuntime.cpp
CacheConfigResult WriteCacheConfig(const std::string &InRepoRoot,
                                   const CacheConfigOptions &InOptions,
                                   const DocConfig &InCurrentConfig);

// From UniPlanCommandBundle.cpp
bool TryLoadBundleByTopic(const fs::path &InRepoRoot,
                          const std::string &InTopicKey,
                          FTopicBundle &OutBundle, std::string &OutError);
std::vector<FTopicBundle> LoadAllBundles(const fs::path &InRepoRoot,
                                         std::vector<std::string> &OutWarnings);
std::vector<BlockerItem> CollectBundleBlockers(const FTopicBundle &InBundle);
int RunTopicCommand(const std::vector<std::string> &InArgs,
                    const std::string &InRepoRoot);
int RunBundlePhaseCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot);
int RunBundleChangelogCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot);
int RunBundleVerificationCommand(const std::vector<std::string> &InArgs,
                                 const std::string &InRepoRoot);
int RunBundleTimelineCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot);
int RunBundleBlockersCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot);
int RunBundleValidateCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot);
int RunTopicAddCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot);
int RunTopicSetCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot);
int RunPhaseSetCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot);
int RunPhaseRemoveCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot);
int RunPhaseAddCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot);
int RunPhaseNormalizeCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot);
int RunJobSetCommand(const std::vector<std::string> &InArgs,
                     const std::string &InRepoRoot);
int RunTaskSetCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot);
int RunChangelogAddCommand(const std::vector<std::string> &InArgs,
                           const std::string &InRepoRoot);
int RunVerificationAddCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot);

// Semantic commands — Tier 1: Phase lifecycle
int RunPhaseStartCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot);
int RunPhaseCompleteCommand(const std::vector<std::string> &InArgs,
                            const std::string &InRepoRoot);
int RunPhaseBlockCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot);
int RunPhaseUnblockCommand(const std::vector<std::string> &InArgs,
                           const std::string &InRepoRoot);
int RunPhaseCancelCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot);
int RunPhaseProgressCommand(const std::vector<std::string> &InArgs,
                            const std::string &InRepoRoot);
int RunPhaseCompleteJobsCommand(const std::vector<std::string> &InArgs,
                                const std::string &InRepoRoot);

// Semantic commands — Tier 2: Topic lifecycle
int RunTopicStartCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot);
int RunTopicCompleteCommand(const std::vector<std::string> &InArgs,
                            const std::string &InRepoRoot);
int RunTopicBlockCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot);

// Semantic commands — Tier 3: Evidence shortcuts
int RunPhaseLogCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot);
int RunPhaseVerifyCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot);

// Semantic commands — Tier 4: Query helpers
int RunPhaseNextCommand(const std::vector<std::string> &InArgs,
                        const std::string &InRepoRoot);
int RunPhaseReadinessCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot);
int RunTopicStatusCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot);
int RunPhaseWaveStatusCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot);

// Phase-drift query (v0.84.0). Detects phases where declared lifecycle
// status lags behind evidence stored elsewhere in the bundle. Exposed as
// both a CLI command (below) and a validate evaluator
// (EvalPhaseStatusLaneAlignment) that share ComputePhaseDriftEntries().
std::vector<FPhaseDriftEntry>
ComputePhaseDriftEntries(const FPhaseRecord &InPhase);
int RunPhaseDriftCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot);
void EvalPhaseStatusLaneAlignment(const std::vector<FTopicBundle> &InBundles,
                                  std::vector<ValidateCheck> &OutChecks);

// Semantic commands — Tier 5: Entity coverage
int RunLaneSetCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot);
int RunLaneCompleteCommand(const std::vector<std::string> &InArgs,
                           const std::string &InRepoRoot);
int RunPhaseSyncExecutionCommand(const std::vector<std::string> &InArgs,
                                 const std::string &InRepoRoot);
int RunTestingAddCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot);
int RunManifestAddCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot);
int RunTestingSetCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot);
int RunVerificationSetCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot);
int RunManifestRemoveCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot);
int RunManifestListCommand(const std::vector<std::string> &InArgs,
                           const std::string &InRepoRoot);
int RunManifestSuggestCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot);
int RunManifestSetCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot);
int RunLaneAddCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot);
int RunChangelogSetCommand(const std::vector<std::string> &InArgs,
                           const std::string &InRepoRoot);

// v0.93.0 CRUD symmetry — command runners filling the add/remove/list
// gaps for job/task/lane/testing and topic normalize. Mutation runners
// live in UniPlanCommandEntityCrud.cpp + UniPlanCommandTopic.cpp.
int RunJobAddCommand(const std::vector<std::string> &InArgs,
                     const std::string &InRepoRoot);
int RunJobRemoveCommand(const std::vector<std::string> &InArgs,
                        const std::string &InRepoRoot);
int RunJobListCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot);
int RunTaskAddCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot);
int RunTaskRemoveCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot);
int RunTaskListCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot);
int RunLaneRemoveCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot);
int RunLaneListCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot);
int RunTestingRemoveCommand(const std::vector<std::string> &InArgs,
                            const std::string &InRepoRoot);
int RunTestingListCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot);
int RunTopicNormalizeCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot);

// v0.93.0 — _catalog dumps the full CLI verb/sub/flag surface as JSON so
// external tooling (skill-recipe linter, docs generators) can detect
// drift without parsing human-oriented help prose. Lives in
// UniPlanCommandCatalog.cpp.
int RunCatalogCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot);

// Semantic commands — Tier 6: Plan-entry CLI groups (v0.89.0+)
//
// `risk`, `next-action`, `acceptance-criterion` groups each expose
// add/set/remove/list. Mutation runners live in
// UniPlanCommandPlanEntries.cpp; parsers live in UniPlanOptionParsing.cpp.
int RunRiskAddCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot);
int RunRiskSetCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot);
int RunRiskRemoveCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot);
int RunRiskListCommand(const std::vector<std::string> &InArgs,
                       const std::string &InRepoRoot);
int RunNextActionAddCommand(const std::vector<std::string> &InArgs,
                            const std::string &InRepoRoot);
int RunNextActionSetCommand(const std::vector<std::string> &InArgs,
                            const std::string &InRepoRoot);
int RunNextActionRemoveCommand(const std::vector<std::string> &InArgs,
                               const std::string &InRepoRoot);
int RunNextActionListCommand(const std::vector<std::string> &InArgs,
                             const std::string &InRepoRoot);
int RunAcceptanceCriterionAddCommand(const std::vector<std::string> &InArgs,
                                     const std::string &InRepoRoot);
int RunAcceptanceCriterionSetCommand(const std::vector<std::string> &InArgs,
                                     const std::string &InRepoRoot);
int RunAcceptanceCriterionRemoveCommand(const std::vector<std::string> &InArgs,
                                        const std::string &InRepoRoot);
int RunAcceptanceCriterionListCommand(const std::vector<std::string> &InArgs,
                                      const std::string &InRepoRoot);

// v0.89.0: eager bundle normalization after typed-array schema change.
int RunMigrateCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot);

FRiskAddOptions ParseRiskAddOptions(const std::vector<std::string> &InTokens);
FRiskSetOptions ParseRiskSetOptions(const std::vector<std::string> &InTokens);
FRiskRemoveOptions
ParseRiskRemoveOptions(const std::vector<std::string> &InTokens);
FRiskListOptions ParseRiskListOptions(const std::vector<std::string> &InTokens);
FNextActionAddOptions
ParseNextActionAddOptions(const std::vector<std::string> &InTokens);
FNextActionSetOptions
ParseNextActionSetOptions(const std::vector<std::string> &InTokens);
FNextActionRemoveOptions
ParseNextActionRemoveOptions(const std::vector<std::string> &InTokens);
FNextActionListOptions
ParseNextActionListOptions(const std::vector<std::string> &InTokens);
FAcceptanceCriterionAddOptions
ParseAcceptanceCriterionAddOptions(const std::vector<std::string> &InTokens);
FAcceptanceCriterionSetOptions
ParseAcceptanceCriterionSetOptions(const std::vector<std::string> &InTokens);
FAcceptanceCriterionRemoveOptions
ParseAcceptanceCriterionRemoveOptions(const std::vector<std::string> &InTokens);
FAcceptanceCriterionListOptions
ParseAcceptanceCriterionListOptions(const std::vector<std::string> &InTokens);
int RunChangelogRemoveCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot);

// Semantic commands — Tier 6: Legacy-gap (stateless V3 <-> V4 parity audit,
// 0.75.0+). Discovers .md artifacts on disk at invoke time via filename
// convention; reports per-phase parity without reading anything from the
// bundle beyond V4 design-material chars. Returns 0 on success.
int RunLegacyGapCommand(const std::vector<std::string> &InArgs,
                        const std::string &InRepoRoot);

// ---------------------------------------------------------------------------
// v0.98.0 — priority-grouping / runbook / residual-risk CLI groups.
// Same four-leaf shape as the v0.89.0 risk group. Mutation runners live in
// UniPlanCommandPlanEntries.cpp; parsers in UniPlanOptionParsing.cpp.
// ---------------------------------------------------------------------------

int RunPriorityGroupingAddCommand(const std::vector<std::string> &InArgs,
                                  const std::string &InRepoRoot);
int RunPriorityGroupingSetCommand(const std::vector<std::string> &InArgs,
                                  const std::string &InRepoRoot);
int RunPriorityGroupingRemoveCommand(const std::vector<std::string> &InArgs,
                                     const std::string &InRepoRoot);
int RunPriorityGroupingListCommand(const std::vector<std::string> &InArgs,
                                   const std::string &InRepoRoot);
int RunRunbookAddCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot);
int RunRunbookSetCommand(const std::vector<std::string> &InArgs,
                         const std::string &InRepoRoot);
int RunRunbookRemoveCommand(const std::vector<std::string> &InArgs,
                            const std::string &InRepoRoot);
int RunRunbookListCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot);
int RunResidualRiskAddCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot);
int RunResidualRiskSetCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot);
int RunResidualRiskRemoveCommand(const std::vector<std::string> &InArgs,
                                 const std::string &InRepoRoot);
int RunResidualRiskListCommand(const std::vector<std::string> &InArgs,
                               const std::string &InRepoRoot);

FPriorityGroupingAddOptions
ParsePriorityGroupingAddOptions(const std::vector<std::string> &InTokens);
FPriorityGroupingSetOptions
ParsePriorityGroupingSetOptions(const std::vector<std::string> &InTokens);
FPriorityGroupingRemoveOptions
ParsePriorityGroupingRemoveOptions(const std::vector<std::string> &InTokens);
FPriorityGroupingListOptions
ParsePriorityGroupingListOptions(const std::vector<std::string> &InTokens);
FRunbookAddOptions
ParseRunbookAddOptions(const std::vector<std::string> &InTokens);
FRunbookSetOptions
ParseRunbookSetOptions(const std::vector<std::string> &InTokens);
FRunbookRemoveOptions
ParseRunbookRemoveOptions(const std::vector<std::string> &InTokens);
FRunbookListOptions
ParseRunbookListOptions(const std::vector<std::string> &InTokens);
FResidualRiskAddOptions
ParseResidualRiskAddOptions(const std::vector<std::string> &InTokens);
FResidualRiskSetOptions
ParseResidualRiskSetOptions(const std::vector<std::string> &InTokens);
FResidualRiskRemoveOptions
ParseResidualRiskRemoveOptions(const std::vector<std::string> &InTokens);
FResidualRiskListOptions
ParseResidualRiskListOptions(const std::vector<std::string> &InTokens);

// v0.98.0 — graph read command. Walks typed dependency graph across all
// bundles; emits uni-plan-graph-v1 JSON. Lives in UniPlanCommandGraph.cpp.
int RunGraphCommand(const std::vector<std::string> &InArgs,
                    const std::string &InRepoRoot);
FGraphOptions ParseGraphOptions(const std::vector<std::string> &InTokens);

// v0.98.0 evaluators — structural checks for the three new typed arrays.
void EvalPriorityGroupingWellformed(const std::vector<FTopicBundle> &InBundles,
                                    std::vector<ValidateCheck> &OutChecks);
void EvalPriorityGroupingPhaseIndexValid(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks);
void EvalPriorityGroupingIdUnique(const std::vector<FTopicBundle> &InBundles,
                                  std::vector<ValidateCheck> &OutChecks);
void EvalRunbookWellformed(const std::vector<FTopicBundle> &InBundles,
                           std::vector<ValidateCheck> &OutChecks);
void EvalRunbookNameUnique(const std::vector<FTopicBundle> &InBundles,
                           std::vector<ValidateCheck> &OutChecks);
void EvalResidualRiskWellformed(const std::vector<FTopicBundle> &InBundles,
                                std::vector<ValidateCheck> &OutChecks);
void EvalResidualRiskClosureShaFormat(
    const std::vector<FTopicBundle> &InBundles,
    std::vector<ValidateCheck> &OutChecks);

} // namespace UniPlan
