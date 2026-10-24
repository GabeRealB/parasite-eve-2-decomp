#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_CAFETERIA_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_CAFETERIA_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Spawn recipes for the cafeteria's battle, follow-up scene and Stranger settling.
extern TaskDesc gAkropolisCafeteriaSceneTasks[4];

extern TmdSource gAkropolisCafeteriaWanderingCreatureModel;

/// Cafeteria actor layouts indexed directly by saved placement variant 0..10.
extern AreaVariant gAkropolisCafeteriaAreaVariants[11];

/// The loose props' models, which main's task bank 7 attaches (slots 160..163).
extern TmdSource gAkropolisCafeteriaLooseProp1Model;

extern TmdSource gAkropolisCafeteriaLooseProp2Model;

extern TmdSource gAkropolisCafeteriaLooseProp3Model;

extern TmdSource gAkropolisCafeteriaLooseProp4Model;

/// The pickups' models, which the stage overlay's object table attaches.
extern TmdSource gAkropolisCafeteriaStimPickupModel;

extern TmdSource gAkropolisCafeteriaMendelJournalModel;

/// Collision resources indexed by cafeteria room minus one (rooms 1..4).
extern WorldCollisionRoomResources gAkropolisCafeteriaCollisionRooms[4];

/// Logical-view maps indexed by cafeteria room minus one (rooms 1..4).
extern u8* gAkropolisCafeteriaViewMaps[4];

/// Reverse-search lengths for cafeteria rooms 1..4, each 24 logical views.
extern ViewCount gAkropolisCafeteriaViewCounts[4];

/// Lighting resources indexed by cafeteria room minus one (rooms 1..4).
extern WorldCoordRoomLighting gAkropolisCafeteriaRoomLightingTable[4];

/// Cafeteria arrival/departure endpoints indexed by warp minus one (1..3).
extern DirectionWarpEntry gAkropolisCafeteriaWarps[3];

/// Number of cafeteria camera/image/sprite slots selected by mapped view indices.
enum { AKROPOLIS_CAFETERIA_MAPPED_VIEW_COUNT = 24 };

/// Cafeteria background sprite resources indexed by mapped view minus one (1..24).
extern SpriteView gAkropolisCafeteriaSpriteViews[AKROPOLIS_CAFETERIA_MAPPED_VIEW_COUNT];

/// Cafeteria cameras indexed by mapped view minus one (1..24).
extern ViewCamera gAkropolisCafeteriaViewCameras[AKROPOLIS_CAFETERIA_MAPPED_VIEW_COUNT];

/// Cafeteria collision and footstep properties indexed by surface class 0..7.
extern WorldCollisionSurfaceProperties* gAkropolisCafeteriaSurfacePropertiesByClass[8];

/// Installs the cafeteria's room-effect messages and five wandering model effects.
void akropolisCafeteriaInitRoomEffectsTask(Task* task);

/// Moves the cafeteria's model effect through random turns, short runs and departure.
void akropolisCafeteriaModelWanderTask(Task* task);

/// Emits drifting cafeteria puffs while the puff gate is enabled in session view 9.
void akropolisCafeteriaPuffEmitterTask(Task* task);

/// Runs the cafeteria's charging pink flash, peak screen tint and fading star.
void akropolisCafeteriaRoomVisualEffectsFlashTask(Task* task);

/// Runs the cafeteria's twin trails from two offsets on the effect's parent.
void akropolisCafeteriaRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the cafeteria's impact flash followed by smoke or orange rings and sparks.
void akropolisCafeteriaRoomVisualEffectsSparkBurstTask(Task* task);

/// Animates the cafeteria's ten-cell drifting puff billboard.
void akropolisCafeteriaPuffTask(Task* task);

/// Runs one state of a cafeteria loose prop that sinks, hops and slides on contact.
void akropolisCafeteriaLoosePropTask(Task* task);

/// Dispatches one state of the cafeteria room task while its overlay is loaded.
void akropolisCafeteriaRoomTask(Task* task);

/// Lights and depth-orders the placed Mendel journal until it is collected.
void akropolisCafeteriaMendelPickupTask(Task* task);

/// Draws the placed Stim pickup in mapped view 9 and retires it after collection.
void akropolisCafeteriaStimPickupTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_CAFETERIA_H
