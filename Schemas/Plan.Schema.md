# plan_schema

Quick summary: This schema specializes `doc_schema` for plan documents named `<TopicPascalCase>.Plan.md`. It is a shared contract for both human contributors and AI agents.

## doc_type
| Property | Value |
| --- | --- |
| Type | `enum` |
| Required | `yes` |
| Allowed | `plan` |
| Rule | Fixed value for this schema. |

## file_name
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Pattern | `<TopicPascalCase>.Plan.md` |
| Rule | Canonical plan filename format for all new plan docs. |

## topic_key
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Rule | Derived from canonical filename `<TopicPascalCase>.Plan.md`. |

## title
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Rule | First non-empty line is H1; must describe the plan topic clearly. |

## summary
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `yes` |
| Rule | High-level intent and expected outcome of the plan. |

## section_menu
| Property | Value |
| --- | --- |
| Type | `table` |
| Required | `yes` |
| Columns | `Section`, `Description` |
| Rule | Must be immediately after H1 for canonical plans. |

## problem_statement
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `conditional` |
| Rule | Required when the plan needs explicit scope/problem framing before execution strategy details. |

## baseline_audit
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `conditional` |
| Rule | Required when the plan depends on repository inventory, pairing, parity, or migration baselines. |

## multi_platforming
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `conditional` |
| Rule | Required for code-bearing plans. Must declare target platforms, parity scope, and verification strategy. |
| Rule | Must identify shared/reusable logic opportunities: what code can be portable C++ vs platform-specific, which abstraction pattern applies (GenericPlatform HAL, virtual dispatch, compile-time selection), and where shared code lives. |
| Rule | Reference `fie-peerbridge-workflow` for remote verification, `PARITY.md` for policy, and `MultiPlatforming.Plan.md` for shared code extraction patterns. |

## execution_strategy
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `yes` |
| Rule | Defines phases, milestones, or ordered execution lanes. |
| Rule | Must contain a `### implementation_phases` subsection with a phase table. |

## implementation_phases
| Property | Value |
| --- | --- |
| Type | `subsection` |
| Required | `yes` |
| Parent | `execution_strategy` |
| Level | H3 (`###`) |
| Rule | Phase table with minimum columns: `Phase`, `Scope`. Recommended: `Phase`, `Scope`, `Output`. |
| Rule | Phase status is not stored in the plan — it is derived from the playbook's `execution_lanes` Status column (single source of truth). |

## risks_and_mitigations
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `yes` |
| Rule | Records delivery risks and explicit mitigations. |

## acceptance_criteria
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `yes` |
| Rule | Defines done criteria and closure gates. |

## validation_commands
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `conditional` |
| Rule | Required when executable verification commands exist for the plan scope. |
| Rule | Keep this section high-level (`what` to validate and command families); detailed actor-step testing procedures belong to phase playbook `testing` sections. |

## validation_heading_policy
| Property | Value |
| --- | --- |
| Type | `rule_set` |
| Required | `yes` |
| Rule | In plan docs, `validation_commands` is the single canonical validation/verification heading surface. |
| Rule | Do not use alias headings such as `verification_model`, `verification_matrix`, `validation_matrix`, `validation_plan_and_checklist`, `validation_gates_gate_scope_required_outcome`, or `verification` for plan validation scope. |
| Rule | When additional structure is required, express it as tables or lists inside `validation_commands` instead of alias headings. |

## next_actions
| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `yes` |
| Rule | Ordered immediate follow-up actions after current plan state. |

## related_implementation
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `conditional` |
| Pattern | `<TopicPascalCase>.Impl.md` |
| Rule | Repo-relative path to paired implementation doc; if not explicitly declared, derive by topic key and report `missing_implementation` when unresolved. |

## related_playbooks
| Property | Value |
| --- | --- |
| Type | `list<string>` |
| Required | `conditional` |
| Pattern | `<TopicPascalCase>.<PhaseKey>.Playbook.md` |
| Rule | Every active plan phase should map to one dedicated playbook path; list order should follow the plan phase order. |
| Rule | Phase entry gate: before any phase is marked `in_progress`/`started`, the corresponding phase playbook must already exist and include investigation baseline plus actionable execution lanes. |

## phase_execution_gate
| Property | Value |
| --- | --- |
| Type | `lifecycle_rule` |
| Required | `yes` |
| Rule | A plan phase may advance to execution status (`in_progress`, `completed`, `closed`) only when its `<TopicPascalCase>.<PhaseKey>.Playbook.md` exists and is prepared as a detailed execution procedure. |
| Rule | For phases with testable behavior, the bound phase playbook must include a prepared `testing` section covering `human` and `ai_agent` execution steps before phase status can move to `in_progress`/`started`. |
| Rule | Execution outcomes and progress history for a phase must be recorded in the paired `<TopicPascalCase>.Impl.md` tracker and sidecar evidence docs, not as append-only history inside the playbook core body. |

## changelog_ref
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `conditional` |
| Pattern | `<TopicPascalCase>.Plan.ChangeLog.md` |
| Rule | Repo-relative path when change history is detached. |

## verification_ref
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `conditional` |
| Pattern | `<TopicPascalCase>.Plan.Verification.md` |
| Rule | Repo-relative path when verification evidence is detached. |

## canonical_section_order

| Order | Section ID | Requirement |
| --- | --- | --- |
| 1 | `section_menu` | required |
| 2 | `summary` | required |
| 3 | `problem_statement` | conditional (required when plan needs explicit scope framing) |
| 4 | `baseline_audit` | conditional (required when plan depends on repository baselines) |
| 5 | `multi_platforming` | conditional (required for code-bearing plans) |
| 6 | `execution_strategy` | required |
| 7 | `risks_and_mitigations` | required |
| 8 | `acceptance_criteria` | required |
| 9 | `validation_commands` | conditional (required when executable verification commands exist) |
| 10 | `next_actions` | required |
