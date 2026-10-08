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

/// The actor task, published by `_actor120500InitBody` so the setters,
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

/// Pair of blocks `_actor120500SceneTask` passes to `evsStartScriptWithSkip`.
extern EvsCommand D_actor_120500_801380D8[];
extern EvsCommand D_actor_120500_80138318[];

/// The actor's own placement, sent to itself as message 0x7D4.
extern ActorTransform D_actor_120500_801380C0;

static void _actor120500PostPlayerRequest(s16 requestId);
static void _actor120500PostBodyRequest(s16 requestId);
static void _actor120500PostScreenRequest(s16 requestId);
static void _actor120500SkipScene(void);

static TmdSource _gActor120500KyleMadiganBody;
void             func_actor_120500_80131E58(Task*);
static void      _actor120500SceneTask(Task* task);
static void      _actor120500SetModelDraw(Task* task, s32 unusedMessageId, s32 drawMode, s32 unusedArg);

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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120500PostScreenRequest }, { .value = ACTOR_120500_SCREEN_REQUEST_FADE_OUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120500PostScreenRequest }, { .value = ACTOR_120500_SCREEN_REQUEST_PLAY_MOVIE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120500PostBodyRequest }, { .value = ACTOR_120500_BODY_REQUEST_APPEAR }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120500PostScreenRequest }, { .value = ACTOR_120500_SCREEN_REQUEST_FADE_OUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor120500SkipScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskMessageEntry D_actor_120500_80138408[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor120500SetModelDraw },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceInView },
};

TaskDesc D_actor_120500_80138418[3] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_120500_80131E58, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _screenFadeInTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _screenFadeOutTask, { .value = 0 } },
};

TaskDesc D_actor_120500_8013843C = { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } };

TaskDesc D_actor_120500_80138448 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor120500SceneTask, { .model = &_gActor120500KyleMadiganBody } };

Task* D_actor_120500_80138454 = NULL;

static void _actor120500RunPlayerRequest(Task* task);
static void _actor120500InitBody(Task* task);

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

/// Consumes the motel scene's player-animation and placement request channel.
///
/// Requires initialized live scene work, the loaded three scene sets and a
/// player task for placement and weapon requests. Animation installs alone
/// tolerate an absent player. Dispatch borrows stack requests synchronously.
/// Queries player playback every tick, then handles the posted halfword code.
/// The first-animation request sets ambient RGB to 2400, starts clip 0 once,
/// and keeps applying the first placement until replaced. Other handled codes
/// fade/show/place and start clip 1, blend clip 2 for eight frames, hide the
/// scene body or restore the equipped-weapon clip with collision disabled.
/// Weapon requests require saved character 1 and weapon 0..32 for the loaded
/// player bank table; the retained alternate-character offset is unproven.
/// All codes except the first-animation request clear after this tick.
static void _actor120500RunPlayerRequest(Task* task)
{
    /// Restores equipped-weapon playback with world collision disabled.
    ///
    /// Captures work, request, weaponId, animationBank and the local bank/clip
    /// constants. Uses the current weapon and saved character at dispatch time;
    /// primary character 1 requires weapon 0..32. Retains the alternate +34
    /// selector, whose runtime reachability is unproven. Borrows request only
    /// through synchronous message dispatch. Undefined after this function.
#define ACTOR_120500_PLAY_PLAYER_WEAPON_ANIMATION()                                                   \
    {                                                                                                 \
        weaponId = gPlayerStatus.weapon;                                                              \
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_120500_PRIMARY_CHARACTER) { \
            animationBank = weaponId + ACTOR_120500_PRIMARY_WEAPON_BANK_BASE;                         \
        } else {                                                                                      \
            animationBank = weaponId + ACTOR_120500_ALTERNATE_WEAPON_BANK_BASE;                       \
        }                                                                                             \
        request.source.index         = animationBank;                                                 \
        request.animationId          = ACTOR_120500_WEAPON_ANIMATION;                                 \
        request.blend                = ANIMATION_BLEND_RESET;                                         \
        request.blendFrames          = 0;                                                             \
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                             \
        TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_PLAY, &request, 0);         \
    }
    enum {
        ACTOR_120500_FIRST_ANIMATION              = 0,
        ACTOR_120500_SECOND_ANIMATION             = 1,
        ACTOR_120500_THIRD_ANIMATION              = 2,
        ACTOR_120500_THIRD_ANIMATION_BLEND_FRAMES = 8,
        ACTOR_120500_FIRST_AMBIENT_CHANNEL        = 2400,
        ACTOR_120500_FADE_IN_TASK                 = 1,
        ACTOR_120500_FADE_INTENSITY_STEP          = 8,
        ACTOR_120500_PRIMARY_CHARACTER            = 1,
        ACTOR_120500_PRIMARY_WEAPON_BANK_BASE     = 1,
        ACTOR_120500_ALTERNATE_WEAPON_BANK_BASE   = 34,
        ACTOR_120500_WEAPON_ANIMATION             = 1,
        ACTOR_120500_FIRST_REQUEST_START          = 0,
        ACTOR_120500_FIRST_REQUEST_HOLD_PLACEMENT = 1,
    };
    _Actor120500Work*     work;
    _Actor120500Work*     reloadedWork;
    _Actor120500Work*     animWork;
    _Actor120500Work*     firstAnimWork;
    SVECTOR               ambientColor;
    AnimationPlayRequest  request;
    AnimationPlayRequest* requestPointer;
    s32                   animationBank;
    s32                   weaponId;

    work = task->work;
    if (work->playerTask != NULL) {
        taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0);
    }
    switch (work->playerRequest) {
        case ACTOR_120500_PLAYER_REQUEST_NONE:
            break;
        case ACTOR_120500_PLAYER_REQUEST_FIRST_ANIMATION:
            switch (work->playerRequestStep) {
                case ACTOR_120500_FIRST_REQUEST_START:
                    ambientColor.vx = ACTOR_120500_FIRST_AMBIENT_CHANNEL;
                    ambientColor.vy = ACTOR_120500_FIRST_AMBIENT_CHANNEL;
                    ambientColor.vz = ACTOR_120500_FIRST_AMBIENT_CHANNEL;
                    worldCoordSetAmbientColorOverride(&ambientColor);
                    // Start the clip once; later ticks keep applying its placement.
                    firstAnimWork  = task->work;
                    requestPointer = &request;
                    if (firstAnimWork->playerTask != NULL) {
                        request.source.sets                  = D_actor_120500_8013807C;
                        request.animationId                  = ACTOR_120500_FIRST_ANIMATION;
                        request.blend                        = ANIMATION_BLEND_RESET;
                        request.blendFrames                  = 0;
                        requestPointer->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(firstAnimWork->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, requestPointer, 0);
                    }
                    work->playerRequestStep = work->playerRequestStep + 1;
                    /* fallthrough */
                case ACTOR_120500_FIRST_REQUEST_HOLD_PLACEMENT:
                    TASK_MESSAGE_DISPATCH_POINTER(((_Actor120500Work*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE,
                                                  &D_actor_120500_80138090, 0);
                    return;
            }
            return;
        case ACTOR_120500_PLAYER_REQUEST_SECOND_ANIMATION:
            taskSpawnFromTable(D_actor_120500_80138418, ACTOR_120500_FADE_IN_TASK, ACTOR_120500_FADE_INTENSITY_STEP, 0);
            reloadedWork = task->work;
            worldCoordSetAmbientColorOverride(NULL);
            taskMessageDispatch(reloadedWork->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            TASK_MESSAGE_DISPATCH_POINTER(reloadedWork->playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_120500_801380A8, 0);
            animWork       = task->work;
            requestPointer = &request;
            if (animWork->playerTask != NULL) {
                request.source.sets                  = D_actor_120500_8013807C;
                requestPointer->animationId          = ACTOR_120500_SECOND_ANIMATION;
                request.blend                        = ANIMATION_BLEND_RESET;
                request.blendFrames                  = 0;
                requestPointer->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(animWork->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, requestPointer, 0);
            }
            break;
        case ACTOR_120500_PLAYER_REQUEST_THIRD_ANIMATION:
            animWork       = task->work;
            requestPointer = &request;
            if (animWork->playerTask != NULL) {
                request.source.sets                  = D_actor_120500_8013807C;
                requestPointer->animationId          = ACTOR_120500_THIRD_ANIMATION;
                requestPointer->blend                = ANIMATION_BLEND_INTERPOLATE;
                requestPointer->blendFrames          = ACTOR_120500_THIRD_ANIMATION_BLEND_FRAMES;
                requestPointer->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(animWork->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, requestPointer, 0);
            }
            break;
        case ACTOR_120500_PLAYER_REQUEST_HIDE_BODY:
            taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            break;
        case ACTOR_120500_PLAYER_REQUEST_WEAPON_ANIMATION:
            ACTOR_120500_PLAY_PLAYER_WEAPON_ANIMATION();
            break;
    }
    work->playerRequest = ACTOR_120500_PLAYER_REQUEST_NONE;
#undef ACTOR_120500_PLAY_PLAYER_WEAPON_ANIMATION
}

/// Resets body slots 1..19 to clip 1 at normal playback rate.
///
/// Requires a rig bound to the loaded two-entry body clip table, with live
/// pose/slot/model storage. Slot 0 is retained; this resets cursors and rates
/// without ticking a pose or changing the enclosing scene request channels.
static __inline__ void _actor120500ResetBodyTracks(_Actor120500Work* work)
{
    enum { ACTOR_120500_FIRST_BODY_SLOT   = 1,
           ACTOR_120500_INITIAL_BODY_CLIP = 1 };
    s32 slotIndex = ACTOR_120500_FIRST_BODY_SLOT;
    do {
        work->rig.slots[(u16)slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, (u16)slotIndex, ACTOR_120500_INITIAL_BODY_CLIP);
        slotIndex++;
    } while ((u16)slotIndex < ARRAY_SIZE(work->rig.slots));
}

/// Initializes the motel scene's Kyle Madigan body and nineteen animation tracks.
///
/// Requires the task's twenty-part TMD and the current area's placement table.
/// Owns a cleared primary-heap work block, publishes the task for scene requests,
/// and binds its lighting and animation storage. Allocation failure kills the
/// task. Texture entry 0x65 is preferred; the table's end record is used if absent.
static void _actor120500InitBody(Task* task)
{
    enum { ACTOR_120500_BODY_TEXTURE_ENTRY_ID = 0x65 };

    _Actor120500Work* work;
    _Actor120500Work* allocatedWork;
    _Actor120500Work* slotsWork;
    TmdObject*        tmd;
    GfxCoord*         coord;
    AreaPlacement*    place;
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
        if (entryId == ACTOR_120500_BODY_TEXTURE_ENTRY_ID) {
            break;
        }
        place++;
        entryId = place->entryId;
    }
    tmdSetTextureOffsets(tmd, place->texturePageOffset, place->clutRowOffset);
    animationInitContext(&work->rig.anim, D_actor_120500_80138088, tmd, work->rig.poses, work->rig.slots);
    slotsWork      = task->work;
    task->msgTable = D_actor_120500_80138408;
    _actor120500ResetBodyTracks(slotsWork);
}

/// Blends the current equipped-weapon pose for the scene's player task.
///
/// Requires saved character 1 with weapon 0..32 in the loaded player bank.
/// The retained other-character bank offset is unproven. Collision is disabled
/// and the stack request is borrowed only through synchronous dispatch.
static inline void _actor120500BlendPlayerWeaponPose(void)
{
    enum {
        ACTOR_120500_PRIMARY_CHARACTER          = 1,
        ACTOR_120500_PRIMARY_WEAPON_BANK_BASE   = 1,
        ACTOR_120500_ALTERNATE_WEAPON_BANK_BASE = 34,
        ACTOR_120500_WEAPON_CLIP                = 1,
        ACTOR_120500_WEAPON_BLEND_FRAMES        = 10,
    };

    AnimationPlayRequest request;
    s32                  animationBank;

    animationBank = gPlayerStatus.weapon;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_120500_PRIMARY_CHARACTER) {
        animationBank = animationBank + ACTOR_120500_PRIMARY_WEAPON_BANK_BASE;
    } else {
        animationBank = animationBank + ACTOR_120500_ALTERNATE_WEAPON_BANK_BASE;
    }
    request.source.index         = animationBank;
    request.animationId          = ACTOR_120500_WEAPON_CLIP;
    request.blend                = ANIMATION_BLEND_INTERPOLATE;
    request.blendFrames          = ACTOR_120500_WEAPON_BLEND_FRAMES;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
}

/// Runs the motel room 6 body, player choreography and movie/fade requests.
///
/// State 0 waits for the attachment wheel and pending display mode to clear,
/// initializes the body, blends the equipped-weapon pose and starts the skippable
/// scene. State 1 requests teardown when event playback becomes idle. Other paths
/// tick non-root body tracks, consume body/screen requests and sample part-1 lighting.
/// Requests borrow overlay and stack data synchronously; model/work and scene assets
/// must remain live. The retained initialization-failure path continues after taskKill.
static void _actor120500SceneTask(Task* task)
{
    enum {
        ACTOR_120500_LIGHT_COUNT         = 3,
        ACTOR_120500_SCENE_INITIALIZE    = 0,
        ACTOR_120500_SCENE_RUNNING       = 1,
        ACTOR_120500_SCENE_OBJECTIVE     = 13,
        ACTOR_120500_FADE_IN_DESCRIPTOR  = 1,
        ACTOR_120500_FADE_OUT_DESCRIPTOR = 2,
        ACTOR_120500_FADE_STEP           = 8,
    };

    _Actor120500Work* work;
    _Actor120500Work* slotsWork;
    _Actor120500Work* screenWork;
    TmdObject*        model;
    s32               slotIndex;

    switch (task->state) {
        case ACTOR_120500_SCENE_INITIALIZE:
            if (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {

                _actor120500InitBody(task);
                _actor120500BlendPlayerWeaponPose();
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACTOR_120500_SCENE_OBJECTIVE);
                evsStartScriptWithSkip(D_actor_120500_801380D8, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_120500_80138318);
                task->state += 1;
                break;
            }
            return;
        case ACTOR_120500_SCENE_RUNNING:
            if (gGameSession->eventState == 0) {
                taskRequestKill(task, 0);
                return;
            }
            break;
    }

    // Choreography precedes body playback and the one-tick appearance/screen requests.
    _actor120500RunPlayerRequest(task);
    work      = task->work;
    slotsWork = work;

    slotIndex = 1;
    do {
        animationTickSlot(&slotsWork->rig.anim, (u16)slotIndex);
        slotIndex++;
    } while ((u16)slotIndex < ARRAY_SIZE(slotsWork->rig.slots));

    // The original scans for the first unfinished track and discards the result.
    slotIndex = 1;
    while (1) {
        if ((slotsWork->rig.slots[(u16)slotIndex].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) != 0) {
            slotIndex++;
            if ((u16)slotIndex < ARRAY_SIZE(slotsWork->rig.slots)) {
                continue;
            }
        }
        break;
    }

    if (work->bodyRequest != ACTOR_120500_BODY_REQUEST_NONE) {
        if (work->bodyRequest == ACTOR_120500_BODY_REQUEST_APPEAR) {
            tmdAllocPrimitiveBuffer(task->extra.tmd);
            taskSpawnFromTable(D_actor_120500_80138418, ACTOR_120500_FADE_IN_DESCRIPTOR, ACTOR_120500_FADE_STEP, 0);
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &D_actor_120500_801380C0, 0);
        }
    }
    work->bodyRequest = ACTOR_120500_BODY_REQUEST_NONE;

    screenWork = task->work;
    switch (screenWork->screenRequest) {
        case ACTOR_120500_SCREEN_REQUEST_FADE_OUT:
            taskSpawnFromTable(D_actor_120500_80138418, ACTOR_120500_FADE_OUT_DESCRIPTOR, ACTOR_120500_FADE_STEP, 0);
            break;
        case ACTOR_120500_SCREEN_REQUEST_PLAY_MOVIE:
            taskMessageDispatch(screenWork->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
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
        VECTOR lightPosition;

        model            = task->extra.tmd;
        lightPosition.vx = task->extra.tmd->coords[1].workm.t[0];
        lightPosition.vy = task->extra.tmd->coords[1].workm.t[1];
        lightPosition.vz = task->extra.tmd->coords[1].workm.t[2];
        worldCoordSetModelLighting(model, &lightPosition, 0, ACTOR_120500_LIGHT_COUNT);
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

/// Posts a fade or movie request for the motel room 6 scene.
///
/// Requires the published actor task and initialized work to be live. Stores
/// requestId's halfword bits and resets the unused screen step. The next update
/// consumes ACTOR_120500_SCREEN_REQUEST_*; zero cancels a pending request and
/// unsupported codes are cleared without effect. Performs no screen operation.
static void _actor120500PostScreenRequest(s16 requestId)
{
    _Actor120500Work* work = D_actor_120500_80138454->work;

    work->screenRequest     = requestId;
    work->screenRequestStep = 0;
}

/// Restores the player's presentation when the motel room 6 scene is skipped.
///
/// Requires the published actor and its work to remain live through dispatch.
/// Stops the movie sound, hides the double and cancels its choreography channels.
/// With a player task, installs scene animation 2 without blending; then clears
/// the ambient RGB override and shows and places the player. Player placement
/// follows animation installation. The enclosing skip script handles scene teardown.
static void _actor120500SkipScene(void)
{
    /// Reloads the player task and installs a scene animation when present.
    ///
    /// Captures task, animWork and writable request storage. Requires live work
    /// and loaded scene sets. requestedAnimation is evaluated once only when the player
    /// is present; dispatch borrows the request synchronously. Restarts without
    /// blending and enables world collision.
#define ACTOR_120500_INSTALL_PLAYER_ANIMATION(requestedAnimation)                                                 \
    {                                                                                                             \
        animWork = task->work;                                                                                    \
        if (animWork->playerTask != NULL) {                                                                       \
            request.source.sets          = D_actor_120500_8013807C;                                               \
            request.animationId          = (requestedAnimation);                                                  \
            request.blend                = ANIMATION_BLEND_RESET;                                                 \
            request.blendFrames          = 0;                                                                     \
            request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;                                      \
            TASK_MESSAGE_DISPATCH_POINTER(animWork->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0); \
        }                                                                                                         \
    }

    enum { ACTOR_120500_SKIP_SOUND_FADE_UPDATES = 10,
           ACTOR_120500_SKIP_PLAYER_ANIMATION   = 2 };

    Task*                task;
    _Actor120500Work*    work;
    _Actor120500Work*    animWork;
    AnimationPlayRequest request;

    task = D_actor_120500_80138454;
    work = task->work;
    sndEvtRequestScriptStop(SOUND_MOTEL_ROOM_6_MOVIE_SFX, ACTOR_120500_SKIP_SOUND_FADE_UPDATES);
    taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
    work->playerRequest = ACTOR_120500_PLAYER_REQUEST_NONE;
    work->bodyRequest   = ACTOR_120500_BODY_REQUEST_NONE;
    work->screenRequest = ACTOR_120500_SCREEN_REQUEST_NONE;
    ACTOR_120500_INSTALL_PLAYER_ANIMATION(ACTOR_120500_SKIP_PLAYER_ANIMATION);
    work = task->work;
    worldCoordSetAmbientColorOverride(NULL);
    taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_120500_801380A8, 0);
#undef ACTOR_120500_INSTALL_PLAYER_ANIMATION
}

/// Sets the cutscene double's model visibility and automatic-buffer policy.
///
/// Requires a live model; the message ID and second payload are ignored.
/// drawMode is ACTOR_MESSAGE_DRAW_HIDE (0), SHOW (1), or HIDE_SKIP_AUTO_BUFFER
/// (2). Unsupported values do nothing. Preserves unrelated model flags and
/// returns no usable message result; callers must ignore the dispatch result.
static void _actor120500SetModelDraw(Task* task, s32 unusedMessageId, s32 drawMode, s32 unusedArg)
{
    TmdObject* model;

    model = task->extra.tmd;
    switch (drawMode) {
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            model->flags = model->flags | TMD_OBJECT_SKIP_AUTO_BUFFER;
            /* fallthrough */
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags = model->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
    }
}

#include "../../shared/actor_messages_place_in_view.inc.c"
