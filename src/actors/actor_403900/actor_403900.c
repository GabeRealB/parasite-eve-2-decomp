#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"
#include "actors/actor_403900.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/model_lighting.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/areas.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
static void _frameCaptureQueue(s32 orderingTableSlot);
#define FRAME_CAPTURE_QUEUE _frameCaptureQueue
#include "../../shared/screen/frame_capture.h"
/// Selects Bishop identity and combat tuning for the shared GOLEM implementation.
///
/// Bind to `GOLEM_KNIGHT_BISHOP_KIND_BISHOP` before the shared header and undefine
/// after it; the fragments use the selected constants, not this selector.
#define GOLEM_KNIGHT_BISHOP_KIND GOLEM_KNIGHT_BISHOP_KIND_BISHOP
#include "../../shared/creatures/golem/golem_knight_bishop.h"
#undef GOLEM_KNIGHT_BISHOP_KIND

/// Main-executable global with no module header yet: the remaining-enemy
/// count. A grab only starts while it is positive.

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static s32 _actor403900RecordGrabEscape(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

static AnimationSet _gGolemGrabStartClip;
static AnimationSet _gGolemGrabHoldClip;
extern AnimationSet gGolemGrabKillClip;
extern AnimationSet gGolemIdleClip;
static AnimationSet _gGolemStrikeClip;
extern AnimationSet gGolemAdvanceClip;
static AnimationSet _gGolemChargeClip;
extern AnimationSet gGolemStaggerClip;
static AnimationSet _gGolemHeavyFlinchFrontClip;
static AnimationSet _gGolemHeavyFlinchBehindClip;
static AnimationSet _gGolemRecoverClip;
static AnimationSet _gGolemGrabReleaseClip;
static AnimationSet _gGolemFallBehindClip;
static AnimationSet _gGolemDownedHitBehindClip;
static AnimationSet _gGolemWritheBehindClip;
static AnimationSet _gGolemLieBehindClip;
static AnimationSet _gGolemFallFrontClip;
static AnimationSet _gGolemDownedHitFrontClip;
static AnimationSet _gGolemWritheFrontClip;
static AnimationSet _gGolemLieFrontClip;
static AnimationSet _gGolemGrabEarlyReleaseClip;
static AnimationSet _gGolemPlayerGrabStartClip;
static AnimationSet _gGolemPlayerGrabHoldClip;
extern AnimationSet gGolemPlayerGrabKillClip;
static AnimationSet _gGolemPlayerGrabReleaseClip;
static TmdSource    _gGolemBody;
static void         _actor403900GolemTask(Task* task);

/// GOLEM message handlers, terminated by `TASK_MESSAGE_TABLE_END`.
///
/// Release-hold records a living player's completed struggle; other IDs return zero.
static TaskMessageEntry _gGolemKnightBishopMessages[2] = {
    { ACTOR_MESSAGE_RELEASE_HOLD, _actor403900RecordGrabEscape },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Blend duration in normal-rate frames for each requested GOLEM clip.
///
/// Indexed by `GOLEM_KNIGHT_BISHOP_ANIM_`; zero restarts without a blend.
static s16 _gGolemKnightBishopAnimBlendFrames[GOLEM_KNIGHT_BISHOP_ANIM_COUNT] = {
    [GOLEM_KNIGHT_BISHOP_ANIM_NONE]                = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_START]          = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_HOLD]           = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_KILL]           = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_STAND]               = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_STRIKE]              = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_ADVANCE]             = 8,
    [GOLEM_KNIGHT_BISHOP_ANIM_CHARGE]              = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_LIGHT_FLINCH]        = 1,
    [GOLEM_KNIGHT_BISHOP_ANIM_HEAVY_FLINCH_FRONT]  = 1,
    [GOLEM_KNIGHT_BISHOP_ANIM_HEAVY_FLINCH_BEHIND] = 1,
    [GOLEM_KNIGHT_BISHOP_ANIM_RECOVER]             = 8,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_RELEASE]        = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_FALL_BEHIND]         = 4,
    [GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_BEHIND]   = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_WRITHE_BEHIND]       = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND]          = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_FALL_FRONT]          = 4,
    [GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_FRONT]    = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_WRITHE_FRONT]        = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT]           = 0,
    [GOLEM_KNIGHT_BISHOP_ANIM_GRAB_EARLY_RELEASE]  = 0,
};

/// Forward-speed curve of the box approach's charge, in world units per frame.
///
/// The first span ending at or after the animation frame supplies the raw
/// halfword speed. The final frame 109 covers the charge through its end.
static GolemKnightBishopChargeSpeedSpan _gGolemKnightBishopChargeSpeedSpans[18] = {
    { 3, -10 },
    { 5, 350 },
    { 6, 210 },
    { 9, 117 },
    { 11, 130 },
    { 15, 85 },
    { 17, 90 },
    { 21, 35 },
    { 22, -50 },
    { 23, -80 },
    { 31, 0 },
    { 37, 2 },
    { 43, 5 },
    { 62, 8 },
    { 64, -5 },
    { 69, -6 },
    { 84, -17 },
    { 109, -6 },
};

/// Base sound-script IDs for the room's GOLEM sound set.
///
/// Sets 1..4 use [set * 2 - 1] for cue 2, [set * 2] for cue 1, and
/// [set + 8] for the fall impact. Set 0 disables animation cues; its impact
/// lookup still selects entry 8. Entries 13..16 have no observed consumers.
/// Callers OR the Enemy placement index into bits 8..11 before starting a script.
static s32 _gGolemKnightBishopAnimSounds[17] = {
    0,
    0x40160001,
    0x40160002,
    0x40160003,
    0x40160004,
    0x40160005,
    0x40160006,
    0x40160012,
    0x40160013,
    0x40160009,
    0x4016000A,
    0x4016000B,
    0x40160014,
    0,
    0,
    0,
    0,
};

/// Base sound-script ID played during a real appearance.
///
/// The placement index is ORed into bits 8..11; the live instance ID is kept
/// until the appearance completes or is interrupted, then stopped.
static s32 _gGolemKnightBishopAppearSound = 0x40160007;

/// Base sound-script ID played while the GOLEM vanishes.
///
/// The placement index is ORed into bits 8..11; the live instance ID is kept
/// until the model is hidden, then stopped.
static s32 _gGolemKnightBishopVanishSound = 0x40160008;

/// Base sound-script ID played once when a normal vanish finishes.
///
/// Callers OR the placement index into bits 8..11; feint shrinking does not play it.
static s32 _gGolemKnightBishopVanishEndSound = 0x4016000F;

/// Base sound-script ID for each strike blow and the charge's attack cue.
///
/// Callers OR the placement index into bits 8..11 before starting the script.
static s32 _gGolemKnightBishopStrikeSound = 0x40160010;

/// Base sound-script ID played twenty ticks into the grab release countdown.
///
/// Callers OR the placement index into bits 8..11 before starting the script.
static s32 _gGolemKnightBishopGrabReleaseSound = 0x40160011;

#include "assets/enemies/golem/body.inc"

#include "assets/enemies/golem/bishop/clips.inc"

/// Damage and reaction rows for the grab, strike and charge.
///
/// The fourth row is retained but is not selected by the included sequences.
static DamageAttack _gGolemKnightBishopAttacks[4] = {
    [GOLEM_KNIGHT_BISHOP_ATTACK_GRAB]   = { 10, 10 },
    [GOLEM_KNIGHT_BISHOP_ATTACK_STRIKE] = { 45, 2 },
    [GOLEM_KNIGHT_BISHOP_ATTACK_CHARGE] = { 58, 2 },
    [3]                                 = { 999, 0 },
};

/// Bishop GOLEM combat parameters and release rewards.
///
/// Shared by live enemies of this carrier; `hpMax` supplies their starting HP.
static EnemyParams _gGolemKnightBishopParams = { _gGolemKnightBishopAttacks, 800, 400, 2500, 7, 100, 20, 0, 0 };

/// Guaranteed fatal-grab HP thresholds indexed by scene difficulty.
///
/// At or below the selected threshold the hold starts its kill clip immediately;
/// higher HP arms the later damage-tick/HP-dependent kill roll. Active difficulty
/// rows 0..4 are normal, Bounty, Scavenger, Nightmare and replay. The sixth
/// stored signed halfword is retained unused; values count HP, not percentages.
static s16 _gGolemKnightBishopGrabKillHpThresholds[6] = {
    6,
    11,
    16,
    16,
    4,
    0,
};

/// Weighted four-bit roll selecting whether a strike appearance is a feint.
///
/// Eleven rolls select a real appearance and five a feint. Values are 0 real, 1 feint.
static u16 _gGolemKnightBishopFeintRoll[16] = {
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_REAL,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_FEINT,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_FEINT,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_FEINT,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_FEINT,
    GOLEM_KNIGHT_BISHOP_FEINT_ROLL_FEINT,
};

/// Weighted base idle waits in actor-running frames.
///
/// A uniform four-bit roll indexes sixteen signed-halfword waits. The hidden
/// idle sequence scales the chosen duration by (16 - attackCount) / 16, using
/// integer truncation. Knight/Bishop attack-count limits are 12/8 respectively.
static s16 _gGolemKnightBishopIdleWaitFrames[16] = {
    10,
    15,
    20,
    20,
    25,
    30,
    40,
    50,
    25,
    30,
    40,
    50,
    25,
    30,
    40,
    50,
};

/// Weighted counterattack choices after the player breaks a feint.
///
/// A uniform four-bit roll indexes sixteen offsets: twelve 1s choose the grab
/// placement check and four 2s choose the strike placement check. The idle
/// sequence adds its PICK_ATTACK step (2), yielding steps 3 and 4.
static u16 _gGolemKnightBishopCounterattackStepOffsets[16] = {
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_GRAB_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_GRAB_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_GRAB_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_GRAB_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_STRIKE_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_STRIKE_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_STRIKE_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_STRIKE_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_STRIKE_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_STRIKE_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_STRIKE_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_STRIKE_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_STRIKE_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_STRIKE_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_STRIKE_OFFSET,
    GOLEM_KNIGHT_BISHOP_COUNTERATTACK_STRIKE_OFFSET,
};

static GolemKnightBishopRegion _gActor403900ShelterB2BreedingRoomRegions[5];
static GolemKnightBishopRegion _gActor403900NeoArkShrineRegions[6];
static GolemKnightBishopRegion _gActor403900ShelterB2PodAccessTunnelRegions[3];
static GolemKnightBishopRegion _gActor403900DryfieldNightParkingLotRegions[3];
static GolemKnightBishopRegion _gActor403900ShelterB1SleepingQuartersRegions[8];
static GolemKnightBishopRegion _gActor403900ShelterB1AccessTunnelRegions[5];
static GolemKnightBishopRegion _gActor403900Shelter1fAirlockRegions[2];
static GolemKnightBishopRegion _gActor403900NeoArkPowerPlant2Regions[4];

/// Room selection for the GOLEM's activation regions.
///
/// `regionTable` indexes `_gGolemKnightBishopRegionTables`; zero ends the list.
/// Matching rows supply both circle and box regions, with the last match winning.
static GolemKnightBishopRoomRegions _gGolemKnightBishopRoomRegions[9] = {
    { 1, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, ARRAY_SIZE(_gActor403900ShelterB2BreedingRoomRegions) },
    { 2, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_SHRINE, ARRAY_SIZE(_gActor403900NeoArkShrineRegions) },
    { 3, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, ARRAY_SIZE(_gActor403900ShelterB2PodAccessTunnelRegions) },
    { 4, GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT, ARRAY_SIZE(_gActor403900DryfieldNightParkingLotRegions) },
    { 5, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, ARRAY_SIZE(_gActor403900ShelterB1SleepingQuartersRegions) },
    { 6, GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, ARRAY_SIZE(_gActor403900ShelterB1AccessTunnelRegions) },
    { 7, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_SHELTER_1F_AIRLOCK, ARRAY_SIZE(_gActor403900Shelter1fAirlockRegions) },
    { 8, GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_2, ARRAY_SIZE(_gActor403900NeoArkPowerPlant2Regions) },
    { GOLEM_KNIGHT_BISHOP_REGION_TABLE_END, GAME_STAGE_NONE, 0, 0 },
};

/// Bishop activation regions in the Shelter B2 breeding room.
///
/// One circle requests a grab behind the player; four boxes select attack posts.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900ShelterB2BreedingRoomRegions[5] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 1000 }, 2500, 4000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 2500, 4300, 10400, 5200, 13000, 3400 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 10500, 5700, 10000, 1500, 11000, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 6450, 1300, 5900, 5400, 7000, 4000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 1700, -500, 1000, 5800, 2400, 3200 },
};

/// Bishop activation regions in the Neo Ark shrine.
///
/// Two circles request grabs behind the player; four boxes select attack posts.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900NeoArkShrineRegions[6] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 1000 }, 7000, -3500, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 7000, 6000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 800, 1000, 0, 6000, 1550, 3000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 7500, 4500, 1550, 6000, 4000, 4000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 7000, 5000, 5000, -4000, 9000, -5000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 500, 4500, 6000, 6000, 8000, 4000 },
};

/// Bishop activation regions in the Shelter B2 pod access tunnel.
///
/// Two circles request grabs behind the player; one box selects an attack post.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900ShelterB2PodAccessTunnelRegions[3] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 3000 }, 7000, -1600, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 1600, -7300, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 1600, -1600, 1000, -7000, 2500, -8000 },
};

/// Bishop activation regions in the Dryfield night parking lot.
///
/// One circle requests a grab behind the player; two boxes select attack posts.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900DryfieldNightParkingLotRegions[3] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 7000, 3000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, -1000, 2750, 1500, 3500, 3500, 2000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 2500, 500, -3500, 1000, -2000, 0 },
};

/// Bishop activation regions in the Shelter B1 sleeping quarters.
///
/// Two circles request grabs behind the player; six boxes select attack posts.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900ShelterB1SleepingQuartersRegions[8] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 2000 }, 1600, -1400, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 1000 }, 10000, 4200, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 10900, 4200, 1000, 5000, 5000, 3500 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 5500, 2500, 1000, 3500, 3500, 1500 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 8950, 1600, 8000, 5800, 9500, 3000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 5500, 4000, 11000, 4500, 12400, 3000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 1700, 4700, 1000, 0, 2400, -2900 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1800 }, 1400, 4700, 2400, 1200, 3500, 0 },
};

/// Bishop activation regions in the Shelter B1 access tunnel.
///
/// Three circles request grabs behind the player; two boxes select attack posts.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900ShelterB1AccessTunnelRegions[5] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 800 }, 10000, 5300, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 800 }, 7500, 8000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 800 }, 4000, 8000, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 1024 }, 1200, 8000, 6000, 9000, 7500, 7000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 10300, 8000, 4000, 9000, 6000, 7000 },
};

/// Bishop activation regions in the Shelter 1F airlock.
///
/// One circle requests a grab behind the player; one box selects an attack post.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900Shelter1fAirlockRegions[2] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 600 }, -1900, 4800, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 2000, 4800, -5500, 5500, -1900, 4000 },
};

/// Bishop activation regions in the Neo Ark power plant 2.
///
/// One circle requests a grab behind the player; three boxes select attack posts.
/// Circles and boxes use exclusive world-XZ bounds; headings use 4096 units
/// per turn. The spawn directory supplies the complete array to the room's
/// golem work, which borrows it for the overlay's lifetime.
static GolemKnightBishopRegion _gActor403900NeoArkPowerPlant2Regions[4] = {
    { GOLEM_KNIGHT_BISHOP_REGION_CIRCLE, { .radius = 600 }, 3500, -1800, 0, 0, 0, 0 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 3072 }, 8500, -1800, 0, -1000, 2000, -2000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 0 }, 7750, -7500, 6500, -1000, 9000, -2000 },
    { GOLEM_KNIGHT_BISHOP_REGION_BOX, { .heading = 2048 }, 7750, -3000, 6500, -6000, 9000, -9000 },
};

/// Bishop room activation-region tables, selected by the room directory's 1..8 keys.
///
/// Slot 0 is NULL. Each selected table is borrowed with the corresponding
/// directory row's complete regionCount for the actor overlay's lifetime.
static GolemKnightBishopRegion* _gGolemKnightBishopRegionTables[9] = {
    NULL,
    _gActor403900ShelterB2BreedingRoomRegions,
    _gActor403900NeoArkShrineRegions,
    _gActor403900ShelterB2PodAccessTunnelRegions,
    _gActor403900DryfieldNightParkingLotRegions,
    _gActor403900ShelterB1SleepingQuartersRegions,
    _gActor403900ShelterB1AccessTunnelRegions,
    _gActor403900Shelter1fAirlockRegions,
    _gActor403900NeoArkPowerPlant2Regions,
};

/// Area-indexed Bishop GOLEM sound-set selectors for `GAME_STAGE_AKROPOLIS`.
///
/// Zero skips bank loading and the two animation cues; 1..4 select the sound
/// file and animation/impact script rows. The area index must be below 21.
/// The table is borrowed at spawn while this actor package is loaded.
static s16 _gActor403900AcropolisSoundSets[21] = {
    [GAME_AREA_AKROPOLIS_CAFETERIA]   = 2,
    [GAME_AREA_AKROPOLIS_FORKED_ROAD] = 2,
    [GAME_AREA_AKROPOLIS_PROMENADE]   = 2,
};

/// Area-indexed Bishop GOLEM sound-set selectors for `GAME_STAGE_DRYFIELD`.
///
/// Zero skips bank loading and the two animation cues; 1..4 select the sound
/// file and animation/impact script rows. The area index must be below 39.
/// The table is borrowed at spawn while this actor package is loaded.
static s16 _gActor403900DryfieldSoundSets[39] = {
    [GAME_AREA_DRYFIELD_MAIN_STREET]   = 4,
    [GAME_AREA_DRYFIELD_BACK_STREET]   = 4,
    [GAME_AREA_DRYFIELD_PARKING_LOT]   = 4,
    [GAME_AREA_DRYFIELD_MOTEL_BALCONY] = 2,
};

/// Area-indexed Bishop GOLEM sound-set selectors for `GAME_STAGE_DRYFIELD_NIGHT`.
///
/// Zero skips bank loading and the two animation cues; 1..4 select the sound
/// file and animation/impact script rows. The area index must be below 39.
/// The table is borrowed at spawn while this actor package is loaded.
static s16 _gActor403900DryfieldNightSoundSets[39] = {
    [GAME_AREA_DRYFIELD_NIGHT_GAS_STATION] = 4,
    [GAME_AREA_DRYFIELD_NIGHT_BACK_STREET] = 4,
    [GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT] = 4,
    [GAME_AREA_DRYFIELD_NIGHT_JUNK_YARD]   = 4,
    [GAME_AREA_DRYFIELD_NIGHT_UNDERPASS]   = 4,
};

/// Area-indexed Bishop GOLEM sound-set selectors for `GAME_STAGE_MINE_SHELTER`.
///
/// Zero skips bank loading and the two animation cues; 1..4 select the sound
/// file and animation/impact script rows. The area index must be below 50.
/// The table is borrowed at spawn while this actor package is loaded.
static s16 _gActor403900MineShelterSoundSets[50] = {
    [GAME_AREA_MINE_MESA]                             = 4,
    [GAME_AREA_MINE_CAVERN]                           = 4,
    [GAME_AREA_SHELTER_B1_ELEVATOR_HALL]              = 3,
    [GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY]  = 1,
    [GAME_AREA_SHELTER_B1_STOREROOM]                  = 2,
    [GAME_AREA_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY]  = 1,
    [GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS]          = 3,
    [GAME_AREA_SHELTER_B1_MAIN_CORRIDOR]              = 2,
    [GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL]          = 2,
    [GAME_AREA_SHELTER_B1_ACCESS_TUNNEL]              = 3,
    [GAME_AREA_SHELTER_B1_TRANSFER_TUNNEL]            = 1,
    [GAME_AREA_SHELTER_B1_CONTROL_ROOM_ACCESS_TUNNEL] = 1,
    [GAME_AREA_SHELTER_B2_ELEVATOR_HALL]              = 3,
    [GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY]  = 1,
    [GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY]  = 1,
    [GAME_AREA_SHELTER_B2_BREEDING_ROOM]              = 2,
    [GAME_AREA_SHELTER_B2_MAIN_CORRIDOR]              = 2,
    [GAME_AREA_SHELTER_B2_SEPTIC_TANK]                = 2,
    [GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL]          = 1,
    [GAME_AREA_SHELTER_B4_LOWER_SEWER]                = 2,
    [GAME_AREA_SHELTER_B4_UPPER_SEWER]                = 2,
};

/// Area-indexed Bishop GOLEM sound-set selectors for `GAME_STAGE_SHELTER_NEO_ARK`.
///
/// Zero skips bank loading and the two animation cues; 1..4 select the sound
/// file and animation/impact script rows. The area index must be below 34.
/// The table is borrowed at spawn while this actor package is loaded.
static s16 _gActor403900ShelterNeoArkSoundSets[34] = {
    [GAME_AREA_SHELTER_1F_VEHICULAR_AIRLOCK] = 2,
    [GAME_AREA_SHELTER_1F_BULWARK]           = 2,
    [GAME_AREA_SHELTER_1F_AIRLOCK]           = 2,
    [GAME_AREA_NEO_ARK_NORTH_PROMENADE]      = 2,
    [GAME_AREA_NEO_ARK_FOREST_ZONE]          = 4,
    [GAME_AREA_NEO_ARK_PAVILION]             = 2,
    [GAME_AREA_NEO_ARK_ISLAND]               = 2,
    [GAME_AREA_NEO_ARK_POWER_PLANT_2]        = 2,
    [GAME_AREA_NEO_ARK_SAVANNA_ZONE]         = 4,
    [GAME_AREA_NEO_ARK_SOUTH_PROMENADE]      = 2,
    [GAME_AREA_NEO_ARK_SHRINE]               = 2,
    [GAME_AREA_NEO_ARK_BRIDGE]               = 1,
    [GAME_AREA_NEO_ARK_PYRAMID]              = 4,
};

/// Per-area sound-set selector tables indexed by `GAME_STAGE_*` (0..5).
///
/// Stage 0 has no table. Area bounds for stages 1..5 are 21, 39, 39, 50 and
/// 34 entries; selectors 0..4 control bank loading and motion/impact scripts.
/// Spawn copies the selector, while these carrier-owned tables remain loaded.
static s16* _gGolemKnightBishopStageSoundSets[6] = {
    [GAME_STAGE_NONE]            = NULL,
    [GAME_STAGE_AKROPOLIS]       = _gActor403900AcropolisSoundSets,
    [GAME_STAGE_DRYFIELD]        = _gActor403900DryfieldSoundSets,
    [GAME_STAGE_DRYFIELD_NIGHT]  = _gActor403900DryfieldNightSoundSets,
    [GAME_STAGE_MINE_SHELTER]    = _gActor403900MineShelterSoundSets,
    [GAME_STAGE_SHELTER_NEO_ARK] = _gActor403900ShelterNeoArkSoundSets,
};

/// Player clips borrowed for the GOLEM grab: start, hold, kill and release.
///
/// Index 0 is unused. The player retains the bank and clip data through playback,
/// so the carrier must remain loaded until scripted control is returned.
#include "assets/enemies/golem/bishop/player_grab_bank.inc"

/// Writable puff-spawn record used at the fatal grab's killing frame.
///
/// The first call caches the player's part-4 coordinate in `coord`; it must remain
/// live for later calls. `spawnArgHi` requests one repeated puff; this effect kind
/// ignores the retained low-halfword value 300.
static EffectSpawnArg _gGolemKnightBishopGrabKillEffectArg = { NULL, 300, 1 };

/// Vertex indices of the two fading ribbons around one aim-beam segment.
///
/// Each row selects four corners from the six screen-space vertices: 0/1 are
/// the beam centreline, 2/3 and 4/5 the two outer edges.
static s16 _gGolemKnightBishopBeamQuadCorners[2][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 4, 5 },
};

TaskDesc gActor403900GolemTaskDesc = { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _actor403900GolemTask, { .model = &_gGolemBody } };

/// GOLEM animation bank indexed by `GOLEM_KNIGHT_BISHOP_ANIM_`.
///
/// Index 0 is unused; clips 1..21 drive the nineteen-part rig. The bank and
/// its clip data are borrowed by live slots for the carrier's loaded lifetime.
#include "assets/enemies/golem/bishop/bank.inc"

#include "../../shared/creatures/golem/golem_knight_bishop_take_hits.inc.c"

/// Bishop GOLEM task phases: spawn setup, living frame and death frame.
///
/// Indexed by `Task::state` in 0..2; the table has no terminator and is copied
/// before dispatch. Each callback receives the borrowed Enemy followed by Task.
/// The live model, work and this overlay must survive through the selected phase.
static const EnemyTaskFuncTable3 _gActor403900GolemTaskStates = {
    .funcs = {
        _golemKnightBishopSpawn,
        _golemKnightBishopFrameState,
        _golemKnightBishopDeadState,
    },
};

#include "../../shared/creatures/golem/golem_knight_bishop.inc.c"

#include "../../shared/screen/frame_capture.inc.c"

#include "../../shared/creatures/golem/golem_knight_bishop_spawn.inc.c"

/// Records the living player's completed struggle as a GOLEM grab escape.
///
/// The release-hold message has no payload; messageId and both arguments are
/// ignored. Requires initialized Knight/Bishop work. A dead player leaves the
/// grab latch intact. Returns zero; the grab sequence performs the release.
static s32 _actor403900RecordGrabEscape(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    if (gPlayerStatus.hp > 0) {
        ((GolemKnightBishopWork*)task->work)->grabBreak = GOLEM_KNIGHT_BISHOP_GRAB_BREAK_STRUGGLE;
    }
    return 0;
}

#include "../../shared/creatures/golem/golem_knight_bishop_frame_capture.inc.c"

/// Dispatches the Bishop GOLEM's spawn, living frame or death frame.
///
/// Task state must be 0, 1 or 2, respectively; dispatch is unchecked. The second
/// spawn argument borrows a live Enemy. Spawn establishes task-owned work;
/// later frames require its model, animation and enemy storage until teardown.
static void _actor403900GolemTask(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = _gActor403900GolemTaskStates;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}
