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

namespace UniPlan
{

// kMarkdownPathRegex moved to DocParsing.cpp (declared in DocForwardDecls.h)

CacheConfigResult WriteCacheConfig(const std::string &InRepoRoot,
                                   const CacheConfigOptions &InOptions,
                                   const DocConfig &InCurrentConfig)
{
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

    // Merge only explicitly-set fields (mbDirSet distinguishes "not passed"
    // from "set to empty")
    if (InOptions.mbDirSet)
    {
        EffectiveDir = InOptions.mDir;
    }
    if (!InOptions.mEnabled.empty())
    {
        EffectiveEnabled = InOptions.mEnabled;
    }
    if (!InOptions.mVerbose.empty())
    {
        EffectiveVerbose = InOptions.mVerbose;
    }

    std::string WriteError;
    if (!TryWriteDocIni(IniPath, EffectiveDir, EffectiveEnabled,
                        EffectiveVerbose, WriteError))
    {
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


const std::vector<DocumentRecord> &
ResolveRecordsByKind(const Inventory &InInventory, const std::string &InKind)
{
    if (InKind == "plan")
    {
        return InInventory.mPlans;
    }
    if (InKind == "playbook")
    {
        return InInventory.mPlaybooks;
    }
    if (InKind == "implementation")
    {
        return InInventory.mImplementations;
    }
    throw UsageError("Invalid value for --type: " + InKind);
}

std::vector<const TopicPairRecord *>
FilterPairsByStatus(const std::vector<TopicPairRecord> &InPairs,
                    const std::string &InStatusFilter)
{
    const bool bFilterActive = (InStatusFilter != "all");
    std::vector<const TopicPairRecord *> Result;
    for (const TopicPairRecord &Pair : InPairs)
    {
        if (bFilterActive &&
            !MatchesStatusFilter(InStatusFilter,
                                 GetDisplayStatus(Pair.mOverallStatus)))
        {
            continue;
        }
        Result.push_back(&Pair);
    }
    return Result;
}

std::vector<const DocumentRecord *>
FilterRecordsByStatus(const std::vector<DocumentRecord> &InRecords,
                      const std::string &InStatusFilter)
{
    const bool bFilterActive = (InStatusFilter != "all");
    std::vector<const DocumentRecord *> Result;
    for (const DocumentRecord &Record : InRecords)
    {
        if (bFilterActive &&
            !MatchesStatusFilter(InStatusFilter,
                                 GetDisplayStatus(Record.mStatus)))
        {
            continue;
        }
        Result.push_back(&Record);
    }
    return Result;
}

std::vector<SchemaField>
ParseSchemaFields(const fs::path &InSchemaPath,
                  std::vector<std::string> &OutWarnings)
{
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(InSchemaPath, Lines, ReadError))
    {
        throw std::runtime_error("Failed to read schema file '" +
                                 InSchemaPath.string() + "': " + ReadError);
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    const std::vector<MarkdownTableRecord> Tables =
        ParseMarkdownTables(Lines, Headings);
    std::vector<SchemaField> Fields;

    for (const MarkdownTableRecord &Table : Tables)
    {
        if (Table.mHeaders.size() < 2)
        {
            continue;
        }

        const std::string Header0 = ToLower(Trim(Table.mHeaders[0]));
        const std::string Header1 = ToLower(Trim(Table.mHeaders[1]));
        if (Header0 != "property" || Header1 != "value")
        {
            continue;
        }

        for (const std::vector<std::string> &Row : Table.mRows)
        {
            if (Row.size() < 2)
            {
                continue;
            }
            SchemaField Field;
            Field.mSectionId = Table.mSectionId;
            Field.mProperty = Row[0];
            Field.mValue = Row[1];
            Fields.push_back(std::move(Field));
        }
    }

    if (Fields.empty())
    {
        AddWarning(OutWarnings, "No `Property|Value` schema tables found in '" +
                                    ToGenericPath(InSchemaPath) + "'.");
    }
    return Fields;
}

bool IsSupportedSchemaType(const std::string &InType)
{
    static const std::set<std::string> Supported = {"doc",
                                                    "plan",
                                                    "playbook",
                                                    "implementation",
                                                    "changelog",
                                                    "verification",
                                                    "plan_changelog",
                                                    "plan_verification",
                                                    "impl_changelog",
                                                    "impl_verification",
                                                    "playbook_changelog",
                                                    "playbook_verification"};
    return Supported.count(InType) > 0;
}

static fs::path SchemaRelativePath(const std::string &InType)
{
    if (InType == "doc")
    {
        return fs::path("Schemas/Doc.Schema.md");
    }
    if (InType == "playbook")
    {
        return fs::path("Schemas/Playbook.Schema.md");
    }
    if (InType == "implementation")
    {
        return fs::path("Schemas/Implementation.Schema.md");
    }
    if (InType == "changelog")
    {
        return fs::path("Schemas/ChangeLog.Schema.md");
    }
    if (InType == "verification")
    {
        return fs::path("Schemas/Verification.Schema.md");
    }
    if (InType == "plan_changelog")
    {
        return fs::path("Schemas/PlanChangeLog.Schema.md");
    }
    if (InType == "plan_verification")
    {
        return fs::path("Schemas/PlanVerification.Schema.md");
    }
    if (InType == "impl_changelog")
    {
        return fs::path("Schemas/ImplChangeLog.Schema.md");
    }
    if (InType == "impl_verification")
    {
        return fs::path("Schemas/ImplVerification.Schema.md");
    }
    if (InType == "playbook_changelog")
    {
        return fs::path("Schemas/PlaybookChangeLog.Schema.md");
    }
    if (InType == "playbook_verification")
    {
        return fs::path("Schemas/PlaybookVerification.Schema.md");
    }
    return fs::path("Schemas/Plan.Schema.md");
}

fs::path ResolveSchemaFilePath(const std::string &InType,
                               const fs::path &InRepoRoot)
{
    const fs::path RelPath = SchemaRelativePath(InType);

    // 1. Try repo-local schema
    if (!InRepoRoot.empty())
    {
        const fs::path RepoSchema = InRepoRoot / RelPath;
        if (fs::exists(RepoSchema))
        {
            return RepoSchema;
        }
    }

    // 2. Try bundled schemas next to executable
    const fs::path ExeDir = GetExecutableDirectory();
    const fs::path BundledSchema = ExeDir / RelPath;
    if (fs::exists(BundledSchema))
    {
        return BundledSchema;
    }

    // 3. Fallback: return repo-local path even if missing (callers handle
    // non-existence)
    if (!InRepoRoot.empty())
    {
        return InRepoRoot / RelPath;
    }
    return RelPath;
}

std::vector<std::string> BuildSchemaExamples(const std::string & /*InType*/)
{
    // Examples are repo-specific; return empty to avoid hardcoding paths
    return {};
}

std::string JoinMarkdownRowCells(const std::vector<std::string> &InRow)
{
    std::ostringstream Stream;
    for (size_t Index = 0; Index < InRow.size(); ++Index)
    {
        if (Index > 0)
        {
            Stream << " | ";
        }
        Stream << Trim(InRow[Index]);
    }
    return Stream.str();
}

bool RowContainsAllTerms(const std::vector<std::string> &InRow,
                         const std::vector<std::string> &InTerms)
{
    if (InTerms.empty())
    {
        return true;
    }

    const std::string RowLower = ToLower(JoinMarkdownRowCells(InRow));
    for (const std::string &Term : InTerms)
    {
        if (RowLower.find(ToLower(Term)) == std::string::npos)
        {
            return false;
        }
    }
    return true;
}

bool TryResolveRuleProvenance(const fs::path &InRepoRoot,
                              const RuleProvenanceProbe &InProbe,
                              RuleEntry &InOutRule,
                              std::vector<std::string> &OutWarnings)
{
    const fs::path AbsolutePath = InRepoRoot / fs::path(InProbe.mPath);
    std::vector<std::string> Lines;
    std::string ReadError;
    if (!TryReadFileLines(AbsolutePath, Lines, ReadError))
    {
        AddWarning(OutWarnings, "Rule provenance read failed for '" +
                                    InProbe.mPath + "' (`" + InOutRule.mId +
                                    "`): " + ReadError);
        return false;
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    const std::vector<MarkdownTableRecord> Tables =
        ParseMarkdownTables(Lines, Headings);
    const std::string TargetSectionId = NormalizeSectionId(InProbe.mSectionId);

    for (const MarkdownTableRecord &Table : Tables)
    {
        if (!TargetSectionId.empty() && Table.mSectionId != TargetSectionId)
        {
            continue;
        }

        for (size_t RowIndex = 0; RowIndex < Table.mRows.size(); ++RowIndex)
        {
            const std::vector<std::string> &Row = Table.mRows[RowIndex];
            if (!RowContainsAllTerms(Row, InProbe.mRowTerms))
            {
                continue;
            }

            InOutRule.mbSourceResolved = true;
            InOutRule.mSourcePath = InProbe.mPath;
            InOutRule.mSourceSectionId = Table.mSectionId;
            InOutRule.mSourceTableId = Table.mTableId;
            InOutRule.mSourceRowIndex = static_cast<int>(RowIndex) + 1;
            InOutRule.mSourceEvidence = JoinMarkdownRowCells(Row);
            if (!InOutRule.mSourceSectionId.empty())
            {
                InOutRule.mSource =
                    InOutRule.mSourcePath + "#" + InOutRule.mSourceSectionId;
            }
            else
            {
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
                                  std::vector<std::string> &OutWarnings)
{
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
         "prepare the phase playbook execution lanes (and `testing` for "
         "testable "
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
         "Keep long-running change/verification history in detached sidecars "
         "and "
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

    if (Rules.size() != Probes.size())
    {
        AddWarning(OutWarnings, "Rule probe configuration mismatch; provenance "
                                "fallback is active.");
        return Rules;
    }

    for (size_t Index = 0; Index < Rules.size(); ++Index)
    {
        TryResolveRuleProvenance(InRepoRoot, Probes[Index], Rules[Index],
                                 OutWarnings);
    }
    return Rules;
}

bool HasIndexedHeadingPrefix(const std::string &InHeadingText)
{
    static const std::regex IndexedPrefixRegex(R"(^\s*\d+\s*[\.\)])");
    return std::regex_search(InHeadingText, IndexedPrefixRegex);
}

std::string BuildTopicPhaseIdentityNormalized(const std::string &InTopicKey,
                                              const std::string &InPhaseKey)
{
    return ToLower(Trim(InTopicKey)) + "::" + ToLower(Trim(InPhaseKey));
}

std::string ExtractPhaseKeyFromCell(const std::string &InCellValue)
{
    const std::string CellValue = Trim(InCellValue);
    const size_t TickStart = CellValue.find('`');
    if (TickStart != std::string::npos)
    {
        const size_t TickEnd = CellValue.find('`', TickStart + 1);
        if (TickEnd != std::string::npos && TickEnd > TickStart + 1)
        {
            return Trim(
                CellValue.substr(TickStart + 1, TickEnd - TickStart - 1));
        }
    }

    static const std::regex TokenPattern(R"(([A-Za-z][A-Za-z0-9_-]*))");
    std::smatch Match;
    if (std::regex_search(CellValue, Match, TokenPattern))
    {
        return Trim(Match[1].str());
    }
    return "";
}

std::vector<ActivePhaseRecord>
CollectActivePhaseRecords(const fs::path &InRepoRoot,
                          const std::vector<DocumentRecord> &InPlans,
                          std::vector<std::string> &OutWarnings)
{
    std::vector<ActivePhaseRecord> ActivePhases;
    for (const DocumentRecord &Plan : InPlans)
    {
        const fs::path AbsolutePath = InRepoRoot / fs::path(Plan.mPath);
        std::vector<std::string> Lines;
        std::string ReadError;
        if (!TryReadFileLines(AbsolutePath, Lines, ReadError))
        {
            AddWarning(OutWarnings, "Active-phase parse skipped for '" +
                                        Plan.mPath + "': " + ReadError);
            continue;
        }

        const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
        const std::vector<MarkdownTableRecord> Tables =
            ParseMarkdownTables(Lines, Headings);
        for (const MarkdownTableRecord &Table : Tables)
        {
            if (Table.mSectionId != "implementation_phases")
            {
                continue;
            }

            const int PhaseIndex = FindHeaderIndex(Table.mHeaders, "phase");
            const int StatusIndex = FindHeaderIndex(Table.mHeaders, "status");
            if (PhaseIndex < 0 || StatusIndex < 0)
            {
                continue;
            }

            for (const std::vector<std::string> &Row : Table.mRows)
            {
                const size_t PhaseCellIndex = static_cast<size_t>(PhaseIndex);
                const size_t StatusCellIndex = static_cast<size_t>(StatusIndex);
                if (PhaseCellIndex >= Row.size() ||
                    StatusCellIndex >= Row.size())
                {
                    continue;
                }

                const std::string PhaseKey =
                    ExtractPhaseKeyFromCell(Row[PhaseCellIndex]);
                if (PhaseKey.empty())
                {
                    continue;
                }

                const std::string StatusRaw = Trim(Row[StatusCellIndex]);
                const std::string Status = NormalizeStatusValue(StatusRaw);
                if (Status != "in_progress" && Status != "blocked")
                {
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
BuildHeadingIdSet(const std::vector<HeadingRecord> &InHeadings)
{
    std::set<std::string> HeadingIds;
    for (const HeadingRecord &Heading : InHeadings)
    {
        HeadingIds.insert(Heading.mSectionId);
    }
    return HeadingIds;
}

bool IsPlaybookPhaseEntryReady(const fs::path &InPlaybookAbsolutePath,
                               std::vector<std::string> &OutMissingSections,
                               std::string &OutReadError)
{
    std::vector<std::string> Lines;
    if (!TryReadFileLines(InPlaybookAbsolutePath, Lines, OutReadError))
    {
        return false;
    }

    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    const std::set<std::string> HeadingIds = BuildHeadingIdSet(Headings);
    const std::vector<std::string> RequiredSections = {
        "phase_binding", "investigation_baseline", "phase_entry_readiness_gate",
        "execution_lanes"};

    for (const std::string &RequiredSection : RequiredSections)
    {
        if (HeadingIds.count(RequiredSection) == 0)
        {
            OutMissingSections.push_back(RequiredSection);
        }
    }
    return true;
}

PhaseEntryGateResult
EvaluatePhaseEntryGate(const fs::path &InRepoRoot,
                       const std::vector<DocumentRecord> &InPlans,
                       const std::vector<DocumentRecord> &InPlaybooks,
                       std::vector<std::string> &OutWarnings)
{
    PhaseEntryGateResult Result;
    const std::vector<ActivePhaseRecord> ActivePhases =
        CollectActivePhaseRecords(InRepoRoot, InPlans, OutWarnings);
    Result.mActivePhaseCount = static_cast<int>(ActivePhases.size());

    std::map<std::string, DocumentRecord> PlaybookByTopicPhase;
    for (const DocumentRecord &Playbook : InPlaybooks)
    {
        const std::string Identity = BuildTopicPhaseIdentityNormalized(
            Playbook.mTopicKey, Playbook.mPhaseKey);
        if (PlaybookByTopicPhase.count(Identity) == 0)
        {
            PlaybookByTopicPhase.emplace(Identity, Playbook);
        }
    }

    for (const ActivePhaseRecord &ActivePhase : ActivePhases)
    {
        const std::string Identity = BuildTopicPhaseIdentityNormalized(
            ActivePhase.mTopicKey, ActivePhase.mPhaseKey);
        const auto FoundPlaybook = PlaybookByTopicPhase.find(Identity);
        if (FoundPlaybook == PlaybookByTopicPhase.end())
        {
            Result.mMissingPlaybookCount += 1;
            AddWarning(OutWarnings, "Active phase '" + ActivePhase.mPhaseKey +
                                        "' for topic '" +
                                        ActivePhase.mTopicKey + "' in '" +
                                        ActivePhase.mPlanPath +
                                        "' has no matching playbook.");
            continue;
        }

        std::vector<std::string> MissingSections;
        std::string ReadError;
        const fs::path PlaybookAbsolutePath =
            InRepoRoot / fs::path(FoundPlaybook->second.mPath);
        if (!IsPlaybookPhaseEntryReady(PlaybookAbsolutePath, MissingSections,
                                       ReadError))
        {
            Result.mUnpreparedPlaybookCount += 1;
            AddWarning(OutWarnings,
                       "Active-phase playbook readiness parse failed for '" +
                           FoundPlaybook->second.mPath + "': " + ReadError);
            continue;
        }

        if (!MissingSections.empty())
        {
            Result.mUnpreparedPlaybookCount += 1;
            std::ostringstream Missing;
            for (size_t Index = 0; Index < MissingSections.size(); ++Index)
            {
                if (Index > 0)
                {
                    Missing << ", ";
                }
                Missing << MissingSections[Index];
            }
            AddWarning(OutWarnings,
                       "Active-phase playbook '" + FoundPlaybook->second.mPath +
                           "' is missing readiness sections: " + Missing.str() +
                           ".");
        }
    }

    return Result;
}

ArtifactRoleBoundaryResult EvaluateArtifactRoleBoundaries(
    const fs::path &InRepoRoot, const std::vector<DocumentRecord> &InPlaybooks,
    const std::vector<DocumentRecord> &InImplementations,
    std::vector<std::string> &OutWarnings)
{
    ArtifactRoleBoundaryResult Result;

    const std::vector<std::string> ForbiddenPlaybookSections = {
        "progress_summary", "phase_tracking", "implementation_phases"};
    for (const DocumentRecord &Playbook : InPlaybooks)
    {
        const fs::path AbsolutePath = InRepoRoot / fs::path(Playbook.mPath);
        std::vector<std::string> Lines;
        std::string ReadError;
        if (!TryReadFileLines(AbsolutePath, Lines, ReadError))
        {
            AddWarning(OutWarnings,
                       "Artifact-role boundary parse skipped for playbook '" +
                           Playbook.mPath + "': " + ReadError);
            continue;
        }

        const std::set<std::string> HeadingIds =
            BuildHeadingIdSet(ParseHeadingRecords(Lines));
        std::vector<std::string> Violations;
        for (const std::string &Forbidden : ForbiddenPlaybookSections)
        {
            if (HeadingIds.count(Forbidden) > 0)
            {
                Violations.push_back(Forbidden);
            }
        }

        if (!Violations.empty())
        {
            Result.mPlaybookViolationCount += 1;
            AddWarning(OutWarnings,
                       "Playbook role-boundary violation in '" +
                           Playbook.mPath +
                           "' (implementation-owned sections detected).");
        }
    }

    const std::vector<std::string> ForbiddenImplementationSections = {
        "phase_entry_readiness_gate", "execution_lanes",
        "investigation_baseline", "lane_artifact_contract", "testing"};
    for (const DocumentRecord &Implementation : InImplementations)
    {
        const fs::path AbsolutePath =
            InRepoRoot / fs::path(Implementation.mPath);
        std::vector<std::string> Lines;
        std::string ReadError;
        if (!TryReadFileLines(AbsolutePath, Lines, ReadError))
        {
            AddWarning(
                OutWarnings,
                "Artifact-role boundary parse skipped for implementation '" +
                    Implementation.mPath + "': " + ReadError);
            continue;
        }

        const std::set<std::string> HeadingIds =
            BuildHeadingIdSet(ParseHeadingRecords(Lines));
        std::vector<std::string> Violations;
        for (const std::string &Forbidden : ForbiddenImplementationSections)
        {
            if (HeadingIds.count(Forbidden) > 0)
            {
                Violations.push_back(Forbidden);
            }
        }

        if (!Violations.empty())
        {
            Result.mImplementationViolationCount += 1;
            AddWarning(OutWarnings,
                       "Implementation role-boundary violation in '" +
                           Implementation.mPath +
                           "' (playbook-owned sections detected).");
        }
    }

    return Result;
}

PlaybookSchemaResult
EvaluatePlaybookSchema(const fs::path &InRepoRoot,
                       const std::vector<DocumentRecord> &InPlaybooks,
                       std::vector<std::string> &OutWarnings)
{
    PlaybookSchemaResult Result;
    const std::vector<SectionSchemaEntry> SchemaEntries =
        BuildSectionSchemaEntries("playbook", InRepoRoot);
    std::vector<std::string> RequiredSectionIds;
    for (const SectionSchemaEntry &Entry : SchemaEntries)
    {
        if (Entry.mbRequired)
        {
            RequiredSectionIds.push_back(Entry.mSectionId);
        }
    }

    Result.mPlaybookCount = static_cast<int>(InPlaybooks.size());
    for (const DocumentRecord &Playbook : InPlaybooks)
    {
        const fs::path AbsolutePath = InRepoRoot / fs::path(Playbook.mPath);
        std::vector<std::string> Lines;
        std::string ReadError;
        if (!TryReadFileLines(AbsolutePath, Lines, ReadError))
        {
            Result.mReadFailureCount += 1;
            AddWarning(OutWarnings, "Playbook-schema check skipped for '" +
                                        Playbook.mPath + "': " + ReadError);
            continue;
        }

        const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
        std::set<std::string> H2Ids;
        for (const HeadingRecord &Heading : Headings)
        {
            if (Heading.mLevel == 2)
            {
                H2Ids.insert(Heading.mSectionId);
            }
        }

        std::vector<std::string> MissingSections;
        for (const std::string &Required : RequiredSectionIds)
        {
            if (H2Ids.count(Required) == 0)
            {
                MissingSections.push_back(Required);
            }
        }
        if (!MissingSections.empty())
        {
            Result.mMissingSectionPlaybookCount += 1;
            Result.mDiagnostics.push_back("playbook=" + Playbook.mPath +
                                          " missing_required_sections=" +
                                          JoinCommaSeparated(MissingSections));
        }
    }

    return Result;
}

// ---------------------------------------------------------------------------
// Section Schema renderers
// ---------------------------------------------------------------------------

// SectionSchemaEntry moved to DocTypes.h

std::vector<SectionSchemaEntry>
BuildSectionSchemaEntries(const std::string &InType, const fs::path &InRepoRoot)
{
    // Resolve schema file: repo-local first, then bundled next to executable
    const fs::path SchemaPath = ResolveSchemaFilePath(InType, InRepoRoot);
    if (fs::exists(SchemaPath))
    {
        std::vector<SectionSchemaEntry> Parsed =
            TryParseSectionSchemaFromFile(SchemaPath);
        if (!Parsed.empty())
        {
            return Parsed;
        }
    }

    return {};
}

void AppendGraphEdgeUnique(std::vector<GraphEdge> &InOutEdges,
                           std::set<std::string> &InOutEdgeKeys,
                           const std::string &InFromNodeId,
                           const std::string &InToNodeId,
                           const std::string &InKind, const int InDepth)
{
    const std::string Key = InFromNodeId + "|" + InToNodeId + "|" + InKind +
                            "|" + std::to_string(InDepth);
    if (InOutEdgeKeys.count(Key) > 0)
    {
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
                  const std::string &InPath, const std::string &InMessage)
{
    DriftItem Item;
    Item.mId = InId;
    Item.mSeverity = InSeverity;
    Item.mTopicKey = InTopicKey;
    Item.mPath = InPath;
    Item.mMessage = InMessage;
    InOutDrifts.push_back(std::move(Item));
}

int SeverityRank(const std::string &InSeverity)
{
    if (InSeverity == "error")
    {
        return 0;
    }
    if (InSeverity == "warning")
    {
        return 1;
    }
    return 2;
}


} // namespace UniPlan
