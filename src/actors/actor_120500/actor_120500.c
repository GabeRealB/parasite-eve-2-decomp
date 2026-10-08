#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/gameflag.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

static void _screenFadeInTask(Task* task);
/// Selects this translation unit's private fade-in callback.
#define SCREEN_FADE_IN_TASK _screenFadeInTask
#include "../../shared/screen_fade.h"
#include "../../shared/actor_messages.h"

static void _screenFadeOutTask(Task* task);

/// Requests the event script posts in `_Actor120500Work::playerRequest`.
///
/// Any other nonzero code is cleared without effect.
enum {
    ACTOR_120500_PLAYER_REQUEST_NONE             = 0,
    ACTOR_120500_PLAYER_REQUEST_FIRST_ANIMATION  = 2, // Overrides ambient RGB and starts the first of the scene's player animations, then holds the player at the first placement every tick until another request replaces this one
    ACTOR_120500_PLAYER_REQUEST_SECOND_ANIMATION = 3, // Fades in from black, disables the ambient RGB override, shows the player at the second placement and starts the second animation
    ACTOR_120500_PLAYER_REQUEST_THIRD_ANIMATION  = 4, // Blends into the third animation over 8 frames
    ACTOR_120500_PLAYER_REQUEST_HIDE_BODY        = 5, // Hides the actor's own model and keeps it from being given buffers again
    ACTOR_120500_PLAYER_REQUEST_WEAPON_ANIMATION = 6, // Returns the player to the equipped weapon's animation
};

/// Requests the event script posts in `_Actor120500Work::bodyRequest`.
enum {
    ACTOR_120500_BODY_REQUEST_NONE   = 0,
    ACTOR_120500_BODY_REQUEST_APPEAR = 1, // Allocates the body model's buffers, fades in from black and places the body in the view
};

/// Requests the event script posts in `_Actor120500Work::screenRequest`.
enum {
    ACTOR_120500_SCREEN_REQUEST_NONE       = 0,
    ACTOR_120500_SCREEN_REQUEST_FADE_OUT   = 1, // Fades to black
    ACTOR_120500_SCREEN_REQUEST_PLAY_MOVIE = 2, // Hides the player's model and spawns the task that plays the scene's movie
};

/// Work block of Kyle Madigan's body in the motel room 6 scene, allocated at
/// its full size, zeroed and kept at `Task::work`.
///
/// The actor directs the scene around its movie: the player's animations
/// before and after it, its own body's appearance, and the fades and the movie
/// themselves. The event script drives all three by posting numbered requests,
/// one channel each. The tick performs a posted request once and clears it;
/// posting one restarts the step beside it, which a request lasting several
/// ticks counts its stages in. Request 0 is none.
///
/// Nothing accesses the two `pad` runs; their roles are unproven.
typedef struct {
    ActorAnimRig20 rig;               // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    MATRIX         light;             // Storage for the model's `TmdObject::lightMtx`
    MATRIX         color;             // Storage for the model's `TmdObject::colorMtx`
    Task*          playerTask;        // The player's task, which the player requests are sent to
    u16            playerRequest;     // Request posted for the player, `ACTOR_120500_PLAYER_REQUEST_*`
    u16            playerRequestStep; // Stage of `playerRequest`, 0 when it is posted (first animation: 0 start it, 1 only hold the placement)
    byte           pad_4BC[0x4];
    u16            bodyRequest;       // Request posted for the actor's body, `ACTOR_120500_BODY_REQUEST_*`
    u16            bodyRequestStep;   // Zeroed when `bodyRequest` is posted; never read
    byte           pad_4C4[0x4];
    u16            screenRequest;     // Request posted for the fades and the movie, `ACTOR_120500_SCREEN_REQUEST_*`
    u16            screenRequestStep; // Zeroed when `screenRequest` is posted; never read
} _Actor120500Work;
STATIC_ASSERT_SIZEOF(_Actor120500Work, 0x4CC);

/// The actor task, published by `func_actor_120500_801322A0` so the setters,
/// which take no task, can reach its work block.
extern Task* D_actor_120500_80138454;

/// The actor's five-entry task table: 0 the streamed sequence
/// (`func_actor_120500_80131E58`), 1 the fade from black, 2 the fade to black,
/// 3 `taskKill`, 4 the actor itself.
extern TaskDesc D_actor_120500_80138418[];

/// Animation-set tables bound to the work block's context by `animationInitContext`.
extern AnimationSet* D_actor_120500_80138088[2];

/// Message table the actor answers with: 0x7D5 shows or hides the model, 0x7D4
/// places it.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_120500_80138408[2];

/// Equipped-weapon id and the flag that selects which block of animation sets
/// it indexes (`+1` when set to 1, `+0x22` otherwise).

/// Flags the tick checks before bringing the actor up (`Gp_StateC08.mode` /
/// `gDisplayState.pendingMode`), and the one it raises alongside the view tasks
/// (`gDisplayState.control.flags.flipMode`).

/// Animation-set table handed to the task in pointer slot 3 as message 0x3F4's
/// `AnimationPlayRequest::source`; the messages select sets 0, 1 and 2 of it.
extern AnimationSet* D_actor_120500_8013807C[];

/// Placement records sent to that same task as message 0x3E9, passed by
/// address.
extern ActorTransform D_actor_120500_80138090;
extern ActorTransform D_actor_120500_801380A8;

/// Pair of blocks `func_actor_120500_8013241C` passes to `evsStartScriptWithSkip`.
extern EvsCommand D_actor_120500_801380D8[];
extern EvsCommand D_actor_120500_80138318[];

/// The actor's own placement, sent to itself as message 0x7D4.
extern ActorTransform D_actor_120500_801380C0;

static void _actor120500PostPlayerRequest(s16 requestId);
static void _actor120500PostBodyRequest(s16 requestId);
void        func_actor_120500_80132900(s16);
void        func_actor_120500_80132920(void);

static TmdSource _gActor120500KyleMadiganBody;
void             func_actor_120500_80131E58(Task*);
void             func_actor_120500_8013241C(Task*);
void             func_actor_120500_80132A04(Task*, s32, s32, s32);

static TmdBone _gActor120500KyleMadiganBodySkeleton[20] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

static u32 _gActor120500KyleMadiganBodyPartVerts[20] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

static SVECTOR _gActor120500KyleMadiganBodyVerts[300] = {
#include "assets/kyle_madigan_body_verts.inc"
};

static SVECTOR _gActor120500KyleMadiganBodyNormals[298] = {
#include "assets/kyle_madigan_body_normals.inc"
};

static u32 _gActor120500KyleMadiganBodyStream[3412] = {
#include "assets/kyle_madigan_body_stream.inc"
};

static TmdSource _gActor120500KyleMadiganBody = {
    0,
    18224,
    5696,
    20,
    _gActor120500KyleMadiganBodyPartVerts,
    _gActor120500KyleMadiganBodyVerts,
    _gActor120500KyleMadiganBodyNormals,
    _gActor120500KyleMadiganBodySkeleton,
    _gActor120500KyleMadiganBodyStream,
};

static AnimationPackedPose _gActor120500Animation05BC8Bank1[2] = {
#include "assets/actor_120500_animation_05BC8_bank1.inc"
};

static AnimationPackedRotation _gActor120500Animation05BC8Bank4[47] = {
#include "assets/actor_120500_animation_05BC8_bank4.inc"
};

static AnimationRecord _gActor120500Animation05BC8Records[167] = {
#include "assets/actor_120500_animation_05BC8_records.inc"
};

static u16 _gActor120500Animation05BC8Indices[20] = {
#include "assets/actor_120500_animation_05BC8_indices.inc"
};

static AnimationSet _gActor120500Animation05BC8 = {
    _gActor120500Animation05BC8Records,
    _gActor120500Animation05BC8Indices,
    { NULL, _gActor120500Animation05BC8Bank1, NULL, NULL, _gActor120500Animation05BC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120500Animation05ECCBank1[6] = {
#include "assets/actor_120500_animation_05ECC_bank1.inc"
};

static AnimationPackedRotation _gActor120500Animation05ECCBank4[46] = {
#include "assets/actor_120500_animation_05ECC_bank4.inc"
};

static AnimationRecord _gActor120500Animation05ECCRecords[109] = {
#include "assets/actor_120500_animation_05ECC_records.inc"
};

static u16 _gActor120500Animation05ECCIndices[20] = {
#include "assets/actor_120500_animation_05ECC_indices.inc"
};

static AnimationSet _gActor120500Animation05ECC = {
    _gActor120500Animation05ECCRecords,
    _gActor120500Animation05ECCIndices,
    { NULL, _gActor120500Animation05ECCBank1, NULL, NULL, _gActor120500Animation05ECCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120500Animation0607CBank1[2] = {
#include "assets/actor_120500_animation_0607C_bank1.inc"
};

static AnimationPackedRotation _gActor120500Animation0607CBank4[6] = {
#include "assets/actor_120500_animation_0607C_bank4.inc"
};

static AnimationRecord _gActor120500Animation0607CRecords[76] = {
#include "assets/actor_120500_animation_0607C_records.inc"
};

static u16 _gActor120500Animation0607CIndices[20] = {
#include "assets/actor_120500_animation_0607C_indices.inc"
};

static AnimationSet _gActor120500Animation0607C = {
    _gActor120500Animation0607CRecords,
    _gActor120500Animation0607CIndices,
    { NULL, _gActor120500Animation0607CBank1, NULL, NULL, _gActor120500Animation0607CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120500Animation06234Bank1[2] = {
#include "assets/actor_120500_animation_06234_bank1.inc"
};

static AnimationPackedRotation _gActor120500Animation06234Bank4[8] = {
#include "assets/actor_120500_animation_06234_bank4.inc"
};

static AnimationRecord _gActor120500Animation06234Records[76] = {
#include "assets/actor_120500_animation_06234_records.inc"
};

static u16 _gActor120500Animation06234Indices[20] = {
#include "assets/actor_120500_animation_06234_indices.inc"
};

static AnimationSet _gActor120500Animation06234 = {
    _gActor120500Animation06234Records,
    _gActor120500Animation06234Indices,
    { NULL, _gActor120500Animation06234Bank1, NULL, NULL, _gActor120500Animation06234Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_120500_8013807C[3] = {
    &_gActor120500Animation05ECC,
    &_gActor120500Animation0607C,
    &_gActor120500Animation06234,
};

AnimationSet* D_actor_120500_80138088[2] = {
    NULL,
    &_gActor120500Animation05BC8,
};

ActorTransform D_actor_120500_80138090 = { { 6330, -3200, -4900, 0 }, { 0, 3584, 0, 0 } };

ActorTransform D_actor_120500_801380A8 = { { 787, 0, 7000, 0 }, { 0, 3584, 0, 0 } };

ActorTransform D_actor_120500_801380C0 = { { 800, -0x2EE0, -2750, 0 }, { 0, 2048, 0, 0 } };

EvsCommand D_actor_120500_801380D8[24] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120500PostPlayerRequest }, { .value = ACTOR_120500_PLAYER_REQUEST_FIRST_ANIMATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120500_80132900 }, { .value = ACTOR_120500_SCREEN_REQUEST_FADE_OUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120500_80132900 }, { .value = ACTOR_120500_SCREEN_REQUEST_PLAY_MOVIE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120500PostBodyRequest }, { .value = ACTOR_120500_BODY_REQUEST_APPEAR }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120500_80132900 }, { .value = ACTOR_120500_SCREEN_REQUEST_FADE_OUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120500PostPlayerRequest }, { .value = ACTOR_120500_PLAYER_REQUEST_HIDE_BODY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120500PostPlayerRequest }, { .value = ACTOR_120500_PLAYER_REQUEST_SECOND_ANIMATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120500PostPlayerRequest }, { .value = ACTOR_120500_PLAYER_REQUEST_THIRD_ANIMATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120500PostPlayerRequest }, { .value = ACTOR_120500_PLAYER_REQUEST_WEAPON_ANIMATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_120500_80138318[10] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120500_80132920 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskMessageEntry D_actor_120500_80138408[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_120500_80132A04 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceInView },
};

TaskDesc D_actor_120500_80138418[3] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_120500_80131E58, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _screenFadeInTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _screenFadeOutTask, { .value = 0 } },
};

TaskDesc D_actor_120500_8013843C = { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } };

TaskDesc D_actor_120500_80138448 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_120500_8013241C, { .model = &_gActor120500KyleMadiganBody } };

Task* D_actor_120500_80138454 = NULL;

static void func_actor_120500_80132028(Task* arg0);
static void func_actor_120500_801322A0(Task* task);

/// Entry 0 of the task table: plays a streamed sequence, then restores the
/// scene. It looks up the stream slot for the current location with view 0x64
/// and enqueues CD command 0x61 on it, turns the display on with sound cue
/// 0x521E0007 once the queue reports ready, and waits for the stream to end or
/// for the pad to cut it short. After the restore it spawns the fade from
/// black, clears the image buffers, and kills itself.
void func_actor_120500_80131E58(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    s16         slot;

    queue = &gCdCmdQueue;
    switch (arg0->state) {
        case 0:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            key          = gGameSession->location;
            key.loc.view = 0x64;
            slot         = streamFindMovieSlot(&key.loc, 0, 0);
            slotParam[0] = slot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            arg0->state = arg0->state + 1;
            return;
        case 2:
            if (queue->movieReady == 0) {
                return;
            }
            sndEvtRequestScriptStart(SOUND_MOTEL_ROOM_6_MOVIE_SFX, 0, 0);
            SetDispMask(1);
            arg0->state = arg0->state + 1;
            return;
        case 3:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                arg0->state = arg0->state + 1;
                return;
            }
            if (padIsStartPressed() == 0) {
                return;
            }
            SetDispMask(0);
            cdCmdRequestCancel();
            arg0->state = arg0->state + 1;
            return;
        case 4:
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                return;
            }
            streamResetGameRestore();
            arg0->state = arg0->state + 1;
            return;
        case 5:
            if ((streamPollGameRestore(0, 1) & 0xFFFF) == 0) {
                return;
            }
            taskSpawnFromTableOnDefaultList(D_actor_120500_80138418, 1, 8, 0);
            memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
            SetDispMask(1);
            taskKill(arg0);
            displayResumeGameLoop();
            return;
    }
}

/// Performs the request posted in `_Actor120500Work::playerRequest`, stepped by
/// the tick body and posted by `_actor120500PostPlayerRequest`. Every tick first
/// sends `ANIMATION_MESSAGE_IS_PLAYING` to `playerTask`, then dispatches on the
/// request. `ACTOR_120500_PLAYER_REQUEST_FIRST_ANIMATION` is the only one that
/// does not clear itself: after its first tick it keeps sending the placement
/// record `D_actor_120500_80138090`. The weapon animation is picked the same
/// way `func_actor_120500_8013241C` picks it. Every code without a handler, 1
/// included, just clears the request.
static void func_actor_120500_80132028(Task* arg0)
{
    _Actor120500Work*     work;
    _Actor120500Work*     reloadedWork;
    _Actor120500Work*     animWork;
    _Actor120500Work*     firstAnimWork;
    SVECTOR               vec;
    AnimationPlayRequest  msg;
    AnimationPlayRequest* p;
    s32                   anim;
    s32                   base;

    work = arg0->work;
    if (work->playerTask != NULL) {
        taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0);
    }
    switch (work->playerRequest) {
        case ACTOR_120500_PLAYER_REQUEST_NONE:
        case 1:
            break;
        case ACTOR_120500_PLAYER_REQUEST_FIRST_ANIMATION:
            switch (work->playerRequestStep) {
                case 0:
                    vec.vx = 0x960;
                    vec.vy = 0x960;
                    vec.vz = 0x960;
                    worldCoordSetAmbientColorOverride(&vec);
                    firstAnimWork = arg0->work;
                    p             = &msg;
                    if (firstAnimWork->playerTask != NULL) {
                        msg.source.sets         = D_actor_120500_8013807C;
                        msg.animationId         = 0;
                        msg.blend               = ANIMATION_BLEND_RESET;
                        msg.blendFrames         = 0;
                        p->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(firstAnimWork->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, p, 0);
                    }
                    work->playerRequestStep = work->playerRequestStep + 1;
                    /* fallthrough */
                case 1:
                    TASK_MESSAGE_DISPATCH_POINTER(((_Actor120500Work*)arg0->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE,
                                                  &D_actor_120500_80138090, 0);
                    return;
            }
            return;
        case ACTOR_120500_PLAYER_REQUEST_SECOND_ANIMATION:
            taskSpawnFromTable(D_actor_120500_80138418, 1, 8, 0);
            reloadedWork = arg0->work;
            worldCoordSetAmbientColorOverride(NULL);
            taskMessageDispatch(reloadedWork->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            TASK_MESSAGE_DISPATCH_POINTER(reloadedWork->playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_120500_801380A8, 0);
            animWork = arg0->work;
            p        = &msg;
            if (animWork->playerTask != NULL) {
                msg.source.sets         = D_actor_120500_8013807C;
                p->animationId          = 1;
                msg.blend               = ANIMATION_BLEND_RESET;
                msg.blendFrames         = 0;
                p->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(animWork->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, p, 0);
            }
            break;
        case ACTOR_120500_PLAYER_REQUEST_THIRD_ANIMATION:
            animWork = arg0->work;
            p        = &msg;
            if (animWork->playerTask != NULL) {
                msg.source.sets         = D_actor_120500_8013807C;
                p->animationId          = 2;
                p->blend                = ANIMATION_BLEND_INTERPOLATE;
                p->blendFrames          = 8;
                p->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(animWork->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, p, 0);
            }
            break;
        case ACTOR_120500_PLAYER_REQUEST_HIDE_BODY:
            taskMessageDispatch(arg0, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            break;
        case ACTOR_120500_PLAYER_REQUEST_WEAPON_ANIMATION:
            base = gPlayerStatus.weapon;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {
                anim = base + 1;
            } else {
                anim = base + 0x22;
            }
            msg.source.index         = anim;
            msg.animationId          = 1;
            msg.blend                = ANIMATION_BLEND_RESET;
            msg.blendFrames          = 0;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_PLAY, &msg, 0);
            break;
    }
    work->playerRequest = ACTOR_120500_PLAYER_REQUEST_NONE;
}

/// Initialize the cutscene actor's model and animations.
///
/// Uses the area placement for resource-entry 0x65, or the end record when
/// that entry is absent. Allocation failure kills `task`.
static void func_actor_120500_801322A0(Task* task)
{
    enum { TEXTURE_RESOURCE_ENTRY_ID = 0x65 };

    _Actor120500Work* work;
    _Actor120500Work* allocatedWork;
    _Actor120500Work* slotsWork;
    TmdObject*        tmd;
    GfxCoord*         coord;
    AreaPlacement*    place;
    s32               slotIndex;
    u8                entryId;

    tmd           = task->extra.tmd;
    coord         = tmd->coords;
    allocatedWork = memMalloc(sizeof(_Actor120500Work), false);
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        taskKill(task);
        return;
    }
    work = allocatedWork;
    memFillBytes(work, 0, sizeof(*work));
    work->playerTask        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    D_actor_120500_80138454 = task;
    coord->parent           = &gGfxViewCoord;
    tmd->lightMtx           = &work->light;
    tmd->flags              = 0;
    tmd->colorMtx           = &work->color;
    place                   = areaGetVariant(&gGameSession->location.loc)->placements;
    entryId                 = place->entryId;
    while (entryId != AREA_PLACEMENT_END) {
        if (entryId == TEXTURE_RESOURCE_ENTRY_ID) {
            break;
        }
        place++;
        entryId = place->entryId;
    }
    tmdSetTextureOffsets(tmd, place->texturePageOffset, place->clutRowOffset);
    animationInitContext(&work->rig.anim, D_actor_120500_80138088, tmd, work->rig.poses, work->rig.slots);
    slotsWork      = task->work;
    task->msgTable = D_actor_120500_80138408;
    slotIndex      = 1;
    do {
        slotsWork->rig.slots[(u16)slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&slotsWork->rig.anim, (u16)slotIndex, 1);
        slotIndex++;
    } while ((u16)slotIndex < ARRAY_SIZE(slotsWork->rig.slots));
}

/// Per-frame body of the actor task, entry 4 of the task table. State 0 waits
/// until `Gp_StateC08.mode` is not 1 and `gDisplayState.pendingMode` is clear, then brings the actor
/// up through `func_actor_120500_801322A0`, sends the task in pointer slot 3
/// the equipped-weapon animation as message 0x3E8 and installs the two
/// `evsStartScriptWithSkip` blocks; state 1 kills the actor once the session's
/// `eventState` clears.
///
/// Every state then steps the request handler, ticks the nineteen animation
/// slots past slot 0 and walks the slots to the first whose
/// `ANIMATION_SLOT_REACHED_BOUNDARY` result is clear. The body request
/// `ACTOR_120500_BODY_REQUEST_APPEAR` allocates the model's buffers, spawns the
/// fade from black and places the actor with its own placement record. Of the
/// screen requests, `ACTOR_120500_SCREEN_REQUEST_FADE_OUT` spawns the fade to
/// black and `ACTOR_120500_SCREEN_REQUEST_PLAY_MOVIE` hides the player's model,
/// spawns the streamed sequence, raises `gDisplayState.control.flags.flipMode`
/// and spawns the view tasks. The model's part-1 translation goes to
/// `worldCoordSetModelLighting` last.
///
/// The animation request and the translation are locals of two separate
/// blocks so that they share one stack slot, as the retail frame has them.
void func_actor_120500_8013241C(Task* arg0)
{
    _Actor120500Work* work;
    _Actor120500Work* slotsWork;
    _Actor120500Work* screenWork;
    TmdObject*        mdl;
    s32               anim;
    s32               i;

    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                AnimationPlayRequest request;

                func_actor_120500_801322A0(arg0);
                anim = gPlayerStatus.weapon;
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {
                    anim = anim + 1;
                } else {
                    anim = anim + 0x22;
                }
                request.source.index         = anim;
                request.animationId          = 1;
                request.blend                = ANIMATION_BLEND_INTERPOLATE;
                request.blendFrames          = 10;
                request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0xD);
                evsStartScriptWithSkip(D_actor_120500_801380D8, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_120500_80138318);
                arg0->state += 1;
                break;
            }
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                taskRequestKill(arg0, 0);
                return;
            }
            break;
    }

    func_actor_120500_80132028(arg0);
    work      = arg0->work;
    slotsWork = work;

    i = 1;
    do {
        animationTickSlot(&slotsWork->rig.anim, (u16)i);
        i++;
    } while ((u16)i < ARRAY_SIZE(slotsWork->rig.slots));

    i = 1;
    while (1) {
        if ((slotsWork->rig.slots[(u16)i].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) != 0) {
            i++;
            if ((u16)i < ARRAY_SIZE(slotsWork->rig.slots)) {
                continue;
            }
        }
        break;
    }

    if (work->bodyRequest != ACTOR_120500_BODY_REQUEST_NONE) {
        if (work->bodyRequest == ACTOR_120500_BODY_REQUEST_APPEAR) {
            tmdAllocPrimitiveBuffer(arg0->extra.tmd);
            taskSpawnFromTable(D_actor_120500_80138418, 1, 8, 0);
            TASK_MESSAGE_DISPATCH_POINTER(arg0, ACTOR_MESSAGE_PLACE, &D_actor_120500_801380C0, 0);
        }
    }
    work->bodyRequest = ACTOR_120500_BODY_REQUEST_NONE;

    screenWork = arg0->work;
    switch (screenWork->screenRequest) {
        case ACTOR_120500_SCREEN_REQUEST_FADE_OUT:
            taskSpawnFromTable(D_actor_120500_80138418, 2, 8, 0);
            break;
        case ACTOR_120500_SCREEN_REQUEST_PLAY_MOVIE:
            taskMessageDispatch(screenWork->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            displaySpawnTaskFromTable(D_actor_120500_80138418, 0, 0, 0);
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
            viewQueueCurrentCameraAndPackets();
            break;
        case ACTOR_120500_SCREEN_REQUEST_NONE:
        default:
            break;
    }
    screenWork->screenRequest = ACTOR_120500_SCREEN_REQUEST_NONE;

    {
        VECTOR pos;

        mdl    = arg0->extra.tmd;
        pos.vx = arg0->extra.tmd->coords[1].workm.t[0];
        pos.vy = arg0->extra.tmd->coords[1].workm.t[1];
        pos.vz = arg0->extra.tmd->coords[1].workm.t[2];
        worldCoordSetModelLighting(mdl, &pos, 0, 3);
    }
}

#include "../../shared/screen_fade_in.inc.c"

#include "../../shared/screen_fade_out.inc.c"

/// Posts a player-choreography request for the motel room 6 scene.
///
/// Requires the published body task and initialized work to remain live.
/// Stores requestId's halfword bits in the unsigned request channel and resets
/// its step. ACTOR_120500_PLAYER_REQUEST_* selects the scene beats; zero cancels
/// a pending request and other codes are consumed without effect. The tick
/// performs the request later; this callback sends no player message.
static void _actor120500PostPlayerRequest(s16 requestId)
{
    _Actor120500Work* work = D_actor_120500_80138454->work;

    work->playerRequest     = requestId;
    work->playerRequestStep = 0;
}

/// Posts a body-choreography request for the motel room 6 scene.
///
/// Requires the published body task and initialized work to remain live.
/// Stores requestId's halfword bits and resets the body step. APPEAR (1) asks
/// the next tick to allocate buffers, fade in and place the body; zero cancels,
/// and other codes are consumed without effect. The step is retained storage
/// that this scene does not read. This callback performs no rendering or fade.
static void _actor120500PostBodyRequest(s16 requestId)
{
    _Actor120500Work* work = D_actor_120500_80138454->work;

    work->bodyRequest     = requestId;
    work->bodyRequestStep = 0;
}

void func_actor_120500_80132900(s16 arg0)
{
    _Actor120500Work* work = D_actor_120500_80138454->work;

    work->screenRequest     = arg0;
    work->screenRequestStep = 0;
}

/// Puts the actor back to rest: plays sound cue `0x521E0007`, sends the actor
/// its own message 0x7D5 with payload 2, hiding the model, and clears the
/// three requests the setters above post. The player's task then
/// gets animation set 2 of `D_actor_120500_8013807C` (message 0x3F4), message
/// 0x3F3 with payload 1, and the placement record `D_actor_120500_801380A8`
/// as message 0x3E9, with the ambient RGB override disabled in between.
void func_actor_120500_80132920(void)
{
    Task*                actor;
    _Actor120500Work*    work;
    _Actor120500Work*    animWork;
    AnimationPlayRequest msg;

    actor = D_actor_120500_80138454;
    work  = actor->work;
    sndEvtRequestScriptStop(SOUND_MOTEL_ROOM_6_MOVIE_SFX, 0xA);
    taskMessageDispatch(actor, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
    work->playerRequest = ACTOR_120500_PLAYER_REQUEST_NONE;
    work->bodyRequest   = ACTOR_120500_BODY_REQUEST_NONE;
    work->screenRequest = ACTOR_120500_SCREEN_REQUEST_NONE;
    animWork            = actor->work;
    if (animWork->playerTask != NULL) {
        msg.source.sets          = D_actor_120500_8013807C;
        msg.animationId          = 2;
        msg.blend                = ANIMATION_BLEND_RESET;
        msg.blendFrames          = 0;
        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(animWork->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
    }
    work = actor->work;
    worldCoordSetAmbientColorOverride(NULL);
    taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_120500_801380A8, 0);
}

/// Message 0x7D5 handler: shows or hides the task's model. Payload 0 hides it
/// (sets `TmdObject` flag 0x80), 1 shows it and clears `TMD_OBJECT_SKIP_AUTO_BUFFER`, and 2 hides
/// it and sets `TMD_OBJECT_SKIP_AUTO_BUFFER`, which keeps `tmdResetAuxHeapAndRestoreBuffers` from giving it
/// buffers again. Payload 2 sets `TMD_OBJECT_SKIP_AUTO_BUFFER` and falls into payload 0, rather than
/// setting both bits at once, and the branch layout follows that. `arg1` is
/// the message id.
void func_actor_120500_80132A04(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* extra;

    extra = task->extra.tmd;
    switch (arg2) {
        case 2:
            extra->flags = extra->flags | TMD_OBJECT_SKIP_AUTO_BUFFER;
            /* fallthrough */
        case 0:
            extra->flags = extra->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case 1:
            extra->flags = extra->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
    }
}

#include "../../shared/actor_messages_place_in_view.inc.c"
