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

void func_800A99B4(void);

void Gp_EnqueueHeldWeaponCd(void);

void Gp_EnqueueConfigCd(s32 arg0);

/// Commits a logical view index to the current session and live save, then kills the task.
///
/// Bank-0 task 0x25. Uses the low eight bits of `task->spawnArg1.value` without
/// validation or view-resource lookup. Requires a live task and `gGameSession`;
/// changes only the view bytes, without loading resources or applying a camera.
void viewCommitIndexTask(Task* task);

void Gp_LoadWaitDispatch(Task* task);

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
/// Each room has eight surface-class pointers; `Gp_LoadRoomParams` copies only
/// their `suppressPushback` flags into `Gp_RoomParams`.
extern WorldCollisionSurfaceProperties*** Gp_RoomParamTables[];

/// Per-stage pointer table. Index is `GameSession.location.loc.stage - 1`.
extern SpriteAreaTable* Gp_SprtTables[];

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

/// 1-based index of `(u8)arg0` in the current room's `Gp_ViewIndexTables` byte
/// list. Length is the `Gp_ViewCountTables` cell as an s16. Returns 0 if absent.
s8 Gp_FindViewIndex(s32 arg0);

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

void func_800AD50C(Task* task);

/// Dispatches background selection and cached-sprite drawing for the current view.
///
/// Bank-0 task 0x1B. A live `task` has state 0 (select the background and advance
/// to state 1) or 1 (repeatedly link cached packets, or refresh the background
/// while drawing is suppressed). Nonzero `freezeRoomObjs` holds both phases.
/// Linking runs when presentation is not task-owned and `skipDraw` is zero;
/// requires the loaded resources and packet contract of `spriteLinkViewCachedPackets`.
void spriteViewTask(Task* task);

#endif // GAMEPLAY_LOADING_H
