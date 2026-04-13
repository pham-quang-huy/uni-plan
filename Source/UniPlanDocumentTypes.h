#pragma once

#include "UniPlanEnums.h"

#include <map>
#include <string>
#include <vector>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Core document model types for the JSON-first architecture.
// These types represent the structured document format that replaces
// raw markdown parsing. Used by the document store, migration engine,
// and mutation commands.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// FDocumentIdentity — uniquely identifies any governance document.
// ---------------------------------------------------------------------------

struct FDocumentIdentity
{
    EDocumentType mType = EDocumentType::Plan;
    EDocumentFormat mFormat = EDocumentFormat::Markdown;
    std::string mTopicKey;
    std::string mPhaseKey;
    std::string mFilePath;
    std::string mContentHash;
};

// ---------------------------------------------------------------------------
// FTableCell — one cell in a structured table.
// ---------------------------------------------------------------------------

struct FTableCell
{
    std::string mValue;
};

// ---------------------------------------------------------------------------
// FStructuredTable — a typed table within a document section.
// ---------------------------------------------------------------------------

struct FStructuredTable
{
    int mTableID = 0;
    std::string mSectionID;
    std::vector<std::string> mHeaders;
    std::vector<std::vector<FTableCell>> mRows;
};

// ---------------------------------------------------------------------------
// FSectionContent — one section in a document.
// ---------------------------------------------------------------------------

struct FSectionContent
{
    std::string mSectionID;
    std::string mHeading;
    int mLevel = 2;
    std::string mContent;
    std::map<std::string, std::string> mFields;
    std::vector<FStructuredTable> mTables;
    std::vector<std::string> mSubsectionIDs;
};

// ---------------------------------------------------------------------------
// FDocumentReference — cross-document link.
// ---------------------------------------------------------------------------

struct FDocumentReference
{
    std::string mTargetPath;
    std::string mRelationType;
};

// ---------------------------------------------------------------------------
// FTag — entity tag for cross-referencing.
// ---------------------------------------------------------------------------

struct FTag
{
    std::string mValue;
};

// ---------------------------------------------------------------------------
// FDocument — universal document container for JSON-first storage.
// Holds the complete structured representation of any governance
// document (plan, playbook, implementation, changelog, verification).
// ---------------------------------------------------------------------------

struct FDocument
{
    FDocumentIdentity mIdentity;
    std::string mTitle;
    EPhaseStatus mStatus = EPhaseStatus::Unknown;
    std::string mStatusRaw;
    std::string mGeneratedUTC;
    int mSchemaVersion = 1;
    std::map<std::string, FSectionContent> mSections;
    std::vector<FStructuredTable> mTables;
    std::vector<FDocumentReference> mReferences;
    std::vector<FTag> mTags;
};

} // namespace UniPlan
