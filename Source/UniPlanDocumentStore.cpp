#include "UniPlanDocumentStore.h"
#include "UniPlanJsonIO.h"
#include "UniPlanMigrate.h"

#include <algorithm>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// TryLoadDocument — format-aware dispatch
// ---------------------------------------------------------------------------

bool TryLoadDocument(const fs::path &InRepoRoot,
                     const std::string &InRelativePath, FDocument &OutDocument,
                     std::string &OutError)
{
    const fs::path AbsPath = InRepoRoot / InRelativePath;
    const std::string Ext = AbsPath.extension().string();

    if (Ext == ".json")
    {
        return TryReadDocumentJson(AbsPath, OutDocument, OutError);
    }

    if (Ext == ".md")
    {
        return TryMigrateMarkdownToDocument(InRepoRoot, InRelativePath,
                                            OutDocument, OutError);
    }

    OutError = "Unsupported document format: " + Ext;
    return false;
}

// ---------------------------------------------------------------------------
// TrySaveDocument — always writes JSON (decision #2)
// ---------------------------------------------------------------------------

bool TrySaveDocument(const fs::path &InRepoRoot, const FDocument &InDocument,
                     std::string &OutError)
{
    std::string FilePath = InDocument.mIdentity.mFilePath;

    if (FilePath.empty())
    {
        OutError = "Document has empty file path";
        return false;
    }

    // Ensure .json extension
    if (FilePath.size() > 3 && FilePath.substr(FilePath.size() - 3) == ".md")
    {
        FilePath = FilePath.substr(0, FilePath.size() - 3) + ".json";
    }

    const fs::path AbsPath = InRepoRoot / FilePath;

    // Ensure parent directory exists
    const fs::path ParentDir = AbsPath.parent_path();
    std::error_code DirError;
    fs::create_directories(ParentDir, DirError);
    if (DirError)
    {
        OutError = "Failed to create directory: " + ParentDir.string() + ": " +
                   DirError.message();
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
    // Search top-level tables
    for (const FStructuredTable &Table : InDocument.mTables)
    {
        if (Table.mTableID == InTableID)
        {
            return Table;
        }
    }

    // Search tables within sections
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

    // Not found
    FStructuredTable Empty;
    Empty.mTableID = -1;
    return Empty;
}

} // namespace UniPlan
