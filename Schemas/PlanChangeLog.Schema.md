# plan_changelog_schema

Quick summary: This schema specializes `doc_schema` for plan changelog sidecar documents named `<TopicPascalCase>.Plan.ChangeLog.md`. It defines the append-only change history contract for plan documents.

## doc_type
| Property | Value |
| --- | --- |
| Type | `enum` |
| Required | `yes` |
| Allowed | `plan_changelog` |
| Rule | Fixed value for this schema. |

## file_name
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Pattern | `<TopicPascalCase>.Plan.ChangeLog.md` |
| Rule | Canonical plan changelog sidecar filename. Must be co-located with the owning plan. |

## section_menu
| Property | Value |
| --- | --- |
| Type | `table` |
| Required | `yes` |
| Columns | `Section`, `Description` |
| Rule | Must be immediately after H1. |

## linked_document
| Property | Value |
| --- | --- |
| Type | `table` |
| Required | `yes` |
| Columns | `Item`, `Path` |
| Rule | Must reference the owning plan document path (repo-relative). |

## entries
| Property | Value |
| --- | --- |
| Type | `table` |
| Required | `yes` |
| Columns | `Date`, `Update`, `Evidence` |
| Rule | Append-only chronological change history. Each row records one document edit or scope change. |
| Rule | `Date` uses ISO 8601 format (`YYYY-MM-DD`). |
| Rule | `Evidence` contains repo-relative file paths or artifact references. |

## canonical_section_order

| Order | Section ID | Requirement |
| --- | --- | --- |
| 1 | `section_menu` | required |
| 2 | `linked_document` | required |
| 3 | `entries` | required |
