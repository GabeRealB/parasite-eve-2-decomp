#ifndef INCLUDE_ROOMS_SHELTER_1F_HELIPORT_H
#define INCLUDE_ROOMS_SHELTER_1F_HELIPORT_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_1f_heliport_80181188;

extern AreaVariant D_shelter_1f_heliport_80182BF4[13];

// shelter_1f_heliport
extern WorldCollisionRoomResources D_shelter_1f_heliport_801812D0[];

/// Number of logical views and corresponding camera/sprite records in the heliport.
enum { SHELTER_1F_HELIPORT_VIEW_COUNT = 12 };

/// Lighting for the heliport's single room, indexed by room ID minus one.
///
/// Borrows mutable point/cone lights and twelve ambient minima from this overlay;
/// keep the room loaded while gameplay composes or samples its lighting.
extern WorldCoordRoomLighting gShelter1fHeliportRoomLighting[1];

/// Logical-view map directory for the heliport's single room.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..12 select matching
/// camera/image/sprite indices. Gameplay and this room must remain loaded.
extern u8* gShelter1fHeliportViewMaps[1];

/// Reverse-lookup limit for the heliport's single room: twelve logical views.
///
/// Indexed by room ID minus one; the count limits searches of the identity map,
/// independently of that gameplay-owned map's capacity.
extern ViewCount gShelter1fHeliportViewCounts[1];

extern DirectionWarpEntry D_shelter_1f_heliport_801812F0[];

/// Cameras selected by the heliport's mapped view index minus one (views 1..12).
///
/// Rotations use `ONE` per unit, translations are negated world-space camera
/// origins in game units, and projection distances are pixels. Queued camera
/// application borrows a record until dispatch; keep this overlay loaded.
extern ViewCamera gShelter1fHeliportViewCameras[SHELTER_1F_HELIPORT_VIEW_COUNT];

/// Sprite resources selected by the heliport's mapped view index minus one.
///
/// All twelve views retain decoded image strips; views 2 and 11 additionally
/// draw 21 and 13 depth-sorted sprites. No view supplies clipping commands.
/// Mutable sources and terminated batch lists remain owned by this overlay.
extern SpriteView gShelter1fHeliportSpriteViews[SHELTER_1F_HELIPORT_VIEW_COUNT];

/// Collision policies indexed by the heliport grid's surface class (0..7).
///
/// Every class blocks probes, accepts weapon impacts and applies pushback.
/// Only class 1 supplies footstep cues; the other classes share a silent record.
/// The directory and borrowed properties remain valid while this room is loaded.
extern WorldCollisionSurfaceProperties* gShelter1fHeliportSurfacePropertiesByClass[8];

/// Runs the heliport room controller's setup, per-frame visibility or teardown state.
///
/// `task->state` must be 0..2. The map's room descriptor starts it at zero;
/// setup installs/publishes the room task and advances to state 1. That state
/// maintains placed-actor visibility; state 2 tears down the task. The room
/// overlay and its imported actor callbacks must remain loaded throughout.
void shelter1fHeliportRoomTask(Task* task);

/// Rebuilds the heliport's reserved companion obstacle from the current actor pose.
///
/// Uses the companion model when present, otherwise the required live player
/// model. Keeps the obstacle at that pose only while the companion exists and
/// the soldier request state is 1; otherwise offsets it by +10000 room Y units.
/// Requires loaded source geometry and a writable room grid with its reserved
/// obstacle entries. Borrows both during the call without rebuilding cell lists.
/// `unusedEventArg` is ignored event-script data; keep this room loaded when called.
void shelter1fHeliportUpdateCompanionObstacle(s32 unusedEventArg);

/// No-op per-frame callback for the heliport's room-effect slot.
///
/// Gameplay effect bank 6, slot 0x166 passes a live `unusedTask`, which is
/// ignored. The task and its resources remain live for external teardown.
/// The heliport overlay must remain loaded while the task can invoke this callback.
void shelter1fHeliportNoOpEffectTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_1F_HELIPORT_H
