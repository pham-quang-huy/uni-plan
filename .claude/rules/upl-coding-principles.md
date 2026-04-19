When writing or modifying C++ code in this repository, always follow these principles:

1. **LONG-TERM FIX**: Never use workarounds, quick hacks, or backward-compat shims. Fix the root cause. If a bug is caused by a design flaw, fix the design — do not patch the caller.
   - **DATA FIX GATE**: When data doesn't match expectations, STOP before writing any code. Answer these in order:
     1. What is the semantic contract this field must uphold?
     2. Which layer is producing the wrong value? (source doc, schema, extraction, serializer)
     3. What should that layer guarantee instead?
   - Only write code after identifying the responsible layer. Fix that layer — never add content-sniffing, pattern-matching, or fallback logic in the consumer.
   - **Workaround smell**: any `if (value.find("..."))` that detects content to decide behavior is a workaround. Rewrite as a guarantee from the producer.
1a. **MAXIMUM-DEPTH SOLUTION — NEVER MINIMUM**: Same contract as the global rule at `~/.claude/rules/solution-depth-discipline.md` — a fix is complete only when SYMPTOM, SWEEP, GUARD, and ROOT clauses are all satisfied. When the user asks for "robust", "deep", or "long-term" solutions, treat that as explicit confirmation that all four clauses are in scope; do not negotiate down. For this repo specifically: a watch/TUI-panel bug almost always implies (a) data integrity in affected `.Plan.json` bundles, (b) schema/validator coverage that would catch the class of bug, (c) CLI command gaps that prevented the data fix from being expressible, and (d) the UI surface itself. Sweep all four; if one layer is out of scope, state why.
2. **SOLID**: Single responsibility, open/closed, Liskov substitution, interface segregation, dependency inversion.
3. **DOMAIN TYPES**: Use typed structs (F-prefix), enum classes (E-prefix) instead of raw strings, ints, bools, or untyped maps.
4. **NO IF/ELSE HELL**: Replace long if/else chains with:
   - `enum class` + `switch` for state/type dispatch
   - `std::unordered_map<Key, Handler>` for open-ended surfaces
   - `std::variant` + `std::visit` for closed type sets
5. **NAMING**: Follow NAMING.md — PascalCase locals, uppercase acronyms (ID, JSON, URL), m-prefix members, In/Out params.
6. **MEMORY**: Prefer value > unique_ptr > optional > shared_ptr > raw pointer. Never raw new/delete.
7. **DIAGNOSTICS**: Use `std::cerr` for CLI diagnostic output.
