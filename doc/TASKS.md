# Parasite Eve 2 — task system (actor model)

The game’s actors are cooperative **tasks**: a `Task` is a 0x48-byte object
with a per-frame callback, optional parent/child links, and an optional TMD model
or single-coordinate body. There is no separate entity list. Enemies, UI, camera,
memcard, title, and room overlays are all spawned the same way.

Field-level layouts: [`include/main/task_types.h`](../include/main/task_types.h).
Overlay RAM slots that many callbacks live in: [`OVERLAYS.md`](OVERLAYS.md).
Naming: [`NAMING.md`](../NAMING.md) (`task` functions / `TaskDesc`).

| Area | Code / data |
|------|-------------|
| Types + APIs | `include/main/task.h`, `src/main/task.c` |
| Extra lists / OT spawn | `src/main/otutil.c` (`displaySpawnTaskFromTable`, `displaySpawnTask`, `taskSpawnFromTableOnDefaultList`, `taskSpawnOnDefaultList`) |
| Frame tick | `src/main/gamemain.c` (`GameMain_Loop` → `taskExecDefaultList`) |
| Bank tables | `asm/USA/main/data/task.data.s` (`gTaskDescBanks`), plus `52E8C` / `578D0` / `57EA8` / `57F34` / `58028` / `59184.data.s` |
| Gameplay banks 6, 10 | `asm/USA/gameplay/data/data.data.s` (`D_8010FC2C`, `0x80114B34`) |
| Title extras | `src/title/title.c`, `Title_TaskDescs` |
| Enemies | `src/gameplay/scene_runtime.c` (`_enemySpawn`, `enemySpawnFromTable`) |
| UI stack descs | `src/main/ui.c` (`uiSpawnObject`) |

**Coverage.** The scheduler is fully described. Bank 0 (system) and bank 9 (FX)
can be catalogued. Banks 6–7 and most overlay-local tables are still an index
of function pointers, not a cast list.

---

## 1. Lifetime

### 1.1 `Task` (0x48)

Allocated with `memCalloc(sizeof(Task), 0)` (0x48 bytes). Inserted into the **active list**
(the list headed by `_gTaskActiveList`) in **priority order**: lower
`priority` runs earlier. Typical values:

| Priority | Who |
|----------|-----|
| `0x10` | Boot, memcard |
| `0x18` | Main gameflow |
| `0x20`–`0x2F` | View / HUD / some gameplay |
| `0x50`–`0x70` | Type-1 TMD attaches (bank 7, shared `Gp_EffAttachTask37`) |
| `0xC0` | Default actor |
| `0xE0`–`0xF8` | Draw / load-wait (late in the list) |

Parent/child is a **sibling ring**: `firstChild` is the head, `nextSibling`
walks the ring and is self when the task is an only child. `taskKill` runs
every child’s `exitCallback` first (with `parent` cleared). `taskReparent`
detaches a live task from its old ring and appends it to the destination
parent’s ring, preserving that ring’s head. Reattaching to the same parent can
change child teardown order. Execution-list membership is unchanged.

`work` is an opaque `void*` to callback-defined storage. Default teardown
passes a non-NULL value to `memFree`, so heap work belongs to the primary heap;
callbacks release any nested resources first. A callback using borrowed storage
must clear `work` before invoking default teardown. For example, the actor_503500
tasks use static work slots and clear the pointer in their exit handlers.
`_stageMusicSelectEntry` allocates its eight-byte `_StageMusicSelection` to hold
the selected entry index and a borrowed map-overlay music table; UI,
scripts, title and other tasks supply their own types.

### 1.2 Spawn

```c
Task* taskSpawn(s32 bank, TaskSpawnArg selector, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2);
Task* taskSpawnFromTable(TaskDesc* table, s32 index, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2);
static Task* _taskSpawnFromDesc(TaskDesc* desc, TaskSpawnArg spawnArg1, TaskSpawnArg spawnArg2, TaskNode* listHead);
```

For `bank` in 0..14, `taskSpawn` selects
`gTaskDescBanks[bank][selector.value]`; the selector is a signed element index.
Every negative `bank` instead selects the descriptor at `selector.pointer`,
without indexing it. Neither path validates its selector: the selected
descriptor must be live and must not be a table terminator. The currently
selected execution-list head must already be initialized.

Both payload words land unchanged in `Task::spawnArg1` / `spawnArg2`. The
callback defines their interpretation and the lifetime and ownership of any
pointed-to storage. Spawning reads the descriptor synchronously and does not
run the callback; a task inserted after the current walk's cursor can run later
in that same walk. The task owns its attached body, while callback code and
borrowed model geometry must remain loaded for their use. Allocation or required
body-attachment failure returns NULL without inserting a task.

`TaskDesc` (0xC):

| Off | Member | Role |
|-----|--------|------|
| 0x0 | `header.fields.flags` | Low byte = body kind (0/1/2); `TASK_DESC_SKIP_AUTO_MODEL_BUFFER` disables automatic TMD primitive-buffer allocation and missing-buffer recovery |
| 0x2 | `header.fields.priority` | Low byte copied to `Task::priority`; equal priorities retain spawn order |
| 0x4 | `callback` | Per-frame entry (`Task::callback`) |
| 0x8 | `data.model` / `data.value` | Type-1 only, the `TmdSource*` for `modelObjectAttachTmdWithBufferFlags`; metadata ignored by ordinary spawning for other body kinds |

`header.word` reads both complete halfwords as one little-endian word, with
flags in the low half and priority in the high half. Location-table selection
compares that word with priority 32/body kind 0, then matches `data.value`
against `stage * 10000 + area * 100 + room` or the area key with room 0.
Its walk ends when the complete flags halfword is `TASK_DESC_END`.

The descriptor is read synchronously; no descriptor pointer is retained in the
new task. Its data word is separate from the two call-supplied spawn payloads.
`taskSpawnFromTable` takes a signed, unchecked element index into a live table.
`TaskSpawnArg` transports one word unchanged; callbacks determine its units,
pointer lifetime and ownership. The private factory does not invoke the task's
callback. It inserts by ascending byte priority, preserving spawn order among
equals, and initializes the exit callback to `taskKill`. Unrecognized nonzero
body kinds fail to spawn; a missing model primitive buffer alone does not.

`TASK_DESC_SKIP_AUTO_MODEL_BUFFER` occupies descriptor bit 8 (`0x100`). For a
TMD body, spawning translates it to creation-buffer bit 0 (`1`), which leaves
the primitive buffer NULL and sets the runtime `TMD_OBJECT_SKIP_AUTO_BUFFER`
mask (`0x04`). The model and its coordinates are still allocated. Missing-buffer
recovery skips the model while that runtime bit remains set; explicit buffer
allocation and release ignore it. Other body kinds ignore this descriptor option.

Spawn type (low byte of `header.fields.flags`, stored as `Task::bodyKind`) is the body:

| Type | Attach (`Task::extra`) | Kill teardown |
|------|------------------------|---------------|
| 0 (`TASK_BODY_NONE`) | no body allocation; `extra.allocation = NULL` | no body to release; normal teardown marks the task for collection, immediate teardown frees it |
| 1 (`TASK_BODY_TMD`) | `modelObjectAttachTmdWithBufferFlags(task, data.model, flags)` — `extra.tmd` | unlink + free TMD (normal teardown waits two countdown-callback dispatches) |
| 2 | `modelObjectAttachCoordBody(task)` — `extra.coordBody` | unlink + free coordinate body immediately |

`TaskBody` holds one allocation pointer. Select its typed member using
`bodyKind`: a model owns `partCount` coordinates, while a coordinate body owns
exactly one. `extra.allocation` supplies the kind-independent NULL check during
spawn. A copy of this union borrows the body; teardown leaves the pointer bits
unchanged, so the release marker (`bodyKind` 0xFF, privately named
`TASK_BODY_RELEASED` in `src/main/task.c`) forbids further body access. It marks
a task ready for collection even if that task never had a body;
`TASK_BODY_NONE` instead denotes a live task without an attached body. Such a
task can still own callback work and children, and a successful later body
attachment replaces this kind.

If attach fails, spawn returns NULL and frees the `Task`. `exitCallback`
defaults to `taskKill`.

### 1.3 Tick

`GameMain_Loop` rebuilds the OT, then:

```c
taskExecDefaultList();   // walks and selects gTaskDefaultList
```

Each node’s `callback` runs, then the walk checks:

- `gDisplayState.stopTaskWalk == 1` — abort the rest of the list this frame
  (gameflow uses this after killing the world and respawning).
- `bodyKind == TASK_BODY_RELEASED` (0xFF) — unlink + `memFree` this node,
  continue. The callback can set this marker during the same walk. A stop
  request takes precedence and leaves the marked node for a later walk.

`taskExecList` is the same walk on an arbitrary list.
`taskExecListForPriority(list, pri)` only runs callbacks on nodes whose `priority`
equals `pri & 0xFF` (stage load uses `0x62`), but checks every node for release.
`taskCallExitForPriority` does the same collection after dispatching selected
exit callbacks.

Unfiltered walkers leave their head selected. Filtered walkers save and restore
the previous selection, including after a stop. Callbacks must restore temporary
list switches so tail unlinking uses the owning head. Walks read the forward
link after dispatch, so tasks inserted after the cursor can run in the same walk;
tasks inserted before it wait until a later walk. The default walker ignores its
definition's `unusedListHead` parameter. Its private declaration stays
unprototyped because the main loop supplies no argument while the display path
supplies the default head, and both call sequences must match.

`taskCallExit` dispatches the installed exit handler; it does not itself mark or
free the task. That handler may release it immediately. The filtered exit walk
still reads the cursor's body kind and forward link afterward unless stopped.
Immediate grenade exits therefore require those released bytes to remain intact
until advancement; primary-heap free preserves the payload, but a callback must
not reuse it before the read. Collection of a marked task saves its successor
before unlinking and freeing the allocation.

### 1.4 Kill

`taskKill` is also the default `exitCallback`. It:

1. Detaches children and calls each child’s `exitCallback`.
2. Unlinks from the parent ring.
3. Frees `work` if set.
4. Tears down `extra` according to `bodyKind`, releasing bodies immediately
   when `gDisplayState.immediateTaskFree` is set.
5. Normally marks non-model tasks `bodyKind = 0xFF` during this call; models
   receive the mark when their release countdown finishes. A walker can collect
   a marked task after callback dispatch during the same walk, unless a stop
   request ends the walk first. The immediate path unlinks and frees the task
   during this call without setting the mark.

Normal type-1 teardown installs `taskCountdownCallback` with `killCountdown` 2
instead of freeing the model in that call. That callback unlinks and frees the
model on the dispatch that stores zero, then marks `bodyKind` 0xFF so the
invoking walk can collect the task unless it stops first.

Normal teardown installs `taskNoopCallback` as `exitCallback` to suppress
repeated cleanup. For type 0/2 it also replaces `callback` with this inert
handler; `bodyKind` is marked released during the same teardown call, and the
execution pass collects the task after its callback returns.

The task must be live and not already torn down. Direct `taskKill` calls bypass
any replacement exit handler, so callers release nested resources and clear
borrowed `work` first. Released work and body pointer slots are not cleared.
Immediate release of a list's last task requires that list to be
`gTaskDefaultList`, including when release comes from a child's exit handler:
the unlink updates that head's tail regardless of the selected list. The
previous selection is restored. Child exit handlers must preserve the sibling
successor until the parent's post-handler read, including when releasing the
child immediately; callers must not access a task after its immediate release.

`taskRequestKill(task, result)` marks `status = 0xFF`, stores the signed result
in `extraState.value` and installs `taskNoopCallback` to suspend frame updates.
It also clears each child's parent and dispatches its exit handler, retaining
the post-handler sibling-read contract above, then clears `firstChild`. The
request leaves the task's own resources, parent relationship and execution-list
membership intact; it does not dispatch the task's own exit handler.

`taskPollKill(task, resultOut)` returns false without changing the output or
dispatching an exit unless `status` is 0xFF. When requested, it copies the
stored result to a non-NULL output before calling the current `exitCallback`,
then returns true. This Boolean reports exit dispatch, not the stored result or
completed resource release: a custom handler may retain the task, and default
model teardown remains deferred. The status byte is left unchanged. The caller
must stop polling after success; an immediate exit may already have freed the
task. Both functions require a live, non-NULL task and loaded exit handlers.

### 1.5 Lists

| List | Role |
|------|------|
| `gTaskDefaultList` | Resident head of the default priority-ordered execution list; initialized and selected on boot / session reset |
| `_gTaskActiveList` | Private borrowed pointer selecting the head used for spawning and tail unlinking |
| `gTaskDisplayList` | Task-owned presentation list. `displaySpawnTaskFromTable` / `displaySpawnTask` configure two 64-tag OTs and the static primitive arena, initialize and select the list, spawn onto it, then restore the previous selection |

Both display spawners return NULL without changes when the game loop does not
own presentation. A successful spawn selects task-owned presentation and requests
the bare-OT mode; the callback runs on a later presentation walk. Allocation or
body-attachment failure leaves ownership and the pending mode unchanged, but the
buffer configuration and empty display list remain. The table form selects an
unchecked descriptor index; the bank form follows `taskSpawn`'s bank/index or
negative-bank/direct-descriptor contract. Neither retains the descriptor.

`taskSpawnFromTableOnDefaultList` / `taskSpawnOnDefaultList` temporarily switch
`_gTaskActiveList` to the default list so a spawn from inside another list
still lands on the main frame walk.

`gTaskDefaultList` needs initialization before spawning: its zeroed BSS has no
self-pointing tail. Resetting it discards the links without invoking exit
callbacks or freeing allocations. Boot and session resets pair this with heap
reinitialization to reclaim the abandoned allocations.

The selection borrows a live, initialized bare `TaskNode`; it owns neither the
head nor its tasks. Before initialization the pointer is NULL. An empty list
still has a head, with `next == NULL` and `prev` pointing to the head itself.
List initialization selects its head. Unfiltered walks select their head on
entry without restoring the previous selection. The default frame walk selects
`gTaskDefaultList`; filtered update and exit walks save and restore the previous
selection, including when a stop request ends the walk.
Other temporary switches use `taskGetActiveList` / `taskSetActiveList` to save
and restore it. Selecting a head changes the spawn and unlink context without
moving any tasks between lists.

---

## 2. `gTaskDescBanks`

`gTaskDescBanks` is 15 pointers (0x3C bytes at `0x8005EF74`). Banks **11–13
are aliases of bank 2**. Deduped size is **996** descriptors (~1047 if aliases
are counted). Roughly 155 of those are `taskKill` placeholders and 13 are
NULL.

| Bank | Symbol | n | What it is |
|------|--------|--:|------------|
| 0 | `D_8005EDA0` | 39 | System: boot, title, gameflow, memcard, view, HUD |
| 1 | `D_800670D0` | 51 | Enemies / room coords / TMD helpers + `0x807xxxxx` overlay |
| 2 (=11–13) | `D_80067828` | 17 | Caption, pad helpers, script-18, room overlay, one stage overlay |
| 3 | `D_80062780` | 8 | Four `taskKill` stubs + four `0x807xxxxx` |
| 4 | `D_800676A8` | 9 | Stubs + TMD / overlay |
| 5 | `D_800626AC` | 5 | Stubs + `taskDebugLaunchCallback` + one overlay |
| 6 | `D_8010FC2C` | **667** | Room-overlay actor catalog (gameplay data → `0x8017xxxx`) |
| 7 | `D_800678F4` | 164 | Equipped TMD attaches (`modelObjectChildTask` + per-item `data.model`) |
| 8 | `D_800626EC` | 6 | Stubs + shared `Gp_EffAttachTask37` |
| 9 | `D_80067734` | 19 | FX / wait: shake, volume fade, sound fade, end-wait |
| 10 | `0x80114B34` | 6 | Stubs + `Gp_EffAttachTask37` (splat-merged into `Gp_CollectedIds`) |
| 14 | `D_80068B7C` | 5 | Stubs + one `0x807xxxxx` |

Callback addresses fall in four windows:

| Range | Resident |
|-------|----------|
| `0x8001xxxx`–`0x800937FF` | Main executable |
| `0x80093800`–`0x80115769` | Gameplay *or* title overlay |
| `0x80115770`+ | Aya / weapon / actor / **room** overlays ([`OVERLAYS.md`](OVERLAYS.md) §2) |
| `0x807xxxxx` | Imported overlay, not splat’d in this tree |

`Gp_EffAttachTask37` is a generic type-1 TMD actor reused in banks 1, 2, 4, 6, 8, 10
(and the bank-2 aliases). It is still a `func_*`.

---

## 3. Bank 0 — system

This is the only bank we can describe entry-by-entry. Spawn with
`taskSpawn(0, type, spawnArg1, spawnArg2)`.

| Type | Pri | Callback | Notes |
|------|-----|----------|-------|
| `00` | `C0` | `taskNoopCallback` | Inert handler; also suppresses updates and repeated teardown |
| `01` | `C0` | `taskCountdownCallback` | Decrement signed `killCountdown`; at zero, release the body and mark for collection |
| `02` | `C0` | `titleScreenTask` | Title phase machine. `Text_BootTask` / gameflow / title spawn this; `spawnArg1` `0x80000000` skips the fade TILE |
| `03` | `C0` | `GameFlow_StateByField34` | Title new-game / demo path. Also a `Title_MenuSpawnIds` entry |
| `04` | `C0` | `gameFlowLoadDialogTask` | Memory-card load flow: reset, create/wait/close the dialog, then return to the title or start the loaded session. Also a `Title_MenuSpawnIds` entry |
| `05` | `C0` | `Text_UiTaskCallback` | Text / UI. Also a `Title_MenuSpawnIds` entry |
| `06` | `C0` | `titleExitTask` | Dispatches this task's installed exit handler (initially `taskKill`). Also a `Title_MenuSpawnIds` entry; requires the title overlay |
| `07` | `C0` | `taskKill` | Unused slot |
| `08` | `00` | NULL | Unused |
| `09` | `18` | `gameFlowStartSessionTask` | Session startup: restore the live-save location, wait for the required disc, queue the initial load, then hand off when the CD queue drains. |
| `0A` | `10` | `mcSaveDialogTask` | Memory-card save dialog |
| `0B` | `10` | `mcLoadDialogTask` | Memory-card load dialog and file selection |
| `0C` | `C0` | `func_80036A1C` | Memcard menu dispatcher (`mcmenu.c`) |
| `0D` | `10` | `Text_BootTask` | Boot: load CLUT, spawn `Title_TaskDescs[0]`, kill self. `Boot` also spawns this |
| `0E` | `2F` | `viewApplyCoordTask` | Type **2** (coordinate body). Gameplay dispatcher |
| `0F` | `2F` | `viewApplyCameraTask` | Camera / view. `viewQueueCamera` / `viewQueueCurrentCameraAndPackets` |
| `10` | `40` | `loadingRoomResourcesTask` | Room collision setup, view refresh and clipping; frozen dispatch suppresses the background |
| `11` | `28` | `gameFlowReloadSessionTask` | Reload from the live save: combat exit, display hold, resource rebuild; first argument selects display handling and may skip battle escape |
| `12` | `10` | `mcSaveDialogTask` | Same as `0A` |
| `13` | `10` | `mcLoadDialogTask` | Same as `0B` |
| `14` | `1F` | `directionTask` | Direction trigger task (`area_transitions.c`, matched) |
| `15` | `C0` | `taskKill` | Unused |
| `16` | `30` | `viewTransitionGateTask` | Monitor saved-view changes, admit view loading and gate readiness with a two-update menu hold |
| `17` | `2F` | `spriteAllocateViewCachedPacketsTask` | Allocate and initialize both cached room-view sprite buffers, then kill self; spawned when view-image loading finishes |
| `18` | `C0` | `taskExitCallback` | Dispatch the task's current exit handler |
| `19` | `C0` | `0x807011D8` | Stage overlay — not in this tree |
| `1A` | `E0` | `modelObjectDrawTemporaryListsTask` | Compose the temporary live model/coordinate-body lists and draw active models into the selected `gGpuCurrentOt`; task argument is ignored. Spawned as `_gModelObjectTemporaryDrawTask` while the previous lists are stashed |
| `1B` | `D0` | `spriteViewTask` | Select the view background, then link cached sprites each frame; frozen with room-object dispatch. Parents the room-object task |
| `1C` | `2F` | `Gp_LoadStateTask` | 8-way dispatcher (pause / menu-ish) |
| `1D` | `18` | `playClockTask` | Play-time/HUD updates, death presentation and session restart; six states |
| `1E` | `F8` | `loadingViewLoadTask` | Six-state view-resource/image load; completion resumes the game loop or publishes scene readiness according to spawn mode |
| `1F` | `10` | `Boot_LoadInitialFile` | Cold boot (`D_8005EC64 == 1`) |
| `20` | `10` | `Boot_LoadTask` | Cold boot (otherwise). `GameMain_SpawnBootTask` |
| `21` | `2F` | `fadeResumeSessionTask` | Hold black, reveal the loaded session, then release display/pause holds (`companion_load.c`) |
| `22` | `F8` | NULL | Unused |
| `23` | `C0` | `0x80701400` | Stage overlay — not in this tree |
| `24` | `2F` | NULL | Unused |
| `25` | `F8` | `viewCommitIndexTask` | Commit the low-byte view index to session and live save; kill self |
| `26` | `F8` | `loadingRestoreViewGraphicsTask` | Restore model/sprite packets, kill self and resume game-loop presentation |

`Title_MenuSpawnIds` (6 words) is `{6, 6, 3, 4, 5, 6}` — bank 0 types spawned
from `Title_MenuTask` on confirm.

Several `func_*` rows are already matched C and only lack a role name.

---

## 4. Bank 9 — FX / wait

| Type | Pri | Callback | Notes |
|------|-----|----------|-------|
| `00`–`05`, `09`–`0A`, `0F`–`10` | `C0`/`20` | `taskKill` or NULL | Unused |
| `06` | `80` | `capControlTask` | Persistent CAP controller: initialize, relocate/update and exit |
| `07` | `20` | `evsInterpreterTask` | Event-script interpreter; borrows the command stream in spawnArg2 and takes a display hold |
| `08` | `80` | `capHudSlideTask` | CAP demo-scene HP/MP slide; `spawnArg1.value` is -1 to hide, +1 to return; live handle `D_801156B8` |
| `0B` | `80` | `Gp_EndWaitTask` | `spawnArg2` is `CapActionRequest*`; non-zero `done` sets the ending flag and kills |
| `0C` | `20` | `evsScreenShakeTask` | Vertical display shake; packed `spawnArg2.value` holds signed amplitude above bit 7 and half-duration (1..255) in the low byte |
| `0D` | `20` | `evsMusicVolumeFadeTask` | `spawnArg2` is `_EvsMusicVolumeFade*` (music volume: target level + duration) |
| `0E` | `20` | `evsSoundAttenuationFadeTask` | `spawnArg2` is `_EvsSoundAttenuationFade*` (one sound's attenuation: target + duration) |
| `11` | `20` | `objectTaskRoomTask` | Select the current room/area task; remain idle as the room message receiver when no descriptor matches |
| `12` | `20` | NULL, `flags = 0xFFFF` | Sentinel |

`CapActionRequest` is shared with CAP playback and lives in `src/gameplay/cap.h`.
The other payload structs live with their sole consumers: `_EvsMusicVolumeFade` /
`_EvsSoundAttenuationFade` in `src/gameplay/evs_scripts.c`.

---

## 5. Other banks (what is proven)

### Bank 2 — caption, pad, script (aliases 11–13)

| Type | Callback | Notes |
|------|----------|-------|
| `07` | `func_800E70AC` | **Caption / dialogue.** `Gp_CapTask = taskSpawn(2, 7, …)` or `displayQueueModeTask(taskGetDesc(2, 7), …)` |
| `0B` | `padScriptBinaryMotorHoldTask` | `_padScriptSpawnBinaryMotorHold` — port 0 binary-motor vibration, remaining script frames in `spawnArg1.value` |
| `0C` | `padScriptVariableMotorRampTask` | `padScriptSpawnVariableMotorRamp` — port 0 variable-motor Q8 intensity ramp, owned work block in `work` |
| `0D` | `padScriptTask` | Two-lane controller-vibration interpreter; borrowed command and segment arrays in owned work |
| `06`, `10` | `Gp_EffAttachTask37` | Shared type-1 TMD |
| `04` | `0x807257A0` | Stage overlay |
| `08`–`0A`, `0E`–`0F` | `0x8017xxxx` / `0x8018xxxx` | Room overlay |

### Bank 1 — enemies + overlay

Named / matched: `enemyTeardownDelayTask` (`0xB`), `worldCoordUpdateRoomLightsTask` (`0xF`, room lights),
`worldCoordPlayerLightingTask` (`0x10`, player/companion lights),
`tmdReleaseAttachedBuffersTask` (`0x21`), `tmdRestoreAttachedBuffersTask` (`0x22`),
`fadeDisplayTransitionTask` (`0x27`). The rest is `taskKill`, `func_*`, or
`0x807xxxxx` (many type-1 with `data.model = 0x8075BED4`).

`tmdReleaseAttachedBuffersTask` runs one of three states per dispatch: hide
attached models from active drawing (0), release their primitive buffers (1),
then kill the bodyless task (2). The first two states each advance once and
walk the current attached-model list independently. Models and coordinates stay
attached; buffer users, including the GPU, must finish before state 1 runs.

`_enemySpawn(bank, selector, spawnArg, parent)` is `taskSpawn` plus a primary-heap
`Enemy` allocation in `spawnArg2.pointer` (`_enemyAllocateWork`). `enemyTaskExit`
releases that allocation before default task teardown.

### Bank 6 — 667 room actors

Gameplay-resident table at `D_8010FC2C`. Almost every non-stub callback is in
**room overlay RAM** (`0x8017D5C0` and following — see [`OVERLAYS.md`](OVERLAYS.md)
§2). Named screen-effect entries include `_effectStatusScreenTintTaskF` (0xF)
and `_effectDarknessScreenDimTaskE8` (0xE8). A handful of `func_800Exxxx` /
`func_800Fxxxx` sit in the gameplay overlay; they are not named.

This is the large “what’s in the room” catalog. Describing a row means
decompiling the overlay it points at.

### Bank 7 — equipped TMD attaches

~80 `taskKill` stubs; the rest are type 1, priority `0x50`/`0x52`, callback
usually `modelObjectChildTask` (gameplay-resident), `data.model` a `TmdSource*` in
weapon / actor overlay RAM (`0x8011xxxx`, `0x8016xxxx`, `0x8018xxxx`).

`src/gameplay/player_actor.c` spawns these as `taskSpawn(7, type, …)` when attaching
gear to an actor, then rewrites `parent` and TMD coord links.

### Banks 3, 4, 5, 8, 10, 14

Placeholder slots plus a few overlay or shared-TMD callbacks. Bank 5 type `1`
is `taskDebugLaunchCallback` (`taskutil.c`): when `gDisplayState.debugMode`
is nonzero, it requests descriptor 0 through a `TaskDesc*` view of the external
debug address `0x80725C54`, with both spawn payload words zero. It then tears
down its own task, including when spawning fails or debug is disabled. The
backing debug image and descriptor contents are unproven.

---

## 6. Tables outside the banks

These are real actors too; they just skip `gTaskDescBanks`.

| Table | Callback / role |
|-------|-----------------|
| `Title_TaskDescs[0]` | `Title_BootTask` |
| `Title_TaskDescs[1]` | `_titleIntroMovieTask` (`displaySpawnTaskFromTable`) |
| `D_8006269C[0]` | `Display_DispatchTaskTable` — 6-way stage load (`_stageSuspendCdAndSpawnModeTask` … `_stageResumeMovieAndFinishModeTask`) |
| `D_80062774[0]` | `_stageMusicTask` — bank-load spawn from gameplay |
| `D_8006268C[0]` | `0x800BF9FC` (gameplay) |
| `Stage_Ctx->taskDesc` | Per-stage desc table; `_stageSpawnModeTask` spawns index 0 |
| `D_80725C54` | External debug-address descriptor view, from `taskDebugLaunchCallback`; backing storage unproven |
| `D_8010D1FC`, `D_8010FB4C`, `D_80115D9C`, `D_80119218`, `D_8011922C`, `D_80113340`, `D_80183824`, … | Gameplay / save-slot / enemy tables (`1BC.c` `func_800B25B0` switches on `gMcSaveData`) |
| Stack `TaskDesc` | `uiSpawnObject` copies `UiObjectDesc.taskFlags`, `taskPriority` and `taskDataValue`; task callback dispatches the panel, which keeps `contentCallback` |

`taskGetDesc(bank, index)` borrows `gTaskDescBanks[bank][index]` without copying
a descriptor or spawning a task. The unsigned bank must be in 0..14 and the
unsigned element index must address an entry in its table; neither is checked.
Banks 11..13 share bank 2's table, so their returned pointers alias. Callers keep
the table loaded while using the result and exclude terminators when spawning.
`displayQueueModeTask` uses this lookup to queue a bank entry without spawning
onto the currently selected execution list.

---

## 7. Work that hangs off a task

The `Task` is the actor. Specific systems stash extra state in the leftover
slots (see [`include/main/task.h`](../include/main/task.h)):

| Slot | Typical payload |
|------|-----------------|
| `extra` | `TmdObject*` / TMD object (type 1) or a coordinate body (type 2) |
| `spawnArg2` | `Enemy*`, `UiObject*`, `_EvsMusicVolumeFade*`, `_EvsSoundAttenuationFade*`, `CapActionRequest*`, view record, … |
| `work` | Opaque `void*` to callback work; default teardown frees non-NULL primary-heap storage |
| `msgTable` | Borrowed `const void*` to id/handler records; callbacks have receiver-specific signatures |
| `state` | Dispatcher index (`TaskFuncTable3`–`8` copied onto the stack) |
| `status` | The task's own byte: a notice id, a course id, a parent's value copied down. `0xFF` is the stop request |
| `killCountdown` | The task's own frame timer; the teardown delay while the task is being freed |
| `extraState` | A payload the task carries; `taskPollKill` hands it back with the stop request |

Message tables use an id word followed by a handler address, but their handler
parameter counts, payload types and return types vary. `taskMessageDispatch` reads
through a const `TaskMessageEntry` view and supplies four ABI words. A caller must use
an id supported by the receiver or a table with the dispatcher's
`TASK_MESSAGE_TABLE_END` (`0x7FFFFFFF`) terminator; some installed tables have no
such terminating entry. The reserved end ID must never be dispatched.
The task borrows the table and never releases it.

`gameSetTaskSlot` / `gameGetTaskSlot` (`GameSession::ptrSlots`) provides 16
borrowed task registrations independent of the execution lists. Getter indices
are elements in 0..15 and are unchecked. Empty registrations return `NULL`;
registration does not keep a task alive or clear itself on task exit. Player
and companion spawns register immediately, before their first tick publishes
them in `gPlayerActorTasks`.
`gameClearTaskSlots` clears all 16 registrations without dispatching handlers or
releasing tasks; session reset does this before discarding the list and heap.

---

## 8. What is not described yet

- **Bank 6** (667 room-overlay callbacks) and most of **bank 7** (per-item TMD
  sources). The tables are complete; the functions are not.
- **`0x807xxxxx`** overlays referenced from banks 0/1/3/4/5/14. No splat tree.
- **Enemy / room `TaskDesc` tables** (`enemySpawnFromTable`, `func_800B25B0`
  save-slot switch). Overlay-local, mostly unnamed.
- **UI** tasks built from `UiObjectDesc` rather than a bank index.
- Bank-0 play monitoring is named `playClockTask`; remaining unnamed matched
  callbacks still need their own role review.

Adding a bank-6/7 row to this file without a proven role is just publishing an
address. Prefer renaming the callback (or its overlay) first.
