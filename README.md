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

Descriptions are best-effort: source path and load address come from `assets/USA/OVR/map.json`; role text is inferred from embedded strings when available, otherwise from the stage/room (or file id) encoding.

| Overlay | Description | Status |
|---|---|---|
| `stage0_001` | Battle / combat UI — source `stage0/file0/1.pe2pkg` — load `0x80093800` — 520 KiB | Not started |
| `stage0_002` | Title / demo / main menu — source `stage0/file1/5.pe2pkg` — load `0x80093800` — 5.2 KiB | Not started |
| `stage0_003` | Stage 0 overlay (file id 100300) — source `stage0/file100300/3.pe2pkg` — load `0x80131E20` — 88.9 KiB | Not started |
| `stage0_004` | Stage 0 overlay (file id 100400) — source `stage0/file100400/3.pe2pkg` — load `0x80131E20` — 88.2 KiB | Not started |
| `stage0_005` | Empty placeholder package — source `stage0/file100500/3.pe2pkg` — load `0x80131E20` — 4 B | Not started |
| `stage0_006` | Stage 0 overlay (file id 100700) — source `stage0/file100700/3.pe2pkg` — load `0x80131E20` — 29.5 KiB | Not started |
| `stage0_007` | Empty placeholder package — source `stage0/file101000/3.pe2pkg` — load `0x80131E20` — 4 B | Not started |
| `stage0_008` | Stage 0 overlay (file id 101100) — source `stage0/file101100/2.pe2pkg` — load `0x80131E20` — 85.6 KiB | Not started |
| `stage0_009` | Stage 0 overlay (file id 101200) — source `stage0/file101200/3.pe2pkg` — load `0x80131E20` — 28.1 KiB | Not started |
| `stage0_010` | Empty placeholder package — source `stage0/file101300/3.pe2pkg` — load `0x80131E20` — 4 B | Not started |
| `stage0_011` | Stage 0 overlay (file id 101500) — source `stage0/file101500/3.pe2pkg` — load `0x80131E20` — 40.2 KiB | Not started |
| `stage0_012` | Stage 0 overlay (file id 101600) — source `stage0/file101600/3.pe2pkg` — load `0x80131E20` — 74.2 KiB | Not started |
| `stage0_013` | Stage 0 overlay (file id 101900) — source `stage0/file101900/3.pe2pkg` — load `0x80131E20` — 92.8 KiB | Not started |
| `stage0_014` | Stage 0 overlay (file id 10200) — source `stage0/file10200/2.pe2pkg` — load `0x80115770` — 30.4 KiB | Not started |
| `stage0_015` | Stage 0 overlay (file id 102000) — source `stage0/file102000/3.pe2pkg` — load `0x80131E20` — 88.2 KiB | Not started |
| `stage0_016` | Stage 0 overlay (file id 102100) — source `stage0/file102100/3.pe2pkg` — load `0x80131E20` — 15.7 KiB | Not started |
| `stage0_017` | Stage 0 overlay (file id 102300) — source `stage0/file102300/3.pe2pkg` — load `0x80131E20` — 87.4 KiB | Not started |
| `stage0_018` | Stage 0 overlay (file id 102400) — source `stage0/file102400/3.pe2pkg` — load `0x80131E20` — 17.6 KiB | Not started |
| `stage0_019` | Stage 0 overlay (file id 102500) — source `stage0/file102500/3.pe2pkg` — load `0x80131E20` — 23.0 KiB | Not started |
| `stage0_020` | Stage 0 overlay (file id 102600) — source `stage0/file102600/3.pe2pkg` — load `0x80131E20` — 34.8 KiB | Not started |
| `stage0_021` | Stage 0 overlay (file id 10300) — source `stage0/file10300/2.pe2pkg` — load `0x80115770` — 29.5 KiB | Not started |
| `stage0_022` | Stage 0 overlay (file id 10301) — source `stage0/file10301/3.pe2pkg` — load `0x8011D1C0` — 54.0 KiB | Not started |
| `stage0_023` | Stage 0 overlay (file id 10302) — source `stage0/file10302/3.pe2pkg` — load `0x8011D1C0` — 55.4 KiB | Not started |
| `stage0_024` | Stage 0 overlay (file id 10303) — source `stage0/file10303/3.pe2pkg` — load `0x8011D1C0` — 58.5 KiB | Not started |
| `stage0_025` | Stage 0 overlay (file id 10304) — source `stage0/file10304/3.pe2pkg` — load `0x8011D1C0` — 54.0 KiB | Not started |
| `stage0_026` | Stage 0 overlay (file id 10305) — source `stage0/file10305/3.pe2pkg` — load `0x8011D1C0` — 57.2 KiB | Not started |
| `stage0_027` | Stage 0 overlay (file id 10306) — source `stage0/file10306/2.pe2pkg` — load `0x8011D1C0` — 53.0 KiB | Not started |
| `stage0_028` | Stage 0 overlay (file id 10307) — source `stage0/file10307/2.pe2pkg` — load `0x8011D1C0` — 52.1 KiB | Not started |
| `stage0_029` | Stage 0 overlay (file id 10308) — source `stage0/file10308/2.pe2pkg` — load `0x8011D1C0` — 52.1 KiB | Not started |
| `stage0_030` | Stage 0 overlay (file id 10309) — source `stage0/file10309/3.pe2pkg` — load `0x8011D1C0` — 54.3 KiB | Not started |
| `stage0_031` | Stage 0 overlay (file id 10310) — source `stage0/file10310/2.pe2pkg` — load `0x8011D1C0` — 52.1 KiB | Not started |
| `stage0_032` | Stage 0 overlay (file id 10311) — source `stage0/file10311/2.pe2pkg` — load `0x8011D1C0` — 57.0 KiB | Not started |
| `stage0_033` | Stage 0 overlay (file id 10312) — source `stage0/file10312/2.pe2pkg` — load `0x8011D1C0` — 64.7 KiB | Not started |
| `stage0_034` | Stage 0 overlay (file id 10313) — source `stage0/file10313/2.pe2pkg` — load `0x8011D1C0` — 60.3 KiB | Not started |
| `stage0_035` | Stage 0 overlay (file id 10314) — source `stage0/file10314/2.pe2pkg` — load `0x8011D1C0` — 59.1 KiB | Not started |
| `stage0_036` | Stage 0 overlay (file id 10315) — source `stage0/file10315/2.pe2pkg` — load `0x8011D1C0` — 60.6 KiB | Not started |
| `stage0_037` | Stage 0 overlay (file id 10316) — source `stage0/file10316/3.pe2pkg` — load `0x8011D1C0` — 58.3 KiB | Not started |
| `stage0_038` | Stage 0 overlay (file id 10317) — source `stage0/file10317/3.pe2pkg` — load `0x8011D1C0` — 63.7 KiB | Not started |
| `stage0_039` | Stage 0 overlay (file id 10318) — source `stage0/file10318/2.pe2pkg` — load `0x8011D1C0` — 52.1 KiB | Not started |
| `stage0_040` | Stage 0 overlay (file id 10319) — source `stage0/file10319/3.pe2pkg` — load `0x8011D1C0` — 59.8 KiB | Not started |
| `stage0_041` | Stage 0 overlay (file id 10320) — source `stage0/file10320/3.pe2pkg` — load `0x8011D1C0` — 58.3 KiB | Not started |
| `stage0_042` | Stage 0 overlay (file id 10321) — source `stage0/file10321/3.pe2pkg` — load `0x8011D1C0` — 58.3 KiB | Not started |
| `stage0_043` | Stage 0 overlay (file id 10322) — source `stage0/file10322/3.pe2pkg` — load `0x8011D1C0` — 71.4 KiB | Not started |
| `stage0_044` | Stage 0 overlay (file id 10323) — source `stage0/file10323/2.pe2pkg` — load `0x8011D1C0` — 69.4 KiB | Not started |
| `stage0_045` | Stage 0 overlay (file id 10324) — source `stage0/file10324/2.pe2pkg` — load `0x8011D1C0` — 52.1 KiB | Not started |
| `stage0_046` | Stage 0 overlay (file id 10325) — source `stage0/file10325/3.pe2pkg` — load `0x8011D1C0` — 65.2 KiB | Not started |
| `stage0_047` | Stage 0 overlay (file id 10326) — source `stage0/file10326/3.pe2pkg` — load `0x8011D1C0` — 65.7 KiB | Not started |
| `stage0_048` | Stage 0 overlay (file id 10327) — source `stage0/file10327/2.pe2pkg` — load `0x8011D1C0` — 68.1 KiB | Not started |
| `stage0_049` | Stage 0 overlay (file id 10328) — source `stage0/file10328/3.pe2pkg` — load `0x8011D1C0` — 65.1 KiB | Not started |
| `stage0_050` | Stage 0 overlay (file id 10329) — source `stage0/file10329/3.pe2pkg` — load `0x8011D1C0` — 70.4 KiB | Not started |
| `stage0_051` | Stage 0 overlay (file id 10330) — source `stage0/file10330/3.pe2pkg` — load `0x8011D1C0` — 56.8 KiB | Not started |
| `stage0_052` | Stage 0 overlay (file id 10331) — source `stage0/file10331/3.pe2pkg` — load `0x8011D1C0` — 56.8 KiB | Not started |
| `stage0_053` | Stage 0 overlay (file id 10332) — source `stage0/file10332/3.pe2pkg` — load `0x8011D1C0` — 56.8 KiB | Not started |
| `stage0_054` | Stage 0 overlay (file id 103700) — source `stage0/file103700/3.pe2pkg` — load `0x80131E20` — 32.3 KiB | Not started |
| `stage0_055` | Stage 0 overlay (file id 103800) — source `stage0/file103800/3.pe2pkg` — load `0x80131E20` — 23.9 KiB | Not started |
| `stage0_056` | Stage 0 overlay (file id 10400) — source `stage0/file10400/2.pe2pkg` — load `0x80115770` — 29.4 KiB | Not started |
| `stage0_057` | Stage 0 overlay (file id 104000) — source `stage0/file104000/3.pe2pkg` — load `0x80131E20` — 49.8 KiB | Not started |
| `stage0_058` | Empty placeholder package — source `stage0/file104100/3.pe2pkg` — load `0x80131E20` — 4 B | Not started |
| `stage0_059` | Stage 0 overlay (file id 104400) — source `stage0/file104400/2.pe2pkg` — load `0x80131E20` — 66.1 KiB | Not started |
| `stage0_060` | Empty placeholder package — source `stage0/file104500/2.pe2pkg` — load `0x80131E20` — 4 B | Not started |
| `stage0_061` | Stage 0 overlay (file id 104600) — source `stage0/file104600/3.pe2pkg` — load `0x80131E20` — 25.2 KiB | Not started |
| `stage0_062` | Stage 0 overlay (file id 104900) — source `stage0/file104900/3.pe2pkg` — load `0x80131E20` — 85.6 KiB | Not started |
| `stage0_063` | Stage 0 overlay (file id 10500) — source `stage0/file10500/2.pe2pkg` — load `0x80115770` — 30.6 KiB | Not started |
| `stage0_064` | Stage 0 overlay (file id 105100) — source `stage0/file105100/3.pe2pkg` — load `0x80131E20` — 61.7 KiB | Not started |
| `stage0_065` | Stage 0 overlay (file id 105300) — source `stage0/file105300/3.pe2pkg` — load `0x80131E20` — 45.5 KiB | Not started |
| `stage0_066` | Stage 0 overlay (file id 105400) — source `stage0/file105400/3.pe2pkg` — load `0x80131E20` — 44.2 KiB | Not started |
| `stage0_067` | Stage 0 overlay (file id 105500) — source `stage0/file105500/3.pe2pkg` — load `0x80131E20` — 34.8 KiB | Not started |
| `stage0_068` | Stage 0 overlay (file id 105600) — source `stage0/file105600/3.pe2pkg` — load `0x80131E20` — 89.4 KiB | Not started |
| `stage0_069` | Stage 0 overlay (file id 105700) — source `stage0/file105700/3.pe2pkg` — load `0x80131E20` — 93.2 KiB | Not started |
| `stage0_070` | Stage 0 overlay (file id 107000) — source `stage0/file107000/3.pe2pkg` — load `0x80131E20` — 54.0 KiB | Not started |
| `stage0_071` | Empty placeholder package — source `stage0/file107200/3.pe2pkg` — load `0x80131E20` — 4 B | Not started |
| `stage0_072` | Empty placeholder package — source `stage0/file107500/3.pe2pkg` — load `0x80131E20` — 4 B | Not started |
| `stage0_073` | Stage 0 overlay (file id 107600) — source `stage0/file107600/6.pe2pkg` — load `0x80131E20` — 14.3 KiB | Not started |
| `stage0_074` | Empty placeholder package — source `stage0/file109300/2.pe2pkg` — load `0x80131E20` — 4 B | Not started |
| `stage0_075` | Stage 0 overlay (file id 110300) — source `stage0/file110300/4.pe2pkg` — load `0x80131E20` — 32.6 KiB | Not started |
| `stage0_076` | Scene graph / root-count logic — source `stage0/file110600/3.pe2pkg` — load `0x80131E20` — 90.1 KiB | Not started |
| `stage0_077` | Stage 0 overlay (file id 110700) — source `stage0/file110700/3.pe2pkg` — load `0x80131E20` — 40.4 KiB | Not started |
| `stage0_078` | Stage 0 overlay (file id 110800) — source `stage0/file110800/2.pe2pkg` — load `0x80131E20` — 32.2 KiB | Not started |
| `stage0_079` | Stage 0 overlay (file id 111800) — source `stage0/file111800/3.pe2pkg` — load `0x80131E20` — 33.6 KiB | Not started |
| `stage0_080` | Stage 0 overlay (file id 113000) — source `stage0/file113000/2.pe2pkg` — load `0x80131E20` — 35.4 KiB | Not started |
| `stage0_081` | Stage 0 overlay (file id 113100) — source `stage0/file113100/2.pe2pkg` — load `0x80131E20` — 73.3 KiB | Not started |
| `stage0_082` | Stage 0 overlay (file id 120300) — source `stage0/file120300/4.pe2pkg` — load `0x80131E20` — 63.4 KiB | Not started |
| `stage0_083` | Stage 0 overlay (file id 120400) — source `stage0/file120400/2.pe2pkg` — load `0x80131E20` — 50.4 KiB | Not started |
| `stage0_084` | Stage 0 overlay (file id 120500) — source `stage0/file120500/2.pe2pkg` — load `0x80131E20` — 25.6 KiB | Not started |
| `stage0_085` | Stage 0 overlay (file id 121300) — source `stage0/file121300/2.pe2pkg` — load `0x80131E20` — 45.8 KiB | Not started |
| `stage0_086` | Empty placeholder package — source `stage0/file123000/3.pe2pkg` — load `0x80131E20` — 4 B | Not started |
| `stage0_087` | Stage 0 overlay (file id 123200) — source `stage0/file123200/2.pe2pkg` — load `0x80131E20` — 21.0 KiB | Not started |
| `stage0_088` | Empty placeholder package — source `stage0/file123400/3.pe2pkg` — load `0x80131E20` — 4 B | Not started |
| `stage0_089` | Empty placeholder package — source `stage0/file135000/2.pe2pkg` — load `0x80131E20` — 4 B | Not started |
| `stage0_090` | Stage 0 overlay (file id 135400) — source `stage0/file135400/2.pe2pkg` — load `0x80131E20` — 54.8 KiB | Not started |
| `stage0_091` | Stage 0 overlay (file id 135600) — source `stage0/file135600/2.pe2pkg` — load `0x80131E20` — 36.8 KiB | Not started |
| `stage0_092` | Stage 0 overlay (file id 136100) — source `stage0/file136100/4.pe2pkg` — load `0x80131E20` — 58.4 KiB | Not started |
| `stage0_093` | Stage 0 overlay (file id 136300) — source `stage0/file136300/1.pe2pkg` — load `0x80131E20` — 42.9 KiB | Not started |
| `stage0_094` | Empty placeholder package — source `stage0/file140400/2.pe2pkg` — load `0x80131E20` — 4 B | Not started |
| `stage0_095` | Stage 0 overlay (file id 141000) — source `stage0/file141000/4.pe2pkg` — load `0x80131E20` — 46.4 KiB | Not started |
| `stage0_096` | Stage 0 overlay (file id 142600) — source `stage0/file142600/0.pe2pkg` — load `0x80131E20` — 18.8 KiB | Not started |
| `stage0_097` | Stage 0 overlay (file id 142900) — source `stage0/file142900/0.pe2pkg` — load `0x80131E20` — 25.1 KiB | Not started |
| `stage0_098` | Stage 0 overlay (file id 143000) — source `stage0/file143000/1.pe2pkg` — load `0x80131E20` — 15.5 KiB | Not started |
| `stage0_099` | Stage 0 overlay (file id 143400) — source `stage0/file143400/0.pe2pkg` — load `0x80131E20` — 15.4 KiB | Not started |
| `stage0_100` | Stage 0 overlay (file id 143900) — source `stage0/file143900/2.pe2pkg` — load `0x80131E20` — 94.2 KiB | Not started |
| `stage0_101` | Stage 0 overlay (file id 146000) — source `stage0/file146000/0.pe2pkg` — load `0x80131E20` — 15.8 KiB | Not started |
| `stage0_102` | Stage 0 overlay (file id 146300) — source `stage0/file146300/2.pe2pkg` — load `0x80131E20` — 66.5 KiB | Not started |
| `stage0_103` | Stage 0 overlay (file id 150400) — source `stage0/file150400/3.pe2pkg` — load `0x80131E20` — 42.8 KiB | Not started |
| `stage0_104` | Stage 0 overlay (file id 151000) — source `stage0/file151000/2.pe2pkg` — load `0x80131E20` — 45.3 KiB | Not started |
| `stage0_105` | Stage 0 overlay (file id 160600) — source `stage0/file160600/2.pe2pkg` — load `0x80131E20` — 48.5 KiB | Not started |
| `stage0_106` | Stage 0 overlay (file id 160700) — source `stage0/file160700/2.pe2pkg` — load `0x80131E20` — 62.3 KiB | Not started |
| `stage0_107` | Stage 0 overlay (file id 160900) — source `stage0/file160900/0.pe2pkg` — load `0x80131E20` — 55.7 KiB | Not started |
| `stage0_108` | Stage 0 overlay (file id 161500) — source `stage0/file161500/4.pe2pkg` — load `0x80131E20` — 57.0 KiB | Not started |
| `stage0_109` | Stage 0 overlay (file id 200300) — source `stage0/file200300/3.pe2pkg` — load `0x80149E20` — 88.9 KiB | Not started |
| `stage0_110` | Stage 0 overlay (file id 200400) — source `stage0/file200400/3.pe2pkg` — load `0x80149E20` — 88.2 KiB | Not started |
| `stage0_111` | Stage 0 overlay (file id 200700) — source `stage0/file200700/3.pe2pkg` — load `0x80149E20` — 29.5 KiB | Not started |
| `stage0_112` | Options / key configuration — source `stage0/file20100/0.pe2pkg` — load `0x801D4000` — 7.8 KiB | Not started |
| `stage0_113` | Empty placeholder package — source `stage0/file201000/3.pe2pkg` — load `0x80149E20` — 4 B | Not started |
| `stage0_114` | Stage 0 overlay (file id 201100) — source `stage0/file201100/2.pe2pkg` — load `0x80149E20` — 85.6 KiB | Not started |
| `stage0_115` | Stage 0 overlay (file id 201200) — source `stage0/file201200/3.pe2pkg` — load `0x80149E20` — 28.1 KiB | Not started |
| `stage0_116` | Empty placeholder package — source `stage0/file201300/3.pe2pkg` — load `0x80149E20` — 4 B | Not started |
| `stage0_117` | Stage 0 overlay (file id 201500) — source `stage0/file201500/3.pe2pkg` — load `0x80149E20` — 40.2 KiB | Not started |
| `stage0_118` | Stage 0 overlay (file id 201600) — source `stage0/file201600/3.pe2pkg` — load `0x80149E20` — 74.2 KiB | Not started |
| `stage0_119` | Empty placeholder package — source `stage0/file201800/3.pe2pkg` — load `0x80149E20` — 4 B | Not started |
| `stage0_120` | Empty placeholder package — source `stage0/file201900/3.pe2pkg` — load `0x80149E20` — 4 B | Not started |
| `stage0_121` | Empty placeholder package — source `stage0/file20200/0.pe2pkg` — load `0x801D4000` — 4 B | Not started |
| `stage0_122` | Stage 0 overlay (file id 202000) — source `stage0/file202000/3.pe2pkg` — load `0x80149E20` — 88.2 KiB | Not started |
| `stage0_123` | Stage 0 overlay (file id 202100) — source `stage0/file202100/3.pe2pkg` — load `0x80149E20` — 15.7 KiB | Not started |
| `stage0_124` | Stage 0 overlay (file id 202300) — source `stage0/file202300/3.pe2pkg` — load `0x80149E20` — 87.4 KiB | Not started |
| `stage0_125` | Stage 0 overlay (file id 202400) — source `stage0/file202400/3.pe2pkg` — load `0x80149E20` — 17.6 KiB | Not started |
| `stage0_126` | Stage 0 overlay (file id 202500) — source `stage0/file202500/3.pe2pkg` — load `0x80149E20` — 23.0 KiB | Not started |
| `stage0_127` | Stage 0 overlay (file id 202600) — source `stage0/file202600/3.pe2pkg` — load `0x80149E20` — 34.8 KiB | Not started |
| `stage0_128` | Stage 0 overlay (file id 202900) — source `stage0/file202900/2.pe2pkg` — load `0x80149E20` — 52.1 KiB | Not started |
| `stage0_129` | Stage 0 overlay (file id 203700) — source `stage0/file203700/3.pe2pkg` — load `0x80149E20` — 32.3 KiB | Not started |
| `stage0_130` | Stage 0 overlay (file id 203800) — source `stage0/file203800/3.pe2pkg` — load `0x80149E20` — 23.9 KiB | Not started |
| `stage0_131` | Stage 0 overlay (file id 204000) — source `stage0/file204000/3.pe2pkg` — load `0x80149E20` — 49.8 KiB | Not started |
| `stage0_132` | Empty placeholder package — source `stage0/file204100/3.pe2pkg` — load `0x80149E20` — 4 B | Not started |
| `stage0_133` | Empty placeholder package — source `stage0/file204400/2.pe2pkg` — load `0x80149E20` — 4 B | Not started |
| `stage0_134` | Stage 0 overlay (file id 204600) — source `stage0/file204600/3.pe2pkg` — load `0x80149E20` — 25.2 KiB | Not started |
| `stage0_135` | Stage 0 overlay (file id 204900) — source `stage0/file204900/3.pe2pkg` — load `0x80149E20` — 85.6 KiB | Not started |
| `stage0_136` | Stage 0 overlay (file id 205200) — source `stage0/file205200/1.pe2pkg` — load `0x80149E20` — 69.6 KiB | Not started |
| `stage0_137` | Stage 0 overlay (file id 205500) — source `stage0/file205500/3.pe2pkg` — load `0x80149E20` — 34.8 KiB | Not started |
| `stage0_138` | Stage 0 overlay (file id 205600) — source `stage0/file205600/3.pe2pkg` — load `0x80149E20` — 89.4 KiB | Not started |
| `stage0_139` | Stage 0 overlay (file id 205700) — source `stage0/file205700/3.pe2pkg` — load `0x80149E20` — 93.2 KiB | Not started |
| `stage0_140` | Stage 0 overlay (file id 20600) — source `stage0/file20600/0.pe2pkg` — load `0x801D6000` — 1.4 KiB | Not started |
| `stage0_141` | Stage 0 overlay (file id 206100) — source `stage0/file206100/3.pe2pkg` — load `0x80149E20` — 59.7 KiB | Not started |
| `stage0_142` | Stage 0 overlay (file id 207000) — source `stage0/file207000/3.pe2pkg` — load `0x80149E20` — 54.0 KiB | Not started |
| `stage0_143` | Stage 0 overlay (file id 207200) — source `stage0/file207200/3.pe2pkg` — load `0x80149E20` — 40.3 KiB | Not started |
| `stage0_144` | Stage 0 overlay (file id 20900) — source `stage0/file20900/4.pe2pkg` — load `0x80115770` — 1.6 KiB | Not started |
| `stage0_145` | Bonus / item reward UI — source `stage0/file21000/6.pe2pkg` — load `0x80115770` — 14.8 KiB | Not started |
| `stage0_146` | Stage 0 overlay (file id 210600) — source `stage0/file210600/2.pe2pkg` — load `0x80149E20` — 77.2 KiB | Not started |
| `stage0_147` | Stage 0 overlay (file id 210700) — source `stage0/file210700/2.pe2pkg` — load `0x80149E20` — 58.0 KiB | Not started |
| `stage0_148` | Stage 0 overlay (file id 213000) — source `stage0/file213000/2.pe2pkg` — load `0x80149E20` — 56.0 KiB | Not started |
| `stage0_149` | Stage 0 overlay (file id 213100) — source `stage0/file213100/2.pe2pkg` — load `0x80149E20` — 33.0 KiB | Not started |
| `stage0_150` | Stage 0 overlay (file id 215100) — source `stage0/file215100/2.pe2pkg` — load `0x80149E20` — 82.1 KiB | Not started |
| `stage0_151` | Stage 0 overlay (file id 223600) — source `stage0/file223600/3.pe2pkg` — load `0x80149E20` — 27.3 KiB | Not started |
| `stage0_152` | Stage 0 overlay (file id 260400) — source `stage0/file260400/3.pe2pkg` — load `0x80149E20` — 43.6 KiB | Not started |
| `stage0_153` | Stage 0 overlay (file id 260500) — source `stage0/file260500/3.pe2pkg` — load `0x80149E20` — 64.1 KiB | Not started |
| `stage0_154` | Empty placeholder package — source `stage0/file300300/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_155` | Empty placeholder package — source `stage0/file300400/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_156` | Stage 0 overlay (file id 300700) — source `stage0/file300700/3.pe2pkg` — load `0x80161E20` — 29.5 KiB | Not started |
| `stage0_157` | Stage 0 overlay (file id 30100) — source `stage0/file30100/2.pe2pkg` — load `0x8012EF30` — 3.0 KiB | Not started |
| `stage0_158` | Empty placeholder package — source `stage0/file301000/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_159` | Stage 0 overlay (file id 30102) — source `stage0/file30102/2.pe2pkg` — load `0x8012EF30` — 148 B | Not started |
| `stage0_160` | Stage 0 overlay (file id 301100) — source `stage0/file301100/2.pe2pkg` — load `0x80161E20` — 85.6 KiB | Not started |
| `stage0_161` | Stage 0 overlay (file id 301200) — source `stage0/file301200/3.pe2pkg` — load `0x80161E20` — 28.1 KiB | Not started |
| `stage0_162` | Empty placeholder package — source `stage0/file301300/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_163` | Stage 0 overlay (file id 301500) — source `stage0/file301500/3.pe2pkg` — load `0x80161E20` — 40.2 KiB | Not started |
| `stage0_164` | Stage 0 overlay (file id 301600) — source `stage0/file301600/3.pe2pkg` — load `0x80161E20` — 74.2 KiB | Not started |
| `stage0_165` | Empty placeholder package — source `stage0/file301800/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_166` | Stage 0 overlay (file id 301900) — source `stage0/file301900/3.pe2pkg` — load `0x80161E20` — 92.8 KiB | Not started |
| `stage0_167` | Stage 0 overlay (file id 30200) — source `stage0/file30200/2.pe2pkg` — load `0x8012EF30` — 944 B | Not started |
| `stage0_168` | Stage 0 overlay (file id 302000) — source `stage0/file302000/3.pe2pkg` — load `0x80161E20` — 88.2 KiB | Not started |
| `stage0_169` | Stage 0 overlay (file id 30202) — source `stage0/file30202/2.pe2pkg` — load `0x8012EF30` — 3.7 KiB | Not started |
| `stage0_170` | Stage 0 overlay (file id 30203) — source `stage0/file30203/2.pe2pkg` — load `0x8012EF30` — 628 B | Not started |
| `stage0_171` | Empty placeholder package — source `stage0/file302100/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_172` | Empty placeholder package — source `stage0/file302300/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_173` | Empty placeholder package — source `stage0/file302400/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_174` | Stage 0 overlay (file id 302500) — source `stage0/file302500/3.pe2pkg` — load `0x80161E20` — 23.0 KiB | Not started |
| `stage0_175` | Stage 0 overlay (file id 302600) — source `stage0/file302600/3.pe2pkg` — load `0x80161E20` — 34.8 KiB | Not started |
| `stage0_176` | Stage 0 overlay (file id 30300) — source `stage0/file30300/2.pe2pkg` — load `0x8012EF30` — 944 B | Not started |
| `stage0_177` | Stage 0 overlay (file id 30302) — source `stage0/file30302/2.pe2pkg` — load `0x8012EF30` — 3.7 KiB | Not started |
| `stage0_178` | Stage 0 overlay (file id 30303) — source `stage0/file30303/2.pe2pkg` — load `0x8012EF30` — 628 B | Not started |
| `stage0_179` | Stage 0 overlay (file id 303600) — source `stage0/file303600/0.pe2pkg` — load `0x80161E20` — 49.7 KiB | Not started |
| `stage0_180` | Empty placeholder package — source `stage0/file303700/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_181` | Empty placeholder package — source `stage0/file303800/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_182` | Stage 0 overlay (file id 30400) — source `stage0/file30400/2.pe2pkg` — load `0x8012EF30` — 460 B | Not started |
| `stage0_183` | Stage 0 overlay (file id 304000) — source `stage0/file304000/3.pe2pkg` — load `0x80161E20` — 49.8 KiB | Not started |
| `stage0_184` | Stage 0 overlay (file id 30402) — source `stage0/file30402/2.pe2pkg` — load `0x8012EF30` — 896 B | Not started |
| `stage0_185` | Stage 0 overlay (file id 30403) — source `stage0/file30403/2.pe2pkg` — load `0x8012EF30` — 3.0 KiB | Not started |
| `stage0_186` | Stage 0 overlay (file id 30404) — source `stage0/file30404/2.pe2pkg` — load `0x8012EF30` — 1.9 KiB | Not started |
| `stage0_187` | Stage 0 overlay (file id 30405) — source `stage0/file30405/2.pe2pkg` — load `0x8012EF30` — 1.0 KiB | Not started |
| `stage0_188` | Stage 0 overlay (file id 30406) — source `stage0/file30406/2.pe2pkg` — load `0x8012EF30` — 852 B | Not started |
| `stage0_189` | Empty placeholder package — source `stage0/file304100/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_190` | Empty placeholder package — source `stage0/file304400/2.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_191` | Empty placeholder package — source `stage0/file304600/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_192` | Empty placeholder package — source `stage0/file304900/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_193` | Stage 0 overlay (file id 30500) — source `stage0/file30500/2.pe2pkg` — load `0x8012EF30` — 3.9 KiB | Not started |
| `stage0_194` | Stage 0 overlay (file id 30502) — source `stage0/file30502/2.pe2pkg` — load `0x8012EF30` — 936 B | Not started |
| `stage0_195` | Stage 0 overlay (file id 30503) — source `stage0/file30503/2.pe2pkg` — load `0x8012EF30` — 1.1 KiB | Not started |
| `stage0_196` | Empty placeholder package — source `stage0/file305500/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_197` | Empty placeholder package — source `stage0/file305600/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_198` | Empty placeholder package — source `stage0/file305700/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_199` | Empty placeholder package — source `stage0/file307000/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_200` | Empty placeholder package — source `stage0/file307200/3.pe2pkg` — load `0x80161E20` — 4 B | Not started |
| `stage0_201` | Stage 0 overlay (file id 310100) — source `stage0/file310100/0.pe2pkg` — load `0x80161E20` — 94.8 KiB | Not started |
| `stage0_202` | Stage 0 overlay (file id 310600) — source `stage0/file310600/4.pe2pkg` — load `0x80161E20` — 94.2 KiB | Not started |
| `stage0_203` | Stage 0 overlay (file id 311500) — source `stage0/file311500/3.pe2pkg` — load `0x80161E20` — 29.3 KiB | Not started |
| `stage0_204` | Stage 0 overlay (file id 311900) — source `stage0/file311900/2.pe2pkg` — load `0x80161E20` — 53.5 KiB | Not started |
| `stage0_205` | Stage 0 overlay (file id 312200) — source `stage0/file312200/2.pe2pkg` — load `0x80161E20` — 32.4 KiB | Not started |
| `stage0_206` | Stage 0 overlay (file id 317000) — source `stage0/file317000/2.pe2pkg` — load `0x80161E20` — 44.3 KiB | Not started |
| `stage0_207` | Stage 0 overlay (file id 323000) — source `stage0/file323000/2.pe2pkg` — load `0x80161E20` — 71.1 KiB | Not started |
| `stage0_208` | Stage 0 overlay (file id 323300) — source `stage0/file323300/2.pe2pkg` — load `0x80161E20` — 75.2 KiB | Not started |
| `stage0_209` | Stage 0 overlay (file id 323400) — source `stage0/file323400/2.pe2pkg` — load `0x80161E20` — 61.1 KiB | Not started |
| `stage0_210` | Stage 0 overlay (file id 335800) — source `stage0/file335800/4.pe2pkg` — load `0x80161E20` — 68.2 KiB | Not started |
| `stage0_211` | Stage 0 overlay (file id 341300) — source `stage0/file341300/0.pe2pkg` — load `0x80161E20` — 15.1 KiB | Not started |
| `stage0_212` | Stage 0 overlay (file id 341700) — source `stage0/file341700/0.pe2pkg` — load `0x80161E20` — 81.3 KiB | Not started |
| `stage0_213` | Stage 0 overlay (file id 341900) — source `stage0/file341900/0.pe2pkg` — load `0x80161E20` — 9.0 KiB | Not started |
| `stage0_214` | Stage 0 overlay (file id 342000) — source `stage0/file342000/0.pe2pkg` — load `0x80161E20` — 12.6 KiB | Not started |
| `stage0_215` | Stage 0 overlay (file id 342100) — source `stage0/file342100/0.pe2pkg` — load `0x80161E20` — 30.5 KiB | Not started |
| `stage0_216` | Stage 0 overlay (file id 342200) — source `stage0/file342200/0.pe2pkg` — load `0x80161E20` — 66.1 KiB | Not started |
| `stage0_217` | Stage 0 overlay (file id 342400) — source `stage0/file342400/2.pe2pkg` — load `0x80161E20` — 71.1 KiB | Not started |
| `stage0_218` | Stage 0 overlay (file id 350500) — source `stage0/file350500/2.pe2pkg` — load `0x80161E20` — 28.2 KiB | Not started |
| `stage0_219` | Stage 0 overlay (file id 350700) — source `stage0/file350700/4.pe2pkg` — load `0x80161E20` — 58.8 KiB | Not started |
| `stage0_220` | Stage 0 overlay (file id 356100) — source `stage0/file356100/2.pe2pkg` — load `0x80161E20` — 69.2 KiB | Not started |
| `stage0_221` | Stage 0 overlay (file id 361100) — source `stage0/file361100/2.pe2pkg` — load `0x80161E20` — 63.4 KiB | Not started |
| `stage0_222` | Player work / EM01 debug overlay — source `stage0/file400100/3.pe2pkg` — load `0x80131E20` — 111 KiB | Not started |
| `stage0_223` | Stage 0 overlay (file id 400500) — source `stage0/file400500/3.pe2pkg` — load `0x80131E20` — 136 KiB | Not started |
| `stage0_224` | Stage 0 overlay (file id 400600) — source `stage0/file400600/2.pe2pkg` — load `0x80131E20` — 127 KiB | Not started |
| `stage0_225` | Stage 0 overlay (file id 401000) — source `stage0/file401000/3.pe2pkg` — load `0x80131E20` — 141 KiB | Not started |
| `stage0_226` | Stage 0 overlay (file id 401300) — source `stage0/file401300/3.pe2pkg` — load `0x80131E20` — 155 KiB | Not started |
| `stage0_227` | Stage 0 overlay (file id 401800) — source `stage0/file401800/3.pe2pkg` — load `0x80131E20` — 143 KiB | Not started |
| `stage0_228` | Stage 0 overlay (file id 402200) — source `stage0/file402200/2.pe2pkg` — load `0x80131E20` — 137 KiB | Not started |
| `stage0_229` | Stage 0 overlay (file id 403000) — source `stage0/file403000/5.pe2pkg` — load `0x80131E20` — 156 KiB | Not started |
| `stage0_230` | Stage 0 overlay (file id 403100) — source `stage0/file403100/5.pe2pkg` — load `0x80131E20` — 149 KiB | Not started |
| `stage0_231` | Stage 0 overlay (file id 403200) — source `stage0/file403200/5.pe2pkg` — load `0x80131E20` — 183 KiB | Not started |
| `stage0_232` | Stage 0 overlay (file id 403600) — source `stage0/file403600/0.pe2pkg` — load `0x80131E20` — 186 KiB | Not started |
| `stage0_233` | Stage 0 overlay (file id 403900) — source `stage0/file403900/2.pe2pkg` — load `0x80131E20` — 137 KiB | Not started |
| `stage0_234` | Stage 0 overlay (file id 405800) — source `stage0/file405800/3.pe2pkg` — load `0x80131E20` — 126 KiB | Not started |
| `stage0_235` | Player work / EM01 debug overlay — source `stage0/file407500/3.pe2pkg` — load `0x80131E20` — 111 KiB | Not started |
| `stage0_236` | Stage 0 overlay (file id 420700) — source `stage0/file420700/4.pe2pkg` — load `0x80131E20` — 52.5 KiB | Not started |
| `stage0_237` | Stage 0 overlay (file id 421600) — source `stage0/file421600/3.pe2pkg` — load `0x80131E20` — 125 KiB | Not started |
| `stage0_238` | Stage 0 overlay (file id 443500) — source `stage0/file443500/2.pe2pkg` — load `0x80131E20` — 154 KiB | Not started |
| `stage0_239` | Stage 0 overlay (file id 444000) — source `stage0/file444000/5.pe2pkg` — load `0x80131E20` — 191 KiB | Not started |
| `stage0_240` | Stage 0 overlay (file id 450200) — source `stage0/file450200/2.pe2pkg` — load `0x80131E20` — 58.9 KiB | Not started |
| `stage0_241` | Stage 0 overlay (file id 450800) — source `stage0/file450800/2.pe2pkg` — load `0x80131E20` — 135 KiB | Not started |
| `stage0_242` | Stage 0 overlay (file id 450900) — source `stage0/file450900/0.pe2pkg` — load `0x80131E20` — 19.6 KiB | Not started |
| `stage0_243` | Stage 0 overlay (file id 451100) — source `stage0/file451100/2.pe2pkg` — load `0x80131E20` — 114 KiB | Not started |
| `stage0_244` | Stage 0 overlay (file id 460200) — source `stage0/file460200/2.pe2pkg` — load `0x80131E20` — 126 KiB | Not started |
| `stage0_245` | Stage 0 overlay (file id 461800) — source `stage0/file461800/2.pe2pkg` — load `0x80131E20` — 70.6 KiB | Not started |
| `stage0_246` | Stage 0 overlay (file id 50100) — source `stage0/file50100/1.pe2pkg` — load `0x8012EF30` — 11.7 KiB | Not started |
| `stage0_247` | Stage 0 overlay (file id 50104) — source `stage0/file50104/1.pe2pkg` — load `0x8012EF30` — 6.6 KiB | Not started |
| `stage0_248` | Stage 0 overlay (file id 50107) — source `stage0/file50107/1.pe2pkg` — load `0x8012EF30` — 5.4 KiB | Not started |
| `stage0_249` | Stage 0 overlay (file id 50110) — source `stage0/file50110/1.pe2pkg` — load `0x8012EF30` — 5.9 KiB | Not started |
| `stage0_250` | Stage 0 overlay (file id 50113) — source `stage0/file50113/1.pe2pkg` — load `0x8012EF30` — 4.1 KiB | Not started |
| `stage0_251` | Stage 0 overlay (file id 50116) — source `stage0/file50116/1.pe2pkg` — load `0x8012EF30` — 7.1 KiB | Not started |
| `stage0_252` | Stage 0 overlay (file id 50119) — source `stage0/file50119/1.pe2pkg` — load `0x8012EF30` — 3.1 KiB | Not started |
| `stage0_253` | Stage 0 overlay (file id 50122) — source `stage0/file50122/1.pe2pkg` — load `0x8012EF30` — 3.3 KiB | Not started |
| `stage0_254` | Stage 0 overlay (file id 50125) — source `stage0/file50125/1.pe2pkg` — load `0x8012EF30` — 7.0 KiB | Not started |
| `stage0_255` | Stage 0 overlay (file id 50128) — source `stage0/file50128/1.pe2pkg` — load `0x8012EF30` — 7.2 KiB | Not started |
| `stage0_256` | Stage 0 overlay (file id 50131) — source `stage0/file50131/1.pe2pkg` — load `0x8012EF30` — 4.5 KiB | Not started |
| `stage0_257` | Stage 0 overlay (file id 50134) — source `stage0/file50134/1.pe2pkg` — load `0x8012EF30` — 8.6 KiB | Not started |
| `stage0_258` | Stage 0 overlay (file id 50146) — source `stage0/file50146/1.pe2pkg` — load `0x8012EF30` — 820 B | Not started |
| `stage0_259` | Stage 0 overlay (file id 50149) — source `stage0/file50149/1.pe2pkg` — load `0x8012EF30` — 1.9 KiB | Not started |
| `stage0_260` | Stage 0 overlay (file id 50152) — source `stage0/file50152/1.pe2pkg` — load `0x8012EF30` — 3.1 KiB | Not started |
| `stage0_261` | Stage 0 overlay (file id 503500) — source `stage0/file503500/7.pe2pkg` — load `0x80131E20` — 284 KiB | Not started |
| `stage0_262` | Stage 0 overlay (file id 510900) — source `stage0/file510900/4.pe2pkg` — load `0x80131E20` — 216 KiB | Not started |
| `stage0_263` | Stage 0 overlay (file id 511000) — source `stage0/file511000/4.pe2pkg` — load `0x80131E20` — 141 KiB | Not started |
| `stage0_264` | Stage 0 overlay (file id 521100) — source `stage0/file521100/4.pe2pkg` — load `0x80131E20` — 226 KiB | Not started |
| `stage0_265` | Stage 0 overlay (file id 535700) — source `stage0/file535700/2.pe2pkg` — load `0x80131E20` — 82.5 KiB | Not started |
| `stage0_266` | Stage 0 overlay (file id 548100) — source `stage0/file548100/0.pe2pkg` — load `0x80131E20` — 25.1 KiB | Not started |
| `stage0_267` | Stage 0 overlay (file id 560800) — source `stage0/file560800/4.pe2pkg` — load `0x80131E20` — 270 KiB | Not started |
| `stage0_268` | Empty placeholder package — source `stage0/file640400/2.pe2pkg` — load `0x80131E20` — 4 B | Not started |
| `stage0_269` | Stage 0 overlay (file id 800100) — source `stage0/file800100/0.pe2pkg` — load `0x80161E20` — 23.1 KiB | Not started |
| `stage0_270` | Stage 0 overlay (file id 800101) — source `stage0/file800101/5.pe2pkg` — load `0x80167A70` — 55.4 KiB | Not started |
| `stage0_271` | Stage 0 overlay (file id 800102) — source `stage0/file800102/5.pe2pkg` — load `0x80167A70` — 62.8 KiB | Not started |
| `stage0_272` | Stage 0 overlay (file id 800103) — source `stage0/file800103/5.pe2pkg` — load `0x80167A70` — 55.3 KiB | Not started |
| `stage0_273` | Stage 0 overlay (file id 800104) — source `stage0/file800104/5.pe2pkg` — load `0x80167A70` — 60.7 KiB | Not started |
| `stage0_274` | Stage 0 overlay (file id 800200) — source `stage0/file800200/3.pe2pkg` — load `0x80161E20` — 53.3 KiB | Not started |
| `stage0_275` | Stage 0 overlay (file id 800300) — source `stage0/file800300/4.pe2pkg` — load `0x80161E20` — 43.7 KiB | Not started |
| `stage0_276` | Map / key-item location data — source `stage0/file900000/6.pe2pkg` — load `0x80179950` — 10.5 KiB | Not started |
| `stage0_277` | Dryfield area map labels — source `stage0/file900002/6.pe2pkg` — load `0x80179950` — 9.7 KiB | Not started |
| `stage0_278` | Dryfield area map labels — source `stage0/file900003/7.pe2pkg` — load `0x80179950` — 15.1 KiB | Not started |
| `stage0_279` | Akropolis / tunnel area map labels — source `stage0/file900004/2.pe2pkg` — load `0x80179950` — 10.4 KiB | Not started |
| `stage0_280` | Shelter / garage area map labels — source `stage0/file900005/2.pe2pkg` — load `0x80179950` — 13.2 KiB | Not started |
| `stage1_001` | Stage 1 room overlay (room 1001) — source `stage1/1001/file0/4.pe2pkg` — load `0x8017D5C0` — 23.8 KiB | Not started |
| `stage1_002` | Status / equipment menu UI — source `stage1/101/file0/3.pe2pkg` — load `0x8017D5C0` — 44.8 KiB | Not started |
| `stage1_003` | Stage 1 room overlay (room 1101) — source `stage1/1101/file0/6.pe2pkg` — load `0x8017D5C0` — 35.9 KiB | Not started |
| `stage1_004` | Stage 1 room overlay (room 1201) — source `stage1/1201/file0/7.pe2pkg` — load `0x8017D5C0` — 38.2 KiB | Not started |
| `stage1_005` | Stage 1 room overlay (room 1301) — source `stage1/1301/file0/7.pe2pkg` — load `0x8017D5C0` — 39.6 KiB | Not started |
| `stage1_006` | Scene graph / root-count logic — source `stage1/1401/file0/6.pe2pkg` — load `0x8017D5C0` — 80.5 KiB | Not started |
| `stage1_007` | Status / equipment menu UI — source `stage1/1501/file0/3.pe2pkg` — load `0x8017D5C0` — 22.7 KiB | Not started |
| `stage1_008` | Stage 1 room overlay (room 1601) — source `stage1/1601/file0/6.pe2pkg` — load `0x8017D5C0` — 42.5 KiB | Not started |
| `stage1_009` | Stage 1 room overlay (room 1701) — source `stage1/1701/file0/7.pe2pkg` — load `0x8017D5C0` — 37.3 KiB | Not started |
| `stage1_010` | Stage 1 room overlay (room 1801) — source `stage1/1801/file0/6.pe2pkg` — load `0x8017D5C0` — 38.2 KiB | Not started |
| `stage1_011` | Status / equipment menu UI — source `stage1/1901/file0/7.pe2pkg` — load `0x8017D5C0` — 95.4 KiB | Not started |
| `stage1_012` | Bonus / item reward UI — source `stage1/2001/file0/5.pe2pkg` — load `0x8017D5C0` — 66.8 KiB | Not started |
| `stage1_013` | Stage 1 room overlay (room 201) — source `stage1/201/file0/3.pe2pkg` — load `0x8017D5C0` — 41.5 KiB | Not started |
| `stage1_014` | Target-set logic — source `stage1/2101/file0/1.pe2pkg` — load `0x8017D5C0` — 484 B | Not started |
| `stage1_015` | Stage 1 room overlay (room 301) — source `stage1/301/file0/5.pe2pkg` — load `0x8017D5C0` — 38.7 KiB | Not started |
| `stage1_016` | Stage 1 room overlay (room 401) — source `stage1/401/file0/8.pe2pkg` — load `0x8017D5C0` — 64.2 KiB | Not started |
| `stage1_017` | Stage 1 room overlay (room 501) — source `stage1/501/file0/5.pe2pkg` — load `0x8017D5C0` — 114 KiB | Not started |
| `stage1_018` | Interactive object scripts (monitor / power supply) — source `stage1/601/file0/5.pe2pkg` — load `0x8017D5C0` — 32.0 KiB | Not started |
| `stage1_019` | Stage 1 room overlay (room 701) — source `stage1/701/file0/5.pe2pkg` — load `0x8017D5C0` — 9.1 KiB | Not started |
| `stage1_020` | Stage 1 room overlay (room 801) — source `stage1/801/file0/5.pe2pkg` — load `0x8017D5C0` — 25.5 KiB | Not started |
| `stage1_021` | Stage 1 room overlay (room 901) — source `stage1/901/file0/5.pe2pkg` — load `0x8017D5C0` — 30.8 KiB | Not started |
| `stage2_001` | Status / equipment menu UI — source `stage2/101/file0/3.pe2pkg` — load `0x8017D5C0` — 29.5 KiB | Not started |
| `stage2_002` | Stage 2 room overlay (room 1101) — source `stage2/1101/file0/3.pe2pkg` — load `0x8017D5C0` — 16.0 KiB | Not started |
| `stage2_003` | Stage 2 room overlay (room 1201) — source `stage2/1201/file0/3.pe2pkg` — load `0x8017D5C0` — 11.8 KiB | Not started |
| `stage2_004` | Stage 2 room overlay (room 1301) — source `stage2/1301/file0/3.pe2pkg` — load `0x8017D5C0` — 3.9 KiB | Not started |
| `stage2_005` | Stage 2 room overlay (room 1401) — source `stage2/1401/file0/3.pe2pkg` — load `0x8017D5C0` — 3.7 KiB | Not started |
| `stage2_006` | Stage 2 room overlay (room 1501) — source `stage2/1501/file0/3.pe2pkg` — load `0x8017D5C0` — 9.4 KiB | Not started |
| `stage2_007` | Stage 2 room overlay (room 1601) — source `stage2/1601/file0/5.pe2pkg` — load `0x8017D5C0` — 51.2 KiB | Not started |
| `stage2_008` | Status / equipment menu UI — source `stage2/1701/file0/3.pe2pkg` — load `0x8017D5C0` — 14.7 KiB | Not started |
| `stage2_009` | Stage 2 room overlay (room 1801) — source `stage2/1801/file0/5.pe2pkg` — load `0x8017D5C0` — 17.6 KiB | Not started |
| `stage2_010` | Stage 2 room overlay (room 1901) — source `stage2/1901/file0/3.pe2pkg` — load `0x8017D5C0` — 7.9 KiB | Not started |
| `stage2_011` | Stage 2 room overlay (room 2001) — source `stage2/2001/file0/5.pe2pkg` — load `0x8017D5C0` — 40.3 KiB | Not started |
| `stage2_012` | Stage 2 room overlay (room 201) — source `stage2/201/file0/5.pe2pkg` — load `0x8017D5C0` — 32.1 KiB | Not started |
| `stage2_013` | Stage 2 room overlay (room 2101) — source `stage2/2101/file0/6.pe2pkg` — load `0x8017D5C0` — 45.9 KiB | Not started |
| `stage2_014` | Stage 2 room overlay (room 2201) — source `stage2/2201/file0/5.pe2pkg` — load `0x8017D5C0` — 27.5 KiB | Not started |
| `stage2_015` | Stage 2 room overlay (room 2301) — source `stage2/2301/file0/5.pe2pkg` — load `0x8017D5C0` — 51.5 KiB | Not started |
| `stage2_016` | Stage 2 room overlay (room 2401) — source `stage2/2401/file0/3.pe2pkg` — load `0x8017D5C0` — 11.1 KiB | Not started |
| `stage2_017` | Stage 2 room overlay (room 2501) — source `stage2/2501/file0/3.pe2pkg` — load `0x8017D5C0` — 12.2 KiB | Not started |
| `stage2_018` | Stage 2 room overlay (room 2601) — source `stage2/2601/file0/5.pe2pkg` — load `0x8017D5C0` — 17.6 KiB | Not started |
| `stage2_019` | Status / equipment menu UI — source `stage2/2701/file0/5.pe2pkg` — load `0x8017D5C0` — 49.7 KiB | Not started |
| `stage2_020` | Stage 2 room overlay (room 2801) — source `stage2/2801/file0/3.pe2pkg` — load `0x8017D5C0` — 4.0 KiB | Not started |
| `stage2_021` | Stage 2 room overlay (room 2901) — source `stage2/2901/file0/3.pe2pkg` — load `0x8017D5C0` — 36.4 KiB | Not started |
| `stage2_022` | Status / equipment menu UI — source `stage2/3001/file0/3.pe2pkg` — load `0x8017D5C0` — 36.6 KiB | Not started |
| `stage2_023` | Stage 2 room overlay (room 301) — source `stage2/301/file0/3.pe2pkg` — load `0x8017D5C0` — 32.3 KiB | Not started |
| `stage2_024` | Stage 2 room overlay (room 3101) — source `stage2/3101/file0/3.pe2pkg` — load `0x8017D5C0` — 4.2 KiB | Not started |
| `stage2_025` | Stage 2 room overlay (room 3201) — source `stage2/3201/file0/5.pe2pkg` — load `0x8017D5C0` — 20.8 KiB | Not started |
| `stage2_026` | Stage 2 room overlay (room 3401) — source `stage2/3401/file0/5.pe2pkg` — load `0x8017D5C0` — 13.4 KiB | Not started |
| `stage2_027` | Stage 2 room overlay (room 3801) — source `stage2/3801/file0/5.pe2pkg` — load `0x8017D5C0` — 14.9 KiB | Not started |
| `stage2_028` | Stage 2 room overlay (room 401) — source `stage2/401/file0/1.pe2pkg` — load `0x8017D5C0` — 3.3 KiB | Not started |
| `stage2_029` | Stage 2 room overlay (room 501) — source `stage2/501/file0/3.pe2pkg` — load `0x8017D5C0` — 14.6 KiB | Not started |
| `stage2_030` | Stage 2 room overlay (room 601) — source `stage2/601/file0/3.pe2pkg` — load `0x8017D5C0` — 8.2 KiB | Not started |
| `stage2_031` | Stage 2 room overlay (room 701) — source `stage2/701/file0/3.pe2pkg` — load `0x8017D5C0` — 19.0 KiB | Not started |
| `stage2_032` | Stage 2 room overlay (room 801) — source `stage2/801/file0/5.pe2pkg` — load `0x8017D5C0` — 13.6 KiB | Not started |
| `stage2_033` | Stage 2 room overlay (room 901) — source `stage2/901/file0/3.pe2pkg` — load `0x8017D5C0` — 51.3 KiB | Not started |
| `stage3_001` | Status / equipment menu UI — source `stage3/101/file0/5.pe2pkg` — load `0x8017D5C0` — 76.5 KiB | Not started |
| `stage3_002` | Stage 3 room overlay (room 1101) — source `stage3/1101/file0/5.pe2pkg` — load `0x8017D5C0` — 12.7 KiB | Not started |
| `stage3_003` | Stage 3 room overlay (room 1201) — source `stage3/1201/file0/5.pe2pkg` — load `0x8017D5C0` — 13.2 KiB | Not started |
| `stage3_004` | Stage 3 room overlay (room 1301) — source `stage3/1301/file0/5.pe2pkg` — load `0x8017D5C0` — 14.0 KiB | Not started |
| `stage3_005` | Stage 3 room overlay (room 1401) — source `stage3/1401/file0/5.pe2pkg` — load `0x8017D5C0` — 11.5 KiB | Not started |
| `stage3_006` | Stage 3 room overlay (room 1501) — source `stage3/1501/file0/5.pe2pkg` — load `0x8017D5C0` — 15.9 KiB | Not started |
| `stage3_007` | Stage 3 room overlay (room 1601) — source `stage3/1601/file0/5.pe2pkg` — load `0x8017D5C0` — 7.6 KiB | Not started |
| `stage3_008` | Status / equipment menu UI — source `stage3/1701/file0/5.pe2pkg` — load `0x8017D5C0` — 27.8 KiB | Not started |
| `stage3_009` | Boss ranking / title UI — source `stage3/1801/file0/6.pe2pkg` — load `0x8017D5C0` — 46.5 KiB | Not started |
| `stage3_010` | Stage 3 room overlay (room 1901) — source `stage3/1901/file0/3.pe2pkg` — load `0x8017D5C0` — 5.6 KiB | Not started |
| `stage3_011` | Stage 3 room overlay (room 2001) — source `stage3/2001/file0/5.pe2pkg` — load `0x8017D5C0` — 21.7 KiB | Not started |
| `stage3_012` | Stage 3 room overlay (room 201) — source `stage3/201/file0/5.pe2pkg` — load `0x8017D5C0` — 45.5 KiB | Not started |
| `stage3_013` | Stage 3 room overlay (room 2101) — source `stage3/2101/file0/5.pe2pkg` — load `0x8017D5C0` — 12.7 KiB | Not started |
| `stage3_014` | Stage 3 room overlay (room 2201) — source `stage3/2201/file0/5.pe2pkg` — load `0x8017D5C0` — 11.8 KiB | Not started |
| `stage3_015` | Stage 3 room overlay (room 2301) — source `stage3/2301/file0/5.pe2pkg` — load `0x8017D5C0` — 52.6 KiB | Not started |
| `stage3_016` | Targeting / combat selection UI — source `stage3/2401/file0/5.pe2pkg` — load `0x8017D5C0` — 40.1 KiB | Not started |
| `stage3_017` | Stage 3 room overlay (room 2501) — source `stage3/2501/file0/3.pe2pkg` — load `0x8017D5C0` — 18.9 KiB | Not started |
| `stage3_018` | Stage 3 room overlay (room 2601) — source `stage3/2601/file0/5.pe2pkg` — load `0x8017D5C0` — 27.8 KiB | Not started |
| `stage3_019` | Status / equipment menu UI — source `stage3/2701/file0/5.pe2pkg` — load `0x8017D5C0` — 59.1 KiB | Not started |
| `stage3_020` | Stage 3 room overlay (room 2801) — source `stage3/2801/file0/5.pe2pkg` — load `0x8017D5C0` — 15.1 KiB | Not started |
| `stage3_021` | Stage 3 room overlay (room 2901) — source `stage3/2901/file0/6.pe2pkg` — load `0x8017D5C0` — 71.3 KiB | Not started |
| `stage3_022` | Status / equipment menu UI — source `stage3/3001/file0/5.pe2pkg` — load `0x8017D5C0` — 35.3 KiB | Not started |
| `stage3_023` | Stage 3 room overlay (room 301) — source `stage3/301/file0/3.pe2pkg` — load `0x8017D5C0` — 32.8 KiB | Not started |
| `stage3_024` | Stage 3 room overlay (room 3101) — source `stage3/3101/file0/6.pe2pkg` — load `0x8017D5C0` — 12.9 KiB | Not started |
| `stage3_025` | Stage 3 room overlay (room 3201) — source `stage3/3201/file0/5.pe2pkg` — load `0x8017D5C0` — 24.1 KiB | Not started |
| `stage3_026` | Stage 3 room overlay (room 3401) — source `stage3/3401/file0/5.pe2pkg` — load `0x8017D5C0` — 12.6 KiB | Not started |
| `stage3_027` | Stage 3 room overlay (room 3801) — source `stage3/3801/file0/5.pe2pkg` — load `0x8017D5C0` — 11.5 KiB | Not started |
| `stage3_028` | Stage 3 room overlay (room 501) — source `stage3/501/file0/5.pe2pkg` — load `0x8017D5C0` — 16.1 KiB | Not started |
| `stage3_029` | Stage 3 room overlay (room 601) — source `stage3/601/file0/3.pe2pkg` — load `0x8017D5C0` — 8.3 KiB | Not started |
| `stage3_030` | Stage 3 room overlay (room 701) — source `stage3/701/file0/3.pe2pkg` — load `0x8017D5C0` — 9.6 KiB | Not started |
| `stage3_031` | Stage 3 room overlay (room 801) — source `stage3/801/file0/3.pe2pkg` — load `0x8017D5C0` — 16.9 KiB | Not started |
| `stage3_032` | Stage 3 room overlay (room 901) — source `stage3/901/file0/3.pe2pkg` — load `0x8017D5C0` — 50.8 KiB | Not started |
| `stage4_001` | Stage 4 room overlay (room 1001) — source `stage4/1001/file0/3.pe2pkg` — load `0x8017D5C0` — 24.1 KiB | Not started |
| `stage4_002` | GPU tpage/clut debug overlay — source `stage4/101/file0/5.pe2pkg` — load `0x8017D5C0` — 49.4 KiB | Not started |
| `stage4_003` | Stage 4 room overlay (room 1101) — source `stage4/1101/file0/3.pe2pkg` — load `0x8017D5C0` — 38.1 KiB | Not started |
| `stage4_004` | Stage 4 room overlay (room 1201) — source `stage4/1201/file0/3.pe2pkg` — load `0x8017D5C0` — 33.4 KiB | Not started |
| `stage4_005` | Targeting / combat selection UI — source `stage4/1301/file0/3.pe2pkg` — load `0x8017D5C0` — 32.0 KiB | Not started |
| `stage4_006` | Stage 4 room overlay (room 1401) — source `stage4/1401/file0/6.pe2pkg` — load `0x8017D5C0` — 26.8 KiB | Not started |
| `stage4_007` | Stage 4 room overlay (room 1501) — source `stage4/1501/file0/5.pe2pkg` — load `0x8017D5C0` — 33.9 KiB | Not started |
| `stage4_008` | Status / equipment menu UI — source `stage4/1601/file0/8.pe2pkg` — load `0x8017D5C0` — 59.4 KiB | Not started |
| `stage4_009` | Stage 4 room overlay (room 1701) — source `stage4/1701/file0/3.pe2pkg` — load `0x8017D5C0` — 29.8 KiB | Not started |
| `stage4_010` | Stage 4 room overlay (room 1801) — source `stage4/1801/file0/5.pe2pkg` — load `0x8017D5C0` — 25.5 KiB | Not started |
| `stage4_011` | Stage 4 room overlay (room 1901) — source `stage4/1901/file0/3.pe2pkg` — load `0x8017D5C0` — 10.5 KiB | Not started |
| `stage4_012` | Status / equipment menu UI — source `stage4/2001/file0/6.pe2pkg` — load `0x8017D5C0` — 64.5 KiB | Not started |
| `stage4_013` | Stage 4 room overlay (room 201) — source `stage4/201/file0/7.pe2pkg` — load `0x8017D5C0` — 69.4 KiB | Not started |
| `stage4_014` | Stage 4 room overlay (room 2101) — source `stage4/2101/file0/5.pe2pkg` — load `0x8017D5C0` — 7.2 KiB | Not started |
| `stage4_015` | Stage 4 room overlay (room 2201) — source `stage4/2201/file0/9.pe2pkg` — load `0x8017D5C0` — 44.6 KiB | Not started |
| `stage4_016` | Stage 4 room overlay (room 2301) — source `stage4/2301/file0/5.pe2pkg` — load `0x8017D5C0` — 19.9 KiB | Not started |
| `stage4_017` | Stage 4 room overlay (room 2401) — source `stage4/2401/file0/3.pe2pkg` — load `0x8017D5C0` — 23.0 KiB | Not started |
| `stage4_018` | Stage 4 room overlay (room 2501) — source `stage4/2501/file0/3.pe2pkg` — load `0x8017D5C0` — 20.2 KiB | Not started |
| `stage4_019` | Stage 4 room overlay (room 2601) — source `stage4/2601/file0/5.pe2pkg` — load `0x8017D5C0` — 5.1 KiB | Not started |
| `stage4_020` | Stage 4 room overlay (room 2701) — source `stage4/2701/file0/3.pe2pkg` — load `0x8017D5C0` — 30.0 KiB | Not started |
| `stage4_021` | Stage 4 room overlay (room 2801) — source `stage4/2801/file0/3.pe2pkg` — load `0x8017D5C0` — 24.8 KiB | Not started |
| `stage4_022` | Stage 4 room overlay (room 2901) — source `stage4/2901/file0/3.pe2pkg` — load `0x8017D5C0` — 27.2 KiB | Not started |
| `stage4_023` | Stage 4 room overlay (room 3001) — source `stage4/3001/file0/3.pe2pkg` — load `0x8017D5C0` — 35.5 KiB | Not started |
| `stage4_024` | Stage 4 room overlay (room 301) — source `stage4/301/file0/5.pe2pkg` — load `0x8017D5C0` — 7.6 KiB | Not started |
| `stage4_025` | Status / equipment menu UI — source `stage4/3101/file0/6.pe2pkg` — load `0x8017D5C0` — 35.9 KiB | Not started |
| `stage4_026` | Stage 4 room overlay (room 3201) — source `stage4/3201/file0/6.pe2pkg` — load `0x8017D5C0` — 28.6 KiB | Not started |
| `stage4_027` | Stage 4 room overlay (room 3301) — source `stage4/3301/file0/5.pe2pkg` — load `0x8017D5C0` — 48.2 KiB | Not started |
| `stage4_028` | Stage 4 room overlay (room 3401) — source `stage4/3401/file0/5.pe2pkg` — load `0x8017D5C0` — 38.6 KiB | Not started |
| `stage4_029` | Stage 4 room overlay (room 3501) — source `stage4/3501/file0/5.pe2pkg` — load `0x8017D5C0` — 32.3 KiB | Not started |
| `stage4_030` | Stage 4 room overlay (room 3601) — source `stage4/3601/file0/8.pe2pkg` — load `0x8017D5C0` — 9.3 KiB | Not started |
| `stage4_031` | Stage 4 room overlay (room 3701) — source `stage4/3701/file0/1.pe2pkg` — load `0x8017D5C0` — 2.3 KiB | Not started |
| `stage4_032` | Stage 4 room overlay (room 3801) — source `stage4/3801/file0/1.pe2pkg` — load `0x8017D5C0` — 2.7 KiB | Not started |
| `stage4_033` | Stage 4 room overlay (room 3901) — source `stage4/3901/file0/8.pe2pkg` — load `0x8017D5C0` — 71.8 KiB | Not started |
| `stage4_034` | Stage 4 room overlay (room 4001) — source `stage4/4001/file0/7.pe2pkg` — load `0x8017D5C0` — 92.7 KiB | Not started |
| `stage4_035` | Stage 4 room overlay (room 401) — source `stage4/401/file0/5.pe2pkg` — load `0x8017D5C0` — 11.4 KiB | Not started |
| `stage4_036` | Status / equipment menu UI — source `stage4/4101/file0/3.pe2pkg` — load `0x8017D5C0` — 21.2 KiB | Not started |
| `stage4_037` | Stage 4 room overlay (room 4201) — source `stage4/4201/file0/3.pe2pkg` — load `0x8017D5C0` — 29.1 KiB | Not started |
| `stage4_038` | Stage 4 room overlay (room 4301) — source `stage4/4301/file0/5.pe2pkg` — load `0x8017D5C0` — 26.1 KiB | Not started |
| `stage4_039` | Stage 4 room overlay (room 4401) — source `stage4/4401/file0/5.pe2pkg` — load `0x8017D5C0` — 45.9 KiB | Not started |
| `stage4_040` | Stage 4 room overlay (room 4501) — source `stage4/4501/file0/6.pe2pkg` — load `0x8017D5C0` — 40.2 KiB | Not started |
| `stage4_041` | Stage 4 room overlay (room 4601) — source `stage4/4601/file0/5.pe2pkg` — load `0x8017D5C0` — 30.1 KiB | Not started |
| `stage4_042` | Status / equipment menu UI — source `stage4/4701/file0/5.pe2pkg` — load `0x8017D5C0` — 52.2 KiB | Not started |
| `stage4_043` | Stage 4 room overlay (room 4801) — source `stage4/4801/file0/5.pe2pkg` — load `0x8017D5C0` — 58.2 KiB | Not started |
| `stage4_044` | Stage 4 room overlay (room 4901) — source `stage4/4901/file0/2.pe2pkg` — load `0x8017D5C0` — 2.1 KiB | Not started |
| `stage4_045` | Stage 4 room overlay (room 501) — source `stage4/501/file0/5.pe2pkg` — load `0x8017D5C0` — 24.2 KiB | Not started |
| `stage4_046` | Status / equipment menu UI — source `stage4/601/file0/6.pe2pkg` — load `0x8017D5C0` — 21.3 KiB | Not started |
| `stage4_047` | Stage 4 room overlay (room 701) — source `stage4/701/file0/7.pe2pkg` — load `0x8017D5C0` — 32.0 KiB | Not started |
| `stage4_048` | Stage 4 room overlay (room 801) — source `stage4/801/file0/5.pe2pkg` — load `0x8017D5C0` — 23.6 KiB | Not started |
| `stage4_049` | Stage 4 room overlay (room 901) — source `stage4/901/file0/5.pe2pkg` — load `0x8017D5C0` — 29.1 KiB | Not started |
| `stage5_001` | Stage 5 room overlay (room 1001) — source `stage5/1001/file0/3.pe2pkg` — load `0x8017D5C0` — 23.3 KiB | Not started |
| `stage5_002` | Stage 5 room overlay (room 101) — source `stage5/101/file0/3.pe2pkg` — load `0x8017D5C0` — 17.0 KiB | Not started |
| `stage5_003` | Flag / progress check scripts — source `stage5/1101/file0/5.pe2pkg` — load `0x8017D5C0` — 22.2 KiB | Not started |
| `stage5_004` | Stage 5 room overlay (room 1201) — source `stage5/1201/file0/5.pe2pkg` — load `0x8017D5C0` — 41.1 KiB | Not started |
| `stage5_005` | Stage 5 room overlay (room 1301) — source `stage5/1301/file0/5.pe2pkg` — load `0x8017D5C0` — 41.1 KiB | Not started |
| `stage5_006` | Stage 5 room overlay (room 1401) — source `stage5/1401/file0/5.pe2pkg` — load `0x8017D5C0` — 26.6 KiB | Not started |
| `stage5_007` | Stage 5 room overlay (room 1501) — source `stage5/1501/file0/5.pe2pkg` — load `0x8017D5C0` — 21.6 KiB | Not started |
| `stage5_008` | Stage 5 room overlay (room 1601) — source `stage5/1601/file0/3.pe2pkg` — load `0x8017D5C0` — 22.5 KiB | Not started |
| `stage5_009` | Stage 5 room overlay (room 1701) — source `stage5/1701/file0/3.pe2pkg` — load `0x8017D5C0` — 17.6 KiB | Not started |
| `stage5_010` | Stage 5 room overlay (room 1801) — source `stage5/1801/file0/3.pe2pkg` — load `0x8017D5C0` — 13.0 KiB | Not started |
| `stage5_011` | Stage 5 room overlay (room 1901) — source `stage5/1901/file0/3.pe2pkg` — load `0x8017D5C0` — 13.0 KiB | Not started |
| `stage5_012` | Stage 5 room overlay (room 2001) — source `stage5/2001/file0/3.pe2pkg` — load `0x8017D5C0` — 10.8 KiB | Not started |
| `stage5_013` | Stage 5 room overlay (room 201) — source `stage5/201/file0/5.pe2pkg` — load `0x8017D5C0` — 21.2 KiB | Not started |
| `stage5_014` | Stage 5 room overlay (room 2101) — source `stage5/2101/file0/5.pe2pkg` — load `0x8017D5C0` — 36.8 KiB | Not started |
| `stage5_015` | Status / equipment menu UI — source `stage5/2201/file0/6.pe2pkg` — load `0x8017D5C0` — 41.1 KiB | Not started |
| `stage5_016` | Stage 5 room overlay (room 2301) — source `stage5/2301/file0/5.pe2pkg` — load `0x8017D5C0` — 12.7 KiB | Not started |
| `stage5_017` | Stage 5 room overlay (room 2401) — source `stage5/2401/file0/3.pe2pkg` — load `0x8017D5C0` — 31.0 KiB | Not started |
| `stage5_018` | Stage 5 room overlay (room 2501) — source `stage5/2501/file0/5.pe2pkg` — load `0x8017D5C0` — 33.7 KiB | Not started |
| `stage5_019` | Stage 5 room overlay (room 2601) — source `stage5/2601/file0/4.pe2pkg` — load `0x8017D5C0` — 5.1 KiB | Not started |
| `stage5_020` | Stage 5 room overlay (room 2701) — source `stage5/2701/file0/5.pe2pkg` — load `0x8017D5C0` — 29.6 KiB | Not started |
| `stage5_021` | Status / equipment menu UI — source `stage5/2801/file0/4.pe2pkg` — load `0x8017D5C0` — 27.5 KiB | Not started |
| `stage5_022` | Flag / progress check scripts — source `stage5/2901/file0/5.pe2pkg` — load `0x8017D5C0` — 29.2 KiB | Not started |
| `stage5_023` | Stage 5 room overlay (room 3001) — source `stage5/3001/file0/5.pe2pkg` — load `0x8017D5C0` — 32.9 KiB | Not started |
| `stage5_024` | Stage 5 room overlay (room 301) — source `stage5/301/file0/3.pe2pkg` — load `0x8017D5C0` — 14.3 KiB | Not started |
| `stage5_025` | Stage 5 room overlay (room 3101) — source `stage5/3101/file0/1.pe2pkg` — load `0x8017D5C0` — 1.6 KiB | Not started |
| `stage5_026` | Stage 5 room overlay (room 3201) — source `stage5/3201/file0/3.pe2pkg` — load `0x8017D5C0` — 16.7 KiB | Not started |
| `stage5_027` | Stage 5 room overlay (room 3301) — source `stage5/3301/file0/3.pe2pkg` — load `0x8017D5C0` — 11.4 KiB | Not started |
| `stage5_028` | Targeting / combat selection UI — source `stage5/401/file0/5.pe2pkg` — load `0x8017D5C0` — 21.8 KiB | Not started |
| `stage5_029` | Stage 5 room overlay (room 501) — source `stage5/501/file0/3.pe2pkg` — load `0x8017D5C0` — 8.7 KiB | Not started |
| `stage5_030` | Stage 5 room overlay (room 601) — source `stage5/601/file0/3.pe2pkg` — load `0x8017D5C0` — 2.6 KiB | Not started |
| `stage5_031` | Stage 5 room overlay (room 701) — source `stage5/701/file0/6.pe2pkg` — load `0x8017D5C0` — 41.1 KiB | Not started |
| `stage5_032` | Stage 5 room overlay (room 801) — source `stage5/801/file0/3.pe2pkg` — load `0x8017D5C0` — 12.5 KiB | Not started |
| `stage5_033` | Stage 5 room overlay (room 901) — source `stage5/901/file0/3.pe2pkg` — load `0x8017D5C0` — 1.6 KiB | Not started |

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
| `-iso_e` / `--iso_extract` | Extract files from disc images |
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
