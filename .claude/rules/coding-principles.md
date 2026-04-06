When writing or modifying C++ code in this repository, always follow these principles:

1. **LONG-TERM FIX**: Never use workarounds, quick hacks, or backward-compat shims. Fix the root cause. If a bug is caused by a design flaw, fix the design — do not patch the caller.
2. **SOLID**: Single responsibility, open/closed, Liskov substitution, interface segregation, dependency inversion.
3. **DOMAIN TYPES**: Use typed structs (F-prefix), enum classes (E-prefix) instead of raw strings, ints, bools, or untyped maps.
4. **NO IF/ELSE HELL**: Replace long if/else chains with:
   - `enum class` + `switch` for state/type dispatch
   - `std::unordered_map<Key, Handler>` for open-ended surfaces
   - `std::variant` + `std::visit` for closed type sets
5. **NAMING**: Follow NAMING.md — PascalCase locals, uppercase acronyms (ID, JSON, URL), m-prefix members, In/Out params.
6. **MEMORY**: Prefer value > unique_ptr > optional > shared_ptr > raw pointer. Never raw new/delete.
7. **DIAGNOSTICS**: Use `std::cerr` for CLI diagnostic output.
