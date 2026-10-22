#include "actors/task_tables.h"

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
#include "../../shared/screen/screen_wave.h"
#include "../../shared/effects/incinerator_blaze.h"

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

/// Records a burn clip and sends its synchronous, collision-disabled play request.
///
/// `workValue` is a live controller work pointer; `requestValue` is a writable
/// local AnimationPlayRequest. Arguments must be stable and side-effect-free: `workValue`
/// and `animationIdValue` are evaluated twice, and `requestValue` six times.
/// The receiver borrows the request only until dispatch returns. This compound
/// statement captures no caller identifiers and discards the dispatch result.
/// animationIdValue is the caller's already narrowed s16 bank-relative ID;
/// bankIndexValue selects a live player bank and blendFramesValue counts normal
/// animation frames. Both are evaluated once. Only source.index and the four
/// request words are initialized, covering the complete AnimationPlayRequest.
/// The controller cache is written before the request ID and synchronous call.
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

static s32 _gScreenWaveAmplitude;

static ScreenWaveCtx* _gScreenWaveContext;

static ScreenWaveGridOscillator _gScreenWaveColumnWaves[10];
static ScreenWaveGridOscillator _gScreenWaveRowWaves[30];

/// Double-buffered 8 by 30 meshes of textured quads, one mesh per frame
/// buffer. `_screenWaveGridTask` builds them once and moves their corners.
extern POLY_FT4 gScreenWaveGrid[2][30][8];

static void _actor342100BlazeFireEmitterTask(Task* task);
static void _actor342100SpawnBlazeFireEmitters(void);
static void _actor342100EncounterBlazeControllerTask(Task* task);

#include "assets/rooms/shelter/b3_dumping_hole/blaze_scene_clips.inc"

enum { ACTOR_342100_HEAT_HAZE_TASK_PRIORITY = 192 };

/// Bodyless captured-frame heat-haze task for the blaze scene, followed by its end marker.
///
/// Spawn slot 0 with a live writable ScreenWaveCtx; the grid task borrows
/// that context through its final draw. Only one wave may be active in this
/// package because it shares the prebuilt grid and oscillator storage.
static TaskDesc _gActor342100BlazeHeatHazeTaskDescs[2] = {
    { { { TASK_BODY_NONE, ACTOR_342100_HEAT_HAZE_TASK_PRIORITY } }, _screenWaveGridTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Current sine-displacement strength, recomputed as frame * scale / span.
///
/// Strength 32 corresponds to one pixel at the sine peak. The seed tick resets
/// the initial 256 to zero; the context must have a positive span while drawing.
static s32 _gScreenWaveAmplitude = 256;

/// Private one-entry message table controlling this carrier's blaze fade.
///
/// Borrowed for the fade task lifetime, with no end marker. Send only
/// `BLAZE_FADE_MESSAGE_SET_STATE` (2011): first payload 2 fast red, 3 slow red
/// or 4 wash to white. The full signed state word is stored; the second
/// payload is ignored and the unspecified result must be discarded.
static TaskMessageEntry _gBlazeFadeMessages[1] = {
    { BLAZE_FADE_MESSAGE_SET_STATE, _actor342100HandleBlazeFadeState },
};

/// Three blaze-scene clips followed by the null terminator used to count them.
///
/// Copied into player bank extension slots 47..49, excluding the terminator.
/// Clip selectors 0..2 refer to these sets relative to the extension base.
/// The bank borrows the loaded descriptors and clip data through scene playback.
#include "assets/rooms/shelter/b3_dumping_hole/player_burn_bank.inc"

enum { ACTOR_342100_BLAZE_NO_SUCCESSOR = -1 };

/// Successor extension clip indices for the blaze scene, or a negative end marker.
///
/// Indexed by animation ID minus `ANIMATION_BANK_BASE_SET_COUNT`. Only clips
/// 0..2 are installed and selected by the script; all three end their chain.
static s16 _gActor342100BlazeClipSuccessors[3] = {
    ACTOR_342100_BLAZE_NO_SUCCESSOR,
    ACTOR_342100_BLAZE_NO_SUCCESSOR,
    ACTOR_342100_BLAZE_NO_SUCCESSOR,
};

/// Two blaze fire-emitter positions for view 30 followed by a zero-X sentinel.
///
/// Signed-halfword XYZ uses integer units in the view-parent coordinate frame;
/// the fourth halfword is unused. Each live record spawns one delayed emitter.
static const SVECTOR _gActor342100BlazeFirePlacementsView30[3] SECTION(".data") = {
    { 0x4650, -100, -2500, 0 },
    { 0x4650, -700, -8700, 0 },
    { 0, 0, 0, 0 },
};

/// Two blaze fire-emitter positions for view 29 followed by a zero-X sentinel.
///
/// Signed-halfword XYZ uses integer units in the view-parent coordinate frame;
/// the fourth halfword is unused. Each live record spawns one delayed emitter.
static const SVECTOR _gActor342100BlazeFirePlacementsView29[3] SECTION(".data") = {
    { 0x4268, -600, -1500, 0 },
    { 0x4268, -100, -0x2710, 0 },
    { 0, 0, 0, 0 },
};

enum { ACTOR_342100_BLAZE_VIEW35_PLACEMENT_START = 3 };

/// Six blaze fire-emitter positions for view 31, sharing their last three with view 35.
///
/// View 31 starts at entry 0; view 35 starts at entry 3. Both walks stop at the
/// zero-X entry 6. Signed-halfword XYZ uses integer units in the view-parent
/// coordinate frame; the fourth halfword is unused. The seven records are one
/// complete span so either sentinel walk stays inside the declared object.
static const SVECTOR _gActor342100BlazeFirePlacementsView31[7] SECTION(".data") = {
    { 9000, -100, -2500, 0 },
    { 5000, -100, -2500, 0 },
    { 0x2710, 0, -0x2710, 0 },
    { 0x2710, -200, -9000, 0 },
    { 0x2710, -200, -2000, 0 },
    { 6000, -200, -3000, 0 },
    { 0, 0, 0, 0 },
};

/// Three blaze fire-emitter positions for view 36 followed by a zero-X sentinel.
///
/// Signed-halfword XYZ uses integer units in the view-parent coordinate frame;
/// the fourth halfword is unused. Each live record spawns one delayed emitter.
static const SVECTOR _gActor342100BlazeFirePlacementsView36[4] SECTION(".data") = {
    { 3000, -200, -5000, 0 },
    { 5000, -200, -7500, 0 },
    { 3000, -200, -8000, 0 },
    { 0, 0, 0, 0 },
};

/// Reusable blast-effect arguments for fire on one live player-model part.
///
/// Each spawn replaces the borrowed coordinate and signed low-half size;
/// the high half stays 1. The effect copies the argument word synchronously.
static EffectSpawnArg _gBlazePlayerFireArg = { NULL, 0, 1 };

/// Sixteen player-model coordinate indices eligible for incinerator body fire.
///
/// Initial and small fire select the first four (2, 4, 6, 10); large recurring
/// fire selects all sixteen. Every coordinate must remain live through spawning.
static u16 _gBlazePlayerFireParts[16] = {
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

enum {
    ACTOR_342100_BLAZE_CLIP_FIRST  = 0,
    ACTOR_342100_BLAZE_CLIP_SECOND = 1,
    ACTOR_342100_BLAZE_CLIP_THIRD  = 2,
    ACTOR_342100_BLAZE_FIRE_START  = 0,
    ACTOR_342100_BLAZE_FIRE_SPREAD = 1,
};

/// Burn sequence that plays three added player clips, spreads fire and triggers death.
///
/// Coordinates body-fire, red/white fades, room emitters and heat haze over
/// frame waits before the END command clears event state. The script and all
/// callback/resource data must remain loaded until playback finishes.
static EvsCommand _gActor342100BlazeScript[18] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100PlayBlazeAnimation }, { .value = ACTOR_342100_BLAZE_CLIP_FIRST }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100SetBlazeBodyFireMode }, { .value = ACTOR_342100_BLAZE_FIRE_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100SetBlazeFadeState }, { .value = ACTOR_342100_BLAZE_FADE_FAST_RED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100PlayBlazeAnimation }, { .value = ACTOR_342100_BLAZE_CLIP_SECOND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342100SpawnBlazeFireEmitters }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342100StartBlazeHeatHaze }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100PlayBlazeAnimation }, { .value = ACTOR_342100_BLAZE_CLIP_THIRD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100SetBlazeBodyFireMode }, { .value = ACTOR_342100_BLAZE_FIRE_SPREAD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 75 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100SetBlazeFadeState }, { .value = ACTOR_342100_BLAZE_FADE_SLOW_RED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100SetBlazeFadeState }, { .value = ACTOR_342100_BLAZE_FADE_TO_WHITE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342100TriggerBlazeDeath }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc gActor342100EncounterBlazeTaskDescriptors[5] = {
    { { { TASK_BODY_NONE, 192 } }, _actor342100EncounterBlazeControllerTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _blazeFadeTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _blazeBodyFireTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, _actor342100BlazeFireEmitterTask, { .value = 0 } },
};

/// Borrowed context of this renderer's active wave task.
///
/// Set on the seed tick and left unchanged at teardown. Only one wave may use
/// this storage; the context must stay live through its final drawing tick.
static ScreenWaveCtx* _gScreenWaveContext = NULL;

/// Borrowed singleton task of the dumping-hole encounter and player blaze scene.
///
/// NULL before successful work allocation publishes the controller. Burn-script
/// callbacks require its task and work to remain live. Teardown leaves the
/// handle unchanged; it does not own or retain the task.
static Task* _gActor342100BlazeControllerTask = NULL;

/// Column sine oscillators for horizontal screen displacement.
///
/// The per-frame renderer uses entries 0..10; the prebuilt-grid renderer uses
/// 0..8. Any remaining declared capacity is unaccessed; its role is unproven.
static ScreenWaveGridOscillator _gScreenWaveColumnWaves[10] = { 0 };

/// Thirty active row sine oscillators for vertical screen displacement.
///
/// Seeded at wave startup and advanced on drawing updates. Any declared
/// entries beyond 29 are unaccessed; their role is unproven.
static ScreenWaveGridOscillator _gScreenWaveRowWaves[30] = { 0 };

POLY_FT4 gScreenWaveGrid[2][30][8] = { 0 };

static s32 _actor342100UpdateBlazeScene(Task* controller);

#include "../../shared/screen/screen_wave_grid.inc.c"

#include "../../shared/effects/incinerator_blaze_fade.inc.c"

/// Starts a burn clip's successor once the player's animation has settled.
///
/// The controller and any non-NULL player task must remain live. An extension
/// animation ID must index `_gActor342100BlazeClipSuccessors` after subtracting
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
            if (_gActor342100BlazeClipSuccessors[work->animationId - ANIMATION_BANK_BASE_SET_COUNT] >= 0) {
                animationId  = _gActor342100BlazeClipSuccessors[work->animationId - ANIMATION_BANK_BASE_SET_COUNT] + ANIMATION_BANK_BASE_SET_COUNT;
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
/// Unsupported views retain an uninitialized placement pointer. View 31 reads
/// six positions; view 35 starts at the fourth entry of that same list.
static void _actor342100SpawnBlazeFireEmitters(void)
{
    enum { ACTOR_342100_BLAZE_FIRE_EMITTER_DESCRIPTOR = 4,
           ACTOR_342100_BLAZE_FIRE_DELAY_MASK         = 31 };
    GfxCoord*      coord;
    const SVECTOR* placement;
    Task*          emitter;
    u32            delayRoll;

    switch (gGameSession->location.view) {
        case 29:
            placement = _gActor342100BlazeFirePlacementsView29;
            break;
        case 30:
            placement = _gActor342100BlazeFirePlacementsView30;
            break;
        case 31:
            placement = _gActor342100BlazeFirePlacementsView31;
            break;
        case 35:
            placement = &_gActor342100BlazeFirePlacementsView31[ACTOR_342100_BLAZE_VIEW35_PLACEMENT_START];
            break;
        case 36:
            placement = _gActor342100BlazeFirePlacementsView36;
            break;
    }
    while (placement->vx != 0) {
        delayRoll       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = delayRoll;
        emitter         = taskSpawnFromTable(gActor342100EncounterBlazeTaskDescriptors, ACTOR_342100_BLAZE_FIRE_EMITTER_DESCRIPTOR, (delayRoll >> 16) & ACTOR_342100_BLAZE_FIRE_DELAY_MASK, 0);
        coord           = emitter->extra.coordBody->coord;
        _actor342100PlaceBlazeFireEmitter(coord, placement);
        placement++;
    }
}

#include "../../shared/effects/incinerator_blaze_body_fire.inc.c"

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
    while (_gActor342100BlazeAnimationSets[setCount & ACTOR_342100_BLAZE_EXTENSION_COUNT_MASK] != NULL) {
        setCount += 1;
    }
    bankCopyRequest.sets  = _gActor342100BlazeAnimationSets;
    bankCopyRequest.count = setCount & ACTOR_342100_BLAZE_EXTENSION_COUNT_MASK;
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
            gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
            evsStartScript(_gActor342100BlazeScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            work->fadeTask   = taskSpawnFromTable(gActor342100EncounterBlazeTaskDescriptors, ACTOR_342100_BLAZE_FADE_DESCRIPTOR, 0, controller);
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
        allocatedWork->playerTask        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        _gActor342100BlazeControllerTask = task;
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
    if (gGameSession->sceneUpdatesPaused != 0 || gAttachmentState.menuOpen != ATTACHMENT_MENU_CLOSED || gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING || gDirectionActionActive != 0) {
        return;
    }
    switch (task->state) {
        case ACTOR_342100_CONTROLLER_INITIALIZE:
            if (gAttachmentState.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                break;
            }
            _actor342100InitializeBlazeController(task);
            taskSpawnFromTable(gShelterB3DumpingHoleEncounterCaptionTaskDescriptors, 0, ACTOR_342100_CONTROLLER_CAPTION_PRIORITY, 0);
            sndEvtRequestScriptStart(SOUND_SHELTER_B3_DUMPING_HOLE_ALERT, 0, 0);
            switch (gGameSession->spawnPhase[0]) {
                case GAME_SESSION_SPAWN_IDLE:
                    task->state++;
                    break;
                case GAME_SESSION_SPAWN_ARMED:
                    // Keep the entry pointer: this store precedes its next-tick refresh.
                    entryWork->encounterTask = taskSpawnFromTable(gShelterB3DumpingHoleEncounterTaskDescriptors, SHELTER_B3_DUMPING_HOLE_ENCOUNTER_TASK_CONTROLLER, 1, 0);
                default:
                    task->state = ACTOR_342100_CONTROLLER_WAIT_CLOCK;
                    break;
            }
            break;
        case ACTOR_342100_CONTROLLER_WAIT_ACTION:
            if (gameFlagGetNibble(GAME_FLAG_11E) != 0) {
                if (worldCollisionReadActionHit(&actionControl, &roomActionId, &roomActionArgument) != 0 && (actionControl & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM && (s8)roomActionId == ACTOR_342100_CONTROLLER_START_ENCOUNTER_ACTION) {
                    entryWork->encounterTask = taskSpawnFromTable(gShelterB3DumpingHoleEncounterTaskDescriptors, SHELTER_B3_DUMPING_HOLE_ENCOUNTER_TASK_CONTROLLER, 0, 0);
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

    work        = _gActor342100BlazeControllerTask->work;
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
    _Actor342100BlazeWork* work = _gActor342100BlazeControllerTask->work;

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
    _Actor342100BlazeWork* work = _gActor342100BlazeControllerTask->work;

    work->blaze.wave.span  = ACTOR_342100_HEAT_HAZE_RISE_FRAMES;
    work->blaze.wave.scale = ACTOR_342100_HEAT_HAZE_PEAK_STRENGTH;
    taskSpawnFromTable(_gActor342100BlazeHeatHazeTaskDescs, 0, 0, &work->blaze.wave);
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
    _Actor342100BlazeWork* work = _gActor342100BlazeControllerTask->work;
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
        work->bodyFireTask = taskSpawnFromTable(gActor342100EncounterBlazeTaskDescriptors, ACTOR_342100_BLAZE_BODY_FIRE_DESCRIPTOR, 0, 0);
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
