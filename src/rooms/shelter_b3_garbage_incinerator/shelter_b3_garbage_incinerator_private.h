#ifndef SRC_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_SHELTER_B3_GARBAGE_INCINERATOR_PRIVATE_H
#define SRC_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_SHELTER_B3_GARBAGE_INCINERATOR_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/cap.h"
#include "gameplay/collision.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

#include "main/task_types.h"
#include "main/text.h"

#include "overlay.h"

/// Bodyless priority-192 recipe running the exit encounter after lift descent.
///
/// Spawn only index 0 with unused zero payloads/metadata when the exit switch
/// is accepted. Spawning reads the recipe synchronously; the deferred encounter
/// borrows this room's actors, animation, CAP and collision resources until reload.
extern TaskDesc gShelterB3GarbageIncineratorExitEncounterTaskDesc;

/// Priority-192 TMD recipe for the incinerator lift's descent and actor-carry move.
///
/// Spawn only index 0 with unused zero payloads. The new task owns its model
/// and work; geometry remains borrowed from this overlay until teardown.
/// Automatic model-buffer allocation/recovery is disabled at spawn; the lift's
/// setup then clears that runtime flag and enables later missing-buffer recovery.
extern TaskDesc gShelterB3GarbageIncineratorLiftTaskDesc;

/// Collision mesh and 5 by 7 XZ cell index shared by incinerator rooms 1..7.
///
/// Cells are 4000 game units wide, with origin (0, -26000) on XZ. Room setup
/// binds the descriptor to the current view coordinate. The overlay owns its
/// writable pools: 29 normals, 84 vertices and 64 faces; lift/exit updates use
/// the first eight normals/faces and first 32 vertices without changing cells.
/// Keep the descriptor and its pools loaded while this grid is active.
extern WorldCollisionGrid gShelterB3GarbageIncineratorCollisionGrid;

/// Model-light collection shared by incinerator rooms 1..3.
///
/// Two directional lights and 25 point lights, with no cone lights.
/// Borrows writable arrays owned by this overlay; gameplay updates their
/// transforms and attenuation. The collection must not outlive the overlay.
extern WorldCoordRoomLights gShelterB3GarbageIncineratorRooms1To3Lights;

/// Model-light collection shared by incinerator rooms 4..7.
///
/// Two directional lights and 21 point lights, with no cone lights.
/// Borrows writable arrays owned by this overlay; gameplay updates their
/// transforms and attenuation. The collection must not outlive the overlay.
extern WorldCoordRoomLights gShelterB3GarbageIncineratorRooms4To7Lights;

/// Twenty-two directed view boundaries for stationary incinerator rooms 1 and 4.
///
/// The final record carries LAST. Room binding mutates links, coordinate and
/// enable/hit state; keep this room-owned array loaded while linked.
extern WorldCollisionTrigger gShelterB3GarbageIncineratorWaitingViewTriggers[22];

/// Twenty directed view boundaries shared by moving/landed rooms 2, 3, 5 and 6.
///
/// The final record carries LAST. Room binding mutates links, coordinate and
/// enable/hit state; keep this room-owned array loaded while linked.
extern WorldCollisionTrigger gShelterB3GarbageIncineratorMovingAndLandedViewTriggers[20];

/// Seventeen directed view boundaries for incinerator room 7.
///
/// The final record carries LAST. Room binding mutates links, coordinate and
/// enable/hit state; keep this room-owned array loaded while linked.
extern WorldCollisionTrigger gShelterB3GarbageIncineratorRoom7ViewTriggers[17];

/// Six action quads shared by incinerator rooms 1..7.
///
/// Two warps, two automatic room actions and two facing-gated CAP actions;
/// the final record carries LAST. Room binding mutates links, coordinate and
/// enable/hit state. These borrowed records must stay loaded while linked.
extern WorldCollisionTrigger gShelterB3GarbageIncineratorActionTriggers[6];

/// One room-space sight-blocking quad shared by incinerator rooms 1..7.
///
/// The sole record carries LAST. Room setup mutates links and enables it;
/// the array belongs to the room overlay and must stay loaded while linked.
/// The low flag bit is retained with its meaning unproven.
extern WorldCollisionOccluder gShelterB3GarbageIncineratorSightOccluders[1];

/// Copied destination retained during the deferred control-room departure choice.
///
/// The room-event reply replaces the full record before spawning the choice
/// task. A committed departure copies area, warp and room into the live save.
/// This overlay owns the value; no caller's request/reply pointer is retained.
extern RoomEventMsg gShelterB3GarbageIncineratorControlRoomExitDestination;

/// Last successfully initialized incinerator lift task.
///
/// Initialization publishes this borrowed handle; there is no current reader.
/// Teardown does not clear it, so a non-NULL value does not establish a live task.
extern Task* gShelterB3GarbageIncineratorLiftTask;

/// Borrowed context of this renderer's active wave task.
///
/// Set on the seed tick and left unchanged at teardown. Only one wave may use
/// this storage; the context must stay live through its final drawing tick.
extern ScreenWaveCtx* gScreenWaveContext;

/// Borrowed controller for the incinerator's scripted player burn scene.
///
/// Initialization publishes the task and its allocated work. Blaze script
/// callbacks require that controller/work and their child tasks to be live.
/// Teardown does not clear this handle; neither the handle nor a non-NULL value
/// owns the task or extends its lifetime beyond the active burn scene.
extern Task* gShelterB3GarbageIncineratorBlazeControllerTask;

extern CapCommandRef* CapCaption_Data_8015E650;

extern TextGlyphCell* CapCaption_Data_8015E654;

extern CapSequenceRecord* CapCaption_Data_8015E658;

extern s16 CapCaption_Data_8015E65C;

extern s16 CapCaption_Data_8015E65E;

extern s16 CapCaption_Data_8015E660;

extern s16 CapCaption_Data_8015E662;

extern s16 CapCaption_Data_8015E664;

extern s16 CapCaption_Data_8015E666;

extern u16 CapCaption_Data_8015E668;
extern u16 CapCaption_Data_8015E66A;

extern u8 CapCaption_Data_8015E66C[4];

/// Column sine oscillators for horizontal screen displacement.
///
/// The per-frame renderer uses entries 0..10; the prebuilt-grid renderer uses
/// 0..8. Any remaining declared capacity is unaccessed; its role is unproven.
extern ScreenWaveGridOscillator gScreenWaveColumnWaves[10];

/// Thirty active row sine oscillators for vertical screen displacement.
///
/// Seeded at wave startup and advanced on drawing updates. Any declared
/// entries beyond 29 are unaccessed; their role is unproven.
extern ScreenWaveGridOscillator gScreenWaveRowWaves[30];

/// Double-buffered 8 by 30 meshes of textured quads, one mesh per frame
/// buffer. `_screenWaveGridTask` builds them once and moves their corners.
extern POLY_FT4 gScreenWaveGrid[2][30][8];

/// Selects the loaded CAP data resource and font texture-page origin for this room.
///
/// `texturePageX` counts VRAM words and `texturePageY` rows. `dataResourceIndex`
/// is a zero-based ordinal among data resources, excluding image/empty slots.
/// The bundle load must be complete and its writable CAP storage and textures
/// must remain live during caption use. Selection relocates storage in place
/// without I/O or allocation. Page coordinates are stored even when a missing
/// ordinal or invalid CAP magic leaves the previous tables selected.
void shelterB3GarbageIncineratorSelectCaptionResource(s16 texturePageX, s16 texturePageY, s16 dataResourceIndex);

/// Installs the six collision walls for the lift's first rest pose.
///
/// Requires this room's active writable grid: vertices 0..23 and faces/normals
/// 0..5, plus vertices 24..31 and faces/normals 6..7 for variant 2. Uses the
/// low lift walls' XZ layout at grid-local Y=1000..2000, in game units. Surface
/// class 1 passes probes, ignores weapon impacts and applies pushback; unit
/// normals use 4096. Retains cell lists and the room's ownership of storage.
void shelterB3GarbageIncineratorSetLiftArrivalCollisionWalls(void);

#endif // SRC_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_SHELTER_B3_GARBAGE_INCINERATOR_PRIVATE_H
