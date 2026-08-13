# Parasite Eve 2 Decompilation

A work-in-progress **matching decompilation** of *Parasite Eve II* for the Sony PlayStation.

The goal is to recover readable C that, when compiled with a period-correct toolchain, produces a binary **byte-identical** to the original executable.

| Item | Value |
|---|---|
| Platform | PlayStation (PSX / PS1) |
| Target | USA main executable `SLUS_010.42` |
| Disks (USA) | `SLUS-01042` (Disk 1), `SLUS-01055` (Disk 2) |
| Compiler | GCC 2.8.1 (PSX `cc1`) + maspsx |
| Libraries | Psy-Q 4.5 object files (see `lib/`) |
| License (project code) | [CC0 1.0](LICENSE) |

> **You must own the game.** This repository does not include ROMs, disc images, or copyrighted assets. Obtain a legal dump of your own discs.

## Status

### USA

| Component | Status |
|---|---|
| **Main executable** (`SLUS_010.42`) | In progress — partially decompiled; matched C rebuilds byte-identically, with remaining functions still in assembly (`INCLUDE_ASM`) |

<details>
<summary><strong>Overlays</strong> (448 — all not started)</summary>

Descriptions are best-effort: source path and load address come from `assets/USA/stages.json` (`room_pkg` entries). Friendly names live in `tools/peassets/names.py` (`NAMES`); unnamed unique overlays use `pe2pkg_N` store stems. Role text is inferred from known names or stage/room encoding.

| Overlay | Description | Status |
|---|---|---|
| `gameplay` | Resident in-game overlay (actors/anim, enemies, combat/field, inventory) — source `stage0/gameplay/gameplay.pe2pkg` — load `0x80093800` — 350.0 KiB | Split (asm only) |
| `title` | Title / demo / main menu — source `stage0/title/title.pe2pkg` — load `0x80093800` — 4.0 KiB | Split |
| `stage0_file100300_3` | Stage 0 overlay (id stage0/file100300/3.pe2pkg) — source `stage0/file100300/3.pe2pkg` — load `0x80131E20` — 68.0 KiB | Not started |
| `stage0_file100400_3` | Stage 0 overlay (id stage0/file100400/3.pe2pkg) — source `stage0/file100400/3.pe2pkg` — load `0x80131E20` — 62.0 KiB | Not started |
| `stage0_file100500_3` | Stage 0 overlay (id stage0/file100500/3.pe2pkg) — source `stage0/file100500/3.pe2pkg` — load `0x80131E20` — 2.0 KiB | Not started |
| `stage0_file100700_3` | Stage 0 overlay (id stage0/file100700/3.pe2pkg) — source `stage0/file100700/3.pe2pkg` — load `0x80131E20` — 22.0 KiB | Not started |
| `stage0_file101000_3` | Stage 0 overlay (id stage0/file101000/3.pe2pkg) — source `stage0/file101000/3.pe2pkg` — load `0x80131E20` — 2.0 KiB | Not started |
| `stage0_file101100_2` | Stage 0 overlay (id stage0/file101100/2.pe2pkg) — source `stage0/file101100/2.pe2pkg` — load `0x80131E20` — 64.0 KiB | Not started |
| `stage0_file101200_3` | Stage 0 overlay (id stage0/file101200/3.pe2pkg) — source `stage0/file101200/3.pe2pkg` — load `0x80131E20` — 20.0 KiB | Not started |
| `stage0_file101300_3` | Stage 0 overlay (id stage0/file101300/3.pe2pkg) — source `stage0/file101300/3.pe2pkg` — load `0x80131E20` — 2.0 KiB | Not started |
| `stage0_file101500_3` | Stage 0 overlay (id stage0/file101500/3.pe2pkg) — source `stage0/file101500/3.pe2pkg` — load `0x80131E20` — 28.0 KiB | Not started |
| `stage0_file101600_3` | Stage 0 overlay (id stage0/file101600/3.pe2pkg) — source `stage0/file101600/3.pe2pkg` — load `0x80131E20` — 54.0 KiB | Not started |
| `stage0_file101900_3` | Stage 0 overlay (id stage0/file101900/3.pe2pkg) — source `stage0/file101900/3.pe2pkg` — load `0x80131E20` — 68.0 KiB | Not started |
| `stage0_file10200_2` | Stage 0 overlay (id stage0/file10200/2.pe2pkg) — source `stage0/file10200/2.pe2pkg` — load `0x80115770` — 24.0 KiB | Not started |
| `stage0_file102000_3` | Stage 0 overlay (id stage0/file102000/3.pe2pkg) — source `stage0/file102000/3.pe2pkg` — load `0x80131E20` — 68.0 KiB | Not started |
| `stage0_file102100_3` | Stage 0 overlay (id stage0/file102100/3.pe2pkg) — source `stage0/file102100/3.pe2pkg` — load `0x80131E20` — 12.0 KiB | Not started |
| `stage0_file102300_3` | Stage 0 overlay (id stage0/file102300/3.pe2pkg) — source `stage0/file102300/3.pe2pkg` — load `0x80131E20` — 66.0 KiB | Not started |
| `stage0_file102400_3` | Stage 0 overlay (id stage0/file102400/3.pe2pkg) — source `stage0/file102400/3.pe2pkg` — load `0x80131E20` — 14.0 KiB | Not started |
| `stage0_file102500_3` | Stage 0 overlay (id stage0/file102500/3.pe2pkg) — source `stage0/file102500/3.pe2pkg` — load `0x80131E20` — 18.0 KiB | Not started |
| `stage0_file102600_3` | Stage 0 overlay (id stage0/file102600/3.pe2pkg) — source `stage0/file102600/3.pe2pkg` — load `0x80131E20` — 26.0 KiB | Not started |
| `stage0_file10300_2` | Stage 0 overlay (id stage0/file10300/2.pe2pkg) — source `stage0/file10300/2.pe2pkg` — load `0x80115770` — 24.0 KiB | Not started |
| `stage0_file10301_3` | Stage 0 overlay (id stage0/file10301/3.pe2pkg) — source `stage0/file10301/3.pe2pkg` — load `0x8011D1C0` — 40.0 KiB | Not started |
| `stage0_file10302_3` | Stage 0 overlay (id stage0/file10302/3.pe2pkg) — source `stage0/file10302/3.pe2pkg` — load `0x8011D1C0` — 42.0 KiB | Not started |
| `stage0_file10303_3` | Stage 0 overlay (id stage0/file10303/3.pe2pkg) — source `stage0/file10303/3.pe2pkg` — load `0x8011D1C0` — 44.0 KiB | Not started |
| `stage0_file10304_3` | Stage 0 overlay (id stage0/file10304/3.pe2pkg) — source `stage0/file10304/3.pe2pkg` — load `0x8011D1C0` — 40.0 KiB | Not started |
| `stage0_file10305_3` | Stage 0 overlay (id stage0/file10305/3.pe2pkg) — source `stage0/file10305/3.pe2pkg` — load `0x8011D1C0` — 42.0 KiB | Not started |
| `stage0_file10306_2` | Stage 0 overlay (id stage0/file10306/2.pe2pkg) — source `stage0/file10306/2.pe2pkg` — load `0x8011D1C0` — 40.0 KiB | Not started |
| `stage0_file10307_2` | Stage 0 overlay (id stage0/file10307/2.pe2pkg) — source `stage0/file10307/2.pe2pkg` — load `0x8011D1C0` — 40.0 KiB | Not started |
| `stage0_file10308_2` | Stage 0 overlay (id stage0/file10308/2.pe2pkg) — source `stage0/file10308/2.pe2pkg` — load `0x8011D1C0` — 40.0 KiB | Not started |
| `stage0_file10309_3` | Stage 0 overlay (id stage0/file10309/3.pe2pkg) — source `stage0/file10309/3.pe2pkg` — load `0x8011D1C0` — 40.0 KiB | Not started |
| `stage0_file10310_2` | Stage 0 overlay (id stage0/file10310/2.pe2pkg) — source `stage0/file10310/2.pe2pkg` — load `0x8011D1C0` — 40.0 KiB | Not started |
| `stage0_file10311_2` | Stage 0 overlay (id stage0/file10311/2.pe2pkg) — source `stage0/file10311/2.pe2pkg` — load `0x8011D1C0` — 42.0 KiB | Not started |
| `stage0_file10312_2` | Stage 0 overlay (id stage0/file10312/2.pe2pkg) — source `stage0/file10312/2.pe2pkg` — load `0x8011D1C0` — 48.0 KiB | Not started |
| `stage0_file10313_2` | Stage 0 overlay (id stage0/file10313/2.pe2pkg) — source `stage0/file10313/2.pe2pkg` — load `0x8011D1C0` — 46.0 KiB | Not started |
| `stage0_file10314_2` | Stage 0 overlay (id stage0/file10314/2.pe2pkg) — source `stage0/file10314/2.pe2pkg` — load `0x8011D1C0` — 44.0 KiB | Not started |
| `stage0_file10315_2` | Stage 0 overlay (id stage0/file10315/2.pe2pkg) — source `stage0/file10315/2.pe2pkg` — load `0x8011D1C0` — 46.0 KiB | Not started |
| `stage0_file10316_3` | Stage 0 overlay (id stage0/file10316/3.pe2pkg) — source `stage0/file10316/3.pe2pkg` — load `0x8011D1C0` — 44.0 KiB | Not started |
| `stage0_file10317_3` | Stage 0 overlay (id stage0/file10317/3.pe2pkg) — source `stage0/file10317/3.pe2pkg` — load `0x8011D1C0` — 48.0 KiB | Not started |
| `stage0_file10318_2` | Stage 0 overlay (id stage0/file10318/2.pe2pkg) — source `stage0/file10318/2.pe2pkg` — load `0x8011D1C0` — 40.0 KiB | Not started |
| `stage0_file10319_3` | Stage 0 overlay (id stage0/file10319/3.pe2pkg) — source `stage0/file10319/3.pe2pkg` — load `0x8011D1C0` — 44.0 KiB | Not started |
| `stage0_file10320_3` | Stage 0 overlay (id stage0/file10320/3.pe2pkg) — source `stage0/file10320/3.pe2pkg` — load `0x8011D1C0` — 44.0 KiB | Not started |
| `stage0_file10321_3` | Stage 0 overlay (id stage0/file10321/3.pe2pkg) — source `stage0/file10321/3.pe2pkg` — load `0x8011D1C0` — 44.0 KiB | Not started |
| `stage0_file10322_3` | Stage 0 overlay (id stage0/file10322/3.pe2pkg) — source `stage0/file10322/3.pe2pkg` — load `0x8011D1C0` — 54.0 KiB | Not started |
| `stage0_file10323_2` | Stage 0 overlay (id stage0/file10323/2.pe2pkg) — source `stage0/file10323/2.pe2pkg` — load `0x8011D1C0` — 52.0 KiB | Not started |
| `stage0_file10324_2` | Stage 0 overlay (id stage0/file10324/2.pe2pkg) — source `stage0/file10324/2.pe2pkg` — load `0x8011D1C0` — 40.0 KiB | Not started |
| `stage0_file10325_3` | Stage 0 overlay (id stage0/file10325/3.pe2pkg) — source `stage0/file10325/3.pe2pkg` — load `0x8011D1C0` — 48.0 KiB | Not started |
| `stage0_file10326_3` | Stage 0 overlay (id stage0/file10326/3.pe2pkg) — source `stage0/file10326/3.pe2pkg` — load `0x8011D1C0` — 48.0 KiB | Not started |
| `stage0_file10327_2` | Stage 0 overlay (id stage0/file10327/2.pe2pkg) — source `stage0/file10327/2.pe2pkg` — load `0x8011D1C0` — 52.0 KiB | Not started |
| `stage0_file10328_3` | Stage 0 overlay (id stage0/file10328/3.pe2pkg) — source `stage0/file10328/3.pe2pkg` — load `0x8011D1C0` — 48.0 KiB | Not started |
| `stage0_file10329_3` | Stage 0 overlay (id stage0/file10329/3.pe2pkg) — source `stage0/file10329/3.pe2pkg` — load `0x8011D1C0` — 52.0 KiB | Not started |
| `stage0_file10330_3` | Stage 0 overlay (id stage0/file10330/3.pe2pkg) — source `stage0/file10330/3.pe2pkg` — load `0x8011D1C0` — 42.0 KiB | Not started |
| `stage0_file10331_3` | Stage 0 overlay (id stage0/file10331/3.pe2pkg) — source `stage0/file10331/3.pe2pkg` — load `0x8011D1C0` — 42.0 KiB | Not started |
| `stage0_file10332_3` | Stage 0 overlay (id stage0/file10332/3.pe2pkg) — source `stage0/file10332/3.pe2pkg` — load `0x8011D1C0` — 42.0 KiB | Not started |
| `stage0_file103700_3` | Stage 0 overlay (id stage0/file103700/3.pe2pkg) — source `stage0/file103700/3.pe2pkg` — load `0x80131E20` — 24.0 KiB | Not started |
| `stage0_file103800_3` | Stage 0 overlay (id stage0/file103800/3.pe2pkg) — source `stage0/file103800/3.pe2pkg` — load `0x80131E20` — 18.0 KiB | Not started |
| `stage0_file10400_2` | Stage 0 overlay (id stage0/file10400/2.pe2pkg) — source `stage0/file10400/2.pe2pkg` — load `0x80115770` — 24.0 KiB | Not started |
| `stage0_file104000_3` | Stage 0 overlay (id stage0/file104000/3.pe2pkg) — source `stage0/file104000/3.pe2pkg` — load `0x80131E20` — 36.0 KiB | Not started |
| `stage0_file104100_3` | Stage 0 overlay (id stage0/file104100/3.pe2pkg) — source `stage0/file104100/3.pe2pkg` — load `0x80131E20` — 2.0 KiB | Not started |
| `stage0_file104400_2` | Stage 0 overlay (id stage0/file104400/2.pe2pkg) — source `stage0/file104400/2.pe2pkg` — load `0x80131E20` — 48.0 KiB | Not started |
| `stage0_file104500_2` | Stage 0 overlay (id stage0/file104500/2.pe2pkg) — source `stage0/file104500/2.pe2pkg` — load `0x80131E20` — 2.0 KiB | Not started |
| `stage0_file104600_3` | Stage 0 overlay (id stage0/file104600/3.pe2pkg) — source `stage0/file104600/3.pe2pkg` — load `0x80131E20` — 20.0 KiB | Not started |
| `stage0_file104900_3` | Stage 0 overlay (id stage0/file104900/3.pe2pkg) — source `stage0/file104900/3.pe2pkg` — load `0x80131E20` — 64.0 KiB | Not started |
| `stage0_file10500_2` | Stage 0 overlay (id stage0/file10500/2.pe2pkg) — source `stage0/file10500/2.pe2pkg` — load `0x80115770` — 24.0 KiB | Not started |
| `stage0_file105100_3` | Stage 0 overlay (id stage0/file105100/3.pe2pkg) — source `stage0/file105100/3.pe2pkg` — load `0x80131E20` — 48.0 KiB | Not started |
| `stage0_file105300_3` | Stage 0 overlay (id stage0/file105300/3.pe2pkg) — source `stage0/file105300/3.pe2pkg` — load `0x80131E20` — 38.0 KiB | Not started |
| `stage0_file105400_3` | Stage 0 overlay (id stage0/file105400/3.pe2pkg) — source `stage0/file105400/3.pe2pkg` — load `0x80131E20` — 36.0 KiB | Not started |
| `stage0_file105500_3` | Stage 0 overlay (id stage0/file105500/3.pe2pkg) — source `stage0/file105500/3.pe2pkg` — load `0x80131E20` — 26.0 KiB | Not started |
| `stage0_file105600_3` | Stage 0 overlay (id stage0/file105600/3.pe2pkg) — source `stage0/file105600/3.pe2pkg` — load `0x80131E20` — 68.0 KiB | Not started |
| `stage0_file105700_3` | Stage 0 overlay (id stage0/file105700/3.pe2pkg) — source `stage0/file105700/3.pe2pkg` — load `0x80131E20` — 70.0 KiB | Not started |
| `stage0_file107000_3` | Stage 0 overlay (id stage0/file107000/3.pe2pkg) — source `stage0/file107000/3.pe2pkg` — load `0x80131E20` — 40.0 KiB | Not started |
| `stage0_file107200_3` | Stage 0 overlay (id stage0/file107200/3.pe2pkg) — source `stage0/file107200/3.pe2pkg` — load `0x80131E20` — 2.0 KiB | Not started |
| `stage0_file107500_3` | Stage 0 overlay (id stage0/file107500/3.pe2pkg) — source `stage0/file107500/3.pe2pkg` — load `0x80131E20` — 2.0 KiB | Not started |
| `stage0_file107600_6` | Stage 0 overlay (id stage0/file107600/6.pe2pkg) — source `stage0/file107600/6.pe2pkg` — load `0x80131E20` — 10.0 KiB | Not started |
| `stage0_file109300_2` | Stage 0 overlay (id stage0/file109300/2.pe2pkg) — source `stage0/file109300/2.pe2pkg` — load `0x80131E20` — 2.0 KiB | Not started |
| `stage0_file110300_4` | Stage 0 overlay (id stage0/file110300/4.pe2pkg) — source `stage0/file110300/4.pe2pkg` — load `0x80131E20` — 26.0 KiB | Not started |
| `stage0_file110600_3` | Stage 0 overlay (id stage0/file110600/3.pe2pkg) — source `stage0/file110600/3.pe2pkg` — load `0x80131E20` — 66.0 KiB | Not started |
| `stage0_file110700_3` | Stage 0 overlay (id stage0/file110700/3.pe2pkg) — source `stage0/file110700/3.pe2pkg` — load `0x80131E20` — 32.0 KiB | Not started |
| `stage0_file110800_2` | Stage 0 overlay (id stage0/file110800/2.pe2pkg) — source `stage0/file110800/2.pe2pkg` — load `0x80131E20` — 26.0 KiB | Not started |
| `stage0_file111800_3` | Stage 0 overlay (id stage0/file111800/3.pe2pkg) — source `stage0/file111800/3.pe2pkg` — load `0x80131E20` — 26.0 KiB | Not started |
| `stage0_file113000_2` | Stage 0 overlay (id stage0/file113000/2.pe2pkg) — source `stage0/file113000/2.pe2pkg` — load `0x80131E20` — 26.0 KiB | Not started |
| `stage0_file113100_2` | Stage 0 overlay (id stage0/file113100/2.pe2pkg) — source `stage0/file113100/2.pe2pkg` — load `0x80131E20` — 56.0 KiB | Not started |
| `stage0_file120300_4` | Stage 0 overlay (id stage0/file120300/4.pe2pkg) — source `stage0/file120300/4.pe2pkg` — load `0x80131E20` — 44.0 KiB | Not started |
| `stage0_file120400_2` | Stage 0 overlay (id stage0/file120400/2.pe2pkg) — source `stage0/file120400/2.pe2pkg` — load `0x80131E20` — 38.0 KiB | Not started |
| `stage0_file120500_2` | Stage 0 overlay (id stage0/file120500/2.pe2pkg) — source `stage0/file120500/2.pe2pkg` — load `0x80131E20` — 20.0 KiB | Not started |
| `stage0_file121300_2` | Stage 0 overlay (id stage0/file121300/2.pe2pkg) — source `stage0/file121300/2.pe2pkg` — load `0x80131E20` — 34.0 KiB | Not started |
| `stage0_file123000_3` | Stage 0 overlay (id stage0/file123000/3.pe2pkg) — source `stage0/file123000/3.pe2pkg` — load `0x80131E20` — 2.0 KiB | Not started |
| `stage0_file123200_2` | Stage 0 overlay (id stage0/file123200/2.pe2pkg) — source `stage0/file123200/2.pe2pkg` — load `0x80131E20` — 16.0 KiB | Not started |
| `stage0_file123400_3` | Stage 0 overlay (id stage0/file123400/3.pe2pkg) — source `stage0/file123400/3.pe2pkg` — load `0x80131E20` — 2.0 KiB | Not started |
| `stage0_file135000_2` | Stage 0 overlay (id stage0/file135000/2.pe2pkg) — source `stage0/file135000/2.pe2pkg` — load `0x80131E20` — 2.0 KiB | Not started |
| `stage0_file135400_2` | Stage 0 overlay (id stage0/file135400/2.pe2pkg) — source `stage0/file135400/2.pe2pkg` — load `0x80131E20` — 42.0 KiB | Not started |
| `stage0_file135600_2` | Stage 0 overlay (id stage0/file135600/2.pe2pkg) — source `stage0/file135600/2.pe2pkg` — load `0x80131E20` — 28.0 KiB | Not started |
| `stage0_file136100_4` | Stage 0 overlay (id stage0/file136100/4.pe2pkg) — source `stage0/file136100/4.pe2pkg` — load `0x80131E20` — 40.0 KiB | Not started |
| `stage0_file136300_1` | Stage 0 overlay (id stage0/file136300/1.pe2pkg) — source `stage0/file136300/1.pe2pkg` — load `0x80131E20` — 28.0 KiB | Not started |
| `stage0_file140400_2` | Stage 0 overlay (id stage0/file140400/2.pe2pkg) — source `stage0/file140400/2.pe2pkg` — load `0x80131E20` — 2.0 KiB | Not started |
| `stage0_file141000_4` | Stage 0 overlay (id stage0/file141000/4.pe2pkg) — source `stage0/file141000/4.pe2pkg` — load `0x80131E20` — 34.0 KiB | Not started |
| `stage0_file142600_0` | Stage 0 overlay (id stage0/file142600/0.pe2pkg) — source `stage0/file142600/0.pe2pkg` — load `0x80131E20` — 14.0 KiB | Not started |
| `stage0_file142900_0` | Stage 0 overlay (id stage0/file142900/0.pe2pkg) — source `stage0/file142900/0.pe2pkg` — load `0x80131E20` — 18.0 KiB | Not started |
| `stage0_file143000_1` | Stage 0 overlay (id stage0/file143000/1.pe2pkg) — source `stage0/file143000/1.pe2pkg` — load `0x80131E20` — 10.0 KiB | Not started |
| `stage0_file143400_0` | Stage 0 overlay (id stage0/file143400/0.pe2pkg) — source `stage0/file143400/0.pe2pkg` — load `0x80131E20` — 10.0 KiB | Not started |
| `stage0_file143900_2` | Stage 0 overlay (id stage0/file143900/2.pe2pkg) — source `stage0/file143900/2.pe2pkg` — load `0x80131E20` — 70.0 KiB | Not started |
| `stage0_file146000_0` | Stage 0 overlay (id stage0/file146000/0.pe2pkg) — source `stage0/file146000/0.pe2pkg` — load `0x80131E20` — 10.0 KiB | Not started |
| `stage0_file146300_2` | Stage 0 overlay (id stage0/file146300/2.pe2pkg) — source `stage0/file146300/2.pe2pkg` — load `0x80131E20` — 48.0 KiB | Not started |
| `stage0_file150400_3` | Stage 0 overlay (id stage0/file150400/3.pe2pkg) — source `stage0/file150400/3.pe2pkg` — load `0x80131E20` — 32.0 KiB | Not started |
| `stage0_file151000_2` | Stage 0 overlay (id stage0/file151000/2.pe2pkg) — source `stage0/file151000/2.pe2pkg` — load `0x80131E20` — 34.0 KiB | Not started |
| `stage0_file160600_2` | Stage 0 overlay (id stage0/file160600/2.pe2pkg) — source `stage0/file160600/2.pe2pkg` — load `0x80131E20` — 34.0 KiB | Not started |
| `stage0_file160700_2` | Stage 0 overlay (id stage0/file160700/2.pe2pkg) — source `stage0/file160700/2.pe2pkg` — load `0x80131E20` — 44.0 KiB | Not started |
| `stage0_file160900_0` | Stage 0 overlay (id stage0/file160900/0.pe2pkg) — source `stage0/file160900/0.pe2pkg` — load `0x80131E20` — 40.0 KiB | Not started |
| `stage0_file161500_4` | Stage 0 overlay (id stage0/file161500/4.pe2pkg) — source `stage0/file161500/4.pe2pkg` — load `0x80131E20` — 36.0 KiB | Not started |
| `stage0_file200300_3` | Stage 0 overlay (id stage0/file200300/3.pe2pkg) — source `stage0/file200300/3.pe2pkg` — load `0x80149E20` — 68.0 KiB | Not started |
| `stage0_file200400_3` | Stage 0 overlay (id stage0/file200400/3.pe2pkg) — source `stage0/file200400/3.pe2pkg` — load `0x80149E20` — 62.0 KiB | Not started |
| `stage0_file200700_3` | Stage 0 overlay (id stage0/file200700/3.pe2pkg) — source `stage0/file200700/3.pe2pkg` — load `0x80149E20` — 22.0 KiB | Not started |
| `stage0_file20100_0` | Stage 0 overlay (id stage0/file20100/0.pe2pkg) — source `stage0/file20100/0.pe2pkg` — load `0x801D4000` — 6.0 KiB | Not started |
| `stage0_file201000_3` | Stage 0 overlay (id stage0/file201000/3.pe2pkg) — source `stage0/file201000/3.pe2pkg` — load `0x80149E20` — 2.0 KiB | Not started |
| `stage0_file201100_2` | Stage 0 overlay (id stage0/file201100/2.pe2pkg) — source `stage0/file201100/2.pe2pkg` — load `0x80149E20` — 64.0 KiB | Not started |
| `stage0_file201200_3` | Stage 0 overlay (id stage0/file201200/3.pe2pkg) — source `stage0/file201200/3.pe2pkg` — load `0x80149E20` — 20.0 KiB | Not started |
| `stage0_file201300_3` | Stage 0 overlay (id stage0/file201300/3.pe2pkg) — source `stage0/file201300/3.pe2pkg` — load `0x80149E20` — 2.0 KiB | Not started |
| `stage0_file201500_3` | Stage 0 overlay (id stage0/file201500/3.pe2pkg) — source `stage0/file201500/3.pe2pkg` — load `0x80149E20` — 28.0 KiB | Not started |
| `stage0_file201600_3` | Stage 0 overlay (id stage0/file201600/3.pe2pkg) — source `stage0/file201600/3.pe2pkg` — load `0x80149E20` — 54.0 KiB | Not started |
| `stage0_file201800_3` | Stage 0 overlay (id stage0/file201800/3.pe2pkg) — source `stage0/file201800/3.pe2pkg` — load `0x80149E20` — 2.0 KiB | Not started |
| `stage0_file201900_3` | Stage 0 overlay (id stage0/file201900/3.pe2pkg) — source `stage0/file201900/3.pe2pkg` — load `0x80149E20` — 2.0 KiB | Not started |
| `stage0_file20200_0` | Stage 0 overlay (id stage0/file20200/0.pe2pkg) — source `stage0/file20200/0.pe2pkg` — load `0x801D4000` — 2.0 KiB | Not started |
| `stage0_file202000_3` | Stage 0 overlay (id stage0/file202000/3.pe2pkg) — source `stage0/file202000/3.pe2pkg` — load `0x80149E20` — 68.0 KiB | Not started |
| `stage0_file202100_3` | Stage 0 overlay (id stage0/file202100/3.pe2pkg) — source `stage0/file202100/3.pe2pkg` — load `0x80149E20` — 12.0 KiB | Not started |
| `stage0_file202300_3` | Stage 0 overlay (id stage0/file202300/3.pe2pkg) — source `stage0/file202300/3.pe2pkg` — load `0x80149E20` — 66.0 KiB | Not started |
| `stage0_file202400_3` | Stage 0 overlay (id stage0/file202400/3.pe2pkg) — source `stage0/file202400/3.pe2pkg` — load `0x80149E20` — 14.0 KiB | Not started |
| `stage0_file202500_3` | Stage 0 overlay (id stage0/file202500/3.pe2pkg) — source `stage0/file202500/3.pe2pkg` — load `0x80149E20` — 18.0 KiB | Not started |
| `stage0_file202600_3` | Stage 0 overlay (id stage0/file202600/3.pe2pkg) — source `stage0/file202600/3.pe2pkg` — load `0x80149E20` — 26.0 KiB | Not started |
| `stage0_file202900_2` | Stage 0 overlay (id stage0/file202900/2.pe2pkg) — source `stage0/file202900/2.pe2pkg` — load `0x80149E20` — 40.0 KiB | Not started |
| `stage0_file203700_3` | Stage 0 overlay (id stage0/file203700/3.pe2pkg) — source `stage0/file203700/3.pe2pkg` — load `0x80149E20` — 24.0 KiB | Not started |
| `stage0_file203800_3` | Stage 0 overlay (id stage0/file203800/3.pe2pkg) — source `stage0/file203800/3.pe2pkg` — load `0x80149E20` — 18.0 KiB | Not started |
| `stage0_file204000_3` | Stage 0 overlay (id stage0/file204000/3.pe2pkg) — source `stage0/file204000/3.pe2pkg` — load `0x80149E20` — 36.0 KiB | Not started |
| `stage0_file204100_3` | Stage 0 overlay (id stage0/file204100/3.pe2pkg) — source `stage0/file204100/3.pe2pkg` — load `0x80149E20` — 2.0 KiB | Not started |
| `stage0_file204400_2` | Stage 0 overlay (id stage0/file204400/2.pe2pkg) — source `stage0/file204400/2.pe2pkg` — load `0x80149E20` — 2.0 KiB | Not started |
| `stage0_file204600_3` | Stage 0 overlay (id stage0/file204600/3.pe2pkg) — source `stage0/file204600/3.pe2pkg` — load `0x80149E20` — 20.0 KiB | Not started |
| `stage0_file204900_3` | Stage 0 overlay (id stage0/file204900/3.pe2pkg) — source `stage0/file204900/3.pe2pkg` — load `0x80149E20` — 64.0 KiB | Not started |
| `stage0_file205200_1` | Stage 0 overlay (id stage0/file205200/1.pe2pkg) — source `stage0/file205200/1.pe2pkg` — load `0x80149E20` — 42.0 KiB | Not started |
| `stage0_file205500_3` | Stage 0 overlay (id stage0/file205500/3.pe2pkg) — source `stage0/file205500/3.pe2pkg` — load `0x80149E20` — 26.0 KiB | Not started |
| `stage0_file205600_3` | Stage 0 overlay (id stage0/file205600/3.pe2pkg) — source `stage0/file205600/3.pe2pkg` — load `0x80149E20` — 68.0 KiB | Not started |
| `stage0_file205700_3` | Stage 0 overlay (id stage0/file205700/3.pe2pkg) — source `stage0/file205700/3.pe2pkg` — load `0x80149E20` — 70.0 KiB | Not started |
| `stage0_file20600_0` | Stage 0 overlay (id stage0/file20600/0.pe2pkg) — source `stage0/file20600/0.pe2pkg` — load `0x801D6000` — 2.0 KiB | Not started |
| `stage0_file206100_3` | Stage 0 overlay (id stage0/file206100/3.pe2pkg) — source `stage0/file206100/3.pe2pkg` — load `0x80149E20` — 44.0 KiB | Not started |
| `stage0_file207000_3` | Stage 0 overlay (id stage0/file207000/3.pe2pkg) — source `stage0/file207000/3.pe2pkg` — load `0x80149E20` — 40.0 KiB | Not started |
| `stage0_file207200_3` | Stage 0 overlay (id stage0/file207200/3.pe2pkg) — source `stage0/file207200/3.pe2pkg` — load `0x80149E20` — 30.0 KiB | Not started |
| `stage0_file20900_4` | Stage 0 overlay (id stage0/file20900/4.pe2pkg) — source `stage0/file20900/4.pe2pkg` — load `0x80115770` — 2.0 KiB | Not started |
| `stage0_file21000_6` | Stage 0 overlay (id stage0/file21000/6.pe2pkg) — source `stage0/file21000/6.pe2pkg` — load `0x80115770` — 12.0 KiB | Not started |
| `stage0_file210600_2` | Stage 0 overlay (id stage0/file210600/2.pe2pkg) — source `stage0/file210600/2.pe2pkg` — load `0x80149E20` — 58.0 KiB | Not started |
| `stage0_file210700_2` | Stage 0 overlay (id stage0/file210700/2.pe2pkg) — source `stage0/file210700/2.pe2pkg` — load `0x80149E20` — 46.0 KiB | Not started |
| `stage0_file213000_2` | Stage 0 overlay (id stage0/file213000/2.pe2pkg) — source `stage0/file213000/2.pe2pkg` — load `0x80149E20` — 44.0 KiB | Not started |
| `stage0_file213100_2` | Stage 0 overlay (id stage0/file213100/2.pe2pkg) — source `stage0/file213100/2.pe2pkg` — load `0x80149E20` — 26.0 KiB | Not started |
| `stage0_file215100_2` | Stage 0 overlay (id stage0/file215100/2.pe2pkg) — source `stage0/file215100/2.pe2pkg` — load `0x80149E20` — 56.0 KiB | Not started |
| `stage0_file223600_3` | Stage 0 overlay (id stage0/file223600/3.pe2pkg) — source `stage0/file223600/3.pe2pkg` — load `0x80149E20` — 20.0 KiB | Not started |
| `stage0_file260400_3` | Stage 0 overlay (id stage0/file260400/3.pe2pkg) — source `stage0/file260400/3.pe2pkg` — load `0x80149E20` — 30.0 KiB | Not started |
| `stage0_file260500_3` | Stage 0 overlay (id stage0/file260500/3.pe2pkg) — source `stage0/file260500/3.pe2pkg` — load `0x80149E20` — 46.0 KiB | Not started |
| `stage0_file300300_3` | Stage 0 overlay (id stage0/file300300/3.pe2pkg) — source `stage0/file300300/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file300400_3` | Stage 0 overlay (id stage0/file300400/3.pe2pkg) — source `stage0/file300400/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file300700_3` | Stage 0 overlay (id stage0/file300700/3.pe2pkg) — source `stage0/file300700/3.pe2pkg` — load `0x80161E20` — 22.0 KiB | Not started |
| `stage0_file30100_2` | Stage 0 overlay (id stage0/file30100/2.pe2pkg) — source `stage0/file30100/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file301000_3` | Stage 0 overlay (id stage0/file301000/3.pe2pkg) — source `stage0/file301000/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file30102_2` | Stage 0 overlay (id stage0/file30102/2.pe2pkg) — source `stage0/file30102/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file301100_2` | Stage 0 overlay (id stage0/file301100/2.pe2pkg) — source `stage0/file301100/2.pe2pkg` — load `0x80161E20` — 64.0 KiB | Not started |
| `stage0_file301200_3` | Stage 0 overlay (id stage0/file301200/3.pe2pkg) — source `stage0/file301200/3.pe2pkg` — load `0x80161E20` — 20.0 KiB | Not started |
| `stage0_file301300_3` | Stage 0 overlay (id stage0/file301300/3.pe2pkg) — source `stage0/file301300/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file301500_3` | Stage 0 overlay (id stage0/file301500/3.pe2pkg) — source `stage0/file301500/3.pe2pkg` — load `0x80161E20` — 28.0 KiB | Not started |
| `stage0_file301600_3` | Stage 0 overlay (id stage0/file301600/3.pe2pkg) — source `stage0/file301600/3.pe2pkg` — load `0x80161E20` — 54.0 KiB | Not started |
| `stage0_file301800_3` | Stage 0 overlay (id stage0/file301800/3.pe2pkg) — source `stage0/file301800/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file301900_3` | Stage 0 overlay (id stage0/file301900/3.pe2pkg) — source `stage0/file301900/3.pe2pkg` — load `0x80161E20` — 68.0 KiB | Not started |
| `stage0_file30200_2` | Stage 0 overlay (id stage0/file30200/2.pe2pkg) — source `stage0/file30200/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file302000_3` | Stage 0 overlay (id stage0/file302000/3.pe2pkg) — source `stage0/file302000/3.pe2pkg` — load `0x80161E20` — 68.0 KiB | Not started |
| `stage0_file30202_2` | Stage 0 overlay (id stage0/file30202/2.pe2pkg) — source `stage0/file30202/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file30203_2` | Stage 0 overlay (id stage0/file30203/2.pe2pkg) — source `stage0/file30203/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file302100_3` | Stage 0 overlay (id stage0/file302100/3.pe2pkg) — source `stage0/file302100/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file302300_3` | Stage 0 overlay (id stage0/file302300/3.pe2pkg) — source `stage0/file302300/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file302400_3` | Stage 0 overlay (id stage0/file302400/3.pe2pkg) — source `stage0/file302400/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file302500_3` | Stage 0 overlay (id stage0/file302500/3.pe2pkg) — source `stage0/file302500/3.pe2pkg` — load `0x80161E20` — 18.0 KiB | Not started |
| `stage0_file302600_3` | Stage 0 overlay (id stage0/file302600/3.pe2pkg) — source `stage0/file302600/3.pe2pkg` — load `0x80161E20` — 26.0 KiB | Not started |
| `stage0_file30300_2` | Stage 0 overlay (id stage0/file30300/2.pe2pkg) — source `stage0/file30300/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file30302_2` | Stage 0 overlay (id stage0/file30302/2.pe2pkg) — source `stage0/file30302/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file30303_2` | Stage 0 overlay (id stage0/file30303/2.pe2pkg) — source `stage0/file30303/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file303600_0` | Stage 0 overlay (id stage0/file303600/0.pe2pkg) — source `stage0/file303600/0.pe2pkg` — load `0x80161E20` — 40.0 KiB | Not started |
| `stage0_file303700_3` | Stage 0 overlay (id stage0/file303700/3.pe2pkg) — source `stage0/file303700/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file303800_3` | Stage 0 overlay (id stage0/file303800/3.pe2pkg) — source `stage0/file303800/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file30400_2` | Stage 0 overlay (id stage0/file30400/2.pe2pkg) — source `stage0/file30400/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file304000_3` | Stage 0 overlay (id stage0/file304000/3.pe2pkg) — source `stage0/file304000/3.pe2pkg` — load `0x80161E20` — 36.0 KiB | Not started |
| `stage0_file30402_2` | Stage 0 overlay (id stage0/file30402/2.pe2pkg) — source `stage0/file30402/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file30403_2` | Stage 0 overlay (id stage0/file30403/2.pe2pkg) — source `stage0/file30403/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file30404_2` | Stage 0 overlay (id stage0/file30404/2.pe2pkg) — source `stage0/file30404/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file30405_2` | Stage 0 overlay (id stage0/file30405/2.pe2pkg) — source `stage0/file30405/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file30406_2` | Stage 0 overlay (id stage0/file30406/2.pe2pkg) — source `stage0/file30406/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file304100_3` | Stage 0 overlay (id stage0/file304100/3.pe2pkg) — source `stage0/file304100/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file304400_2` | Stage 0 overlay (id stage0/file304400/2.pe2pkg) — source `stage0/file304400/2.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file304600_3` | Stage 0 overlay (id stage0/file304600/3.pe2pkg) — source `stage0/file304600/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file304900_3` | Stage 0 overlay (id stage0/file304900/3.pe2pkg) — source `stage0/file304900/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file30500_2` | Stage 0 overlay (id stage0/file30500/2.pe2pkg) — source `stage0/file30500/2.pe2pkg` — load `0x8012EF30` — 4.0 KiB | Not started |
| `stage0_file30502_2` | Stage 0 overlay (id stage0/file30502/2.pe2pkg) — source `stage0/file30502/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file30503_2` | Stage 0 overlay (id stage0/file30503/2.pe2pkg) — source `stage0/file30503/2.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file305500_3` | Stage 0 overlay (id stage0/file305500/3.pe2pkg) — source `stage0/file305500/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file305600_3` | Stage 0 overlay (id stage0/file305600/3.pe2pkg) — source `stage0/file305600/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file305700_3` | Stage 0 overlay (id stage0/file305700/3.pe2pkg) — source `stage0/file305700/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file307000_3` | Stage 0 overlay (id stage0/file307000/3.pe2pkg) — source `stage0/file307000/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file307200_3` | Stage 0 overlay (id stage0/file307200/3.pe2pkg) — source `stage0/file307200/3.pe2pkg` — load `0x80161E20` — 2.0 KiB | Not started |
| `stage0_file310100_0` | Stage 0 overlay (id stage0/file310100/0.pe2pkg) — source `stage0/file310100/0.pe2pkg` — load `0x80161E20` — 74.0 KiB | Not started |
| `stage0_file310600_4` | Stage 0 overlay (id stage0/file310600/4.pe2pkg) — source `stage0/file310600/4.pe2pkg` — load `0x80161E20` — 70.0 KiB | Not started |
| `stage0_file311500_3` | Stage 0 overlay (id stage0/file311500/3.pe2pkg) — source `stage0/file311500/3.pe2pkg` — load `0x80161E20` — 24.0 KiB | Not started |
| `stage0_file311900_2` | Stage 0 overlay (id stage0/file311900/2.pe2pkg) — source `stage0/file311900/2.pe2pkg` — load `0x80161E20` — 40.0 KiB | Not started |
| `stage0_file312200_2` | Stage 0 overlay (id stage0/file312200/2.pe2pkg) — source `stage0/file312200/2.pe2pkg` — load `0x80161E20` — 26.0 KiB | Not started |
| `stage0_file317000_2` | Stage 0 overlay (id stage0/file317000/2.pe2pkg) — source `stage0/file317000/2.pe2pkg` — load `0x80161E20` — 34.0 KiB | Not started |
| `stage0_file323000_2` | Stage 0 overlay (id stage0/file323000/2.pe2pkg) — source `stage0/file323000/2.pe2pkg` — load `0x80161E20` — 52.0 KiB | Not started |
| `stage0_file323300_2` | Stage 0 overlay (id stage0/file323300/2.pe2pkg) — source `stage0/file323300/2.pe2pkg` — load `0x80161E20` — 56.0 KiB | Not started |
| `stage0_file323400_2` | Stage 0 overlay (id stage0/file323400/2.pe2pkg) — source `stage0/file323400/2.pe2pkg` — load `0x80161E20` — 44.0 KiB | Not started |
| `stage0_file335800_4` | Stage 0 overlay (id stage0/file335800/4.pe2pkg) — source `stage0/file335800/4.pe2pkg` — load `0x80161E20` — 50.0 KiB | Not started |
| `stage0_file341300_0` | Stage 0 overlay (id stage0/file341300/0.pe2pkg) — source `stage0/file341300/0.pe2pkg` — load `0x80161E20` — 10.0 KiB | Not started |
| `stage0_file341700_0` | Stage 0 overlay (id stage0/file341700/0.pe2pkg) — source `stage0/file341700/0.pe2pkg` — load `0x80161E20` — 58.0 KiB | Not started |
| `stage0_file341900_0` | Stage 0 overlay (id stage0/file341900/0.pe2pkg) — source `stage0/file341900/0.pe2pkg` — load `0x80161E20` — 6.0 KiB | Not started |
| `stage0_file342000_0` | Stage 0 overlay (id stage0/file342000/0.pe2pkg) — source `stage0/file342000/0.pe2pkg` — load `0x80161E20` — 8.0 KiB | Not started |
| `stage0_file342100_0` | Stage 0 overlay (id stage0/file342100/0.pe2pkg) — source `stage0/file342100/0.pe2pkg` — load `0x80161E20` — 12.0 KiB | Not started |
| `stage0_file342200_0` | Stage 0 overlay (id stage0/file342200/0.pe2pkg) — source `stage0/file342200/0.pe2pkg` — load `0x80161E20` — 48.0 KiB | Not started |
| `stage0_file342400_2` | Stage 0 overlay (id stage0/file342400/2.pe2pkg) — source `stage0/file342400/2.pe2pkg` — load `0x80161E20` — 50.0 KiB | Not started |
| `stage0_file350500_2` | Stage 0 overlay (id stage0/file350500/2.pe2pkg) — source `stage0/file350500/2.pe2pkg` — load `0x80161E20` — 22.0 KiB | Not started |
| `stage0_file350700_4` | Stage 0 overlay (id stage0/file350700/4.pe2pkg) — source `stage0/file350700/4.pe2pkg` — load `0x80161E20` — 44.0 KiB | Not started |
| `stage0_file356100_2` | Stage 0 overlay (id stage0/file356100/2.pe2pkg) — source `stage0/file356100/2.pe2pkg` — load `0x80161E20` — 50.0 KiB | Not started |
| `stage0_file361100_2` | Stage 0 overlay (id stage0/file361100/2.pe2pkg) — source `stage0/file361100/2.pe2pkg` — load `0x80161E20` — 46.0 KiB | Not started |
| `stage0_file400100_3` | Stage 0 overlay (id stage0/file400100/3.pe2pkg) — source `stage0/file400100/3.pe2pkg` — load `0x80131E20` — 78.0 KiB | Not started |
| `stage0_file400500_3` | Stage 0 overlay (id stage0/file400500/3.pe2pkg) — source `stage0/file400500/3.pe2pkg` — load `0x80131E20` — 102.0 KiB | Not started |
| `stage0_file400600_2` | Stage 0 overlay (id stage0/file400600/2.pe2pkg) — source `stage0/file400600/2.pe2pkg` — load `0x80131E20` — 96.0 KiB | Not started |
| `stage0_file401000_3` | Stage 0 overlay (id stage0/file401000/3.pe2pkg) — source `stage0/file401000/3.pe2pkg` — load `0x80131E20` — 102.0 KiB | Not started |
| `stage0_file401300_3` | Stage 0 overlay (id stage0/file401300/3.pe2pkg) — source `stage0/file401300/3.pe2pkg` — load `0x80131E20` — 112.0 KiB | Not started |
| `stage0_file401800_3` | Stage 0 overlay (id stage0/file401800/3.pe2pkg) — source `stage0/file401800/3.pe2pkg` — load `0x80131E20` — 104.0 KiB | Not started |
| `stage0_file402200_2` | Stage 0 overlay (id stage0/file402200/2.pe2pkg) — source `stage0/file402200/2.pe2pkg` — load `0x80131E20` — 104.0 KiB | Not started |
| `stage0_file403000_5` | Stage 0 overlay (id stage0/file403000/5.pe2pkg) — source `stage0/file403000/5.pe2pkg` — load `0x80131E20` — 114.0 KiB | Not started |
| `stage0_file403100_5` | Stage 0 overlay (id stage0/file403100/5.pe2pkg) — source `stage0/file403100/5.pe2pkg` — load `0x80131E20` — 108.0 KiB | Not started |
| `stage0_file403200_5` | Stage 0 overlay (id stage0/file403200/5.pe2pkg) — source `stage0/file403200/5.pe2pkg` — load `0x80131E20` — 122.0 KiB | Not started |
| `stage0_file403600_0` | Stage 0 overlay (id stage0/file403600/0.pe2pkg) — source `stage0/file403600/0.pe2pkg` — load `0x80131E20` — 140.0 KiB | Not started |
| `stage0_file403900_2` | Stage 0 overlay (id stage0/file403900/2.pe2pkg) — source `stage0/file403900/2.pe2pkg` — load `0x80131E20` — 104.0 KiB | Not started |
| `stage0_file405800_3` | Stage 0 overlay (id stage0/file405800/3.pe2pkg) — source `stage0/file405800/3.pe2pkg` — load `0x80131E20` — 94.0 KiB | Not started |
| `stage0_file407500_3` | Stage 0 overlay (id stage0/file407500/3.pe2pkg) — source `stage0/file407500/3.pe2pkg` — load `0x80131E20` — 78.0 KiB | Not started |
| `stage0_file420700_4` | Stage 0 overlay (id stage0/file420700/4.pe2pkg) — source `stage0/file420700/4.pe2pkg` — load `0x80131E20` — 40.0 KiB | Not started |
| `stage0_file421600_3` | Stage 0 overlay (id stage0/file421600/3.pe2pkg) — source `stage0/file421600/3.pe2pkg` — load `0x80131E20` — 88.0 KiB | Not started |
| `stage0_file443500_2` | Stage 0 overlay (id stage0/file443500/2.pe2pkg) — source `stage0/file443500/2.pe2pkg` — load `0x80131E20` — 110.0 KiB | Not started |
| `stage0_file444000_5` | Stage 0 overlay (id stage0/file444000/5.pe2pkg) — source `stage0/file444000/5.pe2pkg` — load `0x80131E20` — 130.0 KiB | Not started |
| `stage0_file450200_2` | Stage 0 overlay (id stage0/file450200/2.pe2pkg) — source `stage0/file450200/2.pe2pkg` — load `0x80131E20` — 38.0 KiB | Not started |
| `stage0_file450800_2` | Stage 0 overlay (id stage0/file450800/2.pe2pkg) — source `stage0/file450800/2.pe2pkg` — load `0x80131E20` — 94.0 KiB | Not started |
| `stage0_file450900_0` | Stage 0 overlay (id stage0/file450900/0.pe2pkg) — source `stage0/file450900/0.pe2pkg` — load `0x80131E20` — 14.0 KiB | Not started |
| `stage0_file451100_2` | Stage 0 overlay (id stage0/file451100/2.pe2pkg) — source `stage0/file451100/2.pe2pkg` — load `0x80131E20` — 80.0 KiB | Not started |
| `stage0_file460200_2` | Stage 0 overlay (id stage0/file460200/2.pe2pkg) — source `stage0/file460200/2.pe2pkg` — load `0x80131E20` — 90.0 KiB | Not started |
| `stage0_file461800_2` | Stage 0 overlay (id stage0/file461800/2.pe2pkg) — source `stage0/file461800/2.pe2pkg` — load `0x80131E20` — 52.0 KiB | Not started |
| `stage0_file50100_1` | Stage 0 overlay (id stage0/file50100/1.pe2pkg) — source `stage0/file50100/1.pe2pkg` — load `0x8012EF30` — 10.0 KiB | Not started |
| `stage0_file50104_1` | Stage 0 overlay (id stage0/file50104/1.pe2pkg) — source `stage0/file50104/1.pe2pkg` — load `0x8012EF30` — 6.0 KiB | Not started |
| `stage0_file50107_1` | Stage 0 overlay (id stage0/file50107/1.pe2pkg) — source `stage0/file50107/1.pe2pkg` — load `0x8012EF30` — 4.0 KiB | Not started |
| `stage0_file50110_1` | Stage 0 overlay (id stage0/file50110/1.pe2pkg) — source `stage0/file50110/1.pe2pkg` — load `0x8012EF30` — 6.0 KiB | Not started |
| `stage0_file50113_1` | Stage 0 overlay (id stage0/file50113/1.pe2pkg) — source `stage0/file50113/1.pe2pkg` — load `0x8012EF30` — 4.0 KiB | Not started |
| `stage0_file50116_1` | Stage 0 overlay (id stage0/file50116/1.pe2pkg) — source `stage0/file50116/1.pe2pkg` — load `0x8012EF30` — 6.0 KiB | Not started |
| `stage0_file50119_1` | Stage 0 overlay (id stage0/file50119/1.pe2pkg) — source `stage0/file50119/1.pe2pkg` — load `0x8012EF30` — 4.0 KiB | Not started |
| `stage0_file50122_1` | Stage 0 overlay (id stage0/file50122/1.pe2pkg) — source `stage0/file50122/1.pe2pkg` — load `0x8012EF30` — 4.0 KiB | Not started |
| `stage0_file50125_1` | Stage 0 overlay (id stage0/file50125/1.pe2pkg) — source `stage0/file50125/1.pe2pkg` — load `0x8012EF30` — 6.0 KiB | Not started |
| `stage0_file50128_1` | Stage 0 overlay (id stage0/file50128/1.pe2pkg) — source `stage0/file50128/1.pe2pkg` — load `0x8012EF30` — 6.0 KiB | Not started |
| `stage0_file50131_1` | Stage 0 overlay (id stage0/file50131/1.pe2pkg) — source `stage0/file50131/1.pe2pkg` — load `0x8012EF30` — 4.0 KiB | Not started |
| `stage0_file50134_1` | Stage 0 overlay (id stage0/file50134/1.pe2pkg) — source `stage0/file50134/1.pe2pkg` — load `0x8012EF30` — 8.0 KiB | Not started |
| `stage0_file50146_1` | Stage 0 overlay (id stage0/file50146/1.pe2pkg) — source `stage0/file50146/1.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file50149_1` | Stage 0 overlay (id stage0/file50149/1.pe2pkg) — source `stage0/file50149/1.pe2pkg` — load `0x8012EF30` — 2.0 KiB | Not started |
| `stage0_file50152_1` | Stage 0 overlay (id stage0/file50152/1.pe2pkg) — source `stage0/file50152/1.pe2pkg` — load `0x8012EF30` — 4.0 KiB | Not started |
| `stage0_file503500_7` | Stage 0 overlay (id stage0/file503500/7.pe2pkg) — source `stage0/file503500/7.pe2pkg` — load `0x80131E20` — 196.0 KiB | Not started |
| `stage0_file510900_4` | Stage 0 overlay (id stage0/file510900/4.pe2pkg) — source `stage0/file510900/4.pe2pkg` — load `0x80131E20` — 166.0 KiB | Not started |
| `stage0_file511000_4` | Stage 0 overlay (id stage0/file511000/4.pe2pkg) — source `stage0/file511000/4.pe2pkg` — load `0x80131E20` — 104.0 KiB | Not started |
| `stage0_file521100_4` | Stage 0 overlay (id stage0/file521100/4.pe2pkg) — source `stage0/file521100/4.pe2pkg` — load `0x80131E20` — 168.0 KiB | Not started |
| `stage0_file535700_2` | Stage 0 overlay (id stage0/file535700/2.pe2pkg) — source `stage0/file535700/2.pe2pkg` — load `0x80131E20` — 60.0 KiB | Not started |
| `stage0_file548100_0` | Stage 0 overlay (id stage0/file548100/0.pe2pkg) — source `stage0/file548100/0.pe2pkg` — load `0x80131E20` — 12.0 KiB | Not started |
| `stage0_file560800_4` | Stage 0 overlay (id stage0/file560800/4.pe2pkg) — source `stage0/file560800/4.pe2pkg` — load `0x80131E20` — 186.0 KiB | Not started |
| `stage0_file640400_2` | Stage 0 overlay (id stage0/file640400/2.pe2pkg) — source `stage0/file640400/2.pe2pkg` — load `0x80131E20` — 2.0 KiB | Not started |
| `stage0_file800100_0` | Stage 0 overlay (id stage0/file800100/0.pe2pkg) — source `stage0/file800100/0.pe2pkg` — load `0x80161E20` — 18.0 KiB | Not started |
| `stage0_file800101_5` | Stage 0 overlay (id stage0/file800101/5.pe2pkg) — source `stage0/file800101/5.pe2pkg` — load `0x80167A70` — 42.0 KiB | Not started |
| `stage0_file800102_5` | Stage 0 overlay (id stage0/file800102/5.pe2pkg) — source `stage0/file800102/5.pe2pkg` — load `0x80167A70` — 48.0 KiB | Not started |
| `stage0_file800103_5` | Stage 0 overlay (id stage0/file800103/5.pe2pkg) — source `stage0/file800103/5.pe2pkg` — load `0x80167A70` — 42.0 KiB | Not started |
| `stage0_file800104_5` | Stage 0 overlay (id stage0/file800104/5.pe2pkg) — source `stage0/file800104/5.pe2pkg` — load `0x80167A70` — 46.0 KiB | Not started |
| `stage0_file800200_3` | Stage 0 overlay (id stage0/file800200/3.pe2pkg) — source `stage0/file800200/3.pe2pkg` — load `0x80161E20` — 40.0 KiB | Not started |
| `stage0_file800300_4` | Stage 0 overlay (id stage0/file800300/4.pe2pkg) — source `stage0/file800300/4.pe2pkg` — load `0x80161E20` — 34.0 KiB | Not started |
| `stage0_file900000_6` | Stage 0 overlay (id stage0/file900000/6.pe2pkg) — source `stage0/file900000/6.pe2pkg` — load `0x80179950` — 6.0 KiB | Not started |
| `stage0_file900002_6` | Stage 0 overlay (id stage0/file900002/6.pe2pkg) — source `stage0/file900002/6.pe2pkg` — load `0x80179950` — 6.0 KiB | Not started |
| `stage0_file900003_7` | Stage 0 overlay (id stage0/file900003/7.pe2pkg) — source `stage0/file900003/7.pe2pkg` — load `0x80179950` — 8.0 KiB | Not started |
| `stage0_file900004_2` | Stage 0 overlay (id stage0/file900004/2.pe2pkg) — source `stage0/file900004/2.pe2pkg` — load `0x80179950` — 6.0 KiB | Not started |
| `stage0_file900005_2` | Stage 0 overlay (id stage0/file900005/2.pe2pkg) — source `stage0/file900005/2.pe2pkg` — load `0x80179950` — 8.0 KiB | Not started |
| `stage1_1001_file0_4` | stage1 room overlay (folder 1001) — source `stage1/1001/file0/4.pe2pkg` — load `0x8017D5C0` — 14.0 KiB | Not started |
| `stage1_101_file0_3` | stage1 room overlay (folder 101) — source `stage1/101/file0/3.pe2pkg` — load `0x8017D5C0` — 28.0 KiB | Not started |
| `stage1_1101_file0_6` | stage1 room overlay (folder 1101) — source `stage1/1101/file0/6.pe2pkg` — load `0x8017D5C0` — 28.0 KiB | Not started |
| `stage1_1201_file0_7` | stage1 room overlay (folder 1201) — source `stage1/1201/file0/7.pe2pkg` — load `0x8017D5C0` — 28.0 KiB | Not started |
| `stage1_1301_file0_7` | stage1 room overlay (folder 1301) — source `stage1/1301/file0/7.pe2pkg` — load `0x8017D5C0` — 34.0 KiB | Not started |
| `stage1_1401_file0_6` | stage1 room overlay (folder 1401) — source `stage1/1401/file0/6.pe2pkg` — load `0x8017D5C0` — 50.0 KiB | Not started |
| `stage1_1501_file0_3` | stage1 room overlay (folder 1501) — source `stage1/1501/file0/3.pe2pkg` — load `0x8017D5C0` — 16.0 KiB | Not started |
| `stage1_1601_file0_6` | stage1 room overlay (folder 1601) — source `stage1/1601/file0/6.pe2pkg` — load `0x8017D5C0` — 24.0 KiB | Not started |
| `stage1_1701_file0_7` | stage1 room overlay (folder 1701) — source `stage1/1701/file0/7.pe2pkg` — load `0x8017D5C0` — 26.0 KiB | Not started |
| `stage1_1801_file0_6` | stage1 room overlay (folder 1801) — source `stage1/1801/file0/6.pe2pkg` — load `0x8017D5C0` — 26.0 KiB | Not started |
| `stage1_1901_file0_7` | stage1 room overlay (folder 1901) — source `stage1/1901/file0/7.pe2pkg` — load `0x8017D5C0` — 56.0 KiB | Not started |
| `stage1_2001_file0_5` | stage1 room overlay (folder 2001) — source `stage1/2001/file0/5.pe2pkg` — load `0x8017D5C0` — 38.0 KiB | Not started |
| `stage1_201_file0_3` | stage1 room overlay (folder 201) — source `stage1/201/file0/3.pe2pkg` — load `0x8017D5C0` — 30.0 KiB | Not started |
| `stage1_2101_file0_1` | stage1 room overlay (folder 2101) — source `stage1/2101/file0/1.pe2pkg` — load `0x8017D5C0` — 2.0 KiB | Not started |
| `stage1_301_file0_5` | stage1 room overlay (folder 301) — source `stage1/301/file0/5.pe2pkg` — load `0x8017D5C0` — 22.0 KiB | Not started |
| `stage1_401_file0_8` | stage1 room overlay (folder 401) — source `stage1/401/file0/8.pe2pkg` — load `0x8017D5C0` — 38.0 KiB | Not started |
| `stage1_501_file0_5` | stage1 room overlay (folder 501) — source `stage1/501/file0/5.pe2pkg` — load `0x8017D5C0` — 70.0 KiB | Not started |
| `stage1_601_file0_5` | stage1 room overlay (folder 601) — source `stage1/601/file0/5.pe2pkg` — load `0x8017D5C0` — 20.0 KiB | Not started |
| `stage1_701_file0_5` | stage1 room overlay (folder 701) — source `stage1/701/file0/5.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage1_801_file0_5` | stage1 room overlay (folder 801) — source `stage1/801/file0/5.pe2pkg` — load `0x8017D5C0` — 14.0 KiB | Not started |
| `stage1_901_file0_5` | stage1 room overlay (folder 901) — source `stage1/901/file0/5.pe2pkg` — load `0x8017D5C0` — 18.0 KiB | Not started |
| `stage2_101_file0_3` | stage2 room overlay (folder 101) — source `stage2/101/file0/3.pe2pkg` — load `0x8017D5C0` — 20.0 KiB | Not started |
| `stage2_1101_file0_3` | stage2 room overlay (folder 1101) — source `stage2/1101/file0/3.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage2_1201_file0_3` | stage2 room overlay (folder 1201) — source `stage2/1201/file0/3.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage2_1301_file0_3` | stage2 room overlay (folder 1301) — source `stage2/1301/file0/3.pe2pkg` — load `0x8017D5C0` — 4.0 KiB | Not started |
| `stage2_1401_file0_3` | stage2 room overlay (folder 1401) — source `stage2/1401/file0/3.pe2pkg` — load `0x8017D5C0` — 2.0 KiB | Not started |
| `stage2_1501_file0_3` | stage2 room overlay (folder 1501) — source `stage2/1501/file0/3.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage2_1601_file0_5` | stage2 room overlay (folder 1601) — source `stage2/1601/file0/5.pe2pkg` — load `0x8017D5C0` — 26.0 KiB | Not started |
| `stage2_1701_file0_3` | stage2 room overlay (folder 1701) — source `stage2/1701/file0/3.pe2pkg` — load `0x8017D5C0` — 10.0 KiB | Not started |
| `stage2_1801_file0_5` | stage2 room overlay (folder 1801) — source `stage2/1801/file0/5.pe2pkg` — load `0x8017D5C0` — 10.0 KiB | Not started |
| `stage2_1901_file0_3` | stage2 room overlay (folder 1901) — source `stage2/1901/file0/3.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage2_2001_file0_5` | stage2 room overlay (folder 2001) — source `stage2/2001/file0/5.pe2pkg` — load `0x8017D5C0` — 22.0 KiB | Not started |
| `stage2_201_file0_5` | stage2 room overlay (folder 201) — source `stage2/201/file0/5.pe2pkg` — load `0x8017D5C0` — 20.0 KiB | Not started |
| `stage2_2101_file0_6` | stage2 room overlay (folder 2101) — source `stage2/2101/file0/6.pe2pkg` — load `0x8017D5C0` — 26.0 KiB | Not started |
| `stage2_2201_file0_5` | stage2 room overlay (folder 2201) — source `stage2/2201/file0/5.pe2pkg` — load `0x8017D5C0` — 18.0 KiB | Not started |
| `stage2_2301_file0_5` | stage2 room overlay (folder 2301) — source `stage2/2301/file0/5.pe2pkg` — load `0x8017D5C0` — 34.0 KiB | Not started |
| `stage2_2401_file0_3` | stage2 room overlay (folder 2401) — source `stage2/2401/file0/3.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage2_2501_file0_3` | stage2 room overlay (folder 2501) — source `stage2/2501/file0/3.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage2_2601_file0_5` | stage2 room overlay (folder 2601) — source `stage2/2601/file0/5.pe2pkg` — load `0x8017D5C0` — 10.0 KiB | Not started |
| `stage2_2701_file0_5` | stage2 room overlay (folder 2701) — source `stage2/2701/file0/5.pe2pkg` — load `0x8017D5C0` — 28.0 KiB | Not started |
| `stage2_2801_file0_3` | stage2 room overlay (folder 2801) — source `stage2/2801/file0/3.pe2pkg` — load `0x8017D5C0` — 2.0 KiB | Not started |
| `stage2_2901_file0_3` | stage2 room overlay (folder 2901) — source `stage2/2901/file0/3.pe2pkg` — load `0x8017D5C0` — 22.0 KiB | Not started |
| `stage2_3001_file0_3` | stage2 room overlay (folder 3001) — source `stage2/3001/file0/3.pe2pkg` — load `0x8017D5C0` — 24.0 KiB | Not started |
| `stage2_301_file0_3` | stage2 room overlay (folder 301) — source `stage2/301/file0/3.pe2pkg` — load `0x8017D5C0` — 16.0 KiB | Not started |
| `stage2_3101_file0_3` | stage2 room overlay (folder 3101) — source `stage2/3101/file0/3.pe2pkg` — load `0x8017D5C0` — 2.0 KiB | Not started |
| `stage2_3201_file0_5` | stage2 room overlay (folder 3201) — source `stage2/3201/file0/5.pe2pkg` — load `0x8017D5C0` — 12.0 KiB | Not started |
| `stage2_3401_file0_5` | stage2 room overlay (folder 3401) — source `stage2/3401/file0/5.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage2_3801_file0_5` | stage2 room overlay (folder 3801) — source `stage2/3801/file0/5.pe2pkg` — load `0x8017D5C0` — 10.0 KiB | Not started |
| `stage2_401_file0_1` | stage2 room overlay (folder 401) — source `stage2/401/file0/1.pe2pkg` — load `0x8017D5C0` — 2.0 KiB | Not started |
| `stage2_501_file0_3` | stage2 room overlay (folder 501) — source `stage2/501/file0/3.pe2pkg` — load `0x8017D5C0` — 10.0 KiB | Not started |
| `stage2_601_file0_3` | stage2 room overlay (folder 601) — source `stage2/601/file0/3.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage2_701_file0_3` | stage2 room overlay (folder 701) — source `stage2/701/file0/3.pe2pkg` — load `0x8017D5C0` — 12.0 KiB | Not started |
| `stage2_801_file0_5` | stage2 room overlay (folder 801) — source `stage2/801/file0/5.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage2_901_file0_3` | stage2 room overlay (folder 901) — source `stage2/901/file0/3.pe2pkg` — load `0x8017D5C0` — 30.0 KiB | Not started |
| `stage3_101_file0_5` | stage3 room overlay (folder 101) — source `stage3/101/file0/5.pe2pkg` — load `0x8017D5C0` — 46.0 KiB | Not started |
| `stage3_1101_file0_5` | stage3 room overlay (folder 1101) — source `stage3/1101/file0/5.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage3_1201_file0_5` | stage3 room overlay (folder 1201) — source `stage3/1201/file0/5.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage3_1301_file0_5` | stage3 room overlay (folder 1301) — source `stage3/1301/file0/5.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage3_1401_file0_5` | stage3 room overlay (folder 1401) — source `stage3/1401/file0/5.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage3_1501_file0_5` | stage3 room overlay (folder 1501) — source `stage3/1501/file0/5.pe2pkg` — load `0x8017D5C0` — 10.0 KiB | Not started |
| `stage3_1601_file0_5` | stage3 room overlay (folder 1601) — source `stage3/1601/file0/5.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage3_1701_file0_5` | stage3 room overlay (folder 1701) — source `stage3/1701/file0/5.pe2pkg` — load `0x8017D5C0` — 18.0 KiB | Not started |
| `stage3_1801_file0_6` | stage3 room overlay (folder 1801) — source `stage3/1801/file0/6.pe2pkg` — load `0x8017D5C0` — 28.0 KiB | Not started |
| `stage3_1901_file0_3` | stage3 room overlay (folder 1901) — source `stage3/1901/file0/3.pe2pkg` — load `0x8017D5C0` — 4.0 KiB | Not started |
| `stage3_2001_file0_5` | stage3 room overlay (folder 2001) — source `stage3/2001/file0/5.pe2pkg` — load `0x8017D5C0` — 12.0 KiB | Not started |
| `stage3_201_file0_5` | stage3 room overlay (folder 201) — source `stage3/201/file0/5.pe2pkg` — load `0x8017D5C0` — 26.0 KiB | Not started |
| `stage3_2101_file0_5` | stage3 room overlay (folder 2101) — source `stage3/2101/file0/5.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage3_2201_file0_5` | stage3 room overlay (folder 2201) — source `stage3/2201/file0/5.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage3_2301_file0_5` | stage3 room overlay (folder 2301) — source `stage3/2301/file0/5.pe2pkg` — load `0x8017D5C0` — 34.0 KiB | Not started |
| `stage3_2401_file0_5` | stage3 room overlay (folder 2401) — source `stage3/2401/file0/5.pe2pkg` — load `0x8017D5C0` — 24.0 KiB | Not started |
| `stage3_2501_file0_3` | stage3 room overlay (folder 2501) — source `stage3/2501/file0/3.pe2pkg` — load `0x8017D5C0` — 10.0 KiB | Not started |
| `stage3_2601_file0_5` | stage3 room overlay (folder 2601) — source `stage3/2601/file0/5.pe2pkg` — load `0x8017D5C0` — 18.0 KiB | Not started |
| `stage3_2701_file0_5` | stage3 room overlay (folder 2701) — source `stage3/2701/file0/5.pe2pkg` — load `0x8017D5C0` — 34.0 KiB | Not started |
| `stage3_2801_file0_5` | stage3 room overlay (folder 2801) — source `stage3/2801/file0/5.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage3_2901_file0_6` | stage3 room overlay (folder 2901) — source `stage3/2901/file0/6.pe2pkg` — load `0x8017D5C0` — 38.0 KiB | Not started |
| `stage3_3001_file0_5` | stage3 room overlay (folder 3001) — source `stage3/3001/file0/5.pe2pkg` — load `0x8017D5C0` — 22.0 KiB | Not started |
| `stage3_301_file0_3` | stage3 room overlay (folder 301) — source `stage3/301/file0/3.pe2pkg` — load `0x8017D5C0` — 18.0 KiB | Not started |
| `stage3_3101_file0_6` | stage3 room overlay (folder 3101) — source `stage3/3101/file0/6.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage3_3201_file0_5` | stage3 room overlay (folder 3201) — source `stage3/3201/file0/5.pe2pkg` — load `0x8017D5C0` — 14.0 KiB | Not started |
| `stage3_3401_file0_5` | stage3 room overlay (folder 3401) — source `stage3/3401/file0/5.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage3_3801_file0_5` | stage3 room overlay (folder 3801) — source `stage3/3801/file0/5.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage3_501_file0_5` | stage3 room overlay (folder 501) — source `stage3/501/file0/5.pe2pkg` — load `0x8017D5C0` — 12.0 KiB | Not started |
| `stage3_601_file0_3` | stage3 room overlay (folder 601) — source `stage3/601/file0/3.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage3_701_file0_3` | stage3 room overlay (folder 701) — source `stage3/701/file0/3.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage3_801_file0_3` | stage3 room overlay (folder 801) — source `stage3/801/file0/3.pe2pkg` — load `0x8017D5C0` — 12.0 KiB | Not started |
| `stage3_901_file0_3` | stage3 room overlay (folder 901) — source `stage3/901/file0/3.pe2pkg` — load `0x8017D5C0` — 32.0 KiB | Not started |
| `stage4_1001_file0_3` | stage4 room overlay (folder 1001) — source `stage4/1001/file0/3.pe2pkg` — load `0x8017D5C0` — 16.0 KiB | Not started |
| `stage4_101_file0_5` | stage4 room overlay (folder 101) — source `stage4/101/file0/5.pe2pkg` — load `0x8017D5C0` — 30.0 KiB | Not started |
| `stage4_1101_file0_3` | stage4 room overlay (folder 1101) — source `stage4/1101/file0/3.pe2pkg` — load `0x8017D5C0` — 26.0 KiB | Not started |
| `stage4_1201_file0_3` | stage4 room overlay (folder 1201) — source `stage4/1201/file0/3.pe2pkg` — load `0x8017D5C0` — 24.0 KiB | Not started |
| `stage4_1301_file0_3` | stage4 room overlay (folder 1301) — source `stage4/1301/file0/3.pe2pkg` — load `0x8017D5C0` — 20.0 KiB | Not started |
| `stage4_1401_file0_6` | stage4 room overlay (folder 1401) — source `stage4/1401/file0/6.pe2pkg` — load `0x8017D5C0` — 16.0 KiB | Not started |
| `stage4_1501_file0_5` | stage4 room overlay (folder 1501) — source `stage4/1501/file0/5.pe2pkg` — load `0x8017D5C0` — 22.0 KiB | Not started |
| `stage4_1601_file0_8` | stage4 room overlay (folder 1601) — source `stage4/1601/file0/8.pe2pkg` — load `0x8017D5C0` — 38.0 KiB | Not started |
| `stage4_1701_file0_3` | stage4 room overlay (folder 1701) — source `stage4/1701/file0/3.pe2pkg` — load `0x8017D5C0` — 20.0 KiB | Not started |
| `stage4_1801_file0_5` | stage4 room overlay (folder 1801) — source `stage4/1801/file0/5.pe2pkg` — load `0x8017D5C0` — 16.0 KiB | Not started |
| `stage4_1901_file0_3` | stage4 room overlay (folder 1901) — source `stage4/1901/file0/3.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage4_2001_file0_6` | stage4 room overlay (folder 2001) — source `stage4/2001/file0/6.pe2pkg` — load `0x8017D5C0` — 38.0 KiB | Not started |
| `stage4_201_file0_7` | stage4 room overlay (folder 201) — source `stage4/201/file0/7.pe2pkg` — load `0x8017D5C0` — 42.0 KiB | Not started |
| `stage4_2101_file0_5` | stage4 room overlay (folder 2101) — source `stage4/2101/file0/5.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage4_2201_file0_9` | stage4 room overlay (folder 2201) — source `stage4/2201/file0/9.pe2pkg` — load `0x8017D5C0` — 26.0 KiB | Not started |
| `stage4_2301_file0_5` | stage4 room overlay (folder 2301) — source `stage4/2301/file0/5.pe2pkg` — load `0x8017D5C0` — 12.0 KiB | Not started |
| `stage4_2401_file0_3` | stage4 room overlay (folder 2401) — source `stage4/2401/file0/3.pe2pkg` — load `0x8017D5C0` — 16.0 KiB | Not started |
| `stage4_2501_file0_3` | stage4 room overlay (folder 2501) — source `stage4/2501/file0/3.pe2pkg` — load `0x8017D5C0` — 14.0 KiB | Not started |
| `stage4_2601_file0_5` | stage4 room overlay (folder 2601) — source `stage4/2601/file0/5.pe2pkg` — load `0x8017D5C0` — 4.0 KiB | Not started |
| `stage4_2701_file0_3` | stage4 room overlay (folder 2701) — source `stage4/2701/file0/3.pe2pkg` — load `0x8017D5C0` — 20.0 KiB | Not started |
| `stage4_2801_file0_3` | stage4 room overlay (folder 2801) — source `stage4/2801/file0/3.pe2pkg` — load `0x8017D5C0` — 18.0 KiB | Not started |
| `stage4_2901_file0_3` | stage4 room overlay (folder 2901) — source `stage4/2901/file0/3.pe2pkg` — load `0x8017D5C0` — 18.0 KiB | Not started |
| `stage4_3001_file0_3` | stage4 room overlay (folder 3001) — source `stage4/3001/file0/3.pe2pkg` — load `0x8017D5C0` — 22.0 KiB | Not started |
| `stage4_301_file0_5` | stage4 room overlay (folder 301) — source `stage4/301/file0/5.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage4_3101_file0_6` | stage4 room overlay (folder 3101) — source `stage4/3101/file0/6.pe2pkg` — load `0x8017D5C0` — 22.0 KiB | Not started |
| `stage4_3201_file0_6` | stage4 room overlay (folder 3201) — source `stage4/3201/file0/6.pe2pkg` — load `0x8017D5C0` — 18.0 KiB | Not started |
| `stage4_3301_file0_5` | stage4 room overlay (folder 3301) — source `stage4/3301/file0/5.pe2pkg` — load `0x8017D5C0` — 28.0 KiB | Not started |
| `stage4_3401_file0_5` | stage4 room overlay (folder 3401) — source `stage4/3401/file0/5.pe2pkg` — load `0x8017D5C0` — 24.0 KiB | Not started |
| `stage4_3501_file0_5` | stage4 room overlay (folder 3501) — source `stage4/3501/file0/5.pe2pkg` — load `0x8017D5C0` — 22.0 KiB | Not started |
| `stage4_3601_file0_8` | stage4 room overlay (folder 3601) — source `stage4/3601/file0/8.pe2pkg` — load `0x8017D5C0` — 4.0 KiB | Not started |
| `stage4_3701_file0_1` | stage4 room overlay (folder 3701) — source `stage4/3701/file0/1.pe2pkg` — load `0x8017D5C0` — 2.0 KiB | Not started |
| `stage4_3801_file0_1` | stage4 room overlay (folder 3801) — source `stage4/3801/file0/1.pe2pkg` — load `0x8017D5C0` — 2.0 KiB | Not started |
| `stage4_3901_file0_8` | stage4 room overlay (folder 3901) — source `stage4/3901/file0/8.pe2pkg` — load `0x8017D5C0` — 42.0 KiB | Not started |
| `stage4_4001_file0_7` | stage4 room overlay (folder 4001) — source `stage4/4001/file0/7.pe2pkg` — load `0x8017D5C0` — 44.0 KiB | Not started |
| `stage4_401_file0_5` | stage4 room overlay (folder 401) — source `stage4/401/file0/5.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage4_4101_file0_3` | stage4 room overlay (folder 4101) — source `stage4/4101/file0/3.pe2pkg` — load `0x8017D5C0` — 14.0 KiB | Not started |
| `stage4_4201_file0_3` | stage4 room overlay (folder 4201) — source `stage4/4201/file0/3.pe2pkg` — load `0x8017D5C0` — 20.0 KiB | Not started |
| `stage4_4301_file0_5` | stage4 room overlay (folder 4301) — source `stage4/4301/file0/5.pe2pkg` — load `0x8017D5C0` — 16.0 KiB | Not started |
| `stage4_4401_file0_5` | stage4 room overlay (folder 4401) — source `stage4/4401/file0/5.pe2pkg` — load `0x8017D5C0` — 30.0 KiB | Not started |
| `stage4_4501_file0_6` | stage4 room overlay (folder 4501) — source `stage4/4501/file0/6.pe2pkg` — load `0x8017D5C0` — 26.0 KiB | Not started |
| `stage4_4601_file0_5` | stage4 room overlay (folder 4601) — source `stage4/4601/file0/5.pe2pkg` — load `0x8017D5C0` — 20.0 KiB | Not started |
| `stage4_4701_file0_5` | stage4 room overlay (folder 4701) — source `stage4/4701/file0/5.pe2pkg` — load `0x8017D5C0` — 34.0 KiB | Not started |
| `stage4_4801_file0_5` | stage4 room overlay (folder 4801) — source `stage4/4801/file0/5.pe2pkg` — load `0x8017D5C0` — 32.0 KiB | Not started |
| `stage4_4901_file0_2` | stage4 room overlay (folder 4901) — source `stage4/4901/file0/2.pe2pkg` — load `0x8017D5C0` — 2.0 KiB | Not started |
| `stage4_501_file0_5` | stage4 room overlay (folder 501) — source `stage4/501/file0/5.pe2pkg` — load `0x8017D5C0` — 14.0 KiB | Not started |
| `stage4_601_file0_6` | stage4 room overlay (folder 601) — source `stage4/601/file0/6.pe2pkg` — load `0x8017D5C0` — 14.0 KiB | Not started |
| `stage4_701_file0_7` | stage4 room overlay (folder 701) — source `stage4/701/file0/7.pe2pkg` — load `0x8017D5C0` — 20.0 KiB | Not started |
| `stage4_801_file0_5` | stage4 room overlay (folder 801) — source `stage4/801/file0/5.pe2pkg` — load `0x8017D5C0` — 16.0 KiB | Not started |
| `stage4_901_file0_5` | stage4 room overlay (folder 901) — source `stage4/901/file0/5.pe2pkg` — load `0x8017D5C0` — 20.0 KiB | Not started |
| `stage5_1001_file0_3` | stage5 room overlay (folder 1001) — source `stage5/1001/file0/3.pe2pkg` — load `0x8017D5C0` — 16.0 KiB | Not started |
| `stage5_101_file0_3` | stage5 room overlay (folder 101) — source `stage5/101/file0/3.pe2pkg` — load `0x8017D5C0` — 12.0 KiB | Not started |
| `stage5_1101_file0_5` | stage5 room overlay (folder 1101) — source `stage5/1101/file0/5.pe2pkg` — load `0x8017D5C0` — 16.0 KiB | Not started |
| `stage5_1201_file0_5` | stage5 room overlay (folder 1201) — source `stage5/1201/file0/5.pe2pkg` — load `0x8017D5C0` — 24.0 KiB | Not started |
| `stage5_1301_file0_5` | stage5 room overlay (folder 1301) — source `stage5/1301/file0/5.pe2pkg` — load `0x8017D5C0` — 26.0 KiB | Not started |
| `stage5_1401_file0_5` | stage5 room overlay (folder 1401) — source `stage5/1401/file0/5.pe2pkg` — load `0x8017D5C0` — 18.0 KiB | Not started |
| `stage5_1501_file0_5` | stage5 room overlay (folder 1501) — source `stage5/1501/file0/5.pe2pkg` — load `0x8017D5C0` — 14.0 KiB | Not started |
| `stage5_1601_file0_3` | stage5 room overlay (folder 1601) — source `stage5/1601/file0/3.pe2pkg` — load `0x8017D5C0` — 14.0 KiB | Not started |
| `stage5_1701_file0_3` | stage5 room overlay (folder 1701) — source `stage5/1701/file0/3.pe2pkg` — load `0x8017D5C0` — 10.0 KiB | Not started |
| `stage5_1801_file0_3` | stage5 room overlay (folder 1801) — source `stage5/1801/file0/3.pe2pkg` — load `0x8017D5C0` — 10.0 KiB | Not started |
| `stage5_1901_file0_3` | stage5 room overlay (folder 1901) — source `stage5/1901/file0/3.pe2pkg` — load `0x8017D5C0` — 10.0 KiB | Not started |
| `stage5_2001_file0_3` | stage5 room overlay (folder 2001) — source `stage5/2001/file0/3.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage5_201_file0_5` | stage5 room overlay (folder 201) — source `stage5/201/file0/5.pe2pkg` — load `0x8017D5C0` — 16.0 KiB | Not started |
| `stage5_2101_file0_5` | stage5 room overlay (folder 2101) — source `stage5/2101/file0/5.pe2pkg` — load `0x8017D5C0` — 22.0 KiB | Not started |
| `stage5_2201_file0_6` | stage5 room overlay (folder 2201) — source `stage5/2201/file0/6.pe2pkg` — load `0x8017D5C0` — 26.0 KiB | Not started |
| `stage5_2301_file0_5` | stage5 room overlay (folder 2301) — source `stage5/2301/file0/5.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage5_2401_file0_3` | stage5 room overlay (folder 2401) — source `stage5/2401/file0/3.pe2pkg` — load `0x8017D5C0` — 10.0 KiB | Not started |
| `stage5_2501_file0_5` | stage5 room overlay (folder 2501) — source `stage5/2501/file0/5.pe2pkg` — load `0x8017D5C0` — 20.0 KiB | Not started |
| `stage5_2601_file0_4` | stage5 room overlay (folder 2601) — source `stage5/2601/file0/4.pe2pkg` — load `0x8017D5C0` — 2.0 KiB | Not started |
| `stage5_2701_file0_5` | stage5 room overlay (folder 2701) — source `stage5/2701/file0/5.pe2pkg` — load `0x8017D5C0` — 20.0 KiB | Not started |
| `stage5_2801_file0_4` | stage5 room overlay (folder 2801) — source `stage5/2801/file0/4.pe2pkg` — load `0x8017D5C0` — 18.0 KiB | Not started |
| `stage5_2901_file0_5` | stage5 room overlay (folder 2901) — source `stage5/2901/file0/5.pe2pkg` — load `0x8017D5C0` — 20.0 KiB | Not started |
| `stage5_3001_file0_5` | stage5 room overlay (folder 3001) — source `stage5/3001/file0/5.pe2pkg` — load `0x8017D5C0` — 20.0 KiB | Not started |
| `stage5_301_file0_3` | stage5 room overlay (folder 301) — source `stage5/301/file0/3.pe2pkg` — load `0x8017D5C0` — 10.0 KiB | Not started |
| `stage5_3101_file0_1` | stage5 room overlay (folder 3101) — source `stage5/3101/file0/1.pe2pkg` — load `0x8017D5C0` — 2.0 KiB | Not started |
| `stage5_3201_file0_3` | stage5 room overlay (folder 3201) — source `stage5/3201/file0/3.pe2pkg` — load `0x8017D5C0` — 12.0 KiB | Not started |
| `stage5_3301_file0_3` | stage5 room overlay (folder 3301) — source `stage5/3301/file0/3.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage5_401_file0_5` | stage5 room overlay (folder 401) — source `stage5/401/file0/5.pe2pkg` — load `0x8017D5C0` — 14.0 KiB | Not started |
| `stage5_501_file0_3` | stage5 room overlay (folder 501) — source `stage5/501/file0/3.pe2pkg` — load `0x8017D5C0` — 6.0 KiB | Not started |
| `stage5_601_file0_3` | stage5 room overlay (folder 601) — source `stage5/601/file0/3.pe2pkg` — load `0x8017D5C0` — 2.0 KiB | Not started |
| `stage5_701_file0_6` | stage5 room overlay (folder 701) — source `stage5/701/file0/6.pe2pkg` — load `0x8017D5C0` — 22.0 KiB | Not started |
| `stage5_801_file0_3` | stage5 room overlay (folder 801) — source `stage5/801/file0/3.pe2pkg` — load `0x8017D5C0` — 8.0 KiB | Not started |
| `stage5_901_file0_3` | stage5 room overlay (folder 901) — source `stage5/901/file0/3.pe2pkg` — load `0x8017D5C0` — 2.0 KiB | Not started |
</details>

### Other regions

The Japanese version (`SLPS-02480` / `SLPS-02481`) is currently not being worked on.

## Prerequisites

### System packages (Linux)

- Python 3.10+
- Ninja
- `binutils-mips-linux-gnu` (assembler, linker, objcopy, cpp) — **binutils ≥ 2.45** is required; older packages often fail to assemble or link correctly
- `clang-format` (used by `tools/build-and-verify.sh`)
- Git (with submodules)

On Ubuntu/Debian-style systems this is roughly:

```bash
sudo apt-get install -y \
  build-essential ninja-build python3 python3-pip python3-venv \
  binutils-mips-linux-gnu clang-format git
```

Check the installed version with `mips-linux-gnu-as --version` (or `ld` / `objcopy`). If your distro ships something older than 2.45, install a newer binutils from a backports/PPA source or build from upstream.

A `Dockerfile` is provided for a containerized environment similar to CI (`ghcr.io/gaberealb/decomp-images-parasite-eve-2`).

### Python dependencies

```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Submodules

```bash
git submodule update --init --recursive
```

This pulls in:

- [splat](https://github.com/ethteck/splat) (via pip) — binary splitting
- [maspsx](https://github.com/mkst/maspsx) — ASPSX-compatible assembler front-end
- [m2c](https://github.com/matt-kempster/m2c) — assembly → C decompiler aid
- [asm-differ](https://github.com/simonlindholm/asm-differ) — instruction-level diffs
- [decomp-permuter](https://github.com/simonlindholm/decomp-permuter) — match search via code mutation

The original PSX GCC 2.8.1 / 2.7.2 compilers ship under `tools/linux/` (and equivalents for Windows/macOS).

## Obtaining game files

1. Dump **both** USA discs to BIN/CUE (or equivalent).
2. Place the images under `rom/image/` with these names:

   | File | Description |
   |---|---|
   | `rom/image/SLUS-01042.bin` + `.cue` | USA Disk 1 |
   | `rom/image/SLUS-01055.bin` + `.cue` | USA Disk 2 |

   Each `.cue` file must reference the matching `.bin` by name (e.g. `FILE "SLUS-01042.bin" BINARY`). Dump tools often leave the original dump filename in the cue sheet; if extraction fails, open the `.cue` in a text editor and fix the `FILE` line so it matches the renamed `.bin`.

3. Extract disc contents:

   ```bash
   python3 ninja_config.py --iso_extract
   ```

   This uses `dumpsxiso` and writes files under `rom/USA/disk1/` and `rom/USA/disk2/`, and prepares the executable/assets used by the build.

## Building

From the repository root (with the venv active):

```bash
# Configure (runs splat, generates build.ninja, link scripts, etc.)
python3 ninja_config.py

# Build and verify checksum against the retail executable
ninja
```

Or use the all-in-one helper (formats sources, reconfigures, builds, and checks):

```bash
./tools/build-and-verify.sh
```

### Useful configure flags

| Flag | Meaning |
|---|---|
| `-c` / `--clean` | Clean build and permuter outputs |
| `-iso_e` / `--iso_extract` | Extract files from disc images (full inflate) |
| `-iso_min` / `--iso_extract_minimal` | CI extract: raw + required overlays only (`title`, `gameplay`) |
| `-iso_raw` / `--iso_extract_raw` | Extract raw/{type}/ only (no inflate) |
| `-sc` / `--skip_checksum` | Build without the matching checksum step |
| `-nm` / `--non_matching` | Compile with `-DNON_MATCHING` |
| `-obj` / `--objdiff_config` | Generate objdiff setup for progress / per-object diffs |
| `-ver USA` / `-ver JAP` | Select game version (default: USA) |

### Diffing a function

```bash
python3 tools/asm-differ/diff.py --no-pager <function_name>
```

### Matching decompilation workflow (high level)

1. Pick a function (e.g. via `python3 tools/score_functions.py asm/USA/main/nonmatchings/`).
2. Decompile / rewrite it in C under the appropriate `src/**/*.c` unit.
3. Replace the corresponding `INCLUDE_ASM(...)` with the C implementation.
4. Rebuild and confirm the full executable still matches.
5. Prefer real structs over raw pointer arithmetic — see project conventions below.

Scratch helpers for AI-assisted matching live under `tools/claude` / `tools/claude-decomp-env/`.

## Repository layout

```text
asm/                   # Disassembled code (matchings + nonmatchings)
assets/                # Extracted assets (main.exe, stages, …)
configs/               # splat config, symbols, relocs, checksum
expected/              # Expected objects for objdiff (when generated)
include/               # Shared headers (decomp types, game, Psy-Q)
lib/                   # Prebuilt Psy-Q .o files linked into the binary
linkers/               # Linker scripts and auto undefineds
rom/                   # Disc images + extracted disc trees (local only)
src/                   # Decompiled C for the main executable and the overlays
tools/                 # Build helpers, peassets, m2c, differ, permuter, …
ninja_config.py        # Configure / extract / generate ninja build
```

Related docs:

- [NAMING.md](NAMING.md) — symbol and module naming conventions
- [DECOMPILATION_LEARNINGS.md](DECOMPILATION_LEARNINGS.md) — GCC 2.8.1 / matching notes
- [CLAUDE.md](CLAUDE.md) — contributor / agent workflow notes

## Legal

- Project source, scripts, and documentation are released under **CC0 1.0** (see [LICENSE](LICENSE)).
- *Parasite Eve II*, related trademarks, and all original game assets remain property of their respective rights holders (Square / Square Enix, et al.).
- No game binaries or assets are redistributed by this project. Contributors must use dumps of discs they own.

## Acknowledgments

Build system and project structure are adapted from other PSX matching decomp efforts (notably the Silent Hill decomp-style `ninja_config` layout). Tooling includes splat, maspsx, m2c, asm-differ, decomp-permuter, and mkpsxiso/dumpsxiso.
