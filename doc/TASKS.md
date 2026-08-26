# Parasite Eve 2 — task system (actor model)

The game’s actors are cooperative **tasks**: a `Task` is a 0x48-byte object
with a per-frame callback, optional parent/child links, and an optional 3D or
2D body. There is no separate entity list. Enemies, UI, camera, memcard, title,
and room overlays are all spawned the same way.

Field-level layouts: [`STRUCT_FIELDS.md`](../STRUCT_FIELDS.md) (Task system).
Overlay RAM slots that many callbacks live in: [`OVERLAYS.md`](OVERLAYS.md).
Naming: [`NAMING.md`](../NAMING.md) (`Task_` / `TaskDesc`).

| Area | Code / data |
|------|-------------|
| Types + APIs | `include/main/task.h`, `src/main/task.c` |
| Extra lists / OT spawn | `src/main/otutil.c` (`Display_SpawnWithOt*`, `Task_SpawnOnDefaultList*`) |
| Frame tick | `src/main/gamemain.c` (`GameMain_Loop` → `Task_ExecDefaultList`) |
| Bank tables | `asm/USA/main/data/task.data.s` (`Task_DescBanks`), plus `52E8C` / `578D0` / `57EA8` / `57F34` / `58028` / `59184.data.s` |
| Gameplay banks 6, 10 | `asm/USA/gameplay/data/data.data.s` (`D_8010FC2C`, `0x80114B34`) |
| Title extras | `src/title/title.c`, `Title_TaskDescs` |
| Enemies | `src/gameplay/1BC.c` (`Gp_SpawnEnemy`, `Gp_SpawnEnemyFromTable`) |
| UI stack descs | `src/main/ui.c` (`Ui_SpawnFromDesc`) |

**Coverage.** The scheduler is fully described. Bank 0 (system) and bank 9 (FX)
can be catalogued. Banks 6–7 and most overlay-local tables are still an index
of function pointers, not a cast list.

---

## 1. Lifetime

### 1.1 `Task` (0x48)

Allocated with `Mem_Calloc(0x48, 0)`. Inserted into the **active list**
(`Task_ActiveList`, usually `Task_DefaultList`) in **priority order**: lower
`priority` runs earlier. Typical values:

| Priority | Who |
|----------|-----|
| `0x10` | Boot, memcard |
| `0x18` | Main gameflow |
| `0x20`–`0x2F` | View / HUD / some gameplay |
| `0x50`–`0x70` | Type-1 TMD attaches (bank 7, shared `func_800F6D18`) |
| `0xC0` | Default actor |
| `0xE0`–`0xF8` | Draw / load-wait (late in the list) |

Parent/child is a **sibling ring**: `firstChild` is the head, `nextSibling`
walks the ring and is self when the task is an only child. `Task_Kill` runs
every child’s `exitCallback` first (with `parent` cleared). `Task_Reparent`
moves a live task onto another parent’s ring.

`idMap` is an optional `TaskIdMap*` (`Mem_Calloc(8)` via `Task_AllocIdMap`).
Kill always `Mem_Free`s it. UI, scripts, and title reuse the slot as a work
pointer (`TitleWork`, `GpState34`, …) — the type is a lie at those call sites.

### 1.2 Spawn

```c
Task* Task_Spawn(s32 bank, s32 type, s32 spawnArg1, s32 spawnArg2);
Task* Task_SpawnFromTable(TaskDesc* table, s32 idx, s32 spawnArg1, s32 spawnArg2);
Task* Task_SpawnFromDesc(TaskDesc* desc, s32 spawnArg1, s32 spawnArg2, TaskNode* list);
```

`Task_Spawn` indexes `Task_DescBanks[bank][type]`. **Negative `bank`** means
`type` is already a `TaskDesc*`. Args land on `Task::spawnArg1` / `spawnArg2`.

`TaskDesc` (0xC):

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `flags` | Low byte = spawn type (0/1/2). Bit `0x100` is a type-1 setup flag |
| 0x2 | `priority` | Low byte copied to `Task::priority` |
| 0x4 | `callback` | Per-frame entry (`Task::callback`) |
| 0x8 | `setupArg` | Type-1 only: `TmdSource*` for `Gp_AttachTmdFlags` |

Spawn type (low byte of `flags`, stored as `Task::spawnType`) is the body:

| Type | Attach (`Task::extra`) | Kill teardown |
|------|------------------------|---------------|
| 0 | none | free the `Task` |
| 1 | `Gp_AttachTmdFlags(task, setupArg, flags)` — 3D TMD | unlink + free TMD (often deferred 2 frames) |
| 2 | `Gp_AttachDisp2d(task)` — 2D | unlink Disp2d (often deferred 1 frame) |

If attach fails, spawn returns NULL and frees the `Task`. `exitCallback`
defaults to `Task_Kill`.

### 1.3 Tick

`GameMain_Loop` rebuilds the OT, then:

```c
Task_ExecDefaultList(...);   // walks Task_DefaultList
```

Each node’s `callback` runs. Two early-outs:

- `Display_State.field_10b == 1` — abort the rest of the list this frame
  (gameflow uses this after killing the world and respawning).
- `spawnType == 0xFF` — **tombstone**: unlink + `Mem_Free` this node, continue.

`Task_ExecList` is the same walk on an arbitrary list.
`Task_ExecListFiltered(list, pri)` only runs nodes whose `priority` equals
`pri & 0xFF` (stage load uses `0x62`).

### 1.4 Kill

`Task_Kill` is also the default `exitCallback`. It:

1. Detaches children and calls each child’s `exitCallback`.
2. Unlinks from the parent ring.
3. Frees `idMap` if set.
4. Tears down `extra` according to `spawnType` (skipped when
   `Display_State.skipTeardown` is set).
5. Sets `spawnType = 0xFF` so the **next** exec pass frees the node.

Type 1 often swaps `callback` to `Task_CountdownCallback` with
`killCountdown = 2` instead of freeing immediately. Type 0/2 typically park
`callback` on the empty stub `func_8002DEC4` for one frame.

`Task_RequestKill` marks `flags = 0xFF` and stashes a result in `extraState`;
`Task_PollKill` reads that and then calls `exitCallback`.

### 1.5 Lists

| List | Role |
|------|------|
| `Task_DefaultList` | Main frame list. `Task_ResetDefaultList` on boot / world reset |
| `Task_ActiveList` | Where new spawns insert. Almost always the default list |
| `D_8007A110` | Side list with its own small OT. `Display_SpawnWithOt` / `Display_SpawnWithOtSmall` init it, spawn onto it, then restore `Task_ActiveList` |

`Task_SpawnOnDefaultList` / `Task_SpawnOnDefaultListA` temporarily switch
`Task_ActiveList` to the default list so a spawn from inside another list
still lands on the main frame walk.

---

## 2. `Task_DescBanks`

`Task_DescBanks` is 15 pointers (0x3C bytes at `0x8005EF74`). Banks **11–13
are aliases of bank 2**. Deduped size is **996** descriptors (~1047 if aliases
are counted). Roughly 155 of those are `Task_Kill` placeholders and 13 are
NULL.

| Bank | Symbol | n | What it is |
|------|--------|--:|------------|
| 0 | `D_8005EDA0` | 39 | System: boot, title, gameflow, memcard, view, HUD |
| 1 | `D_800670D0` | 51 | Enemies / room coords / TMD helpers + `0x807xxxxx` overlay |
| 2 (=11–13) | `D_80067828` | 17 | Caption, pad helpers, script-18, room overlay, one stage overlay |
| 3 | `D_80062780` | 8 | Four `Task_Kill` stubs + four `0x807xxxxx` |
| 4 | `D_800676A8` | 9 | Stubs + TMD / overlay |
| 5 | `D_800626AC` | 5 | Stubs + `Task_KillMaybeSpawn` + one overlay |
| 6 | `D_8010FC2C` | **667** | Room-overlay actor catalog (gameplay data → `0x8017xxxx`) |
| 7 | `D_800678F4` | 164 | Equipped TMD attaches (`func_8010B610` + per-item `setupArg`) |
| 8 | `D_800626EC` | 6 | Stubs + shared `func_800F6D18` |
| 9 | `D_80067734` | 19 | FX / wait: shake, volume fade, sound fade, end-wait |
| 10 | `0x80114B34` | 6 | Stubs + `func_800F6D18` (splat-merged into `Gp_CollectedIds`) |
| 14 | `D_80068B7C` | 5 | Stubs + one `0x807xxxxx` |

Callback addresses fall in four windows:

| Range | Resident |
|-------|----------|
| `0x8001xxxx`–`0x800937FF` | Main executable |
| `0x80093800`–`0x80115769` | Gameplay *or* title overlay |
| `0x80115770`+ | Aya / weapon / actor / **room** overlays ([`OVERLAYS.md`](OVERLAYS.md) §2) |
| `0x807xxxxx` | Imported overlay, not splat’d in this tree |

`func_800F6D18` is a generic type-1 TMD actor reused in banks 1, 2, 4, 6, 8, 10
(and the bank-2 aliases). It is still a `func_*`.

---

## 3. Bank 0 — system

This is the only bank we can describe entry-by-entry. Spawn with
`Task_Spawn(0, type, spawnArg1, spawnArg2)`.

| Type | Pri | Callback | Notes |
|------|-----|----------|-------|
| `00` | `C0` | `func_8002DEC4` | Empty stub; also the deferred-kill callback |
| `01` | `C0` | `Task_CountdownCallback` | Decrement `killCountdown`, then kill |
| `02` | `C0` | `Title_Dispatch` | Title phase machine. `Text_BootTask` / gameflow / title spawn this; `spawnArg1` `0x80000000` skips the fade TILE |
| `03` | `C0` | `GameFlow_StateByField34` | Title new-game / demo path. Also a `Title_MenuSpawnIds` entry |
| `04` | `C0` | `GameFlow_DispatchTable5` | Title load-style gameflow. Also a `Title_MenuSpawnIds` entry |
| `05` | `C0` | `Text_UiTaskCallback` | Text / UI. Also a `Title_MenuSpawnIds` entry |
| `06` | `C0` | `Title_ExitTask` | Title exit. Also a `Title_MenuSpawnIds` entry |
| `07` | `C0` | `Task_Kill` | Unused slot |
| `08` | `00` | NULL | Unused |
| `09` | `18` | `GameFlow_DispatchTable` | **Main in-game flow.** Spawned after session reset, from title, etc. |
| `0A` | `10` | `Mc_DispatchStateTable` | Memcard state machine |
| `0B` | `10` | `Mc_DispatchStateTable26` | Memcard variant |
| `0C` | `C0` | `func_80036A1C` | Memcard menu dispatcher (`mcmenu.c`) |
| `0D` | `10` | `Text_BootTask` | Boot: load CLUT, spawn `Title_TaskDescs[0]`, kill self. `Boot` also spawns this |
| `0E` | `2F` | `func_800A8654` | Type **2** (Disp2d). Gameplay dispatcher |
| `0F` | `2F` | `Gp_ApplyViewTask` | Camera / view. `Gp_TrySpawnViewTask` / `Gp_SpawnViewTasks` |
| `10` | `40` | `func_800AD50C` | Gameplay state dispatcher (`D4.c`) |
| `11` | `28` | `func_800AC0F0` | Pad-gated 3-way dispatcher. Gameflow / area code spawn this |
| `12` | `10` | `Mc_DispatchStateTable` | Same as `0A` |
| `13` | `10` | `Mc_DispatchStateTable26` | Same as `0B` |
| `14` | `1F` | `func_800AEE8C` | Area / dir helper (`1A8.c`, matched) |
| `15` | `C0` | `Task_Kill` | Unused |
| `16` | `30` | `func_800A8E8C` | Gameplay dispatcher |
| `17` | `2F` | `Gp_AllocSprtListsTask` | HUD sprite lists. Spawned next to the view task |
| `18` | `C0` | `Stage_TaskExit` | Stage teardown |
| `19` | `C0` | `0x807011D8` | Stage overlay — not in this tree |
| `1A` | `E0` | `Gp_DrawDisp2dOt` | 2D OT draw. Live pointer `Gp_TmdStashTask` |
| `1B` | `D0` | `func_800AD5B8` | Sibling of type `10` |
| `1C` | `2F` | `func_800AC25C` | 8-way dispatcher (pause / menu-ish) |
| `1D` | `18` | `func_800A77B4` | 6-way dispatcher |
| `1E` | `F8` | `Gp_LoadWaitDispatch` | Load-wait. `D4.c` / stage fade spawn this |
| `1F` | `10` | `Boot_LoadInitialFile` | Cold boot (`D_8005EC64 == 1`) |
| `20` | `10` | `Boot_LoadTask` | Cold boot (otherwise). `GameMain_SpawnBootTask` |
| `21` | `2F` | `func_800AC344` | White fade-out, then continue (`D4.c`) |
| `22` | `F8` | NULL | Unused |
| `23` | `C0` | `0x80701400` | Stage overlay — not in this tree |
| `24` | `2F` | NULL | Unused |
| `25` | `F8` | `Gp_CommitSpawnLoc` | Commit spawn location |
| `26` | `F8` | `Gp_SetupSprtDisplay` | Sprite display setup |

`Title_MenuSpawnIds` (6 words) is `{6, 6, 3, 4, 5, 6}` — bank 0 types spawned
from `Title_MenuTask` on confirm.

Several `func_*` rows are already matched C and only lack a role name.

---

## 4. Bank 9 — FX / wait

| Type | Pri | Callback | Notes |
|------|-----|----------|-------|
| `00`–`05`, `09`–`0A`, `0F`–`10` | `C0`/`20` | `Task_Kill` or NULL | Unused |
| `06` | `80` | `func_800E7570` | Unnamed |
| `07` | `20` | `func_800E8830` | Spawned from fade setup (`Task_Spawn(9, 7, …)`) |
| `08` | `80` | `func_800E8888` | Live pointer `D_801156B8` |
| `0B` | `80` | `Gp_EndWaitTask` | `spawnArg2` is `GpEndWait*`; non-zero `field_2` sets the ending flag and kills |
| `0C` | `20` | `Gp_ShakeTask` | Camera shake |
| `0D` | `20` | `Gp_VolFadeTask` | `spawnArg2` is `GpVolFade*` (target volume + duration) |
| `0E` | `20` | `Gp_SndFadeTask` | `spawnArg2` is `GpSndFade*` (sound-param fade) |
| `11` | `20` | `func_800E4028` | Unnamed |
| `12` | `20` | NULL, `flags = 0xFFFF` | Sentinel |

Payload structs: `include/gameplay/3CD8.h`.

---

## 5. Other banks (what is proven)

### Bank 2 — caption, pad, script (aliases 11–13)

| Type | Callback | Notes |
|------|----------|-------|
| `07` | `func_800E70AC` | **Caption / dialogue.** `Gp_CapTask = Task_Spawn(2, 7, …)` or `Display_InitModeObj(Task_GetDesc(2, 7), …)` |
| `0B` | `Gp_PadHoldTask` | `Gp_SpawnPadHold` — hold countdown in `spawnArg1` |
| `0C` | `Gp_PadLerpTask` | `Gp_SpawnPadLerp` — work block in `idMap` |
| `0D` | `Gp_Script18Task` | Script-18 dispatcher |
| `06`, `10` | `func_800F6D18` | Shared type-1 TMD |
| `04` | `0x807257A0` | Stage overlay |
| `08`–`0A`, `0E`–`0F` | `0x8017xxxx` / `0x8018xxxx` | Room overlay |

### Bank 1 — enemies + overlay

Named / matched: `Gp_EnemyDispatch` (`0xB`), `Gp_UpdateRoomCoords` (`0xF`),
`Tmd_DispatchTask` (`0x21`), `Tmd_AllocNodeBuffers` (`0x22`),
`Gp_FadeTileTask` (`0x27`). The rest is `Task_Kill`, `func_*`, or
`0x807xxxxx` (many type-1 with `setupArg = 0x8075BED4`).

`Gp_SpawnEnemy(bank, type, arg, parent)` is `Task_Spawn` plus a `GpEnemy*`
hung off `spawnArg2` (`Gp_AllocEnemy`). Exit path is `Gp_EnemyTaskExit`.

### Bank 6 — 667 room actors

Gameplay-resident table at `D_8010FC2C`. Almost every non-stub callback is in
**room overlay RAM** (`0x8017D5C0` and following — see [`OVERLAYS.md`](OVERLAYS.md)
§2). One named entry: `Gp_FadeWaveTask`. A handful of `func_800Exxxx` /
`func_800Fxxxx` sit in the gameplay overlay; they are not named.

This is the large “what’s in the room” catalog. Describing a row means
decompiling the overlay it points at.

### Bank 7 — equipped TMD attaches

~80 `Task_Kill` stubs; the rest are type 1, priority `0x50`/`0x52`, callback
usually `func_8010B610` (gameplay-resident), `setupArg` a `TmdSource*` in
weapon / actor overlay RAM (`0x8011xxxx`, `0x8016xxxx`, `0x8018xxxx`).

`src/gameplay/3FB8.c` spawns these as `Task_Spawn(7, type, …)` when attaching
gear to an actor, then rewrites `parent` and TMD coord links.

### Banks 3, 4, 5, 8, 10, 14

Placeholder slots plus a few overlay or shared-TMD callbacks. Bank 5 type `1`
is `Task_KillMaybeSpawn` (`taskutil.c`): on `Display_State.field_112` it
spawns `D_80725C54[0]` (overlay desc) then kills itself.

---

## 6. Tables outside the banks

These are real actors too; they just skip `Task_DescBanks`.

| Table | Callback / role |
|-------|-----------------|
| `Title_TaskDescs[0]` | `Title_BootTask` |
| `Title_TaskDescs[1]` | `Title_DemoStreamTask` (`Display_SpawnWithOt`) |
| `D_8006269C[0]` | `Display_DispatchTaskTable` — 6-way stage load (`Stage_WaitCdActivate` … `Stage_FinishCdFollowUp`) |
| `D_80062774[0]` | `Stage_DispatchTaskTable` — bank-load spawn from gameplay |
| `D_8006268C[0]` | `0x800BF9FC` (gameplay) |
| `Stage_Ctx->field_0` | Per-stage desc table; `Display_SpawnFromMode` spawns index 0 |
| `D_80725C54` | Overlay desc, from `Task_KillMaybeSpawn` |
| `D_8010D1FC`, `D_8010FB4C`, `D_80115D9C`, `D_80119218`, `D_8011922C`, `D_80113340`, `D_80183824`, … | Gameplay / save-slot / enemy tables (`1BC.c` `func_800B25B0` switches on `Mc_SaveData`) |
| Stack `TaskDesc` | `Ui_SpawnFromDesc` seeds flags/priority/callback from a `UiObjectDesc` |

`Task_GetDesc(bank, type)` is the typed way to hand a bank entry to
`Display_InitModeObj` without spawning onto the current list.

---

## 7. Work that hangs off a task

The `Task` is the actor. Specific systems stash extra state in the leftover
slots (see [`STRUCT_FIELDS.md`](../STRUCT_FIELDS.md)):

| Slot | Typical payload |
|------|-----------------|
| `extra` | `GameActorExt*` / TMD object (type 1) or Disp2d (type 2) |
| `spawnArg2` | `GpEnemy*`, `UiObject*`, `GpVolFade*`, `GpSndFade*`, `GpEndWait*`, view record, … |
| `idMap` | Real `TaskIdMap*`, or abused as `TitleWork*` / script work / pad-lerp state |
| `field_24` | `GpMsgEntry*` table (`Gp_DispatchMsg`) |
| `state` | Dispatcher index (`TaskFuncTable3`–`8` copied onto the stack) |

`Game_SetPtrSlot` / `Game_GetPtrSlot` (`GameSession::field_C`) is a parallel
pointer table some tasks publish into; it is not the task list.

---

## 8. What is not described yet

- **Bank 6** (667 room-overlay callbacks) and most of **bank 7** (per-item TMD
  sources). The tables are complete; the functions are not.
- **`0x807xxxxx`** overlays referenced from banks 0/1/3/4/5/14. No splat tree.
- **Enemy / room `TaskDesc` tables** (`Gp_SpawnEnemyFromTable`, `func_800B25B0`
  save-slot switch). Overlay-local, mostly unnamed.
- **UI** tasks built from `UiObjectDesc` rather than a bank index.
- Several bank-0 `func_*` that are matched C but not renamed
  (`func_800AC0F0`, `func_800AD50C`, `func_800A77B4`, …).

Adding a bank-6/7 row to this file without a proven role is just publishing an
address. Prefer renaming the callback (or its overlay) first.
