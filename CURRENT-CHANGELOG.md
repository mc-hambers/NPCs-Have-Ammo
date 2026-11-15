# NPCs Have Ammo 1.0.8

Changes since 1.0.5:

- Moved initial NPC discovery to save loading; load/unload events replace repeated full scans during play.
- Process only changed NPCs and due restocks in small, deduplicated batches.
- Preserved existing restock rules, saved timers and continued restocking while remaining in one area.

Existing saves are supported; no new game or save cleaner is required. Build and automated checks passed. These checks do not prove every gameplay path or hitch improvement.
