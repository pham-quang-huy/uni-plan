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

// From DocAnalysis.cpp
std::string EnsureGraphNode(const std::string &InRelativePath,
                            std::map<std::string, GraphNode> &InOutNodesById);
std::set<std::string>
CollectPlanPathReferences(const fs::path &InRepoRoot,
                          const std::string &InRelativePath,
                          std::vector<std::string> &OutWarnings);
std::map<std::string, fs::path>
BuildMarkdownPathMap(const std::vector<MarkdownDocument> &InDocuments);
std::vector<std::string> CollectTopicSeedPaths(const Inventory &InInventory,
                                               const std::string &InTopicKey);
std::string
GetFirstFieldValue(const std::map<std::string, std::string> &InFields,
                   const std::initializer_list<const char *> &InKeys);
std::vector<BlockerItem> CollectBlockerItemsFromDocument(
    const fs::path &InRepoRoot, const DocumentRecord &InDocument,
    const std::string &InDocClass, std::vector<std::string> &OutWarnings);
bool MatchesPhaseStatusFilter(const std::string &InStatusFilter,
                              const std::string &InItemStatus);
std::vector<PhaseItem> CollectPhaseItemsFromPlan(
    const fs::path &InRepoRoot, const DocumentRecord &InPlan,
    const std::vector<DocumentRecord> &InPlaybooks,
    const std::string &InStatusFilter, std::vector<std::string> &OutWarnings);
std::vector<PhaseListAllEntry>
BuildPhaseListAll(const fs::path &InRepoRoot, const Inventory &InInventory,
                  const std::string &InStatusFilter,
                  std::vector<std::string> &OutWarnings);
bool MatchesBlockerStatusFilter(const std::string &InStatusFilter,
                                const std::string &InItemStatus);
int BlockerStatusRank(const std::string &InStatus);

// From DocCache.cpp
fs::path ResolveExecutableDirectory();
std::string ExpandEnvVars(const std::string &InValue);
IniData ParseIniFile(const fs::path &InPath);
DocConfig LoadConfig(const fs::path &InExeDir);
void Fnv1aUpdateByte(uint64_t &InOutState, const uint8_t InValue);
void Fnv1aUpdateString(uint64_t &InOutState, const std::string &InValue);
void Fnv1aUpdateUint64(uint64_t &InOutState, uint64_t InValue);
std::string ToHexString(const uint64_t InValue);
std::string EscapeCacheField(const std::string &InValue);
std::string UnescapeCacheField(const std::string &InValue);
std::vector<std::string> SplitCacheFields(const std::string &InLine);
fs::path ResolveCodexCacheRoot(const std::string &InConfigCacheDir);
fs::path BuildInventoryCachePath(const fs::path &InRepoRoot,
                                 const std::string &InConfigCacheDir);
bool TryComputeMarkdownCorpusSignature(const fs::path &InRepoRoot,
                                       uint64_t &OutSignature,
                                       std::string &OutError);
bool TryWriteInventoryCache(const fs::path &InCachePath,
                            const Inventory &InInventory,
                            const uint64_t InSignature, std::string &OutError);
bool TryLoadInventoryCache(const fs::path &InCachePath,
                           const std::string &InRepoRoot,
                           const uint64_t InSignature, Inventory &OutInventory,
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
ListOptions ParseListOptions(const std::vector<std::string> &InTokens);
int ParsePositiveInteger(const std::string &InValue,
                         const std::string &InOptionName);
int ParseNonNegativeInteger(const std::string &InValue,
                            const std::string &InOptionName);
ValidateOptions ParseValidateOptions(const std::vector<std::string> &InTokens);
TimelineOptions ParseTimelineOptions(const std::vector<std::string> &InTokens);
BlockersOptions ParseBlockersOptions(const std::vector<std::string> &InTokens);
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
FJobSetOptions ParseJobSetOptions(const std::vector<std::string> &InTokens);
FTaskSetOptions ParseTaskSetOptions(const std::vector<std::string> &InTokens);
FChangelogAddOptions
ParseChangelogAddOptions(const std::vector<std::string> &InTokens);
FVerificationAddOptions
ParseVerificationAddOptions(const std::vector<std::string> &InTokens);

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
bool PathContainsSegment(const std::string &InRelativePath,
                         const std::string &InSegment);
bool IsPlanPath(const std::string &InRelativePath);
bool IsPlaybookPath(const std::string &InRelativePath);
bool IsImplementationPath(const std::string &InRelativePath);
bool TryClassifyCoreDocument(const std::string &InRelativePath,
                             const std::string &InFilename,
                             DocumentRecord &OutRecord);
bool TryClassifySidecarDocument(const std::string &InRelativePath,
                                const std::string &InFilename,
                                SidecarRecord &OutRecord);
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
Inventory BuildInventoryFresh(const std::string &InRepoRoot);
Inventory BuildInventory(const std::string &InRepoRoot, const bool InUseCache,
                         const std::string &InConfigCacheDir,
                         const bool InVerbose, const bool InQuiet = false);
bool IsExcludedDocsScriptPath(const std::string &InRelativePath);
std::vector<MarkdownDocument>
EnumerateMarkdownDocuments(const fs::path &InRepoRoot,
                           std::vector<std::string> &OutWarnings);
bool IsAllowedLintFilename(const std::string &InName);
bool HasFirstNonEmptyLineH1(const fs::path &InPath, std::string &OutError);
int CountFileLines(const fs::path &InPath, std::string &OutError);
bool TryReadFileText(const fs::path &InPath, std::string &OutText,
                     std::string &OutError);
std::string EscapeShellDoubleQuoted(const std::string &InValue);
bool RunCommandCapture(const std::string &InCommand, std::string &OutStdout,
                       int &OutExitCode);
std::string GetLastCommitDate(const fs::path &InRepoRoot,
                              const std::string &InRelativePath);
bool IsPathWithinRoot(const fs::path &InPath, const fs::path &InRoot);
bool IsMarkdownTableLine(const std::string &InTrimmedLine);
std::string NormalizeSectionId(const std::string &InHeadingText);
bool ParseFenceDelimiterLine(const std::string &InLine, char &OutFenceChar,
                             size_t &OutFenceLength, std::string &OutRemainder);
std::vector<HeadingRecord>
ParseHeadingRecords(const std::vector<std::string> &InLines);
std::vector<MarkdownTableRecord>
ParseMarkdownTables(const std::vector<std::string> &InLines,
                    const std::vector<HeadingRecord> &InHeadings);
SectionResolution
ResolveSectionByQuery(const std::vector<std::string> &InLines,
                      const std::vector<HeadingRecord> &InHeadings,
                      const std::string &InSectionQuery);
fs::path ResolveDocumentAbsolutePath(const fs::path &InRepoRoot,
                                     const std::string &InDocPath);
std::string ToRelativePathOrAbsolute(const fs::path &InAbsolutePath,
                                     const fs::path &InRepoRoot);
std::string NormalizeLookupKey(const std::string &InValue);
std::string ExtractTopicTokenFromInput(const std::string &InTopicValue);
std::string ResolveTopicKeyFromInventory(const Inventory &InInventory,
                                         const std::string &InTopicValue);
const DocumentRecord *
FindSingleRecordByTopic(const std::vector<DocumentRecord> &InRecords,
                        const std::string &InTopicKey);
std::vector<DocumentRecord>
CollectRecordsByTopic(const std::vector<DocumentRecord> &InRecords,
                      const std::string &InTopicKey);
std::vector<SidecarRecord>
CollectSidecarsByTopic(const std::vector<SidecarRecord> &InSidecars,
                       const std::string &InTopicKey);
std::string ResolvePairStateForTopic(const Inventory &InInventory,
                                     const std::string &InTopicKey);
std::vector<SidecarRecord>
FilterSidecars(const std::vector<SidecarRecord> &InSidecars,
               const std::string &InOwnerKind, const std::string &InDocKind,
               const std::string &InPhaseKey);
std::vector<SidecarRecord>
ResolveEvidenceSidecars(const std::vector<SidecarRecord> &InTopicSidecars,
                        const std::string &InDocClass,
                        const std::string &InPhaseKey,
                        const std::string &InEvidenceKind);
std::vector<EvidenceEntry> ParseEvidenceEntriesFromFile(
    const fs::path &InAbsolutePath, const std::string &InRelativePath,
    const std::string &InPhaseKey, std::vector<std::string> &OutWarnings);
bool LooksLikeIsoDate(const std::string &InValue);
std::string NormalizeHeaderKey(const std::string &InValue);
int FindHeaderIndex(const std::vector<std::string> &InHeaders,
                    const std::string &InHeaderKey);
int FindFirstHeaderIndex(const std::vector<std::string> &InHeaders,
                         const std::initializer_list<const char *> &InKeys);
std::map<std::string, std::string>
BuildFieldMap(const std::vector<std::pair<std::string, std::string>> &InFields);
std::string ClassifyRelativeMarkdownPath(const std::string &InRelativePath,
                                         std::string *const InOutTopicKey,
                                         std::string *const InOutPhaseKey,
                                         std::string *const InOutOwnerKind,
                                         std::string *const InOutDocKind);
std::string BuildGraphNodeId(const std::string &InType,
                             const std::string &InPath);
std::set<std::string>
ExtractReferencedMarkdownPaths(const fs::path &InRepoRoot,
                               const fs::path &InSourceAbsolutePath,
                               const std::string &InText);
std::string ToDocClassFromOwnerKind(const std::string &InOwnerKind);
std::string
DerivePlaybookPairState(const DocumentRecord &InPlaybook,
                        const std::set<std::string> &InPlanTopics,
                        const std::set<std::string> &InImplementationTopics);
ResolvedDocument ReadAndParseDocument(const BaseOptions &InOptions,
                                      const std::string &InDocPath);

// From DocValidation.cpp
LintResult BuildLintResult(const std::string &InRepoRoot,
                           const bool InQuiet = false);
PlanSchemaValidationResult
EvaluatePlanSchemaConformance(const fs::path &InRepoRoot,
                              const std::vector<DocumentRecord> &InPlans,
                              std::vector<std::string> &OutWarnings);
BlankSectionsResult
EvaluateBlankSections(const fs::path &InRepoRoot,
                      const std::vector<DocumentRecord> &InPlans,
                      std::vector<std::string> &OutWarnings);
CrossStatusResult
EvaluateCrossStatus(const fs::path &InRepoRoot,
                    const std::vector<TopicPairRecord> &InPairs,
                    std::vector<std::string> &OutWarnings);
TaxonomyJobCompletenessResult
EvaluateTaxonomyJobCompleteness(const fs::path &InRepoRoot,
                                const std::vector<DocumentRecord> &InPlaybooks,
                                std::vector<std::string> &OutWarnings);
TaxonomyTaskTraceabilityResult
EvaluateTaxonomyTaskTraceability(const fs::path &InRepoRoot,
                                 const std::vector<DocumentRecord> &InPlaybooks,
                                 std::vector<std::string> &OutWarnings);
ValidationHeadingOwnershipResult EvaluateValidationHeadingOwnership(
    const fs::path &InRepoRoot, const std::vector<DocumentRecord> &InPlans,
    const std::vector<DocumentRecord> &InImplementations,
    std::vector<std::string> &OutWarnings);
TestingActorCoverageResult
EvaluateTestingActorCoverage(const fs::path &InRepoRoot,
                             const std::vector<DocumentRecord> &InPlaybooks,
                             std::vector<std::string> &OutWarnings);
ImplSchemaValidationResult
EvaluateImplSchemaConformance(const fs::path &InRepoRoot,
                              const std::vector<DocumentRecord> &InImpls,
                              std::vector<std::string> &OutWarnings);
PlaybookOrderResult
EvaluatePlaybookCanonicalOrder(const fs::path &InRepoRoot,
                               const std::vector<DocumentRecord> &InPlaybooks,
                               std::vector<std::string> &OutWarnings);
PlaybookHeadingNamingResult
EvaluatePlaybookHeadingNaming(const fs::path &InRepoRoot,
                              const std::vector<DocumentRecord> &InPlaybooks,
                              std::vector<std::string> &OutWarnings);
PlaybookBlankSectionsResult
EvaluatePlaybookBlankSections(const fs::path &InRepoRoot,
                              const std::vector<DocumentRecord> &InPlaybooks,
                              std::vector<std::string> &OutWarnings);
std::vector<ValidateCheck>
ValidateAllBundles(const std::vector<FTopicBundle> &InBundles);

// Additional cross-file declarations
// Defined in DocAnalysis.cpp
void BuildReferenceGraph(
    const fs::path &InRepoRoot,
    const std::map<std::string, fs::path> &InPathMap,
    std::vector<std::string> &InOutWarnings,
    std::map<std::string, std::set<std::string>> &OutOutgoing,
    std::map<std::string, std::set<std::string>> &OutIncoming);

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
bool IsSupportedSchemaType(const std::string &InType);
std::vector<RuleEntry> BuildRules(const fs::path &InRepoRoot,
                                  std::vector<std::string> &OutWarnings);
bool HasIndexedHeadingPrefix(const std::string &InHeadingText);
std::string ExtractPhaseKeyFromCell(const std::string &InCellValue);
PhaseEntryGateResult
EvaluatePhaseEntryGate(const fs::path &InRepoRoot,
                       const std::vector<DocumentRecord> &InPlans,
                       const std::vector<DocumentRecord> &InPlaybooks,
                       std::vector<std::string> &OutWarnings);
ArtifactRoleBoundaryResult EvaluateArtifactRoleBoundaries(
    const fs::path &InRepoRoot, const std::vector<DocumentRecord> &InPlaybooks,
    const std::vector<DocumentRecord> &InImplementations,
    std::vector<std::string> &OutWarnings);
PlaybookSchemaResult
EvaluatePlaybookSchema(const fs::path &InRepoRoot,
                       const std::vector<DocumentRecord> &InPlaybooks,
                       std::vector<std::string> &OutWarnings);
std::vector<SectionSchemaEntry>
BuildSectionSchemaEntries(const std::string &InType,
                          const fs::path &InRepoRoot = fs::path());
fs::path ResolveSchemaFilePath(const std::string &InType,
                               const fs::path &InRepoRoot);

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
int RunJobSetCommand(const std::vector<std::string> &InArgs,
                     const std::string &InRepoRoot);
int RunTaskSetCommand(const std::vector<std::string> &InArgs,
                      const std::string &InRepoRoot);
int RunChangelogAddCommand(const std::vector<std::string> &InArgs,
                           const std::string &InRepoRoot);
int RunVerificationAddCommand(const std::vector<std::string> &InArgs,
                              const std::string &InRepoRoot);

// From UniPlanParsing.cpp
std::vector<SectionSchemaEntry>
TryParseSectionSchemaFromFile(const fs::path &InSchemaPath);

} // namespace UniPlan
