# impl_changelog_schema

Quick summary: This schema specializes `doc_schema` for implementation tracker changelog sidecar documents named `<TopicPascalCase>.Impl.ChangeLog.md`. It defines the append-only change history contract for implementation trackers.

## doc_type
| Property | Value |
| --- | --- |
| Type | `enum` |
| Required | `yes` |
| Allowed | `impl_changelog` |
| Rule | Fixed value for this schema. |

## file_name
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Pattern | `<TopicPascalCase>.Impl.ChangeLog.md` |
| Rule | Canonical implementation changelog sidecar filename. Must be co-located with the owning implementation tracker. |

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
| Rule | Must reference the owning implementation tracker path (repo-relative). |

## entries
| Property | Value |
| --- | --- |
| Type | `table` |
| Required | `yes` |
| Columns | `Date`, `Update`, `Evidence` |
| Rule | Append-only chronological change history. Each row records one tracker edit, milestone sync, or scope change. |
| Rule | `Date` uses ISO 8601 format (`YYYY-MM-DD`). |
| Rule | `Evidence` contains repo-relative file paths or artifact references. |

## canonical_section_order

| Order | Section ID | Requirement |
| --- | --- | --- |
| 1 | `section_menu` | required |
| 2 | `linked_document` | required |
| 3 | `entries` | required |
