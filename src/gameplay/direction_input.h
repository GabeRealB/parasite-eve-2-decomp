#ifndef GAMEPLAY_PRIVATE_DIRECTION_INPUT_H
#define GAMEPLAY_PRIVATE_DIRECTION_INPUT_H

#include "types.h"

#include "gameplay/direction.h"

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

/// Dual area-id bitmask (1–32 / 33–64), rebuilt from the area bit-2 flags.
extern s32 Gp_AreaIdBits[2];

/// Per-stage signed counts, indexed by `GameSession.location.loc.stage - 1`.
/// `Gp_RebuildAreaIdBits` loops area ids `1..count` when the stage is 1–5.
extern s8 Gp_AreaIdCounts[];

void Gp_SetupDirWarp(void);

void Gp_FadeDirWaitMsg(void);

void Gp_CommitWarp(void);

void Gp_WarpPhase4(void);

void func_800AD6BC(void);

/// Consumes a CAP interaction request, dispatching it only while events and CAP are idle.
///
/// A room-message sentinel sends the command byte synchronously to the live
/// room's `ROOM_MESSAGE_COMMAND` handler; otherwise the bytes select a CAP
/// command and its event flags. CAP resources must remain live through playback.
/// Busy events or playback discard the request. After dispatch or discard,
/// clears the request and secondary hit; a newly entered trigger keeps the
/// session's busy flag until the next direction tick.
void directionDispatchCapInteraction(void);

void Gp_RunDirAction(void);

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
