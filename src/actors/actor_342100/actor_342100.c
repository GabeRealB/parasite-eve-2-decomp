#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_dumping_hole.h"
#include "../../shared/screen_wave.h"
#include "../../shared/incinerator_blaze.h"

/// Progress of the burn scene, held in `_Actor342100BlazeWork::sceneState`.
enum {
    ACTOR_342100_BLAZE_SCENE_START   = 0, // Not started; the next update installs the clips and the event script
    ACTOR_342100_BLAZE_SCENE_RUNNING = 1, // Event script playing; the scene ends when the session's event state clears
};

/// Weapon-bank selectors computed from the saved character and equipped weapon.
enum {
    ACTOR_342100_BLAZE_PRIMARY_CHARACTER      = 1,
    ACTOR_342100_BLAZE_PRIMARY_BANK_OFFSET    = 1,
    ACTOR_342100_BLAZE_OTHER_CHARACTER_OFFSET = 0x22,
};

/// Fade phases selected by this package's burn script.
enum {
    ACTOR_342100_BLAZE_FADE_FAST_RED = 2,
    ACTOR_342100_BLAZE_FADE_SLOW_RED = 3,
    ACTOR_342100_BLAZE_FADE_TO_WHITE = 4,
};

/// Work block of the package's blaze controller, the task that burns the
/// player once the dumping hole's scene clock has run out with the player
/// still alive.
///
/// The controller allocates the block zeroed on its first frame and publishes
/// itself, so the event script's callbacks reach the block through the task.
/// It opens with the head the shared blaze tasks expect of their spawner.
/// The incinerator room stages the same scene with a smaller block that has
/// no encounter task and places the later members differently, so the two
/// stay separate types.
typedef struct {
    BlazeParentWork blaze;           // Head the fade task reaches through the controller: the heat-haze screen wave's ramp context
    Task*           playerTask;      // Player task, the receiver of the scene's animation messages
    Task*           encounterTask;   // The room's enemy-wave controller, told to stop spawning as the blaze starts; NULL when this block recorded none
    Task*           fadeTask;        // Screen fade task, sent the state of each colour ramp by message
    Task*           bodyFireTask;    // Task spawning fire on the player's model; its spawn argument is set to 1 to spread the fire over the whole body
    s16             animationId;     // Bank index of the scene clip last played on the player (ANIMATION_BANK_BASE_SET_COUNT + clip)
    s16             sceneState;      // Burn scene progress (ACTOR_342100_BLAZE_SCENE_START or _RUNNING)
    byte            unknown_40[0x4]; // Never accessed; role unproven
} _Actor342100BlazeWork;
STATIC_ASSERT_SIZEOF(_Actor342100BlazeWork, 0x44);

/// The overlay's event/controller task, published by
/// `_actor342100EncounterBlazeControllerTask`.
extern Task* D_actor_342100_80164BB8;

/// Single-entry spawn table `_actor342100SetBlazeBodyFireMode` starts as entry 3.
extern TaskDesc D_actor_342100_80164B78[];

/// Records a burn clip and sends its synchronous, collision-disabled play request.
///
/// `workValue` is a live controller work pointer; `requestValue` is a writable
/// local AnimationPlayRequest. Arguments must be stable and side-effect-free: `workValue`
/// and `animationIdValue` are evaluated twice, and `requestValue` six times.
/// The receiver borrows the request only until dispatch returns. This compound
/// statement captures no caller identifiers and produces no result.
#define ACTOR_342100_SEND_BLAZE_ANIMATION(workValue, requestValue, animationIdValue, bankIndexValue, blendFramesValue) \
    {                                                                                                                  \
        (requestValue).source.index         = (bankIndexValue);                                                        \
        (workValue)->animationId            = (animationIdValue);                                                      \
        (requestValue).animationId          = (animationIdValue);                                                      \
        (requestValue).blend                = ANIMATION_BLEND_INTERPOLATE;                                             \
        (requestValue).blendFrames          = (blendFramesValue);                                                      \
        (requestValue).enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                       \
        TASK_MESSAGE_DISPATCH_POINTER((workValue)->playerTask, ANIMATION_MESSAGE_PLAY, &(requestValue), 0);            \
    }

static s32 _actor342100HandleBlazeFadeState(Task* receiver, s32 unusedMessageId, s32 fadeState, s32 unusedSecondArg);

static void _actor342100PlayBlazeAnimation(s32 clipIndex);

static void _actor342100SetBlazeFadeState(s32 fadeState);

static void _actor342100StartBlazeHeatHaze(void);

static void _actor342100SetBlazeBodyFireMode(s32 spreadToWholeBody);

static void _actor342100TriggerBlazeDeath(void);

/// Main-executable global with no module header yet: the remaining-enemy count.

/// Single-entry spawn table of the screen-wave task
/// `_screenWaveGridTask`: `_actor342100StartBlazeHeatHaze` starts entry 0
/// and hands it the address of the work block's `blaze.wave` as its ramp.
extern TaskDesc D_actor_342100_801648DC[];

/// Null-terminated table of the overlay's per-state message tables, counted
/// and reported by `_actor342100UpdateBlazeScene` when it arms the encounter:
/// three live entries and the null word that ends them.
extern AnimationSet* D_actor_342100_80164900[4];

/// Animation step table `_actor342100AdvanceBlazeAnimation` walks: `s16` entries
/// holding the clip one step on from the work block's `animationId`, both less
/// `ANIMATION_BANK_BASE_SET_COUNT`; the first three entries are `-1`, which ends
/// the chain, and only the fourth is live. Sits directly after
/// `D_actor_342100_80164900`'s null word, and its first element is the address
/// `_actor342100UpdateBlazeScene`'s encounter table of a different size would
/// have started at, so splat cut it out as a symbol of its own.
extern s16 D_actor_342100_80164910[];

/// Placement tables the overlay's spawn task picks between by
/// `gGameSession->location.loc.view`: 0x1D, 0x1E, 0x1F, 0x23 and 0x24 select the 0x80164930
/// / 0x80164918 / 0x80164948 / 0x80164960 / 0x80164980 table respectively, and
/// the values in between select none. Each is a zero-`vx`-terminated `SVECTOR`
/// list of two to three placements -- the terminator is an all-zero entry -- and
/// `_actor342100SpawnBlazeFireEmitters` drops one effect task on every live entry.
extern SVECTOR D_actor_342100_80164918[];
extern SVECTOR D_actor_342100_80164930[];
extern SVECTOR D_actor_342100_80164948[];
extern SVECTOR D_actor_342100_80164960[];
extern SVECTOR D_actor_342100_80164980[];

/// Model/animation set `_actor342100UpdateBlazeScene` installs with
/// `evsStartScript` on the same arm; a byte address is all the installer sees.
extern EvsCommand D_actor_342100_801649C8[];

/// Reusable hit-effect argument for fire on a selected player-model part.
/// `coord` borrows that part's coordinate and `spawnArgLo` carries its fire size;
/// `spawnArgHi` remains the initial repeat count 1.
extern EffectSpawnArg gBlazeFireSpawn;

/// The player-model parts the effect record above is aimed at, as indices into
/// the player's coordinate array (`TmdObject::coords`): sixteen `u16`s
/// running 1..0x12, of which `_blazeBodyFireTask` takes the first four
/// (2, 4, 6, 0xA) when it masks the LCG draw with 3 and all sixteen when it
/// masks with 0xF.
extern u16 gBlazePlayerParts[];

/// Distortion amplitude of the screen wave, `frame * scale / span` of the
/// running ramp, recomputed every frame.
extern s32 gScreenWaveRamp;

/// The ramp the running wave task was spawned with, parked at spawn so the
/// tick reads it back every frame.
extern ScreenWaveCtx* gScreenWaveCtx;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed on every frame
/// `gSceneCombatState.actorControl` is clear.
extern ScreenWaveGridOscillator gScreenWaveColumns[10];
extern ScreenWaveGridOscillator gScreenWaveRows[30];

/// Double-buffered 8 by 30 meshes of textured quads, one mesh per frame
/// buffer. `_screenWaveGridTask` builds them once and moves their corners.
extern POLY_FT4 gScreenWaveGrid[2][30][8];

static void _actor342100BlazeFireEmitterTask(Task* task);
static void _actor342100SpawnBlazeFireEmitters(void);
static void _actor342100EncounterBlazeControllerTask(Task* task);

static AnimationPackedPose _gActor342100Animation019F0Bank1[6] = {
#include "assets/actor_342100_animation_019F0_bank1.inc"
};

static AnimationPackedRotation _gActor342100Animation019F0Bank4[46] = {
#include "assets/actor_342100_animation_019F0_bank4.inc"
};

static AnimationRecord _gActor342100Animation019F0Records[109] = {
#include "assets/actor_342100_animation_019F0_records.inc"
};

static u16 _gActor342100Animation019F0Indices[20] = {
#include "assets/actor_342100_animation_019F0_indices.inc"
};

static AnimationSet _gActor342100Animation019F0 = {
    _gActor342100Animation019F0Records,
    _gActor342100Animation019F0Indices,
    { NULL, _gActor342100Animation019F0Bank1, NULL, NULL, _gActor342100Animation019F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342100Animation0242CBank1[16] = {
#include "assets/actor_342100_animation_0242C_bank1.inc"
};

static AnimationPackedRotation _gActor342100Animation0242CBank4[246] = {
#include "assets/actor_342100_animation_0242C_bank4.inc"
};

static AnimationRecord _gActor342100Animation0242CRecords[341] = {
#include "assets/actor_342100_animation_0242C_records.inc"
};

static u16 _gActor342100Animation0242CIndices[20] = {
#include "assets/actor_342100_animation_0242C_indices.inc"
};

static AnimationSet _gActor342100Animation0242C = {
    _gActor342100Animation0242CRecords,
    _gActor342100Animation0242CIndices,
    { NULL, _gActor342100Animation0242CBank1, NULL, NULL, _gActor342100Animation0242CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor342100Animation02A94Bank1[14] = {
#include "assets/actor_342100_animation_02A94_bank1.inc"
};

static AnimationPackedRotation _gActor342100Animation02A94Bank4[148] = {
#include "assets/actor_342100_animation_02A94_bank4.inc"
};

static AnimationRecord _gActor342100Animation02A94Records[200] = {
#include "assets/actor_342100_animation_02A94_records.inc"
};

static u16 _gActor342100Animation02A94Indices[20] = {
#include "assets/actor_342100_animation_02A94_indices.inc"
};

static AnimationSet _gActor342100Animation02A94 = {
    _gActor342100Animation02A94Records,
    _gActor342100Animation02A94Indices,
    { NULL, _gActor342100Animation02A94Bank1, NULL, NULL, _gActor342100Animation02A94Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_342100_801648DC[2] = {
    { { { TASK_BODY_NONE, 192 } }, _screenWaveGridTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

TaskMessageEntry gBlazeFadeMessages[1] = {
    { BLAZE_FADE_MESSAGE_SET_STATE, _actor342100HandleBlazeFadeState },
};

AnimationSet* D_actor_342100_80164900[4] = {
    &_gActor342100Animation019F0,
    &_gActor342100Animation02A94,
    &_gActor342100Animation0242C,
    NULL,
};

s16 D_actor_342100_80164910[4] = {
    -1,
    -1,
    -1,
    0,
};

SVECTOR D_actor_342100_80164918[3] = {
    { 0x4650, -100, -2500, 0 },
    { 0x4650, -700, -8700, 0 },
    { 0, 0, 0, 0 },
};

SVECTOR D_actor_342100_80164930[3] = {
    { 0x4268, -600, -1500, 0 },
    { 0x4268, -100, -0x2710, 0 },
    { 0, 0, 0, 0 },
};

SVECTOR D_actor_342100_80164948[3] = {
    { 9000, -100, -2500, 0 },
    { 5000, -100, -2500, 0 },
    { 0x2710, 0, -0x2710, 0 },
};

SVECTOR D_actor_342100_80164960[4] = {
    { 0x2710, -200, -9000, 0 },
    { 0x2710, -200, -2000, 0 },
    { 6000, -200, -3000, 0 },
    { 0, 0, 0, 0 },
};

SVECTOR D_actor_342100_80164980[4] = {
    { 3000, -200, -5000, 0 },
    { 5000, -200, -7500, 0 },
    { 3000, -200, -8000, 0 },
    { 0, 0, 0, 0 },
};

EffectSpawnArg gBlazeFireSpawn = { NULL, 0, 1 };

u16 gBlazePlayerParts[16] = {
    2,
    4,
    6,
    10,
    1,
    3,
    5,
    7,
    8,
    9,
    11,
    12,
    13,
    15,
    16,
    18,
};

EvsCommand D_actor_342100_801649C8[18] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100PlayBlazeAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100SetBlazeBodyFireMode }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100SetBlazeFadeState }, { .value = ACTOR_342100_BLAZE_FADE_FAST_RED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100PlayBlazeAnimation }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342100SpawnBlazeFireEmitters }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342100StartBlazeHeatHaze }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100PlayBlazeAnimation }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100SetBlazeBodyFireMode }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 75 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100SetBlazeFadeState }, { .value = ACTOR_342100_BLAZE_FADE_SLOW_RED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100SetBlazeFadeState }, { .value = ACTOR_342100_BLAZE_FADE_TO_WHITE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342100TriggerBlazeDeath }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_342100_80164B78[5] = {
    { { { TASK_BODY_NONE, 192 } }, _actor342100EncounterBlazeControllerTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _blazeFadeTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _blazeBodyFireTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, _actor342100BlazeFireEmitterTask, { .value = 0 } },
};

ScreenWaveCtx* gScreenWaveCtx = NULL;

Task* D_actor_342100_80164BB8 = NULL;

// Nine active columns and one retained zero entry.
ScreenWaveGridOscillator gScreenWaveColumns[10] = { 0 };

ScreenWaveGridOscillator gScreenWaveRows[30] = { 0 };

POLY_FT4 gScreenWaveGrid[2][30][8] = { 0 };

static s32 _actor342100UpdateBlazeScene(Task* controller);

#include "../../shared/screen_wave_grid.inc.c"

#include "../../shared/incinerator_blaze_fade.inc.c"

/// Starts a burn clip's successor once the player's animation has settled.
///
/// The controller and any non-NULL player task must remain live. An extension
/// animation ID must index `D_actor_342100_80164910` after subtracting
/// `ANIMATION_BANK_BASE_SET_COUNT`; a negative entry has no successor. The
/// script supplies clips 0..2, whose entries all terminate. Successors blend
/// over ten frames with world collision disabled; their clip data stays loaded.
/// Returns 0 while the player is still animating, otherwise 1, including when
/// a successor was just requested or no player task is recorded. As with
/// `_actor342100PlayBlazeAnimation`, bank selection for other character IDs
/// has an unproven valid index domain.
static s32 _actor342100AdvanceBlazeAnimation(Task* controller)
{
    enum { ACTOR_342100_BLAZE_CHAIN_BLEND_FRAMES = 10 };
    _Actor342100BlazeWork* work;
    _Actor342100BlazeWork* dispatchWork;
    AnimationPlayRequest   request;
    s16                    animationId;
    s32                    weaponId;
    s32                    bankIndex;

    work = controller->work;
    if (work->playerTask == NULL) {
        return 1;
    }
    // A clip's boundary pose must settle before requesting its successor.
    if (taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        if (work->animationId >= ANIMATION_BANK_BASE_SET_COUNT) {
            if (D_actor_342100_80164910[work->animationId - ANIMATION_BANK_BASE_SET_COUNT] >= 0) {
                animationId  = D_actor_342100_80164910[work->animationId - ANIMATION_BANK_BASE_SET_COUNT] + ANIMATION_BANK_BASE_SET_COUNT;
                dispatchWork = controller->work;
                weaponId     = gPlayerStatus.weapon;
                bankIndex    = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_342100_BLAZE_PRIMARY_CHARACTER) ? weaponId + ACTOR_342100_BLAZE_PRIMARY_BANK_OFFSET : weaponId + ACTOR_342100_BLAZE_OTHER_CHARACTER_OFFSET;
                ACTOR_342100_SEND_BLAZE_ANIMATION(dispatchWork, request, animationId, bankIndex, ACTOR_342100_BLAZE_CHAIN_BLEND_FRAMES);
            }
        }
        return 1;
    }
    return 0;
}

/// Chooses one blaze emitter's local fire offset from five successive LCG draws.
///
/// offsetValue is a stable, side-effect-free pointer to writable SVECTOR storage;
/// it is evaluated three times. Writes XYZ only, with X/Z in -63..63 and Y in
/// -63..0; leaves vector metadata untouched and retains the fifth LCG state.
/// Temporaries are local to the compound statement; no caller locals are captured.
#define ACTOR_342100_CHOOSE_BLAZE_FIRE_OFFSET(offsetValue)                                       \
    {                                                                                            \
        enum { ACTOR_342100_FIRE_EMITTER_OFFSET_MASK = 63 };                                     \
        s32 xMagnitudeRoll, zMagnitudeRoll, xOffset, zOffset;                                    \
        xMagnitudeRoll  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;        \
        xOffset         = ((u32)xMagnitudeRoll >> 16) & ACTOR_342100_FIRE_EMITTER_OFFSET_MASK;   \
        gRandomLcgState = xMagnitudeRoll * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;         \
        if ((gRandomLcgState >> 16) & 1) {                                                       \
            xOffset = -xOffset;                                                                  \
        }                                                                                        \
        gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;      \
        (offsetValue)->vx = xOffset;                                                             \
        (offsetValue)->vy = -((gRandomLcgState >> 16) & ACTOR_342100_FIRE_EMITTER_OFFSET_MASK);  \
        zMagnitudeRoll    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;      \
        zOffset           = ((u32)zMagnitudeRoll >> 16) & ACTOR_342100_FIRE_EMITTER_OFFSET_MASK; \
        gRandomLcgState   = zMagnitudeRoll * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;       \
        if ((gRandomLcgState >> 16) & 1) {                                                       \
            zOffset = -zOffset;                                                                  \
        }                                                                                        \
        (offsetValue)->vz = zOffset;                                                             \
    }

/// Emits scattered fire from one fixed coordinate during the dumping-hole blaze.
///
/// Entry owns a coordinate body and starts at state 0. Allocates an EffectSpawnArg
/// until task teardown; allocation failure kills the task and returns immediately.
/// spawnArg1 is a nonnegative delay in callback updates (the script's spawner uses
/// 0..31). After the delay, emits one size-256 blast every sixteen display frames.
/// Local XYZ offsets are -63..63, with Y restricted to -63..0. Each emission
/// consumes five successive shared LCG draws and retains the final state.
/// The coordinate must outlive effects that borrow it; this task stays active
/// until external teardown and does not gate emission on the combat pause state.
static void _actor342100BlazeFireEmitterTask(Task* task)
{
    enum {
        ACTOR_342100_FIRE_EMITTER_INITIALIZE     = 0,
        ACTOR_342100_FIRE_EMITTER_DELAY          = 1,
        ACTOR_342100_FIRE_EMITTER_EMIT           = 2,
        ACTOR_342100_FIRE_EMITTER_SIZE           = 256,
        ACTOR_342100_FIRE_EMITTER_REPEAT_COUNT   = 1,
        ACTOR_342100_FIRE_EMITTER_DISPLAY_PERIOD = 16,
    };
    EffectSpawnArg* spawnRecord;
    GfxCoord*       coord;
    SVECTOR         localOffset;

    spawnRecord = task->work;
    coord       = task->extra.coordBody->coord;
    switch (task->state) {
        case ACTOR_342100_FIRE_EMITTER_INITIALIZE:
            task->work = memMalloc(sizeof(*spawnRecord), false);
            if (task->work == NULL) {
                taskKill(task);
                return;
            }
            spawnRecord = task->work;
            memFillBytes(spawnRecord, 0, sizeof(*spawnRecord));
            spawnRecord->spawnArgLo = ACTOR_342100_FIRE_EMITTER_SIZE;
            spawnRecord->coord      = task->extra.coordBody->coord;
            spawnRecord->spawnArgHi = ACTOR_342100_FIRE_EMITTER_REPEAT_COUNT;
            task->state++;
            return;
        case ACTOR_342100_FIRE_EMITTER_DELAY:
            if (task->spawnArg1.value <= 0) {
                task->state = ACTOR_342100_FIRE_EMITTER_EMIT;
                return;
            }
            task->spawnArg1.value--;
            return;
        case ACTOR_342100_FIRE_EMITTER_EMIT:
            if (gDisplayState.animFrame & (ACTOR_342100_FIRE_EMITTER_DISPLAY_PERIOD - 1)) {
                return;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            ACTOR_342100_CHOOSE_BLAZE_FIRE_OFFSET(&localOffset);
            effectSpawnHit(EFFECT_HIT_KIND_BLAST, coord, &localOffset, spawnRecord);
            return;
    }
}

#undef ACTOR_342100_CHOOSE_BLAZE_FIRE_OFFSET

/// Places a newly spawned blaze fire emitter in its parent's coordinate frame.
///
/// Replaces its rotation with Q12 identity and copies signed-halfword XYZ into
/// the matrix's full-width translation; placement's pad is unused. The coordinate
/// must already have its parent and a dirty composition stamp from spawning.
/// Retains both, leaves the cached transform untouched and borrows both pointers
/// only through this call.
static inline void _actor342100PlaceBlazeFireEmitter(GfxCoord* coord, const SVECTOR* placement)
{
    gfxSetRotIdentity(&coord->coord);
    coord->coord.t[0] = placement->vx;
    coord->coord.t[1] = placement->vy;
    coord->coord.t[2] = placement->vz;
}

/// Places delayed fire emitters for the dumping-hole blaze's current view.
///
/// The burn script calls this with no payload. Requires view 29, 30, 31, 35 or
/// 36, the corresponding placement data and successful coordinate-body spawns.
/// Walks until X is zero, consuming one shared LCG draw per placement for a
/// 0..31-update delay. Coordinates use the view-parent frame's integer units.
/// Unsupported views retain an uninitialized placement pointer. View 31's
/// sentinel lies beyond its declared array; the complete list extent is unproven.
static void _actor342100SpawnBlazeFireEmitters(void)
{
    enum { ACTOR_342100_BLAZE_FIRE_EMITTER_DESCRIPTOR = 4,
           ACTOR_342100_BLAZE_FIRE_DELAY_MASK         = 31 };
    GfxCoord* coord;
    SVECTOR*  placement;
    Task*     emitter;
    u32       delayRoll;

    switch (gGameSession->location.loc.view) {
        case 29:
            placement = D_actor_342100_80164930;
            break;
        case 30:
            placement = D_actor_342100_80164918;
            break;
        case 31:
            placement = D_actor_342100_80164948;
            break;
        case 35:
            placement = D_actor_342100_80164960;
            break;
        case 36:
            placement = D_actor_342100_80164980;
            break;
    }
    while (placement->vx != 0) {
        delayRoll       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = delayRoll;
        emitter         = taskSpawnFromTable(D_actor_342100_80164B78, ACTOR_342100_BLAZE_FIRE_EMITTER_DESCRIPTOR, (delayRoll >> 16) & ACTOR_342100_BLAZE_FIRE_DELAY_MASK, 0);
        coord           = emitter->extra.coordBody->coord;
        _actor342100PlaceBlazeFireEmitter(coord, placement);
        placement++;
    }
}

#include "../../shared/incinerator_blaze_body_fire.inc.c"

/// Installs the blaze's three clip pointers in the selected player bank's extension.
///
/// Requires initialized controller work, its live player and the loaded, writable
/// character/weapon bank. Scans the four-entry set table through its NULL terminator,
/// using the counter's low 16 bits as the index and transfer count. Copies three
/// pointer words starting at `ANIMATION_BANK_BASE_SET_COUNT`, excluding NULL;
/// does not start playback. Dispatch borrows the stack request synchronously;
/// the copied set descriptors and clip data must remain loaded during playback.
static inline void _actor342100CopyBlazeAnimationSets(Task* controller)
{
    enum { ACTOR_342100_BLAZE_EXTENSION_COUNT_MASK = 0xFFFF };
    _Actor342100BlazeWork*   work;
    AnimationBankCopyRequest bankCopyRequest;
    s32                      setCount;
    // Copy clip pointers before the script can request any of them.
    work     = controller->work;
    setCount = 0;
    while (D_actor_342100_80164900[setCount & ACTOR_342100_BLAZE_EXTENSION_COUNT_MASK] != NULL) {
        setCount += 1;
    }
    bankCopyRequest.source.sets = D_actor_342100_80164900;
    bankCopyRequest.wordCount   = setCount & ACTOR_342100_BLAZE_EXTENSION_COUNT_MASK;
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &bankCopyRequest, 0);
}

/// Starts the player's blaze scene and reports completion after its event script ends.
///
/// Requires live controller work, player and writable character/weapon animation
/// bank. The three loaded extension sets remain borrowed by the bank for playback;
/// the terminating NULL is excluded from the synchronous three-word transfer.
/// First update takes scripted control, locks attachments, starts the burn script,
/// hides the HUD and spawns the fade with a borrowed controller pointer.
/// Later updates advance settled clips while eventState remains nonzero. Returns
/// 1 once it clears, otherwise 0. The caller retains the controller after completion.
static s32 _actor342100UpdateBlazeScene(Task* controller)
{
    enum { ACTOR_342100_BLAZE_FADE_DESCRIPTOR = 2 };
    _Actor342100BlazeWork* work = controller->work;

    switch (work->sceneState) {
        case ACTOR_342100_BLAZE_SCENE_START:
            _actor342100CopyBlazeAnimationSets(controller);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
            evsStartScript(D_actor_342100_801649C8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            work->fadeTask   = taskSpawnFromTable(D_actor_342100_80164B78, ACTOR_342100_BLAZE_FADE_DESCRIPTOR, 0, controller);
            work->sceneState = work->sceneState + 1;
            break;
        case ACTOR_342100_BLAZE_SCENE_RUNNING:
            if (gGameSession->eventState != 0) {
                break;
            }
            return 1;
    }
    _actor342100AdvanceBlazeAnimation(controller);
    return 0;
}

/// Allocates and clears the blaze controller's owned work, then publishes its handles.
///
/// Allocates the complete work block on the primary heap and attaches it to the
/// task for teardown. On success records the current player and publishes the
/// controller for event callbacks; neither borrowed task handle outlives its task.
/// Failure stores NULL work and kills the task without publishing it. The caller
/// retains its subsequent caption, sound and state operations on that path.
static inline void _actor342100InitializeBlazeController(Task* task)
{
    _Actor342100BlazeWork* allocatedWork;
    allocatedWork = memMalloc(sizeof(*allocatedWork), false);
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        taskKill(task);
    } else {
        memFillBytes(allocatedWork, 0, sizeof(*allocatedWork));
        allocatedWork->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        D_actor_342100_80164BB8   = task;
    }
}

/// Runs the dumping-hole encounter until the clock expires, then stages the player's blaze death.
///
/// Starts in state 0 with the room's caption and enemy-wave descriptors loaded.
/// Scene/menu/combat pause gates stop all updates; setup also waits for the
/// attachment wheel and display transition. Owns a cleared blaze work block and
/// publishes the task for event callbacks. Idle spawn phase waits for room action
/// 1 after flag 0x11E; armed phase starts an encounter immediately. Other phases
/// only wait for the clock. An expired sceneClock and positive player HP begin
/// the burn script; its completion leaves the controller idle until teardown.
/// Allocation failure retains the subsequent caption, sound and state operations.
static void _actor342100EncounterBlazeControllerTask(Task* task)
{
    enum {
        ACTOR_342100_CONTROLLER_INITIALIZE             = 0,
        ACTOR_342100_CONTROLLER_WAIT_ACTION            = 1,
        ACTOR_342100_CONTROLLER_WAIT_CLOCK             = 2,
        ACTOR_342100_CONTROLLER_BLAZE                  = 3,
        ACTOR_342100_CONTROLLER_FINISHED               = 4,
        ACTOR_342100_CONTROLLER_CAPTION_PRIORITY       = 208,
        ACTOR_342100_CONTROLLER_START_ENCOUNTER_ACTION = 1,
    };
    u16                    actionControl;
    u8                     roomActionId;
    u8                     roomActionArgument;
    _Actor342100BlazeWork* entryWork;
    s32                    blazeReady;
    PlayerStatus*          playerStatus;

    entryWork = task->work;
    if (gGameSession->sceneUpdatesPaused != 0 || Gp_StateC08.menuOpen != ATTACHMENT_MENU_CLOSED || gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING || D_80114CF8 != 0) {
        return;
    }
    switch (task->state) {
        case ACTOR_342100_CONTROLLER_INITIALIZE:
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                break;
            }
            _actor342100InitializeBlazeController(task);
            taskSpawnFromTable(D_shelter_b3_dumping_hole_8018B57C, 0, ACTOR_342100_CONTROLLER_CAPTION_PRIORITY, 0);
            sndEvtRequestScriptStart(SOUND_SHELTER_B3_DUMPING_HOLE_ALERT, 0, 0);
            switch (gGameSession->spawnPhase[0]) {
                case GAME_SESSION_SPAWN_IDLE:
                    task->state++;
                    break;
                case GAME_SESSION_SPAWN_ARMED:
                    // Keep the entry pointer: this store precedes its next-tick refresh.
                    entryWork->encounterTask = taskSpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 0, 1, 0);
                default:
                    task->state = ACTOR_342100_CONTROLLER_WAIT_CLOCK;
                    break;
            }
            break;
        case ACTOR_342100_CONTROLLER_WAIT_ACTION:
            if (gameFlagGetNibble(GAME_FLAG_11E) != 0) {
                if (worldCollisionReadActionHit(&actionControl, &roomActionId, &roomActionArgument) != 0 && (actionControl & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM && (s8)roomActionId == ACTOR_342100_CONTROLLER_START_ENCOUNTER_ACTION) {
                    entryWork->encounterTask = taskSpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 0, 0, 0);
                    task->state++;
                }
            }
            playerStatus = &gPlayerStatus;
            if (gGameSession->sceneClock > 0 || playerStatus->hp <= 0) {
                blazeReady = 0;
            } else {
                blazeReady = 1;
            }
            if (blazeReady) {
                task->state = ACTOR_342100_CONTROLLER_BLAZE;
            }
            break;
        case ACTOR_342100_CONTROLLER_WAIT_CLOCK:
            playerStatus = &gPlayerStatus;
            if (gGameSession->sceneClock > 0 || playerStatus->hp <= 0) {
                blazeReady = 0;
            } else {
                blazeReady = 1;
            }
            if (blazeReady) {
                task->state = ACTOR_342100_CONTROLLER_BLAZE;
            }
            break;
        case ACTOR_342100_CONTROLLER_BLAZE:
            if ((s16)_actor342100UpdateBlazeScene(task) != 0) {
                task->state++;
            }
            break;
        case ACTOR_342100_CONTROLLER_FINISHED:
            break;
    }
}

/// Applies the first integer payload of `BLAZE_FADE_MESSAGE_SET_STATE`.
///
/// The receiver is the live fade task. Stores the complete signed state word;
/// the message ID and second payload are ignored. The result is unspecified
/// and must be discarded, as it is by the burn script's sender.
static s32 _actor342100HandleBlazeFadeState(Task* receiver, s32 unusedMessageId, s32 fadeState, s32 unusedSecondArg)
{
    receiver->state = fadeState;
    // Senders discard the result; this callback leaves the return word unspecified.
}

/// Plays one of the burn script's three extension clips on the player.
///
/// `clipIndex` is 0..2 in the script, relative to
/// `ANIMATION_BANK_BASE_SET_COUNT`. The resulting animation ID is narrowed to s16
/// before both recording it and sending the request. The controller, player,
/// selected weapon bank and copied clip data must remain live. Playback uses a
/// fifteen-frame blend and disables world collision. The preserved bank-selector
/// branch for other character IDs has an unproven valid index domain.
static void _actor342100PlayBlazeAnimation(s32 clipIndex)
{
    enum { ACTOR_342100_BLAZE_SCRIPT_BLEND_FRAMES = 15 };
    _Actor342100BlazeWork* work;
    AnimationPlayRequest   request;
    s16                    animationId;
    s32                    weaponId;
    s32                    bankIndex;

    work        = D_actor_342100_80164BB8->work;
    animationId = clipIndex + ANIMATION_BANK_BASE_SET_COUNT;
    weaponId    = gPlayerStatus.weapon;
    bankIndex   = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_342100_BLAZE_PRIMARY_CHARACTER) ? weaponId + ACTOR_342100_BLAZE_PRIMARY_BANK_OFFSET : weaponId + ACTOR_342100_BLAZE_OTHER_CHARACTER_OFFSET;
    ACTOR_342100_SEND_BLAZE_ANIMATION(work, request, animationId, bankIndex, ACTOR_342100_BLAZE_SCRIPT_BLEND_FRAMES);
}

#undef ACTOR_342100_SEND_BLAZE_ANIMATION

/// Selects the active burn fade's next colour ramp from the event script.
///
/// The published controller and its initialized fade task must remain live.
/// The script selects fast red, slow red, then white; the signed state word is
/// forwarded unchanged, with no bounds check and no result consumed.
static void _actor342100SetBlazeFadeState(s32 fadeState)
{
    _Actor342100BlazeWork* work = D_actor_342100_80164BB8->work;

    taskMessageDispatch(work->fadeTask, BLAZE_FADE_MESSAGE_SET_STATE, fadeState, 0);
}

/// Starts the burn scene's captured-frame heat haze with a 600-frame strength ramp.
///
/// Requires a live published blaze controller, loaded grid-wave resources and no
/// other active wave. The task borrows the controller's embedded context through
/// its final draw; peak strength is 256 and existing texture modulation is kept.
/// The later blaze fade finishes the ramp before controller teardown. No wave
/// handle is retained and the spawn result is ignored.
static void _actor342100StartBlazeHeatHaze(void)
{
    enum { ACTOR_342100_HEAT_HAZE_RISE_FRAMES   = 600,
           ACTOR_342100_HEAT_HAZE_PEAK_STRENGTH = 256 };
    _Actor342100BlazeWork* work = D_actor_342100_80164BB8->work;

    work->blaze.wave.span  = ACTOR_342100_HEAT_HAZE_RISE_FRAMES;
    work->blaze.wave.scale = ACTOR_342100_HEAT_HAZE_PEAK_STRENGTH;
    taskSpawnFromTable(D_actor_342100_801648DC, 0, 0, &work->blaze.wave);
}

/// Starts the blaze's body fire and stops the encounter, or spreads existing fire over the player.
///
/// Called by the burn script with 0 to start and 1 to spread; every nonzero value
/// selects spreading. Requires the published controller and its loaded player
/// effects. Starting cancels room effects, broadcasts a borrowed stop command to
/// placed actors, sends it to a recorded encounter controller, then retains the
/// body-fire task. The command uses synthetic stage 0/area 44, not a room ID.
/// Both dispatches consume the stack record synchronously and their results are
/// ignored. Spreading requires a successfully spawned, still-live body-fire task.
static void _actor342100SetBlazeBodyFireMode(s32 spreadToWholeBody)
{
    enum { ACTOR_342100_BLAZE_STOP_COMMAND_STAGE   = 0,
           ACTOR_342100_BLAZE_STOP_COMMAND_AREA    = 44,
           ACTOR_342100_BLAZE_BODY_FIRE_DESCRIPTOR = 3 };
    _Actor342100BlazeWork* work = D_actor_342100_80164BB8->work;
    ActorCommand           stopRequest;

    if (spreadToWholeBody == 0) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B3_DUMPING_HOLE_BLAZE, 0, 0);
        roomEffectRequestCancelAll();
        stopRequest.context.loc.area  = ACTOR_342100_BLAZE_STOP_COMMAND_AREA;
        stopRequest.context.loc.stage = ACTOR_342100_BLAZE_STOP_COMMAND_STAGE;
        stopRequest.command           = OVERLAY_ENCOUNTER_COMMAND_STOP;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &stopRequest, ACTOR_COMMAND_MESSAGE_APPLY);
        if (work->encounterTask != NULL) {
            TASK_MESSAGE_DISPATCH_POINTER(work->encounterTask, ACTOR_COMMAND_MESSAGE_APPLY, &stopRequest, 0);
        }
        work->bodyFireTask = taskSpawnFromTable(D_actor_342100_80164B78, ACTOR_342100_BLAZE_BODY_FIRE_DESCRIPTOR, 0, 0);
        return;
    }
    work->bodyFireTask->spawnArg1.value = 1;
}

/// Triggers player death while preserving the burn scene's display.
///
/// The script runs this immediately before its end command clears event state,
/// allowing the normal death monitor to accept zero HP. The preserve-display
/// restart path skips the ordinary death image and framebuffer clear.
static void _actor342100TriggerBlazeDeath(void)
{
    gPlayerStatus.hp          = 0;
    gGameSession->restartMode = GAME_SESSION_RESTART_PRESERVE_DISPLAY;
}
