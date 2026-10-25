#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_HELICOPTER_LANDING_PAD_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_HELICOPTER_LANDING_PAD_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Two-step vibration cue for the landing-pad boss's lethal strike.
extern PadScriptCmd gAkropolisHelicopterLandingPadLethalStrikeVibrationCommands[2];

/// Variable-motor segment for the lethal-strike cue: intensity 120 toward 70 over 20 frames.
extern PadScriptVibrationSegment gAkropolisHelicopterLandingPadLethalStrikeVibrationSegment;

/// Actor layout variants of the landing pad.
extern AreaVariant gAkropolisHelicopterLandingPadAreaVariants[13];

extern TmdSource gAkropolisHelicopterLandingPadAkropolisHelicopterLandingPadModel0547C;

extern TmdSource gAkropolisHelicopterLandingPadAmmunitionPickupModel;

// acropolis_helicopter_landing_pad
/// Collision resources for the landing pad's single room of the landing pad.
extern WorldCollisionRoomResources gAkropolisHelicopterLandingPadCollisionRoomResources[1];

/// Logical-view map directory for the landing pad's single room in area 16.
extern u8* gAkropolisHelicopterLandingPadViewMaps[1];

/// Reverse-view-search limits for the landing pad's single room in area 16.
extern ViewCount gAkropolisHelicopterLandingPadViewCounts[1];

/// Lighting descriptors for the landing pad's single room in area 16.
extern WorldCoordRoomLighting gAkropolisHelicopterLandingPadRoomLightingTable[1];

/// The landing pad's three arrival endpoints, indexed by `warp - 1`.
extern DirectionWarpEntry gAkropolisHelicopterLandingPadWarps[3];

/// Grid-local Q12 normals, with four leading entries reserved for the encounter actor.
extern SVECTOR gAkropolisHelicopterLandingPadCollisionGridNormals[15];

/// Grid-local signed game-coordinate vertices, including sixteen encounter vertices.
extern SVECTOR gAkropolisHelicopterLandingPadCollisionGridVerts[92];

/// Thirty-seven indexed room faces with four reserved leading encounter faces.
extern WorldCollisionGridFace gAkropolisHelicopterLandingPadCollisionGridFaces[37];

/// Sprite sources, drawing batches and clipping lists for the landing pad's 27 views.
extern SpriteView gAkropolisHelicopterLandingPadSpriteViews[27];

/// Authored camera transforms and projection distances for the landing pad's 27 views.
extern ViewCamera gAkropolisHelicopterLandingPadViewCameras[27];

/// The landing pad's collision and footstep policies indexed by surface class (0..7).
extern WorldCollisionSurfaceProperties* gAkropolisHelicopterLandingPadSurfacePropertiesByClass[8];

/// Draws one blue spark on the coordinate's lower local-Y side.
void akropolisHelicopterLandingPadDrawLowerSparkLine(GfxCoord* coord);

/// Pulses the twelve red perimeter lights and selects the view's ground-shadow shade.
void akropolisHelicopterLandingPadPerimeterLightsTask(Task* task);

/// Animates one drifting helipad ember from the six-cell 32-pixel sprite row.
void akropolisHelicopterLandingPadEmberTask(Task* task);

/// Animates one drifting helipad flare from the six-cell 40-pixel sprite row.
void akropolisHelicopterLandingPadLensFlareTask(Task* task);

/// Emits flares and embers at the shot helipad blast source under actor control.
void akropolisHelicopterLandingPadFlareEmitterTask(Task* task);

/// Queues the landing pad's fire-escape exit from a direction action.
void akropolisHelicopterLandingPadStartExit(s32 unusedArgument, s32 unusedActionId);

/// Follows a damaged helipad light and drives its spark lines, bursts and transient lights.
void akropolisHelicopterLandingPadDamagedLightSparksTask(Task* task);

/// Turns the player, descends three steps and reloads the resolved fire-escape room.
void akropolisHelicopterLandingPadExitTask(Task* task);

/// Runs the landing pad lift's initialization, travel and teardown states.
void akropolisHelicopterLandingPadLiftTask(Task* task);

/// Runs the landing pad room's initialization, encounter-phase update and teardown states.
void akropolisHelicopterLandingPadRoomTask(Task* task);

/// Selects the placed pickup model's draw and buffer policy from its saved object state.
void akropolisHelicopterLandingPadPickupModelTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_HELICOPTER_LANDING_PAD_H
