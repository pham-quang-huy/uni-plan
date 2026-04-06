# playbook_schema

## section_menu

| Section | Description |
| --- | --- |
| [summary](#summary) | Scope and intent of the canonical playbook schema. |
| [investigation_baseline_2026_03_03](#investigation_baseline_2026_03_03) | Doc CLI evidence captured from current repository playbooks. |
| [doc_type](#doc_type) | Canonical `doc_type` contract for playbooks. |
| [file_name](#file_name) | Canonical naming pattern for playbook core docs and sidecars. |
| [topic_key](#topic_key) | Topic identity rule. |
| [phase_key](#phase_key) | Phase identity and playbook-first gate rule. |
| [title](#title) | H1 contract. |
| [section_menu_requirement](#section_menu_requirement) | Section-menu contract for playbook docs. |
| [status](#status) | Canonical lifecycle status model for playbooks. |
| [canonical_core_sections](#canonical_core_sections) | Required/conditional section set for canonical playbooks. |
| [minimum_table_contracts](#minimum_table_contracts) | Required table shapes for canonical sections. |
| [testing](#testing) | Canonical actor-step expectation contract for human and AI-agent test execution. |
| [canonical_section_order](#canonical_section_order) | Required section ordering for newly authored canonical playbooks. |
| [extension_section_policy](#extension_section_policy) | Rules for topic-specific extension sections. |
| [evidence_sidecar_contract](#evidence_sidecar_contract) | Detached change-log and verification ownership contract. |
| [validation_and_governance_rules](#validation_and_governance_rules) | Cross-document gates this schema enforces. |

## summary

| Item | Value |
| --- | --- |
| Schema type | Specialized schema for `<TopicPascalCase>.<PhaseKey>.Playbook.md` artifacts. |
| Purpose | Define one canonical playbook structure that aligns AGENTS governance, taxonomy semantics, and observed repository playbook patterns. |
| Scope | Core playbook documents only (not changelog/verification sidecars). |
| Source layers | `Schemas/Doc.Schema.md`, `AGENTS.md`, `Docs/PlanExecutionTaxonomy.Spec.md`, and Doc CLI section inventory evidence. |

## investigation_baseline_2026_03_03

| Evidence Item | Result |
| --- | --- |
| Command | `doc list --type playbook` |
| Playbook corpus size | `72` playbooks |
| Command | `doc section list --doc <playbook>` aggregated across corpus |
| Unique `H2` section IDs | `34` |
| Universal `H2` sections (`72/72`) | `section_menu`, `linked_plan`, `phase_binding`, `linked_implementation`, `detached_evidence_documents`, `execution_lanes`, `change_log`, `verification` |
| Frequent optional sections | `dependencies` (`53/72`), `handoff_points` (`53/72`), `investigation_baseline` (`33/72`), `phase_entry_readiness_gate` (`33/72`), `wave_lane_job_board` (`14/72`), `job_task_checklist` (`11/72`) |
| Modern phase-playbook sections (current governance-aligned pattern) | `internet_best_practice_investigation`, `code_entity_draft_contract`, `validation_commands`; this schema now standardizes `testing` for actor-step testing coverage |

## doc_type

| Property | Value |
| --- | --- |
| Type | `enum` |
| Required | `yes` |
| Allowed | `playbook` |
| Rule | Core playbook docs always use `doc_type=playbook`. |

## file_name

| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Pattern (core playbook) | `<TopicPascalCase>.<PhaseKey>.Playbook.md` |
| Pattern (playbook changelog sidecar) | `<TopicPascalCase>.<PhaseKey>.Playbook.ChangeLog.md` |
| Pattern (playbook verification sidecar) | `<TopicPascalCase>.<PhaseKey>.Playbook.Verification.md` |
| Rule | `TopicPascalCase` and `PhaseKey` must match paired plan/implementation topic-phase identity. |

## topic_key

| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Rule | Derived from `<TopicPascalCase>` in filename and must map to paired `<TopicPascalCase>.Plan.md` / `<TopicPascalCase>.Impl.md`. |

## phase_key

| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Rule | Derived from `<PhaseKey>` in filename; one playbook maps to one plan phase. |
| Rule | Playbook-first gate: phase must not move to `in_progress`/`started` in plan or implementation tracker before this playbook is prepared. |

## title

| Property | Value |
| --- | --- |
| Type | `string` |
| Required | `yes` |
| Rule | First non-empty line is one H1 heading. |

## section_menu_requirement

| Property | Value |
| --- | --- |
| Type | `table` |
| Required | `yes` |
| Columns | `Section`, `Description` |
| Placement | Immediately after H1. |
| Rule | `section_menu` anchors must match actual section IDs in the same document. |

## status

| Property | Value |
| --- | --- |
| Type | `enum` |
| Required | `yes` |
| Allowed | not_started, `in_progress`, `completed`, `closed`, `blocked`, `canceled`, `unknown` |
| Rule | Playbook phase status must remain synchronized with paired plan/implementation phase state. |

## canonical_core_sections

| Section ID | Requirement | Rule |
| --- | --- | --- |
| `summary` | optional | Optional convenience section summarizing phase scope, linked artifacts, and key decisions. Does not replace required standalone sections. |
| `linked_plan` | required | Must point to paired `<TopicPascalCase>.Plan.md`. |
| `phase_binding` | required | Must declare phase key, objective, owner, and current phase status. |
| `investigation_baseline` | required for active-phase preparation | Must capture phase-specific baseline before status can move to `in_progress`. |
| `phase_entry_readiness_gate` | required for active-phase preparation | Must define explicit pass/block state for phase-entry gates. |
| `internet_best_practice_investigation` | required when entering active execution | Must include dated internet sources and adaptation rules for the phase. |
| `code_entity_draft_contract` | required when code entities are created/modified | Must detail planned entities, target files, signatures/fields, and compatibility notes before implementation starts. |
| `code_reference_snippets` | conditional (required for code-bearing phases) | Must include key before/after code snippets, reference implementations, and parity target code that guide implementation. |
| `solid_coding` | conditional (required for code-bearing phases) | Must describe how each SOLID principle (S, O, L, I, D) applies to this phase's code with concrete snippets. |
| `domain_classes` | conditional (required for code-bearing phases) | Must list typed domain classes (`F`-prefix structs, `E`-prefix enums, `I`-prefix interfaces) replacing raw strings/ints/bools. |
| `non_anti_patterns` | conditional (required for code-bearing phases) | Must list anti-patterns to avoid: no god structs (>50 fields), no if/else chains >3 branches, no static globals, no raw new/delete, no `using namespace`. |
| `abstract_coding` | conditional (required for code-bearing phases) | Must describe interfaces, shared logic, reusable functions, and composition patterns used in this phase. |
| `no_runtime_monolith_files` | conditional (required for code-bearing phases) | Must confirm no `*Runtime.h/cpp` monolith files — name files by specific domain (Service, Renderer, UpdateStages). |
| `no_monolith_files` | conditional (required for code-bearing phases) | Must confirm no source files >1000 lines — split by responsibility into same `Private/` directory. |
| `target_file_manifest` | required | Must list all files the phase plans to create, modify, or remove. Use `N/A` row when no file changes are planned. |
| `linked_implementation` | required | Must point to paired `<TopicPascalCase>.Impl.md`. |
| `detached_evidence_documents` | required | Must point to phase playbook sidecars (`ChangeLog`, `Verification`). |
| `execution_lanes` | required | Must define lane ownership and exit criteria. |
| `wave_lane_job_board` | conditional | Required when wave/job orchestration is used. |
| `job_task_checklist` | conditional | Required when jobs are decomposed into tasks. |
| `dependencies` | required for active-phase preparation | Must capture blocking contracts and cross-topic/module dependencies. |
| `validation_commands` | required when executable checks exist | Must list host command recipes for acceptance evidence. |
| `testing` | required when phase output is testable by a runtime/tool/CLI/API behavior | Must define step-by-step actor procedures and expected outcomes for both `human` and `ai_agent` execution paths (use explicit `not_applicable` rationale only for non-executable doc-only phases). |
| `handoff_points` | required | Must define outputs needed for next phase transition. |
| `change_log` | required | Must reference the playbook changelog sidecar source of truth. |
| `verification` | required | Must reference the playbook verification sidecar source of truth. |

## minimum_table_contracts

| Section ID | Minimum Columns | Notes |
| --- | --- | --- |
| `execution_lanes` | `Lane`, `Status`, `Scope`, `Exit Criteria` | `Owner` is strongly recommended. |
| `wave_lane_job_board` | `Wave`, `Lane`, `Job`, `Status`, `Scope`, `Exit Criteria` | Required when waves/jobs are used. |
| `job_task_checklist` | `Job`, `Task ID`, `Status`, `Task`, `Evidence Target` | Required when jobs are decomposed to tasks. |
| `phase_entry_readiness_gate` | `Gate`, `Status`, `Requirement`, `Evidence` | Required for active-phase entry. |
| `dependencies` | `Dependency`, `Type`, `Notes` | Must include cross-topic blockers when applicable. |
| `validation_commands` | `Host`, `Command`, `Purpose` | Keep commands executable and deterministic. |
| `testing` | `Session`, `Actor`, `Preconditions`, `Step`, `Action`, `Expected Result`, `Evidence` | One row per step; `Actor` allowed values: `human`, `ai_agent`, `both`; include at least one `human` row and one `ai_agent` row when testable behavior exists. |
| `target_file_manifest` | `File`, `Action`, `Description` | Action values: `create`, `modify`, `remove`. Use `N/A` row when no file changes are planned. |
| `handoff_points` | `From`, `To`, `Required Artifact` | Must align with next phase key in plan. |

## testing

| Property | Value |
| --- | --- |
| Type | `section` |
| Required | `conditional` |
| Trigger | Required when the phase introduces or changes testable runtime/tool/CLI/API behavior. |
| Rule | Use one step-per-row execution contract so operators and AI agents can run identical flows with deterministic expectations. |
| Rule | Each step row must be actionable without hidden context (all required flags/arguments/inputs stated in `Preconditions` or `Action`). |
| Rule | `Expected Result` must be observable and binary enough for pass/fail judgment. |
| Rule | `Evidence` must name artifact/log/screenshot path or explicit stdout/stderr signal for each step. |
| Rule | If a phase is documentation-only and has no executable behavior, include one row with `Actor=both`, `Session=not_applicable`, and rationale in `Expected Result`. |

## canonical_section_order

| Order | Section ID | Requirement |
| --- | --- | --- |
| 1 | `section_menu` | required |
| 2 | `summary` | optional |
| 3 | `linked_plan` | required |
| 4 | `phase_binding` | required |
| 5 | `investigation_baseline` | required for active-phase preparation |
| 6 | `phase_entry_readiness_gate` | required for active-phase preparation |
| 7 | `internet_best_practice_investigation` | conditional (required for active-phase execution) |
| 8 | `code_entity_draft_contract` | conditional (required for code-entity phases) |
| 9 | `code_reference_snippets` | conditional (required for code-bearing phases) |
| 10 | `solid_coding` | conditional (required for code-bearing phases) |
| 11 | `domain_classes` | conditional (required for code-bearing phases) |
| 12 | `non_anti_patterns` | conditional (required for code-bearing phases) |
| 13 | `abstract_coding` | conditional (required for code-bearing phases) |
| 14 | `no_runtime_monolith_files` | conditional (required for code-bearing phases) |
| 15 | `no_monolith_files` | conditional (required for code-bearing phases) |
| 16 | `target_file_manifest` | required |
| 17 | `linked_implementation` | required |
| 18 | `detached_evidence_documents` | required |
| 19 | `execution_lanes` | required |
| 20 | `wave_lane_job_board` | conditional |
| 21 | `job_task_checklist` | conditional |
| 22 | `dependencies` | required for active-phase preparation |
| 23 | `validation_commands` | conditional |
| 24 | `testing` | conditional |
| 25 | `handoff_points` | required |
| 26 | `change_log` | required |
| 27 | `verification` | required |

## extension_section_policy

| Rule | Guidance |
| --- | --- |
| Extension allowance | Topic-specific sections are allowed when they capture phase-specific contracts (for example rollout matrices, handshake contracts, gate checklists). |
| Naming policy | Extension headings must be snake_case and non-indexed. |
| Placement policy | Extension sections should be placed after core planning sections and before sidecar pointers (`change_log`, `verification`). |
| Ownership policy | Extensions must not duplicate ownership of core sections (`execution_lanes`, `dependencies`, `handoff_points`). |

## evidence_sidecar_contract

| Artifact | Rule |
| --- | --- |
| `<Topic>.<Phase>.Playbook.ChangeLog.md` | Owns append-only document-change history for the phase playbook. |
| `<Topic>.<Phase>.Playbook.Verification.md` | Owns append-only command/evidence outcomes for the phase playbook. |
| Playbook core body | Must stay procedural and concise; do not turn it into full historical ledger. |

## validation_and_governance_rules

| Rule ID | Rule |
| --- | --- |
| `PBS-1` | Keep playbook-first phase-entry gate: no phase `in_progress` status without prepared phase playbook. |
| `PBS-2` | Keep plan/implementation/playbook status synchronized for the same phase key. |
| `PBS-3` | Use canonical heading ownership: playbooks use `verification` heading (not plan-style aliases). |
| `PBS-4` | When waves/jobs are used, enforce taxonomy consistency between `execution_lanes` and `wave_lane_job_board`. |
| `PBS-5` | Run `doc lint` after playbook/schema updates and record any non-playbook pre-existing warnings separately. |
| `PBS-6` | Keep detailed actor-step testing in playbook `testing`; do not move that ownership into plan-level sections. |
