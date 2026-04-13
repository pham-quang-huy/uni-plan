#include "UniPlanDocumentStore.h"
#include "UniPlanJsonIO.h"

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

    // No fragment — try as single-doc JSON (legacy compat)
    const fs::path AbsPath = InRepoRoot / InRelativePath;
    return TryReadDocumentJson(AbsPath, OutDocument, OutError);
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
