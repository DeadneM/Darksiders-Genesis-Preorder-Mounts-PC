# Technical notes

## Goal

Restore the two console pre-order mount skins that are already shipped inside the PC build of **Darksiders Genesis**.

## Discovery

The Windows executable and cooked game data contain the original pre-order DLC framework, including:

```text
EMayhemDlc::PreOrder
bUnlockedByDlc
DLCType
```

The PC skin tables contain both hidden entries:

```text
RuinSkin_Rampage
MayhemSkin_Despair
```

Their actor overrides point to complete cooked Blueprints:

```text
/Game/Blueprints/Player/Horse/Rampage_Blueprint.Rampage_Blueprint_C
/Game/Blueprints/Player/Horse/Despair_Blueprint.Despair_Blueprint_C
```

Those Blueprints in turn use the dedicated meshes:

```text
War_Rampage_Mesh
Strife_Despair_Mesh
```

and the corresponding cooked VFX/material assets are also present in the PC data.

This means the console bonus does not need to be ported from console files. The PC release already contains it and only keeps it behind the original pre-order DLC unlock condition.

## Minimal patch

No executable patch is required.

Only the two cooked DataTables are overridden:

```text
ProjectMayhem/Content/Data/ArtData/Skins/RuinSkinTable.uasset
ProjectMayhem/Content/Data/ArtData/Skins/RuinSkinTable.uexp

ProjectMayhem/Content/Data/ArtData/Skins/MayhemSkinTable.uasset
ProjectMayhem/Content/Data/ArtData/Skins/MayhemSkinTable.uexp
```

The `.uasset` files are byte-identical to the originals. Only four bytes in the two `.uexp` files are changed.

### MayhemSkinTable.uexp

```text
Offset 0x03B3: 00 -> 01   MayhemSkin_Despair bUnlockedByDefault
Offset 0x0478: 01 -> 00   MayhemSkin_Despair bUnlockedByDlc
```

### RuinSkinTable.uexp

```text
Offset 0x03AD: 00 -> 01   RuinSkin_Rampage bUnlockedByDefault
Offset 0x0472: 01 -> 00   RuinSkin_Rampage bUnlockedByDlc
```

No names, paths, meshes, icons, strings, materials, Blueprints or executable code are changed.

## Result

The game now treats both hidden mount skins as normal default-unlocked skins:

- Rampage for War
- Despair for Strife

The mod has been confirmed working in-game on PC.

## Package

The mod is shipped as:

```text
DarksidersGenesis_PreOrderMounts_P.pak
```

PAK mount point:

```text
../../../
```

Archive version: UE4 PAK v3

Files inside the archive:

```text
ProjectMayhem/Content/Data/ArtData/Skins/MayhemSkinTable.uasset
ProjectMayhem/Content/Data/ArtData/Skins/MayhemSkinTable.uexp
ProjectMayhem/Content/Data/ArtData/Skins/RuinSkinTable.uasset
ProjectMayhem/Content/Data/ArtData/Skins/RuinSkinTable.uexp
```

## v1.0.0 hash

```text
a4f8dbacee5ddb424ea9e0cca5968a29ce54abed58c849f6a6317670a91f6e3f  DarksidersGenesis_PreOrderMounts_P.pak
```
