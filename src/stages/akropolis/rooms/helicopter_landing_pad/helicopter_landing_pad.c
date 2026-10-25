#include "stages/akropolis/rooms/helicopter_landing_pad.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "common.h"

#include "helicopter_landing_pad_private.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/gameflag.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../../../shared/actor/actor_messages.h"

#define ROOM_PART declarations_helicopter_landing_pad
#include "assets/rooms/akropolis/helicopter_landing_pad/declarations.inc"

/// Frames one scripted run of the landing pad's lift lasts.
#define LIFT_TRAVEL_FRAMES 120

/// Magnitude of the lift's Y step in parent-coordinate units per update tick.
#define LIFT_TRAVEL_STEP 25

/// Work block of the landing pad's lift, the railed platform that carries the player up to the pad.
typedef struct {
    s32    stepX;      // World units added to the model's X translation each frame of travel
    s32    stepY;      // As `stepX` for Y; negative is upward
    s32    stepZ;      // As `stepX` for Z
    s32    field_C;    // Never accessed; role unproven
    MATRIX lightMtx;   // Light-direction matrix the model borrows, filled from the room's three flat lights
    MATRIX colorMtx;   // Light-color matrix the model borrows, filled with `lightMtx`
    s16    stepFrames; // Frames of travel left; the lift is at rest at 0
} _LiftWork;
STATIC_ASSERT_SIZEOF(_LiftWork, 0x54);

static void _akropolisHelicopterLandingPadLiftInitLighting(Task* task);
static void _akropolisHelicopterLandingPadLiftInit(Task* task);
static void _akropolisHelicopterLandingPadLiftUpdate(Task* task);

static s32 _akropolisHelicopterLandingPadLiftHandleRunRequest(Task* task, s32 messageId,
                                                              const AnimationPlayRequest* request, s32 unusedArg);

/// Lift messages for scripted travel and Euler placement, followed by the terminator.
static TaskMessageEntry _gAkropolisHelicopterLandingPadLiftMessages[3] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _akropolisHelicopterLandingPadLiftHandleRunRequest },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Three directional lights used to initialize the lift's task-owned matrices.
static GsF_LIGHT _gAkropolisHelicopterLandingPadLiftFlatLights[3] = {
    { 0, ONE, 0, 128, 128, 128 },
    { ONE, 0, 0, 128, 128, 128 },
    { 0, 0, ONE, 128, 128, 128 },
};

/// Lift visibility indexed directly by the saved 1-based logical view.
static s8 _gAkropolisHelicopterLandingPadLiftVisibleByView[36] = {
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

#define ROOM_PART _gAkropolisHelicopterLandingPadLiftLowerStopPlacement
#include "assets/rooms/akropolis/helicopter_landing_pad/events.inc"

#include "assets/rooms/akropolis/helicopter_landing_pad/akropolis_helicopter_landing_pad_model_0547C.inc"

/// Initializes the lift's model, lighting and message handling.
static void _akropolisHelicopterLandingPadLiftInit(Task* task)
{
    enum { LIFT_OT_OFFSET = 8 };

    TmdObject* liftModel = task->extra.tmd;
    _LiftWork* work;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work          = work;
    liftModel->otOffset = LIFT_OT_OFFSET;
    liftModel->flags   &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    _akropolisHelicopterLandingPadLiftInitLighting(task);
    task->msgTable      = _gAkropolisHelicopterLandingPadLiftMessages;
    task->killCountdown = 0;
    task->state         = task->state + 1;
}

/// Advances an active lift run and refreshes visibility for a ready camera view.
static void _akropolisHelicopterLandingPadLiftUpdate(Task* task)
{
    _LiftWork* work      = task->work;
    GfxCoord*  rootCoord = task->extra.tmd->coords;
    TmdObject* liftModel = task->extra.tmd;
    s16        remainingFrames;

    // Decrement before stepping so the tick reaching zero still moves the lift.
    remainingFrames = --work->stepFrames;
    if (remainingFrames >= 0) {
        rootCoord->coord.t[0]  += work->stepX;
        rootCoord->coord.t[1]  += work->stepY;
        rootCoord->coord.t[2]  += work->stepZ;
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        work->stepFrames = 0;
    }
    if (gGameSession->viewReady != 0) {
        if (_gAkropolisHelicopterLandingPadLiftVisibleByView[gGameSession->location.view] != 0) {
            liftModel->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            liftModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
}

/// Binds the lift model to its task-owned flat-light matrices.
static void _akropolisHelicopterLandingPadLiftInitLighting(Task* task)
{
    _LiftWork*       work      = task->work;
    TmdObject*       liftModel = task->extra.tmd;
    const GsF_LIGHT* flatLight;
    s32              lightIndex;

    liftModel->lightMtx = &work->lightMtx;
    liftModel->colorMtx = &work->colorMtx;
    for (lightIndex = 0, flatLight = _gAkropolisHelicopterLandingPadLiftFlatLights;
         lightIndex < (s32)ARRAY_SIZE(_gAkropolisHelicopterLandingPadLiftFlatLights); lightIndex++, flatLight++) {
        gfxSetFlatLight(lightIndex, flatLight, &work->lightMtx, &work->colorMtx);
    }
}

/// Resets the lift to a starting stop and arms fixed-duration vertical travel.
static inline void _akropolisHelicopterLandingPadLiftStartRun(Task* task, const ActorTransform* startPlacement, s32 stepY)
{
    _LiftWork* work = task->work;

    actorMsgPlaceEuler(task, 0, startPlacement, 0);
    work->stepFrames = LIFT_TRAVEL_FRAMES;
    work->stepX      = 0;
    work->stepY      = stepY;
    work->stepZ      = 0;
}

/// Handles the lift's scripted run selector carried by a play-animation request.
static s32 _akropolisHelicopterLandingPadLiftHandleRunRequest(Task* task, s32 messageId,
                                                              const AnimationPlayRequest* request, s32 unusedArg)
{
    enum {
        RAISE      = 0,
        LOWER      = 1,
        PARK_LOWER = 2,
    };

    _LiftWork* work = task->work;

    switch (request->animationId) {
        case RAISE:
            _akropolisHelicopterLandingPadLiftStartRun(task, &_gAkropolisHelicopterLandingPadLiftLowerStopPlacement,
                                                       -LIFT_TRAVEL_STEP);
            break;
        case LOWER:
            _akropolisHelicopterLandingPadLiftStartRun(task, &_gAkropolisHelicopterLandingPadLiftPadStopPlacement,
                                                       LIFT_TRAVEL_STEP);
            break;
        case PARK_LOWER:
            actorMsgPlaceEuler(task, 0, &_gAkropolisHelicopterLandingPadLiftLowerStopPlacement, 0);
            work->stepFrames = 0;
            break;
    }
    return 0;
}

#include "../../../../shared/actor/actor_messages_place_euler.inc.c"

/// Landing-pad lift handlers indexed by task state 0..2.
static const TaskFuncTable3 _gAkropolisHelicopterLandingPadLiftStates = {
    .funcs = { _akropolisHelicopterLandingPadLiftInit, _akropolisHelicopterLandingPadLiftUpdate, enemyTaskExit },
};

void akropolisHelicopterLandingPadLiftTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gAkropolisHelicopterLandingPadLiftStates;
    stateHandlers.funcs[task->state](task);
}

void akropolisHelicopterLandingPadUpdateEncounterPhase(Task* unusedTask)
{
    enum {
        STARTED  = 1,
        FINISHED = 2,
        ACTOR    = 0,
        ARM_VIEW = 5,
    };
    s32 phase = gAkropolisHelicopterLandingPadEncounterPhase;

    // Wait for the encounter actor and the presentation holds before starting the ending scripts.
    if (phase == STARTED) {
        if (taskMessageDispatch(sceneFindPlacedActor(ACTOR), ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0) {
            if ((gAttachmentState.mode != phase) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                gAkropolisHelicopterLandingPadEncounterPhase = FINISHED;
                evsStartScriptWithSkip(gAkropolisHelicopterLandingPadEncounterEndingScript, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                       gAkropolisHelicopterLandingPadEncounterEndingSkipScript);
                gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, EVENT_FLAG_OBJECTIVE_AKROPOLIS_NO9_DEFEATED);
            }
        }
    }
    if (gGameSession->location.view == ARM_VIEW) {
        gAkropolisHelicopterLandingPadEncounterReady = 1;
    }
    if (gGameSession->eventState == 0) {
        gAkropolisHelicopterLandingPadPostEncounterPlacementEnabled = 0;
    }
}
