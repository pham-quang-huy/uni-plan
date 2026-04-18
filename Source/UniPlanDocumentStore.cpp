#include "UniPlanDocumentStore.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanJsonIO.h"

#include <map>
#include <set>
#include <string>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Fragment parsing: "Docs/Plans/X.Plan.json#playbook:P1"
//   → filePath = "Docs/Plans/X.Plan.json"
//   → fragment = "playbook:P1"
// ---------------------------------------------------------------------------

static bool ParseBundlePath(const std::string &InPath, std::string &OutFilePath,
                            std::string &OutFragment)
{
    const size_t HashPos = InPath.find('#');
    if (HashPos == std::string::npos)
    {
        OutFilePath = InPath;
        OutFragment.clear();
        return false;
    }
    OutFilePath = InPath.substr(0, HashPos);
    OutFragment = InPath.substr(HashPos + 1);
    return true;
}

// ---------------------------------------------------------------------------
// Synthesize helper — render a typed validation_commands vector as a
// markdown table string. Used by the Document-synthesis layer so legacy
// markdown consumers continue to see readable output.
// ---------------------------------------------------------------------------

static std::string RenderValidationCommandsAsMarkdown(
    const std::vector<FValidationCommand> &InCommands)
{
    if (InCommands.empty())
        return "";
    std::string Out = "| Platform | Command | Description |\n"
                      "| --- | --- | --- |\n";
    for (const FValidationCommand &C : InCommands)
    {
        Out += "| ";
        Out += ToString(C.mPlatform);
        Out += " | `";
        Out += C.mCommand;
        Out += "` | ";
        Out += C.mDescription;
        Out += " |\n";
    }
    return Out;
}

// ---------------------------------------------------------------------------
// Synthesize helper — render a typed dependencies vector as a markdown
// table string. Mirrors RenderValidationCommandsAsMarkdown.
// ---------------------------------------------------------------------------

static std::string
RenderDependenciesAsMarkdown(const std::vector<FBundleReference> &InDeps)
{
    if (InDeps.empty())
        return "";
    std::string Out = "| Kind | Topic | Phase | Path | Note |\n"
                      "| --- | --- | --- | --- | --- |\n";
    for (const FBundleReference &R : InDeps)
    {
        std::string PhaseCell = R.mPhase < 0 ? "" : std::to_string(R.mPhase);
        Out += "| ";
        Out += ToString(R.mKind);
        Out += " | ";
        Out += R.mTopic;
        Out += " | ";
        Out += PhaseCell;
        Out += " | ";
        Out += R.mPath;
        Out += " | ";
        Out += R.mNote;
        Out += " |\n";
    }
    return Out;
}

// ---------------------------------------------------------------------------
// Synthesize helper — add a section from a V4 string field
// ---------------------------------------------------------------------------

static void AddSynthesizedSection(FDocument &OutDocument,
                                  const std::string &InID,
                                  const std::string &InContent)
{
    if (InContent.empty())
    {
        return;
    }
    FSectionContent Section;
    Section.mSectionID = InID;
    Section.mHeading = InID;
    Section.mLevel = 2;
    Section.mContent = InContent;
    OutDocument.mSections[InID] = std::move(Section);
}

// ---------------------------------------------------------------------------
// Extract sub-document from bundle by fragment.
// Synthesizes FDocument from V4 domain fields.
// ---------------------------------------------------------------------------

static bool ExtractDocumentFromBundle(const FTopicBundle &InBundle,
                                      const std::string &InFragment,
                                      FDocument &OutDocument)
{
    if (InFragment == "plan")
    {
        OutDocument = FDocument{};
        OutDocument.mIdentity.mType = EDocumentType::Plan;
        OutDocument.mIdentity.mTopicKey = InBundle.mTopicKey;
        const FPlanMetadata &Meta = InBundle.mMetadata;
        OutDocument.mTitle = Meta.mTitle;
        OutDocument.mStatus = PhaseStatusFromString(ToString(InBundle.mStatus));
        OutDocument.mStatusRaw = ToString(InBundle.mStatus);
        AddSynthesizedSection(OutDocument, "summary", Meta.mSummary);
        AddSynthesizedSection(OutDocument, "goals_and_non_goals", Meta.mGoals);
        AddSynthesizedSection(OutDocument, "risks", Meta.mRisks);
        AddSynthesizedSection(OutDocument, "acceptance_criteria",
                              Meta.mAcceptanceCriteria);
        AddSynthesizedSection(OutDocument, "problem_statement",
                              Meta.mProblemStatement);
        AddSynthesizedSection(
            OutDocument, "validation_commands",
            RenderValidationCommandsAsMarkdown(Meta.mValidationCommands));
        AddSynthesizedSection(OutDocument, "baseline_audit",
                              Meta.mBaselineAudit);
        AddSynthesizedSection(OutDocument, "execution_strategy",
                              Meta.mExecutionStrategy);
        AddSynthesizedSection(OutDocument, "locked_decisions",
                              Meta.mLockedDecisions);
        AddSynthesizedSection(OutDocument, "source_references",
                              Meta.mSourceReferences);
        AddSynthesizedSection(OutDocument, "dependencies",
                              RenderDependenciesAsMarkdown(Meta.mDependencies));
        AddSynthesizedSection(OutDocument, "next_actions",
                              InBundle.mNextActions);
        return true;
    }

    if (InFragment == "implementation")
    {
        OutDocument = FDocument{};
        OutDocument.mIdentity.mType = EDocumentType::Implementation;
        OutDocument.mIdentity.mTopicKey = InBundle.mTopicKey;
        OutDocument.mTitle = InBundle.mMetadata.mTitle;
        // Build a tracking table from phases
        FSectionContent Section;
        Section.mSectionID = "tracking";
        Section.mHeading = "tracking";
        Section.mLevel = 2;
        FStructuredTable Table;
        Table.mTableID = 0;
        Table.mSectionID = "tracking";
        Table.mHeaders = {"Phase", "Scope", "Status"};
        for (size_t Index = 0; Index < InBundle.mPhases.size(); ++Index)
        {
            const FPhaseRecord &Phase = InBundle.mPhases[Index];
            Table.mRows.push_back(
                {FTableCell{std::to_string(Index)}, FTableCell{Phase.mScope},
                 FTableCell{std::string(ToString(Phase.mLifecycle.mStatus))}});
        }
        Section.mTables.push_back(std::move(Table));
        OutDocument.mSections["tracking"] = std::move(Section);
        return true;
    }

    // playbook:<PhaseIndex>
    if (InFragment.size() > 9 && InFragment.substr(0, 9) == "playbook:")
    {
        const std::string PhaseKey = InFragment.substr(9);
        int PhaseIndex = -1;
        try
        {
            PhaseIndex = std::stoi(PhaseKey);
        }
        catch (...)
        {
            return false;
        }
        if (PhaseIndex < 0 ||
            PhaseIndex >= static_cast<int>(InBundle.mPhases.size()))
        {
            return false;
        }
        const FPhaseRecord &Phase =
            InBundle.mPhases[static_cast<size_t>(PhaseIndex)];
        OutDocument = FDocument{};
        OutDocument.mIdentity.mType = EDocumentType::Playbook;
        OutDocument.mIdentity.mTopicKey = InBundle.mTopicKey;
        OutDocument.mTitle = Phase.mScope;
        const std::string PhaseStatusStr(ToString(Phase.mLifecycle.mStatus));
        OutDocument.mStatus = PhaseStatusFromString(PhaseStatusStr);
        OutDocument.mStatusRaw = PhaseStatusStr;
        AddSynthesizedSection(OutDocument, "scope", Phase.mScope);
        AddSynthesizedSection(OutDocument, "output", Phase.mOutput);
        AddSynthesizedSection(OutDocument, "investigation",
                              Phase.mDesign.mInvestigation);
        AddSynthesizedSection(OutDocument, "code_snippets",
                              Phase.mDesign.mCodeSnippets);
        AddSynthesizedSection(
            OutDocument, "dependencies",
            RenderDependenciesAsMarkdown(Phase.mDesign.mDependencies));
        AddSynthesizedSection(OutDocument, "readiness_gate",
                              Phase.mDesign.mReadinessGate);
        AddSynthesizedSection(OutDocument, "handoff", Phase.mDesign.mHandoff);
        AddSynthesizedSection(OutDocument, "validation_commands",
                              RenderValidationCommandsAsMarkdown(
                                  Phase.mDesign.mValidationCommands));
        return true;
    }

    // changelog:<phase>
    if (InFragment.size() >= 10 && InFragment.substr(0, 10) == "changelog:")
    {
        const std::string Owner = InFragment.substr(10);
        OutDocument = FDocument{};
        OutDocument.mIdentity.mType = EDocumentType::ChangeLog;
        OutDocument.mIdentity.mTopicKey = InBundle.mTopicKey;
        FSectionContent Section;
        Section.mSectionID = "entries";
        Section.mHeading = "entries";
        Section.mLevel = 2;
        FStructuredTable Table;
        Table.mTableID = 0;
        Table.mSectionID = "entries";
        Table.mHeaders = {"Date", "Change", "Files"};
        bool Found = false;
        for (const FChangeLogEntry &Entry : InBundle.mChangeLogs)
        {
            const std::string PhaseStr =
                Entry.mPhase < 0 ? "" : std::to_string(Entry.mPhase);
            if (PhaseStr != Owner)
                continue;
            Found = true;
            Table.mRows.push_back({FTableCell{Entry.mDate},
                                   FTableCell{Entry.mChange},
                                   FTableCell{Entry.mAffected}});
        }
        if (!Found)
            return false;
        Section.mTables.push_back(std::move(Table));
        OutDocument.mSections["entries"] = std::move(Section);
        return true;
    }

    // verification:<phase>
    if (InFragment.size() >= 13 && InFragment.substr(0, 13) == "verification:")
    {
        const std::string Owner = InFragment.substr(13);
        OutDocument = FDocument{};
        OutDocument.mIdentity.mType = EDocumentType::Verification;
        OutDocument.mIdentity.mTopicKey = InBundle.mTopicKey;
        FSectionContent Section;
        Section.mSectionID = "entries";
        Section.mHeading = "entries";
        Section.mLevel = 2;
        FStructuredTable Table;
        Table.mTableID = 0;
        Table.mSectionID = "entries";
        Table.mHeaders = {"Date", "Check", "Result", "Detail"};
        bool Found = false;
        for (const FVerificationEntry &Entry : InBundle.mVerifications)
        {
            const std::string PhaseStr =
                Entry.mPhase < 0 ? "" : std::to_string(Entry.mPhase);
            if (PhaseStr != Owner)
                continue;
            Found = true;
            Table.mRows.push_back(
                {FTableCell{Entry.mDate}, FTableCell{Entry.mCheck},
                 FTableCell{Entry.mResult}, FTableCell{Entry.mDetail}});
        }
        if (!Found)
            return false;
        Section.mTables.push_back(std::move(Table));
        OutDocument.mSections["entries"] = std::move(Section);
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// TryLoadDocument — fragment-aware bundle loading
// ---------------------------------------------------------------------------

// In-memory bundle cache to avoid re-parsing the same
// large JSON file hundreds of times during a snapshot build.
static std::map<std::string, FTopicBundle> sBundleCache;

void ClearBundleCache()
{
    sBundleCache.clear();
}

bool TryLoadDocument(const fs::path &InRepoRoot,
                     const std::string &InRelativePath, FDocument &OutDocument,
                     std::string &OutError)
{
    std::string FilePath;
    std::string Fragment;

    if (ParseBundlePath(InRelativePath, FilePath, Fragment))
    {
        // Check cache first
        const std::string CacheKey = InRepoRoot.string() + "/" + FilePath;
        auto CacheIt = sBundleCache.find(CacheKey);
        if (CacheIt == sBundleCache.end())
        {
            // Parse and cache
            const fs::path AbsPath = InRepoRoot / FilePath;
            FTopicBundle Bundle;
            if (!TryReadTopicBundle(AbsPath, Bundle, OutError))
            {
                return false;
            }
            CacheIt = sBundleCache.emplace(CacheKey, std::move(Bundle)).first;
        }
        if (!ExtractDocumentFromBundle(CacheIt->second, Fragment, OutDocument))
        {
            OutError = "Fragment not found in bundle: " + Fragment;
            return false;
        }
        return true;
    }

    // No fragment — try as single-doc JSON, then .md fallback
    const fs::path AbsPath = InRepoRoot / InRelativePath;
    const std::string Ext = AbsPath.extension().string();

    if (Ext == ".json")
    {
        // Read as bundle and synthesize plan document
        FTopicBundle Bundle;
        if (!TryReadTopicBundle(AbsPath, Bundle, OutError))
        {
            return false;
        }
        return ExtractDocumentFromBundle(Bundle, "plan", OutDocument);
    }

    if (Ext == ".md")
    {
        // Markdown fallback: parse headings/tables into
        // FDocument. Used during bundle migration only.
        std::vector<std::string> Lines;
        if (!TryReadFileLines(AbsPath, Lines, OutError))
        {
            return false;
        }
        const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
        const std::vector<MarkdownTableRecord> Tables =
            ParseMarkdownTables(Lines, Headings);

        // Title from H1
        for (const HeadingRecord &H : Headings)
        {
            if (H.mLevel == 1)
            {
                OutDocument.mTitle = H.mText;
                break;
            }
        }

        // Build sections from H2 headings
        const int TotalLines = static_cast<int>(Lines.size());
        std::set<int> TableLineSet;
        for (const MarkdownTableRecord &T : Tables)
        {
            for (int L = T.mStartLine; L <= T.mEndLine; ++L)
                TableLineSet.insert(L);
        }
        std::set<int> HeadingLineSet;
        for (const HeadingRecord &H : Headings)
        {
            HeadingLineSet.insert(H.mLine);
        }

        for (size_t HI = 0; HI < Headings.size(); ++HI)
        {
            // Create sections for H2 and H3 headings.
            // H3 sections like wave_lane_job_board,
            // target_file_manifest must be addressable
            // by section ID.
            if (Headings[HI].mLevel < 2 || Headings[HI].mLevel > 3)
                continue;

            FSectionContent Section;
            Section.mSectionID = Headings[HI].mSectionId;
            Section.mHeading = Headings[HI].mText;
            Section.mLevel = Headings[HI].mLevel;

            // Find section end — next heading at same or
            // higher level
            int SectionEnd = TotalLines - 1;
            const int CurrentLevel = Headings[HI].mLevel;
            for (size_t NI = HI + 1; NI < Headings.size(); ++NI)
            {
                if (Headings[NI].mLevel <= CurrentLevel)
                {
                    SectionEnd = Headings[NI].mLine - 1;
                    break;
                }
            }

            int ContentStart = Headings[HI].mLine + 1;

            // Collect tables
            for (const MarkdownTableRecord &T : Tables)
            {
                if (T.mStartLine >= ContentStart && T.mStartLine <= SectionEnd)
                {
                    FStructuredTable ST;
                    ST.mTableID = T.mTableId;
                    ST.mSectionID = Section.mSectionID;
                    ST.mHeaders = T.mHeaders;
                    for (const auto &Row : T.mRows)
                    {
                        std::vector<FTableCell> Cells;
                        for (const std::string &C : Row)
                            Cells.push_back(FTableCell{Trim(C)});
                        ST.mRows.push_back(std::move(Cells));
                    }
                    Section.mTables.push_back(std::move(ST));
                }
            }

            // Collect free-text content (skip tables, headings,
            // and pipe-delimited rows that look like tables)
            std::string ContentText;
            for (int L = ContentStart; L <= SectionEnd; ++L)
            {
                if (TableLineSet.count(L) > 0)
                    continue;
                if (HeadingLineSet.count(L) > 0)
                    continue;
                const std::string &Line = Lines[static_cast<size_t>(L)];
                const std::string Trimmed = Trim(Line);
                if (Trimmed.empty())
                    continue;
                // Skip pipe-delimited lines (informal tables)
                if (Trimmed.front() == '|')
                    continue;
                // Skip heading-formatted lines
                if (Trimmed.front() == '#')
                    continue;
                // Skip horizontal rules
                if (Trimmed.substr(0, 3) == "---" ||
                    Trimmed.substr(0, 3) == "===")
                    continue;
                if (!ContentText.empty())
                    ContentText += "\n";
                ContentText += Trimmed;
            }
            Section.mContent = std::move(ContentText);

            // Collect subsection IDs
            for (size_t SI = HI + 1; SI < Headings.size(); ++SI)
            {
                if (Headings[SI].mLevel <= 2)
                    break;
                if (Headings[SI].mLevel == 3)
                    Section.mSubsectionIDs.push_back(Headings[SI].mSectionId);
            }

            OutDocument.mSections[Section.mSectionID] = std::move(Section);
        }

        return true;
    }

    OutError = "Unsupported format: " + Ext;
    return false;
}

// ---------------------------------------------------------------------------
// TrySaveDocument — fragment-aware bundle save
// ---------------------------------------------------------------------------

bool TrySaveDocument(const fs::path &InRepoRoot, const FDocument &InDocument,
                     std::string &OutError)
{
    const std::string &FullPath = InDocument.mIdentity.mFilePath;

    if (FullPath.empty())
    {
        OutError = "Document has empty file path";
        return false;
    }

    std::string FilePath;
    std::string Fragment;

    if (ParseBundlePath(FullPath, FilePath, Fragment))
    {
        OutError = "FDocument save via fragment path is no longer "
                   "supported — use typed V4 mutation APIs";
        return false;
    }

    OutError = "FDocument save without fragment path is no longer "
               "supported — use TryWriteTopicBundle directly";
    return false;
}

// ---------------------------------------------------------------------------
// ResolveSectionFromDocument
// ---------------------------------------------------------------------------

FSectionContent ResolveSectionFromDocument(const FDocument &InDocument,
                                           const std::string &InSectionID)
{
    const auto It = InDocument.mSections.find(InSectionID);
    if (It != InDocument.mSections.end())
    {
        return It->second;
    }
    return FSectionContent{};
}

// ---------------------------------------------------------------------------
// ResolveTableFromDocument
// ---------------------------------------------------------------------------

FStructuredTable ResolveTableFromDocument(const FDocument &InDocument,
                                          int InTableID)
{
    for (const FStructuredTable &Table : InDocument.mTables)
    {
        if (Table.mTableID == InTableID)
        {
            return Table;
        }
    }

    for (const auto &Pair : InDocument.mSections)
    {
        for (const FStructuredTable &Table : Pair.second.mTables)
        {
            if (Table.mTableID == InTableID)
            {
                return Table;
            }
        }
    }

    FStructuredTable Empty;
    Empty.mTableID = -1;
    return Empty;
}

// ---------------------------------------------------------------------------
// TryLoadPhaseRecord — load phase directly from bundle cache
// ---------------------------------------------------------------------------

bool TryLoadPhaseRecord(const fs::path &InRepoRoot,
                        const std::string &InPlaybookPath,
                        FPhaseRecord &OutPhase, std::string &OutError)
{
    std::string FilePath;
    std::string Fragment;

    if (!ParseBundlePath(InPlaybookPath, FilePath, Fragment))
    {
        OutError = "Not a fragment path: " + InPlaybookPath;
        return false;
    }

    // Extract phase key from "playbook:P1"
    if (Fragment.substr(0, 9) != "playbook:")
    {
        OutError = "Not a playbook fragment: " + Fragment;
        return false;
    }
    const std::string PhaseKey = Fragment.substr(9);

    // Load bundle (cache-aware)
    const std::string CacheKey = InRepoRoot.string() + "/" + FilePath;
    auto CacheIt = sBundleCache.find(CacheKey);
    if (CacheIt == sBundleCache.end())
    {
        const fs::path AbsPath = InRepoRoot / FilePath;
        FTopicBundle Bundle;
        if (!TryReadTopicBundle(AbsPath, Bundle, OutError))
        {
            return false;
        }
        CacheIt = sBundleCache.emplace(CacheKey, std::move(Bundle)).first;
    }

    // Find phase by index (parse "P<N>" key → array index)
    const FTopicBundle &Bundle = CacheIt->second;
    if (PhaseKey.size() >= 2 && PhaseKey[0] == 'P')
    {
        int Idx = std::atoi(PhaseKey.c_str() + 1);
        if (Idx >= 0 && static_cast<size_t>(Idx) < Bundle.mPhases.size())
        {
            OutPhase = Bundle.mPhases[static_cast<size_t>(Idx)];
            return true;
        }
    }

    OutError = "Phase not found: " + PhaseKey;
    return false;
}

// ---------------------------------------------------------------------------
// TryLoadTopicBundleCached — cache-aware full bundle access
// ---------------------------------------------------------------------------

bool TryLoadTopicBundleCached(const fs::path &InRepoRoot,
                              const std::string &InBundlePath,
                              const FTopicBundle *&OutBundle,
                              std::string &OutError)
{
    const std::string CacheKey = InRepoRoot.string() + "/" + InBundlePath;
    auto CacheIt = sBundleCache.find(CacheKey);
    if (CacheIt == sBundleCache.end())
    {
        const fs::path AbsPath = InRepoRoot / InBundlePath;
        FTopicBundle Bundle;
        if (!TryReadTopicBundle(AbsPath, Bundle, OutError))
        {
            return false;
        }
        CacheIt = sBundleCache.emplace(CacheKey, std::move(Bundle)).first;
    }
    OutBundle = &CacheIt->second;
    return true;
}

} // namespace UniPlan
