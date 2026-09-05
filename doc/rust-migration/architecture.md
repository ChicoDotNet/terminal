# Rust migration architecture

This document defines the migration track in `ChicoDotNet/terminal`. The goal is not a big-bang rewrite. The goal is a verifiable, incremental Rust implementation that preserves Windows Terminal behavior while making each migrated component independently testable.

The repeatable development lifecycle — Draft CI preflight, spelling policy, commit discipline, queued-CI writing windows, and the modified-sandwich strategy that alternates Missing and Partial work — is part of the migration contract and is defined in [`development-strategy.md`](development-strategy.md).

R09 product-host ownership, including the decision to preserve the existing Windows host/XAML path first while keeping a possible future C#/XAML host open, is defined in [`r09-product-host-strategy.md`](r09-product-host-strategy.md).

## Principles

1. **Microsoft C++ remains the oracle until a Rust component proves equivalence.**
2. **Fast Rust feedback is the normal development loop.** Microsoft/TAEF tests are the contractual compatibility gate.
3. **Migrate vertical slices.** Do not port an entire foundational C++ library merely because the current build groups unrelated responsibilities together.
4. **Keep unsafe code at explicit FFI boundaries.** Safe implementation crates should forbid unsafe code.
5. **No product C++ is removed in R00.** Infrastructure and evidence come first.
6. **Known upstream/baseline failures are recorded rather than silently normalized.** New failures are regressions.
7. **Performance and memory are measured, not assumed.**
8. **XAML is not migration debt.** The migration target is C++ behavior that Rust can own, not declarative presentation. The upstream Terminal application currently uses C++/WinRT for its XAML code-behind and product orchestration; there is no existing C# Terminal application layer to preserve. C# remains an optional future owner for UI orchestration when introducing it demonstrably reduces complexity.
9. **Compatibility gates are selected by evidence, not convenience.** Rust tests are the fast inner loop. Microsoft tests remain blocking wherever the equivalence matrix says Rust coverage is partial, platform-only, boundary-sensitive, or missing. The full Microsoft suite remains a stage/final certification gate rather than a default per-commit tax once narrower equivalence has been demonstrated.
10. **R09 preserves the working product shell before replacing it.** Existing Windows packaging, activation, window lifecycle, and XAML machinery remain in place until a narrower owner transition is mechanically proven. A standalone Rust HWND/application path is not the default product strategy.

## Migration order

| Increment | Scope | Exit condition |
|---|---|---|
| R00 | Workspace, CI, TAEF contract harness, baseline, scorecard | Fast Rust CI works and TAEF output is evaluated independently of the legacy wrapper exit status |
| R01 | VT parser: Base64, state machine, output/input engines | Differential corpus agrees and Microsoft `terminal` suite does not regress |
| R02 | Terminal input plus required pure types | Input contracts and differential tests agree |
| R03 | Adapter/dispatch/Sixel | Adapter contract agrees |
| R04 | TextBuffer/TIL/pure foundational types | Foundational suites agree |
| R05 | TerminalCore | Core suite agrees |
| R06 | Host/server/interactivity/ConPTY | Host and ConPTY contracts agree |
| R07 | Renderer | Rendering acceptance/performance evidence agrees |
| R08 | WinRT/COM/XAML/settings/control/UI and product equivalence | Product-level acceptance agrees and migrated Rust behavior reaches complete functional equivalence |
| R09 | Product ownership transfer, compatibility façade removal, and C++ cleanup | The real Windows Terminal product path consumes Rust owners; remaining C++ is intentional native/platform ownership or explicitly scheduled split work |

## FFI shape

```text
Existing C++ code/tests
        |
        v
C++ compatibility façade
        |
        | C ABI
        v
terminal-*-ffi
        |
        | safe Rust API
        v
terminal-*
```

Rust ABI is not exposed directly to C++. The C ABI should use narrow, explicit ownership rules, opaque handles, and byte/slice-oriented buffers where practical.

During R08 and early R09, the actual product UI shape is:

```text
CascadiaPackage / Windows activation
        |
        v
WindowsTerminal.exe
C++/WinRT host seam
        |
        v
XAML presentation + C++/WinRT orchestration
        |
        v
narrow native / FFI seams
        |
        v
safe Rust semantic crates
```

R09 progressively shrinks the C++/WinRT orchestration layer. XAML may remain unchanged. If later evidence favors a managed host, the optional evolution is:

```text
XAML
  |
  v
C# / WinUI code-behind, bindings, view models, orchestration
  |
  | managed/native interop where required
  v
narrow Rust product boundary
  |
  v
safe Rust semantic crates
```

This is not a mandate to introduce C#. It is an explicitly preserved architectural option for UI orchestration after Rust ownership has reduced the native surface enough to evaluate the trade cleanly.

## R08/R09 language ownership

R08 proved behavior while preserving the most appropriate owner for each layer. R09 now transfers ownership and removes duplication:

- **Rust:** deterministic domain logic, settings semantics, control state, input interpretation, lifecycle state machines that are not intrinsically Windows-native, renderer/control policy, parser/buffer/core behavior, portable host/session semantics, and product behavior that does not require UI or platform ownership.
- **XAML:** presentation and declarative UI.
- **C++/WinRT/COM/Win32:** current product host and UI orchestration plus genuine platform/ABI ownership. In R09 this surface must be classified and reduced rather than preserved by default.
- **C#:** optional future owner for XAML code-behind, bindings, view models, and managed UI orchestration where a managed WinUI slice is demonstrably simpler than retaining C++/WinRT. No existing upstream C# Terminal host is assumed.

For each residual C++ surface in R09 use:

- **DELETE** — Rust already owns the behavior and C++ is redundant.
- **KEEP-NATIVE** — deliberate Windows-native boundary.
- **SPLIT** — mixed portable and native responsibility; move portable behavior to Rust and keep the smallest native seam.
- **UNRESOLVED** — requires investigation and must trend toward zero.

The detailed product-host decision and revised R09 slices are in [`r09-product-host-strategy.md`](r09-product-host-strategy.md).

## Test equivalence and CI tiers

`doc/rust-migration/test-equivalence-matrix.md` is the evidence ledger for deciding which Microsoft tests remain necessary at each boundary.

Coverage classifications are:

- **Exact:** the Rust contract covers the same relevant behavior.
- **Stronger:** the Rust contract covers the Microsoft case plus additional vectors or invariants.
- **Partial:** Rust covers only part of the behavior; the Microsoft test remains blocking.
- **Platform-only:** the behavior requires Windows/COM/WinRT/GDI/DWrite/DX or another platform surface; the Microsoft test remains blocking.
- **UI-owned:** the responsibility correctly belongs to presentation/UI orchestration rather than the Rust semantic layer. It may remain XAML/C++/WinRT initially or move to C#/XAML later if justified.
- **Missing:** no adequate migrated equivalent exists yet; the Microsoft test remains blocking.

CI tiers are:

1. **Fast:** every change — Rust fmt, Clippy with `-D warnings`, workspace check/test on Linux and Windows, repository quality/spelling, and TAEF harness self-test.
2. **Boundary:** when C++/FFI/platform boundaries change — the Microsoft tests mapped to that boundary and every matrix row still classified Partial, Platform-only, or Missing for the affected area.
3. **Stage:** before an R07/R08 merge — the accumulated Microsoft contracts for the stage that have not been proven Exact/Stronger by the matrix.
4. **Full certification:** the complete Microsoft Terminal suite at the R08 exit and again during R09 final differential/cleanup validation.
5. **Product path:** during R09 — build/deploy/smoke evidence for the real `CascadiaPackage -> WindowsTerminal.exe -> XAML -> Rust owners` chain whenever product seams change.

No tier may be skipped merely to reduce runtime. A test can leave the per-commit boundary set only after its equivalence evidence is recorded in the matrix.

## R01 target

The first production slice is `src/terminal/parser`. The existing project already isolates parser behavior into a static library and has a strong TAEF contract. R01 begins with Base64 because it is small and deterministic, then moves through the state machine and engines before connecting the C++ façade to Rust.

The intended proof is stronger than "the Rust tests pass":

```text
same VT corpus
   +--> C++ parser --> observation A
   |
   +--> Rust parser -> observation B

A == B
```

Only after differential equality is established does the Microsoft `terminal` suite become the final compatibility gate for the slice.
