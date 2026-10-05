# Regression tests

The reference Linux configuration registers **67 CTest cases**, down from 74
before the 2026-10-06 test cleanup. The Steam password-prompt test is UNIX-only,
so the registered count can differ by platform. `NINSLASH_BUILD_TESTS` defaults
to `OFF`: these are opt-in developer/CI targets, not work performed by the game.

```sh
cmake -S . -B build-tests -DNINSLASH_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests --parallel 2
ctest --test-dir build-tests --output-on-failure
```

Multi-configuration generators also need `--config Release` for the build and
`-C Release` for CTest. See the main build documentation for dependencies.

## What remains

- Behavior and contract tests for content import, saves, networking, Lua/weapon
  execution, inventory layout, PvE/Boss behavior and asset compatibility.
- Steam tests that actually generate/check manifests or exercise password input
  and output redaction. These are not source-text matching tests.
- The official weapon hash check compares the running program with the current
  source inputs; it does not pin every future version to a historical hash.
- Content formatting and generated visual-baseline consistency checks. These
  enforce intentional content/authoring contracts, not C++ variable names or
  implementation layout. They are not substitutes for runtime behavior tests.

## Removed implementation-coupled checks

Seven whole cases and their implementation files were removed: four render-path
source-pattern checks, the build/workflow source-text checklist, a weapon-source
pattern blacklist, and a Lua API declaration checker coupled to internal C++
variable/macro spellings. Their CMake registrations were deleted, not disabled or
moved to a non-blocking gate.

The inventory layout test also lost its source-text scan for a configuration
macro/default. Its actual layout and interaction assertions remain unchanged.

These checks could reject harmless refactors and could pass while the expected
text was present in non-executing code. The cleanup removes that weak coverage;
it does **not** claim that equivalent GPU, architecture or API-documentation
coverage has been added. In particular, the removed menu check's optional GLSL
compiler invocation is no longer in CTest. A developer can still explicitly run
`glslangValidator -S frag data/shaders/menu.frag` when that tool is installed.

## Maintenance policy

Tests should fail when an intended behavior or external contract breaks, not
because a comment, helper name, logging message or internal code arrangement
changes. Legitimate behavior changes may require reviewed expectation updates;
blindly updating snapshots to make a test green is not a validation strategy.

The archived originals and exact change list are in
`design/test-cleanup-20261006/`. Historical reports describing 74 tests or six
source checks are snapshots, not the current inventory; two of the six Python
`check_` files were always behavior tests and remain present.
