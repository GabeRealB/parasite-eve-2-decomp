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

void factoryNightEntryTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_FACTORY_H
