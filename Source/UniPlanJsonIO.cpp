#include "UniPlanJsonIO.h"
#include "UniPlanJson.h"

#include <fstream>
#include <sstream>

namespace UniPlan
{

// ---------------------------------------------------------------------------
// Serialization helpers: C++ structs -> JSON
// ---------------------------------------------------------------------------

static JsonValue SerializeTableCell(const FTableCell &InCell)
{
    return InCell.mValue;
}

static JsonValue SerializeTable(const FStructuredTable &InTable)
{
    JsonValue Table = JsonValue::object();
    Table["table_id"] = InTable.mTableID;
    Table["section_id"] = InTable.mSectionID;

    JsonValue Headers = JsonValue::array();
    for (const std::string &Header : InTable.mHeaders)
    {
        Headers.push_back(Header);
    }
    Table["headers"] = std::move(Headers);

    JsonValue Rows = JsonValue::array();
    for (const std::vector<FTableCell> &Row : InTable.mRows)
    {
        JsonValue JsonRow = JsonValue::array();
        for (const FTableCell &Cell : Row)
        {
            JsonRow.push_back(SerializeTableCell(Cell));
        }
        Rows.push_back(std::move(JsonRow));
    }
    Table["rows"] = std::move(Rows);

    return Table;
}

static JsonValue SerializeSection(const FSectionContent &InSection)
{
    JsonValue Section = JsonValue::object();
    Section["heading"] = InSection.mHeading;
    Section["level"] = InSection.mLevel;

    if (!InSection.mContent.empty())
    {
        Section["content"] = InSection.mContent;
    }

    if (!InSection.mFields.empty())
    {
        JsonValue Fields = JsonValue::object();
        for (const auto &Pair : InSection.mFields)
        {
            Fields[Pair.first] = Pair.second;
        }
        Section["fields"] = std::move(Fields);
    }

    if (!InSection.mTables.empty())
    {
        JsonValue Tables = JsonValue::array();
        for (const FStructuredTable &Table : InSection.mTables)
        {
            Tables.push_back(SerializeTable(Table));
        }
        Section["tables"] = std::move(Tables);
    }

    if (!InSection.mSubsectionIDs.empty())
    {
        JsonValue SubsectionIDs = JsonValue::array();
        for (const std::string &ID : InSection.mSubsectionIDs)
        {
            SubsectionIDs.push_back(ID);
        }
        Section["subsection_ids"] = std::move(SubsectionIDs);
    }

    return Section;
}

static JsonValue SerializeReference(const FDocumentReference &InRef)
{
    JsonValue Ref = JsonValue::object();
    Ref["target_path"] = InRef.mTargetPath;
    Ref["relation_type"] = InRef.mRelationType;
    return Ref;
}

static JsonValue SerializeDocument(const FDocument &InDocument)
{
    JsonValue Root = JsonValue::object();

    // Envelope
    const std::string TypeStr = ToString(InDocument.mIdentity.mType);
    Root["$schema"] = "uni-plan://" + TypeStr + "/v1";
    Root["schema_version"] = InDocument.mSchemaVersion;
    Root["doc_type"] = TypeStr;
    Root["topic_key"] = InDocument.mIdentity.mTopicKey;
    Root["phase_key"] = InDocument.mIdentity.mPhaseKey;
    Root["file_path"] = InDocument.mIdentity.mFilePath;
    Root["title"] = InDocument.mTitle;
    Root["status"] = ToString(InDocument.mStatus);
    Root["status_raw"] = InDocument.mStatusRaw;
    Root["content_hash"] = InDocument.mIdentity.mContentHash;

    // Tags
    JsonValue Tags = JsonValue::array();
    for (const FTag &Tag : InDocument.mTags)
    {
        Tags.push_back(Tag.mValue);
    }
    Root["tags"] = std::move(Tags);

    // Sections
    JsonValue Sections = JsonValue::object();
    for (const auto &Pair : InDocument.mSections)
    {
        Sections[Pair.first] = SerializeSection(Pair.second);
    }
    Root["sections"] = std::move(Sections);

    // Top-level tables
    JsonValue Tables = JsonValue::array();
    for (const FStructuredTable &Table : InDocument.mTables)
    {
        Tables.push_back(SerializeTable(Table));
    }
    Root["tables"] = std::move(Tables);

    // References
    JsonValue References = JsonValue::array();
    for (const FDocumentReference &Ref : InDocument.mReferences)
    {
        References.push_back(SerializeReference(Ref));
    }
    Root["references"] = std::move(References);

    return Root;
}

// ---------------------------------------------------------------------------
// Deserialization helpers: JSON -> C++ structs
// ---------------------------------------------------------------------------

static std::string GetString(const JsonValue &InJson, const std::string &InKey,
                             const std::string &InDefault = "")
{
    if (InJson.contains(InKey) && InJson[InKey].is_string())
    {
        return InJson[InKey].get<std::string>();
    }
    return InDefault;
}

static int GetInt(const JsonValue &InJson, const std::string &InKey,
                  int InDefault = 0)
{
    if (InJson.contains(InKey) && InJson[InKey].is_number_integer())
    {
        return InJson[InKey].get<int>();
    }
    return InDefault;
}

static FStructuredTable DeserializeTable(const JsonValue &InJson)
{
    FStructuredTable Table;
    Table.mTableID = GetInt(InJson, "table_id");
    Table.mSectionID = GetString(InJson, "section_id");

    if (InJson.contains("headers") && InJson["headers"].is_array())
    {
        for (const auto &Header : InJson["headers"])
        {
            if (Header.is_string())
            {
                Table.mHeaders.push_back(Header.get<std::string>());
            }
        }
    }

    if (InJson.contains("rows") && InJson["rows"].is_array())
    {
        for (const auto &Row : InJson["rows"])
        {
            if (!Row.is_array())
            {
                continue;
            }
            std::vector<FTableCell> CellRow;
            for (const auto &Cell : Row)
            {
                FTableCell TableCell;
                if (Cell.is_string())
                {
                    TableCell.mValue = Cell.get<std::string>();
                }
                else if (Cell.is_object() && Cell.contains("value"))
                {
                    TableCell.mValue = GetString(Cell, "value");
                }
                CellRow.push_back(std::move(TableCell));
            }
            Table.mRows.push_back(std::move(CellRow));
        }
    }

    return Table;
}

static FSectionContent DeserializeSection(const std::string &InID,
                                          const JsonValue &InJson)
{
    FSectionContent Section;
    Section.mSectionID = InID;
    Section.mHeading = GetString(InJson, "heading", InID);
    Section.mLevel = GetInt(InJson, "level", 2);
    Section.mContent = GetString(InJson, "content");

    if (InJson.contains("fields") && InJson["fields"].is_object())
    {
        for (const auto &Item : InJson["fields"].items())
        {
            if (Item.value().is_string())
            {
                Section.mFields[Item.key()] = Item.value().get<std::string>();
            }
        }
    }

    if (InJson.contains("tables") && InJson["tables"].is_array())
    {
        for (const auto &TableJson : InJson["tables"])
        {
            if (TableJson.is_object())
            {
                Section.mTables.push_back(DeserializeTable(TableJson));
            }
        }
    }

    if (InJson.contains("subsection_ids") &&
        InJson["subsection_ids"].is_array())
    {
        for (const auto &ID : InJson["subsection_ids"])
        {
            if (ID.is_string())
            {
                Section.mSubsectionIDs.push_back(ID.get<std::string>());
            }
        }
    }

    return Section;
}

static FDocumentReference DeserializeReference(const JsonValue &InJson)
{
    FDocumentReference Ref;
    Ref.mTargetPath = GetString(InJson, "target_path");
    Ref.mRelationType = GetString(InJson, "relation_type");
    return Ref;
}

static bool DeserializeDocument(const JsonValue &InRoot, FDocument &OutDocument,
                                std::string &OutError)
{
    // Validate required envelope fields
    if (!InRoot.contains("doc_type") || !InRoot["doc_type"].is_string())
    {
        OutError = "Missing required field 'doc_type'";
        return false;
    }
    if (!InRoot.contains("topic_key") || !InRoot["topic_key"].is_string())
    {
        OutError = "Missing required field 'topic_key'";
        return false;
    }

    // Identity
    const std::string DocTypeStr = GetString(InRoot, "doc_type");
    OutDocument.mIdentity.mType = DocumentTypeFromString(DocTypeStr);
    OutDocument.mIdentity.mFormat = EDocumentFormat::JSON;
    OutDocument.mIdentity.mTopicKey = GetString(InRoot, "topic_key");
    OutDocument.mIdentity.mPhaseKey = GetString(InRoot, "phase_key");
    OutDocument.mIdentity.mFilePath = GetString(InRoot, "file_path");
    OutDocument.mIdentity.mContentHash = GetString(InRoot, "content_hash");

    // Envelope metadata
    OutDocument.mTitle = GetString(InRoot, "title");
    OutDocument.mStatus =
        PhaseStatusFromString(GetString(InRoot, "status", "unknown"));
    OutDocument.mStatusRaw = GetString(InRoot, "status_raw");
    OutDocument.mGeneratedUTC = GetString(InRoot, "generated_utc");
    OutDocument.mSchemaVersion = GetInt(InRoot, "schema_version", 1);

    // Tags
    if (InRoot.contains("tags") && InRoot["tags"].is_array())
    {
        for (const auto &TagJson : InRoot["tags"])
        {
            if (TagJson.is_string())
            {
                FTag Tag;
                Tag.mValue = TagJson.get<std::string>();
                OutDocument.mTags.push_back(std::move(Tag));
            }
        }
    }

    // Sections
    if (InRoot.contains("sections") && InRoot["sections"].is_object())
    {
        for (const auto &Item : InRoot["sections"].items())
        {
            if (Item.value().is_object())
            {
                OutDocument.mSections[Item.key()] =
                    DeserializeSection(Item.key(), Item.value());
            }
        }
    }

    // Top-level tables
    if (InRoot.contains("tables") && InRoot["tables"].is_array())
    {
        for (const auto &TableJson : InRoot["tables"])
        {
            if (TableJson.is_object())
            {
                OutDocument.mTables.push_back(DeserializeTable(TableJson));
            }
        }
    }

    // References
    if (InRoot.contains("references") && InRoot["references"].is_array())
    {
        for (const auto &RefJson : InRoot["references"])
        {
            if (RefJson.is_object())
            {
                OutDocument.mReferences.push_back(
                    DeserializeReference(RefJson));
            }
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool TryWriteDocumentJson(const FDocument &InDocument, const fs::path &InPath,
                          std::string &OutError)
{
    try
    {
        const JsonValue Root = SerializeDocument(InDocument);
        const std::string Content = Root.dump(2);

        std::ofstream Stream(InPath, std::ios::out | std::ios::trunc);
        if (!Stream.is_open())
        {
            OutError = "Failed to open file for writing: " + InPath.string();
            return false;
        }
        Stream << Content << "\n";
        Stream.close();

        if (Stream.fail())
        {
            OutError = "Write failed: " + InPath.string();
            return false;
        }
        return true;
    }
    catch (const std::exception &Ex)
    {
        OutError = std::string("JSON write error: ") + Ex.what();
        return false;
    }
}

bool TryReadDocumentJson(const fs::path &InPath, FDocument &OutDocument,
                         std::string &OutError)
{
    try
    {
        std::ifstream Stream(InPath);
        if (!Stream.is_open())
        {
            OutError = "Failed to open file: " + InPath.string();
            return false;
        }

        std::ostringstream Buffer;
        Buffer << Stream.rdbuf();
        const std::string Content = Buffer.str();

        if (Content.empty())
        {
            OutError = "File is empty: " + InPath.string();
            return false;
        }

        const JsonValue Root = JsonValue::parse(Content);

        if (!Root.is_object())
        {
            OutError = "Root is not a JSON object: " + InPath.string();
            return false;
        }

        return DeserializeDocument(Root, OutDocument, OutError);
    }
    catch (const JsonValue::parse_error &Ex)
    {
        OutError = std::string("JSON parse error: ") + Ex.what();
        return false;
    }
    catch (const std::exception &Ex)
    {
        OutError = std::string("JSON read error: ") + Ex.what();
        return false;
    }
}

} // namespace UniPlan
