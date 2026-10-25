#include "actors/actor_120500.h"

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
#include "gameplay/captions.h"
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
/// Binds the shared fade-in declaration and definition to this file's private task.
///
/// Requires the bare identifier of the preceding static `void (Task*)` callback.
/// Keep defined from `screen_fade.h` through `screen_fade_in.inc.c`, then undefine.
/// This object-like binding has no arguments, captures or token construction.
#define SCREEN_FADE_IN_TASK _screenFadeInTask
#include "../../shared/screen/screen_fade.h"
#include "../../shared/actor/actor_messages.h"

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

static void _actor120500PostPlayerRequest(s16 requestId);
static void _actor120500PostBodyRequest(s16 requestId);
static void _actor120500PostScreenRequest(s16 requestId);
static void _actor120500SkipScene(void);

extern TmdSource gKyleBody;
static void      _actor120500MovieTask(Task* task);
static void      _actor120500SceneTask(Task* task);
static void      _actor120500SetModelDraw(Task* task, s32 unusedMessageId, s32 drawMode, s32 unusedArg);

#include "assets/characters/kyle/body.inc"

#include "assets/rooms/dryfield/motel_room_6/kyle_clips.inc"

/// Player choreography clips for the motel room 6 movie scene, indexed 0..2.
///
/// Clip 0 is held at the movie placement, 1 starts after the movie, and 2
/// finishes the scene or restores its skipped result. Install requests lend
/// this table to the player until its weapon bank is restored. Keep this
/// actor package loaded through that playback; there is no NULL terminator.
#include "assets/rooms/dryfield/motel_room_6/player_scene_bank.inc"

/// Motel scene Kyle Madigan body clips: 0 unused, 1 the view-parented body pose.
///
/// Body contexts borrow this table and its clip for the model's lifetime;
/// all nineteen non-root tracks start on clip 1.
#include "assets/rooms/dryfield/motel_room_6/kyle_bank.inc"

/// Player placement held during the motel scene's first animation, before the movie.
///
/// XYZ are room-coordinate units; yaw is seven eighths of a turn, in
/// ACTOR_TRANSFORM_ANGLE_TURN units. Placement dispatch borrows this record
/// synchronously and copies its transform into the player's root.
static ActorTransform _gActor120500PlayerMoviePlacement = { { 6330, -3200, -4900, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN * 7 / 8, 0, 0 } };

/// Player placement after the motel movie, also used to complete a skipped scene.
///
/// XYZ are room-coordinate units; yaw is seven eighths of a turn, in
/// ACTOR_TRANSFORM_ANGLE_TURN units. Placement dispatch borrows this record
/// synchronously and copies its transform into the player's root.
static ActorTransform _gActor120500PlayerAfterMoviePlacement = { { 787, 0, 7000, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN * 7 / 8, 0, 0 } };

/// Kyle Madigan double's view-space placement when the motel movie reveals his body.
///
/// XYZ are coordinate units relative to the view; yaw is a half turn.
/// Placement dispatch borrows the record synchronously and reparents the root
/// to the view before applying it. The body does not use room-space placement.
static ActorTransform _gActor120500KyleMadiganViewPlacement = { { 800, -12000, -2750, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

enum {
    ACTOR_120500_MOTEL_CAP_COMMAND         = 8,
    ACTOR_120500_SCENE_HANDOFF_WAIT_FRAMES = 3,
    ACTOR_120500_SKIP_FINAL_VIEW           = 5,
    ACTOR_120500_SKIP_FADE_WAIT_FRAMES     = 8,
    ACTOR_120500_SKIP_VIEW_WAIT_FRAMES     = 2,
};

/// CAP-cued player, Kyle-body and movie choreography for motel room 6.
///
/// Starts CAP command 8, posts requests to the published body task around
/// cue waits and the movie, restores the player's weapon animation, then ends
/// scripted control. END terminates the 24 records. The event runner borrows
/// this script and its callbacks; keep the actor, room and CAP assets loaded
/// through completion or the paired skip script.
static EvsCommand _gActor120500MotelSceneScript[24] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_120500_MOTEL_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120500PostPlayerRequest }, { .value = ACTOR_120500_PLAYER_REQUEST_FIRST_ANIMATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120500PostScreenRequest }, { .value = ACTOR_120500_SCREEN_REQUEST_FADE_OUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor120500PostScreenRequest }, { .value = ACTOR_120500_SCREEN_REQUEST_PLAY_MOVIE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_120500_SCENE_HANDOFF_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_120500_SCENE_HANDOFF_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Motel scene's skip sequence, restoring the player and final view under a fade.
///
/// Waits eight frames after fade start, applies the skipped choreography result,
/// selects view 5, waits two frames before scene cleanup, then returns the fade
/// and scripted player control. END terminates the ten records. The runner
/// borrows this script and the live body task until cleanup finishes.
static EvsCommand _gActor120500MotelSceneSkipScript[10] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_120500_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor120500SkipScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_120500_SKIP_FINAL_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_120500_SKIP_VIEW_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_120500_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Motel Kyle body's supported placement and draw-mode messages.
///
/// Dispatch only ACTOR_MESSAGE_SET_MODEL_DRAW or ACTOR_MESSAGE_PLACE: both
/// entries are live callbacks, with no TASK_MESSAGE_TABLE_END. Placement borrows
/// an ActorTransform during dispatch and interprets it in view coordinates.
/// Draw mode takes ACTOR_MESSAGE_DRAW_*; neither message has a usable result.
static TaskMessageEntry _gActor120500KyleMadiganMessages[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor120500SetModelDraw },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceInView },
};

/// Bodyless motel scene recipes: 0 movie, 1 fade from black, 2 fade to black.
///
/// All run at priority `TASK_PRIORITY_CONTROL`; indices 0..2 have no terminator. Movie arguments
/// are unused and playback requires the room's loaded view-100 stream. Fade
/// argument 1 is an unsigned-halfword intensity step per update, normally 8;
/// argument 2 is unused. Keep the actor package loaded until each task ends.
static TaskDesc _gActor120500MovieFadeTaskDescriptors[3] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor120500MovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _screenFadeInTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _screenFadeOutTask, { .value = 0 } },
};

TaskDesc gActor120500EmptyPlacementTaskDesc[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, taskKill, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor120500SceneTask, { .model = &gKyleBody } },
};

/// Borrowed Kyle Madigan body task for the motel-room movie's request channels.
///
/// Successful body setup publishes the singleton for event setters and skip
/// handling. Those consumers require its initialized work to remain live;
/// teardown does not clear the cache and a later spawn replaces it.
static Task* _gActor120500SceneBodyTask = NULL;

static void _actor120500RunPlayerRequest(Task* task);
static void _actor120500InitBody(Task* task);

/// Plays the motel-room movie, then restores game resources and presentation.
///
/// Start at state 0 with the current location's view-100 movie loaded in the
/// stream table (slot 0..14). CD enqueue copies four argument bytes; only the
/// slot byte is initialized and the movie ignores the other three. START cancels
/// playback; both completion paths wait for the queue to drain before restoration.
/// Owns no work block. After restoration, starts an eight-unit fade, clears the
/// image workspace and resumes the game loop before ending this callback.
static void _actor120500MovieTask(Task* task)
{
    enum {
        ACTOR_120500_MOVIE_PREPARE    = 0,
        ACTOR_120500_MOVIE_QUEUE      = 1,
        ACTOR_120500_MOVIE_WAIT_READY = 2,
        ACTOR_120500_MOVIE_PLAY       = 3,
        ACTOR_120500_MOVIE_WAIT_STOP  = 4,
        ACTOR_120500_MOVIE_RESTORE    = 5,
        ACTOR_120500_MOVIE_VIEW       = 100,
        ACTOR_120500_MOVIE_FADE_TASK  = 1,
        ACTOR_120500_MOVIE_FADE_STEP  = 8,
    };
    u8              movieArgument[4];
    GameLocationKey movieLocation;
    CdCmdQueue*     queue;
    s16             movieSlot;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case ACTOR_120500_MOVIE_PREPARE:
            SetDispMask(0);
            streamPrepareMovieWorkspace(true);
            task->state = task->state + 1;
            return;
        case ACTOR_120500_MOVIE_QUEUE:
            movieLocation      = gGameSession->location;
            movieLocation.view = ACTOR_120500_MOVIE_VIEW;
            movieSlot          = streamFindMovieSlot(&movieLocation, 0, 0);
            movieArgument[0]   = movieSlot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, movieArgument);
            task->state = task->state + 1;
            return;
        case ACTOR_120500_MOVIE_WAIT_READY:
            if (queue->movieReady == 0) {
                return;
            }
            sndEvtRequestScriptStart(SOUND_MOTEL_ROOM_6_MOVIE_SFX, 0, 0);
            SetDispMask(1);
            task->state = task->state + 1;
            return;
        case ACTOR_120500_MOVIE_PLAY:
            if (cdCmdIsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                return;
            }
            if (padIsStartPressed() == 0) {
                return;
            }
            SetDispMask(0);
            cdCmdRequestCancel();
            task->state = task->state + 1;
            return;
        case ACTOR_120500_MOVIE_WAIT_STOP:
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                return;
            }
            streamResetGameRestore();
            task->state = task->state + 1;
            return;
        case ACTOR_120500_MOVIE_RESTORE:
            if ((streamPollGameRestore(false, true) & 0xFFFF) == 0) {
                return;
            }
            taskSpawnFromTableOnDefaultList(_gActor120500MovieFadeTaskDescriptors, ACTOR_120500_MOVIE_FADE_TASK, ACTOR_120500_MOVIE_FADE_STEP, 0);
            memFillBytes(gFsImageWorkspacePtr, 0, sizeof(*gFsImageWorkspacePtr));
            SetDispMask(1);
            taskKill(task);
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
/// Restores equipped-weapon clip 1 immediately with world collision disabled.
///
/// Captures live work->playerTask, writable request/weaponId/animationBank locals,
/// the local character/bank/clip constants, and the live weapon/save state.
/// Character 1 with weapon 0..32 selects valid banks 1..33. The retained alternate
/// +34 selector has no established backing in the current bank table. The loaded
/// weapon resources must outlive playback; dispatch borrows request synchronously.
/// Expands to a compound statement and is undefined after this consumer.
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
                        request.source.sets                  = _gActor120500PlayerSceneAnimationSets;
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
                                                  &_gActor120500PlayerMoviePlacement, 0);
                    return;
            }
            return;
        case ACTOR_120500_PLAYER_REQUEST_SECOND_ANIMATION:
            taskSpawnFromTable(_gActor120500MovieFadeTaskDescriptors, ACTOR_120500_FADE_IN_TASK, ACTOR_120500_FADE_INTENSITY_STEP, 0);
            reloadedWork = task->work;
            worldCoordSetAmbientColorOverride(NULL);
            taskMessageDispatch(reloadedWork->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            TASK_MESSAGE_DISPATCH_POINTER(reloadedWork->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor120500PlayerAfterMoviePlacement, 0);
            animWork       = task->work;
            requestPointer = &request;
            if (animWork->playerTask != NULL) {
                request.source.sets                  = _gActor120500PlayerSceneAnimationSets;
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
                request.source.sets                  = _gActor120500PlayerSceneAnimationSets;
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
static inline void _actor120500ResetBodyTracks(_Actor120500Work* work)
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
    work->playerTask           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    _gActor120500SceneBodyTask = task;
    coord->parent              = &gGfxViewCoord;
    tmd->lightMtx              = &work->light;
    tmd->flags                 = 0;
    tmd->colorMtx              = &work->color;
    place                      = areaGetVariant(&gGameSession->location)->placements;
    entryId                    = place->entryId;
    while (entryId != AREA_PLACEMENT_END) {
        if (entryId == ACTOR_120500_BODY_TEXTURE_ENTRY_ID) {
            break;
        }
        place++;
        entryId = place->entryId;
    }
    tmdSetTextureOffsets(tmd, place->texturePageOffset, place->clutRowOffset);
    animationInitContext(&work->rig.anim, _gActor120500KyleMadiganAnimationSets, tmd, work->rig.poses, work->rig.slots);
    slotsWork      = task->work;
    task->msgTable = _gActor120500KyleMadiganMessages;
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
            if (gAttachmentState.mode != ATTACHMENT_MODE_WHEEL && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {

                _actor120500InitBody(task);
                _actor120500BlendPlayerWeaponPose();
                gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, ACTOR_120500_SCENE_OBJECTIVE);
                evsStartScriptWithSkip(_gActor120500MotelSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor120500MotelSceneSkipScript);
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
            taskSpawnFromTable(_gActor120500MovieFadeTaskDescriptors, ACTOR_120500_FADE_IN_DESCRIPTOR, ACTOR_120500_FADE_STEP, 0);
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor120500KyleMadiganViewPlacement, 0);
        }
    }
    work->bodyRequest = ACTOR_120500_BODY_REQUEST_NONE;

    screenWork = task->work;
    switch (screenWork->screenRequest) {
        case ACTOR_120500_SCREEN_REQUEST_FADE_OUT:
            taskSpawnFromTable(_gActor120500MovieFadeTaskDescriptors, ACTOR_120500_FADE_OUT_DESCRIPTOR, ACTOR_120500_FADE_STEP, 0);
            break;
        case ACTOR_120500_SCREEN_REQUEST_PLAY_MOVIE:
            taskMessageDispatch(screenWork->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            displaySpawnTaskFromTable(_gActor120500MovieFadeTaskDescriptors, 0, 0, 0);
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

#include "../../shared/screen/screen_fade_in.inc.c"
#undef SCREEN_FADE_IN_TASK

#include "../../shared/screen/screen_fade_out.inc.c"

/// Posts a player-choreography request for the motel room 6 scene.
///
/// Requires the published body task and initialized work to remain live.
/// Stores requestId's halfword bits in the unsigned request channel and resets
/// its step. ACTOR_120500_PLAYER_REQUEST_* selects the scene beats; zero cancels
/// a pending request and other codes are consumed without effect. The tick
/// performs the request later; this callback sends no player message.
static void _actor120500PostPlayerRequest(s16 requestId)
{
    _Actor120500Work* work = _gActor120500SceneBodyTask->work;

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
    _Actor120500Work* work = _gActor120500SceneBodyTask->work;

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
    _Actor120500Work* work = _gActor120500SceneBodyTask->work;

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
/// Reloads scene work and restarts the player's selected scene clip when present.
///
/// Requires a live Task* and writable _Actor120500Work* playerWork lvalue, plus
/// AnimationPlayRequest storage. Arguments must have no side effects: actorTask
/// occurs once, playerWork three times, playRequest six times when a player is
/// present, and requestedAnimation once only on that path. Sets remain loaded
/// through playback; dispatch borrows the request synchronously. Enables collision,
/// captures only the package animation table, and forms one compound statement.
#define ACTOR_120500_INSTALL_PLAYER_ANIMATION(actorTask, playerWork, playRequest, requestedAnimation)                       \
    {                                                                                                                       \
        (playerWork) = (actorTask)->work;                                                                                   \
        if ((playerWork)->playerTask != NULL) {                                                                             \
            (playRequest).source.sets          = _gActor120500PlayerSceneAnimationSets;                                     \
            (playRequest).animationId          = (requestedAnimation);                                                      \
            (playRequest).blend                = ANIMATION_BLEND_RESET;                                                     \
            (playRequest).blendFrames          = 0;                                                                         \
            (playRequest).enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;                                          \
            TASK_MESSAGE_DISPATCH_POINTER((playerWork)->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &(playRequest), 0); \
        }                                                                                                                   \
    }

    enum { ACTOR_120500_SKIP_SOUND_FADE_UPDATES = 10,
           ACTOR_120500_SKIP_PLAYER_ANIMATION   = 2 };

    Task*                task;
    _Actor120500Work*    work;
    _Actor120500Work*    animWork;
    AnimationPlayRequest request;

    task = _gActor120500SceneBodyTask;
    work = task->work;
    sndEvtRequestScriptStop(SOUND_MOTEL_ROOM_6_MOVIE_SFX, ACTOR_120500_SKIP_SOUND_FADE_UPDATES);
    taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
    work->playerRequest = ACTOR_120500_PLAYER_REQUEST_NONE;
    work->bodyRequest   = ACTOR_120500_BODY_REQUEST_NONE;
    work->screenRequest = ACTOR_120500_SCREEN_REQUEST_NONE;
    ACTOR_120500_INSTALL_PLAYER_ANIMATION(task, animWork, request, ACTOR_120500_SKIP_PLAYER_ANIMATION);
    work = task->work;
    worldCoordSetAmbientColorOverride(NULL);
    taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &_gActor120500PlayerAfterMoviePlacement, 0);
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

#include "../../shared/actor/actor_messages_place_in_view.inc.c"
