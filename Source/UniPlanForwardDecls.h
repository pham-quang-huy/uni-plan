#pragma once

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
PhaseOptions ParsePhaseOptions(const std::vector<std::string> &InTokens);
LintOptions ParseLintOptions(const std::vector<std::string> &InTokens);
InventoryOptions
ParseInventoryOptions(const std::vector<std::string> &InTokens);
OrphanCheckOptions
ParseOrphanCheckOptions(const std::vector<std::string> &InTokens);
int ParsePositiveInteger(const std::string &InValue,
                         const std::string &InOptionName);
int ParseNonNegativeInteger(const std::string &InValue,
                            const std::string &InOptionName);
ArtifactsOptions
ParseArtifactsOptions(const std::vector<std::string> &InTokens);
EvidenceOptions ParseEvidenceOptions(const std::vector<std::string> &InTokens,
                                     const std::string &InCommandName);
SchemaOptions ParseSchemaOptions(const std::vector<std::string> &InTokens);
RulesOptions ParseRulesOptions(const std::vector<std::string> &InTokens);
ValidateOptions ParseValidateOptions(const std::vector<std::string> &InTokens);
SectionResolveOptions
ParseSectionResolveOptions(const std::vector<std::string> &InTokens);
SectionSchemaOptions
ParseSectionSchemaOptions(const std::vector<std::string> &InTokens);
SectionListOptions
ParseSectionListOptions(const std::vector<std::string> &InTokens);
SectionContentOptions
ParseSectionContentOptions(const std::vector<std::string> &InTokens);
ExcerptOptions ParseExcerptOptions(const std::vector<std::string> &InTokens);
TableListOptions
ParseTableListOptions(const std::vector<std::string> &InTokens);
TableGetOptions ParseTableGetOptions(const std::vector<std::string> &InTokens);
GraphOptions ParseGraphOptions(const std::vector<std::string> &InTokens);
DiagnoseDriftOptions
ParseDiagnoseDriftOptions(const std::vector<std::string> &InTokens);
TimelineOptions ParseTimelineOptions(const std::vector<std::string> &InTokens);
BlockersOptions ParseBlockersOptions(const std::vector<std::string> &InTokens);
CacheInfoOptions
ParseCacheInfoOptions(const std::vector<std::string> &InTokens);
CacheClearOptions
ParseCacheClearOptions(const std::vector<std::string> &InTokens);
CacheConfigOptions
ParseCacheConfigOptions(const std::vector<std::string> &InTokens);

// From DocOutputHuman.cpp
size_t VisibleWidth(const std::string &InText);
void PrintRow(const std::vector<std::string> &InCells,
              const std::vector<size_t> &InWidths, const bool InBold);
void PrintSeparator(const std::vector<size_t> &InWidths);
int RunListHuman(const Inventory &InInventory, const std::string &InKind,
                 const std::string &InStatusFilter);
int RunLintHuman(const LintResult &InResult);
int RunInventoryHuman(const InventoryResult &InResult);
int RunOrphanCheckHuman(const OrphanCheckResult &InResult);
int RunArtifactsHuman(const Inventory &InInventory,
                      const std::string &InTopicKey, const std::string &InKind);
int RunEvidenceHuman(const std::string &InLabel, const std::string &InTopicKey,
                     const std::string &InDocClass,
                     const std::vector<EvidenceEntry> &InEntries,
                     const std::vector<std::string> &InWarnings);
int RunSchemaHuman(const std::string &InType,
                   const std::vector<SchemaField> &InFields,
                   const std::vector<std::string> &InExamples,
                   const std::vector<std::string> &InWarnings);
int RunRulesHuman(const std::vector<RuleEntry> &InRules,
                  const std::vector<std::string> &InWarnings);
int RunValidateHuman(const bool InStrict, const bool InOk,
                     const std::vector<ValidateCheck> &InChecks,
                     const std::vector<std::string> &InErrors,
                     const std::vector<std::string> &InWarnings);
int RunSectionResolveHuman(const std::string &InDocPath,
                           const SectionResolution &InResolution,
                           const std::vector<std::string> &InWarnings);
int RunSectionSchemaHuman(const std::string &InType,
                          const std::vector<SectionSchemaEntry> &InEntries);
int RunSectionListHuman(const std::vector<SectionCount> &InCounts,
                        const bool InShowCount);
int RunSectionListDocHuman(const std::string &InDocPath,
                           const std::vector<HeadingRecord> &InHeadings);
int RunExcerptHuman(const std::string &InDocPath,
                    const SectionResolution &InResolution,
                    const int InExcerptStartLine,
                    const std::vector<std::string> &InExcerptLines,
                    const std::vector<std::string> &InWarnings);
int RunTableListHuman(const std::string &InDocPath,
                      const std::vector<MarkdownTableRecord> &InTables,
                      const std::vector<std::string> &InWarnings);
int RunTableGetHuman(const std::string &InDocPath,
                     const MarkdownTableRecord &InTable,
                     const std::vector<std::string> &InWarnings);
int RunGraphHuman(const std::string &InTopicKey, const int InDepth,
                  const std::vector<GraphNode> &InNodes,
                  const std::vector<GraphEdge> &InEdges,
                  const std::vector<std::string> &InWarnings);
int RunDiagnoseDriftHuman(const bool InOk,
                          const std::vector<DriftItem> &InDrifts,
                          const std::vector<std::string> &InWarnings);
int RunTimelineHuman(const std::string &InTopicKey, const std::string &InSince,
                     const std::vector<TimelineItem> &InItems,
                     const std::vector<std::string> &InWarnings);
int RunPhaseHuman(const std::string &InTopicKey, const std::string &InPlanPath,
                  const std::string &InStatusFilter,
                  const std::vector<PhaseItem> &InItems,
                  const std::vector<std::string> &InWarnings);
int RunPhaseListAllHuman(const std::string &InStatusFilter,
                         const std::vector<PhaseListAllEntry> &InEntries,
                         const std::vector<std::string> &InWarnings);
int RunBlockersHuman(const std::string &InStatusFilter,
                     const std::vector<BlockerItem> &InItems,
                     const std::vector<std::string> &InWarnings);
int RunCacheInfoHuman(const CacheInfoResult &InResult);
int RunCacheClearHuman(const CacheClearResult &InResult);
int RunCacheConfigHuman(const CacheConfigResult &InResult);

// From DocOutputJson.cpp
int RunListJson(const Inventory &InInventory, const std::string &InKind,
                const std::string &InStatusFilter);
int RunLintJson(const LintResult &InResult);
int RunInventoryJson(const InventoryResult &InResult);
int RunOrphanCheckJson(const OrphanCheckResult &InResult);
int RunArtifactsJson(const Inventory &InInventory,
                     const std::string &InTopicKey, const std::string &InKind);
int RunSchemaJson(const std::string &InGeneratedUtc,
                  const std::string &InRepoRoot, const std::string &InType,
                  const std::vector<SchemaField> &InFields,
                  const std::vector<std::string> &InExamples,
                  const std::vector<std::string> &InWarnings);
int RunRulesJson(const std::string &InGeneratedUtc,
                 const std::string &InRepoRoot,
                 const std::vector<RuleEntry> &InRules,
                 const std::vector<std::string> &InWarnings);
int RunSectionResolveJson(const std::string &InGeneratedUtc,
                          const std::string &InRepoRoot,
                          const std::string &InDocPath,
                          const SectionResolution &InResolution,
                          const std::vector<std::string> &InWarnings);
int RunSectionSchemaJson(const std::string &InGeneratedUtc,
                         const std::string &InRepoRoot,
                         const std::string &InType,
                         const std::vector<SectionSchemaEntry> &InEntries);
int RunSectionListJson(const std::string &InGeneratedUtc,
                       const std::string &InRepoRoot,
                       const std::vector<SectionCount> &InCounts,
                       const bool InShowCount);
int RunSectionListDocJson(const std::string &InGeneratedUtc,
                          const std::string &InRepoRoot,
                          const std::string &InDocPath,
                          const std::vector<HeadingRecord> &InHeadings);
int RunTableListJson(const std::string &InGeneratedUtc,
                     const std::string &InRepoRoot,
                     const std::string &InDocPath,
                     const std::vector<MarkdownTableRecord> &InTables,
                     const std::vector<std::string> &InWarnings);
int RunTableGetJson(const std::string &InGeneratedUtc,
                    const std::string &InRepoRoot, const std::string &InDocPath,
                    const MarkdownTableRecord &InTable,
                    const std::vector<std::string> &InWarnings);
int RunDiagnoseDriftJson(const std::string &InGeneratedUtc,
                         const std::string &InRepoRoot, const bool InOk,
                         const std::vector<DriftItem> &InDrifts,
                         const std::vector<std::string> &InWarnings);
int RunTimelineJson(const std::string &InGeneratedUtc,
                    const std::string &InRepoRoot,
                    const std::string &InTopicKey, const std::string &InSince,
                    const std::vector<TimelineItem> &InItems,
                    const std::vector<std::string> &InWarnings);
int RunBlockersJson(const std::string &InGeneratedUtc,
                    const std::string &InRepoRoot,
                    const std::string &InStatusFilter,
                    const std::vector<BlockerItem> &InItems,
                    const std::vector<std::string> &InWarnings);
int RunCacheInfoJson(const std::string &InRepoRoot,
                     const CacheInfoResult &InResult);
int RunCacheClearJson(const std::string &InRepoRoot,
                      const CacheClearResult &InResult);
int RunCacheConfigJson(const std::string &InRepoRoot,
                       const CacheConfigResult &InResult);
int RunPhaseListAllJson(const std::string &InGeneratedUtc,
                        const std::string &InRepoRoot,
                        const std::string &InStatusFilter,
                        const std::vector<PhaseListAllEntry> &InEntries,
                        const std::vector<std::string> &InWarnings);

// From DocOutputText.cpp
int RunListText(const Inventory &InInventory, const std::string &InKind,
                const std::string &InStatusFilter);
int RunLintText(const LintResult &InResult);
int RunInventoryText(const InventoryResult &InResult);
int RunOrphanCheckText(const OrphanCheckResult &InResult);
int RunArtifactsText(const Inventory &InInventory,
                     const std::string &InTopicKey, const std::string &InKind);
int RunEvidenceText(const std::string &InLabel, const std::string &InTopicKey,
                    const std::string &InDocClass,
                    const std::vector<EvidenceEntry> &InEntries,
                    const std::vector<std::string> &InWarnings);
int RunSchemaText(const std::string &InType,
                  const std::vector<SchemaField> &InFields,
                  const std::vector<std::string> &InExamples,
                  const std::vector<std::string> &InWarnings);
int RunRulesText(const std::vector<RuleEntry> &InRules,
                 const std::vector<std::string> &InWarnings);
int RunValidateText(const bool InStrict, const bool InOk,
                    const std::vector<ValidateCheck> &InChecks,
                    const std::vector<std::string> &InErrors,
                    const std::vector<std::string> &InWarnings);
int RunSectionResolveText(const std::string &InDocPath,
                          const SectionResolution &InResolution,
                          const std::vector<std::string> &InWarnings);
int RunSectionSchemaText(const std::string &InType,
                         const std::vector<SectionSchemaEntry> &InEntries);
int RunSectionListText(const std::vector<SectionCount> &InCounts,
                       const bool InShowCount);
int RunSectionListDocText(const std::string &InDocPath,
                          const std::vector<HeadingRecord> &InHeadings);
int RunExcerptText(const std::string &InDocPath,
                   const SectionResolution &InResolution,
                   const int InExcerptStartLine,
                   const std::vector<std::string> &InExcerptLines,
                   const std::vector<std::string> &InWarnings);
int RunTableListText(const std::string &InDocPath,
                     const std::vector<MarkdownTableRecord> &InTables,
                     const std::vector<std::string> &InWarnings);
int RunTableGetText(const std::string &InDocPath,
                    const MarkdownTableRecord &InTable,
                    const std::vector<std::string> &InWarnings);
int RunGraphText(const std::string &InTopicKey, const int InDepth,
                 const std::vector<GraphNode> &InNodes,
                 const std::vector<GraphEdge> &InEdges,
                 const std::vector<std::string> &InWarnings);
int RunDiagnoseDriftText(const bool InOk,
                         const std::vector<DriftItem> &InDrifts,
                         const std::vector<std::string> &InWarnings);
int RunTimelineText(const std::string &InTopicKey, const std::string &InSince,
                    const std::vector<TimelineItem> &InItems,
                    const std::vector<std::string> &InWarnings);
int RunPhaseText(const std::string &InTopicKey, const std::string &InPlanPath,
                 const std::string &InStatusFilter,
                 const std::vector<PhaseItem> &InItems,
                 const std::vector<std::string> &InWarnings);
int RunBlockersText(const std::string &InStatusFilter,
                    const std::vector<BlockerItem> &InItems,
                    const std::vector<std::string> &InWarnings);
int RunCacheInfoText(const CacheInfoResult &InResult);
int RunCacheClearText(const CacheClearResult &InResult);
int RunCacheConfigText(const CacheConfigResult &InResult);
int RunPhaseListAllText(const std::string &InStatusFilter,
                        const std::vector<PhaseListAllEntry> &InEntries,
                        const std::vector<std::string> &InWarnings);
int RunSectionContentText(const std::string &InDocPath,
                          const SectionResolution &InResolution,
                          const int InLineCharLimit,
                          const int InContentStartLine,
                          const std::vector<std::string> &InContentLines,
                          const std::vector<std::string> &InWarnings);

// From DocParsing.cpp
StatusInference InferDocumentStatus(const EDocumentKind InKind,
                                    const fs::path &InAbsolutePath,
                                    std::vector<std::string> &OutWarnings);
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
InventoryResult BuildDocInventoryResult(const std::string &InRepoRoot);
OrphanCheckResult BuildOrphanCheckResult(const std::string &InRepoRoot);
bool IsSnakeCaseHeadingLiteral(const std::string &InHeadingText);
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
BuildValidateChecks(const Inventory &InInventory, const fs::path &InRepoRoot,
                    const bool InStrict, std::vector<std::string> &OutErrors,
                    std::vector<std::string> &OutWarnings, bool &OutOk);

// Additional cross-file declarations
// Defined in DocAnalysis.cpp
void BuildReferenceGraph(
    const fs::path &InRepoRoot,
    const std::map<std::string, fs::path> &InPathMap,
    std::vector<std::string> &InOutWarnings,
    std::map<std::string, std::set<std::string>> &OutOutgoing,
    std::map<std::string, std::set<std::string>> &OutIncoming);
// Defined in DocOutputHuman.cpp
int RunSectionContentHuman(const std::string &InDocPath,
                           const SectionResolution &InResolution,
                           const int InLineCharLimit,
                           const int InContentStartLine,
                           const std::vector<std::string> &InContentLines,
                           const std::vector<MarkdownTableRecord> &InTables,
                           const std::vector<std::string> &InWarnings);
// Defined in DocOutputJson.cpp
int RunEvidenceJson(const char *InSchemaId, const std::string &InGeneratedUtc,
                    const std::string &InRepoRoot,
                    const std::string &InTopicKey,
                    const std::string &InDocClass,
                    const std::vector<EvidenceEntry> &InEntries,
                    const std::vector<std::string> &InWarnings);
// Defined in DocOutputJson.cpp
int RunValidateJson(const std::string &InGeneratedUtc,
                    const std::string &InRepoRoot, const bool InStrict,
                    const bool InOk, const std::vector<ValidateCheck> &InChecks,
                    const std::vector<std::string> &InErrors,
                    const std::vector<std::string> &InWarnings);
// Defined in DocOutputJson.cpp
int RunExcerptJson(const std::string &InGeneratedUtc,
                   const std::string &InRepoRoot, const std::string &InDocPath,
                   const SectionResolution &InResolution,
                   const int InContextLines, const int InExcerptStartLine,
                   const std::vector<std::string> &InExcerptLines,
                   const std::vector<std::string> &InWarnings);
// Defined in DocOutputJson.cpp
int RunGraphJson(const std::string &InGeneratedUtc,
                 const std::string &InRepoRoot, const std::string &InTopicKey,
                 const int InDepth, const std::vector<GraphNode> &InNodes,
                 const std::vector<GraphEdge> &InEdges,
                 const std::vector<std::string> &InWarnings);
// Defined in DocOutputJson.cpp
int RunPhaseJson(const std::string &InGeneratedUtc,
                 const std::string &InRepoRoot, const std::string &InTopicKey,
                 const std::string &InPlanPath,
                 const std::string &InStatusFilter,
                 const std::vector<PhaseItem> &InItems,
                 const std::vector<std::string> &InWarnings);
// Defined in DocOutputJson.cpp
int RunSectionContentJson(const std::string &InGeneratedUtc,
                          const std::string &InRepoRoot,
                          const std::string &InDocPath,
                          const SectionResolution &InResolution,
                          const int InLineCharLimit,
                          const int InContentStartLine,
                          const std::vector<std::string> &InContentLines,
                          const std::vector<MarkdownTableRecord> &InTables,
                          const std::vector<std::string> &InWarnings);
// Defined in DocParsing.cpp
void AppendSidecarIntegrityWarnings(
    const std::vector<DocumentRecord> &InPlans,
    const std::vector<DocumentRecord> &InPlaybooks,
    const std::vector<DocumentRecord> &InImplementations,
    const std::vector<SidecarRecord> &InSidecars,
    std::vector<std::string> &OutWarnings);
// Defined in DocRuntime.cpp
const std::vector<DocumentRecord> &
ResolveRecordsByKind(const Inventory &InInventory, const std::string &InKind);
// Defined in DocRuntime.cpp
std::vector<const TopicPairRecord *>
FilterPairsByStatus(const std::vector<TopicPairRecord> &InPairs,
                    const std::string &InStatusFilter);
// Defined in DocRuntime.cpp
std::vector<const DocumentRecord *>
FilterRecordsByStatus(const std::vector<DocumentRecord> &InRecords,
                      const std::string &InStatusFilter);
// Defined in DocRuntime.cpp
bool IsSupportedSchemaType(const std::string &InType);
// Defined in DocRuntime.cpp
std::vector<RuleEntry> BuildRules(const fs::path &InRepoRoot,
                                  std::vector<std::string> &OutWarnings);
// Defined in DocRuntime.cpp
bool HasIndexedHeadingPrefix(const std::string &InHeadingText);
// Defined in DocRuntime.cpp
std::string ExtractPhaseKeyFromCell(const std::string &InCellValue);
// Defined in DocRuntime.cpp
PhaseEntryGateResult
EvaluatePhaseEntryGate(const fs::path &InRepoRoot,
                       const std::vector<DocumentRecord> &InPlans,
                       const std::vector<DocumentRecord> &InPlaybooks,
                       std::vector<std::string> &OutWarnings);
// Defined in DocRuntime.cpp
ArtifactRoleBoundaryResult EvaluateArtifactRoleBoundaries(
    const fs::path &InRepoRoot, const std::vector<DocumentRecord> &InPlaybooks,
    const std::vector<DocumentRecord> &InImplementations,
    std::vector<std::string> &OutWarnings);
// Defined in DocRuntime.cpp
PlaybookSchemaResult
EvaluatePlaybookSchema(const fs::path &InRepoRoot,
                       const std::vector<DocumentRecord> &InPlaybooks,
                       std::vector<std::string> &OutWarnings);

// From UniPlanRuntime.cpp
CacheConfigResult WriteCacheConfig(const std::string &InRepoRoot,
                                   const CacheConfigOptions &InOptions,
                                   const DocConfig &InCurrentConfig);
std::vector<SchemaField>
ParseSchemaFields(const fs::path &InSchemaPath,
                  std::vector<std::string> &OutWarnings);
std::vector<std::string> BuildSchemaExamples(const std::string &InType);
std::string JoinMarkdownRowCells(const std::vector<std::string> &InRow);
bool RowContainsAllTerms(const std::vector<std::string> &InRow,
                         const std::vector<std::string> &InTerms);
bool TryResolveRuleProvenance(const fs::path &InRepoRoot,
                              const RuleProvenanceProbe &InProbe,
                              std::string &OutResolvedEvidence);
std::string BuildTopicPhaseIdentityNormalized(const std::string &InTopicKey,
                                              const std::string &InPhaseKey);
std::vector<ActivePhaseRecord>
CollectActivePhaseRecords(const fs::path &InRepoRoot,
                          const Inventory &InInventory,
                          const std::vector<DocumentRecord> &InPlaybooks,
                          std::vector<std::string> &OutWarnings);
std::set<std::string>
BuildHeadingIdSet(const std::vector<HeadingRecord> &InHeadings);
bool IsPlaybookPhaseEntryReady(const fs::path &InPlaybookAbsolutePath,
                               const std::string &InPhaseKey);
std::vector<SectionSchemaEntry>
BuildSectionSchemaEntries(const std::string &InType,
                          const fs::path &InRepoRoot = fs::path());
fs::path ResolveSchemaFilePath(const std::string &InType,
                               const fs::path &InRepoRoot);
void AppendGraphEdgeUnique(std::vector<GraphEdge> &InOutEdges,
                           std::set<std::string> &InOutEdgeKeys,
                           const std::string &InFromNodeId,
                           const std::string &InToNodeId,
                           const std::string &InKind, int InDepth);
void AddDriftItem(std::vector<DriftItem> &InOutDrifts, const std::string &InId,
                  const std::string &InSeverity, const std::string &InTopicKey,
                  const std::string &InPath, const std::string &InMessage);
int SeverityRank(const std::string &InSeverity);
void PrintUsage();
void PrintCommandUsage(const std::string &InCommand);

// From UniPlanOptionParsing.cpp
MigrateOptions ParseMigrateOptions(const std::vector<std::string> &InTokens);

// From UniPlanCommandMigrate.cpp
int RunMigrateJson(const MigrateOptions &InOptions, const bool InUseCache,
                   const DocConfig &InConfig);

// From UniPlanParsing.cpp
std::vector<SectionSchemaEntry>
TryParseSectionSchemaFromFile(const fs::path &InSchemaPath);

} // namespace UniPlan
