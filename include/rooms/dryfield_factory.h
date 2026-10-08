#ifndef INCLUDE_ROOMS_DRYFIELD_FACTORY_H
#define INCLUDE_ROOMS_DRYFIELD_FACTORY_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// dryfield_factory
extern WorldCollisionRoomResources D_dryfield_factory_80186F10[];

extern u8* D_dryfield_factory_80186F44[];

extern WorldCoordRoomLighting D_dryfield_factory_80186F4C[];

extern ViewCount D_dryfield_factory_80186F5C[];

extern DirectionWarpEntry D_dryfield_factory_80186F60[];

extern ViewCamera D_dryfield_factory_80187C1C[];

extern SpriteView D_dryfield_factory_801895B0[];

extern WorldCollisionSurfaceProperties* D_dryfield_factory_8018A37C[];

/// Sets the power-controlled sprite visibility in the daytime factory's mapped view 9.
///
/// The low byte of `visible` selects hidden (0) or visible (nonzero); higher
/// bits are ignored. Only the daytime Dryfield stage changes sprite records.
/// Requires a live session and, in that stage, the loaded factory sprite tables,
/// sprite variant 1, mapped view 9 and writable batch 1. The batch holds one
/// sprite; its hidden flag controls subsequent drawing. The loaded overlays own
/// the sprite tables.
void dryfieldFactorySetView9SpriteVisible(s32 visible);

/// Draws the daytime factory's power and lamp glows for the current logical view.
///
/// Per-frame effect callback for room views 1..19. Power enables one fixed-world
/// glow; lamp progress 1 or 2 selects one of two positions and colours, so at
/// most two glows are queued. Other lamp progress values draw no lamp glow.
/// Requires the composed view, initialized scratch stack and current packet
/// arena and ordering table. Packets live until this frame's GPU work completes.
/// The callback leaves task state unchanged and does not use `task`.
void dryfieldFactoryDrawGlowsTask(Task* task);

/// Dispatches the daytime factory room task's setup, idle and teardown states.
///
/// Requires a live bodyless task with `Task::state` in 0..2; the three handlers
/// are copied onto the stack and indexed without a bounds check. Setup installs
/// the room's message table, owns a panel-task slot at `Task::work`, and starts
/// the lift and barrier tasks. Idle keeps the room available for messages;
/// teardown releases its work. The factory overlay must remain loaded while
/// the task can run or receive messages.
void dryfieldFactoryEntryTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_FACTORY_H
