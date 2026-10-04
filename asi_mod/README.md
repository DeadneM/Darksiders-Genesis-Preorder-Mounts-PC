# Darksiders Genesis Enhanced ASI — Development Notebook

This file is the **authoritative development notebook** for the experimental
Darksiders Genesis ASI mod. It is intentionally cumulative so development can
be resumed later without reconstructing decisions from chat history.

> Public `main` currently remains the validated **Pre-Order Mounts PC Unlocker**.
> The ASI work is isolated on `dev/asi-overlay-v0.1` until the foundation is
> validated in game.

---

## Project goal

Build a small, robust Win64 ASI mod for **Darksiders Genesis** with:

- a local `dxgi.dll` ASI loader;
- an in-game overlay inspired by the validated Q Protocol UI model;
- **Insert** to open/close the overlay;
- full mouse interaction while the overlay is visible;
- F1-F12 fixed hotkey slots whose actions are reconfigurable;
- one INI as the only configuration source;
- fail-open behavior if the renderer/overlay cannot initialize;
- no game-file edits for ASI functionality.

The README must continue to record:

- each build;
- exact technical changes;
- test results;
- validated/rejected bases;
- known issues;
- executable compatibility information;
- current TODO;
- next investigation.

---

## Target executable audit

File supplied for the first ASI pass:

```text
DarksidersGenesis-Win64-Shipping.exe
Size:    62,113,280 bytes
SHA-256: 9f4702024df5eea1d51df7745b0ad1ea95b97009982f73ddc1218c53dff33d54
Machine: x86-64 / PE32+
```

Relevant imports confirmed directly from the executable:

```text
dxgi.dll
  CreateDXGIFactory
  CreateDXGIFactory1

d3d11.dll
d3d9.dll
```

The executable therefore provides a clean path for:

```text
game
  -> local dxgi.dll proxy
  -> real Windows System32\dxgi.dll
  -> DarksidersGenesisMod.asi
  -> D3D11 Present / ResizeBuffers hook
  -> Dear ImGui overlay
```

UE4 / MoviePlayer strings confirmed in the executable include:

```text
StartupMovies
WindowsMoviePlayer
MoviePlayer
CreateMoviePlayer
GetMoviePlayer()->SetupLoadingScreenFromIni
DefaultGameMoviePlayer.cpp
```

This gives the Skip Intro feature a real engine-native investigation path.

---

## V0.1 — Overlay foundation

**Status: TEST CANDIDATE / COMPILES SUCCESSFULLY / NOT VALIDATED IN GAME YET**

### First successful Windows build

GitHub Actions run:

```text
37231616133
```

Build result: **PASS**

Artifacts:

```text
dxgi.dll
  SHA-256 513262d7212e4c4fe907ab83e8794704b8970dc2b1e97137b152fafd3abb16f6

DarksidersGenesisMod.asi
  SHA-256 ec9b951727edecbeef73c984068107c06d183d078370b412889bad402f73e1e5

DarksidersGenesis_ASI_V0.1_TEST.zip
  SHA-256 7cf4fb36e1e72a1ad759a97a9ffd418757bd9dd0d5795b09a6c4627e6d7ced1d
```

Both compiled binaries were independently checked as **PE32+ x86-64 DLLs**.

### Build-system notes

The first CI pass exposed two build-environment details which are now documented:

- GitHub `windows-latest` moved to the Windows 2025 / Visual Studio 2026 image,
  so the workflow now uses the `Visual Studio 18 2026` CMake generator.
- MinHook's Windows x64 output uses its configuration postfix
  (`minhook.x64.lib`); CMake target resolution was hardened so the ASI links
  the actual MinHook target instead of guessing a library filename.

The source itself then compiled and linked successfully.

### Purpose

V0.1 is deliberately a foundation build.

It validates:

1. DXGI proxy loading;
2. automatic ASI discovery/loading;
3. D3D11 swap-chain discovery;
4. Present + ResizeBuffers hooks;
5. Dear ImGui initialization on the real game swap chain;
6. Insert menu toggle;
7. mouse input/cursor access;
8. suppression of gameplay F1-F12 input while the menu is open;
9. F1-F12 action mapping;
10. INI Save / Reload / Reset Defaults;
11. clean logging to `DarksidersGenesisMod.log`.

V0.1 does **not** pretend that gameplay features are already implemented.
Every requested feature is enabled by default in configuration, but is clearly
shown as **Hook pending** until a native implementation is identified and tested.

### Overlay layout

Tabs:

```text
General
Features
Hotkeys
About
```

Hotkey architecture follows Q Protocol's clean model:

```text
F1-F12 fixed key
      -> selected Action
      -> shared feature implementation
```

No duplicate hotkey-specific gameplay code should ever be added.

### Authoritative default hotkeys

| Key | Action |
|---|---|
| F1 | Toggle HUD |
| F2 | Movement Speed |
| F3 | Action Recovery |
| F4 | Skip Intro Videos |
| F5 | Third Person |
| F6-F12 | None |

### Authoritative default feature policy

All requested options are **enabled by default**:

```ini
ToggleHUD=1
MovementSpeed=1
ActionRecovery=1
SkipIntroVideos=1
ThirdPerson=1
```

Initial tuning placeholders stored in the INI:

```ini
MovementSpeedMultiplier=1.150
ActionRecoveryMultiplier=2.000
```

These values are intentionally inert until their native hooks are implemented.

---

## Current TODO

### 1. Toggle HUD

Goal:

- toggle the actual in-game HUD without disabling unrelated rendering;
- preserve menus/overlay;
- no permanent asset edits.

Status: **pending native UE4/HUD path audit**.

### 2. Movement Speed

Goal:

- adjustable movement speed;
- user-facing multiplier in overlay/INI;
- avoid globally changing game time.

Default option state: **Enabled**.

Initial stored test value: **1.15x**.

Status: **pending player movement component audit**.

### 3. Action Recovery Speed

Observed behavior:

After some actions there is a short interval where the character is visibly
finished but cannot move yet.

Goal:

- shorten/remove only this post-action movement lock;
- do not indiscriminately speed up all animations;
- expose a tuning control if the engine path supports a scalar.

Default option state: **Enabled**.

Initial stored test value: **2.00x**.

Status: **pending state/action-lock audit**.

### 4. Skip Intro Videos

Goal:

- skip startup intro videos through the ASI;
- do not delete/rename original game files;
- leave normal loading/movie systems intact when possible.

Current lead:

```text
UE4 DefaultGameMoviePlayer
StartupMovies
WindowsMoviePlayer
```

Default option state: **Enabled**.

Status: **first binary lead confirmed; runtime hook not implemented yet**.

### 5. Third Person

Goal:

- switch gameplay to a usable third-person camera;
- preserve aiming/collision/control behavior as much as possible;
- ideally allow runtime toggle.

Default option state: **Enabled**.

Status: **pending camera/player-controller audit**.

---

## Design rules

1. **Fail open.** Overlay failure must not stop the game from running.
2. **No fake success.** UI state and real gameplay-hook state are separate.
3. **INI is authoritative.** No hidden secondary config database.
4. **One feature implementation.** Hotkeys and overlay call the same primitive.
5. **No executable replacement.** ASI development should not require shipping a
   modified game EXE.
6. **Keep public validated work safe.** The existing pre-order mounts release is
   untouched while this branch is experimental.
7. **Journal every build here.** Never rely on chat history as the only source.

---

## Test checklist for V0.1

Install next to the game executable:

```text
dxgi.dll
DarksidersGenesisMod.asi
DarksidersGenesisMod.ini
README.md
```

Then verify:

- [ ] Game boots normally.
- [ ] `DarksidersGenesisMod.log` is created.
- [ ] Log says D3D11 hooks were installed.
- [ ] Log says ImGui overlay is READY.
- [ ] Insert opens the overlay.
- [ ] Insert closes the overlay.
- [ ] Mouse moves and clicks correctly inside the overlay.
- [ ] Game does not react to menu clicks.
- [ ] F1-F12 can be reassigned from Hotkeys.
- [ ] Save persists settings after restart.
- [ ] Reload restores INI values.
- [ ] Reset Defaults restores F1-F5 mapping and all feature defaults.
- [ ] Alt-Tab does not break the overlay.
- [ ] Resolution/fullscreen changes do not break the overlay.
- [ ] Game exits normally.

---

## Build history

### V0.1

- New DXGI proxy loader.
- Loads `*.asi` from the game executable directory.
- Forwards the game's confirmed `CreateDXGIFactory` and
  `CreateDXGIFactory1` calls to the real System32 DXGI.
- Includes compatibility forwarders for common newer DXGI exports.
- New D3D11 overlay core using MinHook + Dear ImGui.
- Insert menu toggle.
- Mouse/keyboard capture while open.
- F1-F12 Q Protocol-style action map.
- INI persistence.
- Requested features all enabled by default.
- Gameplay hooks deliberately left pending for the first foundation test.
- Startup movie strings audited and recorded.

**Compilation:** PASS.

**In-game validation:** awaiting first test.

**First successful build run:** `37231616133`.

**Canonical V0.1-test binary hashes before README-only rebuild:**

```text
dxgi.dll                  513262d7212e4c4fe907ab83e8794704b8970dc2b1e97137b152fafd3abb16f6
DarksidersGenesisMod.asi  ec9b951727edecbeef73c984068107c06d183d078370b412889bad402f73e1e5
```
