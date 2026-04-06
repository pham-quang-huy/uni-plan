# impl_verification_schema

Quick summary: This schema specializes `doc_schema` for implementation tracker verification sidecar documents named `<TopicPascalCase>.Impl.Verification.md`. It defines the append-only evidence record contract for implementation trackers.

## doc_type
| Property | Value |
| --- | --- |
| Type | `enum` |
| Required | `yes` |
| Allowed | `impl_verification` |
| Rule | Fixed value for this schema. |

## file_name
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Pattern | `<TopicPascalCase>.Impl.Verification.md` |
| Rule | Canonical implementation verification sidecar filename. Must be co-located with the owning implementation tracker. |

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

## verification_entries
| Property | Value |
| --- | --- |
| Type | `table` |
| Required | `yes` |
| Columns | `Date`, `Command`, `Result` |
| Rule | Append-only verification evidence. Each row records one validation command and its outcome. |
| Rule | `Date` uses ISO 8601 format (`YYYY-MM-DD`). |
| Rule | `Command` contains the actual shell command or check description. |
| Rule | `Result` contains pass/fail outcome with narrative evidence. |

## canonical_section_order

| Order | Section ID | Requirement |
| --- | --- | --- |
| 1 | `section_menu` | required |
| 2 | `linked_document` | required |
| 3 | `verification_entries` | required |
