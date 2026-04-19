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

// From DocCache.cpp
fs::path ResolveExecutableDirectory();
std::string ExpandEnvVars(const std::string &InValue);
IniData ParseIniFile(const fs::path &InPath);
DocConfig LoadConfig(const fs::path &InExeDir);
void Fnv1aUpdateByte(uint64_t &InOutState, const uint8_t InValue);
void Fnv1aUpdateString(uint64_t &InOutState, const std::string &InValue);
void Fnv1aUpdateUint64(uint64_t &InOutState, uint64_t InValue);
std::string ToHexString(const uint64_t InValue);
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
FLaneAddOptions ParseLaneAddOptions(const std::vector<std::string> &InTokens);
FChangelogSetOptions
ParseChangelogSetOptions(const std::vector<std::string> &InTokens);

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
ValidateAllBundles(const std::vector<FTopicBundle> &InBundles);

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

// Semantic commands — Tier 5: Entity coverage
int RunLaneSetCommand(const std::vector<std::string> &InArgs,
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
int RunManifestSetCommand(const std::vector<std::string> &InArgs,
                          const std::string &InRepoRoot);
int RunLaneAddCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot);
int RunChangelogSetCommand(const std::vector<std::string> &InArgs,
                           const std::string &InRepoRoot);

} // namespace UniPlan
