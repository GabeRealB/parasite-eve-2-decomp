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
/// `func_actor_342100_801630A4`.
extern Task* D_actor_342100_80164BB8;

/// Single-entry spawn table `func_actor_342100_80163454` starts as entry 3.
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

void func_actor_342100_80163454(s32 arg0);

static void _actor342100TriggerBlazeDeath(void);

/// Main-executable global with no module header yet: the remaining-enemy count.

/// Single-entry spawn table of the screen-wave task
/// `_screenWaveGridTask`: `_actor342100StartBlazeHeatHaze` starts entry 0
/// and hands it the address of the work block's `blaze.wave` as its ramp.
extern TaskDesc D_actor_342100_801648DC[];

/// Null-terminated table of the overlay's per-state message tables, counted
/// and reported by `func_actor_342100_80162F54` when it arms the encounter:
/// three live entries and the null word that ends them.
extern AnimationSet* D_actor_342100_80164900[4];

/// Animation step table `_actor342100AdvanceBlazeAnimation` walks: `s16` entries
/// holding the clip one step on from the work block's `animationId`, both less
/// `ANIMATION_BANK_BASE_SET_COUNT`; the first three entries are `-1`, which ends
/// the chain, and only the fourth is live. Sits directly after
/// `D_actor_342100_80164900`'s null word, and its first element is the address
/// `func_actor_342100_80162F54`'s encounter table of a different size would
/// have started at, so splat cut it out as a symbol of its own.
extern s16 D_actor_342100_80164910[];

/// Placement tables the overlay's spawn task picks between by
/// `gGameSession->location.loc.view`: 0x1D, 0x1E, 0x1F, 0x23 and 0x24 select the 0x80164930
/// / 0x80164918 / 0x80164948 / 0x80164960 / 0x80164980 table respectively, and
/// the values in between select none. Each is a zero-`vx`-terminated `SVECTOR`
/// list of two to three placements -- the terminator is an all-zero entry -- and
/// `func_actor_342100_80162C88` drops one effect task on every live entry.
extern SVECTOR D_actor_342100_80164918[];
extern SVECTOR D_actor_342100_80164930[];
extern SVECTOR D_actor_342100_80164948[];
extern SVECTOR D_actor_342100_80164960[];
extern SVECTOR D_actor_342100_80164980[];

/// Model/animation set `func_actor_342100_80162F54` installs with
/// `evsStartScript` on the same arm; a byte address is all the installer sees.
extern EvsCommand D_actor_342100_801649C8[];

/// Effect record `blazeBodyFireTask` hands `effectSpawnHit` together
/// with one part of the player's model: `field_0` is that part's coordinate
/// and `field_4` the scale that goes with it (0x100 for the wide pick, 0x10
/// for the narrow one). Ships as `{ NULL, 0, 1 }` in the data blob, directly
/// before the part table below.
extern EffectSpawnArg gBlazeFireSpawn;

/// The player-model parts the effect record above is aimed at, as indices into
/// the player's coordinate array (`TmdObject::coords`): sixteen `u16`s
/// running 1..0x12, of which `blazeBodyFireTask` takes the first four
/// (2, 4, 6, 0xA) when it masks the LCG draw with 3 and all sixteen when it
/// masks with 0xF.
extern u16 gBlazePlayerParts[];

/// Frame counter the narrow arm of `blazeBodyFireTask`'s state 1 is
/// gated on: it aims the effect only on the frames where the low nibble (or,
/// for the other arm, the low three bits) of this global is clear.

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

void func_actor_342100_80162AB0(Task*);
void func_actor_342100_80162C88(void);
void func_actor_342100_801630A4(Task*);

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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_342100_80163454 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100SetBlazeFadeState }, { .value = ACTOR_342100_BLAZE_FADE_FAST_RED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100PlayBlazeAnimation }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_342100_80162C88 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342100StartBlazeHeatHaze }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100PlayBlazeAnimation }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_342100_80163454 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 75 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100SetBlazeFadeState }, { .value = ACTOR_342100_BLAZE_FADE_SLOW_RED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor342100SetBlazeFadeState }, { .value = ACTOR_342100_BLAZE_FADE_TO_WHITE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor342100TriggerBlazeDeath }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_342100_80164B78[5] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_342100_801630A4, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _blazeFadeTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, blazeBodyFireTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, func_actor_342100_80162AB0, { .value = 0 } },
};

ScreenWaveCtx* gScreenWaveCtx = NULL;

Task* D_actor_342100_80164BB8 = NULL;

// Nine active columns and one retained zero entry.
ScreenWaveGridOscillator gScreenWaveColumns[10] = { 0 };

ScreenWaveGridOscillator gScreenWaveRows[30] = { 0 };

POLY_FT4 gScreenWaveGrid[2][30][8] = { 0 };

static s32 func_actor_342100_80162F54(Task* arg0);

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

/// State 0 allocates the overlay's effect record -- `sizeof(EffectSpawnArg)`, scale 0x100,
/// count 1, aimed at the model's root coordinate -- through `arg0->work`,
/// which is also where the null check reads it back: that is what leaves the
/// copy into `eff` after the branch instead of before it. State 1 waits out
/// `spawnArg1` and steps to 2. State 2 runs on every fourth frame, and builds
/// the effect's offset vector out of five LCG rolls: two per signed component
/// (the value from one roll, its sign from the next) plus a third that is
/// always negative. Only the three rolls whose value goes into `gRandomLcgState`
/// are stored, so the two temporary rolls are separate variables -- one `rng`
/// would be a single long-lived pseudo and take a register the constant needs.
///
/// Where `vec.vx = vx` sits is load-bearing. Placed with the last roll it is
/// scheduled past the argument setup, which lengthens `vx`'s live range enough
/// that global-alloc prefers the `0x71357911` constant and hands the component
/// $a2 (99.49%); between the third roll and the `vec.vy` store it stays short
/// and takes $a1, the constant falling to $a2 (100.00%).
void func_actor_342100_80162AB0(Task* arg0)
{
    EffectSpawnArg* eff;
    GfxCoord*       coord;
    SVECTOR         vec;
    s32             rng;
    s32             rng2;
    s32             vx;
    s32             vz;

    eff   = (EffectSpawnArg*)arg0->work;
    coord = arg0->extra.coordBody->coord;
    switch (arg0->state) {
        case 0:
            arg0->work = memMalloc(sizeof(EffectSpawnArg), false);
            if (arg0->work == NULL) {
                taskKill(arg0);
                return;
            }
            eff = (EffectSpawnArg*)arg0->work;
            memFillBytes(eff, 0, sizeof(EffectSpawnArg));
            eff->spawnArgLo = 0x100;
            eff->coord      = arg0->extra.coordBody->coord;
            eff->spawnArgHi = 1;
            arg0->state++;
            return;
        case 1:
            if (arg0->spawnArg1.value <= 0) {
                arg0->state = 2;
                return;
            }
            arg0->spawnArg1.value--;
            return;
        case 2:
            if (gDisplayState.animFrame & 0xF) {
                return;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            vx              = ((u32)rng >> 16) & 0x3F;
            gRandomLcgState = rng * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                vx = -vx;
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            vec.vx          = vx;
            vec.vy          = -((gRandomLcgState >> 16) & 0x3F);
            rng2            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            vz              = ((u32)rng2 >> 16) & 0x3F;
            gRandomLcgState = rng2 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                vz = -vz;
            }
            vec.vz = vz;
            effectSpawnHit(EFFECT_HIT_KIND_BLAST, coord, &vec, eff);
            return;
    }
}

/// Spawn the encounter's effect tasks: `gGameSession->location.loc.view` selects one of
/// the overlay's placement tables, and every entry in it rolls the LCG once,
/// starts spawn entry 4 (`func_actor_342100_80162AB0`) with the roll's masked
/// high half as its `spawnArg1` -- the lifetime that task's state 1 counts down
/// -- and lays the entry onto the model the new task displays: identity rotation,
/// the entry's `vx` / `vy` / `vz` written to `coord.t[0..2]`. The walk is `while (pos->vx != 0)`,
/// so a table is as many entries as it has non-zero `vx`s and a table whose
/// first entry is zero spawns nothing.
///
/// The table pointer is deliberately uninitialised: `gGameSession->location.loc.view`
/// values 0x20..0x22 -- and anything outside the jump table -- leave it holding
/// whatever the caller left in `$s1`, which is the target's shape.
///
/// Referenced from the `0x0D` entry of the command table in
/// `D_actor_342100_801649C8` (+0x90), next to the same-shaped entries naming
/// `_actor342100PlayBlazeAnimation` / `_actor342100SetBlazeFadeState` /
/// `_actor342100StartBlazeHeatHaze` / `func_actor_342100_80163454`. That entry
/// passes it no arguments, which is why the declaration is `(void)`.
void func_actor_342100_80162C88(void)
{
    GfxCoord* coord;
    SVECTOR*  pos;
    Task*     task;
    u32       rng;

    switch (gGameSession->location.loc.view) {
        case 29:
            pos = D_actor_342100_80164930;
            break;
        case 30:
            pos = D_actor_342100_80164918;
            break;
        case 31:
            pos = D_actor_342100_80164948;
            break;
        case 35:
            pos = D_actor_342100_80164960;
            break;
        case 36:
            pos = D_actor_342100_80164980;
            break;
    }
    while (pos->vx != 0) {
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        task            = taskSpawnFromTable(D_actor_342100_80164B78, 4, (rng >> 16) & 0x1F, 0);
        coord           = task->extra.tmd->coords;
        gfxSetRotIdentity(&coord->coord);
        coord->coord.t[0] = pos->vx;
        coord->coord.t[1] = pos->vy;
        coord->coord.t[2] = pos->vz;
        pos++;
    }
}

#include "../../shared/incinerator_blaze_body_fire.inc.c"

/// First tick of the overlay's event/controller task, the one that arms the
/// encounter as state 0 and then waits for the player's arrival as state 1.
///
/// State 0 counts the live entries of the overlay's message-table list and
/// hands slot 3 that list with message 0x3F7, lets the player's weapon into
/// the message stream (`playerActorSetScriptedControl`), raises the `Gp_StateC08` flag
/// `attachmentQueueIndex` gates on, installs the model set and hands slot 6 the
/// 0xFA4 that starts the encounter, then starts spawn entry 2 with the task
/// itself and steps to state 1. State 1 ticks the child and reports 1 to keep
/// the task alive until `gGameSession->eventState` is set.
static s32 func_actor_342100_80162F54(Task* arg0)
{
    _Actor342100BlazeWork*   work = arg0->work;
    _Actor342100BlazeWork*   msgWork;
    AnimationBankCopyRequest msg;
    s32                      n;

    switch (work->sceneState) {
        case ACTOR_342100_BLAZE_SCENE_START:
            msgWork = arg0->work;
            n       = 0;
            while (D_actor_342100_80164900[n & 0xFFFF] != 0) {
                n += 1;
            }
            msg.source.sets = &D_actor_342100_80164900[0];
            msg.wordCount   = n & 0xFFFF;
            TASK_MESSAGE_DISPATCH_POINTER(msgWork->playerTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &msg, 0);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
            evsStartScript(D_actor_342100_801649C8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            work->fadeTask   = taskSpawnFromTable(D_actor_342100_80164B78, 2, 0, arg0);
            work->sceneState = work->sceneState + 1;
            break;
        case ACTOR_342100_BLAZE_SCENE_RUNNING:
            if (gGameSession->eventState != 0) {
                break;
            }
            return 1;
    }
    _actor342100AdvanceBlazeAnimation(arg0);
    return 0;
}

/// The overlay's event/controller task. Idles while the session or any of
/// the global pause flags hold it. State 0 allocates the work block and
/// publishes the task, then picks state 1 or 2 from
/// `gGameSession->spawnPhase[0]`; state 1 waits on flag 0x11E and pending
/// object 5, and states 1 and 2 both move to 3 once `field_120` has dropped
/// to zero while the player still has HP. State 3 ticks
/// `func_actor_342100_80162F54` until it reports done.
///
/// `work` is read from `work` before state 0 replaces it, so the two
/// `encounterTask` stores go through the block the task held on entry.
void func_actor_342100_801630A4(Task* arg0)
{
    u16                    control;
    u8                     actionId;
    u8                     actionArgument;
    _Actor342100BlazeWork* work;
    _Actor342100BlazeWork* newWork;
    s32                    ready;
    PlayerStatus*          cfg;

    work = arg0->work;
    if (gGameSession->sceneUpdatesPaused != 0 || Gp_StateC08.menuOpen != ATTACHMENT_MENU_CLOSED || gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING || D_80114CF8 != 0) {
        return;
    }
    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                break;
            }
            newWork    = memMalloc(sizeof(*newWork), false);
            arg0->work = newWork;
            if (newWork == NULL) {
                taskKill(arg0);
            } else {
                memFillBytes(newWork, 0, sizeof(*newWork));
                newWork->playerTask     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_actor_342100_80164BB8 = arg0;
            }
            taskSpawnFromTable(D_shelter_b3_dumping_hole_8018B57C, 0, 0xD0, 0);
            sndEvtRequestScriptStart(SOUND_SHELTER_B3_DUMPING_HOLE_ALERT, 0, 0);
            switch (gGameSession->spawnPhase[0]) {
                case GAME_SESSION_SPAWN_IDLE:
                    arg0->state++;
                    break;
                case GAME_SESSION_SPAWN_ARMED:
                    work->encounterTask = taskSpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 0, 1, 0);
                default:
                    arg0->state = 2;
                    break;
            }
            break;
        case 1:
            if (gameFlagGetNibble(GAME_FLAG_11E) != 0) {
                if (worldCollisionReadActionHit(&control, &actionId, &actionArgument) != 0 && (control & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM && (s8)actionId == 1) {
                    work->encounterTask = taskSpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 0, 0, 0);
                    arg0->state++;
                }
            }
            cfg = &gPlayerStatus;
            if (gGameSession->sceneClock > 0 || cfg->hp <= 0) {
                ready = 0;
            } else {
                ready = 1;
            }
            if (ready) {
                arg0->state = 3;
            }
            break;
        case 2:
            cfg = &gPlayerStatus;
            if (gGameSession->sceneClock > 0 || cfg->hp <= 0) {
                ready = 0;
            } else {
                ready = 1;
            }
            if (ready) {
                arg0->state = 3;
            }
            break;
        case 3:
            if ((s16)func_actor_342100_80162F54(arg0) != 0) {
                arg0->state++;
            }
            break;
        case 4:
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

/// Entry/exit of the overlay's spawned child. A zero arm plays the cue, asks
/// slot 4 to forward message 0x7DB with the `{ 0, 0x2C, 4 }` record, passes the
/// same record on to `encounterTask` if that target exists, and starts the child at
/// entry 3; a non-zero arm tells the already-spawned child so through its
/// `Task::spawnArg1`.
void func_actor_342100_80163454(s32 arg0)
{
    _Actor342100BlazeWork* work = D_actor_342100_80164BB8->work;
    ActorCommand           msg;

    if (arg0 == 0) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B3_DUMPING_HOLE_BLAZE, 0, 0);
        roomEffectRequestCancelAll();
        msg.context.loc.area  = 0x2C;
        msg.context.loc.stage = 0;
        msg.command           = 4;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
        if (work->encounterTask != NULL) {
            TASK_MESSAGE_DISPATCH_POINTER(work->encounterTask, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
        }
        work->bodyFireTask = taskSpawnFromTable(D_actor_342100_80164B78, 3, 0, 0);
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
