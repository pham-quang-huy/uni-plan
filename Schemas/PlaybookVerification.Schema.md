# playbook_verification_schema

Quick summary: This schema specializes `doc_schema` for playbook verification sidecar documents named `<TopicPascalCase>.<PhaseKey>.Playbook.Verification.md`. It defines the append-only evidence record contract for phase playbooks, including the conditional `code_delta_vs_proposal` section for phases with `code_reference_snippets`.

## doc_type
| Property | Value |
| --- | --- |
| Type | `enum` |
| Required | `yes` |
| Allowed | `playbook_verification` |
| Rule | Fixed value for this schema. |

## file_name
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Pattern | `<TopicPascalCase>.<PhaseKey>.Playbook.Verification.md` |
| Rule | Canonical playbook verification sidecar filename. Must be co-located with the owning playbook. |

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
| Rule | Must reference the owning playbook document path (repo-relative). |

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

## code_delta_vs_proposal
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `conditional` |
| Trigger | Required when the owning playbook has a `code_reference_snippets` section AND phase execution lanes are `completed`. |
| Rule | Records actual implementation code vs proposed "after" snippets from `code_reference_snippets`. |
| Rule | For each proposed snippet, record: which snippet, actual code block, divergence reason (or `no divergence`). |
| Rule | The owning playbook stays forward-looking — actual post-implementation code goes here only. |

## canonical_section_order

| Order | Section ID | Requirement |
| --- | --- | --- |
| 1 | `section_menu` | required |
| 2 | `linked_document` | required |
| 3 | `verification_entries` | required |
| 4 | `code_delta_vs_proposal` | conditional (required when playbook has code_reference_snippets and phase is completed) |
