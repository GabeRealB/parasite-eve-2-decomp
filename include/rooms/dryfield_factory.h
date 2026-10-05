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

/// Draws the daytime factory's power and lamp glows for the current logical view.
///
/// Per-frame effect callback for room views 1..19. Power enables one fixed-world
/// glow; lamp progress 1 or 2 selects one of two positions and colours, so at
/// most two glows are queued. Other lamp progress values draw no lamp glow.
/// Requires the composed view, initialized scratch stack and current packet
/// arena and ordering table. Packets live until this frame's GPU work completes.
/// The callback leaves task state unchanged and does not use `task`.
void dryfieldFactoryDrawGlowsTask(Task* task);

void factoryDayEntryTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_FACTORY_H
