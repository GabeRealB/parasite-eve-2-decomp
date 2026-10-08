#include "actor_403600_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_403600.h"

#include "actors/actor.h"

#include "actors/actor_303600.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/shelter_b2_pod_bottom.h"
#include "../../shared/frame_capture.h"

/// One row of the table that sizes the boss's recoil from a hit's damage.
///
/// The rows run in rising order of `minDamage` and a hit takes the last row
/// it reaches, so a hit below the first row's damage leaves the recoil as it
/// was. Only the boss's own hits are sized this way, and only while it is in
/// `ACTOR_403600_MODE_FIGHT`.
typedef struct {
    s16 minDamage;   // Least damage of one hit the row applies from
    s16 recoilSpeed; // Speed the hit takes off the boss's forward speed, world units a frame
    u16 recoilHold;  // Frames that speed holds before it decays
    u16 unknown_6;   // Zero in every row and never read; role unproven
} _Actor403600RecoilRow;
STATIC_ASSERT_SIZEOF(_Actor403600RecoilRow, 0x8);

/// Scratch-stack block of the routine that picks, from where the player
/// stands, the fixed point of the room the boss flies to next.
///
/// The candidates are one of two sets of those points: seven, or two. The
/// player's ground distance from each candidate is measured in turn and kept,
/// and the kept distances decide the point. One block serves one choice and is
/// released before the routine returns.
typedef struct {
    s32 deltaX;       // Player's offset on X from the candidate being measured, world units; overwritten for each candidate
    s32 deltaZ;       // The same on Z
    s32 distances[7]; // Ground distance of the player from each candidate, in the set's order; the set of two fills only the first two
} _Actor403600NearestPointScratch;
STATIC_ASSERT_SIZEOF(_Actor403600NearestPointScratch, 0x24);

/// Scratch-stack block of the turn that points the boss at what it aims for.
///
/// The aim is the player or the boss's flight target. A basis is built whose
/// Z axis runs from the boss to the aim, and its Euler angles are where the
/// boss should face. The boss takes them at once, or steps each of its own
/// angles toward them by its turn rate; either way its roll is then added
/// about Z and its rotation rebuilt. Both vectors serve several stages in
/// turn. One block serves one turn and is released before the routine returns.
typedef struct {
    SVECTOR angles;    // Up axis (0, 4096, 0) the basis is built about; then the Euler angles of the basis, 4096 to a turn; last the Z axis of the boss's rebuilt rotation, whose `vx` and `vz` give its yaw. `pad` is never written
    SVECTOR direction; // Low 16 bits of the aim's offset from the boss, world units, then that offset as a unit vector (4096 = 1); the gradual turn then keeps the boss's own Euler angles here as they step toward `angles`. `pad` is never written
    MATRIX  basis;     // Rotation that faces the aim, upright; its translation is never set or read
} _Actor403600AimTurnScratch;
STATIC_ASSERT_SIZEOF(_Actor403600AimTurnScratch, 0x30);

/// Scratch-stack block of the step that puts the boss at the start of a rush
/// pass.
///
/// Every pass but the last starts on a circle about the room's centre. The
/// step reserves one block, works out the bearing the pass starts from, turns
/// an arm of the circle's radius to that bearing and moves the boss to its
/// end. An even pass takes the bearing from the player and leaves the
/// opposite one for the odd pass after it, which needs only the arm. The
/// block is released before the step returns.
typedef struct {
    SVECTOR offset;   // Player's offset from the room's centre on X and Z, the bearing's operands; then the arm: the radius along Z, turned by `rotation` into the boss's offset from the centre
    MATRIX  rotation; // Identity turned about Y by the pass's bearing
    s32     bearing;  // Even pass only: bearing of the player from the room's centre, 4096 to a turn, negated while the player stands in the room's middle
} _Actor403600RushPassScratch;
STATIC_ASSERT_SIZEOF(_Actor403600RushPassScratch, 0x2C);

extern SVECTOR D_actor_403600_8016063C[2];
extern SVECTOR D_actor_403600_8016064C[2];
extern u8      D_actor_403600_80160694;
extern u8      D_actor_403600_80160695;

extern SVECTOR  D_actor_403600_801605D4;
extern SVECTOR  D_actor_403600_801605DC;
extern SVECTOR  D_actor_403600_801605E4;
extern SVECTOR  D_actor_403600_801605EC;
extern TaskDesc D_actor_403600_80160514[];
extern Task*    D_actor_403600_801606B0;
/// Static storage for the placements the package gives the player.
///
/// `placement` is the payload of `GAME_ACTOR_MESSAGE_PLACE`, lent to the player
/// for the length of the dispatch, which consumes it. The boss's knock-back
/// of the player fills it in to turn them to face one of two fixed points,
/// either where they stand or from the other of the two; the scene commands
/// fill it in to pose the player; and the scene figure re-places the player
/// every frame, turning them a little further each time.
///
/// Eight zero bytes separate the record from the next object. No access to
/// them is recovered, so whether they are trailing fields of this object or a
/// separate unreferenced variable is unproven; they stay in this allocation
/// only to keep the data after it at its address.
typedef struct {
    ActorTransform placement;     // Record the player borrows; the scene figure's yaw carries over from one frame's placement to the next
    u8             unknown_18[8]; // Zero in the image; no access established and role unproven
} _Actor403600TransformStorage;
STATIC_ASSERT_SIZEOF(_Actor403600TransformStorage, 32);

extern _Actor403600TransformStorage D_actor_403600_801606E0;

extern Actor303600RotSample D_actor_303600_8016A408[ACTOR_303600_ROT_SAMPLE_COUNT];
extern Actor303600ViewKey   D_actor_303600_8016AEF8[ACTOR_303600_VIEW_KEY_COUNT];
extern SVECTOR              D_actor_403600_8016065C;
extern SVECTOR              D_actor_403600_80160664;

extern ViewCamera D_actor_403600_80160700;

static void _actor403600SpawnBoss(struct Enemy* enemy, Task* task);
static void _actor403600UpdateBoss(Enemy* enemy, Task* task);
static void _actor403600PlaceRushPass(Task* task);
static u8*  _actor403600QueueBodyFrameCapture(Task* task);
static void _actor403600ChooseFlightTarget(Task* task, s32 useRushReferences);
static s32  _actor403600CheckRushPass(Task* task);
static void _actor403600ApplyDamage(Task* task, s32 damage);
static s32  _actor403600TurnYawToAim(Task* task, s16 turnStep);
static s32  _actor403600TurnToAim(Task* task);
static void _actor403600MeasurePlayerRangeBearing(const GfxCoord* referenceCoord, u32* rangeOut, s32* bearingOut);
static s16  _actor403600BearingFromPlayer(const GfxCoord* targetCoord);
static s32  _actor403600PlacePlayerForKnockback(Task* unusedTask, u16 placementFlags);
static void _actor403600ChooseAttack(Task* task);
static void _actor403600UpdateDrainPuffs(Task* task);
static void _actor403600AdvanceRoll(Task* task, s32 rollStep);
static s32  _actor403600ApproachTarget(Task* task);
static void _actor403600RaisePlayerMpForDrain(Task* task);
static s32  _actor403600ApplySceneCommand(Task* task, s32 unusedMessageId, const ActorCommand* request, s32 unusedSecondArg);
static void func_actor_403600_80140B4C(struct Enemy* arg0, Task* arg1);
static void _actor403600ScaleSceneFigure(GfxCoord* coord, s32 uniformScale);

extern TaskDesc D_actor_303600_80162E98[];
/// Models effect 0x80005 spawns, set in `D_800626EC[5].data.model`.
extern Task* D_actor_403600_801606B4;

extern TaskDesc              D_actor_303600_8016E468[];
extern AnimationPlayRequest  D_actor_403600_80160568;
extern AnimationSet*         D_actor_403600_8016057C[22];
extern SVECTOR               D_actor_403600_801605F4[];
extern DamageAttack          D_actor_403600_801606A4;
extern Task*                 D_actor_403600_801606A8;
extern _Actor403600RecoilRow D_actor_403600_8016066C[];

// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_403600_80160504[2];
extern u16              D_actor_403600_801606B8[2];
extern Task*            D_actor_403600_801606AC;
/// Static storage for the cursor of the boss's record of its last two
/// attacks.
///
/// Each attack the boss picks is written to the record at `nextSlot`, which
/// then flips, so the record always holds the two picks before the one being
/// made.
///
/// Thirty-two zero bytes separate the cursor from the next object. No access
/// to them is recovered, so whether they are trailing fields of this object
/// or a separate unreferenced variable is unproven; they stay in this
/// allocation only to keep the data after it at its address.
typedef struct {
    s32 nextSlot;      // Element of the record the next pick overwrites, 0 or 1; cleared when the boss is created
    u8  unknown_4[32]; // Zero in the image; no access established and role unproven
} _Actor403600RecentAttackSlotStorage;
STATIC_ASSERT_SIZEOF(_Actor403600RecentAttackSlotStorage, 36);

extern _Actor403600RecentAttackSlotStorage D_actor_403600_801606BC;

static void _actor403600EnemyExit(Task* task);
static void _actor403600ResetCombatState(Task* task);

static void _actor403600UpdateFightAction(Task* task);
static void _actor403600BindShedPartTextures(Task* task);
static void _actor403600ProcessStatusReactions(Task* task);
static void _actor403600UpdateMode(Task* task);
static void _actor403600ProcessContacts(Task* task);
static void _actor403600ApplyDoubleDamage(Task* task, s32 damage);
static void _actor403600Move(Task* task);
static void _actor403600UpdatePlayerKnockback(Task* task);
static void _actor403600TickBossAnimation(Task* task, u8 partCount);
static void _actor403600SampleBossColor(Enemy* enemy, Task* task);
static void _actor403600ApplyBossFlinch(Task* task);
static void _actor403600UpdateScreenShake(Task* task);
static void _actor403600UpdateWeakPhase(Task* task);
static void _actor403600SetWeakTextures(s32 weakAppearance);
static void _actor403600UpdateFaceTexture(Task* task);
static void _actor403600CancelDrain(Task* task);
static void _actor403600UpdateDoubleMode(Task* task);

static void _actor403600UpdateDoubleAction(Task* task);
static void _actor403600SceneFigureExit(Task* task);
static void _actor403600FadeDouble(Enemy* enemy, Task* task);
static void func_actor_403600_80141D30(Enemy* arg0, Task* arg1);
static void _actor403600WaitSceneFigure(Enemy* unusedEnemy, Task* task);

void func_actor_403600_80141180(Task*);
void func_actor_403600_80141BE0(Task*);
void func_actor_403600_80141CD4(Task*);

TaskMessageEntry D_actor_403600_80160504[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor403600ApplySceneCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_403600_80160514[3] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_403600_80141180, { .model = &gActor403600EveBody } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_403600_80141BE0, { .model = &gActor403600Model199B8 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_403600_80141CD4, { .model = &gActor303600Model02DD0 } },
};

AnimationSet* D_actor_403600_80160538[12] = {
    NULL,
    &gActor403600Animation2C0C4,
    &gActor403600Animation2BF8C,
    &gActor403600Animation2C90C,
    &gActor403600Animation2D0A8,
    &gActor403600Animation2D8B4,
    &gActor403600Animation2E0C8,
    &gActor403600Animation2D0A8,
    &gActor403600Animation2E0C8,
    &gActor303600Animation077F0,
    &gActor303600Animation07C30,
    &gActor303600Animation07E5C,
};

AnimationPlayRequest D_actor_403600_80160568 = { { .sets = D_actor_403600_80160538 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationSet* D_actor_403600_8016057C[22] = {
    NULL,
    &gActor403600Animation1FEB0,
    &gActor403600Animation208EC,
    &gActor403600Animation2139C,
    &gActor403600Animation22424,
    &gActor403600Animation22904,
    &gActor403600Animation237A8,
    &gActor403600Animation241DC,
    &gActor403600Animation24C54,
    &gActor403600Animation25670,
    &gActor403600Animation26650,
    &gActor403600Animation27184,
    &gActor403600Animation27A60,
    &gActor403600Animation283F8,
    &gActor403600Animation29234,
    &gActor403600Animation29674,
    &gActor403600Animation29CB4,
    &gActor403600Animation2A708,
    &gActor403600Animation2A8EC,
    &gActor403600Animation2B364,
    &gActor403600Animation2E6BC,
    &gActor303600Animation075A0,
};

SVECTOR D_actor_403600_801605D4 = { 8256, -0x61A8, 6883, 0 };

SVECTOR D_actor_403600_801605DC = { 8000, -4000, 6600, 0 };

SVECTOR D_actor_403600_801605E4 = { 2029, -5000, 0x332F, 0 };

SVECTOR D_actor_403600_801605EC = { 0x36CD, -5000, 954, 0 };

SVECTOR D_actor_403600_801605F4[9] = {
    { 4740, 0, 3740, 0 },
    { 4220, 0, 7030, 0 },
    { 8100, 0, 3060, 0 },
    { 8380, 0, 7030, 0 },
    { 0x2C38, 0, 0x2756, 0 },
    { 0x2F44, 0, 7000, 0 },
    { 7940, 0, 0x2AF8, 0 },
    { 6080, 0, 4896, 0 },
    { 0x2740, 0, 8736, 0 },
};

SVECTOR D_actor_403600_8016063C[2] = {
    { 1376, -9000, 0x3540, 0 },
    { 0x3860, -9000, 480, 0 },
};

SVECTOR D_actor_403600_8016064C[2] = {
    { 2400, -1000, 1370, 0 },
    { 0x3520, -1000, 0x3138, 0 },
};

SVECTOR D_actor_403600_8016065C = { 1024, 0, 0, 0 };

SVECTOR D_actor_403600_80160664 = { 0, 1200, 0, 0 };

_Actor403600RecoilRow D_actor_403600_8016066C[5] = {
    { 40, 10, 5, 0 },
    { 60, 20, 10, 0 },
    { 80, 30, 15, 0 },
    { 100, 40, 15, 0 },
    { 120, 50, 15, 0 },
};

u8 D_actor_403600_80160694 = 0;

u8 D_actor_403600_80160695 = 0;

u16 D_actor_403600_80160696 = 0x2FBC;

s32 D_actor_403600_80160698 = 0;

u8* D_actor_403600_8016069C = NULL;

GfxCoord* gActor403600RipplePlaneCoord = NULL;

DamageAttack D_actor_403600_801606A4 = { 0, 0 };

Task* D_actor_403600_801606A8 = NULL;

Task* D_actor_403600_801606AC = NULL;

Task* D_actor_403600_801606B0 = NULL;

Task* D_actor_403600_801606B4 = NULL;

/// The boss's last two attack picks, as the chooser numbers them. An attack
/// that fills both elements is not picked a third time running.
u16 D_actor_403600_801606B8[2] = { 0, 0 };

_Actor403600RecentAttackSlotStorage D_actor_403600_801606BC = { 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };

_Actor403600TransformStorage D_actor_403600_801606E0;

ViewCamera D_actor_403600_80160700;

enum {
    ACTOR_403600_DEFAULT_BLEND_FRAMES = 8,
    ACTOR_403600_DEFAULT_ACTION_DELAY = 10,
    ACTOR_403600_DEFAULT_TURN_RATE    = 64
};

enum {
    ACTOR_403600_RUSH_CIRCLE_Z_EDGE        = 15000,
    ACTOR_403600_RUSH_PLAYER_HEIGHT_OFFSET = 1000,
    ACTOR_403600_RUSH_FINAL_START_Y        = -2400,
    ACTOR_403600_RUSH_FINAL_TARGET_Y       = -8000,
    ACTOR_403600_RUSH_FINAL_PASS           = 0xFF,
    ACTOR_403600_RUSH_INNER_X              = 3500,
    ACTOR_403600_RUSH_INNER_Z              = 2000,
    ACTOR_403600_RUSH_INNER_X_SPAN         = 8501,
    ACTOR_403600_RUSH_INNER_Z_SPAN         = 10001,
    ACTOR_403600_RUSH_EVEN_WAIT_FRAMES     = 10,
    ACTOR_403600_RUSH_ODD_WAIT_FRAMES      = 150
};

/// Drain presentation limits shared by its selector, action and puff tick.
enum {
    ACTOR_403600_DRAIN_BREAK_DAMAGE        = 200,
    ACTOR_403600_DRAIN_MP_LOSS             = 251,
    ACTOR_403600_DRAIN_PUFF_ALL_PARTS      = -1,
    ACTOR_403600_DRAIN_PUFF_FIRST_SIZE     = 256,
    ACTOR_403600_DRAIN_PUFF_MAX_SIZE       = 1024,
    ACTOR_403600_DRAIN_PUFF_FIRST_INTERVAL = 50,
    ACTOR_403600_DRAIN_PUFF_MIN_INTERVAL   = 2,
    ACTOR_403600_DRAIN_PUFF_BURST_INTERVAL = 3,
    ACTOR_403600_DRAIN_PUFF_COORD_LIMIT    = 19
};

/// Rush pass events returned to the fight action machine.
enum {
    ACTOR_403600_RUSH_EVENT_NONE           = 0,
    ACTOR_403600_RUSH_EVENT_BOUNDARY       = 1,
    ACTOR_403600_RUSH_EVENT_CONTACT_EFFECT = 2,
    ACTOR_403600_RUSH_EVENT_HIGH_END       = 3
};

/// Axis count returned by the target approach once every axis is near its goal.
enum { ACTOR_403600_TARGET_AXES_SETTLED = 3 };

/// Animation-table indices used by the boss's combat actions.
enum {
    ACTOR_403600_ANIM_IDLE         = 1,
    ACTOR_403600_ANIM_FLY          = 2,
    ACTOR_403600_ANIM_RETREAT      = 3,
    ACTOR_403600_ANIM_RECHARGE     = 4,
    ACTOR_403600_ANIM_RECHARGE_END = 5,
    ACTOR_403600_ANIM_CHARGE       = 6,
    ACTOR_403600_ANIM_VOLLEY_END   = 7,
    ACTOR_403600_ANIM_SUMMON       = 8,
    ACTOR_403600_ANIM_DRAIN_STRIKE = 10,
    ACTOR_403600_ANIM_MELEE_FIRST  = 12,
    ACTOR_403600_ANIM_MELEE_SECOND = 13,
    ACTOR_403600_ANIM_DIVE_WINDUP  = 16,
    ACTOR_403600_ANIM_DIVE_RECOVER = 17,
    ACTOR_403600_ANIM_DIVE         = 18,
    ACTOR_403600_ANIM_RUSH_CHARGE  = 19
};

/// Player position classifications used when choosing a combat attack.
enum {
    ACTOR_403600_PLAYER_ZONE_CENTRE  = 1,
    ACTOR_403600_PLAYER_ZONE_FLOOR   = 2,
    ACTOR_403600_PLAYER_ZONE_OUTSIDE = 3
};

/// Player-animation requests during the boss's scripted knockback.
enum {
    ACTOR_403600_PLAYER_ANIM_NONE            = 0,
    ACTOR_403600_PLAYER_ANIM_HEAVY_KNOCKBACK = 1,
    ACTOR_403600_PLAYER_ANIM_BACKWARD        = 3,
    ACTOR_403600_PLAYER_ANIM_FORWARD         = 4
};

/// Additional animation and player-script slots used by the mode and double handlers.
enum {
    ACTOR_403600_ANIM_DOUBLE_APPEAR           = 9,
    ACTOR_403600_ANIM_STAGGER                 = 11,
    ACTOR_403600_ANIM_STUN_HOLD               = 14,
    ACTOR_403600_ANIM_STUN_RELEASE            = 15,
    ACTOR_403600_ANIM_SCENE_HOVER             = 20,
    ACTOR_403600_PLAYER_ANIM_HEAVY_IMPACT     = 2,
    ACTOR_403600_PLAYER_ANIM_BACKWARD_RECOVER = 5,
    ACTOR_403600_PLAYER_ANIM_FORWARD_RECOVER  = 6
};

/// Weakened-phase puff: size 2048, two ticks per cell, rising in view space.
enum { ACTOR_403600_WEAK_PUFF_ARG = 2048 | (2 << 12) | (1 << 16) };

/// Stages of the weakened appearance, independent of the current combat mode.
enum {
    ACTOR_403600_WEAK_PHASE_NONE       = 0,
    ACTOR_403600_WEAK_PHASE_ACTIVE     = 1,
    ACTOR_403600_WEAK_PHASE_RECOVERING = 2
};

/// Scene selectors accepted by this package's actor-command handler.
///
/// Only command is inspected here; the request's context tags are ignored.
enum {
    ACTOR_403600_COMMAND_POSE_PLAYER_AND_BOSS  = 1,
    ACTOR_403600_COMMAND_PLAY_SCENE_ANIMATIONS = 2,
    ACTOR_403600_COMMAND_BRIGHTEN_BOSS         = 3,
    ACTOR_403600_COMMAND_SPAWN_SCENE_FIGURE    = 4,
    ACTOR_403600_COMMAND_FADE_SCENE_FIGURE     = 5,
    ACTOR_403600_COMMAND_ASCEND_TO_ARENA       = 6,
    ACTOR_403600_COMMAND_HIDE_BOSS             = 8,
    ACTOR_403600_COMMAND_FINISH_BATTLE         = 9,
};

/// Clips and timing used by scene commands, distinct from combat animations.
enum {
    ACTOR_403600_ANIM_SCENE_PAIRED           = 21,
    ACTOR_403600_PLAYER_ANIM_SCENE_POSE      = 9,
    ACTOR_403600_PLAYER_ANIM_SCENE_PAIRED    = 10,
    ACTOR_403600_PLAYER_ANIM_FIGURE_FADE     = 11,
    ACTOR_403600_SCENE_ASCENT_BACKWARD_SPEED = -80,
    ACTOR_403600_BATTLE_END_DELAY_FRAMES     = 5
};

static s32             _actor403600Are32HalfwordsZero(const s16* values);
static __inline__ u8*  _actor403600QueueCoordFrameCapture(GfxCoord* coord);
static inline void     _actor403600InitRushArm(_Actor403600RushPassScratch* scratch);
static inline void     _actor403600PlaceOnRushCircle(Actor403600Work* work, _Actor403600RushPassScratch* scratch);
static inline u32      _actor403600Rand(void);
static __inline__ void _actor403600UpdateAnimation(Task* task, u8 partCount);
static void            _actor403600SpawnDouble(Enemy* enemy, Task* task);
static __inline__ void _actor403600UpdateColor(Enemy* enemy, Task* task);
static __inline__ void _actor403600ApplyDoubleFlinch(Task* task);
static void            _actor403600UpdateDouble(Enemy* enemy, Task* task);
static inline void     _actor403600ResetState(Task* task);

void actor403600LoosePartsTask(Task* task)
{
    Task* fxTask;

    fxTask = task->parent;
    actor403600SwingLooseParts(fxTask, fxTask->parent->work, fxTask->work);
}

void actor403600ProjectileExit(Task* task)
{
    Actor403600ProjectileWork* work = task->work;

    worldCollisionUnlinkBody(&work->attackBody);
    taskKill(task);
}

void actor403600TickRipple(Actor403600Ripple* state)
{
    enum {
        ACTOR_403600_RIPPLE_FULL_STRENGTH      = ONE,
        ACTOR_403600_RIPPLE_STRENGTH_RISE      = 512,
        ACTOR_403600_RIPPLE_STRENGTH_FALL      = 128,
        ACTOR_403600_RIPPLE_PHASE_STEP         = 384,
        ACTOR_403600_RIPPLE_SHALLOW_PHASE_STEP = 256
    };
    s32 head;

    state->head          += ACTOR_403600_RIPPLE_SAMPLE_COUNT - 1;
    state->head          %= ACTOR_403600_RIPPLE_SAMPLE_COUNT;
    head                  = state->head;
    state->phase[head]    = 0;
    state->strength[head] = 0;
    if (state->emitting != 0) {
        if (state->wasEmitting == 0) {
            state->sourcePhase = 0;
        }
        if (state->sourceStrength < ACTOR_403600_RIPPLE_FULL_STRENGTH) {
            state->sourceStrength += ACTOR_403600_RIPPLE_STRENGTH_RISE;
        }
    } else if (state->sourceStrength > 0) {
        state->sourceStrength -= ACTOR_403600_RIPPLE_STRENGTH_FALL;
    }
    state->wasEmitting = state->emitting;
    if (state->sourceStrength != 0) {
        state->phase[head]    = state->sourcePhase;
        state->strength[head] = state->sourceStrength;
        if (state->shallow == 0) {
            state->sourcePhase += ACTOR_403600_RIPPLE_PHASE_STEP;
        } else {
            state->sourcePhase += ACTOR_403600_RIPPLE_SHALLOW_PHASE_STEP;
        }
    }
}

/// Returns 1 when all 32 borrowed halfwords are zero, otherwise 0.
///
/// Requires 32 readable s16 elements. Stops at the first nonzero element;
/// no recovered caller establishes which buffer this standalone scan serves.
static s32 _actor403600Are32HalfwordsZero(const s16* values)
{
    enum { ACTOR_403600_ZERO_SCAN_HALFWORDS = 32 };
    s32 halfwordIndex;

    for (halfwordIndex = 0; halfwordIndex < ACTOR_403600_ZERO_SCAN_HALFWORDS; halfwordIndex++, values++) {
        if (*values != 0) {
            return 0;
        }
    }
    return 1;
}

/// Queues frame capture 30 ordering-table tags beyond a coordinate's origin.
///
/// Composes the borrowed coordinate and projects local (0,0,0). Negative GTE
/// FLAG uses zero depth before the bias. Requires initialized scratch/GTE state
/// and a biased slot within the capture ordering table. Releases its scratch
/// block and returns the restored byte cursor, never the released workspace.
static __inline__ u8* _actor403600QueueCoordFrameCapture(GfxCoord* coord)
{
    enum { ACTOR_403600_CAPTURE_DEPTH_SHIFT = 4,
           ACTOR_403600_CAPTURE_TAG_BIAS    = 30 };
    ActorOriginDepthScratch* block;

    block            = SCRATCH_STACK_RESERVE_BLOCK(ActorOriginDepthScratch);
    block->origin.vx = 0;
    block->origin.vy = 0;
    block->origin.vz = 0;
    actorRenderComposeCoord(coord);
    gte_SetRotMatrix(&coord->workm);
    gte_SetTransMatrix(&coord->workm);
    gte_ldv0(&block->origin);
    gte_rtps();
    gte_stsxy(&block->screenPos);
    gte_stdp(&block->depthCue);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->flag < 0) {
        block->otz = 0;
    }
    block->otz = (block->otz >> ACTOR_403600_CAPTURE_DEPTH_SHIFT) + ACTOR_403600_CAPTURE_TAG_BIAS;
    frameCaptureQueue(block->otz);
    return (u8*)SCRATCH_STACK_RELEASE_BLOCK(ActorOriginDepthScratch);
}

/// Queues capture beyond model coordinate 1 and returns the restored scratch cursor.
///
/// Requires a live model with coordinate 1; uses the projection and lifetime
/// contract of `_actor403600QueueCoordFrameCapture`.
static u8* _actor403600QueueBodyFrameCapture(Task* task)
{
    return _actor403600QueueCoordFrameCapture(&task->extra.tmd->coords[1]);
}

/// Binds the boss rig to its model and seeds body tracks 1..19 with idle.
///
/// Work, the twenty-coordinate model and its animation table must stay live
/// through playback. Slot 0 is the root and is left unseeded.
static inline void _actor403600InitBossAnimationRig(Actor403600Work* work, TmdObject* object)
{
    s32 slotIndex;

    slotIndex = 1;
    animationInitContext(&work->rig.anim, D_actor_403600_8016057C, object, work->rig.poses, work->rig.slots);
    do {
        animationResetSlot(&work->rig.anim, slotIndex, ACTOR_403600_ANIM_IDLE);
        slotIndex += 1;
    } while (slotIndex < (s32)ARRAY_SIZE(work->rig.slots));
}

/// Creates the singleton boss, its animation rig and its linked collision state.
///
/// Requires an enemy/model with twenty coordinates. Owns one zeroed work block;
/// allocation failure destroys the enemy. Moves the model's existing placement
/// into a work-owned room coordinate and preserves its heading. Initial HP is
/// the kind's base plus 75% of the session's boss-parts HP sum, narrowed to s16.
/// Acquires a battle reference, creates the display child and enters parked mode.
static void _actor403600SpawnBoss(Enemy* enemy, Task* task)
{
    s32                    previousTaskState;
    SVECTOR                headingAngles;
    s16                    initialHp;
    s16                    initialYaw;
    GfxCoord*              bodyCoord;
    Task*                  displayTask;
    GfxCoord*              worldCoord;
    GfxCoord*              rootCoord;
    WorldCollisionContact* hitContacts;
    WorldCollisionContact* attackContacts;
    WorldCollisionContact* gridContacts;
    TmdObject*             object;
    Actor403600Work*       work;
    GameSession*           session;

    object    = task->extra.tmd;
    rootCoord = object->coords;
    work      = memCalloc(sizeof(*work), false);
    bodyCoord = &rootCoord[1];
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // Keep placement in work so animation may replace the model root basis.
    task->work              = work;
    work->worldCoord.parent = &gGfxViewCoord;
    gfxSetRotIdentity(&work->worldCoord.coord);
    work->worldCoord.coord.t[0] = rootCoord->coord.t[0];
    work->worldCoord.coord.t[1] = rootCoord->coord.t[1];
    worldCoord                  = &work->worldCoord;
    work->worldCoord.coord.t[2] = rootCoord->coord.t[2];
    rootCoord->parent           = worldCoord;
    gfxSetRotIdentity(&rootCoord->coord);
    rootCoord->coord.t[0]         = 0;
    rootCoord->coord.t[1]         = 0x744;
    rootCoord->coord.t[2]         = 0;
    work->worldCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(worldCoord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    object->flags           = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    object->lightMtx        = &work->light;
    object->colorMtx        = &work->color;
    // Link target and contact state before enabling the fight.
    enemy->field_4  = &rootCoord[1].coord;
    enemy->field_48 = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->bodyPos.vy             = -0x1F4;
    session                       = gGameSession;
    enemy->coord                  = bodyCoord;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &D_actor_403600_80150EC8;
    enemy->recs                   = work->hitContacts;
    initialHp                     = D_actor_403600_80150EC8.hpMax + (((u16)session->bossPartsHpSum * 75) / 100);
    enemy->hp                     = initialHp;
    work->hpMax                   = initialHp;
    work->hpAt60Percent           = (s16)((initialHp * 60) / 100);
    work->hpAt35Percent           = (s16)((work->hpMax * 35) / 100);
    _actor403600InitBossAnimationRig(work, object);
    (sceneAcquireBattleRef)(0);
    work->animId                   = ACTOR_403600_ANIM_IDLE;
    work->hitEffectArg.coord       = &work->worldCoord;
    work->hitEffectArg.spawnArgLo  = 0x600;
    work->hitEffectArg.spawnArgHi  = 2;
    work->hitEffectOffset.vy       = -0x1F4;
    hitContacts                    = work->hitContacts;
    work->appliedAnimId            = 0;
    work->hitCooldown              = 0;
    work->hitEffectOffset.vx       = 0;
    work->hitEffectOffset.vz       = 0xC8;
    work->hitBody.coord            = bodyCoord;
    work->hitBody.context.contacts = hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x24;
    work->hitBody.radius           = 0x3E8;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    attackContacts                    = work->attackContacts;
    work->attackBody.coord            = bodyCoord;
    work->attackBody.context.contacts = attackContacts;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0x3E8;
    work->hitBody.flags               = work->hitBody.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->attackBody.key              = damagePackAttackKey(&D_actor_403600_80150E9C, 1);
    work->attackBody.radius           = 0x5DC;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    gridContacts                    = work->gridContacts;
    work->gridBody.coord            = bodyCoord;
    work->gridBody.context.contacts = gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vz           = 0;
    work->gridBody.pos.vy           = 0x7D0;
    work->gridBody.key              = 0;
    work->gridBody.radius           = 0x64;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->attackBody.flags          = work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->gridBody);
    worldCollisionInitContacts(gridContacts, ARRAY_SIZE(work->gridContacts), 0);
    work->gridBody.flags = work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    // Recover the model heading, then install the singleton presentation task.
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &headingAngles);
    initialYaw       = ratan2(headingAngles.vx, headingAngles.vz);
    headingAngles.vx = 0;
    headingAngles.vy = initialYaw;
    headingAngles.vz = 0;
    RotMatrix(&headingAngles, &work->worldCoord.coord);
    work->yaw               = initialYaw;
    displayTask             = taskSpawnFromTable(D_actor_403600_801421A0, 0, 0, 0);
    D_actor_403600_801606AC = displayTask;
    if (displayTask != 0) {
        taskReparent(task, displayTask);
    }
    work->childEnemy        = 0;
    D_actor_403600_801606A8 = task;
    work->weakPhase         = ACTOR_403600_WEAK_PHASE_NONE;
    work->recoilSpeed       = 0;
    work->recoilHold        = 0;
    work->exposed           = 0;
    _actor403600ResetCombatState(task);
    D_actor_403600_80160568.animationId = 0;
    D_actor_403600_801606B8[1]          = 0;
    D_actor_403600_801606B8[0]          = 0;
    task->msgTable                      = D_actor_403600_80160504;
    task->exitCallback                  = _actor403600EnemyExit;
    work->mode                          = ACTOR_403600_MODE_PARKED;
    previousTaskState                   = task->state;
    D_actor_403600_801606BC.nextSlot    = 0;
    task->state                         = previousTaskState + 1;
}

/// Advances boss control, combat movement, animation and presentation once.
///
/// Requires the live singleton enemy/model/work. Pause samples colour and mutes
/// sound without advancing; hidden control suppresses draw/target state. Running
/// control gates movement, contact and flinch work to combat modes, then updates
/// colour, screen shake and scripted player knockback. Menu entry also mutes sound.
static void _actor403600UpdateBoss(Enemy* enemy, Task* task)
{
    s16              ambientBoost;
    Actor403600Work* work;

    work = task->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->pauseSoundSent != 0) {
                work->pauseSoundSent = 0;
                sndEvtRequestScriptUnmute(SOUND_AREA_BANK_ALL);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor403600SampleBossColor(enemy, task);
            if (work->pauseSoundSent == 0) {
                work->pauseSoundSent = 1;
                sndEvtRequestScriptMute(SOUND_AREA_BANK_ALL);
            }
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = (WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE);
            return;
    }
    if (((gDisplayState.pendingMode & DISPLAY_MODE_MENU_GROUP_MASK) == DISPLAY_MODE_GAME_MENU_GROUP) && (work->pauseSoundSent == 0)) {
        work->pauseSoundSent = 1;
        sndEvtRequestScriptMute(SOUND_AREA_BANK_ALL);
    }
    _actor403600UpdateMode(task);
    if (work->mode != ACTOR_403600_MODE_PARKED) {
        if (work->mode < ACTOR_403600_MODE_SCENE_POSE) {
            _actor403600Move(task);
            _actor403600ProcessStatusReactions(task);
            _actor403600ProcessContacts(task);
        }
    }
    _actor403600TickBossAnimation(task, ARRAY_SIZE(work->rig.slots));
    if (work->mode != ACTOR_403600_MODE_PARKED) {
        if (work->mode < ACTOR_403600_MODE_SCENE_POSE) {
            _actor403600ApplyBossFlinch(task);
            _actor403600UpdateWeakPhase(task);
            _actor403600UpdateFaceTexture(task);
        }
    }
    work->worldCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->worldCoord);
    _actor403600SampleBossColor(enemy, task);
    ambientBoost = work->ambientBoost;
    if (ambientBoost != 0) {
        worldCoordSetModelAmbientColor(task->extra.tmd, ambientBoost, ambientBoost, ambientBoost);
    }
    _actor403600UpdateScreenShake(task);
    _actor403600UpdatePlayerKnockback(task);
}

/// Consumes pending stagger, stun and damage-over-time reactions for the boss.
///
/// Requires the task's live enemy and Actor403600Work. Stun lasts 30 frames per
/// buildup step. Outside rush, a nonzero damage-over-time tick consumes two LCG
/// draws for a signed pitch flinch and applies one fifth of its damage through
/// the boss damage handler. Expired status or HP below 500 clears the status bits.
static void _actor403600ProcessStatusReactions(Task* task)
{
    enum { ACTOR_403600_STUN_FRAMES_PER_STEP    = 30,
           ACTOR_403600_DAMAGE_OVER_TIME_MIN_HP = 500 };
    Actor403600Work* work;
    Enemy*           enemy;
    s32              tickDamage;
    u32              positiveFlinchRandomState;
    u32              negativeFlinchRandomState;
    u32              flinchSignRandomState;
    u8               reactionFlags;

    enemy         = task->spawnArg2.pointer;
    reactionFlags = enemy->reactionFlags;
    work          = task->work;
    if (reactionFlags != 0) {
        if (reactionFlags & ENEMY_REACTION_STAGGER) {
            enemy->reactionFlags = reactionFlags & ENEMY_REACTION_STAGGER_CLEAR;
            work->mode           = ACTOR_403600_MODE_STAGGER;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
            work->mode            = ACTOR_403600_MODE_STUN;
            work->animId          = ACTOR_403600_ANIM_STUN_HOLD;
            work->stunFrames      = D_actor_403600_80150EC8.buildupSteps * ACTOR_403600_STUN_FRAMES_PER_STEP;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            if (work->action != ACTOR_403600_ACTION_RUSH) {
                tickDamage = damageTickEnemyDamageOverTime(enemy);
                if (tickDamage != 0) {
                    flinchSignRandomState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState       = flinchSignRandomState;
                    if ((flinchSignRandomState >> 0x10) & 1) {
                        positiveFlinchRandomState = (flinchSignRandomState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                        gRandomLcgState           = positiveFlinchRandomState;
                        work->flinchRot.vx        = ((positiveFlinchRandomState >> 0xB) & 0x60) + 0x80;
                    } else {
                        negativeFlinchRandomState = (flinchSignRandomState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                        gRandomLcgState           = negativeFlinchRandomState;
                        work->flinchRot.vx        = -(((negativeFlinchRandomState >> 0xB) & 0x60) + 0x80);
                    }
                    _actor403600ApplyDamage(task, tickDamage / 5);
                }
            }
            if ((damageIsEnemyDamageOverTimeExpired(enemy) != 0) || (enemy->hp < ACTOR_403600_DAMAGE_OVER_TIME_MIN_HP)) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
        }
    }
}

/// Restores the boss's combat movement and action defaults.
///
/// Requires a live task with `Actor403600Work`. Clears defeat and active-flight
/// flags, stops movement and roll, aims at the player and returns the action to
/// choosing. Restores normal animation rate, eight-frame blending, a ten-frame
/// action delay and a turn rate of 64 angle units per frame. Leaves mode,
/// animation selection, HP, target, damage history and weakened-phase state intact.
static inline void _actor403600ResetState(Task* task)
{
    Actor403600Work* work = task->work;

    work->defeated        = 0;
    work->animBlendFrames = ACTOR_403600_DEFAULT_BLEND_FRAMES;
    work->animRate        = ANIMATION_RATE_ONE;
    work->aimMode         = ACTOR_403600_AIM_PLAYER;
    work->ignorePushOut   = 0;
    work->ambientBoost    = 0;
    work->committed       = 0;
    work->forwardSpeed    = 0;
    work->action          = ACTOR_403600_ACTION_CHOOSE;
    work->verticalSpeed   = 0;
    work->phaseFrame      = 0;
    work->actionDelay     = ACTOR_403600_DEFAULT_ACTION_DELAY;
    work->turnRate        = ACTOR_403600_DEFAULT_TURN_RATE;
    work->roll            = 0;
    work->diving          = 0;
    work->repositioning   = 0;
    work->pauseSoundSent  = 0;
}

/// Advances the boss's top-level combat reaction or scene mode.
///
/// Requires live boss work, model parts through 19, player and room effects.
/// Fight delegates to the action machine; reaction modes cancel drain and restore
/// combat defaults when their animation/timer ends. Weakening sheds four models,
/// recovery restores the appearance under a white flash, and dying loads actor
/// 303600 before spawning its task. Animation playback advances phaseFrame.
static void _actor403600UpdateMode(Task* task)
{
    // Starts one placed-enemy cue. cue/coord must have no side effects;
    // coord is read twice, and panOut/depthOut are s32/u32 output lvalues.
    // Captures task and soundKey, preserving byte pan and half-depth rounding.
#define ACTOR_403600_PLAY_MODE_SOUND(cue, coord, panOut, depthOut)                                                 \
    {                                                                                                              \
        soundKey   = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | (cue); \
        (panOut)   = (s8)worldCoordGetOriginAudioPan((coord));                                                     \
        (depthOut) = worldCoordGetOriginAudioDepth((coord));                                                       \
        sndEvtRequestScriptStart(soundKey, (panOut), (s32)((((depthOut) >> 31) + (depthOut)) << 23) >> 24);        \
    }

    enum {
        ACTOR_403600_SOUND_RECOVERY_BURST = 0x54160014,
        ACTOR_403600_ASCENT_SPRITE_SIZE   = 3072,
        // Size 3072, four ticks per cell, view-space random drift selector 3.
        ACTOR_403600_ASCENT_SMOKE_ARG           = 3072 | (4 << 12) | (3 << 16),
        ACTOR_403600_SUCCESSOR_FILE_ID          = 303600,
        ACTOR_403600_ASCENT_STEP_RISE           = 0,
        ACTOR_403600_ASCENT_STEP_BOB            = 1,
        ACTOR_403600_ASCENT_STEP_POSE           = 2,
        ACTOR_403600_DEATH_STEP_LOCK_INPUT      = 0,
        ACTOR_403600_DEATH_STEP_LOAD_SUCCESSOR  = 1,
        ACTOR_403600_DEATH_STEP_SPAWN_SUCCESSOR = 2
    };
    SVECTOR          shedHitOffset;
    SVECTOR          repeatHitOffset;
    u8               smokeIndex;
    s32              ascentStep;
    s16              nextRecoveryWhiteout;
    s32              recoveryEndFrame;
    s16              nextSceneWhiteout;
    s16              nextAscentSpeed;
    s32              bobSpeed;
    s16              nextDeathWhiteout;
    s16              nextChargeWhiteout;
    s16              mode;
    s16              weakenFrame;
    s16              chargeFrame;
    s16              deathStep;
    EffectWork*      firstShedEffect;
    EffectWork*      secondShedEffect;
    EffectWork*      thirdShedEffect;
    EffectWork*      fourthShedEffect;
    s32              soundKey;
    s32              nextAscentY;
    s32              chargePan;
    s32              burstPan;
    u16              remainingUpBobs;
    u16              remainingDownBobs;
    u16              remainingPoseUpBobs;
    u16              remainingPoseDownBobs;
    u16              remainingStunFrames;
    u16              remainingFreezeFrames;
    s32              bobYOrSpeedBits;
    u32              chargeDepth;
    u32              burstDepth;
    u8               chargeWhiteout;
    u8               recoveryWhiteout;
    u8               sceneWhiteout;
    u8               deathWhiteout;
    GfxCoord*        chargeCoord;
    GfxCoord*        burstCoord;
    Actor403600Work* work;

    work = task->work;
    mode = work->mode;
    switch (mode) {
        case ACTOR_403600_MODE_PARKED:
            work->worldCoord.coord.t[0] = 0x196E;
            work->worldCoord.coord.t[1] = 0x1AE;
            work->worldCoord.coord.t[2] = 0x1630;
            return;
        case ACTOR_403600_MODE_FIGHT:
            _actor403600UpdateFightAction(task);
            return;
        case ACTOR_403600_MODE_STAGGER:
            _actor403600CancelDrain(task);
            work->ignorePushOut = 0;
            work->committed     = 1;
            work->verticalSpeed = 0;
            work->animId        = ACTOR_403600_ANIM_STAGGER;
            if (work->phaseFrame < 0xF) {
                work->forwardSpeed = -0xA;
            }
            if (work->phaseFrame >= 0x27) {
                _actor403600ResetState(task);
                work->mode = ACTOR_403600_MODE_FIGHT;
                return;
            }
        default:
            return;
        case ACTOR_403600_MODE_STUN:
            _actor403600CancelDrain(task);
            work->forwardSpeed  = 0;
            work->verticalSpeed = 0;
            if (work->animId == ACTOR_403600_ANIM_STUN_HOLD) {
                remainingStunFrames = work->stunFrames - 1;
                work->stunFrames    = remainingStunFrames;
                if ((remainingStunFrames << 0x10) == 0) {
                    work->animId = ACTOR_403600_ANIM_STUN_RELEASE;
                    return;
                }
            } else if (work->phaseFrame >= 0x11) {
                _actor403600ResetState(task);
                work->mode = ACTOR_403600_MODE_FIGHT;
                return;
            }
            break;
        case ACTOR_403600_MODE_FREEZE:
            _actor403600CancelDrain(task);
            work->forwardSpeed    = 0;
            work->verticalSpeed   = 0;
            remainingFreezeFrames = work->stunFrames - 1;
            work->stunFrames      = remainingFreezeFrames;
            if ((remainingFreezeFrames << 0x10) != 0) {
                work->animRate               = 0;
                work->worldCoord.coord.t[1] += rsin(gDisplayState.animFrame << 9) >> 8;
                return;
            }
            _actor403600ResetState(task);
            work->mode = ACTOR_403600_MODE_FIGHT;
            return;
        case ACTOR_403600_MODE_WEAKEN:
            // Shed the four loose models before resuming combat in the weak appearance.
            _actor403600CancelDrain(task);
            work->weakPhase     = ACTOR_403600_WEAK_PHASE_ACTIVE;
            work->committed     = 1;
            work->animId        = ACTOR_403600_ANIM_STAGGER;
            work->ignorePushOut = 0;
            work->verticalSpeed = 0;
            work->chainSweep    = 0;
            if (work->phaseFrame == 1) {
                memset(&shedHitOffset, 0, sizeof(shedHitOffset));
                shedHitOffset.vy = 0x64;
                effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[1], 3, &shedHitOffset);
                _actor403600SetWeakTextures(1);
                D_800626EC[5].data.model = &gShelterB2PodBottomModel0A2A0;
                firstShedEffect          = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, &task->extra.tmd->coords[1], 0, NULL);
                if (firstShedEffect != NULL) {
                    _actor403600BindShedPartTextures(firstShedEffect->task);
                }
                D_800626EC[5].data.model = &gShelterB2PodBottomModel0A68C;
                secondShedEffect         = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, &task->extra.tmd->coords[1], 0, NULL);
                if (secondShedEffect != NULL) {
                    _actor403600BindShedPartTextures(secondShedEffect->task);
                }
                D_800626EC[5].data.model = &gShelterB2PodBottomModel0AA08;
                thirdShedEffect          = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, &task->extra.tmd->coords[1], 0, NULL);
                if (thirdShedEffect != NULL) {
                    _actor403600BindShedPartTextures(thirdShedEffect->task);
                }
                D_800626EC[5].data.model = &gShelterB2PodBottomModel0AE48;
                fourthShedEffect         = effectSpawn(EFFECT_BURST_BODY_PART_BANK8, &task->extra.tmd->coords[1], 0, NULL);
                if (fourthShedEffect != NULL) {
                    _actor403600BindShedPartTextures(fourthShedEffect->task);
                }
                effectSpawn(EFFECT_030, &task->extra.tmd->coords[1], 0x800, NULL);
            }
            weakenFrame = work->phaseFrame;
            if ((weakenFrame == 4) || (weakenFrame == 6)) {
                memset(&repeatHitOffset, 0, sizeof(repeatHitOffset));
                repeatHitOffset.vy = 0x64;
                effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[1], 3, &repeatHitOffset);
            }
            if (work->phaseFrame < 0xF) {
                work->forwardSpeed = -0xA;
            }
            if (work->phaseFrame >= 0x27) {
                _actor403600ResetState(task);
                work->mode = ACTOR_403600_MODE_FIGHT;
            }
            return;
        case ACTOR_403600_MODE_RECOVER:
            // Charge into a white flash, restore the texture, then fade back to combat.
            if (work->weakPhase == ACTOR_403600_WEAK_PHASE_ACTIVE) {
                if (work->phaseFrame >= 0x32) {
                    chargeWhiteout = work->whiteout;
                    fadeDrawOverlay(chargeWhiteout, chargeWhiteout, chargeWhiteout, GPU_BLEND_ADD);
                    nextChargeWhiteout = (u16)work->whiteout + 0xF;
                    work->whiteout     = nextChargeWhiteout;
                    if (nextChargeWhiteout >= 0xFF) {
                        work->whiteout = 0xFF;
                    }
                }
                if (((u16)work->phaseFrame & 3) == 3) {
                    effectSpawn(EFFECT_HIT_PUFF, &task->extra.tmd->coords[1], ACTOR_403600_WEAK_PUFF_ARG, NULL);
                }
                chargeFrame = work->phaseFrame;
                if (chargeFrame < 0x2F) {
                    if (chargeFrame == 0x2E) {
                        chargeCoord = &work->worldCoord;
                        ACTOR_403600_PLAY_MODE_SOUND(SOUND_SHELTER_B2_POD_BTM_ENEMY_CHARGE, chargeCoord, chargePan, chargeDepth);
                    }
                    if (((u16)work->phaseFrame & 0xF) == 0xF) {
                        shelterB2PodBottomSpawnJointFlash(task);
                    }
                } else {
                    shelterB2PodBottomSpawnJointFlash(task);
                    if (work->phaseFrame == 0x32) {
                        burstCoord = &work->worldCoord;
                        ACTOR_403600_PLAY_MODE_SOUND(ACTOR_403600_SOUND_RECOVERY_BURST, burstCoord, burstPan, burstDepth);
                        effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_CHARGE_BURST, &task->extra.tmd->coords[1], 0x1E, NULL);
                    }
                }
                if (work->phaseFrame == 0x3C) {
                    _actor403600SetWeakTextures(0);
                }
                work->forwardSpeed  = 0;
                work->verticalSpeed = 0;
                work->animId        = ACTOR_403600_ANIM_RECHARGE;
                if (work->phaseFrame >= 0x46) {
                    sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_CHARGE, 0x14);
                    _actor403600ResetState(task);
                    work->animId    = ACTOR_403600_ANIM_RECHARGE_END;
                    work->weakPhase = ACTOR_403600_WEAK_PHASE_RECOVERING;
                    return;
                }
            } else {
                recoveryWhiteout = work->whiteout;
                fadeDrawOverlay(recoveryWhiteout, recoveryWhiteout, recoveryWhiteout, GPU_BLEND_ADD);
                nextRecoveryWhiteout = (u16)work->whiteout - 0x28;
                work->whiteout       = nextRecoveryWhiteout;
                if ((nextRecoveryWhiteout << 0x10) <= 0) {
                    work->whiteout = 0;
                }
                recoveryEndFrame = work->phaseFrame;
                if (recoveryEndFrame == 0xA) {
                    sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_CHARGE, 0x14);
                    recoveryEndFrame = work->phaseFrame;
                }
                if (recoveryEndFrame >= 0x1E) {
                    _actor403600ResetState(task);
                    work->weakPhase = ACTOR_403600_WEAK_PHASE_NONE;
                    work->mode      = ACTOR_403600_MODE_FIGHT;
                    return;
                }
            }
            break;
        case ACTOR_403600_MODE_SCENE_POSE:
            sceneWhiteout = work->whiteout;
            fadeDrawOverlay(sceneWhiteout, sceneWhiteout, sceneWhiteout, GPU_BLEND_ADD);
            nextSceneWhiteout = (u16)work->whiteout - 0x1E;
            work->whiteout    = nextSceneWhiteout;
            if ((nextSceneWhiteout << 0x10) <= 0) {
                work->whiteout = 0;
            }
            work->forwardSpeed  = 0;
            work->verticalSpeed = 0;
            worldCoordSetModelAmbientColor(task->extra.tmd, 0x1F40, 0x1F40, 0x1F40);
            return;
        case ACTOR_403600_MODE_SCENE_BRIGHTEN:
            work->forwardSpeed  = 0;
            work->verticalSpeed = 0;
            work->ambientBoost  = (u16)(work->ambientBoost + 0x64);
            return;
        case ACTOR_403600_MODE_SCENE_ASCEND:
            ascentStep = work->step;
            switch (ascentStep) {
                case ACTOR_403600_ASCENT_STEP_RISE:
                    nextAscentY                 = work->worldCoord.coord.t[1] + work->forwardSpeed;
                    work->worldCoord.coord.t[1] = nextAscentY;
                    if (nextAscentY < -0x1B61) {
                        work->actionDelay   = 0;
                        work->forwardSpeed  = 0;
                        work->phaseFrame    = 0;
                        work->actionDelay   = -0x19;
                        work->actionCounter = 3U;
                        work->step          = (s16)((u16)work->step + 1);
                    }
                    if (!((u16)work->phaseFrame & 1)) {
                        nextAscentSpeed    = (u16)work->forwardSpeed + 2;
                        work->forwardSpeed = nextAscentSpeed;
                        if (nextAscentSpeed >= -0x1E) {
                            work->forwardSpeed = -0x1E;
                        }
                    }
                    effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_RISING_SPRITE, &task->extra.tmd->coords[15], ACTOR_403600_ASCENT_SPRITE_SIZE, NULL);
                    effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_RISING_SPRITE, &task->extra.tmd->coords[19], ACTOR_403600_ASCENT_SPRITE_SIZE, NULL);
                    return;
                case ACTOR_403600_ASCENT_STEP_BOB:
                    if ((u16)work->phaseFrame & 1) {
                        bobSpeed                    = work->actionDelay;
                        bobYOrSpeedBits             = work->worldCoord.coord.t[1];
                        bobYOrSpeedBits            += bobSpeed;
                        work->worldCoord.coord.t[1] = bobYOrSpeedBits;
                        bobYOrSpeedBits             = (u16)work->actionDelay;
                        if (bobSpeed < 0) {
                            bobSpeed          = bobYOrSpeedBits + 1;
                            work->actionDelay = bobSpeed;
                            if ((bobSpeed << 0x10) == 0) {
                                work->actionDelay   = ascentStep;
                                remainingUpBobs     = work->actionCounter - 1;
                                work->actionCounter = remainingUpBobs;
                                if ((remainingUpBobs << 0x10) == 0) {
                                    work->actionCounter = 3U;
                                    work->actionDelay   = 0x19;
                                }
                            }
                        } else {
                            bobSpeed          = bobYOrSpeedBits - 1;
                            work->actionDelay = bobSpeed;
                            if ((bobSpeed << 0x10) == 0) {
                                work->actionDelay   = 0;
                                remainingDownBobs   = work->actionCounter - 1;
                                work->actionCounter = remainingDownBobs;
                                if ((remainingDownBobs << 0x10) == 0) {
                                    work->actionCounter = 3U;
                                    work->actionDelay   = -0x19;
                                }
                            }
                        }
                    }
                    if (work->actionTimer >= 0x14) {
                        // Model coordinates the smoke and sprites rise from.
                        u8 smokeCoords[9] = { 1, 12, 13, 14, 15, 16, 17, 18, 19 };

                        for (smokeIndex = 0; smokeIndex < ARRAY_SIZE(smokeCoords); smokeIndex++) {
                            effectSpawn(EFFECT_SMOKE_PUFF, &task->extra.tmd->coords[smokeCoords[smokeIndex]], ACTOR_403600_ASCENT_SMOKE_ARG, NULL);
                            effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_RISING_SPRITE, &task->extra.tmd->coords[smokeCoords[smokeIndex]], ACTOR_403600_ASCENT_SPRITE_SIZE, NULL);
                        }
                        work->actionTimer = 0;
                    }
                    work->actionTimer = (s16)((u16)work->actionTimer + 1);
                    if (work->phaseFrame >= 0xC8) {
                        work->phaseFrame = 0;
                        work->step       = (s16)((u16)work->step + 1);
                    }
                    break;
                case ACTOR_403600_ASCENT_STEP_POSE:
                    work->animId = ACTOR_403600_ANIM_SCENE_HOVER;
                    if (work->actionTimer >= 0x14) {
                        // Model coordinates the smoke and sprites rise from.
                        u8 smokeCoords[9] = { 1, 12, 13, 14, 15, 16, 17, 18, 19 };

                        for (smokeIndex = 0; smokeIndex < ARRAY_SIZE(smokeCoords); smokeIndex++) {
                            effectSpawn(EFFECT_SMOKE_PUFF, &task->extra.tmd->coords[smokeCoords[smokeIndex]], ACTOR_403600_ASCENT_SMOKE_ARG, NULL);
                            effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_RISING_SPRITE, &task->extra.tmd->coords[smokeCoords[smokeIndex]], ACTOR_403600_ASCENT_SPRITE_SIZE, NULL);
                        }
                        work->actionTimer = 0;
                    }
                    work->actionTimer = (s16)((u16)work->actionTimer + 1);
                    if ((u16)work->phaseFrame & 1) {
                        bobSpeed                    = work->actionDelay;
                        bobYOrSpeedBits             = work->worldCoord.coord.t[1];
                        bobYOrSpeedBits            += bobSpeed;
                        work->worldCoord.coord.t[1] = bobYOrSpeedBits;
                        bobYOrSpeedBits             = (u16)work->actionDelay;
                        if (bobSpeed < 0) {
                            bobSpeed          = bobYOrSpeedBits + 1;
                            work->actionDelay = bobSpeed;
                            if ((bobSpeed << 0x10) == 0) {
                                work->actionDelay   = 1;
                                remainingPoseUpBobs = work->actionCounter - 1;
                                work->actionCounter = remainingPoseUpBobs;
                                if ((remainingPoseUpBobs << 0x10) == 0) {
                                    work->actionCounter = 3U;
                                    work->actionDelay   = 0x19;
                                    return;
                                }
                            }
                        } else {
                            bobSpeed          = bobYOrSpeedBits - 1;
                            work->actionDelay = bobSpeed;
                            if ((bobSpeed << 0x10) == 0) {
                                work->actionDelay     = 0;
                                remainingPoseDownBobs = work->actionCounter - 1;
                                work->actionCounter   = remainingPoseDownBobs;
                                if ((remainingPoseDownBobs << 0x10) == 0) {
                                    work->actionCounter = 3U;
                                    work->actionDelay   = -0x19;
                                    return;
                                }
                            }
                        }
                    }
                    break;
            }
            break;
        case ACTOR_403600_MODE_DYING:
            deathWhiteout = work->whiteout;
            fadeDrawOverlay(deathWhiteout, deathWhiteout, deathWhiteout, GPU_BLEND_ADD);
            nextDeathWhiteout = (u16)work->whiteout + 2;
            work->whiteout    = nextDeathWhiteout;
            if (nextDeathWhiteout >= 0xFF) {
                work->whiteout = 0xFF;
            }
            deathStep = work->step;
            switch (deathStep) {
                case ACTOR_403600_DEATH_STEP_LOCK_INPUT:
                    padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET, PAD_INPUT_SUPPRESS_GAMEPLAY);
                    work->chainSweep          = 0;
                    gPlayerStatus.statusFlags = 0;
                    work->step                = (s16)((u16)work->step + 1);
                    break;
                case ACTOR_403600_DEATH_STEP_LOAD_SUCCESSOR:
                    if ((s16)work->whiteout == 0xFF) {
                        // Enqueue reads index/group/stage; key byte 1 is ignored.
                        // Global file 303600 carries the successor actor package.
                        u8 fileKeyBytes[4];
                        u8 loadArgs[4];

                        fileKeyBytes[2] = ACTOR_403600_SUCCESSOR_FILE_ID / 10000;
                        fileKeyBytes[3] = 0;
                        fileKeyBytes[0] = ACTOR_403600_SUCCESSOR_FILE_ID % 100;
                        loadArgs[0]     = (ACTOR_403600_SUCCESSOR_FILE_ID / 100) % 100;
                        loadArgs[3]     = 0;
                        loadArgs[2]     = 0;
                        loadArgs[1]     = 0;
                        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKeyBytes, loadArgs);
                        work->step = (s16)((u16)work->step + 1);
                    }
                    break;
                case ACTOR_403600_DEATH_STEP_SPAWN_SUCCESSOR:
                    if (cdCmdIsIdle() & 0xFFFF) {
                        if (D_actor_403600_801606B4 != 0) {
                            taskCallExit(D_actor_403600_801606B4);
                        }
                        taskSpawnFromTable(D_actor_303600_80162E98, 0, 0, 0);
                        padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR, PAD_INPUT_SUPPRESS_GAMEPLAY);
                        work->step = (s16)((u16)work->step + 1);
                    }
                    break;
            }
            break;
    }

#undef ACTOR_403600_PLAY_MODE_SOUND
}
/// Advances the boss's current combat action and its animation-driven phases.
///
/// Requires the singleton boss's live work, enemy and player, plus the package's
/// room effects and scripted-player animation request. Chooses attacks, sets
/// motion for the later movement step, opens swipe contacts and times projectiles,
/// summoning, dives, player knockback and the drain presentation. phaseFrame is
/// advanced by animation playback; speeds use world units per frame. Drain raises
/// MP during its wind-up, then removes 251 MP at the strike. Owns no work storage.
static void _actor403600UpdateFightAction(Task* task)
{
    // Advance the resident LCG once and take the low four bits of its 16-bit draw.
    // Captures the shared unsigned state and signed roll locals below. The state
    // is committed before the draw is consumed; multiplication wraps in u32.
#define ACTOR_403600_DRAW_PROJECTILE_ROLL()                                                     \
    {                                                                                           \
        projectileRandomState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT; \
        gRandomLcgState       = projectileRandomState;                                          \
        projectileRoll        = (projectileRandomState >> 16) & 0xF;                            \
    }

    enum {
        ACTOR_403600_SLAM_STEP_CLIMB_WINDUP        = 0,
        ACTOR_403600_SLAM_STEP_CLIMB               = 1,
        ACTOR_403600_SLAM_STEP_DIVE                = 2,
        ACTOR_403600_SLAM_STEP_RECOIL              = 3,
        ACTOR_403600_SLAM_STEP_RECOVER             = 4,
        ACTOR_403600_SWOOP_STEP_POSITION           = 0,
        ACTOR_403600_SWOOP_STEP_LOWER              = 1,
        ACTOR_403600_SWOOP_STEP_FACE               = 2,
        ACTOR_403600_SWOOP_STEP_WINDUP             = 3,
        ACTOR_403600_SWOOP_STEP_FLIGHT             = 4,
        ACTOR_403600_SWOOP_STEP_RECOVER            = 5,
        ACTOR_403600_RUSH_STEP_APPROACH            = 0,
        ACTOR_403600_RUSH_STEP_FACE                = 1,
        ACTOR_403600_RUSH_STEP_CHARGE              = 2,
        ACTOR_403600_RUSH_STEP_WINDUP              = 3,
        ACTOR_403600_RUSH_STEP_FLIGHT              = 4,
        ACTOR_403600_RUSH_STEP_REPOSITION          = 5,
        ACTOR_403600_RUSH_STEP_RECOVER             = 6,
        ACTOR_403600_ATTACK_MELEE                  = 1,
        ACTOR_403600_ATTACK_DRAIN                  = 3,
        ACTOR_403600_ATTACK_RUSH                   = 4,
        ACTOR_403600_PROJECTILE_TASK               = 1,
        ACTOR_403600_RUSH_EFFECT_TASK              = 3,
        ACTOR_403600_DOUBLE_TASK                   = 1,
        ACTOR_403600_DRAIN_DISTORTION_WINDUP       = 3072,
        ACTOR_403600_DRAIN_DISTORTION_STEP         = 14,
        ACTOR_403600_MELEE_CHASE_LIMIT             = 90,
        ACTOR_403600_VOLLEY_FIRST_SHOT_FRAME       = 39,
        ACTOR_403600_VOLLEY_SECOND_SHOT_FRAME      = 44,
        ACTOR_403600_VOLLEY_THIRD_SHOT_FRAME       = 49,
        ACTOR_403600_DRAIN_MP_TICK_START_FRAME     = 45,
        ACTOR_403600_DRAIN_SOUND_START_FRAME       = 48,
        ACTOR_403600_DRAIN_STRIKE_FRAME            = 35,
        ACTOR_403600_DRAIN_END_FRAME               = 69,
        ACTOR_403600_SLAM_RESPONSE_NONE            = 0,
        ACTOR_403600_SLAM_RESPONSE_SHORT_KNOCKBACK = 2,
        ACTOR_403600_SLAM_RESPONSE_HEAVY_KNOCKBACK = 3,
    };
    // Start a placed-enemy cue using signed-byte pan and half-depth rounding.
    // cue and soundCoord must have no side effects; soundCoord is read twice.
    // panOut and depthOut are s32/u32 local lvalues, assigned before dispatch.
    // Captures task and the shared soundKey local. Only this function uses it.
#define ACTOR_403600_PLAY_FIGHT_SOUND(cue, soundCoord, panOut, depthOut)                                           \
    {                                                                                                              \
        soundKey   = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | (cue); \
        (panOut)   = (s8)worldCoordGetOriginAudioPan((soundCoord));                                                \
        (depthOut) = worldCoordGetOriginAudioDepth((soundCoord));                                                  \
        sndEvtRequestScriptStart(soundKey, (panOut), (s32)((((depthOut) >> 0x1F) + (depthOut)) << 0x17) >> 0x18);  \
    }

    u32                   playerRange;
    s32                   playerBearing;
    s32                   rushPlayerHeight;
    s32                   summonPlayerHeight;
    s32                   meleeReferenceHeight;
    s16                   action;
    s16                   chaseVerticalSpeed;
    s32                   volleyFrame;
    s32                   flightResult;
    s16                   volleyAnimationCase;
    s16                   swoopVerticalSpeed;
    s16                   nextRushDelay;
    s16                   rechargeFrame;
    s16                   slamStep;
    s16                   drainAnimation;
    s16                   drainStrikeFrame;
    s32                   meleeAnimation;
    s16                   swoopStep;
    s16                   rushStep;
    s16                   summonAnimation;
    s32                   rushFacingMagnitude;
    s32                   chaseSpeedMagnitude;
    s32                   slamFacingMagnitude;
    s32                   swoopFacingMagnitude;
    s32                   swoopDeltaXSquared;
    s32                   swoopDeltaYSquared;
    s32                   rushDeltaXSquared;
    s32                   rushDeltaYSquared;
    s32                   soundKey;
    s32                   rushDeltaX;
    s32                   rushDeltaY;
    s32                   rushDeltaZ;
    s32                   projectileRoll;
    s32                   swoopDeltaX;
    s32                   swoopDeltaY;
    s32                   swoopDeltaZ;
    s32                   slamHitResponse;
    s32                   meleeBearingMagnitude;
    s32                   volleyClimbPan;
    s32                   volleyArrivalPan;
    s32                   volleyFirePan;
    s32                   swoopLaunchPan;
    s32                   swoopPlayerHitPan;
    s32                   rushWindupPan;
    s32                   rushFinalHitPan;
    s32                   rushPassHitPan;
    s32                   rushBoundaryPan;
    s32                   summonPan;
    s32                   drainPlayerHitPan;
    s32                   drainKnockbackPan;
    s32                   rechargePan;
    s32                   meleeFirstPan;
    s32                   meleeSecondPan;
    s32                   rushLaunchPan;
    s32                   slamClimbPan;
    s32                   slamDivePan;
    s32                   slamPlayerHitPan;
    s32                   slamImpactPan;
    u16                   drainFrame;
    u16                   remainingMp;
    u16                   nextDistortion;
    u16                   nextChaseFrames;
    u16                   nextSlamSpeed;
    u32                   rechargeDepth;
    u32                   volleyClimbDepth;
    u32                   volleyArrivalDepth;
    u32                   projectileRandomState;
    u32                   volleyFireDepth;
    u32                   swoopLaunchDepth;
    u32                   swoopPlayerRange;
    u32                   swoopPlayerHitDepth;
    u32                   rushWindupDepth;
    u32                   rushFinalHitDepth;
    u32                   rushPassHitDepth;
    u32                   rushBoundaryDepth;
    u32                   summonDepth;
    u32                   drainPlayerHitDepth;
    u32                   drainKnockbackDepth;
    u32                   meleeFirstDepth;
    u32                   meleeSecondDepth;
    u32                   slamClimbDepth;
    u32                   slamDiveDepth;
    u32                   slamPlayerHitDepth;
    u32                   slamImpactDepth;
    u32                   rushLaunchDepth;
    GfxCoord*             rechargeSoundCoord;
    GfxCoord*             volleyClimbSoundCoord;
    GfxCoord*             volleyFireSoundCoord;
    GfxCoord*             swoopLaunchSoundCoord;
    GfxCoord*             rushWindupSoundCoord;
    GfxCoord*             rushCoord;
    GfxCoord*             summonSoundCoord;
    GfxCoord*             drainKnockbackSoundCoord;
    GfxCoord*             meleeFirstSoundCoord;
    GfxCoord*             meleeSecondSoundCoord;
    GfxCoord*             slamClimbSoundCoord;
    GfxCoord*             slamDiveSoundCoord;
    GfxCoord*             slamImpactSoundCoord;
    GfxCoord*             rushLaunchSoundCoord;
    AnimationPlayRequest* swoopPlayerAnimation;
    AnimationPlayRequest* rushPlayerAnimation;
    Actor403600Work*      work;
    GfxCoord*             slamCoord;
    GfxCoord*             playerCoord;
    Enemy*                enemy;
    GameActor*            playerActor;
    GfxCoord*             volleySoundCoord;
    PlayerStatus*         playerStatus;

    playerStatus = &gPlayerStatus;
    work         = task->work;
    enemy        = task->spawnArg2.pointer;
    action       = work->action;
    playerCoord  = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    switch (action) {
        case ACTOR_403600_ACTION_CHOOSE:
            // Prefer a nearby melee before drawing one of the six longer attacks.
            work->animId        = (u32)ACTOR_403600_ANIM_IDLE;
            work->forwardSpeed  = 0U;
            work->verticalSpeed = 0;
            if (work->phaseFrame >= work->actionDelay) {
                work->actionDelay = ACTOR_403600_DEFAULT_ACTION_DELAY;
                work->phaseFrame  = 0;
                _actor403600MeasurePlayerRangeBearing(&work->worldCoord, &playerRange, &playerBearing);
                if ((playerRange < 0x835U) && (work->meleeGaveUp == 0)) {
                    work->attackBody.pos.vz = 0x3E8;
                    work->attackBody.key    = damagePackAttackKey(&D_actor_403600_80150E9C, ACTOR_403600_ATTACK_MELEE);
                    work->attackBody.radius = 0x5DC;
                    work->animId            = (u32)ACTOR_403600_ANIM_FLY;
                    work->action            = ACTOR_403600_ACTION_MELEE;
                    work->forwardSpeed      = 0U;
                    work->phaseFrame        = 0;
                    work->meleeChaseFrames  = 0U;
                    return;
                }
                _actor403600ChooseAttack(task);
                if (work->action == ACTOR_403600_ACTION_CHOOSE) {
                    work->actionDelay = 0;
                    return;
                }
                work->meleeGaveUp = 0;
                return;
            }
        default:
            return;
        case ACTOR_403600_ACTION_RECHARGE:
            if (work->phaseFrame == 0x14) {
                rechargeSoundCoord = &work->worldCoord;
                ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_SHELTER_B2_POD_BTM_ENEMY_CHARGE, rechargeSoundCoord, rechargePan, rechargeDepth);
            }
            if (work->phaseFrame >= 0x14) {
                shelterB2PodBottomSpawnJointFlash(task);
            }
            work->animId        = (u32)ACTOR_403600_ANIM_RECHARGE;
            work->forwardSpeed  = 0U;
            work->verticalSpeed = 0;
            if (work->phaseFrame >= 0x2D) {
                _actor403600ResetState(task);
                work->action  = ACTOR_403600_ACTION_RECHARGE_END;
                work->exposed = 0;
                return;
            }
            break;
        case ACTOR_403600_ACTION_RECHARGE_END:
            if (work->phaseFrame < 0xC) {
                shelterB2PodBottomSpawnJointFlash(task);
            }
            work->animId  = (u32)ACTOR_403600_ANIM_RECHARGE_END;
            rechargeFrame = work->phaseFrame;
            if (rechargeFrame == 0xA) {
                sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_CHARGE, 0x14);
            }
            if (work->phaseFrame >= 0x1E) {
                _actor403600ResetState(task);
                return;
            }
            break;
        case ACTOR_403600_ACTION_SLAM:
            // Climb to the high anchor, then dive; player height and range size the impact.
            slamStep = work->step;
            switch (slamStep) {
                case ACTOR_403600_SLAM_STEP_CLIMB_WINDUP:
                    work->committed     = 1;
                    work->animRate      = 2 * ANIMATION_RATE_ONE;
                    work->animId        = (u32)ACTOR_403600_ANIM_DIVE_WINDUP;
                    work->forwardSpeed  = 0x14U;
                    work->verticalSpeed = 0;
                    work->targetPos.vx  = (s32)D_actor_403600_801605D4.vx;
                    work->targetPos.vy  = (s32)D_actor_403600_801605D4.vy;
                    work->targetPos.vz  = (s32)D_actor_403600_801605D4.vz;
                    work->turnRate      = 0x40;
                    work->aimMode       = ACTOR_403600_AIM_TARGET;
                    _actor403600TurnToAim(task);
                    if (work->phaseFrame >= 0x13) {
                        slamClimbSoundCoord = &work->worldCoord;
                        ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 4), slamClimbSoundCoord, slamClimbPan, slamClimbDepth);
                        work->animRate   = ANIMATION_RATE_ONE;
                        work->turnRate   = 0x80;
                        work->phaseFrame = 0;
                        work->diving     = 1;
                        work->step       = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case ACTOR_403600_SLAM_STEP_CLIMB:
                    work->animId        = (u32)ACTOR_403600_ANIM_DIVE;
                    work->forwardSpeed  = 0x4B0U;
                    work->verticalSpeed = 0;
                    _actor403600TurnToAim(task);
                    if (work->worldCoord.coord.t[1] < D_actor_403600_801605D4.vy) {
                        work->worldCoord.coord.t[0] = (s32)D_actor_403600_801605D4.vx;
                        work->worldCoord.coord.t[1] = (s32)D_actor_403600_801605D4.vy;
                        work->worldCoord.coord.t[2] = (s32)D_actor_403600_801605D4.vz;
                        work->targetPos.vx          = (s32)D_actor_403600_801605DC.vx;
                        work->targetPos.vy          = (s32)D_actor_403600_801605DC.vy;
                        work->targetPos.vz          = (s32)D_actor_403600_801605DC.vz;
                        work->aimMode               = ACTOR_403600_AIM_TARGET_SNAP;
                        _actor403600TurnToAim(task);
                        work->ignorePushOut = 1;
                        work->phaseFrame    = 0;
                        work->actionParam   = 0;
                        work->forwardSpeed  = 0U;
                        work->actionTimer   = 0x28;
                        work->step          = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case ACTOR_403600_SLAM_STEP_DIVE:
                    if (work->actionTimer != 0) {
                        work->actionTimer = (s16)((u16)work->actionTimer - 1);
                        return;
                    }
                    if (!((u16)work->phaseFrame & 1)) {
                        effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &task->extra.tmd->coords[1], 0, &D_actor_403600_80160664);
                        effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &task->extra.tmd->coords[15], 0x800, NULL);
                        effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &task->extra.tmd->coords[19], 0x800, NULL);
                    }
                    work->animId        = (u32)ACTOR_403600_ANIM_DIVE;
                    work->forwardSpeed  = 0x320U;
                    work->verticalSpeed = 0;
                    _actor403600AdvanceRoll(task, 0x14);
                    if (((D_actor_403600_801605DC.vy - 0x1388) < work->worldCoord.coord.t[1]) && (work->actionParam == 0)) {
                        work->actionParam  = 1;
                        slamDiveSoundCoord = &work->worldCoord;
                        ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 5), slamDiveSoundCoord, slamDivePan, slamDiveDepth);
                    }
                    if ((D_actor_403600_801605DC.vy - 0x3E8) < work->worldCoord.coord.t[1]) {
                        padScriptSpawnVariableMotorRamp(0xA, 0xFF, 0x50);
                        slamHitResponse = ACTOR_403600_SLAM_RESPONSE_NONE;
                        if (gPlayerStatus.coordMtx->t[1] < -0xF3B) {
                            slamCoord                     = &work->worldCoord;
                            D_actor_403600_801606A4.power = (u16)D_actor_403600_80150EA4;
                            _actor403600MeasurePlayerRangeBearing(slamCoord, &playerRange, &playerBearing);
                            if ((u32)(playerRange - 0xFA0) < 0x7D1U) {
                                playerActor                      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
                                D_actor_403600_801606A4.reaction = 0;
                                D_actor_403600_801606A4.power    = (u16)((u16)D_actor_403600_801606A4.power >> 2);
                                playerActor->hitRegion           = 2;
                                playerActor->pendingDamage       = (u16)D_actor_403600_801606A4.power;
                                playerActor->damageReaction      = GAME_ACTOR_REACTION_ORDINARY;
                            } else {
                                if ((u32)(playerRange - 0x9C4) < 0x5DCU) {
                                    D_actor_403600_801606A4.reaction = 0;
                                    D_actor_403600_801606A4.power    = (u16)((u16)D_actor_403600_801606A4.power >> 1);
                                    slamFacingMagnitude              = _actor403600BearingFromPlayer(slamCoord);
                                    if (slamFacingMagnitude < 0) {
                                        slamFacingMagnitude = -slamFacingMagnitude;
                                    }
                                    if (slamFacingMagnitude >= 0x401) {
                                        D_actor_403600_80160568.animationId = ACTOR_403600_PLAYER_ANIM_FORWARD;
                                        _actor403600PlacePlayerForKnockback(task, 0);
                                        work->knockbackSpeed = 0x64;
                                    } else {
                                        D_actor_403600_80160568.animationId = ACTOR_403600_PLAYER_ANIM_BACKWARD;
                                        _actor403600PlacePlayerForKnockback(task, 1);
                                        work->knockbackSpeed = -0x64;
                                    }
                                    TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
                                    slamHitResponse      = ACTOR_403600_SLAM_RESPONSE_SHORT_KNOCKBACK;
                                    work->knockbackFrame = 0;
                                } else if (playerRange < 0x9C4U) {
                                    D_actor_403600_801606A4.reaction    = 0;
                                    D_actor_403600_80160568.animationId = ACTOR_403600_PLAYER_ANIM_HEAVY_KNOCKBACK;
                                    _actor403600PlacePlayerForKnockback(task, 1);
                                    work->knockbackSpeed = -0x190;
                                    TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
                                    slamHitResponse      = ACTOR_403600_SLAM_RESPONSE_HEAVY_KNOCKBACK;
                                    work->knockbackFrame = 0;
                                }
                            }
                            if (slamHitResponse != ACTOR_403600_SLAM_RESPONSE_NONE) {
                                ACTOR_403600_PLAY_FIGHT_SOUND(6, playerCoord, slamPlayerHitPan, slamPlayerHitDepth);
                                taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(&D_actor_403600_801606A4, 0), 0);
                            }
                        }
                        slamImpactSoundCoord = &work->worldCoord;
                        ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 6), slamImpactSoundCoord, slamImpactPan, slamImpactDepth);
                        effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_ARC_FLASH, &task->extra.tmd->coords[1], 0, &D_actor_403600_80160664);
                        work->shakeFrames     = 0x32;
                        work->shakeFadeFrames = 0x10;
                        work->phaseFrame      = 0;
                        work->forwardSpeed    = -0x64U;
                        work->roll            = 0;
                        work->diving          = 0;
                        work->step            = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case ACTOR_403600_SLAM_STEP_RECOIL:
                    work->animId = (u32)ACTOR_403600_ANIM_DIVE;
                    if ((u16)work->phaseFrame & 8) {
                        nextSlamSpeed      = work->forwardSpeed + 5;
                        work->forwardSpeed = nextSlamSpeed;
                        if ((nextSlamSpeed << 0x10) > 0) {
                            work->forwardSpeed = 0U;
                        }
                    }
                    work->verticalSpeed = 0;
                    if ((s16)work->forwardSpeed == 0) {
                        work->exposed     = 1;
                        work->damageTaken = 0;
                        work->phaseFrame  = 0;
                        work->step        = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case ACTOR_403600_SLAM_STEP_RECOVER:
                    work->animId        = (u32)ACTOR_403600_ANIM_DIVE_RECOVER;
                    work->verticalSpeed = 0;
                    if (work->phaseFrame >= 0xA) {
                        work->aimMode = ACTOR_403600_AIM_PLAYER_LEVEL;
                        _actor403600TurnToAim(task);
                    }
                    if (work->phaseFrame >= 0x26) {
                        _actor403600ResetState(task);
                        work->action = ACTOR_403600_ACTION_RECHARGE;
                        return;
                    }
                    break;
            }
            break;
        case ACTOR_403600_ACTION_VOLLEY:
            // Reach a firing point, then release three animation-timed projectiles.
            volleyAnimationCase = work->animId - ACTOR_403600_ANIM_CHARGE;
            switch (volleyAnimationCase) {
                case ACTOR_403600_ANIM_DIVE_WINDUP - ACTOR_403600_ANIM_CHARGE:
                    work->committed     = 1;
                    work->animRate      = 2 * ANIMATION_RATE_ONE;
                    work->animId        = (u32)ACTOR_403600_ANIM_DIVE_WINDUP;
                    work->forwardSpeed  = 0x14U;
                    work->verticalSpeed = 0;
                    work->targetPos.vx  = (s32)D_actor_403600_801605D4.vx;
                    work->targetPos.vy  = (s32)D_actor_403600_801605D4.vy;
                    work->targetPos.vz  = (s32)D_actor_403600_801605D4.vz;
                    work->turnRate      = 0x40;
                    work->aimMode       = ACTOR_403600_AIM_TARGET;
                    _actor403600TurnToAim(task);
                    if (work->phaseFrame >= 0x13) {
                        volleyClimbSoundCoord = &work->worldCoord;
                        ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 4), volleyClimbSoundCoord, volleyClimbPan, volleyClimbDepth);
                        work->turnRate      = 0x80;
                        work->step          = 0;
                        work->animRate      = ANIMATION_RATE_ONE;
                        work->phaseFrame    = 0;
                        work->ignorePushOut = 1;
                        work->animId        = (u32)ACTOR_403600_ANIM_DIVE;
                        return;
                    }
                    break;
                case ACTOR_403600_ANIM_DIVE - ACTOR_403600_ANIM_CHARGE:
                    work->animId        = (u32)ACTOR_403600_ANIM_DIVE;
                    work->forwardSpeed  = 0x4B0U;
                    work->verticalSpeed = 0;
                    if (work->step == 1) {
                        work->targetPos.vy = (s32)(gPlayerStatus.coordMtx->t[1] - 0x258);
                    }
                    work->aimMode = ACTOR_403600_AIM_TARGET;
                    if (_actor403600TurnToAim(task) < 0x7D1) {
                        if (work->step == 0) {
                            work->step = 1;
                            _actor403600ChooseFlightTarget(task, 0);
                            return;
                        }
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->phaseFrame    = 0;
                        work->animId        = (u32)ACTOR_403600_ANIM_DIVE_RECOVER;
                        volleySoundCoord    = &work->worldCoord;

                        ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 4), volleySoundCoord, volleyArrivalPan, volleyArrivalDepth);
                        return;
                    }
                    break;
                case ACTOR_403600_ANIM_DIVE_RECOVER - ACTOR_403600_ANIM_CHARGE:
                    work->animId        = (u32)ACTOR_403600_ANIM_DIVE_RECOVER;
                    work->turnRate      = 0xA0;
                    work->forwardSpeed  = 0U;
                    work->verticalSpeed = 0;
                    work->aimMode       = ACTOR_403600_AIM_PLAYER_LEVEL;
                    _actor403600TurnToAim(task);
                    if (work->phaseFrame >= 0x26) {
                        work->turnRate      = 0x80;
                        work->committed     = 0;
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->phaseFrame    = 0;
                        work->ignorePushOut = 0;
                        work->animId        = (u32)ACTOR_403600_ANIM_CHARGE;
                        return;
                    }
                    break;
                case ACTOR_403600_ANIM_CHARGE - ACTOR_403600_ANIM_CHARGE:
                    volleyFrame = work->phaseFrame;
                    if (volleyFrame == 1) {
                        ACTOR_403600_DRAW_PROJECTILE_ROLL();
                        if (projectileRoll < 2) {
                            work->projectileKind = 0;
                        } else if (projectileRoll < 5) {
                            work->projectileKind = 2;
                        } else if (projectileRoll < 0xA) {
                            work->projectileKind = volleyFrame;
                        } else {
                            work->projectileKind = 3;
                        }
                    }
                    if (work->phaseFrame == ACTOR_403600_VOLLEY_FIRST_SHOT_FRAME) {
                        volleyFireSoundCoord = &work->worldCoord;
                        ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_SHELTER_B2_POD_BTM_ENEMY_VOLLEY, volleyFireSoundCoord, volleyFirePan, volleyFireDepth);
                        taskSpawnFromTable(D_actor_403600_801421A0, ACTOR_403600_PROJECTILE_TASK, (s32)((s16)((u16)work->projectileKind | 0x10)), task);
                    }
                    if ((work->phaseFrame == ACTOR_403600_VOLLEY_SECOND_SHOT_FRAME) || (work->phaseFrame == ACTOR_403600_VOLLEY_THIRD_SHOT_FRAME)) {
                        taskSpawnFromTable(D_actor_403600_801421A0, ACTOR_403600_PROJECTILE_TASK, (s32)(work->projectileKind), task);
                    }
                    if (work->phaseFrame >= work->actionParam) {
                        sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_VOLLEY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        work->phaseFrame = 0;
                        work->animId     = ACTOR_403600_ANIM_VOLLEY_END;
                        return;
                    }
                    break;
                case ACTOR_403600_ANIM_VOLLEY_END - ACTOR_403600_ANIM_CHARGE:
                    if (work->phaseFrame >= 0x33) {
                        work->aimMode    = ACTOR_403600_AIM_PLAYER;
                        work->action     = ACTOR_403600_ACTION_CHOOSE;
                        work->phaseFrame = 0;
                        return;
                    }
                    break;
            }
            break;
        case ACTOR_403600_ACTION_SWOOP:
            // Position at the chosen corner, dive across the room and expose the body on landing.
            swoopStep = work->step;
            switch (swoopStep) {
                case ACTOR_403600_SWOOP_STEP_POSITION:
                    work->animId = (u32)ACTOR_403600_ANIM_FLY;
                    if ((_actor403600ApproachTarget(task) & 0xFF) == ACTOR_403600_TARGET_AXES_SETTLED) {
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->phaseFrame    = 0;
                        if (work->playerZone == ACTOR_403600_PLAYER_ZONE_CENTRE) {
                            work->targetPos.vx = D_actor_403600_8016063C[(s16)work->swoopCorners].vx;
                            work->targetPos.vy = D_actor_403600_8016063C[(s16)work->swoopCorners].vy + 0xFA0;
                            work->targetPos.vz = D_actor_403600_8016063C[(s16)work->swoopCorners].vz;
                        } else if ((u16)work->swoopCorners & 2) {
                            work->targetPos.vx = D_actor_403600_8016064C[1].vx;
                            work->targetPos.vy = D_actor_403600_8016064C[1].vy;
                            work->targetPos.vz = D_actor_403600_8016064C[1].vz;
                        } else {
                            work->targetPos.vx = D_actor_403600_8016064C[0].vx;
                            work->targetPos.vy = D_actor_403600_8016064C[0].vy;
                            work->targetPos.vz = D_actor_403600_8016064C[0].vz;
                        }
                        work->step = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case ACTOR_403600_SWOOP_STEP_LOWER:
                    work->animId        = (u32)ACTOR_403600_ANIM_IDLE;
                    swoopVerticalSpeed  = (-0x1770 - work->worldCoord.coord.t[1]) / 25;
                    work->verticalSpeed = swoopVerticalSpeed;
                    if (swoopVerticalSpeed < 0xA) {
                        work->verticalSpeed = 0;
                        work->phaseFrame    = 0;
                        work->step          = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case ACTOR_403600_SWOOP_STEP_FACE:
                    work->aimMode = ACTOR_403600_AIM_TARGET;
                    _actor403600TurnYawToAim(task, 0x20);
                    if (work->phaseFrame >= 0x28) {
                        work->animRate      = 2 * ANIMATION_RATE_ONE;
                        work->verticalSpeed = 0;
                        work->phaseFrame    = 0;
                        work->turnRate      = 0xA0;
                        work->step          = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case ACTOR_403600_SWOOP_STEP_WINDUP:
                    work->animId        = (u32)ACTOR_403600_ANIM_DIVE_WINDUP;
                    work->committed     = 1;
                    work->forwardSpeed  = 0U;
                    work->verticalSpeed = 0;
                    _actor403600TurnToAim(task);
                    if (work->phaseFrame >= 0x13) {
                        swoopLaunchSoundCoord = &work->worldCoord;
                        ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 11), swoopLaunchSoundCoord, swoopLaunchPan, swoopLaunchDepth);
                        work->forwardSpeed  = 0x320U;
                        work->animRate      = ANIMATION_RATE_ONE;
                        work->ignorePushOut = 0;
                        work->actionParam   = 0;
                        work->phaseFrame    = 0;
                        work->turnRate      = 0x80;
                        work->diving        = 1;
                        work->step          = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case ACTOR_403600_SWOOP_STEP_FLIGHT:
                    work->animId        = (u32)ACTOR_403600_ANIM_DIVE;
                    work->forwardSpeed  = 0x320U;
                    work->verticalSpeed = 0;
                    work->turnRate      = 0xA0;
                    if (!((u16)work->phaseFrame & 1)) {
                        effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &task->extra.tmd->coords[1], 0, &D_actor_403600_80160664);
                        effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &task->extra.tmd->coords[15], 0x800, NULL);
                        effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &task->extra.tmd->coords[19], 0x800, NULL);
                    }
                    work->aimMode      = ACTOR_403600_AIM_TARGET;
                    flightResult       = _actor403600TurnToAim(task);
                    swoopDeltaX        = gPlayerStatus.coordMtx->t[0] - work->worldCoord.coord.t[0];
                    swoopDeltaXSquared = swoopDeltaX * swoopDeltaX;
                    swoopDeltaY        = gPlayerStatus.coordMtx->t[1] - work->worldCoord.coord.t[1];
                    swoopDeltaYSquared = swoopDeltaY * swoopDeltaY;
                    swoopDeltaZ        = gPlayerStatus.coordMtx->t[2] - work->worldCoord.coord.t[2];
                    swoopPlayerRange   = SquareRoot0(swoopDeltaXSquared + swoopDeltaYSquared + (swoopDeltaZ * swoopDeltaZ));
                    playerRange        = swoopPlayerRange;
                    if (swoopPlayerRange < 0x76DU) {
                        swoopPlayerAnimation = &D_actor_403600_80160568;
                        if (swoopPlayerAnimation->animationId == ACTOR_403600_PLAYER_ANIM_NONE) {
                            padScriptSpawnVariableMotorRamp(0x14, 0xFF, 0x50);
                            work->knockbackFrame = 0;
                            swoopFacingMagnitude = _actor403600BearingFromPlayer(&work->worldCoord);
                            if (swoopFacingMagnitude < 0) {
                                swoopFacingMagnitude = -swoopFacingMagnitude;
                            }
                            if (swoopFacingMagnitude >= 0x401) {
                                work->knockbackSpeed              = 0x28;
                                swoopPlayerAnimation->animationId = ACTOR_403600_PLAYER_ANIM_FORWARD;
                                TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, swoopPlayerAnimation, 0);
                            } else {
                                work->knockbackSpeed              = -0x28;
                                swoopPlayerAnimation->animationId = ACTOR_403600_PLAYER_ANIM_BACKWARD;
                                TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, swoopPlayerAnimation, 0);
                            }
                            ACTOR_403600_PLAY_FIGHT_SOUND(6, playerCoord, swoopPlayerHitPan, swoopPlayerHitDepth);
                            taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(&D_actor_403600_80150E9C, 0), 0);
                        }
                    }
                    if (flightResult < 0x3E9) {
                        work->animRate      = ANIMATION_RATE_ONE;
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->aimMode       = ACTOR_403600_AIM_PLAYER;
                        work->actionParam   = 0;
                        work->phaseFrame    = 0;
                        work->diving        = 0;
                        work->exposed       = 1;
                        work->damageTaken   = 0;
                        work->step          = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case ACTOR_403600_SWOOP_STEP_RECOVER:
                    work->animId        = (u32)ACTOR_403600_ANIM_DIVE_RECOVER;
                    work->verticalSpeed = 0;
                    work->aimMode       = ACTOR_403600_AIM_PLAYER_LEVEL;
                    work->turnRate      = 0x40;
                    _actor403600TurnToAim(task);
                    if (work->phaseFrame >= 0x26) {
                        _actor403600ResetState(task);
                        work->action = ACTOR_403600_ACTION_RECHARGE;
                        return;
                    }
                    break;
            }
            break;
        case ACTOR_403600_ACTION_RUSH:
            // Chain spinning passes; boundary events move the boss to the next start.
            rushStep = work->step;
            switch (rushStep) {
                case ACTOR_403600_RUSH_STEP_APPROACH:
                    work->aimMode       = ACTOR_403600_AIM_TARGET;
                    work->forwardSpeed  = 0xC8U;
                    rushPlayerHeight    = gPlayerStatus.coordMtx->t[1] + 0x1F4;
                    work->verticalSpeed = (s16)((rushPlayerHeight - work->worldCoord.coord.t[1]) / 25);
                    if (_actor403600TurnYawToAim(task, 0xB0) < 0x3E9) {
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->phaseFrame    = 0;
                        work->step          = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case ACTOR_403600_RUSH_STEP_FACE:
                    work->aimMode = ACTOR_403600_AIM_PLAYER;
                    _actor403600TurnYawToAim(task, 0x20);
                    work->forwardSpeed  = 0U;
                    work->verticalSpeed = 0;
                    _actor403600MeasurePlayerRangeBearing(&work->worldCoord, &playerRange, &playerBearing);
                    if (work->phaseFrame >= 0x32) {
                        work->phaseFrame = 0;
                        work->step       = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case ACTOR_403600_RUSH_STEP_CHARGE:
                    shelterB2PodBottomSpawnJointEnergySparks(task);
                    work->animId = (u32)ACTOR_403600_ANIM_RUSH_CHARGE;
                    if (work->phaseFrame == 0x19) {
                        rushWindupSoundCoord = &work->worldCoord;
                        ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 21), rushWindupSoundCoord, rushWindupPan, rushWindupDepth);
                    }
                    if (work->phaseFrame < 0x14) {
                        work->targetPos.vx = (s32)gPlayerStatus.coordMtx->t[0];
                        work->targetPos.vy = (s32)(gPlayerStatus.coordMtx->t[1] - 0x3E8);
                        work->targetPos.vz = (s32)gPlayerStatus.coordMtx->t[2];
                    }
                    if (work->phaseFrame >= 0x32) {
                        work->animId     = (u32)ACTOR_403600_ANIM_DIVE_WINDUP;
                        work->aimMode    = ACTOR_403600_AIM_TARGET;
                        work->animRate   = 2 * ANIMATION_RATE_ONE;
                        work->phaseFrame = 0;
                        work->turnRate   = 0xA0;
                        work->step       = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case ACTOR_403600_RUSH_STEP_WINDUP:
                    work->committed     = 1;
                    work->animId        = ACTOR_403600_ANIM_DIVE_WINDUP;
                    work->forwardSpeed  = 0U;
                    work->verticalSpeed = 0;
                    _actor403600TurnToAim(task);
                    if (work->phaseFrame >= 0x13) {
                        work->animRate       = ANIMATION_RATE_ONE;
                        work->forwardSpeed   = 0x320U;
                        work->turnRate       = 0xA0;
                        work->chainSweep     = 0x7000;
                        work->animId         = (u32)ACTOR_403600_ANIM_DIVE;
                        work->aimMode        = ACTOR_403600_AIM_TARGET;
                        work->actionParam    = 0;
                        work->phaseFrame     = 0;
                        work->roll           = 0;
                        work->gridHitLatched = 0;
                        work->actionTimer    = 0x96;
                        work->diving         = 1;
                        work->step           = (s16)((u16)work->step + 1);
                        rushLaunchSoundCoord = &work->worldCoord;
                        ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 11), rushLaunchSoundCoord, rushLaunchPan, rushLaunchDepth);
                        return;
                    }
                    break;
                case ACTOR_403600_RUSH_STEP_FLIGHT:
                    if (!((u16)work->phaseFrame & 1)) {
                        if (work->actionParam != ACTOR_403600_RUSH_FINAL_PASS) {
                            effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &task->extra.tmd->coords[1], 0, &D_actor_403600_80160664);
                            effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &task->extra.tmd->coords[15], 0x800, NULL);
                            effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &task->extra.tmd->coords[19], 0x800, NULL);
                        }
                    }
                    enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
                    work->animId                  = (u32)ACTOR_403600_ANIM_DIVE;
                    _actor403600AdvanceRoll(task, 0xA);
                    rushCoord = &work->worldCoord;
                    _actor403600MeasurePlayerRangeBearing(rushCoord, &playerRange, &playerBearing);
                    if (playerRange < 0x5DDU) {
                        rushPlayerAnimation = &D_actor_403600_80160568;
                        if (rushPlayerAnimation->animationId == ACTOR_403600_PLAYER_ANIM_NONE) {
                            if (work->actionParam == ACTOR_403600_RUSH_FINAL_PASS) {
                                padScriptSpawnVariableMotorRamp(0xA, 0xFF, 0x50);
                                D_actor_403600_801606A4.reaction = 0xA;
                                rushPlayerAnimation->animationId = ACTOR_403600_PLAYER_ANIM_HEAVY_KNOCKBACK;
                                D_actor_403600_801606A4.power    = (u16)D_actor_403600_80150EAC;
                                _actor403600PlacePlayerForKnockback(task, 1);
                                work->knockbackSpeed = -0x190;
                                TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, rushPlayerAnimation, 0);
                                work->knockbackFrame = 0;
                                ACTOR_403600_PLAY_FIGHT_SOUND(6, playerCoord, rushFinalHitPan, rushFinalHitDepth);
                                taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(&D_actor_403600_801606A4, 0), 0);
                            } else {
                                padScriptSpawnVariableMotorRamp(0x14, 0xB0, 0x50);
                                work->knockbackFrame = 0;
                                rushFacingMagnitude  = _actor403600BearingFromPlayer(rushCoord);
                                if (rushFacingMagnitude < 0) {
                                    rushFacingMagnitude = -rushFacingMagnitude;
                                }
                                if (rushFacingMagnitude >= 0x401) {
                                    work->knockbackSpeed             = 0x28;
                                    rushPlayerAnimation->animationId = ACTOR_403600_PLAYER_ANIM_FORWARD;
                                    TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, rushPlayerAnimation, 0);
                                } else {
                                    work->knockbackSpeed             = -0x28;
                                    rushPlayerAnimation->animationId = ACTOR_403600_PLAYER_ANIM_BACKWARD;
                                    TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, rushPlayerAnimation, 0);
                                }
                                ACTOR_403600_PLAY_FIGHT_SOUND(6, playerCoord, rushPassHitPan, rushPassHitDepth);
                                taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(&D_actor_403600_80150E9C, ACTOR_403600_ATTACK_RUSH), 0);
                                work->actionParam = (s16)work->rushPasses;
                            }
                            work->actionTimer = 0x96;
                        }
                    }
                    flightResult = _actor403600CheckRushPass(task) & 0xFF;
                    if (flightResult == ACTOR_403600_RUSH_EVENT_CONTACT_EFFECT) {
                        taskSpawnFromTable(D_actor_403600_801421A0, ACTOR_403600_RUSH_EFFECT_TASK, 0, task);
                    }
                    if ((flightResult == ACTOR_403600_RUSH_EVENT_HIGH_END) && (work->actionParam == ACTOR_403600_RUSH_FINAL_PASS)) {
                        work->step        = ACTOR_403600_RUSH_STEP_RECOVER;
                        work->diving      = 0;
                        work->roll        = 0;
                        work->exposed     = 1;
                        work->damageTaken = 0;
                    }
                    if (flightResult == ACTOR_403600_RUSH_EVENT_BOUNDARY) {
                        work->diving        = 0;
                        work->repositioning = 1;
                        ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 14), playerCoord, rushBoundaryPan, rushBoundaryDepth);
                        taskSpawnFromTable(D_actor_403600_801421A0, ACTOR_403600_RUSH_EFFECT_TASK, 1, task);
                        work->actionDelay             = 0;
                        work->gridBody.flags          = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
                        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                        worldTargetDisableNodeLockOn(&enemy->node);
                        work->phaseFrame    = 0;
                        work->step          = ACTOR_403600_RUSH_STEP_REPOSITION;
                        work->actionCounter = (u16)work->actionTimer;
                        return;
                    }
                    break;
                case ACTOR_403600_RUSH_STEP_REPOSITION:
                    _actor403600AdvanceRoll(task, 0xA);
                    nextRushDelay     = (u16)work->actionDelay + 1;
                    work->actionDelay = nextRushDelay;
                    if (nextRushDelay == 8) {
                        work->forwardSpeed = 0U;
                    }
                    if (work->actionDelay == (s16)work->actionCounter) {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B2_POD_BTM_ENEMY_RUMBLE_LOOP, 0, 0);
                        _actor403600PlaceRushPass(task);
                        work->aimMode = ACTOR_403600_AIM_TARGET_SNAP;
                        _actor403600TurnToAim(task);
                        taskSpawnFromTable(D_actor_403600_801421A0, ACTOR_403600_RUSH_EFFECT_TASK, 2, task);
                    }
                    rushDeltaX        = gPlayerStatus.coordMtx->t[0] - work->worldCoord.coord.t[0];
                    rushDeltaXSquared = rushDeltaX * rushDeltaX;
                    rushDeltaY        = gPlayerStatus.coordMtx->t[1] - work->worldCoord.coord.t[1];
                    rushDeltaYSquared = rushDeltaY * rushDeltaY;
                    rushDeltaZ        = gPlayerStatus.coordMtx->t[2] - work->worldCoord.coord.t[2];
                    playerRange       = SquareRoot0(rushDeltaXSquared + rushDeltaYSquared + (rushDeltaZ * rushDeltaZ));
                    if (work->phaseFrame >= 8) {
                        work->phaseFrame = 0;
                        if (playerRange < 0x3E9U) {
                            padScriptSpawnVariableMotorRamp(5, 0xB0, 0xB0);
                        } else if (playerRange < 0x7D1U) {
                            padScriptSpawnVariableMotorRamp(5, 0x80, 0x80);
                        } else if (playerRange < 0xBB9U) {
                            padScriptSpawnVariableMotorRamp(5, 0x50, 0x50);
                        }
                    }
                    if (work->actionDelay >= ((s16)work->actionCounter + 0x1E)) {
                        sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_RUMBLE_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 0x10), 0, 0);
                        work->forwardSpeed   = 0x320U;
                        work->turnRate       = 0xA0;
                        work->step           = ACTOR_403600_RUSH_STEP_FLIGHT;
                        work->phaseFrame     = 0;
                        work->gridHitLatched = 0;
                        work->diving         = 1;
                        work->repositioning  = 0;
                        return;
                    }
                    break;
                case ACTOR_403600_RUSH_STEP_RECOVER:
                    work->animId       = (u32)ACTOR_403600_ANIM_DIVE_RECOVER;
                    work->chainSweep   = 0;
                    work->forwardSpeed = 0U;
                    work->aimMode      = ACTOR_403600_AIM_PLAYER_LEVEL;
                    _actor403600TurnToAim(task);
                    if (work->phaseFrame >= 0x26) {
                        _actor403600ResetState(task);
                        work->action = ACTOR_403600_ACTION_RECHARGE;
                        return;
                    }
                    break;
            }
            break;
        case ACTOR_403600_ACTION_SUMMON:
            // Charge near the player and create a double before withdrawing.
            summonAnimation = (s16)work->animId;
            switch (summonAnimation) {
                case ACTOR_403600_ANIM_FLY:
                    work->aimMode = ACTOR_403600_AIM_PLAYER;
                    _actor403600TurnYawToAim(task, 0xA0);
                    work->forwardSpeed  = 0x12CU;
                    summonPlayerHeight  = gPlayerStatus.coordMtx->t[1] + 0x1F4;
                    work->verticalSpeed = (s16)((summonPlayerHeight - work->worldCoord.coord.t[1]) / 25);
                    _actor403600MeasurePlayerRangeBearing(&work->worldCoord, &playerRange, &playerBearing);
                    if ((playerRange < 0x1389U) && (work->verticalSpeed < 0x12D)) {
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->phaseFrame    = 0;
                        work->animId        = (u32)ACTOR_403600_ANIM_CHARGE;
                        effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_CHARGE_BURST, &task->extra.tmd->coords[1], (s32)(work->actionParam), NULL);
                        return;
                    }
                    break;
                case ACTOR_403600_ANIM_CHARGE:
                    work->forwardSpeed  = 0U;
                    work->verticalSpeed = 0;
                    if (work->phaseFrame == 1) {
                        summonSoundCoord = &work->worldCoord;
                        ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 20), summonSoundCoord, summonPan, summonDepth);
                    }
                    if (work->phaseFrame >= work->actionParam) {
                        work->phaseFrame = 0;
                        work->animId     = ACTOR_403600_ANIM_SUMMON;
                        return;
                    }
                    break;
                case ACTOR_403600_ANIM_SUMMON:
                    if ((work->phaseFrame == 1) && (work->childEnemy == 0)) {
                        work->childEnemy = enemySpawnFromTable(D_actor_403600_80160514, ACTOR_403600_DOUBLE_TASK, worldTargetGetActorLockMask(&enemy->node), 0);
                    }
                    if (work->phaseFrame >= 0x1E) {
                        work->forwardSpeed  = -0xAU;
                        work->verticalSpeed = -0x14;
                    }
                    if (work->phaseFrame >= 0x42) {
                        work->phaseFrame = 0;
                        work->animId     = (u32)ACTOR_403600_ANIM_RETREAT;
                        case ACTOR_403600_ANIM_RETREAT:
                    }
                    work->forwardSpeed  = -0x14U;
                    work->verticalSpeed = -0x1E;
                    if (work->phaseFrame >= 0x1E) {
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->action        = ACTOR_403600_ACTION_CHOOSE;
                        work->phaseFrame    = 0;
                        work->actionDelay   = 5;
                        return;
                    }
                    break;
            }
            break;
        case ACTOR_403600_ACTION_DRAIN:
            // Build MP and distortion during the wind-up, then strike and remove MP.
            drainAnimation = (s16)work->animId;
            switch (drainAnimation) {
                case ACTOR_403600_ANIM_FLY:
                    work->targetPos.vx = 0x1F40;
                    work->targetPos.vy = -0x1B58;
                    work->targetPos.vz = 0x1900;
                    if ((_actor403600ApproachTarget(task) & 0xFF) == ACTOR_403600_TARGET_AXES_SETTLED) {
                        work->drainPuffArg      = ACTOR_403600_DRAIN_PUFF_FIRST_SIZE;
                        work->drainPuffFrames   = 0;
                        work->drainPuffInterval = ACTOR_403600_DRAIN_PUFF_FIRST_INTERVAL;
                        work->damageTaken       = 0;
                        work->forwardSpeed      = 0U;
                        work->verticalSpeed     = 0;
                        work->phaseFrame        = 0;
                        work->animId            = (u32)ACTOR_403600_ANIM_CHARGE;
                        work->drainStartMp      = (u16)playerStatus->mp;
                        return;
                    }
                    break;
                case ACTOR_403600_ANIM_CHARGE:
                    if (work->phaseFrame >= ACTOR_403600_DRAIN_MP_TICK_START_FRAME) {
                        _actor403600RaisePlayerMpForDrain(task);
                        drainFrame             = (u16)work->phaseFrame;
                        work->screenDistortion = (u16)(work->screenDistortion + (ACTOR_403600_DRAIN_DISTORTION_WINDUP / (s16)work->actionParam));
                        if (work->phaseFrame == ACTOR_403600_DRAIN_SOUND_START_FRAME) {
                            padScriptSpawnVariableMotorRamp((s16)(((u16)work->actionParam - drainFrame) + 0x23), 0x40, 0xFF);
                            sndEvtRequestScriptStart(SOUND_SHELTER_B2_POD_BTM_ENEMY_DRAIN_WINDUP, 0, 0);
                        }
                    }
                    _actor403600UpdateDrainPuffs(task);
                    if (work->phaseFrame >= work->actionParam) {
                        work->phaseFrame = 0;
                        work->animId     = (u32)ACTOR_403600_ANIM_DRAIN_STRIKE;
                    }
                    if (work->damageTaken >= ACTOR_403600_DRAIN_BREAK_DAMAGE) {
                        _actor403600CancelDrain(task);
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->action        = ACTOR_403600_ACTION_CHOOSE;
                        work->phaseFrame    = 0;
                    }
                    return;
                case ACTOR_403600_ANIM_DRAIN_STRIKE:
                    _actor403600UpdateDrainPuffs(task);
                    drainStrikeFrame = work->phaseFrame;
                    if (drainStrikeFrame == ACTOR_403600_DRAIN_STRIKE_FRAME) {
                        padScriptSpawnVariableMotorRamp(0xA, 0xFF, 0xFF);
                        work->screenDistortion = (u32)ONE;
                        ACTOR_403600_PLAY_FIGHT_SOUND(6, playerCoord, drainPlayerHitPan, drainPlayerHitDepth);
                        taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(&D_actor_403600_80150E9C, ACTOR_403600_ATTACK_DRAIN), 0);
                        D_actor_403600_80160568.animationId = ACTOR_403600_PLAYER_ANIM_FORWARD;
                        TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
                        remainingMp      = playerStatus->mp - ACTOR_403600_DRAIN_MP_LOSS;
                        playerStatus->mp = remainingMp;
                        if ((remainingMp << 0x10) <= 0) {
                            playerStatus->mp = 0U;
                        }
                        work->drainPuffInterval  = ACTOR_403600_DRAIN_PUFF_ALL_PARTS;
                        work->knockbackFrame     = 0;
                        work->knockbackSpeed     = 0x28;
                        drainKnockbackSoundCoord = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
                        ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 3), drainKnockbackSoundCoord, drainKnockbackPan, drainKnockbackDepth);
                        sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_DRAIN_WINDUP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    } else if (drainStrikeFrame < ACTOR_403600_DRAIN_STRIKE_FRAME) {
                        _actor403600RaisePlayerMpForDrain(task);
                    }
                    nextDistortion         = work->screenDistortion + ACTOR_403600_DRAIN_DISTORTION_STEP;
                    work->screenDistortion = nextDistortion;
                    if ((s16)nextDistortion >= ONE) {
                        work->screenDistortion = (u32)ONE;
                    }
                    if (work->phaseFrame >= ACTOR_403600_DRAIN_END_FRAME) {
                        if (playerStatus->mp <= 0) {
                            playerStatus->mp = 0U;
                        }
                        work->screenDistortion = 0U;
                        work->action           = ACTOR_403600_ACTION_CHOOSE;
                        work->phaseFrame       = 0;
                    }
                    if ((work->damageTaken >= ACTOR_403600_DRAIN_BREAK_DAMAGE) && (work->phaseFrame < 0x32)) {
                        _actor403600CancelDrain(task);
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->action        = ACTOR_403600_ACTION_CHOOSE;
                        work->phaseFrame    = 0;
                    }
                    break;
            }
            break;
        case ACTOR_403600_ACTION_MELEE:
            // Align range, bearing and height before opening each swipe contact.
            meleeAnimation = (s16)work->animId;
            switch (meleeAnimation) {
                case ACTOR_403600_ANIM_FLY:
                    work->aimMode = ACTOR_403600_AIM_PLAYER;
                    _actor403600TurnYawToAim(task, 0x20);
                    _actor403600MeasurePlayerRangeBearing(&work->worldCoord, &playerRange, &playerBearing);
                    if (playerRange < 0x7D1U) {
                        work->forwardSpeed = 0U;
                    } else {
                        work->forwardSpeed = 0x50U;
                    }
                    meleeReferenceHeight = work->worldCoord.coord.t[1] + 0x3E8;
                    chaseVerticalSpeed   = (gPlayerStatus.coordMtx->t[1] - meleeReferenceHeight) / 25;
                    work->verticalSpeed  = chaseVerticalSpeed;
                    if (playerRange < 0x7D1U) {
                        meleeBearingMagnitude = playerBearing;
                        if (meleeBearingMagnitude < 0) {
                            meleeBearingMagnitude = -meleeBearingMagnitude;
                        }
                        if (meleeBearingMagnitude < 0x200) {
                            chaseSpeedMagnitude = chaseVerticalSpeed;
                            if (chaseSpeedMagnitude < 0) {
                                chaseSpeedMagnitude = -chaseSpeedMagnitude;
                            }
                            if (chaseSpeedMagnitude < 0x28) {
                                work->forwardSpeed    = 0U;
                                work->verticalSpeed   = 0;
                                work->phaseFrame      = 0;
                                work->animId          = (u32)ACTOR_403600_ANIM_MELEE_FIRST;
                                work->animBlendFrames = 0;
                            }
                        }
                    }
                    nextChaseFrames        = work->meleeChaseFrames + 1;
                    work->meleeChaseFrames = nextChaseFrames;
                    if (((s16)nextChaseFrames >= ACTOR_403600_MELEE_CHASE_LIMIT) || (playerRange >= 0xFA0U)) {
                        work->animBlendFrames = ACTOR_403600_DEFAULT_BLEND_FRAMES;
                        work->forwardSpeed    = 0U;
                        work->phaseFrame      = 0;
                        work->action          = ACTOR_403600_ACTION_CHOOSE;
                        work->meleeGaveUp     = 1;
                        return;
                    }
                    break;
                case ACTOR_403600_ANIM_MELEE_FIRST:
                    work->animId = (u16)meleeAnimation;
                    if (work->phaseFrame == 0xE) {
                        meleeFirstSoundCoord = &work->worldCoord;
                        ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 13), meleeFirstSoundCoord, meleeFirstPan, meleeFirstDepth);
                    }
                    if (work->phaseFrame == 0x11) {
                        work->attackBody.flags = (u16)(work->attackBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    }
                    if (work->phaseFrame == 0x15) {
                        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
                    }
                    if (work->phaseFrame >= 0x1E) {
                        work->forwardSpeed = 0U;
                        work->phaseFrame   = 0;
                        work->animId       = (u32)ACTOR_403600_ANIM_MELEE_SECOND;
                        _actor403600MeasurePlayerRangeBearing(&work->worldCoord, &playerRange, &playerBearing);
                        if (playerRange >= 0x7D0U) {
                            work->animBlendFrames = ACTOR_403600_DEFAULT_BLEND_FRAMES;
                            work->forwardSpeed    = 0U;
                            work->phaseFrame      = 0;
                            work->action          = ACTOR_403600_ACTION_CHOOSE;
                        }
                    }
                    break;
                case ACTOR_403600_ANIM_MELEE_SECOND:
                    work->animId = (u16)meleeAnimation;
                    if (work->phaseFrame == 9) {
                        meleeSecondSoundCoord = &work->worldCoord;
                        ACTOR_403600_PLAY_FIGHT_SOUND(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 13), meleeSecondSoundCoord, meleeSecondPan, meleeSecondDepth);
                    }
                    if (work->phaseFrame == 0xA) {
                        work->attackBody.flags = (u16)(work->attackBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    }
                    if (work->phaseFrame == 0xE) {
                        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
                    }
                    if (work->phaseFrame >= 0x23) {
                        work->animBlendFrames = ACTOR_403600_DEFAULT_BLEND_FRAMES;
                        work->forwardSpeed    = 0U;
                        work->phaseFrame      = 0;
                        work->action          = ACTOR_403600_ACTION_CHOOSE;
                    }
            }
            break;
    }
#undef ACTOR_403600_DRAW_PROJECTILE_ROLL
#undef ACTOR_403600_PLAY_FIGHT_SOUND
}

/// Initializes the horizontal radius arm and rotation for a rush placement.
///
/// scratch belongs to the caller's live rush-pass block. The arm's length is
/// the room's Z edge (15000 world units) minus the room-centre anchor's Z;
/// its initial direction is +Z and its rotation starts as identity.
static inline void _actor403600InitRushArm(_Actor403600RushPassScratch* scratch)
{
    scratch->offset.vx = 0;
    scratch->offset.vy = 0;
    scratch->offset.vz = ACTOR_403600_RUSH_CIRCLE_Z_EDGE - D_actor_403600_801605D4.vz;
    gfxSetRotIdentity(&scratch->rotation);
}

/// Places the boss on the rush circle, 1000 world units above the player.
///
/// Rotates scratch's radius arm with its prepared Q12 rotation, overwriting the
/// arm with signed-halfword GTE results, then adds the room-centre anchor.
/// Borrows the live boss work and caller-owned scratch block; changes position,
/// not targetPos. The caller handles facing and coordinate-cache invalidation.
static inline void _actor403600PlaceOnRushCircle(Actor403600Work* work, _Actor403600RushPassScratch* scratch)
{
    gte_SetRotMatrix(&scratch->rotation);
    gte_ldv0(&scratch->offset);
    gte_rtv0();
    gte_stsv(&scratch->offset);
    work->worldCoord.coord.t[0] = scratch->offset.vx + D_actor_403600_801605D4.vx;
    work->worldCoord.coord.t[1] = gPlayerStatus.coordMtx->t[1] - ACTOR_403600_RUSH_PLAYER_HEIGHT_OFFSET;
    work->worldCoord.coord.t[2] = scratch->offset.vz + D_actor_403600_801605D4.vz;
}

/// Places the boss for its next rush pass or the final central dive.
///
/// actionParam is the completed-pass count, compared with rushPasses. Even
/// passes derive a bearing from the player, aim across the inner rectangle
/// towards the player or otherwise towards the centre, and save the opposite
/// bearing for the following odd pass. Odd passes retain that target. The last
/// placement uses centre X/Z, start Y -2400 and target Y -8000, then marks the
/// final pass with 255. Non-final placements reserve and release one scratch block.
/// Bearings use 4096 units per turn; the historical negative fold is retained.
static void _actor403600PlaceRushPass(Task* task)
{
    Actor403600Work*             work;
    _Actor403600RushPassScratch* scratch;

    work = task->work;
    // The final dive uses fixed heights rather than the player-height circle.
    if (work->actionParam == work->rushPasses) {
        work->worldCoord.coord.t[0] = D_actor_403600_801605D4.vx;
        work->worldCoord.coord.t[1] = ACTOR_403600_RUSH_FINAL_START_Y;
        work->worldCoord.coord.t[2] = D_actor_403600_801605D4.vz;
        work->targetPos.vx          = D_actor_403600_801605D4.vx;
        work->targetPos.vy          = ACTOR_403600_RUSH_FINAL_TARGET_Y;
        work->targetPos.vz          = D_actor_403600_801605D4.vz;
        work->actionParam           = ACTOR_403600_RUSH_FINAL_PASS;
        return;
    }
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403600RushPassScratch);
    if (!(work->actionParam & 1)) {
        scratch->offset.vx = gPlayerStatus.coordMtx->t[0] - (u16)D_actor_403600_801605D4.vx;
        scratch->offset.vz = gPlayerStatus.coordMtx->t[2] - (u16)D_actor_403600_801605D4.vz;
        scratch->bearing   = ratan2(scratch->offset.vx, scratch->offset.vz);
        if (ABS(scratch->bearing) > ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
            scratch->bearing = (scratch->bearing > 0) ? scratch->bearing - ACTOR_TRANSFORM_ANGLE_TURN : ACTOR_TRANSFORM_ANGLE_TURN - scratch->bearing;
        }
        if ((u32)(gPlayerStatus.coordMtx->t[0] - ACTOR_403600_RUSH_INNER_X) < ACTOR_403600_RUSH_INNER_X_SPAN &&
            (u32)(gPlayerStatus.coordMtx->t[2] - ACTOR_403600_RUSH_INNER_Z) < ACTOR_403600_RUSH_INNER_Z_SPAN) {
            scratch->bearing = -scratch->bearing;
        }
        _actor403600InitRushArm(scratch);
        RotMatrixY(scratch->bearing, &scratch->rotation);
        _actor403600PlaceOnRushCircle(work, scratch);
        if ((u32)(gPlayerStatus.coordMtx->t[0] - ACTOR_403600_RUSH_INNER_X) < ACTOR_403600_RUSH_INNER_X_SPAN &&
            (u32)(gPlayerStatus.coordMtx->t[2] - ACTOR_403600_RUSH_INNER_Z) < ACTOR_403600_RUSH_INNER_Z_SPAN) {
            work->targetPos.vx = gPlayerStatus.coordMtx->t[0];
            work->targetPos.vy = gPlayerStatus.coordMtx->t[1] - ACTOR_403600_RUSH_PLAYER_HEIGHT_OFFSET;
            work->targetPos.vz = gPlayerStatus.coordMtx->t[2];
        } else {
            work->targetPos.vx = D_actor_403600_801605D4.vx;
            work->targetPos.vy = gPlayerStatus.coordMtx->t[1] - ACTOR_403600_RUSH_PLAYER_HEIGHT_OFFSET;
            work->targetPos.vz = D_actor_403600_801605D4.vz;
        }
        work->rushAngle = scratch->bearing + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        // Keep the original fold: a negative input below -2048 becomes 4096-input.
        if (ABS(work->rushAngle) > ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
            work->rushAngle = (work->rushAngle > 0) ? work->rushAngle - ACTOR_TRANSFORM_ANGLE_TURN : ACTOR_TRANSFORM_ANGLE_TURN - work->rushAngle;
        }
        work->actionTimer = ACTOR_403600_RUSH_EVEN_WAIT_FRAMES;
        work->actionParam++;
    } else {
        _actor403600InitRushArm(scratch);
        RotMatrixY(work->rushAngle, &scratch->rotation);
        _actor403600PlaceOnRushCircle(work, scratch);
        work->actionTimer = ACTOR_403600_RUSH_ODD_WAIT_FRAMES;
        work->actionParam++;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403600RushPassScratch);
}

/// Measures the player's ground distance from a row of the boss's flight-point table.
///
/// Requires pointIndex in 0..8, a live player coordinate matrix and writable
/// scratch separate from that matrix. Overwrites deltaX and deltaZ and returns
/// whole world units; their squared sum must fit signed 32 bits. Borrows all
/// storage and ignores Y.
static inline s32 _actor403600MeasureFlightPoint(_Actor403600NearestPointScratch* scratch, s32 pointIndex)
{
    s32 deltaX;
    s32 deltaZ;

    scratch->deltaX = gPlayerStatus.coordMtx->t[0] - D_actor_403600_801605F4[pointIndex].vx;
    deltaZ          = gPlayerStatus.coordMtx->t[2] - D_actor_403600_801605F4[pointIndex].vz;
    scratch->deltaZ = deltaZ;
    deltaX          = scratch->deltaX;
    return SquareRoot0((deltaX * deltaX) + (deltaZ * deltaZ));
}

/// Chooses the boss's flight target from the player's horizontal position.
///
/// Zero useRushReferences chooses the nearest of rows 0..6. Nonzero measures
/// rows 7 and 8, then deliberately retains the binary's selection of row 1
/// when distance8 >= distance7, otherwise row 0; it does not choose row 7 or 8.
/// All destinations use player Y minus 600 world units. Borrows the task's
/// live work and releases its temporary seven-distance scratch block.
static void _actor403600ChooseFlightTarget(Task* task, s32 useRushReferences)
{
    enum { ACTOR_403600_FLIGHT_DISTANCE_INITIAL_LIMIT = 0xFFFFFF,
           ACTOR_403600_FLIGHT_PLAYER_HEIGHT_OFFSET   = 600 };
    s32                              candidateDistance;
    s32                              referenceDistanceB;
    s32                              candidateIndex;
    s32                              nearestDistance;
    s32                              destinationIndex;
    _Actor403600NearestPointScratch* scratch;
    Actor403600Work*                 work;
    SVECTOR*                         points;
    SVECTOR*                         destination;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403600NearestPointScratch);
    work    = task->work;
    if (useRushReferences == 0) {
        scratch->distances[0] = _actor403600MeasureFlightPoint(scratch, 0);
        scratch->distances[1] = _actor403600MeasureFlightPoint(scratch, 1);
        scratch->distances[2] = _actor403600MeasureFlightPoint(scratch, 2);
        scratch->distances[3] = _actor403600MeasureFlightPoint(scratch, 3);
        scratch->distances[4] = _actor403600MeasureFlightPoint(scratch, 4);
        scratch->distances[5] = _actor403600MeasureFlightPoint(scratch, 5);
        scratch->distances[6] = _actor403600MeasureFlightPoint(scratch, 6);
        nearestDistance       = ACTOR_403600_FLIGHT_DISTANCE_INITIAL_LIMIT;
        destinationIndex      = 0;
        candidateIndex        = 0;
        do {
            candidateDistance = scratch->distances[candidateIndex & 0xFF];
            if (candidateDistance < nearestDistance) {
                destinationIndex = candidateIndex;
                nearestDistance  = candidateDistance;
            }
            candidateIndex += 1;
        } while ((u32)(candidateIndex & 0xFF) < (u32)ARRAY_SIZE(scratch->distances));
    } else {
        scratch->distances[0] = _actor403600MeasureFlightPoint(scratch, 7);
        referenceDistanceB    = _actor403600MeasureFlightPoint(scratch, 8);
        scratch->distances[1] = referenceDistanceB;
        destinationIndex      = referenceDistanceB >= scratch->distances[0];
    }
    points             = D_actor_403600_801605F4;
    destination        = (destinationIndex & 0xFF) + points;
    work->targetPos.vx = destination->vx;
    work->targetPos.vy = gPlayerStatus.coordMtx->t[1] - ACTOR_403600_FLIGHT_PLAYER_HEIGHT_OFFSET;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403600NearestPointScratch);
    work->targetPos.vz = destination->vz;
}

/// Draws 0..65535 from the resident gameplay random sequence.
///
/// Advances the shared unsigned 32-bit LCG once with wraparound and returns
/// its upper halfword. This consumes the same sequence as other overlays.
static inline u32 _actor403600Rand(void)
{
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    return gRandomLcgState >> 16;
}

/// Starts the boss's weakened transition after a qualifying hit.
///
/// Clears the phase counters and the enemy's pending stagger bit, retaining
/// all other reaction flags. The caller checks that weakening may begin.
static inline void _actor403600StartWeakenMode(Actor403600Work* work, Enemy* enemy)
{
    work->weakFrames      = 0;
    work->phaseFrame      = 0;
    work->mode            = ACTOR_403600_MODE_WEAKEN;
    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
}

/// Applies resolved grid displacement when the singleton boss accepts pushout.
///
/// Reads the integer halves of the Q16 correction; fractional halves are not
/// integrated here. Borrows the work and scratch blocks and composes no cache.
static inline void _actor403600ApplyGridCorrection(Task* task, Actor403600Work* work, const ActorContactDeltaWideScratch* scratch)
{
    if ((task == D_actor_403600_801606A8) && (work->ignorePushOut == 0)) {
        work->worldCoord.coord.t[0] += scratch->delta.fixed.vx.halves.integer;
        work->worldCoord.coord.t[1] += scratch->delta.fixed.vy.halves.integer;
        work->worldCoord.coord.t[2] += scratch->delta.fixed.vz.halves.integer;
    }
}

/// Resolves pushback and player-attack hits for the boss or its summoned double.
///
/// Requires live work, enemy, player and initialized contact tables. Only the
/// singleton boss accepts grid displacement, unless ignorePushOut is set.
/// Applies critical/exposed damage, status reactions, recoil and hit cooldowns;
/// consumes hit contacts and ends a paired swipe once it contacts a victim.
/// Borrows one ActorContactDeltaWideScratch block. The double's instant-defeat
/// return retains the binary's early exit before contact clearing and release.
static void _actor403600ProcessContacts(Task* task)
{
    enum {
        ACTOR_403600_HIT_NORMAL                       = 0,
        ACTOR_403600_HIT_CRITICAL                     = 1,
        ACTOR_403600_HIT_EXPOSED                      = 2,
        ACTOR_403600_HIT_DISTANCE_HEIGHT_OFFSET       = 2000,
        ACTOR_403600_HIT_REACTION_EXTRA_STAGGER       = 4,
        ACTOR_403600_HIT_REACTION_EXTRA_LOW_HP_WEAKEN = 9,
        ACTOR_403600_DOUBLE_INSTANT_DEFEAT_READOUT    = 999,
        ACTOR_403600_ATTACHMENT_ATTACK_SELECTOR       = 0x8000,
        ACTOR_403600_DIVE_BREAK_ATTACK_ROW_3          = WORLD_COLLISION_CONTACT_ATTACK | ACTOR_403600_ATTACHMENT_ATTACK_SELECTOR | 3,
        ACTOR_403600_DIVE_BREAK_ATTACK_ROW_6          = WORLD_COLLISION_CONTACT_ATTACK | ACTOR_403600_ATTACHMENT_ATTACK_SELECTOR | 6,
        ACTOR_403600_DIVE_BREAK_ATTACK_ROW_15         = WORLD_COLLISION_CONTACT_ATTACK | ACTOR_403600_ATTACHMENT_ATTACK_SELECTOR | 15,
    };
    u32                           playerRange;
    s32                           playerBearing;
    Actor403600Work*              work;
    Enemy*                        enemy;
    ActorContactDeltaWideScratch* scratch;
    WorldCollisionContact*        attackContacts;
    s32                           contactIndex;
    s32                           recoilRow;
    s32                           playerDeltaX;
    s32                           playerDeltaY;
    s32                           playerDeltaZ;
    s16                           hitKind;
    s32                           damage;
    s32                           attackKey;
    s16                           hitCooldown;
    s32                           hpMax;

    work    = task->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorContactDeltaWideScratch);
    enemy   = task->spawnArg2.pointer;
    // Grid corrections are whole units here; the same storage later holds a distance offset.
    switch (worldCollisionResolvePushback(work->hitContacts, &scratch->delta, ARRAY_SIZE(work->hitContacts), 0)) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            _actor403600ApplyGridCorrection(task, work, scratch);
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            _actor403600ApplyGridCorrection(task, work, scratch);
            break;
    }
    if (work->hitCooldown != 0) {
        work->hitCooldown--;
        if (work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    // Apply one hit at a time; a cooldown from one hit suppresses later contacts.
    for (contactIndex = 0; contactIndex < (s32)ARRAY_SIZE(work->hitContacts); contactIndex++) {
        if ((u16)(work->hitContacts[contactIndex].key.value >> 16) == (WORLD_COLLISION_CONTACT_PLAYER_BODY >> 16)) {
            continue;
        }
        if ((u16)(work->hitContacts[contactIndex].key.value >> 16) != (WORLD_COLLISION_CONTACT_ATTACK >> 16)) {
            continue;
        }
        if (work->hitCooldown != 0) {
            continue;
        }
        playerDeltaX             = gPlayerStatus.coordMtx->t[0] - work->worldCoord.coord.t[0];
        scratch->delta.vector.vx = playerDeltaX;
        playerDeltaY             = gPlayerStatus.coordMtx->t[1] - ACTOR_403600_HIT_DISTANCE_HEIGHT_OFFSET;
        playerDeltaY            -= work->worldCoord.coord.t[1];
        scratch->delta.vector.vy = playerDeltaY;
        playerDeltaZ             = gPlayerStatus.coordMtx->t[2] - work->worldCoord.coord.t[2];
        hitKind                  = ACTOR_403600_HIT_NORMAL;
        scratch->delta.vector.vz = playerDeltaZ;
        damage                   = damageComputePlayerAttack(work->hitContacts[contactIndex].key.value,
                                                             SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz),
                                                             0, 0);
        if (damageRollCriticalHit(task->spawnArg2.pointer, work->hitContacts[contactIndex].key.value, 0) != 0) {
            hitKind = ACTOR_403600_HIT_CRITICAL;
            damage *= 4;
        }
        if (work->exposed != 0) {
            hitKind = ACTOR_403600_HIT_EXPOSED;
            damage *= 2;
            if (work->damageTaken > ACTOR_403600_DRAIN_BREAK_DAMAGE) {
                work->exposed = 0;
            }
        }
        if (hitKind == ACTOR_403600_HIT_CRITICAL) {
            effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[1], 0, 0);
        } else if (hitKind == ACTOR_403600_HIT_EXPOSED) {
            effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[1], 3, 0);
        }
        switch ((u16)damageGetPlayerAttackReaction(work->hitContacts[contactIndex].key.value)) {
            case DAMAGE_PLAYER_REACTION_NONE:
                break;
            case DAMAGE_PLAYER_REACTION_BUILDUP:
                if (work->committed == 0) {
                    work->exposed = 0;
                    damageStartEnemyBuildup(task->spawnArg2.pointer, work->hitContacts[contactIndex].key.value, 0);
                    if (((u16)work->animId - ACTOR_403600_ANIM_DIVE_WINDUP < 2U) && ((u16)work->phaseFrame - 6 < 0x18U)) {
                        work->stunFrames = D_actor_403600_80150EC8.buildupSteps;
                    }
                    if ((work->animId == ACTOR_403600_ANIM_IDLE) && (work->phaseFrame < 30)) {
                        work->stunFrames = D_actor_403600_80150EC8.buildupSteps;
                    }
                }
                break;
            case DAMAGE_PLAYER_REACTION_POISON:
                if ((work->committed == 0) && (enemy->hp > 500)) {
                    damageTryStartEnemyDamageOverTime(task->spawnArg2.pointer, work->hitContacts[contactIndex].key.value, 0);
                }
                // This tests row-index bit 3; the intended attack grouping is unproven.
                if (work->hitContacts[contactIndex].key.value & 8) {
                    if (_actor403600Rand() & 1) {
                        work->damageTaken = ACTOR_403600_DRAIN_BREAK_DAMAGE;
                    }
                    if ((u16)work->animId - ACTOR_403600_ANIM_DIVE_WINDUP < 2U) {
                        work->exposed = 0;
                        if (work->phaseFrame < 30) {
                            work->mode       = ACTOR_403600_MODE_FREEZE;
                            work->stunFrames = D_actor_403600_80150EC8.buildupSteps * 10;
                        }
                    }
                }
                break;
            case DAMAGE_PLAYER_REACTION_STAGGER:
            case ACTOR_403600_HIT_REACTION_EXTRA_STAGGER:
                if (work->exposed != 0) {
                    damageStartEnemyStagger(enemy);
                    work->exposed = 0;
                    if (((u16)(_actor403600Rand() % 10) == 0) && (work->weakPhase == 0) && (work->repositioning == 0)) {
                        _actor403600StartWeakenMode(work, enemy);
                    }
                }
                // Stagger also checks the low-HP weakened transition.
            case DAMAGE_PLAYER_REACTION_EXPLOSION:
            case DAMAGE_PLAYER_REACTION_INCENDIARY:
            case ACTOR_403600_HIT_REACTION_EXTRA_LOW_HP_WEAKEN:
                hpMax = work->hpMax;
                if (enemy->hp < hpMax / 10) {
                    work->exposed = 0;
                    if ((work->weakPhase == 0) && (work->repositioning == 0)) {
                        _actor403600StartWeakenMode(work, enemy);
                    }
                }
                break;
        }
        if (work->diving != 0) {
            attackKey = work->hitContacts[contactIndex].key.value;
            if ((attackKey == ACTOR_403600_DIVE_BREAK_ATTACK_ROW_3) || (attackKey == ACTOR_403600_DIVE_BREAK_ATTACK_ROW_6) || (attackKey == ACTOR_403600_DIVE_BREAK_ATTACK_ROW_15)) {
                work->exposed = 0;
                if ((work->weakPhase == 0) && (work->repositioning == 0)) {
                    _actor403600StartWeakenMode(work, enemy);
                }
            }
        }
        if (task != D_actor_403600_801606A8) {
            damageAccumulateLifeDrainHp(enemy, work->hitContacts[contactIndex].key.value, damage, 0);
            // This tests row-index bit 3; the intended attack grouping is unproven.
            if (work->hitContacts[contactIndex].key.value & 8) {
                if ((_actor403600Rand() & 3) == 0) {
                    effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[1], 3, 0);
                    worldTargetAddReadoutAmount(&enemy->node, ACTOR_403600_DOUBLE_INSTANT_DEFEAT_READOUT, 0);
                    work->defeated = 1;
                    // Preserve the instant-defeat exit before scratch release.
                    return;
                }
            }
            _actor403600ApplyDoubleDamage(task, damage);
        } else {
            damageAccumulateLifeDrainHp(enemy, work->hitContacts[contactIndex].key.value, damage, 0);
            _actor403600ApplyDamage(task, damage);
            if (work->mode == ACTOR_403600_MODE_FIGHT) {
                for (recoilRow = 0; recoilRow < ARRAY_SIZE(D_actor_403600_8016066C); recoilRow++) {
                    if (damage >= D_actor_403600_8016066C[recoilRow].minDamage) {
                        work->recoilSpeed = D_actor_403600_8016066C[recoilRow].recoilSpeed;
                        work->recoilHold  = D_actor_403600_8016066C[recoilRow].recoilHold;
                    }
                }
            }
        }
        work->damageTaken += damage;
        if ((enemy->hp > 0) && (task == D_actor_403600_801606A8)) {
            effectSpawnHit(damageGetPlayerAttackEffectId(work->hitContacts[contactIndex].key.value), &work->worldCoord, &work->hitEffectOffset, &work->hitEffectArg);
        }
        hitCooldown = damageGetPlayerAttackHitCooldown(work->hitContacts[contactIndex].key.value);
        if (hitCooldown > 0) {
            work->hitCooldown = hitCooldown;
        }
        _actor403600MeasurePlayerRangeBearing(&work->worldCoord, &playerRange, &playerBearing);
        if (__builtin_abs(playerBearing) <= ACTOR_TRANSFORM_ANGLE_TURN / 4) {
            work->flinchRot.vx = ((_actor403600Rand() & 3) << 5) + 0x80;
        } else {
            work->flinchRot.vx = -(((_actor403600Rand() & 3) << 5) + 0x80);
            work->recoilSpeed *= -1;
        }
    }
    // Consume the contacts after the ordinary hit path finishes.
    worldCollisionClearContacts(work->hitContacts);
    attackContacts = work->attackContacts;
    if (worldCollisionFindContactIndex(attackContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        worldCollisionClearContacts(attackContacts);
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactDeltaWideScratch);
}

/// Checks scenery contacts and the boss's projected position during a rush pass.
///
/// Returns an ACTOR_403600_RUSH_EVENT value. A first grid contact of surface
/// class 3 returns CONTACT_EFFECT and latches further contacts in this pass.
/// Otherwise clears the grid contacts and tests a point 3000 world units ahead
/// on X/Z: leaving the room bounds or reaching Y >= 0 returns BOUNDARY;
/// Y <= -6000 returns HIGH_END. NONE continues the pass. Requires live work.
static s32 _actor403600CheckRushPass(Task* task)
{
    enum {
        ACTOR_403600_RUSH_CONTACT_EFFECT_SURFACE = 3,
        ACTOR_403600_RUSH_MIN_X                  = 257,
        ACTOR_403600_RUSH_MAX_X                  = 15616,
        ACTOR_403600_RUSH_MIN_Z                  = -127,
        ACTOR_403600_RUSH_MAX_Z                  = 14336,
        ACTOR_403600_RUSH_HIGH_Y_LIMIT           = -5999,
        ACTOR_403600_RUSH_LOOKAHEAD_Q9           = 375,
    };
    s32              contactIndex;
    s32              lookaheadX;
    s32              lookaheadZ;
    s32              height;
    Actor403600Work* work;

    work = task->work;
    for (contactIndex = 0; contactIndex < (s32)ARRAY_SIZE(work->gridContacts); contactIndex++) {
        if (((u32)(work->gridContacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) >> 16) == (WORLD_COLLISION_CONTACT_GRID >> 16)) {
            if (work->gridHitLatched == 0) {
                work->gridHitLatched++;
                if (worldCollisionSurfaceClassFromKey(work->gridContacts[contactIndex].key.value) == ACTOR_403600_RUSH_CONTACT_EFFECT_SURFACE) {
                    return ACTOR_403600_RUSH_EVENT_CONTACT_EFFECT;
                }
            }
        }
    }

    // A contact-effect event leaves the table for the next call; other paths consume it.
    worldCollisionClearContacts(work->gridContacts);
    lookaheadX = work->worldCoord.coord.t[0] + ((work->worldCoord.coord.m[0][2] * ACTOR_403600_RUSH_LOOKAHEAD_Q9) >> 9);
    lookaheadZ = work->worldCoord.coord.t[2] + ((work->worldCoord.coord.m[2][2] * ACTOR_403600_RUSH_LOOKAHEAD_Q9) >> 9);
    if (lookaheadX < ACTOR_403600_RUSH_MIN_X) {
        return ACTOR_403600_RUSH_EVENT_BOUNDARY;
    }
    if ((lookaheadX >= ACTOR_403600_RUSH_MAX_X) || (lookaheadZ >= ACTOR_403600_RUSH_MAX_Z)) {
        return ACTOR_403600_RUSH_EVENT_BOUNDARY;
    }
    if (lookaheadZ < ACTOR_403600_RUSH_MIN_Z) {
        return ACTOR_403600_RUSH_EVENT_BOUNDARY;
    }
    height = work->worldCoord.coord.t[1];
    if (height >= 0) {
        return ACTOR_403600_RUSH_EVENT_BOUNDARY;
    }
    if (height < ACTOR_403600_RUSH_HIGH_Y_LIMIT) {
        return ACTOR_403600_RUSH_EVENT_HIGH_END;
    }
    return ACTOR_403600_RUSH_EVENT_NONE;
}

/// Restores the boss's default combat motion and action state before defeat.
///
/// Reloads the task's live Actor403600Work after scripted-player dispatch.
/// Clears attack motion, roll, commitment and presentation latches; restores
/// normal animation rate, blend, turn rate and player aim. The caller selects
/// the death mode and animation and sets defeated after this reset.
static inline void _actor403600ResetForDeath(Task* task)
{
    Actor403600Work* work = task->work;
    work->animBlendFrames = ACTOR_403600_DEFAULT_BLEND_FRAMES;
    work->actionDelay     = ACTOR_403600_DEFAULT_ACTION_DELAY;
    work->defeated        = 0;
    work->aimMode         = ACTOR_403600_AIM_PLAYER;
    work->ignorePushOut   = 0;
    work->animRate        = ANIMATION_RATE_ONE;
    work->ambientBoost    = 0;
    work->committed       = 0;
    work->forwardSpeed    = 0;
    work->action          = ACTOR_403600_ACTION_CHOOSE;
    work->verticalSpeed   = 0;
    work->phaseFrame      = 0;
    work->turnRate        = ACTOR_403600_DEFAULT_TURN_RATE;
    work->roll            = 0;
    work->diving          = 0;
    work->repositioning   = 0;
    work->pauseSoundSent  = 0;
}

/// Subtracts hit damage from the boss and starts its defeat sequence at zero HP.
///
/// Uses the task's enemy and Actor403600Work, reporting damage to the target
/// readout. If the player is already dead, restores boss HP to 10 and returns.
/// Otherwise ends the child enemy, releases paired attacks and scripted player
/// control, cancels drain effects, restores combat defaults, then enters dying
/// mode with animation 1, no blend and zero whiteout. HP subtraction retains the
/// unsigned-halfword read followed by the enemy's signed-halfword store.
static void _actor403600ApplyDamage(Task* task, s32 damage)
{
    enum { ACTOR_403600_FATAL_HIT_PLAYER_DEAD_HP = 10,
           ACTOR_403600_CHILD_EXIT_STATE         = 2,
           ACTOR_403600_CHILD_FADE_STEP          = 1,
           ACTOR_403600_DEATH_ANIMATION          = 1,
           ACTOR_403600_DEFEAT_SCENE_HOLD        = 1,
           ACTOR_403600_PLAYER_ANIMATION_IDLE    = 0 };
    Enemy*           childEnemy;
    Enemy*           enemy;
    Actor403600Work* work;
    Task*            childTask;

    enemy     = task->spawnArg2.pointer;
    work      = task->work;
    enemy->hp = (u16)enemy->hp - damage;
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    if (enemy->hp <= 0) {
        if (gPlayerStatus.hp <= 0) {
            enemy->hp = ACTOR_403600_FATAL_HIT_PLAYER_DEAD_HP;
            return;
        }
        childEnemy = work->childEnemy;
        if (childEnemy != NULL) {
            childTask                                 = childEnemy->task;
            childTask->state                          = ACTOR_403600_CHILD_EXIT_STATE;
            childTask->killCountdown                  = 0;
            ((Actor403600Work*)childTask->work)->step = ACTOR_403600_CHILD_FADE_STEP;
        }
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        roomEffectRequestCancelPe();
        gGameSession->eventState            = ACTOR_403600_DEFEAT_SCENE_HOLD;
        D_actor_403600_80160568.animationId = ACTOR_403600_PLAYER_ANIMATION_IDLE;
        taskMessageDispatch(*gPlayerActorTasks, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        _actor403600ResetForDeath(task);
        _actor403600SetWeakTextures(0);
        _actor403600CancelDrain(task);
        work->defeated        = 1;
        work->animId          = ACTOR_403600_DEATH_ANIMATION;
        work->animBlendFrames = 0;
        work->animRate        = ANIMATION_RATE_ONE;
        work->mode            = ACTOR_403600_MODE_DYING;
        work->step            = 0;
        work->whiteout        = 0;
        padScriptHalt();
        sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_DRAIN_WINDUP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
}

/// Moves the boss or double along its facing and advances hit recoil.
///
/// Saves the old translation in prevPos before moving. Speeds are signed world
/// units per frame and rotation coefficients are Q12. A weakened actor uses
/// 60 percent of forwardSpeed. Committed motion follows all three facing axes;
/// otherwise Y takes verticalSpeed. The signed halfword countdown and recoil
/// step retain their wrap and clamp behavior; no transform cache is composed.
static void _actor403600Move(Task* task)
{
    enum {
        ACTOR_403600_WEAK_SPEED_PERCENT   = 60,
        ACTOR_403600_MATRIX_FRACTION_BITS = 12,
    };
    s16              nextRecoilSpeed;
    s16              nextRecoilHold;
    u16              speedBits;
    s16              signedSpeed;
    Actor403600Work* work;

    work             = task->work;
    speedBits        = work->forwardSpeed;
    work->prevPos.vx = work->worldCoord.coord.t[0];
    work->prevPos.vy = work->worldCoord.coord.t[1];
    work->prevPos.vz = work->worldCoord.coord.t[2];
    signedSpeed      = speedBits;
    if ((work->weakPhase != 0) && (signedSpeed != 0)) {
        speedBits = (signedSpeed * ACTOR_403600_WEAK_SPEED_PERCENT) / 100;
    }
    if (work->committed != 0) {
        work->worldCoord.coord.t[0] +=
            (work->worldCoord.coord.m[0][2] * ((s16)speedBits - work->recoilSpeed)) >> ACTOR_403600_MATRIX_FRACTION_BITS;
        work->worldCoord.coord.t[1] +=
            (work->worldCoord.coord.m[1][2] * ((s16)speedBits - work->recoilSpeed)) >> ACTOR_403600_MATRIX_FRACTION_BITS;
        work->worldCoord.coord.t[2] +=
            (work->worldCoord.coord.m[2][2] * ((s16)speedBits - work->recoilSpeed)) >> ACTOR_403600_MATRIX_FRACTION_BITS;
    } else {
        work->worldCoord.coord.t[1] += work->verticalSpeed;
        work->worldCoord.coord.t[0] +=
            (work->worldCoord.coord.m[0][2] * ((s16)speedBits - work->recoilSpeed)) >> ACTOR_403600_MATRIX_FRACTION_BITS;
        work->worldCoord.coord.t[2] +=
            (work->worldCoord.coord.m[2][2] * ((s16)speedBits - work->recoilSpeed)) >> ACTOR_403600_MATRIX_FRACTION_BITS;
    }
    // Hold recoil first, then decay it toward zero using signed halfword tests.
    nextRecoilHold   = work->recoilHold - 1;
    work->recoilHold = nextRecoilHold;
    if (nextRecoilHold < 0) {
        nextRecoilSpeed   = (u16)work->recoilSpeed - 1;
        work->recoilSpeed = nextRecoilSpeed;
        if (nextRecoilSpeed < 0) {
            work->recoilSpeed = 0;
        }
        work->recoilHold = 0;
    }
}

/// Turns the boss's yaw toward its player or flight aim and returns ground range.
///
/// Requires aimMode PLAYER or TARGET and a live Actor403600Work. turnStep is
/// angle units per call (4096 per turn); zero uses 32. Measures full-width X/Z
/// range in world units, but bearing operands narrow to signed halfwords.
/// Rebuilds a yaw-only rotation, discarding pitch/roll/scale while retaining
/// translation. Owns one temporary ActorFaceScratch block for the call.
static s32 _actor403600TurnYawToAim(Task* task, s16 turnStep)
{
    enum { ACTOR_403600_DEFAULT_YAW_STEP = 32 };
    Actor403600Work*  work;
    ActorFaceScratch* scratch;
    s16               effectiveTurnStep;
    s32               distance;
    u16               desiredYaw;
    s32               angleValue;
    s32               absoluteDifference;
    s32               turnDirection;
    s32               wrappedDifference;
    s32               nextYaw;

    effectiveTurnStep = turnStep;
    scratch           = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    work              = task->work;
    if ((turnStep << 16) == 0) {
        effectiveTurnStep = ACTOR_403600_DEFAULT_YAW_STEP;
    }

    switch (work->aimMode) {
        case ACTOR_403600_AIM_PLAYER:
            scratch->delta.vx = gPlayerStatus.coordMtx->t[0] - work->worldCoord.coord.t[0];
            scratch->delta.vy = 0;
            scratch->delta.vz = gPlayerStatus.coordMtx->t[2] - work->worldCoord.coord.t[2];
            break;
        case ACTOR_403600_AIM_TARGET:
            scratch->delta.vx = work->targetPos.vx - work->worldCoord.coord.t[0];
            scratch->delta.vy = 0;
            scratch->delta.vz = work->targetPos.vz - work->worldCoord.coord.t[2];
            break;
    }

    distance           = SquareRoot0((scratch->delta.vx * scratch->delta.vx) +
                                     (scratch->delta.vy * scratch->delta.vy) +
                                     (scratch->delta.vz * scratch->delta.vz));
    desiredYaw         = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & (ACTOR_TRANSFORM_ANGLE_TURN - 1);
    angleValue         = desiredYaw - (work->yaw & (ACTOR_TRANSFORM_ANGLE_TURN - 1));
    absoluteDifference = __builtin_abs((s16)angleValue);
    turnDirection      = angleValue;
    if (effectiveTurnStep >= absoluteDifference) {
        work->yaw = desiredYaw;
    } else {
        if (absoluteDifference >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
            wrappedDifference = angleValue - ACTOR_TRANSFORM_ANGLE_TURN;
            if ((s16)angleValue <= 0) {
                wrappedDifference = ACTOR_TRANSFORM_ANGLE_TURN - angleValue;
            }
            turnDirection = wrappedDifference;
        }
        // Reuse the signed angle temporary for the current heading.
        angleValue = (s16)work->yaw;
        if ((turnDirection << 16) > 0) {
            nextYaw = angleValue + effectiveTurnStep;
        } else {
            nextYaw = (s16)work->yaw - effectiveTurnStep;
        }
        work->yaw = nextYaw;
    }

    scratch->rot.vx = 0;
    scratch->rot.vy = work->yaw;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &work->worldCoord.coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
    return distance;
}

/// Builds upright aim angles from the scratch block's signed-halfword offset.
///
/// Overwrites direction with its Q12 unit vector and angles with the basis's
/// Euler angles, 4096 units per turn. A useful basis requires a nonzero offset
/// with a horizontal component so it is not parallel to the Y-axis hint. Borrows
/// the complete live scratch block and uses GTE working registers.
static inline void _actor403600BuildAimBasis(_Actor403600AimTurnScratch* scratch)
{
    gfxSetRotIdentity(&scratch->basis);
    VectorNormalSS(&scratch->direction, &scratch->direction);
    scratch->angles.vx = 0;
    scratch->angles.vy = ONE;
    scratch->angles.vz = 0;
    gfxBuildOrthonormalBasis(&scratch->basis, &scratch->direction, &scratch->angles);
    gfxMatrixToEuler(&scratch->basis, &scratch->angles);
}

/// Turns the boss's full rotation toward its aim and returns the distance to it.
///
/// Requires a valid ACTOR_403600_AIM value and live work. The target offset
/// narrows to signed halfwords before its 3D length is measured in world units;
/// PLAYER_LEVEL zeros Y. TARGET_SNAP takes the facing at once; other modes
/// step each Euler angle by turnRate (4096 units per turn). Adds roll, rebuilds
/// the basis and records yaw from its Z axis. Borrows one temporary scratch
/// block and uses GTE registers; does not compose worldCoord.workm.
static s32 _actor403600TurnToAim(Task* task)
{
    // Step one signed-halfword Euler angle, in 4096 units per turn. The two
    // arguments must be side-effect-free s16 lvalues: they are read repeatedly,
    // and angleSlot is written. Captures work->turnRate and the shared angle
    // temporaries below; scoped here so all three axes use the same locals.
#define ACTOR_403600_STEP_AIM_ANGLE(goalAngle, angleSlot)                                                          \
    {                                                                                                              \
        angleDifference = ((goalAngle) & ACTOR_TRANSFORM_ANGLE_MASK) - ((angleSlot) & ACTOR_TRANSFORM_ANGLE_MASK); \
        turnDirection   = angleDifference;                                                                         \
        if (work->turnRate >= __builtin_abs(angleDifference)) {                                                    \
            (angleSlot) = (goalAngle);                                                                             \
        } else {                                                                                                   \
            if (__builtin_abs(angleDifference) > ACTOR_TRANSFORM_ANGLE_HALF_TURN) {                                \
                wrappedDifference = angleDifference - ACTOR_TRANSFORM_ANGLE_TURN;                                  \
                if (angleDifference <= 0) {                                                                        \
                    wrappedDifference = ACTOR_TRANSFORM_ANGLE_TURN - angleDifference;                              \
                }                                                                                                  \
                turnDirection = wrappedDifference;                                                                 \
            }                                                                                                      \
            currentAngle = (angleSlot);                                                                            \
            if (turnDirection > 0) {                                                                               \
                nextAngle = currentAngle + work->turnRate;                                                         \
            } else {                                                                                               \
                nextAngle = currentAngle - work->turnRate;                                                         \
            }                                                                                                      \
            (angleSlot) = nextAngle;                                                                               \
        }                                                                                                          \
    }

    s16                         deltaX;
    s16                         deltaY;
    s16                         deltaZ;
    s16                         aimMode;
    s16                         angleDifference;
    s16                         turnDirection;
    s16                         wrappedDifference;
    s32                         currentAngle;
    s32                         nextAngle;
    s32                         deltaZSquared;
    s32                         range;
    s32                         levelDeltaX;
    Actor403600Work*            work;
    _Actor403600AimTurnScratch* scratch;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403600AimTurnScratch);
    work    = task->work;
    aimMode = work->aimMode;
    switch (aimMode) {
        case ACTOR_403600_AIM_PLAYER:
            scratch->direction.vx =
                (s16)(gPlayerStatus.coordMtx->t[0] - work->worldCoord.coord.t[0]);
            scratch->direction.vy =
                (s16)(gPlayerStatus.coordMtx->t[1] - work->worldCoord.coord.t[1]);
            scratch->direction.vz =
                (s16)(gPlayerStatus.coordMtx->t[2] - work->worldCoord.coord.t[2]);
            break;
        case ACTOR_403600_AIM_TARGET:
        case ACTOR_403600_AIM_TARGET_SNAP:
            scratch->direction.vx = (s16)(work->targetPos.vx - work->worldCoord.coord.t[0]);
            scratch->direction.vy = (s16)(work->targetPos.vy - work->worldCoord.coord.t[1]);
            scratch->direction.vz = (s16)(work->targetPos.vz - work->worldCoord.coord.t[2]);
            break;
        case ACTOR_403600_AIM_PLAYER_LEVEL:
            levelDeltaX           = (s16)(gPlayerStatus.coordMtx->t[0] - work->worldCoord.coord.t[0]);
            scratch->direction.vy = 0;
            scratch->direction.vx = levelDeltaX;
            scratch->direction.vz =
                (s16)(gPlayerStatus.coordMtx->t[2] - work->worldCoord.coord.t[2]);
            break;
    }
    deltaX        = scratch->direction.vx;
    deltaY        = scratch->direction.vy;
    deltaZ        = scratch->direction.vz;
    deltaZSquared = deltaZ * deltaZ;
    range         = SquareRoot0((deltaX * deltaX) + (deltaY * deltaY) + deltaZSquared);
    if (work->aimMode == ACTOR_403600_AIM_TARGET_SNAP) {
        _actor403600BuildAimBasis(scratch);
        scratch->angles.vz += work->roll;
        gfxRotMatrixXYZ(&work->worldCoord.coord, &scratch->angles, GRAPHICS_ROTATION_REPLACE);
        gfxReadMatrixZAxis(&work->worldCoord.coord, &scratch->angles);
    } else {
        _actor403600BuildAimBasis(scratch);
        // The direction is spent: its slot takes the boss's own angles, which
        // step toward the basis's one axis at a time.
        gfxMatrixToEuler(&work->worldCoord.coord, &scratch->direction);
        ACTOR_403600_STEP_AIM_ANGLE(scratch->angles.vx, scratch->direction.vx);

        ACTOR_403600_STEP_AIM_ANGLE(scratch->angles.vy, scratch->direction.vy);

        ACTOR_403600_STEP_AIM_ANGLE(scratch->angles.vz, scratch->direction.vz);
        scratch->direction.vz += work->roll;
        gfxRotMatrixXYZ(&work->worldCoord.coord, &scratch->direction, GRAPHICS_ROTATION_REPLACE);
        work->yaw = scratch->direction.vy;
        gfxReadMatrixZAxis(&work->worldCoord.coord, &scratch->angles);
    }
    // Either turn leaves the Z axis of the rebuilt rotation in `angles`.
    work->yaw = ratan2(scratch->angles.vx, scratch->angles.vz);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403600AimTurnScratch);
    return range;
#undef ACTOR_403600_STEP_AIM_ANGLE
}

/// Measures horizontal player range and player yaw in the reference's cached frame.
///
/// Requires live composed workm caches in the same frame and distinct writable
/// outputs. Bearing rotates the signed-halfword XYZ cache difference through
/// the reference's transpose; writes signed yaw in [-2048,2048], 4096 per turn.
/// Range ignores Y and uses full-width room X/Z from gPlayerStatus and coord,
/// returning whole world units; the squared X/Z sum must fit signed 32 bits.
/// Uses GTE working registers and one temporary
/// ActorRangeBearingScratch; does not compose or retain either coordinate.
static void _actor403600MeasurePlayerRangeBearing(const GfxCoord* referenceCoord, u32* rangeOut, s32* bearingOut)
{
    GfxCoord*                 playerCoord;
    s32                       deltaX;
    s32                       deltaZ;
    ActorRangeBearingScratch* scratch;

    scratch     = SCRATCH_STACK_RESERVE_BLOCK(ActorRangeBearingScratch);
    playerCoord = (*gPlayerActorTasks)->extra.tmd->coords;
    // Keep the raw output store before folding the caller-visible angle.
    *bearingOut = _actorAngleMeasureBearingInFrame(&scratch->bearing, referenceCoord, playerCoord);
    if (*bearingOut >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
        *bearingOut -= ACTOR_TRANSFORM_ANGLE_TURN;
    } else if (*bearingOut < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        *bearingOut += ACTOR_TRANSFORM_ANGLE_TURN;
    }
    deltaX             = gPlayerStatus.coordMtx->t[0] - referenceCoord->coord.t[0];
    scratch->offset.vx = deltaX;
    scratch->offset.vy = gPlayerStatus.coordMtx->t[1] - referenceCoord->coord.t[1];
    deltaZ             = gPlayerStatus.coordMtx->t[2] - referenceCoord->coord.t[2];
    scratch->offset.vz = deltaZ;
    *rangeOut          = SquareRoot0((deltaX * deltaX) + (deltaZ * deltaZ));
    SCRATCH_STACK_RELEASE_BLOCK(ActorRangeBearingScratch);
}

/// Returns the boss coordinate's yaw in the player's cached coordinate frame.
///
/// Both workm caches must be initialized in the same frame. The XYZ difference
/// narrows to signed halfwords and rotates through the player's transposed Q12
/// basis. Narrows the raw yaw before folding to [-2048,2048], 4096 per turn.
/// Borrows one ActorRangeBearingScratch block and uses GTE working registers;
/// refreshes neither coordinate and retains no storage.
static s16 _actor403600BearingFromPlayer(const GfxCoord* targetCoord)
{
    GfxCoord*                 playerCoord;
    s16                       angle;
    s16                       result;
    ActorRangeBearingScratch* scratch;

    scratch     = SCRATCH_STACK_RESERVE_BLOCK(ActorRangeBearingScratch);
    playerCoord = (*gPlayerActorTasks)->extra.tmd->coords;
    // Narrow before the comparisons; folding stays in the caller.
    angle  = _actorAngleMeasureBearingInFrame(&scratch->bearing, playerCoord, targetCoord);
    result = angle;
    if (angle >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
        result = angle - ACTOR_TRANSFORM_ANGLE_TURN;
    } else if (angle < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        result = angle + ACTOR_TRANSFORM_ANGLE_TURN;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorRangeBearingScratch);
    return result;
}

/// Faces and optionally relocates the player for the boss's knockback sequence.
///
/// placementFlags bit 0 faces the farther of the two fixed anchors (clear:
/// nearer); bit 1 places the player at the opposite anchor, keeping player Y.
/// With bit 1 clear, retains position and returns 0. With it set, returns 1
/// for placement at anchor A, 0 for anchor B; the caller uses this for view
/// selection. Yaw is 4096 units per turn. unusedTask is ignored. Dispatches a
/// borrowed static placement synchronously and marks the player's cache dirty.
static s32 _actor403600PlacePlayerForKnockback(Task* unusedTask, u16 placementFlags)
{
    enum { ACTOR_403600_KNOCKBACK_FACE_FAR_ANCHOR = 1,
           ACTOR_403600_KNOCKBACK_RELOCATE        = 2 };
    Task*     playerTask;
    GfxCoord* playerCoord;
    s32       deltaZ;
    s32       distanceB;
    s32       deltaX;
    s32       distanceA;
    s32       bearingA;
    s32       placedAtA;
    s32       bearingB;

    playerTask  = *gPlayerActorTasks;
    playerCoord = playerTask->extra.tmd->coords;
    deltaX      = D_actor_403600_801605E4.vx - playerCoord->coord.t[0];
    deltaZ      = D_actor_403600_801605E4.vz - playerCoord->coord.t[2];
    placedAtA   = 0;
    distanceA   = SquareRoot0((deltaX * deltaX) + (deltaZ * deltaZ));
    bearingA    = ratan2(deltaX, deltaZ);
    if (bearingA >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
        bearingA -= ACTOR_TRANSFORM_ANGLE_TURN;
    } else if (bearingA < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        bearingA += ACTOR_TRANSFORM_ANGLE_TURN;
    }
    deltaX    = D_actor_403600_801605EC.vx - playerCoord->coord.t[0];
    deltaZ    = D_actor_403600_801605EC.vz - playerCoord->coord.t[2];
    distanceB = SquareRoot0((deltaX * deltaX) + (deltaZ * deltaZ));
    bearingB  = ratan2(deltaX, deltaZ);
    if (bearingB >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
        bearingB -= ACTOR_TRANSFORM_ANGLE_TURN;
    } else if (bearingB < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        bearingB += ACTOR_TRANSFORM_ANGLE_TURN;
    }
    if ((placementFlags & ACTOR_403600_KNOCKBACK_FACE_FAR_ANCHOR) ? (distanceB < distanceA) : (distanceA < distanceB)) {
        D_actor_403600_801606E0.placement.rot.vy = bearingA;
    } else {
        D_actor_403600_801606E0.placement.rot.vy = bearingB;
    }
    playerCoord->composeStamp                = GRAPHICS_COORD_DIRTY;
    D_actor_403600_801606E0.placement.rot.vx = 0;
    D_actor_403600_801606E0.placement.rot.vz = 0;
    if (placementFlags & ACTOR_403600_KNOCKBACK_RELOCATE) {
        D_actor_403600_801606E0.placement.pos.vy = playerCoord->coord.t[1];
        if (D_actor_403600_801606E0.placement.rot.vy == bearingA) {
            D_actor_403600_801606E0.placement.pos.vx = D_actor_403600_801605EC.vx;
            D_actor_403600_801606E0.placement.pos.vz = D_actor_403600_801605EC.vz;
            placedAtA                                = 0;
        } else {
            D_actor_403600_801606E0.placement.pos.vx = D_actor_403600_801605E4.vx;
            D_actor_403600_801606E0.placement.pos.vz = D_actor_403600_801605E4.vz;
            placedAtA                                = 1;
        }
    } else {
        D_actor_403600_801606E0.placement.pos.vx = playerCoord->coord.t[0];
        D_actor_403600_801606E0.placement.pos.vy = playerCoord->coord.t[1];
        D_actor_403600_801606E0.placement.pos.vz = playerCoord->coord.t[2];
    }
    TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_403600_801606E0.placement, 0);
    return placedAtA;
}

/// Chooses and initializes a boss attack from player position and combat history.
///
/// Requires live boss work, enemy and player coordinates. Random draws choose
/// one of six attack picks; HP bands gate volley/swoop and summoning, and the
/// last two picks prevent a third consecutive repeat. Rejected picks leave
/// CHOOSE for a later retry. An enabled forced pick bypasses those gates and
/// history. Initializes the selected action's target, timing, aim and animation;
/// does not advance movement or animation. Coordinates are whole world units.
static void _actor403600ChooseAttack(Task* task)
{
    enum {
        ACTOR_403600_PLAYER_FLOOR_Y       = -2000,
        ACTOR_403600_PLAYER_MIDDLE_Y      = -4100,
        ACTOR_403600_PLAYER_CENTRE_X      = 4000,
        ACTOR_403600_PLAYER_CENTRE_Z      = 3000,
        ACTOR_403600_PLAYER_CENTRE_SPAN   = 8001,
        ACTOR_403600_ATTACK_PICK_NONE     = 0,
        ACTOR_403600_ATTACK_PICK_SLAM     = 1,
        ACTOR_403600_ATTACK_PICK_VOLLEY   = 2,
        ACTOR_403600_ATTACK_PICK_SWOOP    = 3,
        ACTOR_403600_ATTACK_PICK_RUSH     = 4,
        ACTOR_403600_ATTACK_PICK_SUMMON   = 5,
        ACTOR_403600_ATTACK_PICK_DRAIN    = 6,
        ACTOR_403600_SUMMON_LIMIT         = 10,
        ACTOR_403600_DRAIN_CHARGE_FRAMES  = 210,
        ACTOR_403600_DRAIN_FIRST_MP_DELAY = 20,
        ACTOR_403600_DRAIN_NEXT_MP_DELAY  = 19
    };
    s32 recentAttackSlot;
    s32 highCornerAXSquared;
    s32 lowCornerAXSquared;
    s32 highCornerARange;
    s32 lowCornerARange;
    // Matching constraint: the load must not be a single-set pseudo, or sched1
    // promotes it as a register birth and moves it down beside the compare.
    register s32     hp asm("a0");
    s32              highCornerBRange;
    s32              playerY;
    s32              cornerDelta;
    u32              attackCase;
    s32              highCornerBXSquared;
    s32              lowCornerBXSquared;
    s32              attackPick;
    Actor403600Work* work;
    Enemy*           enemy;

    work             = task->work;
    enemy            = task->spawnArg2.pointer;
    attackPick       = ACTOR_403600_ATTACK_PICK_NONE;
    work->playerZone = 0;
    if (work->childEnemy != NULL) {
        if (_actor403600Rand() & 1) {
            attackPick = ACTOR_403600_ATTACK_PICK_VOLLEY;
        } else {
            attackPick = ACTOR_403600_ATTACK_PICK_SWOOP;
        }
    } else {
        playerY = gPlayerStatus.coordMtx->t[1];
        if (playerY >= ACTOR_403600_PLAYER_FLOOR_Y) {
            s32 roll;

            work->playerZone = ACTOR_403600_PLAYER_ZONE_FLOOR;
            roll             = _actor403600Rand() & 0xF;
            if (roll < 3) {
                attackPick = ACTOR_403600_ATTACK_PICK_VOLLEY;
            } else if (roll < 6) {
                attackPick = ACTOR_403600_ATTACK_PICK_SWOOP;
            } else if (roll < 0xB) {
                attackPick = ACTOR_403600_ATTACK_PICK_SUMMON;
            } else {
                if (_actor403600Rand() & 1) {
                    attackPick = ACTOR_403600_ATTACK_PICK_DRAIN;
                } else {
                    attackPick = ACTOR_403600_ATTACK_PICK_RUSH;
                }
            }
        } else if (playerY >= ACTOR_403600_PLAYER_MIDDLE_Y) {
            if (((u32)(gPlayerStatus.coordMtx->t[0] - ACTOR_403600_PLAYER_CENTRE_X) < (u32)ACTOR_403600_PLAYER_CENTRE_SPAN) &&
                ((u32)(gPlayerStatus.coordMtx->t[2] - ACTOR_403600_PLAYER_CENTRE_Z) < (u32)ACTOR_403600_PLAYER_CENTRE_SPAN)) {
                s32 roll;

                work->playerZone = ACTOR_403600_PLAYER_ZONE_CENTRE;
                hp               = enemy->hp;
                roll             = _actor403600Rand();
                roll            &= 0xF;
                if (work->hpAt60Percent < hp) {
                    attackPick = ACTOR_403600_ATTACK_PICK_SLAM;
                    if (roll & 1) {
                        attackPick = ACTOR_403600_ATTACK_PICK_RUSH;
                    }
                } else {
                    if (roll < 2) {
                        attackPick = ACTOR_403600_ATTACK_PICK_RUSH;
                    } else if (roll < 6) {
                        attackPick = ACTOR_403600_ATTACK_PICK_SWOOP;
                    } else {
                        if (_actor403600Rand() & 1) {
                            attackPick = ACTOR_403600_ATTACK_PICK_VOLLEY;
                        } else {
                            attackPick = ACTOR_403600_ATTACK_PICK_SLAM;
                        }
                    }
                }
            } else {
                s32 roll;

                work->playerZone = ACTOR_403600_PLAYER_ZONE_OUTSIDE;
                roll             = _actor403600Rand() & 0xF;
                if (roll < 2) {
                    attackPick = ACTOR_403600_ATTACK_PICK_SWOOP;
                } else if (roll < 5) {
                    attackPick = ACTOR_403600_ATTACK_PICK_DRAIN;
                } else if (roll < 8) {
                    attackPick = ACTOR_403600_ATTACK_PICK_RUSH;
                } else {
                    if (_actor403600Rand() & 1) {
                        attackPick = ACTOR_403600_ATTACK_PICK_VOLLEY;
                    } else {
                        attackPick = ACTOR_403600_ATTACK_PICK_SUMMON;
                    }
                }
            }
        }
    }
    // Forced picks bypass both combat gates and the two-pick repetition history.
    if (D_actor_403600_80160695 != 0) {
        attackPick = D_actor_403600_80160694;
    } else {
        if (!((((u32)(attackPick - ACTOR_403600_ATTACK_PICK_VOLLEY) >= 2U) || (enemy->hp <= work->hpAt60Percent)) &&
              ((attackPick != ACTOR_403600_ATTACK_PICK_SUMMON) ||
               ((enemy->hp <= work->hpAt35Percent) && (work->summonCount < ACTOR_403600_SUMMON_LIMIT))) &&
              ((D_actor_403600_801606B8[0] != attackPick) ||
               (D_actor_403600_801606B8[1] != attackPick)))) {
            return;
        }
        recentAttackSlot                          = D_actor_403600_801606BC.nextSlot;
        D_actor_403600_801606B8[recentAttackSlot] = (u16)attackPick;
        D_actor_403600_801606BC.nextSlot          = recentAttackSlot ^ 1;
    }
    // Pick numbers 1..6 map to action initialization; the action values are separate.
    attackCase = attackPick - ACTOR_403600_ATTACK_PICK_SLAM;
    switch (attackCase) {
        case ACTOR_403600_ATTACK_PICK_SLAM - ACTOR_403600_ATTACK_PICK_SLAM:
            work->aimMode = ACTOR_403600_AIM_TARGET;
            work->roll    = 0;
            work->step    = 0;
            work->action  = ACTOR_403600_ACTION_SLAM;
            return;
        case ACTOR_403600_ATTACK_PICK_VOLLEY - ACTOR_403600_ATTACK_PICK_SLAM:
            work->animId      = ACTOR_403600_ANIM_DIVE_WINDUP;
            work->actionParam = 0x32;
            work->aimMode     = ACTOR_403600_AIM_PLAYER;
            work->action      = ACTOR_403600_ACTION_VOLLEY;
            return;
        case ACTOR_403600_ATTACK_PICK_SWOOP - ACTOR_403600_ATTACK_PICK_SLAM:
            cornerDelta         = gPlayerStatus.coordMtx->t[0] - D_actor_403600_8016063C[0].vx;
            highCornerAXSquared = cornerDelta * cornerDelta;
            cornerDelta         = gPlayerStatus.coordMtx->t[2] - D_actor_403600_8016063C[0].vz;
            highCornerARange    = SquareRoot0(highCornerAXSquared + (cornerDelta * cornerDelta));
            cornerDelta         = gPlayerStatus.coordMtx->t[0] - D_actor_403600_8016063C[1].vx;
            highCornerBXSquared = cornerDelta * cornerDelta;
            cornerDelta         = gPlayerStatus.coordMtx->t[2] - D_actor_403600_8016063C[1].vz;
            highCornerBRange    = SquareRoot0(highCornerBXSquared + (cornerDelta * cornerDelta));
            if (work->playerZone == ACTOR_403600_PLAYER_ZONE_CENTRE) {
                if (highCornerARange < highCornerBRange) {
                    work->swoopCorners = 0U;
                    work->targetPos.vx = D_actor_403600_8016063C[1].vx;
                    work->targetPos.vy = D_actor_403600_8016063C[1].vy;
                    work->targetPos.vz = D_actor_403600_8016063C[1].vz;
                } else {
                    work->swoopCorners = 1U;
                    work->targetPos.vx = D_actor_403600_8016063C[0].vx;
                    work->targetPos.vy = D_actor_403600_8016063C[0].vy;
                    work->targetPos.vz = D_actor_403600_8016063C[0].vz;
                }
            } else {
                if (highCornerARange < highCornerBRange) {
                    work->swoopCorners = 0U;
                    work->targetPos.vx = D_actor_403600_8016063C[0].vx;
                    work->targetPos.vy = D_actor_403600_8016063C[0].vy;
                    work->targetPos.vz = D_actor_403600_8016063C[0].vz;
                } else {
                    work->swoopCorners = 1U;
                    work->targetPos.vx = D_actor_403600_8016063C[1].vx;
                    work->targetPos.vy = D_actor_403600_8016063C[1].vy;
                    work->targetPos.vz = D_actor_403600_8016063C[1].vz;
                }
                cornerDelta        = gPlayerStatus.coordMtx->t[0] - D_actor_403600_8016064C[0].vx;
                lowCornerAXSquared = cornerDelta * cornerDelta;
                cornerDelta        = gPlayerStatus.coordMtx->t[2] - D_actor_403600_8016064C[0].vz;
                lowCornerARange    = SquareRoot0(lowCornerAXSquared + (cornerDelta * cornerDelta));
                cornerDelta        = gPlayerStatus.coordMtx->t[0] - D_actor_403600_8016064C[1].vx;
                lowCornerBXSquared = cornerDelta * cornerDelta;
                cornerDelta        = gPlayerStatus.coordMtx->t[2] - D_actor_403600_8016064C[1].vz;
                if (SquareRoot0(lowCornerBXSquared + (cornerDelta * cornerDelta)) < lowCornerARange) {
                    work->swoopCorners = (u16)(work->swoopCorners | 2);
                }
            }
            work->ignorePushOut = 1;
            work->step          = 0;
            work->aimMode       = ACTOR_403600_AIM_PLAYER;
            work->actionParam   = 0;
            work->action        = ACTOR_403600_ACTION_SWOOP;
            return;
        case ACTOR_403600_ATTACK_PICK_RUSH - ACTOR_403600_ATTACK_PICK_SLAM:
            work->ignorePushOut   = 1;
            work->step            = 0;
            work->aimMode         = ACTOR_403600_AIM_TARGET;
            work->action          = ACTOR_403600_ACTION_RUSH;
            work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->rushPasses      = (_actor403600Rand() & 3) * 2;
            work->actionParam     = (_actor403600Rand() % 20) + 0x28;
            _actor403600ChooseFlightTarget(task, 1);
            return;
        case ACTOR_403600_ATTACK_PICK_SUMMON - ACTOR_403600_ATTACK_PICK_SLAM:
            work->aimMode     = ACTOR_403600_AIM_PLAYER;
            work->animId      = ACTOR_403600_ANIM_FLY;
            work->action      = ACTOR_403600_ACTION_SUMMON;
            work->actionParam = (_actor403600Rand() % 20) + 0x28;
            work->summonCount++;
            return;
        case ACTOR_403600_ATTACK_PICK_DRAIN - ACTOR_403600_ATTACK_PICK_SLAM:
            work->animId        = ACTOR_403600_ANIM_FLY;
            work->actionParam   = ACTOR_403600_DRAIN_CHARGE_FRAMES;
            work->action        = ACTOR_403600_ACTION_DRAIN;
            work->actionTimer   = ACTOR_403600_DRAIN_FIRST_MP_DELAY;
            work->actionCounter = ACTOR_403600_DRAIN_NEXT_MP_DELAY;
            break;
    }
}

/// Moves the scripted player along its own facing on X/Z, in world units.
///
/// Requires live boss work/player root. Rotation coefficients use Q12; Y and
/// coordinate stamps are left to the player animation/message handling.
static inline void _actor403600MoveKnockedBackPlayer(Task* task)
{
    enum { ACTOR_403600_FACING_FRACTION_BITS = 12 };
    Actor403600Work* work  = task->work;
    GfxCoord*        coord = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;

    coord->coord.t[0] += (coord->coord.m[0][2] * work->knockbackSpeed) >> ACTOR_403600_FACING_FRACTION_BITS;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->knockbackSpeed) >> ACTOR_403600_FACING_FRACTION_BITS;
}

/// Advances the player's scripted heavy or directional knockback sequence.
///
/// The package's player-animation request is also the sequence state (0 idle,
/// 1 heavy flight, 2 impact, 3 backward, 4 forward, 5/6 recovery). Requires a live
/// player model and boss work. Moves X/Z along the player's Q12 facing using a
/// signed world-unit speed, changes view/placement on heavy landing, applies the
/// impact damage and releases scripted control when recovery ends.
static void _actor403600UpdatePlayerKnockback(Task* task)
{
    enum {
        ACTOR_403600_SOUND_KNOCKBACK_IMPACT        = 0x54160011,
        ACTOR_403600_SOUND_LIGHT_KNOCKBACK         = 0x54160012,
        ACTOR_403600_SOUND_HEAVY_PLAYER_HIT        = 6,
        ACTOR_403600_KNOCKBACK_IMPACT_FRAME        = 12,
        ACTOR_403600_KNOCKBACK_HEAVY_END_FRAME     = 102,
        ACTOR_403600_KNOCKBACK_DIRECTION_END_FRAME = 36,
        ACTOR_403600_KNOCKBACK_RECOVERY_FRAMES     = 40,
        ACTOR_403600_KNOCKBACK_RELOCATE_FACE_FAR   = 1 | 2,
        ACTOR_403600_KNOCKBACK_VIEW_AT_A           = 3,
        ACTOR_403600_KNOCKBACK_VIEW_AT_B           = 7,
        ACTOR_403600_KNOCKBACK_IMPACT_POWER        = 20
    };
    s16       nextBackwardSpeed;
    s16       backwardAction;
    s16       nextForwardSpeed;
    s16       forwardAction;
    s32       soundKey;
    GfxCoord* playerCoord;
    s32       heavyHitPan;
    s32       impactPan;
    s32       backwardPan;
    s32       forwardPan;
    u16       impactFrame;
    u16       recoveryFrame;

    Actor403600Work* work;

    playerCoord = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    work        = task->work;
    switch (D_actor_403600_80160568.animationId) {
        case ACTOR_403600_PLAYER_ANIM_HEAVY_KNOCKBACK:
            // Finish the heavy flight at one of the two scripted landing viewpoints.
            work->knockbackFrame++;
            _actor403600MoveKnockedBackPlayer(task);
            if ((work->knockbackFrame >= ACTOR_403600_KNOCKBACK_IMPACT_FRAME) || (gGameSession->viewReady != 0)) {
                work->knockbackFrame = 0;
                if (_actor403600PlacePlayerForKnockback(task, ACTOR_403600_KNOCKBACK_RELOCATE_FACE_FAR) == 0) {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_403600_KNOCKBACK_VIEW_AT_B;
                } else {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_403600_KNOCKBACK_VIEW_AT_A;
                }
                D_actor_403600_80160568.animationId = ACTOR_403600_PLAYER_ANIM_HEAVY_IMPACT;
                Gp_StateC08.flags                  |= ATTACHMENT_FLAG_EVENT_LOCK;
                TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
                return;
            }
            break;

        case ACTOR_403600_PLAYER_ANIM_HEAVY_IMPACT:
            impactFrame          = work->knockbackFrame + 1;
            work->knockbackFrame = impactFrame;
            if ((s16)impactFrame == ACTOR_403600_KNOCKBACK_IMPACT_FRAME) {
                padScriptSpawnVariableMotorRamp(0xA, 0xFF, 0xFF);
                D_actor_403600_801606A4.power    = ACTOR_403600_KNOCKBACK_IMPACT_POWER;
                D_actor_403600_801606A4.reaction = 0;
                soundKey                         = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403600_SOUND_HEAVY_PLAYER_HIT;
                heavyHitPan                      = (s8)worldCoordGetOriginAudioPan(playerCoord);
                sndEvtRequestScriptStart(soundKey, heavyHitPan,
                                         (s8)worldCoordGetOriginAudioDepth(playerCoord));
                soundKey =
                    (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403600_SOUND_KNOCKBACK_IMPACT;
                impactPan = (s8)worldCoordGetOriginAudioPan(playerCoord);
                sndEvtRequestScriptStart(soundKey, impactPan,
                                         (s8)worldCoordGetOriginAudioDepth(playerCoord));
                taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_APPLY_DAMAGE,
                                    damagePackAttackKey(&D_actor_403600_801606A4, 0), 0);
            }
            if (work->knockbackFrame >= ACTOR_403600_KNOCKBACK_HEAVY_END_FRAME) {
                work->knockbackFrame                = 0;
                D_actor_403600_80160568.animationId = ACTOR_403600_PLAYER_ANIM_NONE;
                taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            break;

        case ACTOR_403600_PLAYER_ANIM_BACKWARD:
            work->knockbackFrame++;
            nextBackwardSpeed    = (u16)work->knockbackSpeed + 2;
            work->knockbackSpeed = nextBackwardSpeed;
            if (nextBackwardSpeed > 0) {
                work->knockbackSpeed = 0;
            }
            _actor403600MoveKnockedBackPlayer(task);
            backwardAction = work->action;
            if ((backwardAction != ACTOR_403600_ACTION_DRAIN) && (backwardAction != ACTOR_403600_ACTION_RUSH) &&
                (work->knockbackFrame == ACTOR_403600_KNOCKBACK_IMPACT_FRAME)) {
                soundKey =
                    (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403600_SOUND_LIGHT_KNOCKBACK;
                backwardPan = (s8)worldCoordGetOriginAudioPan(playerCoord);
                sndEvtRequestScriptStart(soundKey, backwardPan,
                                         (s8)worldCoordGetOriginAudioDepth(playerCoord));
            }
            if (work->knockbackFrame >= ACTOR_403600_KNOCKBACK_DIRECTION_END_FRAME) {
                Gp_StateC08.flags                  |= ATTACHMENT_FLAG_EVENT_LOCK;
                work->knockbackFrame                = 0;
                D_actor_403600_80160568.animationId = ACTOR_403600_PLAYER_ANIM_BACKWARD_RECOVER;
                TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
                return;
            }
            break;

        case ACTOR_403600_PLAYER_ANIM_FORWARD:
            work->knockbackFrame++;
            nextForwardSpeed     = (u16)work->knockbackSpeed - 2;
            work->knockbackSpeed = nextForwardSpeed;
            if (nextForwardSpeed < 0) {
                work->knockbackSpeed = 0;
            }
            _actor403600MoveKnockedBackPlayer(task);
            forwardAction = work->action;
            if ((forwardAction != ACTOR_403600_ACTION_DRAIN) && (forwardAction != ACTOR_403600_ACTION_RUSH) &&
                (work->knockbackFrame == ACTOR_403600_KNOCKBACK_IMPACT_FRAME)) {
                soundKey =
                    (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403600_SOUND_LIGHT_KNOCKBACK;
                forwardPan = (s8)worldCoordGetOriginAudioPan(playerCoord);
                sndEvtRequestScriptStart(soundKey, forwardPan,
                                         (s8)worldCoordGetOriginAudioDepth(playerCoord));
            }
            if (work->knockbackFrame >= ACTOR_403600_KNOCKBACK_DIRECTION_END_FRAME) {
                Gp_StateC08.flags                  |= ATTACHMENT_FLAG_EVENT_LOCK;
                work->knockbackFrame                = 0;
                D_actor_403600_80160568.animationId = ACTOR_403600_PLAYER_ANIM_FORWARD_RECOVER;
                TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
                return;
            }
            break;

        case ACTOR_403600_PLAYER_ANIM_BACKWARD_RECOVER:
        case ACTOR_403600_PLAYER_ANIM_FORWARD_RECOVER:
            recoveryFrame        = work->knockbackFrame + 1;
            work->knockbackFrame = recoveryFrame;
            if ((s16)recoveryFrame >= ACTOR_403600_KNOCKBACK_RECOVERY_FRAMES) {
                work->knockbackFrame                = 0;
                D_actor_403600_80160568.animationId = ACTOR_403600_PLAYER_ANIM_NONE;
                taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            break;
    }
}

/// Advances the drain attack's puff timing and spawns puffs on the player rig.
///
/// Requires a live player model with coordinates 0..18. Ordinary mode shortens
/// the interval by eight toward two frames and grows the puff size toward 1024;
/// each due puff consumes one LCG draw and chooses coordinate 0..18. Interval
/// -1 instead puffs coordinates 1..18 every third frame at size 1024. Coordinates
/// are borrowed by the spawned effects; the player must outlive those effects.
static void _actor403600UpdateDrainPuffs(Task* task)
{
    Actor403600Work* work;
    s16              nextInterval;
    s16              interval;
    s32              puffSize;
    s32              waiting;
    s32              partIndex;
    u16              intervalBits;
    u16              burstFrames;
    u16              singlePuffFrames;
    u16              nextSize;
    u32              randomDraw;

    work     = task->work;
    interval = work->drainPuffInterval;
    if (interval == ACTOR_403600_DRAIN_PUFF_ALL_PARTS) {
        burstFrames           = (u16)work->drainPuffFrames + 1;
        work->drainPuffFrames = burstFrames;
        if ((s16)burstFrames >= ACTOR_403600_DRAIN_PUFF_BURST_INTERVAL) {
            partIndex             = 1;
            work->drainPuffFrames = 0;
            do {
                effectSpawn(EFFECT_ADDITIVE_PUFF, (*gPlayerActorTasks)->extra.tmd->coords + partIndex, ACTOR_403600_DRAIN_PUFF_MAX_SIZE, NULL);
                partIndex += 1;
            } while (partIndex < ACTOR_403600_DRAIN_PUFF_COORD_LIMIT);
        }
    } else {
        singlePuffFrames      = (u16)work->drainPuffFrames + 1;
        work->drainPuffFrames = singlePuffFrames;
        waiting               = (s16)singlePuffFrames < interval;
        intervalBits          = (u16)work->drainPuffInterval;
        if (!waiting) {
            nextInterval            = intervalBits - 8;
            work->drainPuffInterval = nextInterval;
            if (nextInterval < ACTOR_403600_DRAIN_PUFF_MIN_INTERVAL + 1) {
                work->drainPuffInterval = ACTOR_403600_DRAIN_PUFF_MIN_INTERVAL;
            }
            nextSize           = (u16)work->drainPuffArg + 1;
            work->drainPuffArg = nextSize;
            if ((s16)nextSize >= ACTOR_403600_DRAIN_PUFF_MAX_SIZE) {
                work->drainPuffArg = ACTOR_403600_DRAIN_PUFF_MAX_SIZE;
            }
            randomDraw = _actor403600Rand();
            puffSize   = work->drainPuffArg;

            effectSpawn(EFFECT_ADDITIVE_PUFF,
                        &(*gPlayerActorTasks)->extra.tmd->coords[(randomDraw % ACTOR_403600_DRAIN_PUFF_COORD_LIMIT) & 0xFFFF],
                        puffSize, NULL);
            work->drainPuffFrames = 0;
        }
    }
}

/// Reseeds or ticks the selected animation on model parts 1 through partCount-1.
///
/// Requires a live `Actor403600Work`, an animId in the package's 22-entry table
/// and partCount no greater than the twenty-slot rig. A NULL table entry does
/// nothing. A changed animation resets phaseFrame and seeks each driven slot
/// with animBlendFrames; subsequent calls increment phaseFrame and tick those
/// slots at animRate (16 is normal playback). Part 0 is not driven.
static __inline__ void _actor403600UpdateAnimation(Task* task, u8 partCount)
{
    Actor403600Work* work;
    s32              partIndex;

    work = task->work;
    if (D_actor_403600_8016057C[work->animId] != 0) {
        if (work->animId != work->appliedAnimId) {
            work->appliedAnimId = work->animId;
            work->phaseFrame    = 0;
            for (partIndex = 1; partIndex < partCount; partIndex++) {
                animationSeekSlotWithBlend(&work->rig.anim, partIndex, work->animId, 0, work->animBlendFrames);
            }
        } else {
            work->phaseFrame++;
            for (partIndex = 1; partIndex < partCount; partIndex++) {
                work->rig.slots[partIndex].rate = work->animRate;
                animationTickSlot(&work->rig.anim, partIndex);
            }
        }
    }
}

/// Creates the translucent summoned double at the live boss's position and yaw.
///
/// Allocates and owns one Actor403600Work through the task, or destroys the enemy
/// on allocation failure. Requires the singleton boss and model parts 0..19.
/// Links the target and hit/attack spheres, installs the shared animation rig,
/// acquires a battle reference and starts the entrance action. One LCG draw sets
/// lifetime to 3000..3049 frames; nonzero spawnArg1 transfers the player's lock.
static void _actor403600SpawnDouble(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_403600_DOUBLE_MIN_LIFETIME    = 3000,
        ACTOR_403600_DOUBLE_LIFETIME_SPREAD = 50
    };
    SVECTOR                rotation;
    TmdObject*             model;
    GfxCoord*              worldCoord;
    GfxCoord*              modelCoord;
    GfxCoord*              bodyCoord;
    WorldCollisionContact* bodyRecs;
    WorldCollisionContact* attackRecs;
    Actor403600Work*       ownerWork;
    Actor403600Work*       work;
    u32                    randomProduct;
    s32                    yaw;
    s32                    partIndex;
    u32                    randomState;

    model      = task->extra.tmd;
    modelCoord = model->coords;
    ownerWork  = D_actor_403600_801606A8->work;
    work       = memCalloc(sizeof(Actor403600Work), false);
    bodyCoord  = &modelCoord[1];
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    worldCoord                        = &work->worldCoord;
    task->work                        = work;
    model->flags                      = TMD_OBJECT_SEMI_TRANS;
    model->shading.screenFadeDistance = 0;
    work->worldCoord.parent           = &gGfxViewCoord;
    gfxSetRotIdentity(&work->worldCoord.coord);
    work->worldCoord.coord.t[0] = 0;
    work->worldCoord.coord.t[1] = 0;
    work->worldCoord.coord.t[2] = 0;
    modelCoord->parent          = worldCoord;
    gfxSetRotIdentity(&modelCoord->coord);
    modelCoord->coord.t[0]        = 0;
    modelCoord->coord.t[1]        = 0x744;
    modelCoord->coord.t[2]        = 0;
    work->worldCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(worldCoord);
    modelCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(modelCoord);
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    enemy->field_4  = &modelCoord[1].coord;
    enemy->field_48 = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
    enemy->coord                  = bodyCoord;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &D_actor_403600_80150ED8;
    enemy->recs                   = work->hitContacts;
    enemy->hp                     = D_actor_403600_80150ED8.hpMax;
    animationInitContext(&work->rig.anim, D_actor_403600_8016057C, model, work->rig.poses, work->rig.slots);
    partIndex = 1;
    do {
        animationResetSlot(&work->rig.anim, partIndex, 1);
        partIndex += 1;
    } while (partIndex < ARRAY_SIZE(work->rig.slots));
    (sceneAcquireBattleRef)(0);
    bodyRecs            = work->hitContacts;
    work->animId        = ACTOR_403600_ANIM_DOUBLE_APPEAR;
    work->appliedAnimId = 0;
    work->hitCooldown   = 0;
    work->field_6C0 =
        &task->extra.tmd->coords[1];
    work->field_6C4                = 0x100;
    work->field_6C6                = 1;
    work->hitEffectOffset.vx       = 0;
    work->hitEffectOffset.vy       = 0;
    work->hitEffectOffset.vz       = 0;
    work->action                   = ACTOR_403600_DOUBLE_ACTION_WAIT;
    work->hitBody.coord            = bodyCoord;
    work->hitBody.context.contacts = bodyRecs;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = 0x30024;
    work->hitBody.radius           = 0x3E8;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(bodyRecs, ARRAY_SIZE(work->hitContacts), 0);
    attackRecs                        = work->attackContacts;
    work->attackBody.coord            = bodyCoord;
    work->attackBody.context.contacts = attackRecs;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0x3E8;
    work->hitBody.flags              |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->attackBody.key              = damagePackAttackKey(&D_actor_403600_80150EB0, 0);
    work->attackBody.radius           = 0x5DC;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(attackRecs, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->worldCoord.coord.t[0] = ownerWork->worldCoord.coord.t[0];
    work->worldCoord.coord.t[1] = ownerWork->worldCoord.coord.t[1];
    work->worldCoord.coord.t[2] = ownerWork->worldCoord.coord.t[2];
    gfxReadMatrixZAxis(&ownerWork->worldCoord.coord, &rotation);
    yaw         = ratan2(rotation.vx, rotation.vz);
    rotation.vx = 0;
    rotation.vy = (s16)yaw;
    rotation.vz = 0;
    RotMatrix(&rotation, &work->worldCoord.coord);
    randomProduct            = gRandomLcgState * RANDOM_LCG_MULTIPLIER;
    randomState              = randomProduct + RANDOM_LCG_INCREMENT;
    work->yaw                = yaw;
    work->animBlendFrames    = 0;
    work->defeated           = 0;
    work->age                = 0;
    work->action             = ACTOR_403600_DOUBLE_ACTION_APPEAR;
    work->colorRefreshFrames = 0xA;
    work->chaseSpeed         = 0x14;
    work->lifetime           = (s16)(((randomState >> 0x10) % ACTOR_403600_DOUBLE_LIFETIME_SPREAD) + ACTOR_403600_DOUBLE_MIN_LIFETIME);
    gRandomLcgState          = randomState;
    if (task->spawnArg1.value != 0) {
        worldTargetSetPlayerLock(&enemy->node);
    }
    _actor403600UpdateAnimation(task, ARRAY_SIZE(work->rig.slots));
    work->animRate     = ANIMATION_RATE_ONE;
    task->exitCallback = _actor403600EnemyExit;
    task->state       += 1;
}

/// Samples the actor's lighting colour at its cached work-coordinate translation.
///
/// Requires the task's live Actor403600Work and an initialized worldCoord.workm;
/// does not compose the coordinate. Passes a temporary full-width VECTOR to the
/// world colour query, with both options zero, and releases it before returning.
/// The enemy's lighting and colour matrices are updated; no pointer is retained.
static __inline__ void _actor403600UpdateColor(Enemy* enemy, Task* task)
{
    Actor403600Work* work;
    VECTOR*          position;

    work         = task->work;
    position     = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    position->vx = work->worldCoord.workm.t[0];
    position->vy = work->worldCoord.workm.t[1];
    position->vz = work->worldCoord.workm.t[2];
    worldCoordUpdateActorColor(enemy, position, 0, 0);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Post-multiplies part 2's Q12 basis by the flinch basis using GTE registers.
///
/// Requires the borrowed coordinate array through part 2 and a Q12 flinch
/// matrix. Changes only part 2's nine rotation coefficients; leaves translation
/// and coordinate compose stamps intact. GTE rotation/vector state is overwritten.
static inline void _actor403600MultiplyPartFlinch(GfxCoord* coord, const MATRIX* matrix)
{
    gte_SetRotMatrix(&coord[2].coord.m[0][0]);
    gte_ldclmv(matrix);
    gte_rtir();
    gte_stclmv(&coord[2].coord.m[0][0]);

    gte_ldclmv(&matrix->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[2].coord.m[0][1]);

    gte_ldclmv(&matrix->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[2].coord.m[0][2]);
}

/// Applies the double's pitch flinch to model part 2 and relaxes it each frame.
///
/// Requires live double work and coordinates through 2, plus one scratch MATRIX.
/// Post-multiplies the Q12 basis; translation and compose stamps stay intact.
/// Pitch uses 4096 units per turn. Both ordered 32-unit halfword relaxation
/// tests run, so small positive values can pass through the second test too.
static __inline__ void _actor403600ApplyDoubleFlinch(Task* task)
{
    enum { ACTOR_403600_DOUBLE_FLINCH_DECAY = 32 };
    Actor403600Work* work;
    GfxCoord*        coord;
    MATRIX*          matrix;

    work = task->work;
    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    matrix = SCRATCH_STACK_CURSOR(MATRIX);
    coord  = task->extra.tmd->coords;
    RotMatrix(&work->flinchRot, matrix);
    _actor403600MultiplyPartFlinch(coord, matrix);
    if (work->flinchRot.vx != 0) {
        if (work->flinchRot.vx >= ACTOR_403600_DOUBLE_FLINCH_DECAY) {
            work->flinchRot.vx -= ACTOR_403600_DOUBLE_FLINCH_DECAY;
            if (work->flinchRot.vx <= 0) {
                work->flinchRot.vx = 0;
            }
        }
        if (work->flinchRot.vx <= ACTOR_403600_DOUBLE_FLINCH_DECAY) {
            work->flinchRot.vx += ACTOR_403600_DOUBLE_FLINCH_DECAY;
            if (work->flinchRot.vx >= 0) {
                work->flinchRot.vx = 0;
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Advances the summoned double and starts fading on defeat or lifetime expiry.
///
/// Requires a live double enemy/model/work. Pause only refreshes colour; hidden
/// control suppresses drawing and targeting. Running updates actions, movement,
/// contacts, animation and flinch, samples colour every ten frames and increases
/// chase speed every 42 frames up to 100 world units per frame. Fade starts at
/// distance 300 with a 60-frame kill countdown; the next task state releases it.
static void _actor403600UpdateDouble(Enemy* enemy, Task* task)
{
    enum { ACTOR_403600_DOUBLE_COLOR_REFRESH_FRAMES  = 10,
           ACTOR_403600_DOUBLE_ACCEL_INTERVAL_FRAMES = 42,
           ACTOR_403600_DOUBLE_MAX_CHASE_SPEED       = 100,
           ACTOR_403600_DOUBLE_INITIAL_FADE_DISTANCE = 300,
           ACTOR_403600_DOUBLE_FADE_FRAMES           = 60 };
    TmdObject*       object;
    Actor403600Work* work;

    object = task->extra.tmd;
    work   = task->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor403600UpdateColor(enemy, task);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            object->flags                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = (WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE);
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            _actor403600UpdateDoubleMode(task);
            _actor403600Move(task);
            _actor403600ProcessContacts(task);
            _actor403600UpdateAnimation(task, ARRAY_SIZE(work->rig.slots));
            _actor403600ApplyDoubleFlinch(task);
            work->worldCoord.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&work->worldCoord);
            if (++work->colorRefreshFrames >= ACTOR_403600_DOUBLE_COLOR_REFRESH_FRAMES) {
                work->colorRefreshFrames = 0;
                _actor403600UpdateColor(enemy, task);
            }
            if (++work->age % ACTOR_403600_DOUBLE_ACCEL_INTERVAL_FRAMES == 0) {
                if (++work->chaseSpeed >= ACTOR_403600_DOUBLE_MAX_CHASE_SPEED) {
                    work->chaseSpeed = ACTOR_403600_DOUBLE_MAX_CHASE_SPEED;
                }
            }
            if (work->age >= work->lifetime || work->defeated != 0) {
                enemy->node.state.parts.flags      = WORLD_TARGET_NOT_LOCKABLE;
                object->shading.screenFadeDistance = ACTOR_403600_DOUBLE_INITIAL_FADE_DISTANCE;
                task->killCountdown                = ACTOR_403600_DOUBLE_FADE_FRAMES;
                task->state++;
            }
            break;
    }
}

/// Handlers for states 0-2 of the task `func_actor_403600_80141BE0` dispatches,
/// indexed by `Task::state`. The state-0 handler sets the task up and
/// advances it.
static const EnemyTaskFuncTable3 D_actor_403600_801320A0 = { {
    _actor403600SpawnDouble,
    _actor403600UpdateDouble,
    _actor403600FadeDouble,
} };

/// Advances the double's entrance, chase and alternating swipe actions.
///
/// Requires live double work and player coordinates. Chase sets yaw and world-unit
/// speeds for the later movement step. It starts a swipe within 2100 units and a
/// quarter turn of the player; swipes chain within 2000 units and that same angle.
/// Animation frames gate attack pairing and sound. Does not move or age the task.
static void _actor403600UpdateDoubleAction(Task* task)
{
    // Starts one placed-enemy cue. cue/coord must have no side effects;
    // coord is read twice, and panOut/depthOut are s32/u32 output lvalues.
    // Captures task and soundKey, preserving byte pan and half-depth rounding.
#define ACTOR_403600_PLAY_DOUBLE_SOUND(cue, coord, panOut, depthOut)                                               \
    {                                                                                                              \
        soundKey   = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | (cue); \
        (panOut)   = (s8)worldCoordGetOriginAudioPan((coord));                                                     \
        (depthOut) = worldCoordGetOriginAudioDepth((coord));                                                       \
        sndEvtRequestScriptStart(soundKey, (panOut), (s32)((((depthOut) >> 31) + (depthOut)) << 23) >> 24);        \
    }

    enum {
        ACTOR_403600_SOUND_DOUBLE_SWIPE = 0x5416000D,
        ACTOR_403600_DOUBLE_CHASE_RANGE = 2100,
        ACTOR_403600_DOUBLE_CHAIN_RANGE = 2000
    };
    u32              playerRange;
    s32              playerBearing;
    s16              action;
    s32              soundKey;
    s32              followHeight;
    s32              playerY;
    s32              chaseBearingMagnitude;
    s32              firstSwipeBearingMagnitude;
    s32              secondSwipeBearingMagnitude;
    s32              firstSwipePan;
    s32              secondSwipePan;
    u32              firstSwipeDepth;
    u32              secondSwipeDepth;
    GfxCoord*        firstSwipeCoord;
    GfxCoord*        secondSwipeCoord;
    Actor403600Work* work;

    work   = task->work;
    action = work->action;
    switch (action) {
        case ACTOR_403600_DOUBLE_ACTION_WAIT:
            work->animId        = ACTOR_403600_ANIM_IDLE;
            work->forwardSpeed  = 0U;
            work->verticalSpeed = 0;
            if (work->phaseFrame >= 0x1E) {
                work->action     = ACTOR_403600_DOUBLE_ACTION_CHASE;
                work->phaseFrame = 0;
                return;
            }
        default:
            return;
        case ACTOR_403600_DOUBLE_ACTION_CHASE:
            _actor403600TurnYawToAim(task, 0);
            work->animId        = ACTOR_403600_ANIM_FLY;
            work->forwardSpeed  = work->chaseSpeed;
            playerY             = gPlayerStatus.coordMtx->t[1];
            followHeight        = work->worldCoord.coord.t[1] + 0x3E8;
            work->verticalSpeed = (s16)((playerY - followHeight) / 25);
            _actor403600MeasurePlayerRangeBearing(&work->worldCoord, &playerRange, &playerBearing);
            if (playerRange < (u32)(ACTOR_403600_DOUBLE_CHASE_RANGE + 1)) {
                chaseBearingMagnitude = playerBearing;
                if (chaseBearingMagnitude < 0) {
                    chaseBearingMagnitude = -chaseBearingMagnitude;
                }
                if (chaseBearingMagnitude < (ACTOR_TRANSFORM_ANGLE_TURN / 4)) {
                    work->animBlendFrames = 0;
                    work->forwardSpeed    = 0U;
                    work->phaseFrame      = 0;
                    work->action          = ACTOR_403600_DOUBLE_ACTION_SWIPE_B;
                    return;
                }
            }
            break;
        case ACTOR_403600_DOUBLE_ACTION_SWIPE_A:
            work->attackBody.key = damagePackAttackKey(&D_actor_403600_80150EB0, 0);
            work->animId         = ACTOR_403600_ANIM_MELEE_FIRST;
            work->forwardSpeed   = 0U;
            work->verticalSpeed  = 0;
            if (work->phaseFrame == 0xE) {
                firstSwipeCoord = &work->worldCoord;
                ACTOR_403600_PLAY_DOUBLE_SOUND(ACTOR_403600_SOUND_DOUBLE_SWIPE, firstSwipeCoord, firstSwipePan, firstSwipeDepth);
            }
            if (work->phaseFrame == 0x11) {
                work->attackBody.flags = work->attackBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->phaseFrame == 0x15) {
                work->attackBody.flags = work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->phaseFrame >= 0x1E) {
                work->forwardSpeed = 0U;
                work->phaseFrame   = 0;
                _actor403600MeasurePlayerRangeBearing(&work->worldCoord, &playerRange, &playerBearing);
                if (playerRange < (u32)(ACTOR_403600_DOUBLE_CHAIN_RANGE + 1)) {
                    firstSwipeBearingMagnitude = playerBearing;
                    if (firstSwipeBearingMagnitude < 0) {
                        firstSwipeBearingMagnitude = -firstSwipeBearingMagnitude;
                    }
                    if (firstSwipeBearingMagnitude < (ACTOR_TRANSFORM_ANGLE_TURN / 4)) {
                        work->action = ACTOR_403600_DOUBLE_ACTION_SWIPE_B;
                        return;
                    }
                }
                work->action = ACTOR_403600_DOUBLE_ACTION_WAIT;
                return;
            }
            break;
        case ACTOR_403600_DOUBLE_ACTION_SWIPE_B:
            work->attackBody.key = damagePackAttackKey(&D_actor_403600_80150EB0, 1);
            work->animId         = ACTOR_403600_ANIM_MELEE_SECOND;
            work->forwardSpeed   = 0U;
            work->verticalSpeed  = 0;
            if (work->phaseFrame == 9) {
                secondSwipeCoord = &work->worldCoord;
                ACTOR_403600_PLAY_DOUBLE_SOUND(ACTOR_403600_SOUND_DOUBLE_SWIPE, secondSwipeCoord, secondSwipePan, secondSwipeDepth);
            }
            if (work->phaseFrame == 0xA) {
                work->attackBody.flags = work->attackBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->phaseFrame == 0xE) {
                work->attackBody.flags = work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->phaseFrame >= 0x23) {
                work->animBlendFrames = ACTOR_403600_DEFAULT_BLEND_FRAMES;
                work->forwardSpeed    = 0U;
                work->phaseFrame      = 0;
                _actor403600MeasurePlayerRangeBearing(&work->worldCoord, &playerRange, &playerBearing);
                if (playerRange < (u32)(ACTOR_403600_DOUBLE_CHAIN_RANGE + 1)) {
                    secondSwipeBearingMagnitude = playerBearing;
                    if (secondSwipeBearingMagnitude < 0) {
                        secondSwipeBearingMagnitude = -secondSwipeBearingMagnitude;
                    }
                    if (secondSwipeBearingMagnitude < (ACTOR_TRANSFORM_ANGLE_TURN / 4)) {
                        work->action = ACTOR_403600_DOUBLE_ACTION_SWIPE_A;
                        return;
                    }
                }
                work->action = ACTOR_403600_DOUBLE_ACTION_WAIT;
                return;
            }
            break;
        case ACTOR_403600_DOUBLE_ACTION_APPEAR:
            work->animId        = ACTOR_403600_ANIM_DOUBLE_APPEAR;
            work->forwardSpeed  = 0U;
            work->verticalSpeed = 0;
            if (work->phaseFrame >= 0x46) {
                work->action          = ACTOR_403600_DOUBLE_ACTION_CHASE;
                work->phaseFrame      = 0;
                work->animBlendFrames = ACTOR_403600_DEFAULT_BLEND_FRAMES;
            }
            break;
    }

#undef ACTOR_403600_PLAY_DOUBLE_SOUND
}

/// Unlinks the fading double's borrowed enemy/model state and releases its task.
///
/// The caller has released its battle reference and cleared the boss's child
/// pointer. Detaches the root before freeing its work-owned parent coordinate.
static inline void _actor403600ReleaseFadedDouble(Task* task)
{
    Enemy*           enemy;
    Actor403600Work* work;
    enemy                           = task->spawnArg2.pointer;
    work                            = task->work;
    task->extra.tmd->coords->parent = &gGfxViewCoord;
    enemy->recs                     = NULL;
    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->hitBody);
    worldCollisionUnlinkBody(&work->attackBody);
    if (task == D_actor_403600_801606A8) {
        worldCollisionUnlinkBody(&work->gridBody);
    }
    enemyTaskExit(task);
}

/// Fades the defeated or expired double, then releases its battle and task state.
///
/// Requires live singleton boss, double enemy/work and model. Pause holds the
/// fade until the boss is defeated; hidden mode suppresses drawing and targeting.
/// Adds three to screen-fade distance each running frame until killCountdown
/// expires, then clears the boss's child pointer and unlinks collision/target
/// state before enemyTaskExit releases storage. Animation continues during fade.
static void _actor403600FadeDouble(Enemy* enemy, Task* task)
{
    enum { ACTOR_403600_DOUBLE_FADE_STEP_DRAW    = 0,
           ACTOR_403600_DOUBLE_FADE_STEP_RELEASE = 1 };
    TmdObject*       object;
    Actor403600Work* work;
    Actor403600Work* bossWork;

    object   = task->extra.tmd;
    work     = task->work;
    bossWork = D_actor_403600_801606A8->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (bossWork->defeated != 1) {
                return;
            }
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            object->flags                |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            break;
    }
    switch (work->step) {
        case ACTOR_403600_DOUBLE_FADE_STEP_DRAW:
            object->shading.screenFadeDistance += 3;
            task->extra.tmd->flags              = 0;
            if (--task->killCountdown <= 0) {
                work->step        = ACTOR_403600_DOUBLE_FADE_STEP_RELEASE;
                work->actionParam = 0;
            }
            break;
        case ACTOR_403600_DOUBLE_FADE_STEP_RELEASE:
            // The battle API ignores its second argument; rewards come from enemy params.
            sceneReleaseBattleRefWithRewards(task, 0x24);
            bossWork->childEnemy = NULL;
            _actor403600ReleaseFadedDouble(task);
            return;
    }
    _actor403600UpdateAnimation(task, ARRAY_SIZE(work->rig.slots));
}

/// Applies a scene selector to the boss or its borrowed scene figure.
///
/// Handles ACTOR_COMMAND_MESSAGE_APPLY; request is read only during synchronous
/// dispatch and only its command is read. Returns zero even for an unknown
/// selector. Requires live enemy/work/model; the fade selector additionally
/// requires the previously spawned child. Placements use room units and Euler
/// angles in 4096 units per turn. The message ID and second payload are unused.
static s32 _actor403600ApplySceneCommand(Task* task, s32 unusedMessageId, const ActorCommand* request, s32 unusedSecondArg)
{
    SVECTOR          angles;
    u16              selector;
    Enemy*           enemy;
    Actor403600Work* work;
    Actor403600Work* childWork;
    TmdObject*       childObject;

    selector = request->command;
    work     = task->work;
    enemy    = task->spawnArg2.pointer;
    switch (selector) {
        case ACTOR_403600_COMMAND_POSE_PLAYER_AND_BOSS:
            _actor403600ResetState(task);
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            worldTargetDisableNodeLockOn(&enemy->node);
            work->animId                = ACTOR_403600_ANIM_IDLE;
            work->worldCoord.coord.t[0] = 0x1D7A;
            work->worldCoord.coord.t[1] = -0x145A;
            work->worldCoord.coord.t[2] = 0x19AE;
            work->mode                  = ACTOR_403600_MODE_SCENE_POSE;
            work->appliedAnimId         = 0;
            angles.vx                   = 0;
            angles.vy                   = 0x200;
            angles.vz                   = 0;
            RotMatrix(&angles, &work->worldCoord.coord);
            D_actor_403600_801606E0.placement.rot.vx = 0;
            D_actor_403600_801606E0.placement.rot.vy = -0x600;
            D_actor_403600_801606E0.placement.rot.vz = 0;
            D_actor_403600_801606E0.placement.pos.vx = 0x1E8D;
            D_actor_403600_801606E0.placement.pos.vy = -0xF9F;
            D_actor_403600_801606E0.placement.pos.vz = 0x1AC6;
            TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_PLACE, &D_actor_403600_801606E0.placement, 0);
            D_actor_403600_80160568.animationId = ACTOR_403600_PLAYER_ANIM_SCENE_POSE;
            TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
            break;
        case ACTOR_403600_COMMAND_PLAY_SCENE_ANIMATIONS:
            work->animId                        = ACTOR_403600_ANIM_SCENE_PAIRED;
            work->appliedAnimId                 = 0;
            D_actor_403600_80160568.animationId = ACTOR_403600_PLAYER_ANIM_SCENE_PAIRED;
            TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
            break;
        case ACTOR_403600_COMMAND_BRIGHTEN_BOSS:
            work->ambientBoost = 0x3E8;
            work->mode         = ACTOR_403600_MODE_SCENE_BRIGHTEN;
            break;
        case ACTOR_403600_COMMAND_SPAWN_SCENE_FIGURE:
            // The figure and shaft controller take over presentation from both actors.
            D_actor_403600_801606B0 = taskSpawnFromTable(D_actor_303600_8016E468, 0, 0, 0);
            taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
            task->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            task->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->childEnemy        = enemySpawnFromTable(D_actor_403600_80160514, 2, 0, 0);
            break;
        case ACTOR_403600_COMMAND_FADE_SCENE_FIGURE:
            D_actor_403600_801606E0.placement.rot.vx = 0;
            D_actor_403600_801606E0.placement.rot.vy = 0;
            D_actor_403600_801606E0.placement.rot.vz = 0;
            D_actor_403600_801606E0.placement.pos.vx = 0;
            D_actor_403600_801606E0.placement.pos.vy = 0;
            D_actor_403600_801606E0.placement.pos.vz = 0;
            TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_PLACE, &D_actor_403600_801606E0.placement, 0);
            D_actor_403600_80160568.animationId = ACTOR_403600_PLAYER_ANIM_FIGURE_FADE;
            TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
            childWork                               = work->childEnemy->task->work;
            childObject                             = work->childEnemy->task->extra.tmd;
            childWork->mode                         = ACTOR_403600_MODE_SCENE_FADE;
            childObject->shading.screenFadeDistance = 0;
            break;
        case ACTOR_403600_COMMAND_ASCEND_TO_ARENA:
            work->forwardSpeed          = ACTOR_403600_SCENE_ASCENT_BACKWARD_SPEED;
            work->mode                  = ACTOR_403600_MODE_SCENE_ASCEND;
            work->animId                = ACTOR_403600_ANIM_IDLE;
            work->worldCoord.coord.t[0] = 0x196E;
            work->worldCoord.coord.t[1] = -0x7D0;
            work->worldCoord.coord.t[2] = 0x1630;
            work->appliedAnimId         = 0;
            angles.vx                   = 0;
            angles.vy                   = 0x200;
            angles.vz                   = 0;
            RotMatrix(&angles, &work->worldCoord.coord);
            work->worldCoord.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&work->worldCoord);
            tmdAllocPrimitiveBuffer(task->extra.tmd);
            task->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            task->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->step              = 0;
            break;
        case ACTOR_403600_COMMAND_START_FIGHT:
            _actor403600ResetState(task);
            work->mode                    = ACTOR_403600_MODE_FIGHT;
            work->step                    = 0;
            enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
            work->animId                  = ACTOR_403600_ANIM_IDLE;
            work->worldCoord.coord.t[0]   = 0x196E;
            work->worldCoord.coord.t[1]   = -0x1B62;
            work->worldCoord.coord.t[2]   = 0x1630;
            work->appliedAnimId           = 0;
            angles.vx                     = 0;
            angles.vy                     = 0x200;
            angles.vz                     = 0;
            RotMatrix(&angles, &work->worldCoord.coord);
            tmdAllocPrimitiveBuffer(task->extra.tmd);
            task->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            task->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case ACTOR_403600_COMMAND_HIDE_BOSS:
            work->mode                    = ACTOR_403600_MODE_PARKED;
            task->extra.tmd->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            task->extra.tmd->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case ACTOR_403600_COMMAND_FINISH_BATTLE:
            // Rewards come from enemy parameters; the release API ignores argument 2.
            sceneReleaseBattleRefWithRewards(task, 0x24);
            gGameSession->flowFlags                        = (u8)(gGameSession->flowFlags | GAME_SESSION_FLOW_REEQUIP_WEAPON);
            gSceneCombatState.signals.bytes.endDelayFrames = ACTOR_403600_BATTLE_END_DELAY_FRAMES;
            break;
    }
    return 0;
}

static void func_actor_403600_80140B4C(Enemy* enemy, Task* actor)
{
    SVECTOR             offset;
    GfxCoord            view;
    ActorCommand        forwardCommand;
    ActorCommand        reverseCommand;
    Actor303600ViewKey* key;
    s32                 i;
    s32                 transparency;
    TmdObject*          object;
    Actor403600Work*    work;

    work        = actor->work;
    object      = actor->extra.tmd;
    view.parent = &gGfxViewCoord;
    gfxSetRotIdentity(&view.coord);
    view.coord.t[0]            = 0;
    view.coord.t[1]            = 0;
    view.coord.t[2]            = 0;
    D_actor_403600_8016065C.vz = D_actor_303600_8016A408[work->sceneFrame].z;
    D_actor_403600_8016065C.vy = D_actor_303600_8016A408[work->sceneFrame].y;
    RotMatrix(&D_actor_403600_8016065C, &work->worldCoord.coord);
    object->otOffset = -0x1F;
    if (work->sceneFrame >= ACTOR_303600_VIEW_KEY_COUNT - 1) {
        key = &D_actor_303600_8016AEF8[ACTOR_303600_VIEW_KEY_COUNT - 1];
    } else {
        key = &D_actor_303600_8016AEF8[work->sceneFrame];
    }
    // The compact key stores all nine rotation coefficients in row-major order.
    for (i = 0; i < (s32)ARRAY_SIZE(key->rotation); i++) {
        ((s16(*)[9])D_actor_403600_80160700.transform.m)[0][i] = key->rotation[i];
    }
    for (i = 0; i < (s32)ARRAY_SIZE(key->translation); i++) {
        D_actor_403600_80160700.transform.t[i] = key->translation[i];
    }
    D_actor_403600_80160700.screenDistance = 0x149;
    viewQueueCamera(&D_actor_403600_80160700);
    _actor403600ScaleSceneFigure(&work->worldCoord, work->hitCooldown);
    work->sceneFrame++;
    if (work->sceneFrame >= ACTOR_303600_ROT_SAMPLE_COUNT) {
        work->sceneFrame = ACTOR_303600_ROT_SAMPLE_COUNT - 1;
    }
    if (work->mode != ACTOR_403600_MODE_SCENE_FADE) {
        if (work->sceneFrame == 1) {
            forwardCommand.context.loc.stage = 4;
            forwardCommand.context.loc.area  = 0x16;
            forwardCommand.command           = ACTOR_303600_SHAFT_COMMAND_SCROLL_FORWARD;
            TASK_MESSAGE_DISPATCH_POINTER(D_actor_403600_801606B0, ACTOR_COMMAND_MESSAGE_APPLY, &forwardCommand, 0);
            gDisplayState.screenDistance = 0x149;
            gte_SetGeomScreen(gDisplayState.screenDistance);
            gte_SetGeomOffset(0, 0);
        }
        if (work->sceneFrame >= 0x100) {
            work->hitCooldown += 0x20;
            if (work->hitCooldown >= 0x1200) {
                work->hitCooldown = 0x1200;
            }
            work->ambientBoost -= 0x2D;
        }
        if (work->phaseFrame >= 0x32 && work->phaseFrame < 0x191) {
            s16 angle;
            s16 radius;
            s32 x;

            angle     = _actor403600Rand() & 0xF80;
            radius    = (_actor403600Rand() & 0xF00) + 0x200;
            x         = radius * rcos(angle);
            offset.vy = -0x1800;
            offset.vx = x >> 12;
            offset.vz = (radius * rsin(angle)) >> 12;
            effectSpawn(EFFECT_EVE_LIGHT_BEAM, &view, 0x300, &offset);
        }
        if (work->phaseFrame == 0x15E) {
            reverseCommand.context.loc.stage = 4;
            reverseCommand.context.loc.area  = 0x16;
            reverseCommand.command           = ACTOR_303600_SHAFT_COMMAND_REVERSE_SCROLL;
            TASK_MESSAGE_DISPATCH_POINTER(D_actor_403600_801606B0, ACTOR_COMMAND_MESSAGE_APPLY, &reverseCommand, 0);
        }
    } else {
        if (work->phaseFrame >= 0x258) {
            object->shading.screenFadeDistance += 3;
            if (object->shading.screenFadeDistance >= 0x259) {
                object->shading.screenFadeDistance = 0x258;
            }
            if (work->phaseFrame < 0x2EF && (work->phaseFrame & 2)) {
                if (_actor403600Rand() & 1) {
                    offset.vx = _actor403600Rand() & 0x7FF;
                    offset.vy = _actor403600Rand() & 0x7FF;
                } else {
                    offset.vx = -(_actor403600Rand() & 0x7FF);
                    offset.vy = -(_actor403600Rand() & 0x7FF);
                }
                effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_RISING_SPRITE, &work->worldCoord, 0x10800, &offset);
            }
        }
        if (work->phaseFrame == 0x2A8) {
            taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
        }
        if (work->phaseFrame >= 0x2A8 && work->phaseFrame < 0x385 && (work->phaseFrame & 3) == 3) {
            effectSpawn(EFFECT_SHELTER_B2_POD_BOTTOM_RISING_SPRITE, &work->worldCoord, 0x10800, NULL);
        }
        D_actor_403600_801606E0.placement.rot.vx  = 0;
        D_actor_403600_801606E0.placement.rot.vz  = 0;
        D_actor_403600_801606E0.placement.pos.vx  = -0x1F4;
        D_actor_403600_801606E0.placement.pos.vy  = 0x3E8;
        D_actor_403600_801606E0.placement.pos.vz  = -0x1F4;
        D_actor_403600_801606E0.placement.rot.vy += 0x38;
        TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_PLACE, &D_actor_403600_801606E0.placement, 0);
        if (work->phaseFrame >= 0x2BC && work->phaseFrame < 0x385) {
            s16 angle;
            s16 radius;
            s32 x;

            angle     = _actor403600Rand() & 0xF80;
            radius    = (_actor403600Rand() & 0xF00) + 0x200;
            x         = radius * rcos(angle);
            offset.vy = 0x1800;
            offset.vx = x >> 12;
            offset.vz = (radius * rsin(angle)) >> 12;
            effectSpawn(EFFECT_EVE_LIGHT_BEAM, &view, -0x300, &offset);
        }
    }
    work->worldCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->worldCoord);
    _actor403600UpdateColor(enemy, actor);
    transparency = work->ambientBoost;
    if (transparency != 0) {
        worldCoordSetModelAmbientColor(actor->extra.tmd, transparency, transparency, transparency);
    }
    work->phaseFrame++;
}

/// The actor's task entry: runs the handler for `task->state` from a two-entry
/// table built on the stack, passing the enemy the task was spawned for and
/// the task. State 0 is the spawn (`_actor403600SpawnBoss`, which
/// advances the state), state 1 the per-frame update.
void func_actor_403600_80141180(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        _actor403600SpawnBoss,
        _actor403600UpdateBoss,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Advances the boss model's animation through the package's playback helper.
///
/// partCount is an exclusive rig-slot bound, at most 20; the caller supplies 20
/// to advance parts 1..19. Requires a live rig and model.
static void _actor403600TickBossAnimation(Task* task, u8 partCount)
{
    _actor403600UpdateAnimation(task, partCount);
}

/// Samples the boss's lighting at its cached composed world translation.
///
/// Requires live enemy/work and worldCoord.workm. Borrows a scratch VECTOR for
/// the colour query, with both options zero, and releases it before returning.
/// Does not compose the coordinate; no pointer survives the query.
static void _actor403600SampleBossColor(Enemy* enemy, Task* task)
{
    Actor403600Work* work;

    VECTOR* position;

    work                         = task->work;
    position                     = SCRATCH_STACK_CURSOR(VECTOR) - 1;
    position->vx                 = work->worldCoord.workm.t[0];
    position->vy                 = work->worldCoord.workm.t[1];
    SCRATCH_STACK_CURSOR(VECTOR) = position;
    position->vz                 = work->worldCoord.workm.t[2];
    worldCoordUpdateActorColor(enemy, position, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(*position));
}

/// Applies the boss's pitch flinch to model part 2 and steps the flinch toward zero.
///
/// Requires live work and model coordinates through 2. Post-multiplies part 2's
/// Q12 basis by flinchRot using GTE registers and one scratch MATRIX. Pitch uses
/// 4096 units per turn and decays by 32 per frame; the two ordered tests and
/// halfword arithmetic are retained. Translation and compose stamps are untouched.
static void _actor403600ApplyBossFlinch(Task* task)
{
    enum { ACTOR_403600_FLINCH_DECAY = 32 };
    Actor403600Work* work;
    GfxCoord*        coord;
    MATRIX*          matrix;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    matrix = SCRATCH_STACK_CURSOR(MATRIX);
    work   = task->work;
    coord  = task->extra.tmd->coords;
    RotMatrix(&work->flinchRot, matrix);

    _actor403600MultiplyPartFlinch(coord, matrix);

    if (work->flinchRot.vx != 0) {
        if (work->flinchRot.vx >= ACTOR_403600_FLINCH_DECAY) {
            work->flinchRot.vx -= ACTOR_403600_FLINCH_DECAY;
            if (work->flinchRot.vx <= 0) {
                work->flinchRot.vx = 0;
            }
        }
        if (work->flinchRot.vx <= ACTOR_403600_FLINCH_DECAY) {
            work->flinchRot.vx += ACTOR_403600_FLINCH_DECAY;
            if (work->flinchRot.vx >= 0) {
                work->flinchRot.vx = 0;
            }
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Advances the boss's vertical screen shake and clears it when its timer expires.
///
/// shakeFadeFrames enables the effect and selects the final half-strength frames.
/// The display animation frame supplies a sine phase (512 angle units per frame),
/// yielding about +/-4 pixels, then +/-2. Retains halfword countdown wrapping.
static void _actor403600UpdateScreenShake(Task* task)
{
    Actor403600Work* work;
    s16              fadeFrames;
    s16              remainingFrames;
    s32              shakeWave;

    work       = task->work;
    fadeFrames = work->shakeFadeFrames;
    if (fadeFrames != 0) {
        if (fadeFrames < work->shakeFrames) {
            shakeWave = rsin(gDisplayState.animFrame << 9) << 0xD;
        } else {
            shakeWave = rsin(gDisplayState.animFrame << 9) << 0xC;
        }
        displaySetShakeY(shakeWave >> 0x18);
        remainingFrames   = (u16)work->shakeFrames - 1;
        work->shakeFrames = remainingFrames;
        if ((remainingFrames << 0x10) <= 0) {
            work->shakeFadeFrames = 0;
            displaySetShakeY(0);
        }
    }
}

/// Unlinks collision and targeting state before releasing a boss or double task.
///
/// Requires a live enemy in spawnArg2 and Actor403600Work. Detaches the model
/// root from work-owned coordinates, clears the enemy's borrowed hit records,
/// unlinks its target and hit/attack bodies, and also unlinks the grid body for
/// the singleton boss. enemyTaskExit performs the final resource release.
static void _actor403600EnemyExit(Task* task)
{
    Actor403600Work* work;
    Enemy*           enemy;

    enemy                           = task->spawnArg2.pointer;
    work                            = task->work;
    task->extra.tmd->coords->parent = &gGfxViewCoord;
    enemy->recs                     = NULL;
    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->hitBody);
    worldCollisionUnlinkBody(&work->attackBody);
    if (task == D_actor_403600_801606A8) {
        worldCollisionUnlinkBody(&work->gridBody);
    }
    enemyTaskExit(task);
}

/// Advances weakened-phase puffs, hovering and the conditional recovery transition.
///
/// Requires live boss enemy/work and model part 1. Every fourth weakened frame
/// spawns a puff unless rush is repositioning, then weakFrames wraps as a u16 and the
/// boss bobs vertically. Recovery starts at 901 frames while choosing an attack
/// and only once current HP exceeds a tenth of its initial maximum.
static void _actor403600UpdateWeakPhase(Task* task)
{
    enum { ACTOR_403600_WEAK_RECOVERY_FRAMES     = 901,
           ACTOR_403600_WEAK_PUFF_SKIP_RUSH_STEP = 5 };
    Actor403600Work* work;
    Enemy*           enemy;
    u16*             weakFrames;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->weakPhase == ACTOR_403600_WEAK_PHASE_ACTIVE) {
        if (((work->weakFrames & 3) == 3) &&
            ((work->action != ACTOR_403600_ACTION_RUSH) || (work->step != ACTOR_403600_WEAK_PUFF_SKIP_RUSH_STEP))) {
            effectSpawn(EFFECT_HIT_PUFF, task->extra.tmd->coords + 1, ACTOR_403600_WEAK_PUFF_ARG, NULL);
        }
        /* Stored through a plain halfword pointer: as a structure store it
         * makes the compiler read `gDisplayState.animFrame` again after it. */
        weakFrames                   = &work->weakFrames;
        *weakFrames                  = (u16)(work->weakFrames + 1);
        work->worldCoord.coord.t[1] += rsin(gDisplayState.animFrame << 8) >> 6;
        if ((work->hpMax / 10 < enemy->hp) &&
            (work->weakFrames >= ACTOR_403600_WEAK_RECOVERY_FRAMES) && (work->action == ACTOR_403600_ACTION_CHOOSE)) {
            work->mode = ACTOR_403600_MODE_RECOVER;
        }
    }
}

/// Restores combat action and movement defaults when the boss is created.
///
/// Requires live Actor403600Work. Uses the same reset as later combat transitions;
/// leaves mode, animation selection, HP, target and weakened-phase state intact.
static void _actor403600ResetCombatState(Task* task)
{
    _actor403600ResetState(task);
}

/// Advances the boss's roll and adds it to its current Euler rotation.
///
/// Only rollStep's low byte is used, in angle units (4096 per turn). The signed
/// halfword sum retains the original fold: below -2048 it becomes 4096 minus
/// the sum. Rebuilds the local rotation after adding the accumulated roll to
/// its extracted Z angle, retaining translation and leaving the cache untouched.
static void _actor403600AdvanceRoll(Task* task, s32 rollStep)
{
    SVECTOR          rotation;
    Actor403600Work* work;
    s16              angle;

    work       = task->work;
    angle      = work->roll + (rollStep & 0xFF);
    work->roll = angle;
    if (ABS(angle) > ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        if (angle > 0) {
            work->roll = angle - ACTOR_TRANSFORM_ANGLE_TURN;
        } else {
            work->roll = ACTOR_TRANSFORM_ANGLE_TURN - angle;
        }
    }
    gfxMatrixToEuler(&work->worldCoord.coord, &rotation);
    rotation.vz += work->roll;
    RotMatrix(&rotation, &work->worldCoord.coord);
}

/// Steers toward targetPos and reports how many coordinate axes are near it.
///
/// Requires live work with prevPos saved by the previous movement step. Sets
/// player-independent target aim, yaw step 160 and forward speed 300 world
/// units per frame. Each axis within 500 units restores its prevPos component;
/// Y otherwise takes vertical speed +/-300, with a bob while ascending.
/// Returns 0..3 settled axes, not a mask; all three finishes positioning.
static s32 _actor403600ApproachTarget(Task* task)
{
    enum {
        ACTOR_403600_TARGET_APPROACH_TURN  = 160,
        ACTOR_403600_TARGET_APPROACH_SPEED = 300,
        ACTOR_403600_TARGET_AXIS_TOLERANCE = 500,
    };
    Actor403600Work* work;
    s32              settledAxes;
    s32              deltaX;
    s32              deltaY;
    s32              deltaZ;
    s32              targetY;
    s32              currentY;

    settledAxes   = 0;
    work          = task->work;
    work->aimMode = ACTOR_403600_AIM_TARGET;
    _actor403600TurnYawToAim(task, ACTOR_403600_TARGET_APPROACH_TURN);

    // Roll back only settled axes; the later movement step uses the new speeds.
    work->forwardSpeed = ACTOR_403600_TARGET_APPROACH_SPEED;
    deltaX             = work->targetPos.vx - work->worldCoord.coord.t[0];
    if (ABS(deltaX) < (ACTOR_403600_TARGET_AXIS_TOLERANCE + 1)) {
        settledAxes                 = 1;
        work->worldCoord.coord.t[0] = work->prevPos.vx;
    }

    targetY  = work->targetPos.vy;
    currentY = work->worldCoord.coord.t[1];
    deltaY   = targetY - currentY;
    if (ABS(deltaY) < (ACTOR_403600_TARGET_AXIS_TOLERANCE + 1)) {
        settledAxes                += 1;
        work->worldCoord.coord.t[1] = work->prevPos.vy;
    } else if (targetY < currentY) {
        work->verticalSpeed          = -ACTOR_403600_TARGET_APPROACH_SPEED;
        work->worldCoord.coord.t[1] += rsin(gDisplayState.animFrame << 8) >> 6;
    } else {
        work->verticalSpeed = ACTOR_403600_TARGET_APPROACH_SPEED;
    }

    deltaZ = work->targetPos.vz - work->worldCoord.coord.t[2];
    if (ABS(deltaZ) < (ACTOR_403600_TARGET_AXIS_TOLERANCE + 1)) {
        work->worldCoord.coord.t[2] = work->prevPos.vz;
        settledAxes                += 1;
    }
    return settledAxes & 0xFF;
}

/// Installs the boss's weakened or normal texture and palette blocks in VRAM.
///
/// Exactly 1 selects the weakened appearance; any other value restores normal.
/// Copies a 128-word by 128-row block to (384,384), then a 256-word palette to
/// row 249 at X 0. Requires the package's source image already uploaded.
static void _actor403600SetWeakTextures(s32 weakAppearance)
{
    enum {
        ACTOR_403600_TEXTURE_BLOCK_WORDS = 128,
        ACTOR_403600_TEXTURE_BLOCK_ROWS  = 128,
        ACTOR_403600_TEXTURE_SOURCE_Y    = 128,
        ACTOR_403600_WEAK_TEXTURE_X      = 384,
        ACTOR_403600_NORMAL_TEXTURE_X    = 448,
        ACTOR_403600_TEXTURE_DEST_X      = 384,
        ACTOR_403600_TEXTURE_DEST_Y      = 384,
        ACTOR_403600_PALETTE_WORDS       = 256,
        ACTOR_403600_WEAK_PALETTE_ROW    = 253,
        ACTOR_403600_NORMAL_PALETTE_ROW  = 254,
        ACTOR_403600_PALETTE_DEST_ROW    = 249
    };
    RECT rect;

    rect.y = ACTOR_403600_TEXTURE_SOURCE_Y;
    rect.h = ACTOR_403600_TEXTURE_BLOCK_ROWS;
    rect.w = ACTOR_403600_TEXTURE_BLOCK_WORDS;
    if (weakAppearance == 1) {
        rect.x = ACTOR_403600_WEAK_TEXTURE_X;
    } else {
        rect.x = ACTOR_403600_NORMAL_TEXTURE_X;
    }
    MoveImage(&rect, ACTOR_403600_TEXTURE_DEST_X, ACTOR_403600_TEXTURE_DEST_Y);
    rect.w = ACTOR_403600_PALETTE_WORDS;
    rect.h = 1;
    rect.x = 0;
    if (weakAppearance == 1) {
        rect.y = ACTOR_403600_WEAK_PALETTE_ROW;
    } else {
        rect.y = ACTOR_403600_NORMAL_PALETTE_ROW;
    }
    MoveImage(&rect, 0, ACTOR_403600_PALETTE_DEST_ROW);
}

/// Binds a shed boss-part model to the weakened appearance's texture placement.
///
/// Sets texture-page offset -15 and palette-row offset 2 on the task's live
/// TMD object. Rebuilds both buffer halves when a primitive buffer exists;
/// allocates no buffer and borrows the model.
static void _actor403600BindShedPartTextures(Task* task)
{
    enum { ACTOR_403600_SHED_TEXTURE_PAGE_OFFSET = -15,
           ACTOR_403600_SHED_CLUT_ROW_OFFSET     = 2 };
    TmdObject* model;

    model                    = task->extra.tmd;
    model->texturePageOffset = ACTOR_403600_SHED_TEXTURE_PAGE_OFFSET;
    model->clutRowOffset     = ACTOR_403600_SHED_CLUT_ROW_OFFSET;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
}

/// Updates the two face texture patches when the boss's committed flag changes.
///
/// Exactly 1 selects the active-flight face; other values restore the normal face.
/// Requires source images in VRAM. Copies a 21-word by 10-row patch to (321,338)
/// and a 23-word by 21-row patch to (321,356), then remembers the current flag.
static void _actor403600UpdateFaceTexture(Task* task)
{
    enum {
        ACTOR_403600_FACE_UPPER_WIDTH       = 21,
        ACTOR_403600_FACE_UPPER_ROWS        = 10,
        ACTOR_403600_FACE_LOWER_WIDTH       = 23,
        ACTOR_403600_FACE_DEST_X            = 321,
        ACTOR_403600_FACE_UPPER_DEST_Y      = 338,
        ACTOR_403600_FACE_LOWER_DEST_Y      = 356,
        ACTOR_403600_FACE_COMMITTED_UPPER_X = 354,
        ACTOR_403600_FACE_COMMITTED_UPPER_Y = 354,
        ACTOR_403600_FACE_COMMITTED_LOWER_X = 367,
        ACTOR_403600_FACE_COMMITTED_LOWER_Y = 258,
        ACTOR_403600_FACE_NORMAL_UPPER_X    = 321,
        ACTOR_403600_FACE_NORMAL_UPPER_Y    = 499,
        ACTOR_403600_FACE_NORMAL_LOWER_X    = 383,
        ACTOR_403600_FACE_NORMAL_LOWER_Y    = 427
    };
    RECT             rect;
    Actor403600Work* work;
    s16              committed;

    work      = task->work;
    committed = work->committed;
    if (work->appliedFace != committed) {
        if (committed == 1) {
            s16 upperWidth;

            rect.x     = ACTOR_403600_FACE_COMMITTED_UPPER_X;
            rect.y     = ACTOR_403600_FACE_COMMITTED_UPPER_Y;
            upperWidth = ACTOR_403600_FACE_UPPER_WIDTH;
            rect.w     = upperWidth;
            rect.h     = ACTOR_403600_FACE_UPPER_ROWS;
            MoveImage(&rect, ACTOR_403600_FACE_DEST_X, ACTOR_403600_FACE_UPPER_DEST_Y);
            rect.x = ACTOR_403600_FACE_COMMITTED_LOWER_X;
            rect.y = ACTOR_403600_FACE_COMMITTED_LOWER_Y;
            rect.w = ACTOR_403600_FACE_LOWER_WIDTH;
            rect.h = upperWidth;
            MoveImage(&rect, ACTOR_403600_FACE_DEST_X, ACTOR_403600_FACE_LOWER_DEST_Y);
        } else {
            s16 upperWidth;

            rect.x     = ACTOR_403600_FACE_NORMAL_UPPER_X;
            rect.y     = ACTOR_403600_FACE_NORMAL_UPPER_Y;
            upperWidth = ACTOR_403600_FACE_UPPER_WIDTH;
            rect.w     = upperWidth;
            rect.h     = ACTOR_403600_FACE_UPPER_ROWS;
            MoveImage(&rect, ACTOR_403600_FACE_DEST_X, ACTOR_403600_FACE_UPPER_DEST_Y);
            rect.x = ACTOR_403600_FACE_NORMAL_LOWER_X;
            rect.y = ACTOR_403600_FACE_NORMAL_LOWER_Y;
            rect.w = ACTOR_403600_FACE_LOWER_WIDTH;
            rect.h = upperWidth;
            MoveImage(&rect, ACTOR_403600_FACE_DEST_X, ACTOR_403600_FACE_LOWER_DEST_Y);
        }
        work->appliedFace = (u16)work->committed;
    }
}

/// Cancels the boss's drain presentation and clears its screen distortion.
///
/// Requires a live task with Actor403600Work. Halts scripted input and requests
/// the drain wind-up sound to stop while keeping its release behavior. Does not
/// change the combat action, accumulated damage or the player's MP.
static void _actor403600CancelDrain(Task* task)
{
    Actor403600Work* work = task->work;

    padScriptHalt();
    sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_DRAIN_WINDUP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    work->screenDistortion = 0;
}

/// Raises the player's MP by one when the drain attack's countdown expires.
///
/// Clamps at mpMax. Reloads the countdown from the decremented actionCounter,
/// so delays shorten until MP rises every frame. Retains signed-halfword tests
/// of the wrapping timers and MP addition. The later drain strike removes MP;
/// this tick changes no HP and requires a live Actor403600Work.
static void _actor403600RaisePlayerMpForDrain(Task* task)
{
    s16              nextCountdown;
    u16              countdown;
    u16              nextMp;
    Actor403600Work* work;
    PlayerStatus*    playerStatus;

    work              = task->work;
    countdown         = (u16)work->actionTimer - 1;
    work->actionTimer = countdown;
    if ((s16)countdown <= 0) {
        playerStatus     = &gPlayerStatus;
        nextMp           = playerStatus->mp + 1;
        playerStatus->mp = nextMp;
        if ((s16)nextMp >= playerStatus->mpMax) {
            playerStatus->mp = playerStatus->mpMax;
        }
        if (work->actionCounter <= 0) {
            work->actionTimer = 1;
            return;
        }
        nextCountdown       = (u16)work->actionCounter - 1;
        work->actionCounter = nextCountdown;
        work->actionTimer   = nextCountdown;
    }
}

void func_actor_403600_80141BE0(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_403600_801320A0;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Runs double combat actions only in the parked or fight mode.
///
/// Negative modes and modes at or above stagger leave its action untouched.
static void _actor403600UpdateDoubleMode(Task* task)
{
    Actor403600Work* work;
    s16              mode;

    work = task->work;
    mode = work->mode;
    if (mode < 0) {
        return;
    }
    if (mode < ACTOR_403600_MODE_STAGGER) {
        _actor403600UpdateDoubleAction(task);
    }
}

/// Subtracts hit damage from the summoned double and marks it defeated at zero HP.
///
/// Reports the HP loss to its target readout. Requires a live task, enemy and
/// Actor403600Work; defeat is consumed by the double's update to start fading.
/// Retains the unsigned-halfword HP read and signed-halfword store. Releases
/// no resources and does not run the boss's death sequence.
static void _actor403600ApplyDoubleDamage(Task* task, s32 damage)
{
    Enemy*           enemy;
    Actor403600Work* work;

    enemy     = task->spawnArg2.pointer;
    work      = task->work;
    enemy->hp = (u16)enemy->hp - damage;
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    if (enemy->hp <= 0) {
        work->defeated = 1;
    }
}

/// Handlers for states 0-2 of the task `func_actor_403600_80141CD4` dispatches,
/// indexed by `Task::state`. The state-0 handler sets the task up and
/// advances it.
static const EnemyTaskFuncTable3 D_actor_403600_801320EC = { {
    func_actor_403600_80141D30,
    _actor403600WaitSceneFigure,
    func_actor_403600_80140B4C,
} };

void func_actor_403600_80141CD4(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_403600_801320EC;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

static void func_actor_403600_80141D30(Enemy* arg0, Task* arg1)
{
    GfxCoord*        workCoord;
    GfxCoord*        coord;
    Actor403600Work* work;

    coord = arg1->extra.tmd->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }

    arg1->work              = work;
    work->worldCoord.parent = &gGfxViewCoord;
    gfxSetRotIdentity(&work->worldCoord.coord);
    work->worldCoord.coord.t[0] = coord->coord.t[0];
    work->worldCoord.coord.t[1] = coord->coord.t[1];
    workCoord                   = &work->worldCoord;
    work->worldCoord.coord.t[2] = coord->coord.t[2];
    coord->parent               = workCoord;
    gfxSetRotIdentity(&coord->coord);
    coord->coord.t[1]             = 0x690;
    coord->coord.t[0]             = 0;
    coord->coord.t[2]             = 0x5DC;
    work->worldCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(workCoord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    work->worldCoord.coord.t[0] = 0;
    work->worldCoord.coord.t[1] = 0;
    work->worldCoord.coord.t[2] = 0;
    work->mode                  = ACTOR_403600_MODE_PARKED;
    arg1->msgTable              = D_actor_403600_80160504;
    arg1->exitCallback          = _actor403600SceneFigureExit;
    work->ambientBoost          = 0x2328;
    work->hitCooldown           = 0;
    arg1->state                += 1;
}

/// Enables the scene figure's model buffers after two task updates.
///
/// Requires zero-initialized figure work and a live model. Reuses hitCooldown
/// for the signed-halfword delay, then sets it to Q12 unity for subsequent
/// scaling, clears both scene counters and advances to the running handler.
static void _actor403600WaitSceneFigure(Enemy* unusedEnemy, Task* task)
{
    enum { ACTOR_403600_SCENE_START_DELAY_FRAMES = 2 };
    TmdObject*       object;
    Actor403600Work* work;
    u16              elapsedFrames;

    work              = task->work;
    elapsedFrames     = work->hitCooldown + 1;
    work->hitCooldown = elapsedFrames;
    if ((s16)elapsedFrames >= ACTOR_403600_SCENE_START_DELAY_FRAMES) {
        tmdAllocPrimitiveBuffer(task->extra.tmd);
        object         = task->extra.tmd;
        object->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        object         = task->extra.tmd;
        object->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        task->state++;
        work->phaseFrame  = 0;
        work->sceneFrame  = 0;
        work->hitCooldown = ONE;
    }
}

/// Detaches the scene model before its work-owned parent coordinate is freed.
///
/// Requires the figure's live model/work. Enemy exit begins default task teardown;
/// this figure has no linked target or collision body to unlink.
static void _actor403600SceneFigureExit(Task* task)
{
    task->extra.tmd->coords->parent = &gGfxViewCoord;
    enemyTaskExit(task);
}

/// Multiplies the coordinate's current rotation basis by a signed Q12 scale.
///
/// Requires a live coordinate and room for one scratch SVECTOR. uniformScale
/// uses 4096 for unity and must fit the GTE's signed IR0 input; the scene uses
/// 4096..4608. GPF12 retains GTE halfword saturation/quantization. Translation
/// stays intact, composition is invalidated and the scratch reservation is freed.
static void _actor403600ScaleSceneFigure(GfxCoord* coord, s32 uniformScale)
{
    // Scales one column of the borrowed matrix with the live scratch vector.
    // columnIndex must be a literal in 0..2; captures matrix, column and
    // uniformScale. GTE state is overwritten, and arguments have no side effects.
#define ACTOR_403600_SCALE_ROTATION_COLUMN(columnIndex)     \
    {                                                       \
        gte_ReadMatrixColumn(matrix, columnIndex, column);  \
        gte_lddp(uniformScale);                             \
        gte_ldsv(column);                                   \
        gte_gpf12();                                        \
        gte_stsv(column);                                   \
        gte_WriteMatrixColumn(column, matrix, columnIndex); \
    }

    SVECTOR** cursorSlot;
    SVECTOR*  cursor;
    SVECTOR*  column;
    MATRIX*   matrix;

    cursorSlot  = (SVECTOR**)SCRATCH_STACK_CURSOR_SLOT;
    cursor      = *cursorSlot;
    column      = cursor - 1;
    *cursorSlot = column;
    matrix      = &coord->coord;

    ACTOR_403600_SCALE_ROTATION_COLUMN(0);

    ACTOR_403600_SCALE_ROTATION_COLUMN(1);

    ACTOR_403600_SCALE_ROTATION_COLUMN(2);

    cursor              = *cursorSlot;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    *cursorSlot         = cursor + 1;

#undef ACTOR_403600_SCALE_ROTATION_COLUMN
}
