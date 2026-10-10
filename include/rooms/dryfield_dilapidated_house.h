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

/// The room-1 directory mapping logical views 1..21 to the same camera/image indices.
///
/// Borrows gameplay's 50-byte identity map; only the first 21 entries are used
/// by this room. The Dryfield map selects the directory at area 9, room 1.
/// This one-entry directory stays valid while the room overlay is loaded.
extern u8* gDryfieldDilapidatedHouseRoomViewMaps[1];

/// Room 1's logical view count for the dilapidated house, 21 entries.
///
/// The Dryfield map selects this one-entry array at area 9, indexed by room
/// minus one. It limits reverse searches of the room's identity view map;
/// the count is in elements and has no terminator. Keep the room overlay loaded.
extern ViewCount gDryfieldDilapidatedHouseRoomViewCounts[1];

/// Room 1's authored lights and minimum ambient levels for logical views 1..21.
///
/// The Dryfield map selects this one-entry room array at area 9. It borrows
/// mutable point/cone lights and a read-only ambient table from this overlay;
/// all must remain loaded through lighting updates and queries.
extern WorldCoordRoomLighting gDryfieldDilapidatedHouseRoomLighting[1];

extern DirectionWarpEntry D_dryfield_dilapidated_house_80186974[];

/// Cameras for mapped views 1..21 of the Dryfield dilapidated house.
///
/// The area-9 map directory borrows this array, indexed by mapped view minus
/// one. Rotation uses 12 fractional bits, translation is the negated world
/// origin in game units, and projection distances are pixels. There is no
/// terminator; records stay readable while the room overlay is loaded.
extern ViewCamera gDryfieldDilapidatedHouseViewCameras[21];

/// Sprite descriptors for mapped views 1..21 of the Dryfield dilapidated house.
///
/// The area-9 map directory borrows this array at mapped view minus one.
/// Each descriptor retains decoded image strips and has no clipping list.
/// Views 1, 10, 12 and 15..21 have no source sprites; their source slot retains
/// the batch-list address and is never dereferenced as sprites. All descriptors
/// and mutable source/batch lists stay valid while the room overlay is loaded.
extern SpriteView gDryfieldDilapidatedHouseSpriteViews[21];

/// Collision policies and footstep cues for the room's surface classes 0..7.
///
/// Every class blocks probes, allows weapon impacts and applies pushback.
/// Classes 1 and 2 have distinct footsteps; classes 0 and 3..7 share a silent
/// surface. The Dryfield map selects this directory at area 9. Records and
/// sound tables stay borrowed only while the room overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldDilapidatedHouseSurfacePropertiesByClass[8];

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

/// Flashes and expands No. 9's fire blast, spawning a cone and three tilted rings.
///
/// Effect slot 0x273 owns counted `EffectWork` in `spawnArg2.pointer` and a
/// coordinate body with a composed world transform. Initialization draws an
/// additive screen flash, replaces transient point-light slot 0 for four frames,
/// and chooses a sprite roll in 4096 units per turn. `scale` holds intensity
/// 192 in the 0..255 range, `angle` the sprite/disc radius factor, and `period`
/// the retained roll.
/// State 1 expands the radius from 896 by 64 per running frame, releasing the
/// work and task after the radius passes 1408. Paused/hidden updates restore age;
/// cancellation releases the work and attached rings. The cone is independent.
void dryfieldDilapidatedHouseFireBlastTask(Task* task);

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

/// Runs the dilapidated-house encounter's room message and event controller.
///
/// The Dryfield map spawns this bodyless room task. States are 0 setup, 1 run,
/// and 2 teardown; other indices are invalid. Setup registers the room receiver
/// and starts blackout control, plus head tracking when placed actor 1 exists.
/// The running state
/// advances the latched encounter when the introductory event releases control.
/// The room overlay and its state tables must remain loaded for the task's life.
void dryfieldDilapidatedHouseRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_DILAPIDATED_HOUSE_H
