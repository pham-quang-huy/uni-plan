# implementation_schema

Quick summary: This schema specializes `doc_schema` for implementation tracker documents named `<TopicPascalCase>.Impl.md`. It is a shared contract for both human contributors and AI agents.

## doc_type
| Property | Value |
| --- | --- |
| Type | `enum` |
| Required | `yes` |
| Allowed | `implementation` |
| Rule | Fixed value for this schema. |

## file_name
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Pattern | `<TopicPascalCase>.Impl.md` |
| Rule | Canonical implementation tracker filename format for all new implementation docs. |

## topic_key
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Rule | Derived from canonical filename `<TopicPascalCase>.Impl.md`. |

## title
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Rule | First non-empty line is H1; must describe the implementation tracker topic clearly. |

## section_menu
| Property | Value |
| --- | --- |
| Type | `table` |
| Required | `yes` |
| Columns | `Section`, `Description` |
| Rule | Must be immediately after H1 for canonical implementation trackers. |

## linked_plan
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `yes` |
| Rule | Must point to paired `<TopicPascalCase>.Plan.md`. |

## progress_summary
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `yes` |
| Rule | High-level progress snapshot: what is done, what remains, current blockers. |

## phase_tracking
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `yes` |
| Rule | Phase execution status table tracking done/remaining work per phase. |

## phase_tracking_table
| Property | Value |
| --- | --- |
| Type | `table` |
| Required | `yes` |
| Columns | `Phase`, `Done`, `Remaining` |
| Rule | One row per plan phase. Phase status is derived from the playbook's `execution_lanes` (single source of truth) — not stored here. |

## change_log
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `yes` |
| Rule | References the implementation changelog sidecar source of truth. |

## next_actions
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `yes` |
| Rule | Ordered immediate follow-up actions after current implementation state. |

## verification
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `yes` |
| Rule | References the implementation verification sidecar source of truth. Uses `verification` heading (not plan-style `validation_commands`). |

## verification_heading_policy
| Property | Value |
| --- | --- |
| Type | `rule_set` |
| Required | `yes` |
| Rule | Implementation trackers use `verification` as the canonical validation heading (PET-7). |
| Rule | Do not use plan-style aliases such as `validation_commands`, `validation_matrix`, or `validation_plan_and_checklist`. |

## canonical_section_order
| Property | Value |
| --- | --- |
| Type | `list<string>` |
| Required | `yes` |
| Order | `section_menu`, `linked_plan`, `progress_summary`, `phase_tracking`, `change_log`, `next_actions`, `verification` |
| Rule | Canonical implementation trackers must keep this order. |

## extension_section_policy
| Rule | Guidance |
| --- | --- |
| Extension allowance | Topic-specific sections are allowed when they capture execution-specific contracts, lane details, or blocker tracking. |
| Common extensions | `linked_playbook`, `risks_and_blockers`, `execution_lanes_current`, `change_log_active_milestones`, `detached_evidence_documents` |
| Naming policy | Extension headings must be snake_case and non-indexed. |
| Placement policy | Extension sections should be placed after core sections and before `verification`. |
| Ownership policy | Extensions must not duplicate ownership of core sections. |

## related_plan
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Pattern | `<TopicPascalCase>.Plan.md` |
| Rule | Repo-relative path to paired plan document; must exist. |

## changelog_ref
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `conditional` |
| Pattern | `<TopicPascalCase>.Impl.ChangeLog.md` |
| Rule | Repo-relative path when change history is detached. |

## verification_ref
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `conditional` |
| Pattern | `<TopicPascalCase>.Impl.Verification.md` |
| Rule | Repo-relative path when verification evidence is detached. |
