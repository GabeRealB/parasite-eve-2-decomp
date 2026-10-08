#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_FACTORY_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_FACTORY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_factory_8018A70C[11];

// dryfield_night_factory
extern u8* D_dryfield_night_factory_80186F1C[];

extern WorldCoordRoomLighting D_dryfield_night_factory_80186F24[];

extern WorldCollisionRoomResources D_dryfield_night_factory_80186F34[];

extern ViewCount D_dryfield_night_factory_80186F54[];

extern DirectionWarpEntry D_dryfield_night_factory_80186F58[];

extern ViewCamera D_dryfield_night_factory_80187C14[];

extern SpriteView D_dryfield_night_factory_80189A24[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_factory_8018A79C[];

/// Night factory instance of the mapped view 9 power-sprite visibility setter.
///
/// The low byte of `visible` selects hidden (0) or visible (nonzero); higher
/// bits are ignored. This instance also acts only in the daytime Dryfield stage;
/// calls in the night stage leave sprite records unchanged. Requires a live
/// session and, in the daytime stage, the loaded factory sprite tables, sprite
/// variant 1, mapped view 9 and writable batch 1 containing the power sprite.
/// The hidden flag controls subsequent drawing. The loaded overlays own the
/// sprite tables.
void dryfieldNightFactorySetView9SpriteVisible(s32 visible);

/// Draws the nighttime factory's power and lamp glows for the current logical view.
///
/// Per-frame effect callback for room views 1..19. Power enables one fixed-world
/// glow; lamp progress 1 or 2 selects one of two positions and colours, so at
/// most two glows are queued. Other lamp progress values draw no lamp glow.
/// `task` must have a live coordinate body and writable coordinate parent chain;
/// its cached transform is refreshed before drawing, without changing task state.
/// Requires the composed view, initialized scratch stack and current packet
/// arena and ordering table. Packets live until this frame's GPU work completes.
void dryfieldNightFactoryDrawGlowsTask(Task* task);

/// Dispatches the nighttime factory room task's setup, idle and teardown states.
///
/// Requires a live bodyless task with `Task::state` in 0..2; the three handlers
/// are copied onto the stack and indexed without a bounds check. Setup installs
/// the room's message table, owns a panel-task slot at `Task::work`, and starts
/// the lift and barrier tasks. Idle keeps the room available for messages;
/// teardown releases its work. The factory overlay must remain loaded while
/// the task can run or receive messages.
void dryfieldNightFactoryEntryTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_FACTORY_H
