#ifndef GAMEPLAY_PRIVATE_DIRECTION_INPUT_H
#define GAMEPLAY_PRIVATE_DIRECTION_INPUT_H

#include "types.h"

#include "gameplay/direction.h"

#include "main/gameflow.h"

// Direction input state, action dispatch and warp handling.

/// Direction and warp state shared within gameplay.
extern s16 D_80114CD0;

extern u16 Gp_DirFlags;

extern u16 D_80114CD4;

extern u16 Gp_DirPhase;

extern u8 Gp_DirByte;

extern u8 Gp_DirNibble;

extern u8 Gp_DirAlt;

extern u8 Gp_DirAltNibble;

/// Discards the primary action parameters and the secondary trigger hit.
///
/// Clears both control words and their two byte parameters. The active-action
/// marker, phase, trigger-control change latch and session busy flag remain intact;
/// callers decide when to release activity. Each gameplay TU uses an inline copy.
static inline void _directionClearTriggerParameters(void)
{
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
}

extern u8 D_80114CDC;

extern u8 D_80114CDD;

extern u8 D_80114CDE;

extern s16 D_80114CE0;

extern RoomEventMsg Gp_WarpLoc;

extern u16 Gp_DirFadeLevel;

/// Draws and advances the active warp departure shade by one frame.
///
/// Zero disables the fade. Active shades start at 30 and saturate at 255;
/// drawing uses the current low byte before the increment. The signed local
/// preserves this ramp's halfword load. Each gameplay TU uses an inline copy.
static inline void _directionStepDepartureFade(void)
{
    enum { DIRECTION_DEPARTURE_FADE_STEP = 30,
           DIRECTION_DEPARTURE_FADE_MAX  = 255 };
    u8  fadeShade;
    s16 activeShade;

    activeShade = (s16)Gp_DirFadeLevel;
    if (activeShade != 0) {
        fadeShade = (u8)Gp_DirFadeLevel;
        fadeDrawOverlay(fadeShade, fadeShade, fadeShade, GPU_BLEND_SUBTRACT);
        Gp_DirFadeLevel += DIRECTION_DEPARTURE_FADE_STEP;
        if ((s16)Gp_DirFadeLevel >= DIRECTION_DEPARTURE_FADE_MAX + 1) {
            Gp_DirFadeLevel = DIRECTION_DEPARTURE_FADE_MAX;
        }
    }
}

/// Dual area-id bitmask (1–32 / 33–64), rebuilt from the area bit-2 flags.
extern s32 Gp_AreaIdBits[2];

/// Per-stage signed counts, indexed by `GameSession.location.loc.stage - 1`.
/// `menuMapRebuildMarkedAreaBits` loops area ids `1..count` when the stage is 1–5.
extern s8 Gp_AreaIdCounts[];

/// Queries the room for a warp and starts the appropriate departure turn.
///
/// Requires live room/player tasks, stage 1..5, a populated current area and a
/// valid departure endpoint in the trigger's high nibble (1..15). The low
/// nibble requests the arrival slot; the first trigger byte requests the area.
/// Room dispatch borrows and resolves the reusable request synchronously.
/// Replies narrow to s16: 1 starts departure, 0 stays in the area, and 2 executes
/// the room's replacement action immediately. A stay reply during combat also
/// executes immediately; otherwise it turns before execution. Other replies
/// retain the query phase. Active events discard both trigger tuples instead.
/// Facing uses 4096 units per turn, reversing the endpoint's arrival yaw unless
/// its sentinel retains the player's yaw or selects Dryfield's driveway target.
void directionQueryWarp(void);

/// Steps the departure fade while awaiting the player's scripted turn.
///
/// Requires the live player task and the latched warp query result. When
/// scripted motion ends, a nonzero result pauses scene actors and the warp
/// advances to its one-frame hold phase; a zero result also advances.
void directionAwaitWarpTurn(void);

/// Executes the queried warp at the room and starts its selected departure sound.
///
/// Requires the latched trigger, query result and live room/player tasks after
/// the departure turn and hold. Rebuilds the request from the trigger and copies
/// the current endpoint, with the same table bounds as `directionQueryWarp`.
/// Dispatch resolves the request in place; its return value is ignored. A zero
/// query result ends scripted control and clears primary action parameters.
/// Otherwise advances to the sound wait, leaving the resolved destination for
/// the final phase to save. Sound playback is suppressed for a dead player.
void directionResolveWarp(void);

/// Steps the departure fade until the selected warp sound is no longer active.
///
/// Uses the latched sound id; zero skips the wait. Completion advances to the
/// destination-save phase. The sound query follows script-instance lifetime,
/// including release, rather than testing only currently sounding voices.
void directionAwaitWarpSound(void);

/// Accepts a world-trigger request or advances the currently latched direction action.
///
/// Called once per direction-task update with live session, player and collision
/// state. While no action is active and attachments are idle, decrements the
/// manual rearm delay and reads a trigger hit. Automatic actions bypass that
/// delay but require no pending display mode; manual actions require an
/// interaction press and zero delay. Both reject a triangle press and combat's
/// end delay, and OUTSIDE_BATTLE rejects the engaged phase. View/display changes
/// and accepted manual actions seed ten eligible updates of rearm delay.
///
/// A latched selector must be 0..DIRECTION_ACTION_COUNT-1 or CANCEL; dispatch
/// has no other range check. Active handlers run through a stack copy of the
/// action table even while attachment or activation gates are closed. Cancellation
/// releases activity, and inactive updates discard primary/secondary parameters.
/// Records the trigger control and battle phase for the next update.
void directionUpdateAction(void);

/// Consumes a CAP interaction request, dispatching it only while events and CAP are idle.
///
/// A room-message sentinel sends the command byte synchronously to the live
/// room's `ROOM_MESSAGE_COMMAND` handler; otherwise the bytes select a CAP
/// command and its event flags. CAP resources must remain live through playback.
/// Busy events or playback discard the request. After dispatch or discard,
/// clears the request and secondary hit; a newly entered trigger keeps the
/// session's busy flag until the next direction tick.
void directionDispatchCapInteraction(void);

/// Consumes a trigger callback request, dispatching it only while event state is idle.
///
/// Bits 8..14 of the latched control word must select callback 0 (fountain
/// climb) or 1 (helipad exit); there is no bounds check. The selected room
/// overlay and its runtime must be live. Forwards the two trigger bytes as
/// signed words, then clears action activity and primary/secondary parameters.
/// An active event discards the request. Phase and session busy state are retained.
void directionDispatchCallbackAction(void);

/// Updates the stair surface and waits for the climb to finish or encounter a warp.
///
/// Runs once per frame after the facing action starts the player's stair motion.
/// Requires a live scripted player and the original latched trigger parameters.
/// Parameter0's low nibble is the positive step count; bits 4..6 select a surface
/// class, or flight 0..4 when bit 7 is set. Control bit 8 selects descent.
/// Indexed rows must stay loaded and contain entries 0 through the step count;
/// the player countdown must remain in 0..stepCount. Completion releases
/// scripted control and clears the action. A warp hit while motion is pending
/// pauses scene actors and advances to `directionCommitStairWarp` instead.
void directionAwaitStairClimb(void);

/// Resolves and commits the warp latched during a stair climb, then ends the action.
///
/// Requires a live room task and a secondary WARP hit in the active stage.
/// Its first byte requests the area; the second byte's low nibble requests the
/// arrival slot. The room resolves the reusable request in place with effects
/// enabled, retaining the request's existing flag id. Only its area, arrival
/// and room selectors are copied to the live
/// save before the area-change task is spawned; the dispatch result and task
/// allocation failure do not prevent request cleanup.
void directionCommitStairWarp(void);

#endif // GAMEPLAY_PRIVATE_DIRECTION_INPUT_H
