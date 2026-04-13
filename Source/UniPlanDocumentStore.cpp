#include "UniPlanDocumentStore.h"
#include "UniPlanJsonIO.h"

namespace UniPlan
{

// ---------------------------------------------------------------------------
// TryLoadDocument — JSON-only
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

    OutError = "Unsupported document format (JSON required): " + Ext;
    return false;
}

// ---------------------------------------------------------------------------
// TrySaveDocument — writes JSON
// ---------------------------------------------------------------------------

bool TrySaveDocument(const fs::path &InRepoRoot, const FDocument &InDocument,
                     std::string &OutError)
{
    const std::string &FilePath = InDocument.mIdentity.mFilePath;

    if (FilePath.empty())
    {
        OutError = "Document has empty file path";
        return false;
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
