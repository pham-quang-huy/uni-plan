#include "UniPlanMutation.h"
#include "UniPlanDocumentStore.h"
#include "UniPlanHelpers.h"

#include <cstdint>
#include <sstream>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Content hash — FNV-1a 64-bit
// ---------------------------------------------------------------------------

std::string ComputeContentHash(const std::string &InContent)
{
    uint64_t Hash = 14695981039346656037ULL;
    for (char Character : InContent)
    {
        Hash ^= static_cast<uint64_t>(static_cast<unsigned char>(Character));
        Hash *= 1099511628211ULL;
    }
    std::ostringstream Stream;
    Stream << std::hex << Hash;
    return Stream.str();
}

// ---------------------------------------------------------------------------
// Phase status state machine
// ---------------------------------------------------------------------------

bool ValidatePhaseTransition(EPhaseStatus InCurrentStatus,
                             EPhaseStatus InTargetStatus,
                             std::vector<EPhaseStatus> &OutAllowedTargets)
{
    OutAllowedTargets.clear();

    switch (InCurrentStatus)
    {
    case EPhaseStatus::NotStarted:
        OutAllowedTargets = {EPhaseStatus::InProgress, EPhaseStatus::Blocked,
                             EPhaseStatus::Canceled};
        break;
    case EPhaseStatus::InProgress:
        OutAllowedTargets = {EPhaseStatus::Completed, EPhaseStatus::Blocked,
                             EPhaseStatus::Canceled};
        break;
    case EPhaseStatus::Blocked:
        OutAllowedTargets = {EPhaseStatus::InProgress, EPhaseStatus::Canceled};
        break;
    case EPhaseStatus::Completed:
        OutAllowedTargets = {EPhaseStatus::Closed};
        break;
    case EPhaseStatus::Closed:
    case EPhaseStatus::Canceled:
    case EPhaseStatus::Unknown:
        OutAllowedTargets = {};
        break;
    }

    for (EPhaseStatus Allowed : OutAllowedTargets)
    {
        if (Allowed == InTargetStatus)
        {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Mutation helpers
// ---------------------------------------------------------------------------

static bool LoadAndValidateSection(const fs::path &InRepoRoot,
                                   const std::string &InRelativePath,
                                   const std::string &InSectionID,
                                   FDocument &OutDocument,
                                   std::string &OutError)
{
    if (!TryLoadDocument(InRepoRoot, InRelativePath, OutDocument, OutError))
    {
        return false;
    }

    if (OutDocument.mSections.find(InSectionID) == OutDocument.mSections.end())
    {
        OutError = "Section not found: " + InSectionID;
        return false;
    }

    return true;
}

static bool SaveWithHash(const fs::path &InRepoRoot, FDocument &InOutDocument,
                         FMutationResult &OutResult, std::string &OutError)
{
    // Ensure identity has .json path
    std::string &FilePath = InOutDocument.mIdentity.mFilePath;
    if (FilePath.size() > 3 && FilePath.substr(FilePath.size() - 3) == ".md")
    {
        FilePath = FilePath.substr(0, FilePath.size() - 3) + ".json";
    }

    InOutDocument.mIdentity.mFormat = EDocumentFormat::JSON;
    OutResult.mTargetPath = FilePath;

    if (!TrySaveDocument(InRepoRoot, InOutDocument, OutError))
    {
        return false;
    }

    OutResult.mType = "update";
    OutResult.mTargetEntity = InOutDocument.mIdentity.mTopicKey;
    return true;
}

// ---------------------------------------------------------------------------
// Mutation implementations
// ---------------------------------------------------------------------------

bool TryUpdateSectionContent(const fs::path &InRepoRoot,
                             const std::string &InRelativePath,
                             const std::string &InSectionID,
                             const std::string &InNewContent,
                             FMutationResult &OutResult, std::string &OutError)
{
    FDocument Document;
    if (!LoadAndValidateSection(InRepoRoot, InRelativePath, InSectionID,
                                Document, OutError))
    {
        return false;
    }

    FSectionContent &Section = Document.mSections[InSectionID];
    const std::string OldContent = Section.mContent;
    Section.mContent = InNewContent;

    FMutationChange Change;
    Change.mField = InSectionID + ".content";
    Change.mOldValue =
        OldContent.substr(0, 80) + (OldContent.size() > 80 ? "..." : "");
    Change.mNewValue =
        InNewContent.substr(0, 80) + (InNewContent.size() > 80 ? "..." : "");
    OutResult.mChanges.push_back(std::move(Change));

    return SaveWithHash(InRepoRoot, Document, OutResult, OutError);
}

bool TryUpdateSectionField(const fs::path &InRepoRoot,
                           const std::string &InRelativePath,
                           const std::string &InSectionID,
                           const std::string &InFieldName,
                           const std::string &InFieldValue,
                           FMutationResult &OutResult, std::string &OutError)
{
    FDocument Document;
    if (!LoadAndValidateSection(InRepoRoot, InRelativePath, InSectionID,
                                Document, OutError))
    {
        return false;
    }

    FSectionContent &Section = Document.mSections[InSectionID];
    const std::string OldValue = Section.mFields[InFieldName];
    Section.mFields[InFieldName] = InFieldValue;

    FMutationChange Change;
    Change.mField = InSectionID + ".fields." + InFieldName;
    Change.mOldValue = OldValue;
    Change.mNewValue = InFieldValue;
    OutResult.mChanges.push_back(std::move(Change));

    return SaveWithHash(InRepoRoot, Document, OutResult, OutError);
}

bool TryAppendTableRow(const fs::path &InRepoRoot,
                       const std::string &InRelativePath,
                       const std::string &InSectionID, int InTableIndex,
                       const std::vector<std::string> &InCellValues,
                       FMutationResult &OutResult, std::string &OutError)
{
    FDocument Document;
    if (!LoadAndValidateSection(InRepoRoot, InRelativePath, InSectionID,
                                Document, OutError))
    {
        return false;
    }

    FSectionContent &Section = Document.mSections[InSectionID];
    if (InTableIndex < 0 ||
        InTableIndex >= static_cast<int>(Section.mTables.size()))
    {
        OutError = "Table index out of range: " + std::to_string(InTableIndex);
        return false;
    }

    FStructuredTable &Table =
        Section.mTables[static_cast<size_t>(InTableIndex)];
    std::vector<FTableCell> NewRow;
    for (const std::string &Value : InCellValues)
    {
        NewRow.push_back(FTableCell{Value});
    }
    Table.mRows.push_back(std::move(NewRow));

    FMutationChange Change;
    Change.mField =
        InSectionID + ".tables[" + std::to_string(InTableIndex) + "].rows";
    Change.mOldValue =
        std::to_string(static_cast<int>(Table.mRows.size()) - 1) + " rows";
    Change.mNewValue =
        std::to_string(static_cast<int>(Table.mRows.size())) + " rows";
    OutResult.mChanges.push_back(std::move(Change));

    return SaveWithHash(InRepoRoot, Document, OutResult, OutError);
}

bool TryUpdateTableCell(const fs::path &InRepoRoot,
                        const std::string &InRelativePath,
                        const std::string &InSectionID, int InTableIndex,
                        int InRowIndex, const std::string &InColumnName,
                        const std::string &InNewValue,
                        FMutationResult &OutResult, std::string &OutError)
{
    FDocument Document;
    if (!LoadAndValidateSection(InRepoRoot, InRelativePath, InSectionID,
                                Document, OutError))
    {
        return false;
    }

    FSectionContent &Section = Document.mSections[InSectionID];
    if (InTableIndex < 0 ||
        InTableIndex >= static_cast<int>(Section.mTables.size()))
    {
        OutError = "Table index out of range";
        return false;
    }

    FStructuredTable &Table =
        Section.mTables[static_cast<size_t>(InTableIndex)];
    if (InRowIndex < 0 || InRowIndex >= static_cast<int>(Table.mRows.size()))
    {
        OutError = "Row index out of range";
        return false;
    }

    // Find column index
    int ColIndex = -1;
    for (int Col = 0; Col < static_cast<int>(Table.mHeaders.size()); ++Col)
    {
        if (Table.mHeaders[static_cast<size_t>(Col)] == InColumnName)
        {
            ColIndex = Col;
            break;
        }
    }
    if (ColIndex < 0)
    {
        OutError = "Column not found: " + InColumnName;
        return false;
    }

    std::vector<FTableCell> &Row = Table.mRows[static_cast<size_t>(InRowIndex)];
    if (ColIndex >= static_cast<int>(Row.size()))
    {
        OutError = "Column index out of range in row";
        return false;
    }

    const std::string OldValue = Row[static_cast<size_t>(ColIndex)].mValue;
    Row[static_cast<size_t>(ColIndex)].mValue = InNewValue;

    FMutationChange Change;
    Change.mField = InSectionID + ".tables[" + std::to_string(InTableIndex) +
                    "].rows[" + std::to_string(InRowIndex) + "]." +
                    InColumnName;
    Change.mOldValue = OldValue;
    Change.mNewValue = InNewValue;
    OutResult.mChanges.push_back(std::move(Change));

    return SaveWithHash(InRepoRoot, Document, OutResult, OutError);
}

} // namespace UniPlan
