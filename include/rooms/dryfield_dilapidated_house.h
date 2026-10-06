#ifndef INCLUDE_ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H
#define INCLUDE_ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_dilapidated_house_80189938[13];

// dryfield_dilapidated_house
extern WorldCollisionRoomResources D_dryfield_dilapidated_house_80186954[];

extern u8* D_dryfield_dilapidated_house_80186964[];

extern ViewCount D_dryfield_dilapidated_house_80186968[];

extern WorldCoordRoomLighting D_dryfield_dilapidated_house_8018696C[];

extern DirectionWarpEntry D_dryfield_dilapidated_house_80186974[];

extern ViewCamera D_dryfield_dilapidated_house_80187308[];

extern SpriteView D_dryfield_dilapidated_house_80188C0C[];

extern WorldCollisionSurfaceProperties* D_dryfield_dilapidated_house_80189A80[];

/// Draws the room's three pulsing light prisms selected by the current view.
///
/// Effect-table slot 0xC8 supplies a coordinate body with a composed transform.
/// View IDs must be 0..20; each prism has its own view mask. This persistent
/// drawer does not advance task state or honor the room-effect pause control.
void dryfieldDilapidatedHouseLightPrismTask(Task* task);

/// Records and draws No. 9's yellow swing ribbon (effect slot 0x188).
///
/// `spawnArg2.pointer` is owned `EffectWork`; its borrowed `parent` is model
/// part 8's coordinate. `spawnArg1.value` is a positive lifetime in active frames
/// (the actor supplies 8 or 12). Initialization seeds two shared eight-frame
/// histories and returns without drawing. Later frames snapshot both endpoints
/// and release the counted effect when `age` equals the lifetime and is nonzero.
/// Updates continue through pause value 1 and stop entirely at hidden/cancel
/// values 2 or above. All instances overwrite the same histories at initialization.
void dryfieldDilapidatedHouseTwinTrailTask(Task* task);

void func_dryfield_dilapidated_house_80182744(Task* task);

/// Expands and fades the fire blast's flame cone (effect slot 0x274).
///
/// `spawnArg2.pointer` owns the counted effect work. Running frames advance its
/// age, radius by 64 world units and byte intensity down by 16 from 192;
/// initialization starts the radius at 256. Paused/hidden frames do nothing;
/// cancellation or intensity below 16 releases the work and task.
void dryfieldDilapidatedHouseFlameConeTask(Task* task);

/// Expands and fades a tilted flame ring (effect slot 0x275).
///
/// `spawnArg2.pointer` owns the counted effect work. Initialization composes
/// `spawnArg1.value` as a Z rotation (4096 units per turn) once. Running frames
/// draw a 256-unit-wide ring, expand its inner radius from 256 by 128 world
/// units, and fade intensity from 128 by eight. Age is not advanced. Paused/
/// hidden frames do nothing; cancellation or intensity below nine releases it.
void dryfieldDilapidatedHouseFlameRingTask(Task* task);

void func_dryfield_dilapidated_house_8017EB60(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H
