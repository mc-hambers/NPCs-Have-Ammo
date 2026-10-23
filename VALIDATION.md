# NPCs Have Ammo — validation

## Automated checks

CoreBehavior covers every schedule and follower multiplier, cap ranges, shortfall-only initial fills, floor-half refills, once-per-cycle city credit including immediate expiry, follower progress preservation, no catch-up batches, stable caps, ammo kind/cost/damage/species selection, conservative player-gift quantities, reset/save coalescing, deterministic save/load, independent save branches, truncation/future-schema rejection, and corrupted cap/deadline rejection. The exact check count is recorded in validation-results.json.

The x64 combined SE/AE DLL is compiled with CommonLibSSE-NG, then its SKSE exports, version/name metadata, and Windows loader are checked without invoking SKSEPlugin_Load. Reset vtable slot 0x8A and the global option's exact name are checked against the installed 1.7.104 executable/Address Library, read-only. Mapping IDs are derived from installed masters; no game data is modified. Install/source ZIP integrity, packaged DLL equality, and hashes are checked.

These results do not establish Skyrim gameplay behavior. No DLL was installed into the game or Skyrim launched by these automated tools.

## Required in-game acceptance

1. Check NPCsHaveAmmo.log for successful load, all relevant fallbacks, and the forced ammo-consumption boolean. Shoot/observe arrows and bolts actually decreasing. Inspect ranged creatures and turrets too: the global engine option is broader than this mod's restock eligibility.
2. Compare ordinary hunters, civilians with bows, guards, soldiers, bandits/Forsworn, Falmer, draugr/skeleton archers, and mod-added ranged NPCs. Hunters/sport should use 18–24 and battle profiles 40–60. Crossbow-only actors must not get an extra arrow stock. Check bound/player-only weapons, dead actors, children, and summons.
3. Verify initial fill subtracts all current ammunition, including gifts, without deleting above-cap inventories. Later fills add only half-cap or the remaining deficit. Confirm guards use 0.5 day in cities, 1 day outside; camp bandits 1 day; other city residents 1–1.5 days; wilderness archers 2–3 days.
4. Recruit a victim/archer: remaining time should scale to twice the normal interval. Dismissal must preserve progress at normal speed. Verify city guards following the player use 1 day, not an instant restart.
5. Follow an NPC crossing city boundaries and city interior transitions. First outside-to-city entry subtracts exactly one game day and can expire immediately. Repeat entry/scan notifications cannot reapply the credit within the same cycle. Test moving between camp/city/wilderness contexts and overdue timers.
6. Leave a cell for several weeks, then return. At most one refill occurs per kind, followed by a new interval. No refill is triggered merely by reopening inventory or reloading a save before the deadline.
7. Give an NPC rare ammo with no native stock/list source, check gift protection in the log, save/reload, and verify it does not become a type source. Test mixed native/gifted stacks, firing, dropping, transfer/removal, and both ammo kinds. A native list may independently permit the gifted type. Historical pre-installation gifts are not distinguishable.
8. Test current leveled-list level thresholds, nested/cyclic custom lists, unavailable/invalid projectiles, quest-tagged inventory ammo, species fallbacks, custom ammo, and cost ties. Confirm actual runtime list/weapon APIs and NPC usability, which pure helper tests cannot prove.
9. Trigger genuine full and leveled-only inventory resets. Caps should remain stable, current inventory should be subtracted again, surviving player gifts should remain conservatively protected, and duplicate hook/event notifications should not double-fill. Save immediately before deferred event processing to check persistence.
10. Run alongside Dynamic Pickpocket Loot and the user's full SKSE/AI/ammo/distribution stack. Test every intended SE/AE runtime separately. Global-ammo-bypass mods, special projectile weapons, companion private inventories, and nonstandard transfers require explicit verification.

## Boundaries

The existing live evidence for Dynamic Pickpocket Loot is not evidence for this separate DLL. Names/records/settings found in game files and successful compilation do not prove engine consumption semantics, event timing, leveled-list simulation, gift provenance, or custom creature compatibility. Read README.md for implemented defaults and deliberate scope limits.

Follower correction: automated coverage includes all doubled intervals, recruit/dismiss progress preservation, old 0.25-day timer rescaling, and six-day timer serialization. Save schema remains 1.

Release 1.0.1 updates public/binary version metadata without changing the mod name, DLL filename, or saved identities. The corrected follower schedules remain twice the normal interval.

## 1.0.2 follower cap transitions

Automated regressions cover civilian-to-follower combat caps, restoration of the exact default cap, repeated recruit/dismiss stability, no immediate refill or timer/credit reset, above-cap retention after dismissal, first encounter while following, existing combat profiles, both caps surviving save/load/reset, malformed combat caps, and an independently assembled schema-1 migration fixture. Current save schema is 2; older records migrate without a forced generation cycle. Verify these transitions and the new cap-change log entries in Skyrim, for both arrows and bolts. Previous automated results are not gameplay evidence.

## 1.0.3 carried-stock priority and weapon requirement

Core tests cover equipped stock, larger unprotected stacks, priority over cheaper native lists, carried species variants, invalid ammo, and arrow/bolt separation. Save schema and encoding are unchanged.

Unrun Skyrim checks: remove all bows/crossbows from hunter and archer-class NPCs and confirm no restocks; restore a compatible weapon and verify saved caps/timers resume without rerolls. Test bow-only/crossbow-only/both-weapon inventories and weapon removal during callback-driven changes. Confirm equipped or larger unprotected current stacks beat lists/fallbacks, and gifts remain protected.
