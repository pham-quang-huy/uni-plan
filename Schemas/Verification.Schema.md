# verification_schema

Quick summary: This schema defines the required sections for any verification sidecar document (`*.Verification.md`). It is the generic verification contract shared across plan, implementation, and playbook verifications.

## doc_type
| Property | Value |
| --- | --- |
| Type | `enum` |
| Required | `yes` |
| Allowed | `verification` |
| Rule | Fixed value for this schema. |

## file_name
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Pattern | `*.Verification.md` |
| Rule | Canonical verification sidecar filename. Must be co-located with the owning document. |

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
| Rule | Must reference the owning document path (repo-relative). |

## entries
| Property | Value |
| --- | --- |
| Type | `table` |
| Required | `yes` |
| Columns | `Date`, `Update`, `Evidence` |
| Rule | Append-only chronological change history. Each row records one document edit or scope change. |
| Rule | `Date` uses ISO 8601 format (`YYYY-MM-DD`). |
| Rule | `Evidence` contains repo-relative file paths or artifact references. |

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
| 3 | `entries` | required |
| 4 | `verification_entries` | required |
