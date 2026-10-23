# NPCs Have Ammo

Skyrim SE/AE mod — source version **1.0.3**.

## Version 1.0.3

Restocks strictly prefer valid unprotected ammunition already carried: equipped stock first, then the largest current stack, then native lists and fallbacks. Each channel now requires an actual compatible bow/crossbow in inventory, rechecked immediately before filling. Hunter/archer roles no longer substitute for a missing weapon.

Removed the unused archer-eligibility map and avoid native-list scans for actors without ranged weapons. Save schema 2, IDs, caps, timers and player-gift protection are retained; existing saves need no reset. Build and automated checks pass; Skyrim acceptance remains untested.

## Complete source

[Download all corresponding source and build files](NPCs-Have-Ammo-source.zip). The archive includes the mod implementation, configuration, scripts, dependency source, build instructions, and original license notices. Native mod code is also browsable in [src/](src/).

## Precompiled CommonLib

[Download the matching precompiled CommonLib](CommonLibSSE.lib.xz). This is a development dependency; do not install it into Skyrim.

Extract the complete source archive, then run:

```text
python Restore-CommonLib.py <path-to-extracted-source-folder>
```

Keep [Restore-CommonLib.py](Restore-CommonLib.py) beside CommonLibSSE.lib.xz. The helper decompresses the library to the correct project path and verifies its SHA-256. Python 3.11 or later is required. Follow the original project build instructions for the MSVC compiler, SDK, and Papyrus tools. CMake projects normally compile CommonLib from the included source; the precompiled library is additionally provided for development.

## License

Copyright (C) 2026 Maxmc.

The original mod code is free software: you may redistribute it and/or modify it under the GNU General Public License, version 3 or (at your option) any later version.

This program is distributed WITHOUT ANY WARRANTY, including the implied warranties of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See [LICENSE.txt](LICENSE.txt) for the full terms. Third-party code retains its original copyrights, license terms, and applicable exceptions, preserved in the complete source archive.

---

# NPCs Have Ammo (arrow/bolt restock mod) - 1.0.3

A standalone SKSE plugin that enables NPC ammo consumption and gives eligible archers limited, inventory-aware arrow and crossbow-bolt supplies. Initial test release; Skyrim acceptance testing is outstanding. Dynamic Pickpocket Loot is neither required nor modified.

## Requirements and installation

- Skyrim Special Edition / Anniversary Edition on a supported PC runtime.
- Matching SKSE64 and Address Library for SKSE Plugins.
- Microsoft Visual C++ 2015�2022 x64 runtime.
- Dawnguard supplies the standard bolt fallbacks and is included with Skyrim SE/AE.

Install NPCs-Have-Ammo-v1.0.3.zip through your mod manager. The only runtime file is Data/SKSE/Plugins/NPCsHaveAmmo.dll. No ESP/ESM/ESL slot, SPID, SkyUI, or MCM is used. Launch through SKSE. Diagnostics are written to Documents/My Games/Skyrim Special Edition/SKSE/NPCsHaveAmmo.log (or the applicable SKSE log directory for your distribution).

Build targets: SE 1.5.97; AE 1.6.317, 318, 323, 342, 353, 629, 640, 659, 1130, 1170, 1179; 1.7.99 and 1.7.104. Other executables and VR are rejected. These are compatibility targets, not proof of in-game operation on each version.

## NPC ammo consumption

The plugin forces the engine boolean bForceNPCsUseAmmo:Combat to true at data load/game entry and checks it periodically while active. It does not edit Skyrim.ini, change weapon records, or write an ESP. If the setting cannot be resolved as a boolean, restocking is paused and the log explains why. The engine-wide setting affects NPCs beyond this mod's eligible restock population, including special creature/quest weapons; test those with your mod list. Infinite-ammo patches that bypass consumption can defeat the intended limited-ammo behavior.

## Eligibility and caps

An eligible living adult NPC must currently carry a usable bow for arrow restocks or a usable crossbow for bolt restocks. Hunter factions and archer classes alone no longer qualify. Weapons marked player-only, bound, or deleted do not qualify. Native weapon lists do not substitute for an actual weapon in inventory. Each channel is checked again from the fresh inventory snapshot before refill; an NPC with both weapon types can receive both ammunition kinds.

- Hunter/sport/civilian profiles receive a saved cap of 18�24 per ammunition kind.
- Guard/soldier/combat/bandit profiles receive a saved cap of 40�60 per kind.
- Known Falmer, draugr/skeleton, and Forsworn profiles use the battle cap.
- Active followers participate and take twice the normal time between restocks.
- The player, children, dead/deleted/disabled actors, summons, and commanded/reanimated actors do not receive additions. Restocking waits for the actor to be loaded with 3D. Custom NPCs using normal bow/crossbow inventory records can participate; special projectile-only creatures are not automatically treated as bow users.

Each reference and ammo kind stores its default occupation cap. Active followers always use the combat allowance of 40�60, even when their native occupation is hunter/sport/civilian. An already-combat NPC keeps its existing combat cap; other followers receive one saved combat-cap roll. Dismissal restores the exact saved default cap. Recruiting/dismissing repeatedly does not reroll either cap or force a refill. Existing ammunition above a restored lower cap is left alone. Caps are synchronized when the actor is observed by the periodic/event scan, including before a refill is due. No items are removed or converted.

## Initial supply and refills

First initialization fills only the shortfall up to the cap, including existing/player-given ammunition in the count. Each later due timer adds half the saved cap, rounded down, limited to the current shortfall. For example, a cap of 45 permits at most 22 arrows per refill. Gold, other inventory items, and the opposite ammunition kind are untouched.

Only one refill per kind is processed after a long absence; there are no accumulated missed batches. A full quiver receives nothing and starts its next timer. If no usable ammo type or fallback exists for a needed addition, it remains pending. Real inventory resets allow another initial fill while retaining caps and conservatively preserving any player gifts that survived the reset.

## Restock schedules

All intervals use elapsed in-game days, so sleeping, waiting, and travel that advances game time count. Time outside the game does not.

- Ordinary people in cities: 1�1.5 days.
- Guards in cities: 0.5 day.
- Guards outside cities: 1 day.
- Bandits currently in bandit-camp locations: 1 day.
- Other archers/hunters outside cities: 2�3 days.

The random position within an interval is selected once per cycle and saved. Guard schedules take priority over camp schedules, then bandit camps, then general city/wilderness rules.

Followers use twice those intervals: city guards 1 day, outside guards/camp bandits 2 days, other city residents 2�3 days, and wilderness archers/hunters 4�6 days. Recruitment/dismissal and location changes preserve the completed portion of the timer rather than restarting it. An already-due timer stays due when moving to a slower environment. Saves made with the earlier fast-follower build retain their caps and progress; active timers are rescaled to the corrected slower rate when observed. Follower cap changes affect the next due refill quantity (half the active cap); recruitment does not itself force a refill. First-ever initialization still fills the active cap.

The first observed outside-to-city entry on an active timer subtracts one additional game day, once per cycle. It is allowed to make the timer immediately due. Repeated scans/reentries during that timer cannot subtract another day. A timer starting inside a city receives its ordinary city interval without an automatic subtraction. City status follows LocTypeCity and parent locations, including interiors; towns/villages without that keyword do not automatically count. Bandit camps follow LocTypeBanditCamp for actors in the bandit/Forsworn factions. Unloaded historical movement cannot be reconstructed; the discount is applied when the plugin observes the transition.

## Smart ammo selection

Selection strictly prioritizes valid, unprotected ammunition already carried by the NPC: currently equipped carried ammo first, then the largest current stack of that kind. Existing valid carried ammo is preserved as the restock type even when it differs from the species fallback. Only when no suitable carried stock exists are level-eligible native inventory/outfit lists considered, then species/occupation fallbacks. Within an equal source/stack priority, lower value wins, followed by higher damage and stable FormID. Invalid, quest-only, deleted or zero-damage ammo is excluded. Player gifts count toward the cap but do not independently seed replenishment. Arrow and bolt channels never cross.

Generic sport/hunter fallbacks prefer iron arrows, with steel as backup. Battle profiles prefer steel, with iron as backup. Crossbows use steel bolts, then dwarven bolts if necessary. Species with no distinct vanilla bolt type use ordinary compatible bolts. Modded ammunition already provided through valid native inventories/lists is supported without a separate patch; the mod does not populate its own external item configuration.

## Player-given ammunition

Player-to-NPC ammunition transfers observed while this plugin is running are saved as protected quantities. They count toward the cap, are never removed/upgraded by this plugin, and do not seed replenishment on their own. If an NPC's native list independently permits that same ammo form, it can still be selected from that independent source.

Skyrim merges identical arrows/bolts into stacks, so protection is conservative by quantity. After consumption or removal, it is clamped to the remaining stack rather than claiming an exact physical identity. Gifts made before installation, restored only by another mod's private mechanism, or inserted without a normal transfer event cannot reliably be identified. Keep matched ESS/SKSE saves together to retain protection. Normal game combat can consume gifted ammunition; this mod does not make gifts infinite or prevent firing them.

## Saves, compatibility, and performance

Caps, both ammo timers, saved interval randomness, city-entry credit, and protected gift quantities follow the current save. Form IDs are remapped when loading. Save schema 2 retains default and follower combat caps. Schema-1 saves migrate automatically, preserving their existing default caps, timers, random state, and gifts. Following/dismissal then selects the appropriate saved cap without reopening initialization. Older binaries cannot read schema 2 and fail closed. Unreadable/future state disables restocking rather than discarding gift protection. Genuine reset notifications are coalesced and flushed before saving. Timers and protection do not reset merely because of a mod update or follower transition. No uninstall/reinstall persistence guarantee is made.

A periodic queued main-thread scan and inventory/location/reset events drive the system. Engine inventory operations do not run on the background timer thread. Already-initialized actors skip inventory/leveled-list scans until due, unless a weapon transfer requires rechecking. There is no per-frame ammo generation. Adds are bounded by a fresh inventory snapshot before each kind is filled.

The DLL has its own serialization ID and uses no Dynamic Pickpocket Loot code at runtime. Both mods can chain the same inventory-reset virtual-function hooks, but their combined operation and other native hook conflicts still need in-game testing. Other distributors may add ammo afterward; this mod does not remove excess stock supplied by them. Do not assume compatibility with every custom ammunition/AI framework from compilation alone.

## Building and source

The source ZIP includes code, tests, mapping notes, CommonLibSSE-NG, corresponding dependency source archives/port recipes, and third-party notices. No game masters, saves, compiler binaries, or game assets are included.

Use Visual Studio 2022 C++ tools, Windows SDK, CMake/Ninja, and the vcpkg dependencies in vcpkg.json. scripts/bootstrap.ps1 can prepare portable tools; scripts/build.ps1 -DependenciesOnly prepares dependencies. scripts/build.ps1 accepts -ToolRoot and -DependencyInstall. In this workspace it can reuse the sibling Pickpocketing-Loot toolchain/dependency install without editing that mod's source or release. Standalone source builds can use their own tools and dependency paths.

License: GPL-3.0-or-later with CommonLibSSE-NG exceptions in EXCEPTIONS.md. See third-party notices.

## 1.0.1

Corrected follower restocking to twice the normal interval, with saved timer progress preserved. The mod name, DLL filename, serialization identifiers, and save schema remain unchanged. This release supersedes the corrected 1.0.0 preview.

## 1.0.2

Active followers always use a saved combat cap; dismissal restores their original occupation cap. Combat fallback ammunition is preferred while following. Timer intervals remain twice as long as normal and preserve progress. Version/name/DLL identities remain stable apart from the version increment; save schema 2 migrates schema-1 records without forcing a refill.

## 1.0.3

Existing ammo now has strict priority over native lists and fallbacks. An actual carried bow/crossbow is mandatory, including a fresh check before each refill. Removed the unused archer-role eligibility map and moved native-list scanning after the weapon check. Serialization schema 2, identifiers, existing caps, gifts and timers are unchanged; existing states are retained when an NPC loses a weapon and resume when a compatible weapon returns. No forced refill is introduced by updating.
