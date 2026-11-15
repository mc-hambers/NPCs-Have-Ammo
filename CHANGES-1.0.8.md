# NPCs Have Ammo 1.0.8

- Initial actor discovery now runs in save-load completion before gameplay tasks are queued.
- Load/unload events maintain tracking, with due/context checks during play.
- Deduplicated actor work uses small slices with a count and elapsed-time admission limit.
- Saved state formats and existing game-day deadlines are preserved.

Validation: 63,787 automated behavior checks and native build passed. In-game performance and gameplay acceptance remain unverified.
