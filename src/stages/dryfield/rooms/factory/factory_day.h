#ifndef STAGES_DRYFIELD_ROOMS_FACTORY_FACTORY_DAY_H
#define STAGES_DRYFIELD_ROOMS_FACTORY_FACTORY_DAY_H

#include <psyq/sys/types.h>

#include "gameplay/collision.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

/// Directed camera-change quads shared by both daytime factory room variants.
///
/// Fourteen records form seven paired boundaries between logical views
/// 2/3, 4/7, 7/8, 4/6, 8/9, 3/4 and 2/6. The LAST-marked final record is a
/// live boundary. Room activation binds their coordinates and mutable list
/// links; keep this overlay loaded until they have been unlinked.
extern WorldCollisionTrigger gDryfieldFactoryViewBoundaryTriggers[14];

/// Interaction quads shared by both daytime factory room variants.
///
/// The twenty live records request driveway/breezeway/garage warps, room
/// commands, CAP playback or an automatic room action. Geometry uses room
/// coordinates, with 4096 per unit normal. The final record carries LAST;
/// activation supplies the view coordinate, links and enables the records.
/// Their hit latches and links are mutable and require this overlay to stay
/// loaded until the action list is cleared.
extern WorldCollisionTrigger gDryfieldFactoryActionTriggers[20];

/// The daytime factory's shared collection of five mutable point lights, with no directional or cone lights.
///
/// Both room variants borrow this single record and its light array while the
/// overlay is loaded. Coordinate composition and light queries update the lights.
extern WorldCoordRoomLights gDryfieldFactoryRoomLights;

/// Minimum ambient RGB and stored luma for the daytime factory's logical views 1..19.
///
/// Entry zero gives the following view count. Levels use 16 units per 8-bit
/// colour level; lighting reads this table without applying the sprite view map.
/// Both room variants borrow the same table for the loaded overlay's lifetime.
extern WorldCoordRoomAmbientEntry gDryfieldFactoryViewAmbientMinima[20];

// Callbacks referenced by the overlay's shared data tables.

s32 func_dryfield_factory_8017DB08(Task*, s32, RoomEventMsg*, RoomEventMsg*);

void func_dryfield_factory_8017DD00(Task*);

void func_dryfield_factory_8017FC18(Task*);

void func_dryfield_factory_8017FDDC(Task*);

void func_dryfield_factory_8018001C(Task*);

void func_dryfield_factory_801807DC(Task*);

void func_dryfield_factory_80180964(Task*);

#endif // STAGES_DRYFIELD_ROOMS_FACTORY_FACTORY_DAY_H
