# R09 product host strategy

Status: **Accepted for R09 execution**

Decision: **Use the existing Windows Terminal host, packaging, and XAML product surface while Rust progressively becomes the owner of migrated behavior. Keep a managed C#/XAML host as a possible later evolution, but do not require it for the first R09 product milestone.**

## Context

R08 proved behavioral equivalence between the migrated Rust implementation and Microsoft C++ across the covered contract surface. R09 changes the project contract from equivalence proof to ownership transfer and removal of duplicate C++ implementation.

The first R09 plan assumed that `windows-rus-terminal` should immediately become a standalone Rust executable with its own Windows bootstrap, HWND lifecycle, message loop, ConPTY session, and rendering path.

Inspection of the actual Microsoft product build showed that this would duplicate product integration that Windows Terminal already has:

- `src/cascadia/WindowsTerminal/WindowsTerminal.vcxproj` is the current application project and produces `WindowsTerminal.exe`.
- `src/cascadia/CascadiaPackage/CascadiaPackage.wapproj` is the package/deployment owner and names `WindowsTerminal.vcxproj` as its entry-point project.
- `TerminalApp` and related projects own the existing XAML product surface through C++/WinRT.
- The repository currently contains no C# implementation of the Terminal application host. Therefore, preserving XAML does not currently mean preserving an existing C# layer.
- Microsoft explicitly documents that the normal product path is package/deploy driven; directly launching `WindowsTerminal.exe` is not the canonical development/product bootstrap.

The consequence is important: R09 should preserve the working Windows product shell first and replace semantic owners underneath it. Reimplementing the shell in Rust would create unnecessary risk and delay the actual objective: deleting duplicate C++ where Rust is already authoritative.

## Decision

R09 adopts **Option A** as the immediate product-integration strategy.

### Option A — preserve the existing Windows host and XAML

Keep the current Windows product shell while Rust replaces behavior below it.

```text
CascadiaPackage / package registration
                |
                v
WindowsTerminal.exe
C++/WinRT Windows host seam
                |
                v
existing XAML product surface
                |
                v
narrow interop seams
                |
                v
Rust owners
(parser, input, adapter, buffer, core,
host semantics, renderer semantics, settings, app semantics)
```

This is the active R09 execution path.

The C++ host is not automatically considered migration debt. Each surface must be classified by responsibility:

- `DELETE` — Rust already owns the behavior and C++ is redundant.
- `KEEP-NATIVE` — the code is a deliberate Win32, COM, WinRT, ABI, GDI, DWrite, DirectX, packaging, or other genuine Windows boundary.
- `SPLIT` — the component mixes portable/product logic with a native boundary; move the portable logic to Rust and retain the smallest native seam.
- `UNRESOLVED` — requires investigation. This category must trend toward zero.

`AppHost`, `WindowEmperor`, `IslandWindow`, `NonClientIslandWindow`, and similar host surfaces therefore begin R09 as classification targets, not automatic deletion targets.

### Option B — managed C#/XAML product host

Option B remains open as a deliberate future evolution:

```text
CascadiaPackage / package registration
                |
                v
C# / WinUI application host
                |
                v
existing or migrated XAML presentation
                |
                v
managed/native interop seam
                |
                v
Rust owners
```

This can become attractive when the remaining C++/WinRT UI orchestration is mostly presentation glue rather than native Windows ownership.

Option B is **not** required to reach the first usable R09 product. It must be justified slice by slice by lower complexity, better maintainability, or clearer ownership. We will not introduce C# merely to increase language uniformity.

### Option C — standalone Rust Windows application host

A complete Rust replacement of the existing application host, HWND lifecycle, XAML integration, packaging behavior, window orchestration, and related Windows product machinery is **not the active strategy**.

It may be reconsidered only if later evidence shows that the native host is itself the dominant source of irreducible complexity. Until then it duplicates mature Windows integration without helping the primary R09 goal.

## Ownership rules

R09 preserves **behavior**, not Microsoft C++ shape.

The preferred owners are:

- **Rust** — deterministic domain logic, parser behavior, input interpretation, adapter/dispatch semantics, text buffer, terminal core, portable host/session semantics, renderer policy/state, settings semantics, command/product state, and any other migrated behavior that does not intrinsically require a Windows-native owner.
- **XAML** — declarative presentation.
- **C++/WinRT / Win32** — only the minimum Windows-native product seams still required by the existing product shell.
- **C#** — optional future owner for UI orchestration where a managed WinUI layer is demonstrably cleaner than retaining C++/WinRT. There is no assumption that such a layer already exists in the upstream repository.

A compatibility façade is not protected merely because it was required by R08. If the product path can call the Rust owner directly through a narrower seam, the R08 façade should be removed.

## Revised R09 product slices

The earlier S01–S08 plan is corrected so that product integration occurs inside the existing Windows Terminal product path.

### R09-S01 — establish the real product build and insertion point

- Build the existing Terminal product through its real solution/package path.
- Verify the current `CascadiaPackage -> WindowsTerminal.exe -> TerminalApp/XAML` chain.
- Identify the smallest production seam through which an existing Rust owner can participate in the running product.
- Treat the standalone `windows-rus-terminal` Cargo executable as an experiment/harness, not as the product architecture.

Exit evidence: the branch can build the real product path and the first Rust insertion seam is mechanically identified.

### R09-S02 — first Rust-owned behavior in the real product

- Route one narrow, observable product behavior through its Rust owner.
- Preserve the existing window lifecycle, packaging, XAML, and shell behavior.
- Add mechanical proof that the product path actually consumes Rust rather than the duplicate C++ implementation.

Exit evidence: running the existing product exercises Rust for the selected behavior.

### R09-S03 — product input/output path ownership

- Expand the real product path through migrated input/parser/core/buffer owners where seams are ready.
- Delete or bypass C++ implementations once dependency/build/test evidence proves they are no longer consumed.

Exit evidence: terminal interaction traverses migrated Rust owners while using the existing application shell.

### R09-S04 — ConPTY/session ownership

- Connect the product session path to migrated Rust host/session semantics where appropriate.
- Keep Windows-native ConPTY/handle/ABI operations native only where they are genuine platform seams.

Exit evidence: a real shell session uses Rust-owned semantic flow in the production application.

### R09-S05 — renderer ownership

- Connect the existing presentation/control path to migrated Rust renderer state and policy.
- Preserve GDI/DWrite/DirectX or other native rendering seams only where required.

Exit evidence: observable terminal output is produced through the migrated renderer ownership chain.

### R09-S06 — app/settings/product semantics

- Move remaining portable TerminalApp/settings/control behavior to Rust owners.
- Classify UI orchestration separately from native host responsibility.
- Consider C# only where it simplifies genuine UI orchestration and does not expand the milestone unnecessarily.

### R09-S07 — C++ deletion wave

- Use build graphs, source maps, tests, and runtime evidence to delete duplicate C++ implementation.
- Convert mixed surfaces to `SPLIT` with a minimal native seam.
- Drive `UNRESOLVED` toward zero.

### R09-S08 — usable product hardening

- Release/package build.
- Product smoke tests.
- CI gates for the real Windows product path.
- Verify no compatibility façade remains solely for R08 parity shape.
- Record remaining C++ as intentional `KEEP-NATIVE` or explicitly scheduled `SPLIT` work.

Exit evidence: the normal Windows Terminal product shell is usable as a basic terminal while the migrated Rust owners carry the relevant terminal behavior.

## First usable-product milestone

The milestone is no longer defined by `cargo run --release -p windows-rus-terminal` alone.

The tangible product proof is:

```text
real Windows Terminal package/application bootstrap
        |
        v
existing window and XAML open
        |
        v
real shell session starts
        |
        v
keyboard input reaches Rust-owned input/session path
        |
        v
ConPTY output reaches Rust-owned parser/core/buffer path
        |
        v
Rust-owned renderer semantics feed the existing presentation path
        |
        v
output is visible and resize/focus/lifecycle remain usable
```

A standalone Cargo executable may still be useful as a test harness, diagnostics tool, or future bootstrap experiment. It is not evidence of product integration unless it exercises the same product ownership chain.

## Mechanical-first deletion rule

Before designing a replacement abstraction, prefer mechanical evidence:

1. dependency graph shows the C++ implementation is no longer referenced;
2. Windows product build succeeds without it;
3. targeted Rust and Microsoft compatibility tests remain green;
4. product smoke evidence remains green;
5. only then delete the duplicate C++ implementation.

For mixed native/portable components, perform the same proof after extracting portable behavior to Rust and shrinking the native seam.

## Criteria for revisiting Option B

A C#/XAML host should be evaluated when one or more of these conditions become true:

- remaining C++/WinRT code primarily performs UI orchestration rather than native Windows work;
- managed WinUI interop would remove more complexity than it adds;
- the native host can be reduced to a stable ABI/platform seam beneath C#;
- packaging, activation, COM, remoting, accessibility, titlebar, drag/drop, or other Windows integration can remain intact without rebuilding product machinery;
- a vertical C# slice can be demonstrated with the same mechanical and product evidence required of Rust migrations.

Until those conditions are met, Option A is the shortest path to Rust ownership and C++ deletion.

## Non-goals

R09 does not require:

- reproducing Microsoft internal C++ class structure in Rust;
- replacing XAML merely because the project is migrating to Rust;
- introducing C# where it does not simplify ownership;
- rewriting mature Windows packaging, activation, COM, or window lifecycle machinery before there is evidence that doing so reduces product risk;
- preserving R08 compatibility façades after their product consumers can use the Rust owner directly.

## Summary

R09 product integration follows this sequence:

```text
PRESERVE THE WORKING PRODUCT SHELL
        -> INSERT RUST OWNERS
        -> VERIFY OBSERVABLE BEHAVIOR
        -> DELETE DUPLICATE C++
        -> SHRINK NATIVE SEAMS
        -> REASSESS C#/XAML HOST WHEN JUSTIFIED
```

The priority remains a usable WindowsRusTerminal product with Rust owning terminal behavior. The shortest safe path to that goal is to reuse the existing Windows Terminal shell first, not to recreate it.
