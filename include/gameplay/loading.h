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

void Gp_CommitSpawnLoc(Task* task);

void Gp_LoadWaitDispatch(Task* task);

void Gp_SetupSprtDisplay(Task* task);

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

/// Alloc dual-buffer merged `DR_TPAGE`+`SPRT` lists into `Gp_SprtLists`
/// from the current view's `SpriteView` records. Byte size is the sum of
/// each batch's `spriteCount`, times two 0x1C slots. Packet initialization
/// skips batches with `skipCachedPackets` set. RGB is `0x8000`; SPRT code is `0x65`.
void Gp_AllocSprtLists(void);

/// 1-based index of `(u8)arg0` in the current room's `Gp_ViewIndexTables` byte
/// list. Length is the `Gp_ViewCountTables` cell as an s16. Returns 0 if absent.
s8 Gp_FindViewIndex(s32 arg0);

/// Borrows the current mapped view's draw-area list, or returns NULL if absent.
///
/// Requires valid loaded stage, area, room and logical-view indices. The list
/// ends at `SPRITE_DRAW_AREA_END` and belongs to the selected room overlay;
/// retain it only while that overlay and the selected view resources stay live.
SpriteDrawArea* spriteGetViewDrawAreas(void);

void Gp_AllocSprtListsTask(Task* task);

void func_800AD50C(Task* task);

void func_800AD5B8(Task* task);

#endif // GAMEPLAY_LOADING_H
