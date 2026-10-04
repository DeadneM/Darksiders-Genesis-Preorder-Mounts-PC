# Darksiders Genesis Enhanced ASI — Development Notebook

This file is the **authoritative development notebook** for the experimental
Darksiders Genesis ASI mod. It is intentionally cumulative so development can
be resumed later without reconstructing decisions from chat history.

> Public `main` currently remains the validated **Pre-Order Mounts PC Unlocker**.
> **V0.1 overlay foundation is validated in game.** Further ASI work is kept on
> experimental development branches until each feature is validated.

---

## Project goal

Build a small, robust Win64 ASI mod for **Darksiders Genesis** with:

- a local `dxgi.dll` ASI loader;
- an in-game overlay inspired by the validated Q Protocol UI model;
- a **rebindable menu key** to open/close the overlay, default **Insert**;
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

## V0.3A — Native Toggle HUD

**Status: TEST CANDIDATE**

V0.3A is cumulative from V0.2A:

- validated V0.1 DXGI / D3D11 / ImGui overlay foundation;
- rebindable overlay menu key;
- first real gameplay feature: native Toggle HUD.

### HUD binary audit

The supplied target executable contains the game-specific console variable:

```text
ui.HideHud
>0 Hides all HUD completely.
Force HUD to be hidden.
<=0: default HUD behavior
1: HUD always hidden
```

Static audit of the validated executable:

```text
ui.HideHud UTF-16 string RVA : 0x26F3F78
registration name xref RVA    : 0x000E779A
CVar data-slot RVA            : 0x03803748
native boolean getter RVA     : 0x0063AB50
```

Getter body:

```asm
mov rax, [rip + ui.HideHud_data_slot]
cmp dword ptr [rax], 0
setne al
ret
```

### V0.3A implementation

The ASI **does not overwrite the CVar value**.

At runtime it:

1. finds the UTF-16 `ui.HideHud` name inside `.rdata`;
2. finds the unique RIP-relative registration reference in `.text`;
3. follows the validated registration layout to the CVar data slot;
4. locates the unique native boolean getter using that exact data slot;
5. hooks only the getter with MinHook.

Return policy:

```text
nativeHidden || modHidden
```

This means the mod can request HUD hiding without cancelling a native game
request to hide the HUD.

Runtime behavior:

- F1 defaults to Toggle HUD;
- first press: HUD hidden;
- next press: HUD visible again;
- overlay -> Features -> HUD Hidden can also change the runtime state;
- runtime HUD state starts visible on every launch;
- `[Features] ToggleHUD=1` enables the feature by default;
- if resolution fails, Toggle HUD becomes unavailable while the rest of the
  ASI stays fail-open.

### V0.2A — Rebindable menu key / safe rollback

**Status: TEST CANDIDATE**

V0.2 keeps the validated V0.1 renderer/input foundation and makes the overlay
open/close key fully rebindable at runtime.

Behavior:

- default remains `Insert`;
- `General -> Rebind Menu Key` waits for the next keyboard key;
- `Esc` cancels capture;
- the new key is saved immediately to `[Overlay] MenuKey`;
- Save / Reload / Reset Defaults all understand the menu binding;
- Reset Defaults restores `Insert`;
- arbitrary keyboard keys are persisted as a readable token when known, or
  `VK_XX` for less common virtual keys;
- the captured key is debounced so the same press does not instantly close the menu.

### V0.1 — Overlay foundation

**Status: VALIDATED IN GAME**

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
6. validated menu toggle;
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

Status: **implemented in V0.3A through native `ui.HideHud` getter hook; awaiting in-game validation**.

### 2. Movement Speed

Goal:

- adjustable movement speed;
- user-facing multiplier in overlay/INI;
- avoid globally changing game time;
- do not affect enemies/NPCs;
- do not consume the separate horse-speed roadmap item.

Default option state: **Enabled**.

Initial stored test value: **1.15x**.

Status: **implemented in V0.4A; awaiting in-game validation**.

Implementation notes:

- resolves the Mayhem reflection registration for `GetMaxSpeed`;
- identifies the `AMayhemCharacter::GetMaxSpeed` native helper through its exec wrapper;
- applies the multiplier only when `APawn::IsLocallyControlled` is true;
- explicitly excludes `AMayhemPlayerCharacter::IsHorseActive`;
- horse movement remains untouched for the dedicated Horse Speed / Sprint feature.

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

Status: **implemented in V0.5A; awaiting in-game validation**.

Native audit result:

- owner: `UMayhemPlayerAbilityComponent`;
- reflected field: `MoveInterruptDelaySec`;
- field offset: `+0x110`;
- runtime elapsed timer used by the same gate: `+0x114`;
- `ECharacterActions::MOVE = 0x1D`;
- native gate RVA in the audited executable: `0x5B3150`;
- signature match count: exactly **1**.

The native logic explicitly rejects `MOVE` while:

```text
MoveInterruptDelaySec > elapsed timer
```

V0.5A therefore scales only this delay during the native check.

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

### 6. Pistol Damage

Goal:

- increase pistol damage;
- ideally expose a multiplier in the overlay/INI;
- avoid affecting unrelated weapon classes.

Status: **pending weapon-damage audit**.

### 7. Melee Damage

Goal:

- increase melee damage;
- keep the modifier separate from firearm damage;
- ideally expose a multiplier in the overlay/INI.

Status: **pending melee-damage audit**.

### 8. Jump Height

Goal:

- increase jump height;
- expose an adjustable multiplier if the native movement path allows it;
- preserve reliable landing and collision behavior.

Status: **pending character-movement audit**.

### 9. Horse Speed / Sprint

Goal:

- increase normal horse movement speed;
- increase horse sprint speed;
- keep normal speed and sprint tunable independently if possible.

Status: **pending mount movement/sprint audit**.

### 10. FOV

Goal:

- expose an adjustable gameplay FOV;
- keep it runtime-configurable from the overlay/INI;
- preserve camera transitions and special camera states.

Status: **pending camera/FOV audit**.

### 11. Hotstreak Charge

Goal:

- increase Hotstreak charge/gain rate;
- expose a configurable multiplier if the underlying system supports it;
- avoid changing unrelated resource/ability charge systems.

Status: **pending Hotstreak system audit**.

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

- [x] Game boots normally.
- [ ] `DarksidersGenesisMod.log` is created.
- [ ] Log says D3D11 hooks were installed.
- [ ] Log says ImGui overlay is READY.
- [x] Insert opens the overlay.
- [x] Insert closes the overlay.
- [x] Mouse moves and clicks correctly inside the overlay.
- [ ] Game does not react to menu clicks.
- [x] F1-F12 can be reassigned from Hotkeys.
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

**In-game validation:** PASS — user confirmed V0.1 foundation works perfectly.

**First successful build run:** `37231616133`.

**Canonical V0.1-test binary hashes before README-only rebuild:**

```text
dxgi.dll                  513262d7212e4c4fe907ab83e8794704b8970dc2b1e97137b152fafd3abb16f6
DarksidersGenesisMod.asi  ec9b951727edecbeef73c984068107c06d183d078370b412889bad402f73e1e5
```


### V0.2

- V0.1 becomes the validated overlay foundation.
- Adds runtime menu-key rebinding from the General tab.
- Default key remains Insert.
- Escape cancels a pending key capture.
- New binding is written immediately to `[Overlay] MenuKey`.
- INI parser accepts named common keys, F1-F24, letters/numbers and `VK_XX` fallback tokens.
- Reset Defaults restores Insert.
- Menu title shows the current open/close key instead of hard-coding Insert.
- Captured key is debounced to prevent instant menu closure on the capture press.
- Gameplay features remain enabled by default and continue to be developed independently.

**Validation:** awaiting V0.2 in-game test.


### V0.3A

- Cumulative from V0.2A.
- Added binary audit for the game-specific `ui.HideHud` CVar.
- Added semantic resolver anchored to the UTF-16 CVar name.
- No raw fixed RVA is used as the runtime resolver.
- Added native getter hook instead of writing the CVar.
- F1 now toggles runtime HUD hidden/visible state.
- Overlay Features tab exposes current HUD Hidden runtime state.
- Native game HUD-hide state is preserved with `nativeHidden || modHidden`.
- Reset Defaults restores the mod HUD runtime state to visible.
- Menu-key rebinding from V0.2A is preserved.
- Remaining Movement Speed / Action Recovery / Skip Intro / Third Person hooks
  are still pending.

**Validation:** awaiting first V0.3A in-game test.


## V0.4A — On-foot Movement Speed

**Status: TEST CANDIDATE**

Cumulative from V0.3A.

### Binary audit

The game exposes the relevant Mayhem functions through UE4 reflection:

```text
GetMaxSpeed
GetDefaultMaxSpeed
GetMayhemMovementComponent
AddSpeedLimitOverride
StopOverridingMaxSpeed
ESpeedModType::ADD_VALUE
ESpeedModType::ADD_MOD
ESpeedModType::MULTIPLY_MOD
```

For the audited executable, the `GetMaxSpeed` Blueprint exec wrapper calls the
native helper at RVA `0x56FBC0`. That helper retrieves the Mayhem movement
component from character offset `+0xA20` and dispatches its virtual max-speed
query.

Direct gameplay callers of this helper were also found outside the reflection
wrapper, confirming that it participates in real movement calculations.

### Player-only filtering

The feature must not speed enemies or NPCs.

The hook therefore applies the multiplier only when the character's inherited
`APawn::IsLocallyControlled` virtual returns true.

The audited UE4 wrapper dispatches this virtual through vtable offset:

```text
0x680
```

### Horse exclusion

Horse speed is a separate TODO and must remain independently tunable.

`AMayhemPlayerCharacter::IsHorseActive` was audited and checks:

```asm
cmp qword ptr [rcx + 0xE70], 0
setne al
```

V0.4A therefore leaves `GetMaxSpeed` unchanged while a horse mount is active.

### Runtime behavior

- Movement Speed is enabled by default.
- Default multiplier: **1.15x**.
- F2 toggles Movement Speed ON/OFF.
- Overlay slider range: **1.00x to 2.50x**.
- Slider changes save immediately to the INI.
- No global TimeScale modification.
- No enemy/NPC speed modification.
- No horse-speed modification.
- Resolver failure is fail-open and leaves vanilla movement untouched.

**Validation:** awaiting first V0.4A in-game test.


## V0.5A — Action Recovery / MOVE interrupt delay

**Status: TEST CANDIDATE**

Cumulative from V0.4A.

### What was found

The delay reported by the user is represented directly in
`UMayhemPlayerAbilityComponent` as:

```text
MoveInterruptDelaySec
```

The generated reflection data confirms:

```text
UMayhemPlayerAbilityComponent object size: 0x118
MoveInterruptDelaySec offset:              0x110
runtime elapsed timer offset:              0x114
```

A unique native action-gate function was then identified at audited RVA:

```text
0x5B3150
```

Its opening logic is equivalent to:

```text
if action == ECharacterActions::MOVE (0x1D):
    if MoveInterruptDelaySec > elapsed:
        reject movement
```

This is the exact post-action movement lock targeted by the feature.

### V0.5A implementation

The mod does **not** speed up animations and does **not** change global
TimeScale.

For a MOVE action only:

```text
effectiveDelay = native MoveInterruptDelaySec / ActionRecoveryMultiplier
```

The ASI temporarily substitutes that effective value only while the original
native action-gate function executes, then immediately restores the object's
original value.

This preserves every other native condition checked by the game.

### Runtime behavior

- Action Recovery is enabled by default.
- Default multiplier: **2.00x**.
- F3 toggles Action Recovery ON/OFF.
- Overlay slider range: **1.00x to 5.00x**.
- Slider changes are saved immediately.
- 2.00x means the MOVE lock lasts half as long.
- 5.00x means the MOVE lock lasts one fifth as long.
- No animation-speed change.
- No attack-speed change.
- No global TimeScale change.
- No permanent write to `MoveInterruptDelaySec`.
- Signature mismatch is fail-open and leaves vanilla behavior untouched.

### Additional input fix

V0.5A also fixes a hotkey-state issue inherited by Movement Speed:

- F2 can now re-enable Movement Speed after it has been toggled OFF.
- F3 can likewise toggle Action Recovery both OFF and back ON.
- Toggle HUD still respects its separate feature-enable checkbox.

**Compilation:** PASS.

**GitHub Actions run:** `37235986567`.

Compiled binary hashes:

```text
dxgi.dll
be75f8f2a12b1e44482ae5ffa76708e788eaddd64fdd591fff621b89a1e7277b

DarksidersGenesisMod.asi
10ee522195dd391da435249cdc9d4c1388de41c685db845a5bee7f1d8b7a6002
```

**Validation:** awaiting first V0.5A in-game test.
