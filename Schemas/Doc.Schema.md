# doc_schema

Quick summary: This is the minimal markdown document schema for this repository. It is a shared contract for both human contributors and AI agents.

## playbook_specialization
| Property | Value |
| --- | --- |
| Path | `Schemas/Playbook.Schema.md` |
| Rule | Use the specialized playbook schema for canonical phase-playbook section contracts and ordering. |

## doc_type
| Property | Value |
| --- | --- |
| Type | `enum` |
| Required | `yes` |
| Allowed | `plan`, `implementation`, `playbook`, `changelog`, `verification`, `readme`, `reference`, `diagram`, `adr`, `migration`, `schema`, `other` |
| Rule | Selects the document parser profile. |

## file_name
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Pattern (plan/implementation core docs) | `<TopicPascalCase>.<DocKind>.md` |
| Pattern (phase-playbook core docs) | `<TopicPascalCase>.<PhaseKey>.Playbook.md` |
| Pattern (plan/impl sidecars) | `<TopicPascalCase>.<OwnerDocKind>.<DocKind>.md` |
| Pattern (phase-playbook sidecars) | `<TopicPascalCase>.<PhaseKey>.Playbook.<DocKind>.md` |
| `TopicPascalCase` rule | UpperCamelCase ASCII alphanumeric token (for example `DocToolCliSchema`). |
| `PhaseKey` rule | Plan phase token (for example `P0`, `P1`, `P4W1`) using ASCII alphanumeric/underscore. |
| Allowed `OwnerDocKind` | `Plan`, `Impl`, `Playbook` |
| Allowed `DocKind` | `Plan`, `Impl`, `Playbook`, `ChangeLog`, `Verification`, `Readme`, `Reference`, `Diagram`, `ADR`, `Migration`, `Schema`, `Other` |
| Rule | For non-sidecar docs, `DocKind` must match `doc_type` (`plan`->`Plan`, `implementation`->`Impl`, `playbook`->`Playbook`). |
| Rule | For sidecars, canonical names must include owning kind (`<OwnerDocKind>`) before `ChangeLog`/`Verification` to avoid duplicate stems across artifact classes. |
| Rule | For `playbook` docs, canonical names must include `PhaseKey` so each active plan phase has a dedicated playbook document. |

## topic_key
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `conditional` |
| Rule | For workflow docs, derive from canonical filename stem (`<TopicPascalCase>`). |

## phase_key
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `conditional` |
| Rule | Required for `playbook`, `playbook` sidecar (`changelog`/`verification`) docs; maps one playbook doc to one plan phase token (for example `P0`). |
| Rule | Phase entry gate: the phase playbook must exist and be prepared before the matching phase is marked `in_progress`/`started` in plan or implementation docs. |

## title
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Rule | First non-empty line must be a single H1 (`# ...`). |

## status
| Property | Value |
| --- | --- |
| Type | `enum` |
| Required | `conditional` |
| Allowed | not_started, `in_progress`, `completed`, `closed`, `blocked`, `canceled`, `unknown` |
| Rule | Normalized lifecycle status for workflow docs when status is represented. |

## status_normalization
| Property | Value |
| --- | --- |
| Type | `mapping` |
| Required | `conditional` |
| Rule | Convert raw status text to canonical `status` enum for workflow docs. |

| Raw Value Pattern | Canonical Status |
| --- | --- |
| `not started`, `planned`, `pending` | not_started |
| `in progress`, `partial`, `started`, `investigated`, `deferred` | in_progress |
| `done`, `completed`, `parity-ready`, `verified` | completed |
| closed, `closure completed` | closed |
| blocked | `blocked` |
| canceled, `out of scope`, `removed`, `archive`, `delete` | canceled |
| anything else | `unknown` |

## summary
| Property | Value |
| --- | --- |
| Type | `string` or `section` |
| Required | `conditional` |
| Rule | Workflow docs should provide a `summary` section; short H1-adjacent summaries are allowed for lightweight/schema docs. |

## linked_document
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `conditional` |
| Rule | Required for `changelog` and `verification` docs; repo-relative path to owning core document. |
| Rule | `changelog` sidecars track document-change history for the owning artifact, not full execution result logs. |
| Rule | `verification` sidecars track command/evidence outcomes for gates and checks. |

## section_menu
| Property | Value |
| --- | --- |
| Type | `table` |
| Required | `conditional` |
| Columns | `Section`, `Description` |
| Rule | Required for workflow docs (`plan`, `implementation`, `playbook`, `changelog`, `verification`); optional for lightweight docs. |
| Placement | For workflow docs, place immediately after H1. |

## verification_heading_policy
| Property | Value |
| --- | --- |
| Type | `rule_set` |
| Required | `conditional` |
| Rule | For non-plan docs that include verification evidence, use `verification` as the canonical heading ID. |
| Rule | Avoid alias/indexed literals such as `Verification`, `Verification (Active Gate)`, `4. Validation`, or `6. Verification`; headings must stay snake_case and non-indexed. |
| Rule | Keep active-gate semantics in table rows/columns inside `verification`, not in heading aliases. |

## sections
| Property | Value |
| --- | --- |
| Type | `list<object>` |
| Required | `yes` |
| Object shape | `{ section_level, heading_text, section_id, body_blocks }` |
| Rule | `section_level` must follow markdown heading hierarchy without skipping levels. |
| Rule | `section_id` normalization: lowercase, strip numeric prefix, spaces/hyphens to `_`, remove punctuation except `_`, collapse repeated `_`, trim `_`. |

## links
| Property | Value |
| --- | --- |
| Type | `list<object>` |
| Required | `no` |
| Object shape | `{ label, target, kind }` |
| Allowed `kind` | `doc_path`, `section_anchor`, `external_url`, `asset_path` |
| Rule | In-repo targets and section anchors must resolve. |

## changelog_ref
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `conditional` |
| Pattern | `<TopicPascalCase>.<OwnerDocKind>.ChangeLog.md` or `<TopicPascalCase>.<PhaseKey>.Playbook.ChangeLog.md` |
| Rule | Repo-relative path to detached change history sidecar when used by a core workflow doc. |

## verification_ref
| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `conditional` |
| Pattern | `<TopicPascalCase>.<OwnerDocKind>.Verification.md` or `<TopicPascalCase>.<PhaseKey>.Playbook.Verification.md` |
| Rule | Repo-relative path to detached verification sidecar when used by a core workflow doc. |

## related_docs
| Property | Value |
| --- | --- |
| Type | `list<string>` |
| Required | `no` |
| Rule | Repo-relative paths to directly related documents. |

## workflow_lifecycle_roles
| Artifact | Lifecycle Role |
| --- | --- |
| `<TopicPascalCase>.Plan.md` | Strategy and phase boundary contract (`what` and `why`). |
| `<TopicPascalCase>.<PhaseKey>.Playbook.md` | Pre-execution phase procedure (`how`) with investigation baseline, execution lanes, owners, dependencies, and exit criteria. |
| `<TopicPascalCase>.Impl.md` | Execution status rollup and delivered outcomes (`done`, `remaining`, blockers, next actions). |
| `<TopicPascalCase>.*.ChangeLog.md` | Append-only document-change history for the owning artifact. |
| `<TopicPascalCase>.*.Verification.md` | Append-only verification evidence history (commands/checks/results). |
