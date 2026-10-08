#ifndef GAMEPLAY_LOADING_H
#define GAMEPLAY_LOADING_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Room-resource loading, view setup and sprite-list construction.

/// Per-area pointer table. Index is `GameLocationKey.stage`.
extern AreaRecord* Gp_AreaTables[];

/// Requests an asynchronous restoration of the current view's graphics.
///
/// Spawns `loadingRestoreViewGraphicsTask` on the display list. Requires the
/// presentation ownership, disposable task-list storage and resource lifetimes
/// of `displaySpawnTask` and `loadingRestoreViewGraphicsTask`. The callback runs
/// later; this wrapper discards the spawn result. A rejected request does
/// nothing, while allocation failure can leave prepared display buffers and an
/// empty display list without transferring presentation ownership.
void loadingRequestViewGraphicsRestore(void);

/// Queues the equipped weapon package and any ammunition-dependent resource package.
///
/// Weapon indices 1..32 select global files 10301..10332. An unequipped player
/// still queues 10301, without changing the equipped selection. Also requests
/// a later seek back to the current view. Sources are copied immediately;
/// completion is asynchronous. Requires one free CD ring slot, plus a second
/// for weapons with an ammunition-dependent package, and live player/save data.
void loadingEnqueueEquippedWeaponResources(void);

/// Queues the player's selected character resource package or only its images.
///
/// Does nothing when the live save's character ID is zero. Otherwise requires
/// `gPlayerStatus.resourceVariant` in 1..5; it selects global files 10400,
/// 10300, 10200, 10500 or 10600. A zero low byte of `imagesOnly` loads normally;
/// a nonzero low byte uploads images only. Textures shift by six 64-word VRAM
/// pages horizontally, with no vertical displacement. Requires one free CD
/// ring slot and valid file destinations; it neither waits nor updates the
/// session's character-resource cache. Request sources are copied immediately.
void loadingEnqueueCharacterResources(s32 imagesOnly);

/// Commits a logical view index to the current session and live save, then kills the task.
///
/// Bank-0 task 0x25. Uses the low eight bits of `task->spawnArg1.value` without
/// validation or view-resource lookup. Requires a live task and `gGameSession`;
/// changes only the view bytes, without loading resources or applying a camera.
void viewCommitIndexTask(Task* task);

/// Dispatches the current view's asynchronous resource and image load.
///
/// Bank-0 task 0x1E. A live task's state is 0 (begin/select scene image or movie),
/// 1 (queue view resources after CD idle), 2 (restore retained image and queue
/// movie/replacement), 3 (wait for movie readiness), 4 (wait for CD idle), or
/// 5 (wait for scene-image completion). Any negative state finishes instead of
/// indexing; handlers write -1 for ready completion and -2 for CD idle.
/// Renews port 0's input block on each dispatch; completion clears it and kills
/// the task. Nonnegative states must be in 0..5; there is no bounds check.
///
/// `spawnArg1.value` selects completion policy: 0 resumes the game loop, 1
/// resumes task-controlled flips, and 2 preserves stage-controlled flips;
/// other nonzero values behave like 2. Requires loaded location/view/resource
/// tables, live CD/decode state, GPU/heap lifetimes valid for the selected phase,
/// and free CD/task capacity for queued work. Image-upload phases additionally
/// require the source, timer and scratch contract of `fsUploadImageChunk` and
/// retry without a timeout. Scene-image completion includes timeout cleanup.
void loadingViewLoadTask(Task* viewLoadTask);

/// Restores the current view's graphics and returns presentation to the game loop.
///
/// Bank-0 task 0x26. Holds framebuffer flips, then resets GPU work and rebuilds
/// eligible attached-model buffers if `gDisplayState.keepGraphics` is zero.
/// Always allocates cached view-sprite packets, kills the task and resumes the
/// game loop, including after allocation failure.
///
/// Requires a live task, loaded view resources and the memory-lifetime contracts
/// of `spriteAllocateViewCachedPackets` and `displayResumeGameLoop`. When
/// graphics are discarded, other users of auxiliary storage must have ended
/// before `tmdResetAuxHeapAndRestoreBuffers` resets it; preserved graphics need
/// a live configured auxiliary heap. Previous cached sprite storage must already
/// be retired. No task-owned drawing may remain when game-loop presentation resumes.
void loadingRestoreViewGraphicsTask(Task* task);

/// Per-stage pointer table. Index is `GameSession.location.loc.stage - 1`.
/// Each entry is an array of `WorldCollisionSurfaceProperties**`, indexed by area - 1.
/// Each room has eight surface-class pointers; `worldCollisionLoadSurfacePushbackFlags` copies only
/// their `suppressPushback` flags into `Gp_RoomParams`.
extern WorldCollisionSurfaceProperties*** Gp_RoomParamTables[];

/// Five stage-map directories of per-area mapped-view sprite arrays.
///
/// Indexed by stage minus one (stages 1..5). Each entry borrows exactly one
/// map-overlay record; consumers using spriteVariant minus one require variant 1.
/// Area and mapped-view bounds belong to their directories and sprite arrays.
/// Selected map and room overlays must stay loaded while resources are used.
extern SpriteAreaTable* gSpriteAreaTables[5];

/// Links the current view's cached sprites into the current frame ordering table.
///
/// Selects the packet buffer with `gDisplayState.drawBuffer`, then links included
/// batches at their sources' scaled, masked sorting depths. Hidden batches still
/// consume their cached packet slots; excluded batches do not. A nonzero first
/// count suppresses background-image strips; zero leaves that selection intact
/// and advances to the next batch before testing the terminal marker.
///
/// Requires valid loaded stage/area/room/view indices and source ranges, a
/// terminated list after advancing past a zero-count first batch, cached packets
/// initialized for this view (or no allocation), and a 1024-tag depth-sorted OT.
void spriteLinkViewCachedPackets(void);

/// Allocates and initializes both cached sprite-packet buffers for the current view.
///
/// Requires loaded stage/area/room/view directories, a terminated batch list,
/// valid source ranges for included batches, and an initialized auxiliary heap.
/// Each buffer reserves one `SpriteDrawModePacket` per nonterminal source count,
/// including excluded batches; initialization packs only included batches.
/// Both buffers snapshot source geometry, texture state and code flags with raw
/// texture enabled. Initial RGB is (0,128,0), ignored while raw texture is enabled.
/// Hidden batches are initialized too; depth remains in sources and is read when
/// linking. The combined byte count must fit unsigned 32 bits.
///
/// Publishes one allocation and its second half in `Gp_SprtLists`. A zero count
/// or allocation failure sets only the first head to NULL; consumers use that
/// head as the validity guard. Does not release the previous block. Call after
/// its auxiliary storage has been reset or its previous allocation retired;
/// the new packets stay live until that storage is released or repurposed.
void spriteAllocateViewCachedPackets(void);

/// Borrows the current mapped view's draw-area list, or returns NULL if absent.
///
/// Requires valid loaded stage, area, room and logical-view indices. The list
/// ends at `SPRITE_DRAW_AREA_END` and belongs to the selected room overlay;
/// retain it only while that overlay and the selected view resources stay live.
SpriteDrawArea* spriteGetViewDrawAreas(void);

/// Initializes the current view's cached sprite packets and kills this task.
///
/// Bank-0 task 0x17; `task` must be live. Uses the resource and heap requirements
/// of `spriteAllocateViewCachedPackets` and ends even if allocation fails.
void spriteAllocateViewCachedPacketsTask(Task* task);

/// Dispatches room collision setup and per-frame view-resource maintenance.
///
/// Bank-0 task 0x10. A live `task` has state 0 (bind collision resources and
/// spawn a child `spriteViewTask`), 1 (refresh collision/view state and queue
/// clipping packets), or 2 (tear down the task and its children). Nonzero
/// `freezeRoomObjs` holds dispatch and suppresses background images instead.
/// `spawnArg1.value` becomes the last logical view seen by state 1.
///
/// Requires valid loaded stage/area/room/view resources and live collision
/// records until unlinked. State 1 also needs a composed view and sufficient
/// current-frame GPU packet storage and a 1024-tag depth-sorted ordering table.
void loadingRoomResourcesTask(Task* task);

/// Dispatches background selection and cached-sprite drawing for the current view.
///
/// Bank-0 task 0x1B. A live `task` has state 0 (select the background and advance
/// to state 1) or 1 (repeatedly link cached packets, or refresh the background
/// while drawing is suppressed). Nonzero `freezeRoomObjs` holds both phases.
/// Linking runs when presentation is not task-owned and `skipDraw` is zero;
/// requires the loaded resources and packet contract of `spriteLinkViewCachedPackets`.
void spriteViewTask(Task* task);

#endif // GAMEPLAY_LOADING_H
