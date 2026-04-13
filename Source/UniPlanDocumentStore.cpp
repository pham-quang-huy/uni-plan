#include "UniPlanDocumentStore.h"
#include "UniPlanForwardDecls.h"
#include "UniPlanHelpers.h"
#include "UniPlanJsonIO.h"

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
// Extract sub-document from bundle by fragment
// ---------------------------------------------------------------------------

static bool ExtractDocumentFromBundle(const FTopicBundle &InBundle,
                                      const std::string &InFragment,
                                      FDocument &OutDocument)
{
    if (InFragment == "plan")
    {
        OutDocument = InBundle.mPlan;
        return true;
    }
    if (InFragment == "implementation")
    {
        OutDocument = InBundle.mImplementation;
        return true;
    }

    // playbook:<PhaseKey>
    if (InFragment.substr(0, 9) == "playbook:")
    {
        const std::string PhaseKey = InFragment.substr(9);
        const auto It = InBundle.mPlaybooks.find(PhaseKey);
        if (It != InBundle.mPlaybooks.end())
        {
            OutDocument = It->second;
            return true;
        }
        return false;
    }

    // changelog:<owner>  (owner = "plan", "implementation",
    //                      or phase key like "P1")
    if (InFragment.substr(0, 10) == "changelog:")
    {
        const std::string Owner = InFragment.substr(10);
        const auto It = InBundle.mChangeLogs.find(Owner);
        if (It != InBundle.mChangeLogs.end())
        {
            // Build a synthetic FDocument with entries table
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
            Table.mHeaders = {"Date", "Change", "Files", "Evidence"};
            for (const FChangeLogEntry &Entry : It->second)
            {
                Table.mRows.push_back(
                    {FTableCell{Entry.mDate}, FTableCell{Entry.mChange},
                     FTableCell{Entry.mFiles}, FTableCell{Entry.mEvidence}});
            }
            Section.mTables.push_back(std::move(Table));
            OutDocument.mSections["entries"] = std::move(Section);
            return true;
        }
        return false;
    }

    // verification:<owner>
    if (InFragment.substr(0, 13) == "verification:")
    {
        const std::string Owner = InFragment.substr(13);
        const auto It = InBundle.mVerifications.find(Owner);
        if (It != InBundle.mVerifications.end())
        {
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
            for (const FVerificationEntry &Entry : It->second)
            {
                Table.mRows.push_back(
                    {FTableCell{Entry.mDate}, FTableCell{Entry.mCheck},
                     FTableCell{Entry.mResult}, FTableCell{Entry.mDetail}});
            }
            Section.mTables.push_back(std::move(Table));
            OutDocument.mSections["entries"] = std::move(Section);
            return true;
        }
        return false;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Update sub-document within bundle by fragment
// ---------------------------------------------------------------------------

static bool UpdateDocumentInBundle(FTopicBundle &InOutBundle,
                                   const std::string &InFragment,
                                   const FDocument &InDocument)
{
    if (InFragment == "plan")
    {
        InOutBundle.mPlan = InDocument;
        return true;
    }
    if (InFragment == "implementation")
    {
        InOutBundle.mImplementation = InDocument;
        return true;
    }
    if (InFragment.substr(0, 9) == "playbook:")
    {
        const std::string PhaseKey = InFragment.substr(9);
        InOutBundle.mPlaybooks[PhaseKey] = InDocument;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// TryLoadDocument — fragment-aware bundle loading
// ---------------------------------------------------------------------------

bool TryLoadDocument(const fs::path &InRepoRoot,
                     const std::string &InRelativePath, FDocument &OutDocument,
                     std::string &OutError)
{
    std::string FilePath;
    std::string Fragment;

    if (ParseBundlePath(InRelativePath, FilePath, Fragment))
    {
        // Bundle path with #fragment
        const fs::path AbsPath = InRepoRoot / FilePath;
        FTopicBundle Bundle;
        if (!TryReadTopicBundle(AbsPath, Bundle, OutError))
        {
            return false;
        }
        if (!ExtractDocumentFromBundle(Bundle, Fragment, OutDocument))
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
        return TryReadDocumentJson(AbsPath, OutDocument, OutError);
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
            if (Headings[HI].mLevel != 2)
                continue;

            FSectionContent Section;
            Section.mSectionID = Headings[HI].mSectionId;
            Section.mHeading = Headings[HI].mText;
            Section.mLevel = 2;

            // Find section end
            int SectionEnd = TotalLines - 1;
            for (size_t NI = HI + 1; NI < Headings.size(); ++NI)
            {
                if (Headings[NI].mLevel <= 2)
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
        // Read existing bundle, update sub-doc, write back
        const fs::path AbsPath = InRepoRoot / FilePath;
        FTopicBundle Bundle;

        // Read existing or create empty
        if (fs::exists(AbsPath))
        {
            if (!TryReadTopicBundle(AbsPath, Bundle, OutError))
            {
                return false;
            }
        }

        if (!UpdateDocumentInBundle(Bundle, Fragment, InDocument))
        {
            OutError = "Cannot update fragment: " + Fragment;
            return false;
        }

        // Update bundle-level status from plan
        if (Fragment == "plan")
        {
            Bundle.mStatus = ToString(InDocument.mStatus);
        }

        // Ensure parent directory
        const fs::path ParentDir = AbsPath.parent_path();
        std::error_code DirError;
        fs::create_directories(ParentDir, DirError);
        if (DirError)
        {
            OutError = "Failed to create directory: " + ParentDir.string();
            return false;
        }

        return TryWriteTopicBundle(Bundle, AbsPath, OutError);
    }

    // No fragment — write as single-doc JSON
    const fs::path AbsPath = InRepoRoot / FullPath;

    const fs::path ParentDir = AbsPath.parent_path();
    std::error_code DirError;
    fs::create_directories(ParentDir, DirError);
    if (DirError)
    {
        OutError = "Failed to create directory: " + ParentDir.string();
        return false;
    }

    return TryWriteDocumentJson(InDocument, AbsPath, OutError);
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

} // namespace UniPlan
