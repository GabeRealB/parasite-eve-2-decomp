#ifndef SRC_ROOMS_DRYFIELD_TOILET_DRYFIELD_TOILET_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_TOILET_DRYFIELD_TOILET_PRIVATE_H

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"

extern AnimationSet gDryfieldToiletAnimation03054;

extern AnimationSet gDryfieldToiletAnimation035A4;

/// Runs the once-only woman-to-Stranger encounter and enters the room's battle.
///
/// Selects the encounter stream, stages audio, plays player clips 48/49 and
/// replaces the woman with her temporary Lesser Stranger. Timed views and
/// playback precede release of that actor, player placement, the Odd Stranger
/// ambush and removal of the event collision face. Ends in view 3.
/// Commands include END; keep this overlay, its payloads and its callbacks
/// loaded until the script finishes. Start with the paired skip script.
extern EvsCommand gDryfieldToiletEncounterScript[31];

/// Cancels encounter playback under a fade and establishes its final battle setup.
///
/// Hides/releases the woman, releases her temporary Stranger if present,
/// restores player placement with equipped-bank clip 9, arms the Odd Stranger
/// ambush, shifts event collision and reloads view 3. Cleans up the scene and
/// returns the fade before engaging battle and resuming player control.
/// Includes END; requires the normal script's selected scene and the room's
/// actors, payloads and callbacks to remain loaded through completion.
extern EvsCommand gDryfieldToiletEncounterSkipScript[20];

/// Mutable collision grid for the toilet room, with seventeen faces in two XZ cells.
///
/// Room-coordinate vertices and Q12 normals remain borrowed while active.
/// Face 0, normal 0 and vertices 0..3 are reserved for the encounter obstacle;
/// room callbacks restore them from the one-face template or shift them aside.
/// The remaining mesh and both cell lists remain intact. Keep the overlay and
/// its pools loaded while the grid is active; room setup binds the view coordinate.
extern WorldCollisionGrid gDryfieldToiletRoomCollisionGrid;

/// Event-script choices for the room grid's reserved collision face.
enum {
    DRYFIELD_TOILET_EVENT_COLLISION_RESTORE    = 0,
    DRYFIELD_TOILET_EVENT_COLLISION_MOVE_ASIDE = 1,
};

/// Restores or shifts the room grid's reserved event collision face.
///
/// Zero restores XYZ of normal 0 and vertices 0..3, plus the complete face 0
/// record, from the template. Nonzero shifts those vertices 2000 whole
/// room-coordinate units toward negative X from their current positions;
/// repeated nonzero calls accumulate. Vector fourth components, other faces
/// and cell lists are retained. The live room grid and template must be loaded.
void dryfieldToiletMoveEventCollision(s32 moveAside);

/// Stages deferred audio start for the event script's already selected scene.
///
/// The selected scene and its prepared buffers must remain live until the
/// resident CD dispatcher commits and consumes the deferred request.
void dryfieldToiletStageSceneAudioStart(void);

/// Enqueues playback of the event script's selected scene/audio session.
///
/// The selected scene and prepared playback buffers must remain live through
/// consumption; the resident CD request ring must have capacity.
void dryfieldToiletStartScenePlayback(void);

/// Finishes the selected scene stream and restores its saved random state.
///
/// Requires a successful scene selection. Buffer/task owners retain teardown;
/// pending resident CD requests are not cancelled by this callback.
void dryfieldToiletFinishScene(void);

/// Cancels the selected scene when the event takes its skip path.
///
/// Discards deferred audio start, requests resident CD cancellation and finishes
/// the stream immediately. Requires a successful scene selection; owners keep
/// buffers and tasks live until cancellation permits teardown.
void dryfieldToiletCancelScene(void);

/// Engages an idle scene battle after either event-script path.
///
/// Other battle phases and fields remain intact. `unusedArg` is forwarded for
/// script callback compatibility and has no effect on the battle.
void dryfieldToiletEngageBattle(s32 unusedArg);

#endif // SRC_ROOMS_DRYFIELD_TOILET_DRYFIELD_TOILET_PRIVATE_H
