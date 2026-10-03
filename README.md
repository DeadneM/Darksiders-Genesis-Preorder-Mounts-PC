# Darksiders Genesis – Pre-Order Mounts PC Unlocker

Unlocks the two console pre-order horse skins in the PC version of **Darksiders Genesis**:

- **Rampage** for War
- **Despair** for Strife

The assets are already present in the PC game files. This mod only changes the skin unlock data so the two mounts are available without the unavailable console pre-order entitlement.

## Installation

1. Download `DarksidersGenesis_PreOrderMounts_P.pak`.
2. Copy it to:

   ```text
   Darksiders Genesis\ProjectMayhem\Content\Paks\
   ```

3. Launch the game normally.

To uninstall, delete the mod `.pak`.

## What the mod changes

Only two DataTables are overridden:

```text
ProjectMayhem/Content/Data/ArtData/Skins/RuinSkinTable
ProjectMayhem/Content/Data/ArtData/Skins/MayhemSkinTable
```

The following four boolean values are changed:

```text
RuinSkin_Rampage
  bUnlockedByDefault: false -> true
  bUnlockedByDlc:      true  -> false

MayhemSkin_Despair
  bUnlockedByDefault: false -> true
  bUnlockedByDlc:      true  -> false
```

Everything else in the original tables is preserved.

## Technical discovery

The PC build already contains the complete pre-order content path:

- `EMayhemDlc::PreOrder`
- `RuinSkin_Rampage`
- `MayhemSkin_Despair`
- `Rampage_Blueprint`
- `Despair_Blueprint`
- `War_Rampage_Mesh`
- `Strife_Despair_Mesh`
- the corresponding icons, materials and VFX

The console bonus was therefore not a missing asset port. It was a dormant PC content unlock.

See [docs/TECHNICAL.md](docs/TECHNICAL.md) for the detailed notes.

## Tested

Confirmed working in-game on PC.

## Version

**v1.0.0**

SHA-256:

```text
a4f8dbacee5ddb424ea9e0cca5968a29ce54abed58c849f6a6317670a91f6e3f  DarksidersGenesis_PreOrderMounts_P.pak
```

## Credits

Research, testing and mod project: **DeadneM**

Built with assistance from ChatGPT.

## Disclaimer

Darksiders Genesis and all related names, assets and trademarks belong to their respective owners. This is an unofficial fan-made compatibility/unlock mod and is not affiliated with or endorsed by the game developers or publishers.
