DARKSIDERS GENESIS - PRE-ORDER MOUNTS PC UNLOCKER
Version 1.0.0
=================================================

WHAT THIS MOD DOES
------------------
Unlocks the two console pre-order mount skins that are already present
in the PC version of Darksiders Genesis:

- Rampage for War
- Despair for Strife

No game executable is modified. The mod only overrides the two cooked
skin DataTables responsible for the pre-order DLC lock.

INSTALLATION
------------
1. Copy:
   DarksidersGenesis_PreOrderMounts_P.pak

2. Paste it into:
   Darksiders Genesis\ProjectMayhem\Content\Paks\

3. Launch the game normally.

UNINSTALLATION
--------------
Delete DarksidersGenesis_PreOrderMounts_P.pak from the game's Paks folder.

TECHNICAL SUMMARY
-----------------
The PC build already contains:
- Rampage_Blueprint
- Despair_Blueprint
- War_Rampage_Mesh
- Strife_Despair_Mesh
- the matching icons, materials and VFX
- EMayhemDlc::PreOrder

The mod changes only four boolean bytes:

MayhemSkin_Despair:
  bUnlockedByDefault: false -> true
  bUnlockedByDlc:      true  -> false

RuinSkin_Rampage:
  bUnlockedByDefault: false -> true
  bUnlockedByDlc:      true  -> false

TESTED
------
Confirmed working in-game on the PC version.

FILES
-----
DarksidersGenesis_PreOrderMounts_P.pak

SHA-256
-------
a4f8dbacee5ddb424ea9e0cca5968a29ce54abed58c849f6a6317670a91f6e3f  DarksidersGenesis_PreOrderMounts_P.pak

PROJECT
-------
https://github.com/DeadneM/Darksiders-Genesis-Preorder-Mounts-PC

CREDITS
-------
Research, testing and mod project: DeadneM
Built with assistance from ChatGPT.

DISCLAIMER
----------
Darksiders Genesis and all related names, assets and trademarks belong
to their respective owners. This is an unofficial fan-made compatibility/
unlock mod and is not affiliated with or endorsed by the game's developers
or publishers.
