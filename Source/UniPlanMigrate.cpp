#include "UniPlanMigrate.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanJsonIO.h"

#include <algorithm>
#include <regex>
#include <set>
#include <sstream>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int FindSectionEndLine(const std::vector<HeadingRecord> &InHeadings,
                              size_t InHeadingIndex, int InTotalLines)
{
    const int Level = InHeadings[InHeadingIndex].mLevel;
    for (size_t Index = InHeadingIndex + 1; Index < InHeadings.size(); ++Index)
    {
        if (InHeadings[Index].mLevel <= Level)
        {
            return InHeadings[Index].mLine - 1;
        }
    }
    return InTotalLines - 1;
}

static bool IsTableLine(const std::string &InLine)
{
    const std::string Trimmed = Trim(InLine);
    return !Trimmed.empty() && Trimmed.front() == '|';
}

static bool IsSeparatorRow(const std::string &InLine)
{
    const std::string Trimmed = Trim(InLine);
    if (Trimmed.size() < 3 || Trimmed.front() != '|')
    {
        return false;
    }
    for (char Character : Trimmed)
    {
        if (Character != '|' && Character != '-' && Character != ':' &&
            Character != ' ')
        {
            return false;
        }
    }
    return true;
}

static std::string
ExtractSectionContent(const std::vector<std::string> &InLines, int InStartLine,
                      int InEndLine, const std::set<int> &InTableLines,
                      const std::set<int> &InHeadingLines)
{
    std::ostringstream Content;
    bool HadContent = false;
    for (int LineIndex = InStartLine; LineIndex <= InEndLine; ++LineIndex)
    {
        if (InTableLines.count(LineIndex) > 0)
        {
            continue;
        }
        if (InHeadingLines.count(LineIndex) > 0)
        {
            continue;
        }
        if (LineIndex >= 0 && LineIndex < static_cast<int>(InLines.size()))
        {
            const std::string &Line = InLines[static_cast<size_t>(LineIndex)];
            if (HadContent || !Trim(Line).empty())
            {
                if (HadContent)
                {
                    Content << "\n";
                }
                Content << Line;
                HadContent = true;
            }
        }
    }
    // Trim trailing whitespace
    std::string Result = Content.str();
    while (!Result.empty() && (Result.back() == '\n' || Result.back() == ' '))
    {
        Result.pop_back();
    }
    return Result;
}

static void
ExtractFieldsFromTable(const FStructuredTable &InTable,
                       std::map<std::string, std::string> &OutFields)
{
    if (InTable.mHeaders.size() != 2)
    {
        return;
    }
    for (const auto &Row : InTable.mRows)
    {
        if (Row.size() >= 2)
        {
            const std::string Key = Trim(Row[0].mValue);
            const std::string Value = Trim(Row[1].mValue);
            if (!Key.empty())
            {
                OutFields[Key] = Value;
            }
        }
    }
}

static void ExtractReferences(const std::vector<std::string> &InLines,
                              std::vector<FDocumentReference> &OutReferences)
{
    static const std::regex PathRegex(R"([A-Za-z0-9_./\\-]+\.(md|json))");
    std::set<std::string> Seen;
    for (const std::string &Line : InLines)
    {
        auto Begin = std::sregex_iterator(Line.begin(), Line.end(), PathRegex);
        auto End = std::sregex_iterator();
        for (auto It = Begin; It != End; ++It)
        {
            const std::string Path = (*It).str();
            if (Seen.count(Path) > 0)
            {
                continue;
            }
            Seen.insert(Path);

            FDocumentReference Ref;
            Ref.mTargetPath = Path;
            if (Path.find(".Plan.") != std::string::npos)
            {
                Ref.mRelationType = "plan";
            }
            else if (Path.find(".Impl.") != std::string::npos)
            {
                Ref.mRelationType = "implementation";
            }
            else if (Path.find(".Playbook.") != std::string::npos)
            {
                Ref.mRelationType = "playbook";
            }
            else if (Path.find("ChangeLog") != std::string::npos)
            {
                Ref.mRelationType = "changelog";
            }
            else if (Path.find("Verification") != std::string::npos)
            {
                Ref.mRelationType = "verification";
            }
            else
            {
                Ref.mRelationType = "reference";
            }
            OutReferences.push_back(std::move(Ref));
        }
    }
}

static std::string ReplaceExtension(const std::string &InPath,
                                    const std::string &InOldExt,
                                    const std::string &InNewExt)
{
    const size_t Pos = InPath.rfind(InOldExt);
    if (Pos != std::string::npos && Pos + InOldExt.size() == InPath.size())
    {
        return InPath.substr(0, Pos) + InNewExt;
    }
    return InPath;
}

// ---------------------------------------------------------------------------
// Core migration: markdown -> FDocument
// ---------------------------------------------------------------------------

bool TryMigrateMarkdownToDocument(const fs::path &InRepoRoot,
                                  const std::string &InRelativePath,
                                  FDocument &OutDocument, std::string &OutError)
{
    const fs::path AbsPath = InRepoRoot / InRelativePath;

    // Read file
    std::vector<std::string> Lines;
    if (!TryReadFileLines(AbsPath, Lines, OutError))
    {
        return false;
    }

    // Parse headings and tables
    const std::vector<HeadingRecord> Headings = ParseHeadingRecords(Lines);
    const std::vector<MarkdownTableRecord> Tables =
        ParseMarkdownTables(Lines, Headings);

    // Classify document type
    const std::string Filename = fs::path(InRelativePath).filename().string();
    DocumentRecord Record;
    SidecarRecord Sidecar;
    bool IsSidecar = false;

    if (TryClassifyCoreDocument(InRelativePath, Filename, Record))
    {
        switch (Record.mKind)
        {
        case EDocumentKind::Plan:
            OutDocument.mIdentity.mType = EDocumentType::Plan;
            break;
        case EDocumentKind::Playbook:
            OutDocument.mIdentity.mType = EDocumentType::Playbook;
            break;
        case EDocumentKind::Implementation:
            OutDocument.mIdentity.mType = EDocumentType::Implementation;
            break;
        }
        OutDocument.mIdentity.mTopicKey = Record.mTopicKey;
        OutDocument.mIdentity.mPhaseKey = Record.mPhaseKey;
    }
    else if (TryClassifySidecarDocument(InRelativePath, Filename, Sidecar))
    {
        IsSidecar = true;
        if (Sidecar.mDocKind == "ChangeLog")
        {
            OutDocument.mIdentity.mType = EDocumentType::ChangeLog;
        }
        else
        {
            OutDocument.mIdentity.mType = EDocumentType::Verification;
        }
        OutDocument.mIdentity.mTopicKey = Sidecar.mTopicKey;
        OutDocument.mIdentity.mPhaseKey = Sidecar.mPhaseKey;
    }
    else
    {
        OutError = "Cannot classify document: " + InRelativePath;
        return false;
    }

    // Set identity
    OutDocument.mIdentity.mFormat = EDocumentFormat::JSON;
    OutDocument.mIdentity.mFilePath =
        ReplaceExtension(InRelativePath, ".md", ".json");

    // Infer status
    if (!IsSidecar)
    {
        std::vector<std::string> Warnings;
        const StatusInference Status =
            InferDocumentStatus(Record.mKind, AbsPath, Warnings);
        OutDocument.mStatus = PhaseStatusFromString(Status.mNormalized);
        OutDocument.mStatusRaw = Status.mRaw;
    }

    // Title from H1
    for (const HeadingRecord &Heading : Headings)
    {
        if (Heading.mLevel == 1)
        {
            OutDocument.mTitle = Heading.mText;
            break;
        }
    }

    // Build table line set and heading line set
    std::set<int> TableLines;
    for (const MarkdownTableRecord &Table : Tables)
    {
        for (int LineIdx = Table.mStartLine; LineIdx <= Table.mEndLine;
             ++LineIdx)
        {
            TableLines.insert(LineIdx);
        }
    }
    std::set<int> HeadingLines;
    for (const HeadingRecord &Heading : Headings)
    {
        HeadingLines.insert(Heading.mLine);
    }

    // Build sections from H2 headings
    const int TotalLines = static_cast<int>(Lines.size());
    for (size_t HeadingIdx = 0; HeadingIdx < Headings.size(); ++HeadingIdx)
    {
        const HeadingRecord &Heading = Headings[HeadingIdx];
        if (Heading.mLevel < 2)
        {
            continue;
        }

        // Only build top-level H2 sections. H3+ are subsections.
        if (Heading.mLevel > 2)
        {
            continue;
        }

        FSectionContent Section;
        Section.mSectionID = Heading.mSectionId;
        Section.mHeading = Heading.mText;
        Section.mLevel = Heading.mLevel;

        const int SectionEnd =
            FindSectionEndLine(Headings, HeadingIdx, TotalLines);
        const int ContentStart = Heading.mLine + 1;

        // Collect tables within this section
        for (const MarkdownTableRecord &Table : Tables)
        {
            if (Table.mStartLine >= ContentStart &&
                Table.mStartLine <= SectionEnd)
            {
                FStructuredTable StructTable;
                StructTable.mTableID = Table.mTableId;
                StructTable.mSectionID = Section.mSectionID;
                StructTable.mHeaders = Table.mHeaders;
                for (const auto &Row : Table.mRows)
                {
                    std::vector<FTableCell> CellRow;
                    for (const std::string &Cell : Row)
                    {
                        CellRow.push_back(FTableCell{Trim(Cell)});
                    }
                    StructTable.mRows.push_back(std::move(CellRow));
                }
                Section.mTables.push_back(std::move(StructTable));
            }
        }

        // Extract fields from 2-column tables
        for (const FStructuredTable &Table : Section.mTables)
        {
            ExtractFieldsFromTable(Table, Section.mFields);
        }

        // Collect subsection IDs (H3+ within this H2)
        for (size_t SubIdx = HeadingIdx + 1; SubIdx < Headings.size(); ++SubIdx)
        {
            if (Headings[SubIdx].mLevel <= 2)
            {
                break;
            }
            if (Headings[SubIdx].mLevel == 3)
            {
                Section.mSubsectionIDs.push_back(Headings[SubIdx].mSectionId);
            }
        }

        // Extract text content (non-table, non-heading lines)
        Section.mContent = ExtractSectionContent(
            Lines, ContentStart, SectionEnd, TableLines, HeadingLines);

        OutDocument.mSections[Section.mSectionID] = std::move(Section);
    }

    // Preamble: text before first heading
    if (!Headings.empty() && Headings[0].mLine > 0)
    {
        std::string Preamble;
        for (int LineIdx = 0; LineIdx < Headings[0].mLine; ++LineIdx)
        {
            const std::string &Line = Lines[static_cast<size_t>(LineIdx)];
            if (!Preamble.empty() || !Trim(Line).empty())
            {
                if (!Preamble.empty())
                {
                    Preamble += "\n";
                }
                Preamble += Line;
            }
        }
        if (!Preamble.empty())
        {
            FSectionContent PreambleSection;
            PreambleSection.mSectionID = "_preamble";
            PreambleSection.mHeading = "_preamble";
            PreambleSection.mLevel = 0;
            PreambleSection.mContent = Preamble;
            OutDocument.mSections["_preamble"] = std::move(PreambleSection);
        }
    }

    // Extract cross-document references
    ExtractReferences(Lines, OutDocument.mReferences);

    OutDocument.mSchemaVersion = 1;
    return true;
}

// ---------------------------------------------------------------------------
// Single file migration: .md -> .json on disk
// ---------------------------------------------------------------------------

bool TryMigrateFileToJson(const fs::path &InRepoRoot,
                          const std::string &InRelativePath,
                          std::string &OutJsonPath, std::string &OutError)
{
    FDocument Document;
    if (!TryMigrateMarkdownToDocument(InRepoRoot, InRelativePath, Document,
                                      OutError))
    {
        return false;
    }

    const std::string JsonRelPath =
        ReplaceExtension(InRelativePath, ".md", ".json");
    const fs::path JsonAbsPath = InRepoRoot / JsonRelPath;

    if (!TryWriteDocumentJson(Document, JsonAbsPath, OutError))
    {
        return false;
    }

    OutJsonPath = JsonRelPath;
    return true;
}

// ---------------------------------------------------------------------------
// Topic-level migration
// ---------------------------------------------------------------------------

FMigrateTopicResult MigrateTopicToJson(const fs::path &InRepoRoot,
                                       const Inventory &InInventory,
                                       const std::string &InTopicKey)
{
    FMigrateTopicResult Result;
    Result.mTopicKey = InTopicKey;

    // Collect all .md paths for this topic
    std::vector<std::string> Paths;

    for (const DocumentRecord &Plan : InInventory.mPlans)
    {
        if (Plan.mTopicKey == InTopicKey)
        {
            Paths.push_back(Plan.mPath);
        }
    }
    for (const DocumentRecord &Impl : InInventory.mImplementations)
    {
        if (Impl.mTopicKey == InTopicKey)
        {
            Paths.push_back(Impl.mPath);
        }
    }
    for (const DocumentRecord &Playbook : InInventory.mPlaybooks)
    {
        if (Playbook.mTopicKey == InTopicKey)
        {
            Paths.push_back(Playbook.mPath);
        }
    }
    for (const SidecarRecord &Sidecar : InInventory.mSidecars)
    {
        if (Sidecar.mTopicKey == InTopicKey)
        {
            Paths.push_back(Sidecar.mPath);
        }
    }

    for (const std::string &Path : Paths)
    {
        // Skip already-json files
        if (Path.size() > 5 && Path.substr(Path.size() - 5) == ".json")
        {
            continue;
        }

        std::string JsonPath;
        std::string Error;
        if (TryMigrateFileToJson(InRepoRoot, Path, JsonPath, Error))
        {
            Result.mConverted++;
            Result.mConvertedPaths.push_back(JsonPath);
        }
        else
        {
            Result.mFailed++;
            Result.mFailedPaths.push_back(Path);
            Result.mErrors.push_back(Path + ": " + Error);
        }
    }

    return Result;
}

// ---------------------------------------------------------------------------
// Migration status
// ---------------------------------------------------------------------------

FMigrateStatusResult ComputeMigrateStatus(const fs::path &InRepoRoot)
{
    FMigrateStatusResult Result;

    std::error_code Error;
    if (!fs::exists(InRepoRoot, Error) || !fs::is_directory(InRepoRoot, Error))
    {
        return Result;
    }

    for (auto It = fs::recursive_directory_iterator(
             InRepoRoot, fs::directory_options::skip_permission_denied, Error);
         It != fs::recursive_directory_iterator(); It.increment(Error))
    {
        if (Error)
        {
            break;
        }
        if (ShouldSkipRecursionDirectory(It->path()))
        {
            It.disable_recursion_pending();
            continue;
        }
        if (!It->is_regular_file())
        {
            continue;
        }

        const std::string Ext = It->path().extension().string();
        const std::string Name = It->path().filename().string();

        const bool IsPlan = Name.find(".Plan.") != std::string::npos;
        const bool IsImpl = Name.find(".Impl.") != std::string::npos;
        const bool IsPlaybook = Name.find(".Playbook.") != std::string::npos;
        const bool IsChangeLog = Name.find(".ChangeLog.") != std::string::npos;
        const bool IsVerification =
            Name.find(".Verification.") != std::string::npos;

        if (!IsPlan && !IsImpl && !IsPlaybook && !IsChangeLog &&
            !IsVerification)
        {
            continue;
        }

        Result.mTotalDocuments++;
        if (Ext == ".json")
        {
            Result.mJsonDocuments++;
        }
        else if (Ext == ".md")
        {
            Result.mMarkdownDocuments++;
        }
    }

    return Result;
}

} // namespace UniPlan
