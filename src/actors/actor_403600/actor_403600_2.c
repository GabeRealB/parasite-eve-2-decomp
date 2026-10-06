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
#include "gameplay/object_fields.h"
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
    SVECTOR   offset;   // Player's offset from the room's centre on X and Z, the bearing's operands; then the arm: the radius along Z, turned by `rotation` into the boss's offset from the centre
    GfxMatrix rotation; // Identity turned about Y by the pass's bearing
    s32       bearing;  // Even pass only: bearing of the player from the room's centre, 4096 to a turn, negated while the player stands in the room's middle
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

static void func_actor_403600_80138EF8(struct Enemy* enemy, Task* task);
static void func_actor_403600_8013938C(Enemy* arg0, Task* arg1);
static void func_actor_403600_8013C864(Task* arg0);
void        func_actor_403600_80138C9C(Actor403600Ripple* arg0);
static u8*  func_actor_403600_80138DCC(Task* arg0);
static void func_actor_403600_8013CCEC(Task* arg0, s32 arg1);
static s32  func_actor_403600_8013D9A8(Task* arg0);
static void func_actor_403600_8013DAF4(Task* arg0, s32 arg1);
static s32  func_actor_403600_8013DDF4(Task* arg0, s16 arg1);
static s32  func_actor_403600_8013DFE0(Task* arg0);
static void func_actor_403600_8013E470(GfxCoord* arg0, s32* arg1, s32* arg2);
static s16  func_actor_403600_8013E66C(GfxCoord* arg0);
static s32  func_actor_403600_8013E7D4(Task* arg0, u16 arg1);
static void func_actor_403600_8013EA04(Task* arg0);
static void func_actor_403600_8013F608(Task* arg0);
static void func_actor_403600_801417A8(Task* arg0, s32 arg1);
static s32  func_actor_403600_80141840(Task* arg0);
static void func_actor_403600_80141B60(Task* arg0);
s32         func_actor_403600_801406A4(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3);
static void func_actor_403600_80140B4C(struct Enemy* arg0, Task* arg1);
static void func_actor_403600_80141F58(GfxCoord* arg0, s32 arg1);

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

static void func_actor_403600_80141598(Task* arg0);
static void func_actor_403600_8014174C(Task* arg0);

static void func_actor_403600_8013A444(Task* arg0);
static void func_actor_403600_801419E8(Task* arg0);
static void func_actor_403600_8013955C(Task* arg0);
static void func_actor_403600_801396F8(Task* arg0);
static void func_actor_403600_8013D15C(Task* arg0);
static void func_actor_403600_80141C7C(Task* arg0, s32 arg1);
static void func_actor_403600_8013DC7C(Task* arg0);
static void func_actor_403600_8013F0C0(Task* arg0);
static void func_actor_403600_801411D4(Task* arg0, s32 arg1);
static void func_actor_403600_801412D0(Enemy* arg0, Task* arg1);
static void func_actor_403600_80141338(Task* arg0);
static void func_actor_403600_801414FC(Task* arg0);
static void func_actor_403600_8014161C(Task* arg0);
static void func_actor_403600_80141954(s32 arg0);
static void func_actor_403600_80141A34(Task* arg0);
static void func_actor_403600_80141B24(Task* arg0);
static void func_actor_403600_80141C3C(Task* arg0);

static void func_actor_403600_801400BC(Task* arg0);
static void func_actor_403600_80141F28(Task* arg0);
static void func_actor_403600_80140488(Enemy* arg0, Task* arg1);
static void func_actor_403600_80141D30(Enemy* arg0, Task* arg1);
static void func_actor_403600_80141E78(Enemy* arg0, Task* arg1);

s32  func_actor_403600_801406A4(Task* task, s32 msgId, ActorCommand* request, s32 arg3);
void func_actor_403600_80141180(Task*);
void func_actor_403600_80141BE0(Task*);
void func_actor_403600_80141CD4(Task*);

TaskMessageEntry D_actor_403600_80160504[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_403600_801406A4 },
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

GfxCoord* D_actor_403600_801606A0 = NULL;

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

static s32             func_actor_403600_80138D9C(s16* arg0);
static __inline__ u8*  _actor403600ProjectDepth(GfxCoord* coord);
static inline void     _actor403600ArcStart(_Actor403600RushPassScratch* s);
static inline void     _actor403600ArcFinish(Actor403600Work* work, _Actor403600RushPassScratch* s);
static inline u32      _actor403600Rand(void);
static __inline__ void _actor403600UpdateAnimation(Task* task, u8 count);
static void            func_actor_403600_8013F7B8(Enemy* enemy, Task* task);
static __inline__ void _actor403600UpdateColor(Enemy* enemy, Task* task);
static __inline__ void _actor403600RotateParts(Task* task);
static void            func_actor_403600_8013FC2C(Enemy* arg0, Task* arg1);
static inline void     _actor403600ResetState(Task* task);

void func_actor_403600_80138C34(Task* arg0)
{
    Task* parent;

    parent = arg0->parent;
    func_actor_403600_80132E40(parent, parent->parent->work, parent->work);
}

void func_actor_403600_80138C68(Task* arg0)
{
    worldCollisionUnlinkBody(&((Actor403600ProjectileWork*)arg0->work)->attackBody);
    taskKill(arg0);
}

/// Advances the ripple by one step: moves `head` back one slot in the two
/// sample rings, clears it, ramps the source's strength up while `emitting` is
/// set (restarting its phase on a rising edge) or down otherwise, and records
/// the source's phase and strength in the new head while the strength is
/// non-zero.
void func_actor_403600_80138C9C(Actor403600Ripple* state)
{
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
        if (state->sourceStrength < 0x1000) {
            state->sourceStrength += 0x200;
        }
    } else if (state->sourceStrength > 0) {
        state->sourceStrength -= 0x80;
    }
    state->wasEmitting = state->emitting;
    if (state->sourceStrength != 0) {
        state->phase[head]    = state->sourcePhase;
        state->strength[head] = state->sourceStrength;
        if (state->shallow == 0) {
            state->sourcePhase += 0x180;
        } else {
            state->sourcePhase += 0x100;
        }
    }
}

static s32 func_actor_403600_80138D9C(s16* arg0)
{
    s32 i;

    for (i = 0; i < 0x20; i++, arg0++) {
        if (*arg0 != 0) {
            return 0;
        }
    }
    return 1;
}

/// Projects the origin of coordinate 1 and passes its depth on.
static __inline__ u8* _actor403600ProjectDepth(GfxCoord* coord)
{
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
    block->otz = (block->otz >> 4) + 0x1E;
    frameCaptureQueue(block->otz);
    return (u8*)SCRATCH_STACK_RELEASE_BLOCK(ActorOriginDepthScratch);
}

static u8* func_actor_403600_80138DCC(Task* arg0)
{
    return _actor403600ProjectDepth(&arg0->extra.tmd->coords[1]);
}

/// Spawns this actor: parks its work block in `task->work`, destroying the
/// enemy if there is none, hangs the model's root coordinate under the block's
/// world coordinate, links the enemy and its three collision bodies, sets its
/// hit points from the kind's `hpMax` raised by the session's
/// `bossPartsHpSum`, builds the animation rig, turns the actor to the heading
/// its model already had, and spawns its display task above it.
static void func_actor_403600_80138EF8(Enemy* enemy, Task* task)
{
    s32                    state;
    SVECTOR                rot;
    s16                    temp_a0_2;
    s16                    temp_s0_5;
    GfxCoord*              temp_s5;
    Task*                  temp_v0_4;
    s32                    var_s0;
    GfxCoord*              temp_a0;
    GfxCoord*              temp_s0;
    WorldCollisionContact* temp_s0_2;
    WorldCollisionContact* temp_s0_3;
    WorldCollisionContact* temp_s0_4;
    TmdObject*             temp_s2;
    Actor403600Work*       work;
    GfxRotationWords*      workRotation;
    GfxRotationWords*      modelRotation;
    GameSession*           gpSess;

    temp_s2 = task->extra.tmd;
    temp_s0 = temp_s2->coords;
    work    = memCalloc(sizeof(Actor403600Work), false);
    temp_s5 = &temp_s0[1];
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work                                           = work;
    work->worldCoord.parent                              = &gGfxViewCoord;
    workRotation                                         = (GfxRotationWords*)&work->worldCoord.coord;
    ((GfxRotationWords*)&work->worldCoord.coord)->m00M01 = ONE;
    workRotation->m02M10                                 = 0;
    workRotation->m11M12                                 = ONE;
    workRotation->m20M21                                 = 0;
    workRotation->m22                                    = ONE;
    work->worldCoord.coord.t[0]                          = temp_s0->coord.t[0];
    work->worldCoord.coord.t[1]                          = temp_s0->coord.t[1];
    temp_a0                                              = &work->worldCoord;
    work->worldCoord.coord.t[2]                          = temp_s0->coord.t[2];
    modelRotation                                        = (GfxRotationWords*)&temp_s0->coord;
    temp_s0->parent                                      = temp_a0;
    ((GfxRotationWords*)&temp_s0->coord)->m00M01         = ONE;
    modelRotation->m02M10                                = 0;
    modelRotation->m11M12                                = ONE;
    modelRotation->m20M21                                = 0;
    modelRotation->m22                                   = ONE;
    temp_s0->coord.t[0]                                  = 0;
    temp_s0->coord.t[1]                                  = 0x744;
    temp_s0->coord.t[2]                                  = 0;
    work->worldCoord.composeStamp                        = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(temp_a0);
    temp_s0->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(temp_s0);
    temp_s2->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    temp_s0->composeStamp = GRAPHICS_COORD_DIRTY;
    temp_s2->lightMtx     = &work->light;
    temp_s2->colorMtx     = &work->color;
    enemy->field_4        = &temp_s0[1].coord;
    enemy->field_48       = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->bodyPos.vy             = -0x1F4;
    gpSess                        = gGameSession;
    enemy->coord                  = temp_s5;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &D_actor_403600_80150EC8;
    enemy->recs                   = work->hitContacts;
    temp_a0_2                     = D_actor_403600_80150EC8.hpMax + (((u16)gpSess->bossPartsHpSum * 0x4B) / 100);
    enemy->hp                     = temp_a0_2;
    work->hpMax                   = temp_a0_2;
    var_s0                        = 1;
    work->hpAt60Percent           = (s16)((temp_a0_2 * 0x3C) / 100);
    work->hpAt35Percent           = (s16)((work->hpMax * 0x23) / 100);
    animationInitContext(&work->rig.anim, D_actor_403600_8016057C, temp_s2, work->rig.poses, work->rig.slots);
    do {
        animationResetSlot(&work->rig.anim, var_s0, 1);
        var_s0 += 1;
    } while (var_s0 < 0x14);
    (sceneAcquireBattleRef)(0);
    work->animId                   = 1;
    work->hitEffectArg.coord       = &work->worldCoord;
    work->hitEffectArg.spawnArgLo  = 0x600;
    work->hitEffectArg.spawnArgHi  = 2;
    work->hitEffectOffset.vy       = -0x1F4;
    temp_s0_2                      = work->hitContacts;
    work->appliedAnimId            = 0;
    work->hitCooldown              = 0;
    work->hitEffectOffset.vx       = 0;
    work->hitEffectOffset.vz       = 0xC8;
    work->hitBody.coord            = temp_s5;
    work->hitBody.context.contacts = temp_s0_2;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = 0x30024;
    work->hitBody.radius           = 0x3E8;
    work->hitBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(temp_s0_2, 4, 0);
    temp_s0_3                         = work->attackContacts;
    work->attackBody.coord            = temp_s5;
    work->attackBody.context.contacts = temp_s0_3;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0x3E8;
    work->hitBody.flags               = work->hitBody.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->attackBody.key              = damagePackAttackKey(&D_actor_403600_80150E9C, 1);
    work->attackBody.radius           = 0x5DC;
    work->attackBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(temp_s0_3, 1, 0);
    temp_s0_4                       = work->gridContacts;
    work->gridBody.coord            = temp_s5;
    work->gridBody.context.contacts = temp_s0_4;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vz           = 0;
    work->gridBody.pos.vy           = 0x7D0;
    work->gridBody.key              = 0;
    work->gridBody.radius           = 0x64;
    work->gridBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    work->attackBody.flags          = work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->gridBody);
    worldCollisionInitContacts(temp_s0_4, 4, 0);
    work->gridBody.flags = work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &rot);
    temp_s0_5 = ratan2(rot.vx, rot.vz);
    rot.vx    = 0;
    rot.vy    = temp_s0_5;
    rot.vz    = 0;
    RotMatrix(&rot, &work->worldCoord.coord);
    work->yaw               = temp_s0_5;
    temp_v0_4               = taskSpawnFromTable(D_actor_403600_801421A0, 0, 0, 0);
    D_actor_403600_801606AC = temp_v0_4;
    if (temp_v0_4 != 0) {
        taskReparent(task, temp_v0_4);
    }
    work->childEnemy        = 0;
    D_actor_403600_801606A8 = task;
    work->weakPhase         = 0;
    work->recoilSpeed       = 0;
    work->recoilHold        = 0;
    work->exposed           = 0;
    func_actor_403600_8014174C(task);
    D_actor_403600_80160568.animationId = 0;
    D_actor_403600_801606B8[1]          = 0;
    D_actor_403600_801606B8[0]          = 0;
    task->msgTable                      = D_actor_403600_80160504;
    task->exitCallback                  = func_actor_403600_80141598;
    work->mode                          = ACTOR_403600_MODE_PARKED;
    state                               = task->state;
    D_actor_403600_801606BC.nextSlot    = 0;
    task->state                         = state + 1;
}

static void func_actor_403600_8013938C(Enemy* arg0, Task* arg1)
{
    s16              temp_a1;
    Actor403600Work* work;

    work = arg1->work;
    switch (gSceneCombatState.actorControl) {
        case 0:
            if (work->pauseSoundSent != 0) {
                work->pauseSoundSent = 0;
                SndEvt_EnqueueType9(SOUND_AREA_BANK_ALL);
            }
            break;
        case 1:
            func_actor_403600_801412D0(arg0, arg1);
            if (work->pauseSoundSent == 0) {
                work->pauseSoundSent = 1;
                SndEvt_EnqueueType8(SOUND_AREA_BANK_ALL);
            }
            return;
        case 2:
            arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = (WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE);
            return;
    }
    if (((gDisplayState.pendingMode & DISPLAY_MODE_MENU_GROUP_MASK) == DISPLAY_MODE_GAME_MENU_GROUP) && (work->pauseSoundSent == 0)) {
        work->pauseSoundSent = 1;
        SndEvt_EnqueueType8(SOUND_AREA_BANK_ALL);
    }
    func_actor_403600_801396F8(arg1);
    if (work->mode != ACTOR_403600_MODE_PARKED) {
        if (work->mode < ACTOR_403600_MODE_SCENE_POSE) {
            func_actor_403600_8013DC7C(arg1);
            func_actor_403600_8013955C(arg1);
            func_actor_403600_8013D15C(arg1);
        }
    }
    func_actor_403600_801411D4(arg1, 0x14);
    if (work->mode != ACTOR_403600_MODE_PARKED) {
        if (work->mode < ACTOR_403600_MODE_SCENE_POSE) {
            func_actor_403600_80141338(arg1);
            func_actor_403600_8014161C(arg1);
            func_actor_403600_80141A34(arg1);
        }
    }
    work->worldCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->worldCoord);
    func_actor_403600_801412D0(arg0, arg1);
    temp_a1 = work->ambientBoost;
    if (temp_a1 != 0) {
        worldCoordSetModelAmbientColor(arg1->extra.tmd, temp_a1, temp_a1, temp_a1);
    }
    func_actor_403600_801414FC(arg1);
    func_actor_403600_8013F0C0(arg1);
}

static void func_actor_403600_8013955C(Task* arg0)
{
    Actor403600Work* work;
    Enemy*           temp_s0;
    s32              temp_ret;
    u32              temp_v0;
    u32              temp_v0_2;
    u32              temp_v1_2;
    u8               temp_v1;

    temp_s0 = arg0->spawnArg2.pointer;
    temp_v1 = temp_s0->reactionFlags;
    work    = arg0->work;
    if (temp_v1 != 0) {
        if (temp_v1 & ENEMY_REACTION_STAGGER) {
            temp_s0->reactionFlags = temp_v1 & ENEMY_REACTION_STAGGER_CLEAR;
            work->mode             = ACTOR_403600_MODE_STAGGER;
        }
        if (temp_s0->reactionFlags & ENEMY_REACTION_BUILDUP) {
            temp_s0->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
            work->mode              = ACTOR_403600_MODE_STUN;
            work->animId            = 0xE;
            work->stunFrames        = D_actor_403600_80150EC8.buildupSteps * 0x1E;
        }
        if (temp_s0->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            if (work->action != ACTOR_403600_ACTION_RUSH) {
                temp_ret = Gp_TickObjFlag4(temp_s0);
                if (temp_ret != 0) {
                    temp_v1_2       = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = temp_v1_2;
                    if ((temp_v1_2 >> 0x10) & 1) {
                        temp_v0            = (temp_v1_2 * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                        gRandomLcgState    = temp_v0;
                        work->flinchRot.vx = ((temp_v0 >> 0xB) & 0x60) + 0x80;
                    } else {
                        temp_v0_2          = (temp_v1_2 * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                        gRandomLcgState    = temp_v0_2;
                        work->flinchRot.vx = -(((temp_v0_2 >> 0xB) & 0x60) + 0x80);
                    }
                    func_actor_403600_8013DAF4(arg0, temp_ret / 5);
                }
            }
            if ((Gp_ObjFlag4Expired(temp_s0) != 0) || (temp_s0->hp < 0x1F4)) {
                temp_s0->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
        }
    }
}

/// Restores a fixed set of fields in `task`'s work block to their starting
/// values.
static inline void _actor403600ResetState(Task* task)
{
    Actor403600Work* work = task->work;

    work->defeated        = 0;
    work->animBlendFrames = 8;
    work->animRate        = 0x10;
    work->aimMode         = ACTOR_403600_AIM_PLAYER;
    work->ignorePushOut   = 0;
    work->ambientBoost    = 0;
    work->committed       = 0;
    work->forwardSpeed    = 0;
    work->action          = ACTOR_403600_ACTION_CHOOSE;
    work->verticalSpeed   = 0;
    work->phaseFrame      = 0;
    work->actionDelay     = 0xA;
    work->turnRate        = 0x40;
    work->roll            = 0;
    work->diving          = 0;
    work->repositioning   = 0;
    work->pauseSoundSent  = 0;
}

static void func_actor_403600_801396F8(Task* arg0)
{
    SVECTOR          sp10;
    SVECTOR          sp18;
    u8               smokeIndex;
    s32              temp_a0_4;
    s16              temp_v0_13;
    s32              temp_v0_14;
    s16              temp_v0_16;
    s16              temp_v0_18;
    s32              temp_v0_19;
    s16              temp_v0_29;
    s16              temp_v0_9;
    s16              temp_v1;
    s16              temp_v1_2;
    s16              temp_v1_3;
    s16              temp_v1_6;
    EffectWork*      temp_v0_5;
    EffectWork*      temp_v0_6;
    EffectWork*      temp_v0_7;
    EffectWork*      temp_v0_8;
    s32              temp_s2;
    s32              temp_v0_17;
    s32              temp_s0_2;
    s32              temp_s0_4;
    u16              temp_v0_22;
    u16              temp_v0_23;
    u16              temp_v0_27;
    u16              temp_v0_28;
    u16              temp_v0_2;
    u16              temp_v0_4;
    s32              temp_v1_4;
    u32              temp_v0_10;
    u32              temp_v0_11;
    u8               temp_a0;
    u8               temp_a0_2;
    u8               temp_a0_3;
    u8               temp_a0_5;
    GfxCoord*        temp_s0;
    GfxCoord*        temp_s0_3;
    Actor403600Work* work;

    work    = arg0->work;
    temp_v1 = work->mode;
    switch (temp_v1) {
        case ACTOR_403600_MODE_PARKED:
            work->worldCoord.coord.t[0] = 0x196E;
            work->worldCoord.coord.t[1] = 0x1AE;
            work->worldCoord.coord.t[2] = 0x1630;
            return;
        case ACTOR_403600_MODE_FIGHT:
            func_actor_403600_8013A444(arg0);
            return;
        case ACTOR_403600_MODE_STAGGER:
            func_actor_403600_80141B24(arg0);
            work->ignorePushOut = 0;
            work->committed     = 1;
            work->verticalSpeed = 0;
            work->animId        = 0xB;
            if (work->phaseFrame < 0xF) {
                work->forwardSpeed = -0xA;
            }
            if (work->phaseFrame >= 0x27) {
                _actor403600ResetState(arg0);
                work->mode = ACTOR_403600_MODE_FIGHT;
                return;
            }
        default:
            return;
        case ACTOR_403600_MODE_STUN:
            func_actor_403600_80141B24(arg0);
            work->forwardSpeed  = 0;
            work->verticalSpeed = 0;
            if (work->animId == 0xE) {
                temp_v0_2        = work->stunFrames - 1;
                work->stunFrames = temp_v0_2;
                if ((temp_v0_2 << 0x10) == 0) {
                    work->animId = 0xF;
                    return;
                }
            } else if (work->phaseFrame >= 0x11) {
                _actor403600ResetState(arg0);
                work->mode = ACTOR_403600_MODE_FIGHT;
                return;
            }
            break;
        case ACTOR_403600_MODE_FREEZE:
            func_actor_403600_80141B24(arg0);
            work->forwardSpeed  = 0;
            work->verticalSpeed = 0;
            temp_v0_4           = work->stunFrames - 1;
            work->stunFrames    = temp_v0_4;
            if ((temp_v0_4 << 0x10) != 0) {
                work->animRate               = 0;
                work->worldCoord.coord.t[1] += rsin(gDisplayState.animFrame << 9) >> 8;
                return;
            }
            _actor403600ResetState(arg0);
            work->mode = ACTOR_403600_MODE_FIGHT;
            return;
        case ACTOR_403600_MODE_WEAKEN:
            func_actor_403600_80141B24(arg0);
            work->weakPhase     = 1;
            work->committed     = 1;
            work->animId        = 0xB;
            work->ignorePushOut = 0;
            work->verticalSpeed = 0;
            work->chainSweep    = 0;
            if (work->phaseFrame == 1) {
                memset(&sp10, 0, 8);
                sp10.vy = 0x64;
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[1], 3, &sp10);
                func_actor_403600_80141954(1);
                D_800626EC[5].data.model = &gShelterB2PodBottomModel0A2A0;
                temp_v0_5                = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, &arg0->extra.tmd->coords[1], 0, NULL);
                if (temp_v0_5 != NULL) {
                    func_actor_403600_801419E8(temp_v0_5->task);
                }
                D_800626EC[5].data.model = &gShelterB2PodBottomModel0A68C;
                temp_v0_6                = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, &arg0->extra.tmd->coords[1], 0, NULL);
                if (temp_v0_6 != NULL) {
                    func_actor_403600_801419E8(temp_v0_6->task);
                }
                D_800626EC[5].data.model = &gShelterB2PodBottomModel0AA08;
                temp_v0_7                = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, &arg0->extra.tmd->coords[1], 0, NULL);
                if (temp_v0_7 != NULL) {
                    func_actor_403600_801419E8(temp_v0_7->task);
                }
                D_800626EC[5].data.model = &gShelterB2PodBottomModel0AE48;
                temp_v0_8                = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK8, &arg0->extra.tmd->coords[1], 0, NULL);
                if (temp_v0_8 != NULL) {
                    func_actor_403600_801419E8(temp_v0_8->task);
                }
                Gp_SpawnEff(EFFECT_030, &arg0->extra.tmd->coords[1], 0x800, NULL);
            }
            temp_v1_2 = work->phaseFrame;
            if ((temp_v1_2 == 4) || (temp_v1_2 == 6)) {
                memset(&sp18, 0, 8);
                sp18.vy = 0x64;
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[1], 3, &sp18);
            }
            if (work->phaseFrame < 0xF) {
                work->forwardSpeed = -0xA;
            }
            if (work->phaseFrame >= 0x27) {
                _actor403600ResetState(arg0);
                work->mode = ACTOR_403600_MODE_FIGHT;
            }
            return;
        case ACTOR_403600_MODE_RECOVER:
            if (work->weakPhase == 1) {
                if (work->phaseFrame >= 0x32) {
                    temp_a0 = work->whiteout;
                    fadeDrawOverlay(temp_a0, temp_a0, temp_a0, GPU_BLEND_ADD);
                    temp_v0_9      = (u16)work->whiteout + 0xF;
                    work->whiteout = temp_v0_9;
                    if (temp_v0_9 >= 0xFF) {
                        work->whiteout = 0xFF;
                    }
                }
                if (((u16)work->phaseFrame & 3) == 3) {
                    Gp_SpawnEff(EFFECT_HIT_PUFF, &arg0->extra.tmd->coords[1], 0x12800, NULL);
                }
                temp_v1_3 = work->phaseFrame;
                if (temp_v1_3 < 0x2F) {
                    if (temp_v1_3 == 0x2E) {
                        temp_s0    = &work->worldCoord;
                        temp_s2    = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160013;
                        temp_s0_2  = (s8)worldCoordGetOriginAudioPan(temp_s0);
                        temp_v0_10 = worldCoordGetOriginAudioDepth(temp_s0);
                        sndEvtRequestScriptStart(temp_s2, temp_s0_2, (s32)(((temp_v0_10 >> 0x1F) + temp_v0_10) << 0x17) >> 0x18);
                    }
                    if (((u16)work->phaseFrame & 0xF) == 0xF) {
                        func_shelter_b2_pod_bottom_80181940(arg0);
                    }
                } else {
                    func_shelter_b2_pod_bottom_80181940(arg0);
                    if (work->phaseFrame == 0x32) {
                        temp_s0_3  = &work->worldCoord;
                        temp_s2    = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160014;
                        temp_s0_4  = (s8)worldCoordGetOriginAudioPan(temp_s0_3);
                        temp_v0_11 = worldCoordGetOriginAudioDepth(temp_s0_3);
                        sndEvtRequestScriptStart(temp_s2, temp_s0_4, (s32)(((temp_v0_11 >> 0x1F) + temp_v0_11) << 0x17) >> 0x18);
                        Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_CHARGE_BURST, &arg0->extra.tmd->coords[1], 0x1E, NULL);
                    }
                }
                if (work->phaseFrame == 0x3C) {
                    func_actor_403600_80141954(0);
                }
                work->forwardSpeed  = 0;
                work->verticalSpeed = 0;
                work->animId        = 4;
                if (work->phaseFrame >= 0x46) {
                    sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_CHARGE, 0x14);
                    _actor403600ResetState(arg0);
                    work->animId    = 5;
                    work->weakPhase = 2;
                    return;
                }
            } else {
                temp_a0_2 = work->whiteout;
                fadeDrawOverlay(temp_a0_2, temp_a0_2, temp_a0_2, GPU_BLEND_ADD);
                temp_v0_13     = (u16)work->whiteout - 0x28;
                work->whiteout = temp_v0_13;
                if ((temp_v0_13 << 0x10) <= 0) {
                    work->whiteout = 0;
                }
                temp_v0_14 = work->phaseFrame;
                if (temp_v0_14 == 0xA) {
                    sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_CHARGE, 0x14);
                    temp_v0_14 = work->phaseFrame;
                }
                if (temp_v0_14 >= 0x1E) {
                    _actor403600ResetState(arg0);
                    work->weakPhase = 0;
                    work->mode      = ACTOR_403600_MODE_FIGHT;
                    return;
                }
            }
            break;
        case ACTOR_403600_MODE_SCENE_POSE:
            temp_a0_3 = work->whiteout;
            fadeDrawOverlay(temp_a0_3, temp_a0_3, temp_a0_3, GPU_BLEND_ADD);
            temp_v0_16     = (u16)work->whiteout - 0x1E;
            work->whiteout = temp_v0_16;
            if ((temp_v0_16 << 0x10) <= 0) {
                work->whiteout = 0;
            }
            work->forwardSpeed  = 0;
            work->verticalSpeed = 0;
            worldCoordSetModelAmbientColor(arg0->extra.tmd, 0x1F40, 0x1F40, 0x1F40);
            return;
        case ACTOR_403600_MODE_SCENE_BRIGHTEN:
            work->forwardSpeed  = 0;
            work->verticalSpeed = 0;
            work->ambientBoost  = (u16)(work->ambientBoost + 0x64);
            return;
        case ACTOR_403600_MODE_SCENE_ASCEND:
            temp_a0_4 = work->step;
            switch (temp_a0_4) {
                case 0:
                    temp_v0_17                  = work->worldCoord.coord.t[1] + work->forwardSpeed;
                    work->worldCoord.coord.t[1] = temp_v0_17;
                    if (temp_v0_17 < -0x1B61) {
                        work->actionDelay   = 0;
                        work->forwardSpeed  = 0;
                        work->phaseFrame    = 0;
                        work->actionDelay   = -0x19;
                        work->actionCounter = 3U;
                        work->step          = (s16)((u16)work->step + 1);
                    }
                    if (!((u16)work->phaseFrame & 1)) {
                        temp_v0_18         = (u16)work->forwardSpeed + 2;
                        work->forwardSpeed = temp_v0_18;
                        if (temp_v0_18 >= -0x1E) {
                            work->forwardSpeed = -0x1E;
                        }
                    }
                    Gp_SpawnEff(0x601BF, &arg0->extra.tmd->coords[15], 0xC00, NULL);
                    Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_RISING_SPRITE, &arg0->extra.tmd->coords[19], 0xC00, NULL);
                    return;
                case 1:
                    if ((u16)work->phaseFrame & 1) {
                        temp_v0_19                  = work->actionDelay;
                        temp_v1_4                   = work->worldCoord.coord.t[1];
                        temp_v1_4                  += temp_v0_19;
                        work->worldCoord.coord.t[1] = temp_v1_4;
                        temp_v1_4                   = (u16)work->actionDelay;
                        if (temp_v0_19 < 0) {
                            temp_v0_19        = temp_v1_4 + 1;
                            work->actionDelay = temp_v0_19;
                            if ((temp_v0_19 << 0x10) == 0) {
                                work->actionDelay   = temp_a0_4;
                                temp_v0_22          = work->actionCounter - 1;
                                work->actionCounter = temp_v0_22;
                                if ((temp_v0_22 << 0x10) == 0) {
                                    work->actionCounter = 3U;
                                    work->actionDelay   = 0x19;
                                }
                            }
                        } else {
                            temp_v0_19        = temp_v1_4 - 1;
                            work->actionDelay = temp_v0_19;
                            if ((temp_v0_19 << 0x10) == 0) {
                                work->actionDelay   = 0;
                                temp_v0_23          = work->actionCounter - 1;
                                work->actionCounter = temp_v0_23;
                                if ((temp_v0_23 << 0x10) == 0) {
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
                            Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[smokeCoords[smokeIndex]], 0x34C00, NULL);
                            Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_RISING_SPRITE, &arg0->extra.tmd->coords[smokeCoords[smokeIndex]], 0xC00, NULL);
                        }
                        work->actionTimer = 0;
                    }
                    work->actionTimer = (s16)((u16)work->actionTimer + 1);
                    if (work->phaseFrame >= 0xC8) {
                        work->phaseFrame = 0;
                        work->step       = (s16)((u16)work->step + 1);
                    }
                    break;
                case 2:
                    work->animId = 0x14;
                    if (work->actionTimer >= 0x14) {
                        // Model coordinates the smoke and sprites rise from.
                        u8 smokeCoords[9] = { 1, 12, 13, 14, 15, 16, 17, 18, 19 };

                        for (smokeIndex = 0; smokeIndex < ARRAY_SIZE(smokeCoords); smokeIndex++) {
                            Gp_SpawnEff(EFFECT_SMOKE_PUFF, &arg0->extra.tmd->coords[smokeCoords[smokeIndex]], 0x34C00, NULL);
                            Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_RISING_SPRITE, &arg0->extra.tmd->coords[smokeCoords[smokeIndex]], 0xC00, NULL);
                        }
                        work->actionTimer = 0;
                    }
                    work->actionTimer = (s16)((u16)work->actionTimer + 1);
                    if ((u16)work->phaseFrame & 1) {
                        temp_v0_19                  = work->actionDelay;
                        temp_v1_4                   = work->worldCoord.coord.t[1];
                        temp_v1_4                  += temp_v0_19;
                        work->worldCoord.coord.t[1] = temp_v1_4;
                        temp_v1_4                   = (u16)work->actionDelay;
                        if (temp_v0_19 < 0) {
                            temp_v0_19        = temp_v1_4 + 1;
                            work->actionDelay = temp_v0_19;
                            if ((temp_v0_19 << 0x10) == 0) {
                                work->actionDelay   = 1;
                                temp_v0_27          = work->actionCounter - 1;
                                work->actionCounter = temp_v0_27;
                                if ((temp_v0_27 << 0x10) == 0) {
                                    work->actionCounter = 3U;
                                    work->actionDelay   = 0x19;
                                    return;
                                }
                            }
                        } else {
                            temp_v0_19        = temp_v1_4 - 1;
                            work->actionDelay = temp_v0_19;
                            if ((temp_v0_19 << 0x10) == 0) {
                                work->actionDelay   = 0;
                                temp_v0_28          = work->actionCounter - 1;
                                work->actionCounter = temp_v0_28;
                                if ((temp_v0_28 << 0x10) == 0) {
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
            temp_a0_5 = work->whiteout;
            fadeDrawOverlay(temp_a0_5, temp_a0_5, temp_a0_5, GPU_BLEND_ADD);
            temp_v0_29     = (u16)work->whiteout + 2;
            work->whiteout = temp_v0_29;
            if (temp_v0_29 >= 0xFF) {
                work->whiteout = 0xFF;
            }
            temp_v1_6 = work->step;
            switch (temp_v1_6) {
                case 0:
                    func_800E9BDC(1, 0xF9FF);
                    work->chainSweep          = 0;
                    gPlayerStatus.statusFlags = 0;
                    work->step                = (s16)((u16)work->step + 1);
                    break;
                case 1:
                    if ((s16)work->whiteout == 0xFF) {
                        u8 param1[4];
                        u8 param2[4];

                        param1[2] = 0x1E;
                        param1[3] = 0;
                        param1[0] = 0;
                        param2[0] = 0x24;
                        param2[3] = 0;
                        param2[2] = 0;
                        param2[1] = 0;
                        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
                        work->step = (s16)((u16)work->step + 1);
                    }
                    break;
                case 2:
                    if (cdCmdIsIdle() & 0xFFFF) {
                        if (D_actor_403600_801606B4 != 0) {
                            taskCallExit(D_actor_403600_801606B4);
                        }
                        taskSpawnFromTable(D_actor_303600_80162E98, 0, 0, 0);
                        func_800E9BDC(0, 0xF9FF);
                        work->step = (s16)((u16)work->step + 1);
                    }
                    break;
            }
            break;
    }
}

static void func_actor_403600_8013A444(Task* arg0)
{
    u32                   sp10;
    s32                   sp14;
    s32                   adjusted_y0;
    s32                   adjusted_y1;
    s32                   adjusted_y2;
    s32                   var_a1;
    s16                   temp_a0;
    s16                   temp_a0_2;
    s32                   temp_a2;
    s32                   temp_s1;
    s16                   temp_v0_11;
    s16                   temp_v0_16;
    s16                   temp_v0_25;
    s16                   temp_v0_3;
    s16                   temp_v1;
    s16                   temp_v1_10;
    s16                   temp_v1_11;
    s32                   temp_v1_12;
    s16                   temp_v1_4;
    s16                   temp_v1_8;
    s16                   temp_v1_9;
    s32                   var_v0_10;
    s32                   var_v0_13;
    s32                   var_v0_3;
    s32                   var_v0_8;
    s32                   temp_lo;
    s32                   temp_lo_2;
    s32                   temp_lo_3;
    s32                   temp_lo_4;
    s32                   temp_s4;
    s32                   temp_v0_26;
    s32                   temp_v0_27;
    s32                   temp_v0_28;
    s32                   temp_v1_3;
    s32                   temp_v1_5;
    s32                   temp_v1_6;
    s32                   temp_v1_7;
    s32                   var_s2;
    s32                   var_v0_12;
    s32                   temp_s0_11;
    s32                   temp_s0_12;
    s32                   temp_s0_14;
    s32                   temp_s0_16;
    s32                   temp_s0_17;
    s32                   temp_s0_19;
    s32                   temp_s0_21;
    s32                   temp_s0_22;
    s32                   temp_s0_23;
    s32                   temp_s0_25;
    s32                   temp_s0_26;
    s32                   temp_s0_28;
    s32                   temp_s0_2;
    s32                   temp_s0_30;
    s32                   temp_s0_32;
    s32                   temp_pan_28;
    s32                   temp_s0_4;
    s32                   temp_s0_6;
    s32                   temp_s0_7;
    s32                   temp_s0_9;
    u16                   temp_a3;
    u16                   temp_v0_32;
    u16                   temp_v0_34;
    u16                   temp_v0_35;
    u16                   temp_v0_9;
    u32                   temp_v0;
    u32                   temp_v0_12;
    u32                   temp_v0_13;
    u32                   temp_v0_14;
    u32                   temp_v0_15;
    u32                   temp_v0_17;
    u32                   temp_v0_18;
    u32                   temp_v0_19;
    u32                   temp_v0_21;
    u32                   temp_v0_22;
    u32                   temp_v0_23;
    u32                   temp_v0_24;
    u32                   temp_v0_30;
    u32                   temp_v0_31;
    u32                   temp_v0_33;
    u32                   temp_v0_36;
    u32                   temp_v0_37;
    u32                   temp_v0_5;
    u32                   temp_v0_6;
    u32                   temp_v0_7;
    u32                   temp_v0_8;
    u32                   temp_depth_28;
    GfxCoord*             temp_s0;
    GfxCoord*             temp_s0_10;
    GfxCoord*             temp_s0_13;
    GfxCoord*             temp_s0_15;
    GfxCoord*             temp_s0_18;
    GfxCoord*             temp_s0_20;
    GfxCoord*             temp_s0_24;
    GfxCoord*             temp_s0_27;
    GfxCoord*             temp_s0_29;
    GfxCoord*             temp_s0_31;
    GfxCoord*             temp_s0_3;
    GfxCoord*             temp_s0_5;
    GfxCoord*             temp_s0_8;
    GfxCoord*             temp_sound_28;
    AnimationPlayRequest* temp_s0_msg;
    AnimationPlayRequest* temp_s1_3;
    Actor403600Work*      work;
    GfxCoord*             temp_s4_4;
    GfxCoord*             temp_s6;
    Enemy*                temp_s7;
    GameActor*            temp_v1_2;
    GfxCoord*             var_a0;
    GfxCoord*             var_s0;
    PlayerStatus*         temp_wip;

    temp_wip = &gPlayerStatus;
    work     = arg0->work;
    temp_s7  = arg0->spawnArg2.pointer;
    temp_a0  = work->action;
    temp_s6  = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    switch (temp_a0) {
        case ACTOR_403600_ACTION_CHOOSE:
            work->animId        = 1U;
            work->forwardSpeed  = 0U;
            work->verticalSpeed = 0;
            if (work->phaseFrame >= work->actionDelay) {
                work->actionDelay = 0xA;
                work->phaseFrame  = 0;
                func_actor_403600_8013E470(&work->worldCoord, &sp10, &sp14);
                if ((sp10 < 0x835U) && (work->meleeGaveUp == 0)) {
                    work->attackBody.pos.vz = 0x3E8;
                    work->attackBody.key    = damagePackAttackKey(&D_actor_403600_80150E9C, 1);
                    work->attackBody.radius = 0x5DC;
                    work->animId            = 2U;
                    work->action            = ACTOR_403600_ACTION_MELEE;
                    work->forwardSpeed      = 0U;
                    work->phaseFrame        = 0;
                    work->meleeChaseFrames  = 0U;
                    return;
                }
                func_actor_403600_8013EA04(arg0);
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
                temp_s0   = &work->worldCoord;
                temp_s4   = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160013;
                temp_s0_2 = (s8)worldCoordGetOriginAudioPan(temp_s0);
                temp_v0   = worldCoordGetOriginAudioDepth(temp_s0);
                sndEvtRequestScriptStart(temp_s4, temp_s0_2, (s32)(((temp_v0 >> 0x1F) + temp_v0) << 0x17) >> 0x18);
            }
            if (work->phaseFrame >= 0x14) {
                func_shelter_b2_pod_bottom_80181940(arg0);
            }
            work->animId        = 4U;
            work->forwardSpeed  = 0U;
            work->verticalSpeed = 0;
            if (work->phaseFrame >= 0x2D) {
                _actor403600ResetState(arg0);
                work->action  = ACTOR_403600_ACTION_RECHARGE_END;
                work->exposed = 0;
                return;
            }
            break;
        case ACTOR_403600_ACTION_RECHARGE_END:
            if (work->phaseFrame < 0xC) {
                func_shelter_b2_pod_bottom_80181940(arg0);
            }
            work->animId = 5U;
            temp_v0_3    = work->phaseFrame;
            if (temp_v0_3 == 0xA) {
                sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_CHARGE, 0x14);
            }
            if (work->phaseFrame >= 0x1E) {
                _actor403600ResetState(arg0);
                return;
            }
            break;
        case ACTOR_403600_ACTION_SLAM:
            temp_v1 = work->step;
            switch (temp_v1) {
                case 0:
                    work->committed     = 1;
                    work->animRate      = 0x20;
                    work->animId        = 0x10U;
                    work->forwardSpeed  = 0x14U;
                    work->verticalSpeed = 0;
                    work->targetPos.vx  = (s32)D_actor_403600_801605D4.vx;
                    work->targetPos.vy  = (s32)D_actor_403600_801605D4.vy;
                    work->targetPos.vz  = (s32)D_actor_403600_801605D4.vz;
                    work->turnRate      = 0x40;
                    work->aimMode       = ACTOR_403600_AIM_TARGET;
                    func_actor_403600_8013DFE0(arg0);
                    if (work->phaseFrame >= 0x13) {
                        temp_s0_3 = &work->worldCoord;
                        temp_s4   = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160004;
                        temp_s0_4 = (s8)worldCoordGetOriginAudioPan(temp_s0_3);
                        temp_v0_5 = worldCoordGetOriginAudioDepth(temp_s0_3);
                        sndEvtRequestScriptStart(temp_s4, temp_s0_4, (s32)(((temp_v0_5 >> 0x1F) + temp_v0_5) << 0x17) >> 0x18);
                        work->animRate   = 0x10;
                        work->turnRate   = 0x80;
                        work->phaseFrame = 0;
                        work->diving     = 1;
                        work->step       = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case 1:
                    work->animId        = 0x12U;
                    work->forwardSpeed  = 0x4B0U;
                    work->verticalSpeed = 0;
                    func_actor_403600_8013DFE0(arg0);
                    if (work->worldCoord.coord.t[1] < D_actor_403600_801605D4.vy) {
                        work->worldCoord.coord.t[0] = (s32)D_actor_403600_801605D4.vx;
                        work->worldCoord.coord.t[1] = (s32)D_actor_403600_801605D4.vy;
                        work->worldCoord.coord.t[2] = (s32)D_actor_403600_801605D4.vz;
                        work->targetPos.vx          = (s32)D_actor_403600_801605DC.vx;
                        work->targetPos.vy          = (s32)D_actor_403600_801605DC.vy;
                        work->targetPos.vz          = (s32)D_actor_403600_801605DC.vz;
                        work->aimMode               = ACTOR_403600_AIM_TARGET_SNAP;
                        func_actor_403600_8013DFE0(arg0);
                        work->ignorePushOut = 1;
                        work->phaseFrame    = 0;
                        work->actionParam   = 0;
                        work->forwardSpeed  = 0U;
                        work->actionTimer   = 0x28;
                        work->step          = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case 2:
                    if (work->actionTimer != 0) {
                        work->actionTimer = (s16)((u16)work->actionTimer - 1);
                        return;
                    }
                    if (!((u16)work->phaseFrame & 1)) {
                        Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &arg0->extra.tmd->coords[1], 0, &D_actor_403600_80160664);
                        Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &arg0->extra.tmd->coords[15], 0x800, NULL);
                        Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &arg0->extra.tmd->coords[19], 0x800, NULL);
                    }
                    work->animId        = 0x12U;
                    work->forwardSpeed  = 0x320U;
                    work->verticalSpeed = 0;
                    func_actor_403600_801417A8(arg0, 0x14);
                    if (((D_actor_403600_801605DC.vy - 0x1388) < work->worldCoord.coord.t[1]) && (work->actionParam == 0)) {
                        work->actionParam = 1;
                        temp_s0_5         = &work->worldCoord;
                        temp_s4           = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160005;
                        temp_s0_6         = (s8)worldCoordGetOriginAudioPan(temp_s0_5);
                        temp_v0_6         = worldCoordGetOriginAudioDepth(temp_s0_5);
                        sndEvtRequestScriptStart(temp_s4, temp_s0_6, (s32)(((temp_v0_6 >> 0x1F) + temp_v0_6) << 0x17) >> 0x18);
                    }
                    if ((D_actor_403600_801605DC.vy - 0x3E8) < work->worldCoord.coord.t[1]) {
                        Gp_SpawnPadLerp(0xA, 0xFF, 0x50);
                        var_s2 = 0;
                        if (gPlayerStatus.coordMtx->t[1] < -0xF3B) {
                            temp_s4_4                     = &work->worldCoord;
                            D_actor_403600_801606A4.power = (u16)D_actor_403600_80150EA4;
                            func_actor_403600_8013E470(temp_s4_4, &sp10, &sp14);
                            if ((u32)(sp10 - 0xFA0) < 0x7D1U) {
                                temp_v1_2                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
                                D_actor_403600_801606A4.reaction = 0;
                                D_actor_403600_801606A4.power    = (u16)((u16)D_actor_403600_801606A4.power >> 2);
                                temp_v1_2->hitRegion             = 2;
                                temp_v1_2->pendingDamage         = (u16)D_actor_403600_801606A4.power;
                                temp_v1_2->damageReaction        = GAME_ACTOR_REACTION_ORDINARY;
                            } else {
                                if ((u32)(sp10 - 0x9C4) < 0x5DCU) {
                                    D_actor_403600_801606A4.reaction = 0;
                                    D_actor_403600_801606A4.power    = (u16)((u16)D_actor_403600_801606A4.power >> 1);
                                    var_v0_3                         = (s16)func_actor_403600_8013E66C(temp_s4_4);
                                    if (var_v0_3 < 0) {
                                        var_v0_3 = -var_v0_3;
                                    }
                                    if (var_v0_3 >= 0x401) {
                                        D_actor_403600_80160568.animationId = 4;
                                        func_actor_403600_8013E7D4(arg0, 0);
                                        work->knockbackSpeed = 0x64;
                                    } else {
                                        D_actor_403600_80160568.animationId = 3;
                                        func_actor_403600_8013E7D4(arg0, 1);
                                        work->knockbackSpeed = -0x64;
                                    }
                                    TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
                                    var_s2               = 2;
                                    work->knockbackFrame = 0;
                                } else if (sp10 < 0x9C4U) {
                                    D_actor_403600_801606A4.reaction    = 0;
                                    D_actor_403600_80160568.animationId = 1;
                                    func_actor_403600_8013E7D4(arg0, 1);
                                    work->knockbackSpeed = -0x190;
                                    TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
                                    var_s2               = 3;
                                    work->knockbackFrame = 0;
                                }
                            }
                            if (var_s2 != 0) {
                                temp_s4   = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                                temp_s0_7 = (s8)worldCoordGetOriginAudioPan(temp_s6);
                                temp_v0_7 = worldCoordGetOriginAudioDepth(temp_s6);
                                sndEvtRequestScriptStart(temp_s4, temp_s0_7, (s32)(((temp_v0_7 >> 0x1F) + temp_v0_7) << 0x17) >> 0x18);
                                taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(&D_actor_403600_801606A4, 0), 0);
                            }
                        }
                        temp_s0_8 = &work->worldCoord;
                        temp_s4   = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160006;
                        temp_s0_9 = (s8)worldCoordGetOriginAudioPan(temp_s0_8);
                        temp_v0_8 = worldCoordGetOriginAudioDepth(temp_s0_8);
                        sndEvtRequestScriptStart(temp_s4, temp_s0_9, (s32)(((temp_v0_8 >> 0x1F) + temp_v0_8) << 0x17) >> 0x18);
                        Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_ARC_FLASH, &arg0->extra.tmd->coords[1], 0, &D_actor_403600_80160664);
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
                case 3:
                    work->animId = 0x12U;
                    if ((u16)work->phaseFrame & 8) {
                        temp_v0_9          = work->forwardSpeed + 5;
                        work->forwardSpeed = temp_v0_9;
                        if ((temp_v0_9 << 0x10) > 0) {
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
                case 4:
                    work->animId        = 0x11U;
                    work->verticalSpeed = 0;
                    if (work->phaseFrame >= 0xA) {
                        work->aimMode = ACTOR_403600_AIM_PLAYER_LEVEL;
                        func_actor_403600_8013DFE0(arg0);
                    }
                    if (work->phaseFrame >= 0x26) {
                        _actor403600ResetState(arg0);
                        work->action = ACTOR_403600_ACTION_RECHARGE;
                        return;
                    }
                    break;
            }
            break;
        case ACTOR_403600_ACTION_VOLLEY:
            temp_v0_11 = work->animId - 6;
            switch (temp_v0_11) {
                case 10:
                    work->committed     = 1;
                    work->animRate      = 0x20;
                    work->animId        = 0x10U;
                    work->forwardSpeed  = 0x14U;
                    work->verticalSpeed = 0;
                    work->targetPos.vx  = (s32)D_actor_403600_801605D4.vx;
                    work->targetPos.vy  = (s32)D_actor_403600_801605D4.vy;
                    work->targetPos.vz  = (s32)D_actor_403600_801605D4.vz;
                    work->turnRate      = 0x40;
                    work->aimMode       = ACTOR_403600_AIM_TARGET;
                    func_actor_403600_8013DFE0(arg0);
                    if (work->phaseFrame >= 0x13) {
                        temp_s0_10 = &work->worldCoord;
                        temp_s4    = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160004;
                        temp_s0_11 = (s8)worldCoordGetOriginAudioPan(temp_s0_10);
                        temp_v0_12 = worldCoordGetOriginAudioDepth(temp_s0_10);
                        sndEvtRequestScriptStart(temp_s4, temp_s0_11, (s32)(((temp_v0_12 >> 0x1F) + temp_v0_12) << 0x17) >> 0x18);
                        work->turnRate      = 0x80;
                        work->step          = 0;
                        work->animRate      = 0x10;
                        work->phaseFrame    = 0;
                        work->ignorePushOut = 1;
                        work->animId        = 0x12U;
                        return;
                    }
                    break;
                case 12:
                    work->animId        = 0x12U;
                    work->forwardSpeed  = 0x4B0U;
                    work->verticalSpeed = 0;
                    if (work->step == 1) {
                        work->targetPos.vy = (s32)(gPlayerStatus.coordMtx->t[1] - 0x258);
                    }
                    work->aimMode = ACTOR_403600_AIM_TARGET;
                    if (func_actor_403600_8013DFE0(arg0) < 0x7D1) {
                        if (work->step == 0) {
                            work->step = 1;
                            func_actor_403600_8013CCEC(arg0, 0);
                            return;
                        }
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->phaseFrame    = 0;
                        work->animId        = 0x11U;
                        var_s0              = &work->worldCoord;
                        var_a0              = var_s0;
                        temp_s4             = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160004;
                        temp_s0_12          = (s8)worldCoordGetOriginAudioPan(var_a0);
                        temp_v0_13          = worldCoordGetOriginAudioDepth(var_s0);
                        sndEvtRequestScriptStart(temp_s4, temp_s0_12, (s32)(((temp_v0_13 >> 0x1F) + temp_v0_13) << 0x17) >> 0x18);
                        return;
                    }
                    break;
                case 11:
                    work->animId        = 0x11U;
                    work->turnRate      = 0xA0;
                    work->forwardSpeed  = 0U;
                    work->verticalSpeed = 0;
                    work->aimMode       = ACTOR_403600_AIM_PLAYER_LEVEL;
                    func_actor_403600_8013DFE0(arg0);
                    if (work->phaseFrame >= 0x26) {
                        work->turnRate      = 0x80;
                        work->committed     = 0;
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->phaseFrame    = 0;
                        work->ignorePushOut = 0;
                        work->animId        = 6U;
                        return;
                    }
                    break;
                case 0:
                    temp_a2 = work->phaseFrame;
                    var_a1  = RANDOM_LCG_INCREMENT & ~0xFFFF;
                    if (temp_a2 == 1) {
                        var_a1          = RANDOM_LCG_INCREMENT;
                        temp_v0_14      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                        gRandomLcgState = temp_v0_14;
                        temp_v1_3       = (temp_v0_14 >> 0x10) & 0xF;
                        if (temp_v1_3 < 2) {
                            work->projectileKind = 0;
                        } else if (temp_v1_3 < 5) {
                            work->projectileKind = 2;
                        } else if (temp_v1_3 < 0xA) {
                            work->projectileKind = temp_a2;
                        } else {
                            work->projectileKind = 3;
                        }
                    }
                    if (work->phaseFrame == 0x27) {
                        temp_s0_13 = &work->worldCoord;
                        temp_s4    = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160008;
                        temp_s0_14 = (s8)worldCoordGetOriginAudioPan(temp_s0_13);
                        temp_v0_15 = worldCoordGetOriginAudioDepth(temp_s0_13);
                        sndEvtRequestScriptStart(temp_s4, temp_s0_14, (s32)(((temp_v0_15 >> 0x1F) + temp_v0_15) << 0x17) >> 0x18);
                        taskSpawnFromTable(D_actor_403600_801421A0, 1, (s32)((s16)((u16)work->projectileKind | 0x10)), arg0);
                    }
                    if ((work->phaseFrame == 0x2C) || (work->phaseFrame == 0x31)) {
                        taskSpawnFromTable(D_actor_403600_801421A0, 1, (s32)(work->projectileKind), arg0);
                    }
                    if (work->phaseFrame >= work->actionParam) {
                        sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_VOLLEY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        work->phaseFrame = 0;
                        work->animId     = 7;
                        return;
                    }
                    break;
                case 1:
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
            temp_v1_4 = work->step;
            switch (temp_v1_4) {
                case 0:
                    work->animId = 2U;
                    if ((func_actor_403600_80141840(arg0) & 0xFF) == 3) {
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->phaseFrame    = 0;
                        if (work->playerZone == 1) {
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
                case 1:
                    work->animId        = 1U;
                    temp_v0_16          = (-0x1770 - work->worldCoord.coord.t[1]) / 25;
                    work->verticalSpeed = temp_v0_16;
                    if (temp_v0_16 < 0xA) {
                        work->verticalSpeed = 0;
                        work->phaseFrame    = 0;
                        work->step          = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case 2:
                    work->aimMode = ACTOR_403600_AIM_TARGET;
                    func_actor_403600_8013DDF4(arg0, 0x20);
                    if (work->phaseFrame >= 0x28) {
                        work->animRate      = 0x20;
                        work->verticalSpeed = 0;
                        work->phaseFrame    = 0;
                        work->turnRate      = 0xA0;
                        work->step          = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case 3:
                    work->animId        = 0x10U;
                    work->committed     = 1;
                    work->forwardSpeed  = 0U;
                    work->verticalSpeed = 0;
                    func_actor_403600_8013DFE0(arg0);
                    if (work->phaseFrame >= 0x13) {
                        temp_s0_15 = &work->worldCoord;
                        temp_s4    = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x5416000B;
                        temp_s0_16 = (s8)worldCoordGetOriginAudioPan(temp_s0_15);
                        temp_v0_17 = worldCoordGetOriginAudioDepth(temp_s0_15);
                        sndEvtRequestScriptStart(temp_s4, temp_s0_16, (s32)(((temp_v0_17 >> 0x1F) + temp_v0_17) << 0x17) >> 0x18);
                        work->forwardSpeed  = 0x320U;
                        work->animRate      = 0x10;
                        work->ignorePushOut = 0;
                        work->actionParam   = 0;
                        work->phaseFrame    = 0;
                        work->turnRate      = 0x80;
                        work->diving        = 1;
                        work->step          = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case 4:
                    work->animId        = 0x12U;
                    work->forwardSpeed  = 0x320U;
                    work->verticalSpeed = 0;
                    work->turnRate      = 0xA0;
                    if (!((u16)work->phaseFrame & 1)) {
                        Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &arg0->extra.tmd->coords[1], 0, &D_actor_403600_80160664);
                        Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &arg0->extra.tmd->coords[15], 0x800, NULL);
                        Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &arg0->extra.tmd->coords[19], 0x800, NULL);
                    }
                    work->aimMode = ACTOR_403600_AIM_TARGET;
                    temp_s1       = func_actor_403600_8013DFE0(arg0);
                    temp_v1_5     = gPlayerStatus.coordMtx->t[0] - work->worldCoord.coord.t[0];
                    temp_lo       = temp_v1_5 * temp_v1_5;
                    temp_v1_6     = gPlayerStatus.coordMtx->t[1] - work->worldCoord.coord.t[1];
                    temp_lo_2     = temp_v1_6 * temp_v1_6;
                    temp_v1_7     = gPlayerStatus.coordMtx->t[2] - work->worldCoord.coord.t[2];
                    temp_v0_18    = SquareRoot0(temp_lo + temp_lo_2 + (temp_v1_7 * temp_v1_7));
                    sp10          = temp_v0_18;
                    if (temp_v0_18 < 0x76DU) {
                        temp_s0_msg = &D_actor_403600_80160568;
                        if (temp_s0_msg->animationId == 0) {
                            Gp_SpawnPadLerp(0x14, 0xFF, 0x50);
                            work->knockbackFrame = 0;
                            var_v0_8             = (s16)func_actor_403600_8013E66C(&work->worldCoord);
                            if (var_v0_8 < 0) {
                                var_v0_8 = -var_v0_8;
                            }
                            if (var_v0_8 >= 0x401) {
                                work->knockbackSpeed     = 0x28;
                                temp_s0_msg->animationId = 4;
                                TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, temp_s0_msg, 0);
                            } else {
                                work->knockbackSpeed     = -0x28;
                                temp_s0_msg->animationId = 3;
                                TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, temp_s0_msg, 0);
                            }
                            temp_s4    = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                            temp_s0_17 = (s8)worldCoordGetOriginAudioPan(temp_s6);
                            temp_v0_19 = worldCoordGetOriginAudioDepth(temp_s6);
                            sndEvtRequestScriptStart(temp_s4, temp_s0_17, (s32)(((temp_v0_19 >> 0x1F) + temp_v0_19) << 0x17) >> 0x18);
                            taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(&D_actor_403600_80150E9C, 0), 0);
                        }
                    }
                    if (temp_s1 < 0x3E9) {
                        work->animRate      = 0x10;
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
                case 5:
                    work->animId        = 0x11U;
                    work->verticalSpeed = 0;
                    work->aimMode       = ACTOR_403600_AIM_PLAYER_LEVEL;
                    work->turnRate      = 0x40;
                    func_actor_403600_8013DFE0(arg0);
                    if (work->phaseFrame >= 0x26) {
                        _actor403600ResetState(arg0);
                        work->action = ACTOR_403600_ACTION_RECHARGE;
                        return;
                    }
                    break;
            }
            break;
        case ACTOR_403600_ACTION_RUSH:
            temp_v1_8 = work->step;
            switch (temp_v1_8) {
                case 0:
                    work->aimMode       = ACTOR_403600_AIM_TARGET;
                    work->forwardSpeed  = 0xC8U;
                    adjusted_y0         = gPlayerStatus.coordMtx->t[1] + 0x1F4;
                    work->verticalSpeed = (s16)((adjusted_y0 - work->worldCoord.coord.t[1]) / 25);
                    if (func_actor_403600_8013DDF4(arg0, 0xB0) < 0x3E9) {
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->phaseFrame    = 0;
                        work->step          = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case 1:
                    work->aimMode = ACTOR_403600_AIM_PLAYER;
                    func_actor_403600_8013DDF4(arg0, 0x20);
                    work->forwardSpeed  = 0U;
                    work->verticalSpeed = 0;
                    func_actor_403600_8013E470(&work->worldCoord, &sp10, &sp14);
                    if (work->phaseFrame >= 0x32) {
                        work->phaseFrame = 0;
                        work->step       = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case 2:
                    func_shelter_b2_pod_bottom_80181A48(arg0);
                    work->animId = 0x13U;
                    if (work->phaseFrame == 0x19) {
                        temp_s0_18 = &work->worldCoord;
                        temp_s4    = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160015;
                        temp_s0_19 = (s8)worldCoordGetOriginAudioPan(temp_s0_18);
                        temp_v0_21 = worldCoordGetOriginAudioDepth(temp_s0_18);
                        sndEvtRequestScriptStart(temp_s4, temp_s0_19, (s32)(((temp_v0_21 >> 0x1F) + temp_v0_21) << 0x17) >> 0x18);
                    }
                    if (work->phaseFrame < 0x14) {
                        work->targetPos.vx = (s32)gPlayerStatus.coordMtx->t[0];
                        work->targetPos.vy = (s32)(gPlayerStatus.coordMtx->t[1] - 0x3E8);
                        work->targetPos.vz = (s32)gPlayerStatus.coordMtx->t[2];
                    }
                    if (work->phaseFrame >= 0x32) {
                        work->animId     = 0x10U;
                        work->aimMode    = ACTOR_403600_AIM_TARGET;
                        work->animRate   = 0x20;
                        work->phaseFrame = 0;
                        work->turnRate   = 0xA0;
                        work->step       = (s16)((u16)work->step + 1);
                        return;
                    }
                    break;
                case 3:
                    work->committed     = 1;
                    work->animId        = 0x10;
                    work->forwardSpeed  = 0U;
                    work->verticalSpeed = 0;
                    func_actor_403600_8013DFE0(arg0);
                    if (work->phaseFrame >= 0x13) {
                        work->animRate       = 0x10;
                        work->forwardSpeed   = 0x320U;
                        work->turnRate       = 0xA0;
                        work->chainSweep     = 0x7000;
                        work->animId         = 0x12U;
                        work->aimMode        = ACTOR_403600_AIM_TARGET;
                        work->actionParam    = 0;
                        work->phaseFrame     = 0;
                        work->roll           = 0;
                        work->gridHitLatched = 0;
                        work->actionTimer    = 0x96;
                        work->diving         = 1;
                        work->step           = (s16)((u16)work->step + 1);
                        temp_sound_28        = &work->worldCoord;
                        temp_s4              = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x5416000B;
                        temp_pan_28          = (s8)worldCoordGetOriginAudioPan(temp_sound_28);
                        temp_depth_28        = worldCoordGetOriginAudioDepth(temp_sound_28);
                        sndEvtRequestScriptStart(temp_s4, temp_pan_28, (s32)(((temp_depth_28 >> 0x1F) + temp_depth_28) << 0x17) >> 0x18);
                        return;
                    }
                    break;
                case 4:
                    if (!((u16)work->phaseFrame & 1)) {
                        if (work->actionParam != 0xFF) {
                            Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &arg0->extra.tmd->coords[1], 0, &D_actor_403600_80160664);
                            Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &arg0->extra.tmd->coords[15], 0x800, NULL);
                            Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_SHOCK_RING, &arg0->extra.tmd->coords[19], 0x800, NULL);
                        }
                    }
                    temp_s7->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
                    work->animId                    = 0x12U;
                    func_actor_403600_801417A8(arg0, 0xA);
                    temp_s0_20 = &work->worldCoord;
                    func_actor_403600_8013E470(temp_s0_20, &sp10, &sp14);
                    if (sp10 < 0x5DDU) {
                        temp_s1_3 = &D_actor_403600_80160568;
                        if (temp_s1_3->animationId == 0) {
                            if (work->actionParam == 0xFF) {
                                Gp_SpawnPadLerp(0xA, 0xFF, 0x50);
                                D_actor_403600_801606A4.reaction = 0xA;
                                temp_s1_3->animationId           = 1;
                                D_actor_403600_801606A4.power    = (u16)D_actor_403600_80150EAC;
                                func_actor_403600_8013E7D4(arg0, 1);
                                work->knockbackSpeed = -0x190;
                                TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, temp_s1_3, 0);
                                work->knockbackFrame = 0;
                                temp_s4              = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                                temp_s0_21           = (s8)worldCoordGetOriginAudioPan(temp_s6);
                                temp_v0_22           = worldCoordGetOriginAudioDepth(temp_s6);
                                sndEvtRequestScriptStart(temp_s4, temp_s0_21, (s32)(((temp_v0_22 >> 0x1F) + temp_v0_22) << 0x17) >> 0x18);
                                taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(&D_actor_403600_801606A4, 0), 0);
                            } else {
                                Gp_SpawnPadLerp(0x14, 0xB0, 0x50);
                                work->knockbackFrame = 0;
                                var_v0_10            = (s16)func_actor_403600_8013E66C(temp_s0_20);
                                if (var_v0_10 < 0) {
                                    var_v0_10 = -var_v0_10;
                                }
                                if (var_v0_10 >= 0x401) {
                                    work->knockbackSpeed   = 0x28;
                                    temp_s1_3->animationId = 4;
                                    TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, temp_s1_3, 0);
                                } else {
                                    work->knockbackSpeed   = -0x28;
                                    temp_s1_3->animationId = 3;
                                    TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, temp_s1_3, 0);
                                }
                                temp_s4    = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                                temp_s0_22 = (s8)worldCoordGetOriginAudioPan(temp_s6);
                                temp_v0_23 = worldCoordGetOriginAudioDepth(temp_s6);
                                sndEvtRequestScriptStart(temp_s4, temp_s0_22, (s32)(((temp_v0_23 >> 0x1F) + temp_v0_23) << 0x17) >> 0x18);
                                taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(&D_actor_403600_80150E9C, 4), 0);
                                work->actionParam = (s16)work->rushPasses;
                            }
                            work->actionTimer = 0x96;
                        }
                    }
                    temp_s1 = func_actor_403600_8013D9A8(arg0) & 0xFF;
                    if (temp_s1 == 2) {
                        taskSpawnFromTable(D_actor_403600_801421A0, 3, 0, arg0);
                    }
                    if ((temp_s1 == 3) && (work->actionParam == 0xFF)) {
                        work->step        = 6;
                        work->diving      = 0;
                        work->roll        = 0;
                        work->exposed     = 1;
                        work->damageTaken = 0;
                    }
                    if (temp_s1 == 1) {
                        work->diving        = 0;
                        work->repositioning = 1;
                        temp_s4             = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x5416000E;
                        temp_s0_23          = (s8)worldCoordGetOriginAudioPan(temp_s6);
                        temp_v0_24          = worldCoordGetOriginAudioDepth(temp_s6);
                        sndEvtRequestScriptStart(temp_s4, temp_s0_23, (s32)(((temp_v0_24 >> 0x1F) + temp_v0_24) << 0x17) >> 0x18);
                        taskSpawnFromTable(D_actor_403600_801421A0, 3, 1, arg0);
                        work->actionDelay               = 0;
                        work->gridBody.flags            = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
                        temp_s7->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                        worldTargetDisableNodeLockOn(&temp_s7->node);
                        work->phaseFrame    = 0;
                        work->step          = 5;
                        work->actionCounter = (u16)work->actionTimer;
                        return;
                    }
                    break;
                case 5:
                    func_actor_403600_801417A8(arg0, 0xA);
                    temp_v0_25        = (u16)work->actionDelay + 1;
                    work->actionDelay = temp_v0_25;
                    if (temp_v0_25 == 8) {
                        work->forwardSpeed = 0U;
                    }
                    if (work->actionDelay == (s16)work->actionCounter) {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B2_POD_BTM_ENEMY_RUMBLE_LOOP, 0, 0);
                        func_actor_403600_8013C864(arg0);
                        work->aimMode = ACTOR_403600_AIM_TARGET_SNAP;
                        func_actor_403600_8013DFE0(arg0);
                        taskSpawnFromTable(D_actor_403600_801421A0, 3, 2, arg0);
                    }
                    temp_v0_26 = gPlayerStatus.coordMtx->t[0] - work->worldCoord.coord.t[0];
                    temp_lo_3  = temp_v0_26 * temp_v0_26;
                    temp_v0_27 = gPlayerStatus.coordMtx->t[1] - work->worldCoord.coord.t[1];
                    temp_lo_4  = temp_v0_27 * temp_v0_27;
                    temp_v0_28 = gPlayerStatus.coordMtx->t[2] - work->worldCoord.coord.t[2];
                    sp10       = SquareRoot0(temp_lo_3 + temp_lo_4 + (temp_v0_28 * temp_v0_28));
                    if (work->phaseFrame >= 8) {
                        work->phaseFrame = 0;
                        if (sp10 < 0x3E9U) {
                            Gp_SpawnPadLerp(5, 0xB0, 0xB0);
                        } else if (sp10 < 0x7D1U) {
                            Gp_SpawnPadLerp(5, 0x80, 0x80);
                        } else if (sp10 < 0xBB9U) {
                            Gp_SpawnPadLerp(5, 0x50, 0x50);
                        }
                    }
                    if (work->actionDelay >= ((s16)work->actionCounter + 0x1E)) {
                        sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_RUMBLE_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_BOTTOM, 0x10), 0, 0);
                        work->forwardSpeed   = 0x320U;
                        work->turnRate       = 0xA0;
                        work->step           = 4;
                        work->phaseFrame     = 0;
                        work->gridHitLatched = 0;
                        work->diving         = 1;
                        work->repositioning  = 0;
                        return;
                    }
                    break;
                case 6:
                    work->animId       = 0x11U;
                    work->chainSweep   = 0;
                    work->forwardSpeed = 0U;
                    work->aimMode      = ACTOR_403600_AIM_PLAYER_LEVEL;
                    func_actor_403600_8013DFE0(arg0);
                    if (work->phaseFrame >= 0x26) {
                        _actor403600ResetState(arg0);
                        work->action = ACTOR_403600_ACTION_RECHARGE;
                        return;
                    }
                    break;
            }
            break;
        case ACTOR_403600_ACTION_SUMMON:
            temp_v1_9 = (s16)work->animId;
            switch (temp_v1_9) {
                case 2:
                    work->aimMode = ACTOR_403600_AIM_PLAYER;
                    func_actor_403600_8013DDF4(arg0, 0xA0);
                    work->forwardSpeed  = 0x12CU;
                    adjusted_y1         = gPlayerStatus.coordMtx->t[1] + 0x1F4;
                    work->verticalSpeed = (s16)((adjusted_y1 - work->worldCoord.coord.t[1]) / 25);
                    func_actor_403600_8013E470(&work->worldCoord, &sp10, &sp14);
                    if ((sp10 < 0x1389U) && (work->verticalSpeed < 0x12D)) {
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->phaseFrame    = 0;
                        work->animId        = 6U;
                        Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_CHARGE_BURST, &arg0->extra.tmd->coords[1], (s32)(work->actionParam), NULL);
                        return;
                    }
                    break;
                case 6:
                    work->forwardSpeed  = 0U;
                    work->verticalSpeed = 0;
                    if (work->phaseFrame == 1) {
                        temp_s0_24 = &work->worldCoord;
                        temp_s4    = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160014;
                        temp_s0_25 = (s8)worldCoordGetOriginAudioPan(temp_s0_24);
                        temp_v0_30 = worldCoordGetOriginAudioDepth(temp_s0_24);
                        sndEvtRequestScriptStart(temp_s4, temp_s0_25, (s32)(((temp_v0_30 >> 0x1F) + temp_v0_30) << 0x17) >> 0x18);
                    }
                    if (work->phaseFrame >= work->actionParam) {
                        work->phaseFrame = 0;
                        work->animId     = 8;
                        return;
                    }
                    break;
                case 8:
                    if ((work->phaseFrame == 1) && (work->childEnemy == 0)) {
                        work->childEnemy = Gp_SpawnEnemyFromTable(D_actor_403600_80160514, 1, worldTargetGetActorLockMask(&temp_s7->node), 0);
                    }
                    if (work->phaseFrame >= 0x1E) {
                        work->forwardSpeed  = -0xAU;
                        work->verticalSpeed = -0x14;
                    }
                    if (work->phaseFrame >= 0x42) {
                        work->phaseFrame = 0;
                        work->animId     = 3U;
                        case 3:
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
            temp_v1_10 = (s16)work->animId;
            switch (temp_v1_10) {
                case 2:
                    work->targetPos.vx = 0x1F40;
                    work->targetPos.vy = -0x1B58;
                    work->targetPos.vz = 0x1900;
                    if ((func_actor_403600_80141840(arg0) & 0xFF) == 3) {
                        work->drainPuffArg      = 0x100;
                        work->drainPuffFrames   = 0;
                        work->drainPuffInterval = 0x32;
                        work->damageTaken       = 0;
                        work->forwardSpeed      = 0U;
                        work->verticalSpeed     = 0;
                        work->phaseFrame        = 0;
                        work->animId            = 6U;
                        work->drainStartMp      = (u16)temp_wip->mp;
                        return;
                    }
                    break;
                case 6:
                    if (work->phaseFrame >= 0x2D) {
                        func_actor_403600_80141B60(arg0);
                        temp_a3                = (u16)work->phaseFrame;
                        work->screenDistortion = (u16)(work->screenDistortion + (0xC00 / (s16)work->actionParam));
                        if (work->phaseFrame == 0x30) {
                            Gp_SpawnPadLerp((s16)(((u16)work->actionParam - temp_a3) + 0x23), 0x40, 0xFF);
                            sndEvtRequestScriptStart(SOUND_SHELTER_B2_POD_BTM_ENEMY_DRAIN_WINDUP, 0, 0);
                        }
                    }
                    func_actor_403600_8013F608(arg0);
                    if (work->phaseFrame >= work->actionParam) {
                        work->phaseFrame = 0;
                        work->animId     = 0xAU;
                    }
                    if (work->damageTaken >= 0xC8) {
                        func_actor_403600_80141B24(arg0);
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->action        = ACTOR_403600_ACTION_CHOOSE;
                        work->phaseFrame    = 0;
                    }
                    return;
                case 10:
                    func_actor_403600_8013F608(arg0);
                    temp_v1_11 = work->phaseFrame;
                    if (temp_v1_11 == 0x23) {
                        Gp_SpawnPadLerp(0xA, 0xFF, 0xFF);
                        work->screenDistortion = 0x1000U;
                        temp_s4                = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                        temp_s0_26             = (s8)worldCoordGetOriginAudioPan(temp_s6);
                        temp_v0_31             = worldCoordGetOriginAudioDepth(temp_s6);
                        sndEvtRequestScriptStart(temp_s4, temp_s0_26, (s32)(((temp_v0_31 >> 0x1F) + temp_v0_31) << 0x17) >> 0x18);
                        taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(&D_actor_403600_80150E9C, 3), 0);
                        D_actor_403600_80160568.animationId = 4;
                        TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
                        temp_v0_32   = temp_wip->mp - 0xFB;
                        temp_wip->mp = temp_v0_32;
                        if ((temp_v0_32 << 0x10) <= 0) {
                            temp_wip->mp = 0U;
                        }
                        work->drainPuffInterval = -1;
                        work->knockbackFrame    = 0;
                        work->knockbackSpeed    = 0x28;
                        temp_s0_27              = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
                        temp_s4                 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160003;
                        temp_s0_28              = (s8)worldCoordGetOriginAudioPan(temp_s0_27);
                        temp_v0_33              = worldCoordGetOriginAudioDepth(temp_s0_27);
                        sndEvtRequestScriptStart(temp_s4, temp_s0_28, (s32)(((temp_v0_33 >> 0x1F) + temp_v0_33) << 0x17) >> 0x18);
                        sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_DRAIN_WINDUP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    } else if (temp_v1_11 < 0x23) {
                        func_actor_403600_80141B60(arg0);
                    }
                    temp_v0_34             = work->screenDistortion + 0xE;
                    work->screenDistortion = temp_v0_34;
                    if ((s16)temp_v0_34 >= 0x1000) {
                        work->screenDistortion = 0x1000U;
                    }
                    if (work->phaseFrame >= 0x45) {
                        if (temp_wip->mp <= 0) {
                            temp_wip->mp = 0U;
                        }
                        work->screenDistortion = 0U;
                        work->action           = ACTOR_403600_ACTION_CHOOSE;
                        work->phaseFrame       = 0;
                    }
                    if ((work->damageTaken >= 0xC8) && (work->phaseFrame < 0x32)) {
                        func_actor_403600_80141B24(arg0);
                        work->forwardSpeed  = 0U;
                        work->verticalSpeed = 0;
                        work->action        = ACTOR_403600_ACTION_CHOOSE;
                        work->phaseFrame    = 0;
                    }
                    break;
            }
            break;
        case ACTOR_403600_ACTION_MELEE:
            temp_v1_12 = (s16)work->animId;
            switch (temp_v1_12) {
                case 2:
                    work->aimMode = ACTOR_403600_AIM_PLAYER;
                    func_actor_403600_8013DDF4(arg0, 0x20);
                    func_actor_403600_8013E470(&work->worldCoord, &sp10, &sp14);
                    if (sp10 < 0x7D1U) {
                        work->forwardSpeed = 0U;
                    } else {
                        work->forwardSpeed = 0x50U;
                    }
                    adjusted_y2         = work->worldCoord.coord.t[1] + 0x3E8;
                    temp_a0_2           = (gPlayerStatus.coordMtx->t[1] - adjusted_y2) / 25;
                    work->verticalSpeed = temp_a0_2;
                    if (sp10 < 0x7D1U) {
                        var_v0_12 = sp14;
                        if (var_v0_12 < 0) {
                            var_v0_12 = -var_v0_12;
                        }
                        if (var_v0_12 < 0x200) {
                            var_v0_13 = temp_a0_2;
                            if (var_v0_13 < 0) {
                                var_v0_13 = -var_v0_13;
                            }
                            if (var_v0_13 < 0x28) {
                                work->forwardSpeed    = 0U;
                                work->verticalSpeed   = 0;
                                work->phaseFrame      = 0;
                                work->animId          = 0xCU;
                                work->animBlendFrames = 0;
                            }
                        }
                    }
                    temp_v0_35             = work->meleeChaseFrames + 1;
                    work->meleeChaseFrames = temp_v0_35;
                    if (((s16)temp_v0_35 >= 0x5A) || (sp10 >= 0xFA0U)) {
                        work->animBlendFrames = 8;
                        work->forwardSpeed    = 0U;
                        work->phaseFrame      = 0;
                        work->action          = ACTOR_403600_ACTION_CHOOSE;
                        work->meleeGaveUp     = 1;
                        return;
                    }
                    break;
                case 12:
                    work->animId = (u16)temp_v1_12;
                    if (work->phaseFrame == 0xE) {
                        temp_s0_29 = &work->worldCoord;
                        temp_s4    = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x5416000D;
                        temp_s0_30 = (s8)worldCoordGetOriginAudioPan(temp_s0_29);
                        temp_v0_36 = worldCoordGetOriginAudioDepth(temp_s0_29);
                        sndEvtRequestScriptStart(temp_s4, temp_s0_30, (s32)(((temp_v0_36 >> 0x1F) + temp_v0_36) << 0x17) >> 0x18);
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
                        work->animId       = 0xDU;
                        func_actor_403600_8013E470(&work->worldCoord, &sp10, &sp14);
                        if (sp10 >= 0x7D0U) {
                            work->animBlendFrames = 8;
                            work->forwardSpeed    = 0U;
                            work->phaseFrame      = 0;
                            work->action          = ACTOR_403600_ACTION_CHOOSE;
                        }
                    }
                    break;
                case 13:
                    work->animId = (u16)temp_v1_12;
                    if (work->phaseFrame == 9) {
                        temp_s0_31 = &work->worldCoord;
                        temp_s4    = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x5416000D;
                        temp_s0_32 = (s8)worldCoordGetOriginAudioPan(temp_s0_31);
                        temp_v0_37 = worldCoordGetOriginAudioDepth(temp_s0_31);
                        sndEvtRequestScriptStart(temp_s4, temp_s0_32, (s32)(((temp_v0_37 >> 0x1F) + temp_v0_37) << 0x17) >> 0x18);
                    }
                    if (work->phaseFrame == 0xA) {
                        work->attackBody.flags = (u16)(work->attackBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    }
                    if (work->phaseFrame == 0xE) {
                        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
                    }
                    if (work->phaseFrame >= 0x23) {
                        work->animBlendFrames = 8;
                        work->forwardSpeed    = 0U;
                        work->phaseFrame      = 0;
                        work->action          = ACTOR_403600_ACTION_CHOOSE;
                    }
            }
            break;
    }
}

/// Starts the arc point: a vector 0x3A98 minus the anchor's depth along Z, and
/// an identity turn matrix for `RotMatrixY` to rotate.
static inline void _actor403600ArcStart(_Actor403600RushPassScratch* s)
{
    GfxMatrix* m;

    s->offset.vx            = 0;
    s->offset.vy            = 0;
    m                       = &s->rotation;
    s->offset.vz            = 0x3A98 - D_actor_403600_801605D4.vz;
    m->rotationWords.m00M01 = ONE;
    m->rotationWords.m02M10 = 0;
    m->rotationWords.m11M12 = ONE;
    m->rotationWords.m20M21 = 0;
    m->rotationWords.m22    = ONE;
}

/// Rotates the arc vector by the turn matrix on the GTE and places the actor's
/// target at the result offset from the anchor, 0x3E8 above the player.
static inline void _actor403600ArcFinish(Actor403600Work* work, _Actor403600RushPassScratch* s)
{
    gte_SetRotMatrix(&s->rotation.mat);
    gte_ldv0(&s->offset);
    gte_rtv0();
    gte_stsv(&s->offset);
    work->worldCoord.coord.t[0] = s->offset.vx + D_actor_403600_801605D4.vx;
    work->worldCoord.coord.t[1] = gPlayerStatus.coordMtx->t[1] - 0x3E8;
    work->worldCoord.coord.t[2] = s->offset.vz + D_actor_403600_801605D4.vz;
}

static void func_actor_403600_8013C864(Task* arg0)
{
    Actor403600Work*             work;
    _Actor403600RushPassScratch* s;

    work = arg0->work;
    if (work->actionParam == work->rushPasses) {
        work->worldCoord.coord.t[0] = D_actor_403600_801605D4.vx;
        work->worldCoord.coord.t[1] = -0x960;
        work->worldCoord.coord.t[2] = D_actor_403600_801605D4.vz;
        work->targetPos.vx          = D_actor_403600_801605D4.vx;
        work->targetPos.vy          = -0x1F40;
        work->targetPos.vz          = D_actor_403600_801605D4.vz;
        work->actionParam           = 0xFF;
        return;
    }
    s = SCRATCH_STACK_RESERVE_BLOCK(_Actor403600RushPassScratch);
    if (!(work->actionParam & 1)) {
        s->offset.vx = gPlayerStatus.coordMtx->t[0] - (u16)D_actor_403600_801605D4.vx;
        s->offset.vz = gPlayerStatus.coordMtx->t[2] - (u16)D_actor_403600_801605D4.vz;
        s->bearing   = ratan2(s->offset.vx, s->offset.vz);
        if (ABS(s->bearing) > 0x800) {
            s->bearing = (s->bearing > 0) ? s->bearing - 0x1000 : 0x1000 - s->bearing;
        }
        if ((u32)(gPlayerStatus.coordMtx->t[0] - 0xDAC) < 0x2135 &&
            (u32)(gPlayerStatus.coordMtx->t[2] - 0x7D0) < 0x2711) {
            s->bearing = -s->bearing;
        }
        _actor403600ArcStart(s);
        RotMatrixY(s->bearing, &s->rotation.mat);
        _actor403600ArcFinish(work, s);
        if ((u32)(gPlayerStatus.coordMtx->t[0] - 0xDAC) < 0x2135 &&
            (u32)(gPlayerStatus.coordMtx->t[2] - 0x7D0) < 0x2711) {
            work->targetPos.vx = gPlayerStatus.coordMtx->t[0];
            work->targetPos.vy = gPlayerStatus.coordMtx->t[1] - 0x3E8;
            work->targetPos.vz = gPlayerStatus.coordMtx->t[2];
        } else {
            work->targetPos.vx = D_actor_403600_801605D4.vx;
            work->targetPos.vy = gPlayerStatus.coordMtx->t[1] - 0x3E8;
            work->targetPos.vz = D_actor_403600_801605D4.vz;
        }
        work->rushAngle = s->bearing + 0x800;
        if (ABS(work->rushAngle) > 0x800) {
            work->rushAngle = (work->rushAngle > 0) ? work->rushAngle - 0x1000 : 0x1000 - work->rushAngle;
        }
        work->actionTimer = 0xA;
        work->actionParam++;
    } else {
        _actor403600ArcStart(s);
        RotMatrixY(work->rushAngle, &s->rotation.mat);
        _actor403600ArcFinish(work, s);
        work->actionTimer = 0x96;
        work->actionParam++;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403600RushPassScratch);
}

static void func_actor_403600_8013CCEC(Task* arg0, s32 arg1)
{
    s32                              temp_a0;
    s32                              temp_v0;
    s32                              temp_v0_10;
    s32                              temp_v0_2;
    s32                              temp_v0_3;
    s32                              temp_v0_4;
    s32                              temp_v0_5;
    s32                              temp_v0_6;
    s32                              temp_v0_7;
    s32                              temp_v0_8;
    s32                              temp_v0_9;
    s32                              temp_v1;
    s32                              temp_v1_2;
    s32                              temp_v1_3;
    s32                              temp_v1_4;
    s32                              temp_v1_5;
    s32                              temp_v1_6;
    s32                              temp_v1_7;
    s32                              temp_v1_8;
    s32                              temp_v1_9;
    s32                              var_a1;
    s32                              var_a2;
    s32                              var_v1;
    _Actor403600NearestPointScratch* scratch;
    Actor403600Work*                 work;
    SVECTOR*                         temp_v0_11;
    SVECTOR*                         temp_v1_10;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403600NearestPointScratch);
    work    = arg0->work;
    if (arg1 == 0) {
        scratch->deltaX       = gPlayerStatus.coordMtx->t[0] - D_actor_403600_801605F4[0].vx;
        temp_v1               = gPlayerStatus.coordMtx->t[2] - D_actor_403600_801605F4[0].vz;
        scratch->deltaZ       = temp_v1;
        temp_v0               = scratch->deltaX;
        scratch->distances[0] = SquareRoot0((temp_v0 * temp_v0) + (temp_v1 * temp_v1));

        scratch->deltaX       = gPlayerStatus.coordMtx->t[0] - D_actor_403600_801605F4[1].vx;
        temp_v1_2             = gPlayerStatus.coordMtx->t[2] - D_actor_403600_801605F4[1].vz;
        scratch->deltaZ       = temp_v1_2;
        temp_v0_2             = scratch->deltaX;
        scratch->distances[1] = SquareRoot0((temp_v0_2 * temp_v0_2) + (temp_v1_2 * temp_v1_2));

        scratch->deltaX       = gPlayerStatus.coordMtx->t[0] - D_actor_403600_801605F4[2].vx;
        temp_v1_3             = gPlayerStatus.coordMtx->t[2] - D_actor_403600_801605F4[2].vz;
        scratch->deltaZ       = temp_v1_3;
        temp_v0_3             = scratch->deltaX;
        scratch->distances[2] = SquareRoot0((temp_v0_3 * temp_v0_3) + (temp_v1_3 * temp_v1_3));

        scratch->deltaX       = gPlayerStatus.coordMtx->t[0] - D_actor_403600_801605F4[3].vx;
        temp_v1_4             = gPlayerStatus.coordMtx->t[2] - D_actor_403600_801605F4[3].vz;
        scratch->deltaZ       = temp_v1_4;
        temp_v0_4             = scratch->deltaX;
        scratch->distances[3] = SquareRoot0((temp_v0_4 * temp_v0_4) + (temp_v1_4 * temp_v1_4));

        scratch->deltaX       = gPlayerStatus.coordMtx->t[0] - D_actor_403600_801605F4[4].vx;
        temp_v1_5             = gPlayerStatus.coordMtx->t[2] - D_actor_403600_801605F4[4].vz;
        scratch->deltaZ       = temp_v1_5;
        temp_v0_5             = scratch->deltaX;
        scratch->distances[4] = SquareRoot0((temp_v0_5 * temp_v0_5) + (temp_v1_5 * temp_v1_5));

        scratch->deltaX       = gPlayerStatus.coordMtx->t[0] - D_actor_403600_801605F4[5].vx;
        temp_v1_6             = gPlayerStatus.coordMtx->t[2] - D_actor_403600_801605F4[5].vz;
        scratch->deltaZ       = temp_v1_6;
        temp_v0_6             = scratch->deltaX;
        scratch->distances[5] = SquareRoot0((temp_v0_6 * temp_v0_6) + (temp_v1_6 * temp_v1_6));

        scratch->deltaX       = gPlayerStatus.coordMtx->t[0] - D_actor_403600_801605F4[6].vx;
        temp_v1_7             = gPlayerStatus.coordMtx->t[2] - D_actor_403600_801605F4[6].vz;
        scratch->deltaZ       = temp_v1_7;
        temp_v0_7             = scratch->deltaX;
        scratch->distances[6] = SquareRoot0((temp_v0_7 * temp_v0_7) + (temp_v1_7 * temp_v1_7));
        var_a2                = 0xFFFFFF;
        var_v1                = 0;
        var_a1                = 0;
        do {
            temp_a0 = scratch->distances[var_a1 & 0xFF];
            if (temp_a0 < var_a2) {
                var_v1 = var_a1;
                var_a2 = temp_a0;
            }
            var_a1 += 1;
        } while ((u32)(var_a1 & 0xFF) < 7U);
    } else {
        scratch->deltaX       = gPlayerStatus.coordMtx->t[0] - D_actor_403600_801605F4[7].vx;
        temp_v1_8             = gPlayerStatus.coordMtx->t[2] - D_actor_403600_801605F4[7].vz;
        scratch->deltaZ       = temp_v1_8;
        temp_v0_8             = scratch->deltaX;
        scratch->distances[0] = SquareRoot0((temp_v0_8 * temp_v0_8) + (temp_v1_8 * temp_v1_8));

        scratch->deltaX       = gPlayerStatus.coordMtx->t[0] - D_actor_403600_801605F4[8].vx;
        temp_v1_9             = gPlayerStatus.coordMtx->t[2] - D_actor_403600_801605F4[8].vz;
        scratch->deltaZ       = temp_v1_9;
        temp_v0_9             = scratch->deltaX;
        temp_v0_10            = SquareRoot0((temp_v0_9 * temp_v0_9) + (temp_v1_9 * temp_v1_9));
        scratch->distances[1] = temp_v0_10;
        var_v1                = temp_v0_10 >= scratch->distances[0];
    }
    temp_v0_11 = D_actor_403600_801605F4;
    temp_v1_10 = (var_v1 & 0xFF) + temp_v0_11;
    do {
        work->targetPos.vx = temp_v1_10->vx;
    } while (0);
    work->targetPos.vy = gPlayerStatus.coordMtx->t[1] - 0x258;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403600NearestPointScratch);
    work->targetPos.vz = temp_v1_10->vz;
}

/// Steps the shared LCG and returns the upper half of the new state.
static inline u32 _actor403600Rand(void)
{
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    return gRandomLcgState >> 16;
}

static void func_actor_403600_8013D15C(Task* arg0)
{
    s32                           sp10;
    s32                           sp14;
    Actor403600Work*              work;
    Enemy*                        enemy;
    ActorContactDeltaWideScratch* scratch;
    WorldCollisionContact*        other;
    s32                           i;
    s32                           j;
    s32                           dx;
    s32                           dy;
    s32                           dz;
    s16                           hitKind;
    s32                           damage;
    s32                           key;
    s16                           stun;
    s32                           hpMax;

    work    = arg0->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorContactDeltaWideScratch);
    enemy   = arg0->spawnArg2.pointer;
    switch (func_800E0C10(work->hitContacts, &scratch->delta, 4, 0)) {
        case 0:
            break;
        case 1:
            if ((arg0 == D_actor_403600_801606A8) && (work->ignorePushOut == 0)) {
                work->worldCoord.coord.t[0] += scratch->delta.fixed.vx.halves.integer;
                work->worldCoord.coord.t[1] += scratch->delta.fixed.vy.halves.integer;
                work->worldCoord.coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            }
            break;
        case 2:
            if ((arg0 == D_actor_403600_801606A8) && (work->ignorePushOut == 0)) {
                work->worldCoord.coord.t[0] += scratch->delta.fixed.vx.halves.integer;
                work->worldCoord.coord.t[1] += scratch->delta.fixed.vy.halves.integer;
                work->worldCoord.coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            }
            break;
    }
    if (work->hitCooldown != 0) {
        work->hitCooldown--;
        if (work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    for (i = 0; i < 4; i++) {
        if ((u16)(work->hitContacts[i].key.value >> 16) == 1) {
            continue;
        }
        if ((u16)(work->hitContacts[i].key.value >> 16) != 2) {
            continue;
        }
        if (work->hitCooldown != 0) {
            continue;
        }
        dx                       = gPlayerStatus.coordMtx->t[0] - work->worldCoord.coord.t[0];
        scratch->delta.vector.vx = dx;
        dy                       = gPlayerStatus.coordMtx->t[1] - 2000;
        dy                      -= work->worldCoord.coord.t[1];
        scratch->delta.vector.vy = dy;
        dz                       = gPlayerStatus.coordMtx->t[2] - work->worldCoord.coord.t[2];
        hitKind                  = 0;
        scratch->delta.vector.vz = dz;
        damage                   = Gp_ComputeDamage(work->hitContacts[i].key.value,
                                                    SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz),
                                                    0, 0);
        if (damageRollCriticalHit(arg0->spawnArg2.pointer, work->hitContacts[i].key.value, 0) != 0) {
            hitKind = 1;
            damage *= 4;
        }
        if (work->exposed != 0) {
            hitKind = 2;
            damage *= 2;
            if (work->damageTaken > 200) {
                work->exposed = 0;
            }
        }
        if (hitKind == 1) {
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[1], 0, 0);
        } else if (hitKind == 2) {
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[1], 3, 0);
        }
        switch ((u16)Gp_GetIdParam0(work->hitContacts[i].key.value)) {
            case 0:
                break;
            case 2:
                if (work->committed == 0) {
                    work->exposed = 0;
                    Gp_SetObjFlag2(arg0->spawnArg2.pointer, work->hitContacts[i].key.value, 0);
                    if (((u16)work->animId - 0x10 < 2U) && ((u16)work->phaseFrame - 6 < 0x18U)) {
                        work->stunFrames = D_actor_403600_80150EC8.buildupSteps;
                    }
                    if ((work->animId == 1) && (work->phaseFrame < 30)) {
                        work->stunFrames = D_actor_403600_80150EC8.buildupSteps;
                    }
                }
                break;
            case 3:
                if ((work->committed == 0) && (enemy->hp > 500)) {
                    Gp_SetObjFlag4(arg0->spawnArg2.pointer, work->hitContacts[i].key.value, 0);
                }
                if (work->hitContacts[i].key.value & 8) {
                    if (_actor403600Rand() & 1) {
                        work->damageTaken = 200;
                    }
                    if ((u16)work->animId - 0x10 < 2U) {
                        work->exposed = 0;
                        if (work->phaseFrame < 30) {
                            work->mode       = ACTOR_403600_MODE_FREEZE;
                            work->stunFrames = D_actor_403600_80150EC8.buildupSteps * 10;
                        }
                    }
                }
                break;
            case 1:
            case 4:
                if (work->exposed != 0) {
                    Gp_SetObjFlag1(enemy);
                    work->exposed = 0;
                    if (((u16)(_actor403600Rand() % 10) == 0) && (work->weakPhase == 0) && (work->repositioning == 0)) {
                        work->weakFrames      = 0;
                        work->phaseFrame      = 0;
                        work->mode            = ACTOR_403600_MODE_WEAKEN;
                        enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    }
                }
            case 6:
            case 7:
            case 9:
                hpMax = work->hpMax;
                if (enemy->hp < hpMax / 10) {
                    work->exposed = 0;
                    if ((work->weakPhase == 0) && (work->repositioning == 0)) {
                        work->weakFrames      = 0;
                        work->phaseFrame      = 0;
                        work->mode            = ACTOR_403600_MODE_WEAKEN;
                        enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    }
                }
                break;
        }
        if (work->diving != 0) {
            key = work->hitContacts[i].key.value;
            if ((key == 0x28003) || (key == 0x28006) || (key == 0x2800F)) {
                work->exposed = 0;
                if ((work->weakPhase == 0) && (work->repositioning == 0)) {
                    work->weakFrames      = 0;
                    work->phaseFrame      = 0;
                    work->mode            = ACTOR_403600_MODE_WEAKEN;
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                }
            }
        }
        if (arg0 != D_actor_403600_801606A8) {
            damageAccumulateLifeDrainHp(enemy, work->hitContacts[i].key.value, damage, 0);
            if (work->hitContacts[i].key.value & 8) {
                if ((_actor403600Rand() & 3) == 0) {
                    Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[1], 3, 0);
                    worldTargetAddReadoutAmount(&enemy->node, 999, 0);
                    work->defeated = 1;
                    return;
                }
            }
            func_actor_403600_80141C7C(arg0, damage);
        } else {
            damageAccumulateLifeDrainHp(enemy, work->hitContacts[i].key.value, damage, 0);
            func_actor_403600_8013DAF4(arg0, damage);
            if (work->mode == ACTOR_403600_MODE_FIGHT) {
                for (j = 0; j < ARRAY_SIZE(D_actor_403600_8016066C); j++) {
                    if (damage >= D_actor_403600_8016066C[j].minDamage) {
                        work->recoilSpeed = D_actor_403600_8016066C[j].recoilSpeed;
                        work->recoilHold  = D_actor_403600_8016066C[j].recoilHold;
                    }
                }
            }
        }
        work->damageTaken += damage;
        if ((enemy->hp > 0) && (arg0 == D_actor_403600_801606A8)) {
            func_800FDB18((u16)Gp_GetIdParam1(work->hitContacts[i].key.value), &work->worldCoord, &work->hitEffectOffset, &work->hitEffectArg);
        }
        stun = Gp_GetIdParam2(work->hitContacts[i].key.value);
        if (stun > 0) {
            work->hitCooldown = stun;
        }
        func_actor_403600_8013E470(&work->worldCoord, &sp10, &sp14);
        if (__builtin_abs(sp14) <= 0x400) {
            work->flinchRot.vx = ((_actor403600Rand() & 3) << 5) + 0x80;
        } else {
            work->flinchRot.vx = -(((_actor403600Rand() & 3) << 5) + 0x80);
            work->recoilSpeed *= -1;
        }
    }
    worldCollisionClearContacts(work->hitContacts);
    other = work->attackContacts;
    if (worldCollisionFindContactIndex(other, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        worldCollisionClearContacts(other);
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactDeltaWideScratch);
}

static s32 func_actor_403600_8013D9A8(Task* arg0)
{
    s32              i;
    s32              x;
    s32              y;
    s32              z;
    Actor403600Work* work;

    work = arg0->work;
    for (i = 0; i < 4; i++) {
        if (((u32)(work->gridContacts[i].key.value & 0xFFFF0000) >> 16) == 0x10) {
            if (work->gridHitLatched == 0) {
                work->gridHitLatched++;
                if (worldCollisionSurfaceClassFromKey(work->gridContacts[i].key.value) == 3) {
                    return 2;
                }
            }
        }
    }

    worldCollisionClearContacts(work->gridContacts);
    x = work->worldCoord.coord.t[0] + ((work->worldCoord.coord.m[0][2] * 0x177) >> 9);
    y = work->worldCoord.coord.t[2] + ((work->worldCoord.coord.m[2][2] * 0x177) >> 9);
    if (x < 0x101) {
        return 1;
    }
    if ((x >= 0x3D00) || (y >= 0x3800)) {
        return 1;
    }
    if (y < -0x7F) {
        return 1;
    }
    z = work->worldCoord.coord.t[1];
    if (z >= 0) {
        return 1;
    }
    if (z < -0x176F) {
        return 3;
    }
    return 0;
}

static void func_actor_403600_8013DAF4(Task* arg0, s32 arg1)
{
    Enemy*           temp_v0;
    Enemy*           temp_s0;
    Actor403600Work* work;
    Task*            temp_v0_2;
    Actor403600Work* temp_v0_3;

    temp_s0     = arg0->spawnArg2.pointer;
    work        = arg0->work;
    temp_s0->hp = (u16)temp_s0->hp - arg1;
    worldTargetAddReadoutAmount(&temp_s0->node, arg1, 0);
    if (temp_s0->hp <= 0) {
        if (gPlayerStatus.hp <= 0) {
            temp_s0->hp = 0xA;
            return;
        }
        temp_v0 = work->childEnemy;
        if (temp_v0 != NULL) {
            temp_v0_2                                 = temp_v0->task;
            temp_v0_2->state                          = 2;
            temp_v0_2->killCountdown                  = 0;
            ((Actor403600Work*)temp_v0_2->work)->step = 1;
        }
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_PulseState1C80();
        gGameSession->eventState            = 1;
        D_actor_403600_80160568.animationId = 0;
        taskMessageDispatch(*gPlayerActorTasks, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        temp_v0_3                  = arg0->work;
        temp_v0_3->animBlendFrames = 8;
        temp_v0_3->actionDelay     = 0xA;
        temp_v0_3->defeated        = 0;
        temp_v0_3->aimMode         = ACTOR_403600_AIM_PLAYER;
        temp_v0_3->ignorePushOut   = 0;
        temp_v0_3->animRate        = 0x10;
        temp_v0_3->ambientBoost    = 0;
        temp_v0_3->committed       = 0;
        temp_v0_3->forwardSpeed    = 0;
        temp_v0_3->action          = ACTOR_403600_ACTION_CHOOSE;
        temp_v0_3->verticalSpeed   = 0;
        temp_v0_3->phaseFrame      = 0;
        temp_v0_3->turnRate        = 0x40;
        temp_v0_3->roll            = 0;
        temp_v0_3->diving          = 0;
        temp_v0_3->repositioning   = 0;
        temp_v0_3->pauseSoundSent  = 0;
        func_actor_403600_80141954(0);
        func_actor_403600_80141B24(arg0);
        work->defeated        = 1;
        work->animId          = 1;
        work->animBlendFrames = 0;
        work->animRate        = 0x10;
        work->mode            = ACTOR_403600_MODE_DYING;
        work->step            = 0;
        work->whiteout        = 0;
        Gp_HaltPadScripts();
        sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_DRAIN_WINDUP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
}

static void func_actor_403600_8013DC7C(Task* arg0)
{
    s16              temp_v0_2;
    s16              temp_v0;
    u16              var_a3;
    s16              var_a3_signed;
    Actor403600Work* work;

    work             = arg0->work;
    var_a3           = work->forwardSpeed;
    work->prevPos.vx = work->worldCoord.coord.t[0];
    work->prevPos.vy = work->worldCoord.coord.t[1];
    work->prevPos.vz = work->worldCoord.coord.t[2];
    var_a3_signed    = var_a3;
    if ((work->weakPhase != 0) && (var_a3_signed != 0)) {
        var_a3 = (var_a3_signed * 0x3C) / 100;
    }
    if (work->committed != 0) {
        work->worldCoord.coord.t[0] +=
            (work->worldCoord.coord.m[0][2] * ((s16)var_a3 - work->recoilSpeed)) >> 0xC;
        work->worldCoord.coord.t[1] +=
            (work->worldCoord.coord.m[1][2] * ((s16)var_a3 - work->recoilSpeed)) >> 0xC;
        work->worldCoord.coord.t[2] +=
            (work->worldCoord.coord.m[2][2] * ((s16)var_a3 - work->recoilSpeed)) >> 0xC;
    } else {
        work->worldCoord.coord.t[1] += work->verticalSpeed;
        work->worldCoord.coord.t[0] +=
            (work->worldCoord.coord.m[0][2] * ((s16)var_a3 - work->recoilSpeed)) >> 0xC;
        work->worldCoord.coord.t[2] +=
            (work->worldCoord.coord.m[2][2] * ((s16)var_a3 - work->recoilSpeed)) >> 0xC;
    }
    temp_v0          = work->recoilHold - 1;
    work->recoilHold = temp_v0;
    if (temp_v0 < 0) {
        temp_v0_2         = (u16)work->recoilSpeed - 1;
        work->recoilSpeed = temp_v0_2;
        if (temp_v0_2 < 0) {
            work->recoilSpeed = 0;
        }
        work->recoilHold = 0;
    }
}

static s32 func_actor_403600_8013DDF4(Task* arg0, s16 arg1)
{
    Actor403600Work*  work;
    ActorFaceScratch* scratch;
    ActorFaceScratch* oldHead;
    s16               step;
    s32               distance;
    u16               angle;
    s32               rawDiff;
    s32               adiff;
    s32               turnDiff;
    s32               next;

    step    = arg1;
    oldHead = SCRATCH_STACK_CURSOR(ActorFaceScratch);
    scratch = (SCRATCH_STACK_CURSOR(ActorFaceScratch) =
                   oldHead - 1);
    work    = arg0->work;
    if ((arg1 << 0x10) == 0) {
        step = 0x20;
    }

    switch (work->aimMode) {
        case ACTOR_403600_AIM_PLAYER:
            oldHead[-1].delta.vx = gPlayerStatus.coordMtx->t[0] - work->worldCoord.coord.t[0];
            scratch->delta.vy    = 0;
            scratch->delta.vz    = gPlayerStatus.coordMtx->t[2] - work->worldCoord.coord.t[2];
            break;
        case ACTOR_403600_AIM_TARGET:
            oldHead[-1].delta.vx = work->targetPos.vx - work->worldCoord.coord.t[0];
            scratch->delta.vy    = 0;
            scratch->delta.vz    = work->targetPos.vz - work->worldCoord.coord.t[2];
            break;
    }

    distance = SquareRoot0((scratch->delta.vx * scratch->delta.vx) +
                           (scratch->delta.vy * scratch->delta.vy) +
                           (scratch->delta.vz * scratch->delta.vz));
    angle    = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & 0xFFF;
    rawDiff  = angle - (work->yaw & 0xFFF);
    adiff    = __builtin_abs((s16)rawDiff);
    turnDiff = rawDiff;
    if (step >= adiff) {
        work->yaw = angle;
    } else {
        if (adiff >= 0x801) {
            next = rawDiff - 0x1000;
            if ((s16)rawDiff <= 0) {
                next = 0x1000 - rawDiff;
            }
            turnDiff = next;
        }
        rawDiff = (s16)work->yaw;
        if ((turnDiff << 0x10) > 0) {
            next = rawDiff + step;
        } else {
            next = (s16)work->yaw - step;
        }
        work->yaw = next;
    }

    scratch->rot.vx = 0;
    scratch->rot.vy = work->yaw;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &work->worldCoord.coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
    return distance;
}

static s32 func_actor_403600_8013DFE0(Task* arg0)
{
    s16                         temp_v0;
    s16                         temp_v0_2;
    s16                         temp_v0_3;
    s16                         temp_v1_3;
    s16                         diff;
    s16                         turnDiff;
    s16                         wrapped;
    s32                         angle;
    s32                         stepped;
    s32                         temp_lo;
    s32                         temp_s5;
    Actor403600Work*            work;
    _Actor403600AimTurnScratch* scratch;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(_Actor403600AimTurnScratch);
    work      = arg0->work;
    temp_v1_3 = work->aimMode;
    switch (temp_v1_3) {
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
            temp_s5               = (s16)(gPlayerStatus.coordMtx->t[0] - work->worldCoord.coord.t[0]);
            scratch->direction.vy = 0;
            scratch->direction.vx = temp_s5;
            scratch->direction.vz =
                (s16)(gPlayerStatus.coordMtx->t[2] - work->worldCoord.coord.t[2]);
            break;
    }
    temp_v0   = scratch->direction.vx;
    temp_v0_2 = scratch->direction.vy;
    temp_v0_3 = scratch->direction.vz;
    temp_lo   = temp_v0_3 * temp_v0_3;
    temp_s5   = SquareRoot0((temp_v0 * temp_v0) + (temp_v0_2 * temp_v0_2) + temp_lo);
    if (work->aimMode == ACTOR_403600_AIM_TARGET_SNAP) {
        gfxSetRotIdentity(&scratch->basis);
        VectorNormalSS(&scratch->direction, &scratch->direction);
        scratch->angles.vx = 0;
        scratch->angles.vy = ONE;
        scratch->angles.vz = 0;
        gfxBuildOrthonormalBasis(&scratch->basis, &scratch->direction, &scratch->angles);
        gfxMatrixToEuler(&scratch->basis, &scratch->angles);
        scratch->angles.vz += work->roll;
        gfxRotMatrixXYZ(&work->worldCoord.coord, &scratch->angles, GRAPHICS_ROTATION_REPLACE);
        gfxReadMatrixZAxis(&work->worldCoord.coord, &scratch->angles);
    } else {
        gfxSetRotIdentity(&scratch->basis);
        VectorNormalSS(&scratch->direction, &scratch->direction);
        scratch->angles.vx = 0;
        scratch->angles.vy = ONE;
        scratch->angles.vz = 0;
        gfxBuildOrthonormalBasis(&scratch->basis, &scratch->direction, &scratch->angles);
        gfxMatrixToEuler(&scratch->basis, &scratch->angles);
        // The direction is spent: its slot takes the boss's own angles, which
        // step toward the basis's one axis at a time.
        gfxMatrixToEuler(&work->worldCoord.coord, &scratch->direction);
        diff     = (scratch->angles.vx & 0xFFF) - (scratch->direction.vx & 0xFFF);
        turnDiff = diff;
        if (work->turnRate >= __builtin_abs(diff)) {
            scratch->direction.vx = scratch->angles.vx;
        } else {
            if (__builtin_abs(diff) > 0x800) {
                wrapped = diff - 0x1000;
                if (diff <= 0) {
                    wrapped = 0x1000 - diff;
                }
                turnDiff = wrapped;
            }
            angle = scratch->direction.vx;
            if (turnDiff > 0) {
                stepped = angle + work->turnRate;
            } else {
                stepped = angle - work->turnRate;
            }
            scratch->direction.vx = stepped;
        }

        diff     = (scratch->angles.vy & 0xFFF) - (scratch->direction.vy & 0xFFF);
        turnDiff = diff;
        if (work->turnRate >= __builtin_abs(diff)) {
            scratch->direction.vy = scratch->angles.vy;
        } else {
            if (__builtin_abs(diff) > 0x800) {
                wrapped = diff - 0x1000;
                if (diff <= 0) {
                    wrapped = 0x1000 - diff;
                }
                turnDiff = wrapped;
            }
            angle = scratch->direction.vy;
            if (turnDiff > 0) {
                stepped = angle + work->turnRate;
            } else {
                stepped = angle - work->turnRate;
            }
            scratch->direction.vy = stepped;
        }

        diff     = (scratch->angles.vz & 0xFFF) - (scratch->direction.vz & 0xFFF);
        turnDiff = diff;
        if (work->turnRate >= __builtin_abs(diff)) {
            scratch->direction.vz = scratch->angles.vz;
        } else {
            if (__builtin_abs(diff) > 0x800) {
                wrapped = diff - 0x1000;
                if (diff <= 0) {
                    wrapped = 0x1000 - diff;
                }
                turnDiff = wrapped;
            }
            angle = scratch->direction.vz;
            if (turnDiff > 0) {
                stepped = angle + work->turnRate;
            } else {
                stepped = angle - work->turnRate;
            }
            scratch->direction.vz = stepped;
        }
        scratch->direction.vz += work->roll;
        gfxRotMatrixXYZ(&work->worldCoord.coord, &scratch->direction, GRAPHICS_ROTATION_REPLACE);
        work->yaw = scratch->direction.vy;
        gfxReadMatrixZAxis(&work->worldCoord.coord, &scratch->angles);
    }
    // Either turn leaves the Z axis of the rebuilt rotation in `angles`.
    work->yaw = ratan2(scratch->angles.vx, scratch->angles.vz);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403600AimTurnScratch);
    return temp_s5;
}

static void func_actor_403600_8013E470(GfxCoord* arg0, s32* arg1, s32* arg2)
{
    GfxCoord*                 coord;
    s32                       angle;
    s32                       x;
    s32                       z;
    ActorRangeBearingScratch* head;
    SVECTOR*                  vec;
    MATRIX*                   matrix;
    ActorRangeBearingScratch* scratch;

    head                      = SCRATCH_STACK_CURSOR(void);
    coord                     = (*gPlayerActorTasks)->extra.tmd->coords;
    head[-1].bearing.delta.vx = (s16)(coord->workm.t[0] - arg0->workm.t[0]);
    vec                       = &head[-1].bearing.delta;
    vec->vy                   = (s16)(coord->workm.t[1] - arg0->workm.t[1]);
    scratch                   = (SCRATCH_STACK_CURSOR(void) = &head[-1]);
    vec->vz                   = (s16)(coord->workm.t[2] - arg0->workm.t[2]);
    matrix                    = &head[-1].bearing.inverseRotation;
    TransposeMatrix(&arg0->workm, matrix);
    _gfxLoadRotSv(matrix, vec);
    gte_rtv0();
    gte_stsv(vec);
    angle = ratan2(head[-1].bearing.delta.vx, vec->vz);
    *arg2 = angle;
    if (angle >= 0x801) {
        *arg2 = angle - 0x1000;
    } else if (angle < -0x800) {
        *arg2 = angle + 0x1000;
    }
    x                  = gPlayerStatus.coordMtx->t[0] - arg0->coord.t[0];
    scratch->offset.vx = x;
    scratch->offset.vy = gPlayerStatus.coordMtx->t[1] - arg0->coord.t[1];
    z                  = gPlayerStatus.coordMtx->t[2] - arg0->coord.t[2];
    scratch->offset.vz = z;
    *arg1              = SquareRoot0((x * x) + (z * z));
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorRangeBearingScratch));
}

static s16 func_actor_403600_8013E66C(GfxCoord* arg0)
{
    GfxCoord*                 coord;
    s16                       angle;
    s16                       result;
    SVECTOR*                  vec;
    ActorRangeBearingScratch* head;

    head                       = SCRATCH_STACK_CURSOR(void);
    coord                      = (*gPlayerActorTasks)->extra.tmd->coords;
    SCRATCH_STACK_CURSOR(void) = &head[-1];
    head[-1].bearing.delta.vx  = (s16)(arg0->workm.t[0] - coord->workm.t[0]);
    vec                        = &head[-1].bearing.delta;
    vec->vy                    = (s16)(arg0->workm.t[1] - coord->workm.t[1]);
    vec->vz                    = (s16)(arg0->workm.t[2] - coord->workm.t[2]);
    TransposeMatrix(&coord->workm, &head[-1].bearing.inverseRotation);
    _gfxRotateSv(&head[-1].bearing.inverseRotation, vec);
    angle  = ratan2(head[-1].bearing.delta.vx, vec->vz);
    result = angle;
    if (angle >= 0x801) {
        result = angle - 0x1000;
    } else if (angle < -0x800) {
        result = angle + 0x1000;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorRangeBearingScratch));
    return result;
}

static s32 func_actor_403600_8013E7D4(Task* arg0, u16 arg1)
{
    Task*     temp_s7;
    GfxCoord* temp_s3;
    s32       temp_s0;
    s32       temp_s0_3;
    s32       temp_s1;
    s32       temp_s5;
    s32       var_s2;
    s32       var_s4;
    s32       var_v1;

    temp_s7 = *gPlayerActorTasks;
    temp_s3 = temp_s7->extra.tmd->coords;
    temp_s1 = D_actor_403600_801605E4.vx - temp_s3->coord.t[0];
    temp_s0 = D_actor_403600_801605E4.vz - temp_s3->coord.t[2];
    var_s4  = 0;
    temp_s5 = SquareRoot0((temp_s1 * temp_s1) + (temp_s0 * temp_s0));
    var_s2  = ratan2(temp_s1, temp_s0);
    if (var_s2 >= 0x801) {
        var_s2 -= 0x1000;
    } else if (var_s2 < -0x800) {
        var_s2 += 0x1000;
    }
    temp_s1   = D_actor_403600_801605EC.vx - temp_s3->coord.t[0];
    temp_s0   = D_actor_403600_801605EC.vz - temp_s3->coord.t[2];
    temp_s0_3 = SquareRoot0((temp_s1 * temp_s1) + (temp_s0 * temp_s0));
    var_v1    = ratan2(temp_s1, temp_s0);
    if (var_v1 >= 0x801) {
        var_v1 -= 0x1000;
    } else if (var_v1 < -0x800) {
        var_v1 += 0x1000;
    }
    if ((arg1 & 1) ? (temp_s0_3 < temp_s5) : (temp_s5 < temp_s0_3)) {
        D_actor_403600_801606E0.placement.rot.vy = var_s2;
    } else {
        D_actor_403600_801606E0.placement.rot.vy = var_v1;
    }
    temp_s3->composeStamp                    = GRAPHICS_COORD_DIRTY;
    D_actor_403600_801606E0.placement.rot.vx = 0;
    D_actor_403600_801606E0.placement.rot.vz = 0;
    if (arg1 & 2) {
        D_actor_403600_801606E0.placement.pos.vy = temp_s3->coord.t[1];
        if (D_actor_403600_801606E0.placement.rot.vy == var_s2) {
            D_actor_403600_801606E0.placement.pos.vx = D_actor_403600_801605EC.vx;
            D_actor_403600_801606E0.placement.pos.vz = D_actor_403600_801605EC.vz;
            var_s4                                   = 0;
        } else {
            D_actor_403600_801606E0.placement.pos.vx = D_actor_403600_801605E4.vx;
            D_actor_403600_801606E0.placement.pos.vz = D_actor_403600_801605E4.vz;
            var_s4                                   = 1;
        }
    } else {
        D_actor_403600_801606E0.placement.pos.vx = temp_s3->coord.t[0];
        D_actor_403600_801606E0.placement.pos.vy = temp_s3->coord.t[1];
        D_actor_403600_801606E0.placement.pos.vz = temp_s3->coord.t[2];
    }
    TASK_MESSAGE_DISPATCH_POINTER(temp_s7, GAME_ACTOR_MESSAGE_PLACE, &D_actor_403600_801606E0.placement, 0);
    return var_s4;
}

static void func_actor_403600_8013EA04(Task* arg0)
{
    s32 index;
    s32 temp_lo;
    s32 temp_lo_3;
    s32 temp_s0;
    s32 temp_s0_2;
    // Matching constraint: the load must not be a single-set pseudo, or sched1
    // promotes it as a register birth and moves it down beside the compare.
    register s32     hp asm("a0");
    s32              temp_v0_3;
    s32              playerY;
    s32              delta;
    u32              var_v1;
    s32              temp_lo_2;
    s32              temp_lo_4;
    s32              var_a2;
    Actor403600Work* work;
    Enemy*           temp_t0;

    work             = arg0->work;
    temp_t0          = arg0->spawnArg2.pointer;
    var_a2           = 0;
    work->playerZone = 0;
    if (work->childEnemy != 0) {
        if (_actor403600Rand() & 1) {
            var_a2 = 2;
        } else {
            var_a2 = 3;
        }
    } else {
        playerY = gPlayerStatus.coordMtx->t[1];
        if (playerY >= -0x7D0) {
            s32 roll;

            work->playerZone = 2;
            roll             = _actor403600Rand() & 0xF;
            if (roll < 3) {
                var_a2 = 2;
            } else if (roll < 6) {
                var_a2 = 3;
            } else if (roll < 0xB) {
                var_a2 = 5;
            } else {
                if (_actor403600Rand() & 1) {
                    var_a2 = 6;
                } else {
                    var_a2 = 4;
                }
            }
        } else if (playerY >= -0x1004) {
            if (((u32)(gPlayerStatus.coordMtx->t[0] - 0xFA0) < 0x1F41U) &&
                ((u32)(gPlayerStatus.coordMtx->t[2] - 0xBB8) < 0x1F41U)) {
                s32 roll;

                work->playerZone = 1;
                hp               = temp_t0->hp;
                roll             = _actor403600Rand();
                roll            &= 0xF;
                if (work->hpAt60Percent < hp) {
                    var_a2 = 1;
                    if (roll & 1) {
                        var_a2 = 4;
                    }
                } else {
                    if (roll < 2) {
                        var_a2 = 4;
                    } else if (roll < 6) {
                        var_a2 = 3;
                    } else {
                        if (_actor403600Rand() & 1) {
                            var_a2 = 2;
                        } else {
                            var_a2 = 1;
                        }
                    }
                }
            } else {
                s32 roll;

                work->playerZone = 3;
                roll             = _actor403600Rand() & 0xF;
                if (roll < 2) {
                    var_a2 = 3;
                } else if (roll < 5) {
                    var_a2 = 6;
                } else if (roll < 8) {
                    var_a2 = 4;
                } else {
                    if (_actor403600Rand() & 1) {
                        var_a2 = 2;
                    } else {
                        var_a2 = 5;
                    }
                }
            }
        }
    }
    if (D_actor_403600_80160695 != 0) {
        var_a2 = D_actor_403600_80160694;
    } else {
        if (!((((u32)(var_a2 - 2) >= 2U) || (temp_t0->hp <= work->hpAt60Percent)) &&
              ((var_a2 != 5) ||
               ((temp_t0->hp <= work->hpAt35Percent) && (work->summonCount < 0xA))) &&
              ((D_actor_403600_801606B8[0] != var_a2) ||
               (D_actor_403600_801606B8[1] != var_a2)))) {
            return;
        }
        index                            = D_actor_403600_801606BC.nextSlot;
        D_actor_403600_801606B8[index]   = (u16)var_a2;
        D_actor_403600_801606BC.nextSlot = index ^ 1;
    }
    var_v1 = var_a2 - 1;
    switch (var_v1) {
        case 0:
            work->aimMode = ACTOR_403600_AIM_TARGET;
            work->roll    = 0;
            work->step    = 0;
            work->action  = ACTOR_403600_ACTION_SLAM;
            return;
        case 1:
            work->animId      = 0x10;
            work->actionParam = 0x32;
            work->aimMode     = ACTOR_403600_AIM_PLAYER;
            work->action      = ACTOR_403600_ACTION_VOLLEY;
            return;
        case 2:
            delta     = gPlayerStatus.coordMtx->t[0] - D_actor_403600_8016063C[0].vx;
            temp_lo   = delta * delta;
            delta     = gPlayerStatus.coordMtx->t[2] - D_actor_403600_8016063C[0].vz;
            temp_s0   = SquareRoot0(temp_lo + (delta * delta));
            delta     = gPlayerStatus.coordMtx->t[0] - D_actor_403600_8016063C[1].vx;
            temp_lo_2 = delta * delta;
            delta     = gPlayerStatus.coordMtx->t[2] - D_actor_403600_8016063C[1].vz;
            temp_v0_3 = SquareRoot0(temp_lo_2 + (delta * delta));
            if (work->playerZone == 1) {
                if (temp_s0 < temp_v0_3) {
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
                if (temp_s0 < temp_v0_3) {
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
                delta     = gPlayerStatus.coordMtx->t[0] - D_actor_403600_8016064C[0].vx;
                temp_lo_3 = delta * delta;
                delta     = gPlayerStatus.coordMtx->t[2] - D_actor_403600_8016064C[0].vz;
                temp_s0_2 = SquareRoot0(temp_lo_3 + (delta * delta));
                delta     = gPlayerStatus.coordMtx->t[0] - D_actor_403600_8016064C[1].vx;
                temp_lo_4 = delta * delta;
                delta     = gPlayerStatus.coordMtx->t[2] - D_actor_403600_8016064C[1].vz;
                if (SquareRoot0(temp_lo_4 + (delta * delta)) < temp_s0_2) {
                    work->swoopCorners = (u16)(work->swoopCorners | 2);
                }
            }
            work->ignorePushOut = 1;
            work->step          = 0;
            work->aimMode       = ACTOR_403600_AIM_PLAYER;
            work->actionParam   = 0;
            work->action        = ACTOR_403600_ACTION_SWOOP;
            return;
        case 3:
            work->ignorePushOut   = 1;
            work->step            = 0;
            work->aimMode         = ACTOR_403600_AIM_TARGET;
            work->action          = ACTOR_403600_ACTION_RUSH;
            work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->rushPasses      = (_actor403600Rand() & 3) * 2;
            work->actionParam     = (_actor403600Rand() % 20) + 0x28;
            func_actor_403600_8013CCEC(arg0, 1);
            return;
        case 4:
            work->aimMode     = ACTOR_403600_AIM_PLAYER;
            work->animId      = 2;
            work->action      = ACTOR_403600_ACTION_SUMMON;
            work->actionParam = (_actor403600Rand() % 20) + 0x28;
            work->summonCount++;
            return;
        case 5:
            work->animId        = 2;
            work->actionParam   = 0xD2;
            work->action        = ACTOR_403600_ACTION_DRAIN;
            work->actionTimer   = 0x14;
            work->actionCounter = 0x13;
            break;
    }
}

static void func_actor_403600_8013F0C0(Task* arg0)
{
    s16              temp_v1;
    s16              temp_v1_2;
    s16              temp_v1_3;
    s16              temp_v1_4;
    s32              temp_s2;
    GfxCoord*        temp_s4;
    s32              temp_s0;
    s32              temp_s0_2;
    s32              temp_s0_3;
    s32              temp_s0_4;
    u16              temp_v0;
    u16              temp_v0_2;
    GfxCoord*        temp_a0;
    GfxCoord*        temp_a0_2;
    GfxCoord*        temp_a0_3;
    Actor403600Work* temp_a1;
    Actor403600Work* temp_a1_2;
    Actor403600Work* temp_a1_3;
    Actor403600Work* work;

    temp_s4 = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    work    = arg0->work;
    switch (D_actor_403600_80160568.animationId) {
        case 1:
            work->knockbackFrame++;
            temp_a1 = arg0->work;
            temp_a0 = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
            temp_a0->coord.t[0] +=
                (temp_a0->coord.m[0][2] * temp_a1->knockbackSpeed) >> 0xC;
            temp_a0->coord.t[2] +=
                (temp_a0->coord.m[2][2] * temp_a1->knockbackSpeed) >> 0xC;
            if ((work->knockbackFrame >= 0xC) || (gGameSession->viewReady != 0)) {
                work->knockbackFrame = 0;
                if (func_actor_403600_8013E7D4(arg0, 3) == 0) {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 7;
                } else {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 3;
                }
                D_actor_403600_80160568.animationId = 2;
                Gp_StateC08.flags                  |= ATTACHMENT_FLAG_EVENT_LOCK;
                TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
                return;
            }
            break;

        case 2:
            temp_v0              = work->knockbackFrame + 1;
            work->knockbackFrame = temp_v0;
            if ((s16)temp_v0 == 0xC) {
                Gp_SpawnPadLerp(0xA, 0xFF, 0xFF);
                D_actor_403600_801606A4.power    = 0x14;
                D_actor_403600_801606A4.reaction = 0;
                temp_s2                          = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                temp_s0                          = (s8)worldCoordGetOriginAudioPan(temp_s4);
                sndEvtRequestScriptStart(temp_s2, temp_s0,
                                         (s8)worldCoordGetOriginAudioDepth(temp_s4));
                temp_s2 =
                    (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160011;
                temp_s0_2 = (s8)worldCoordGetOriginAudioPan(temp_s4);
                sndEvtRequestScriptStart(temp_s2, temp_s0_2,
                                         (s8)worldCoordGetOriginAudioDepth(temp_s4));
                taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_APPLY_DAMAGE,
                                    damagePackAttackKey(&D_actor_403600_801606A4, 0), 0);
            }
            if (work->knockbackFrame >= 0x66) {
                work->knockbackFrame                = 0;
                D_actor_403600_80160568.animationId = 0;
                taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            break;

        case 3:
            work->knockbackFrame++;
            temp_v1              = (u16)work->knockbackSpeed + 2;
            work->knockbackSpeed = temp_v1;
            if (temp_v1 > 0) {
                work->knockbackSpeed = 0;
            }
            temp_a1_2 = arg0->work;
            temp_a0_2 = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
            temp_a0_2->coord.t[0] +=
                (temp_a0_2->coord.m[0][2] * temp_a1_2->knockbackSpeed) >> 0xC;
            temp_a0_2->coord.t[2] +=
                (temp_a0_2->coord.m[2][2] * temp_a1_2->knockbackSpeed) >> 0xC;
            temp_v1_2 = work->action;
            if ((temp_v1_2 != ACTOR_403600_ACTION_DRAIN) && (temp_v1_2 != ACTOR_403600_ACTION_RUSH) &&
                (work->knockbackFrame == 0xC)) {
                temp_s2 =
                    (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160012;
                temp_s0_3 = (s8)worldCoordGetOriginAudioPan(temp_s4);
                sndEvtRequestScriptStart(temp_s2, temp_s0_3,
                                         (s8)worldCoordGetOriginAudioDepth(temp_s4));
            }
            if (work->knockbackFrame >= 0x24) {
                Gp_StateC08.flags                  |= ATTACHMENT_FLAG_EVENT_LOCK;
                work->knockbackFrame                = 0;
                D_actor_403600_80160568.animationId = 5;
                TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
                return;
            }
            break;

        case 4:
            work->knockbackFrame++;
            temp_v1_3            = (u16)work->knockbackSpeed - 2;
            work->knockbackSpeed = temp_v1_3;
            if (temp_v1_3 < 0) {
                work->knockbackSpeed = 0;
            }
            temp_a1_3 = arg0->work;
            temp_a0_3 = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
            temp_a0_3->coord.t[0] +=
                (temp_a0_3->coord.m[0][2] * temp_a1_3->knockbackSpeed) >> 0xC;
            temp_a0_3->coord.t[2] +=
                (temp_a0_3->coord.m[2][2] * temp_a1_3->knockbackSpeed) >> 0xC;
            temp_v1_4 = work->action;
            if ((temp_v1_4 != ACTOR_403600_ACTION_DRAIN) && (temp_v1_4 != ACTOR_403600_ACTION_RUSH) &&
                (work->knockbackFrame == 0xC)) {
                temp_s2 =
                    (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54160012;
                temp_s0_4 = (s8)worldCoordGetOriginAudioPan(temp_s4);
                sndEvtRequestScriptStart(temp_s2, temp_s0_4,
                                         (s8)worldCoordGetOriginAudioDepth(temp_s4));
            }
            if (work->knockbackFrame >= 0x24) {
                Gp_StateC08.flags                  |= ATTACHMENT_FLAG_EVENT_LOCK;
                work->knockbackFrame                = 0;
                D_actor_403600_80160568.animationId = 6;
                TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
                return;
            }
            break;

        case 5:
        case 6:
            temp_v0_2            = work->knockbackFrame + 1;
            work->knockbackFrame = temp_v0_2;
            if ((s16)temp_v0_2 >= 0x28) {
                work->knockbackFrame                = 0;
                D_actor_403600_80160568.animationId = 0;
                taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            break;
    }
}

static void func_actor_403600_8013F608(Task* arg0)
{
    Actor403600Work* work;
    s16              temp_v0_3;
    s16              temp_v1;
    s32              temp_arg2;
    s32              var_check;
    s32              var_s1;
    u16              temp_field;
    u16              temp_v0;
    u16              temp_v0_2;
    u16              temp_v0_4;
    u32              temp_t0;
    u32              temp_v0_5;

    work    = arg0->work;
    temp_v1 = work->drainPuffInterval;
    if (temp_v1 == -1) {
        temp_v0               = (u16)work->drainPuffFrames + 1;
        work->drainPuffFrames = temp_v0;
        if ((s16)temp_v0 >= 3) {
            var_s1                = 1;
            work->drainPuffFrames = 0;
            do {
                Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, (*gPlayerActorTasks)->extra.tmd->coords + var_s1, 0x400, NULL);
                var_s1 += 1;
            } while (var_s1 < 0x13);
        }
    } else {
        temp_v0_2             = (u16)work->drainPuffFrames + 1;
        work->drainPuffFrames = temp_v0_2;
        var_check             = (s16)temp_v0_2 < temp_v1;
        temp_field            = (u16)work->drainPuffInterval;
        if (!var_check) {
            temp_v0_3               = temp_field - 8;
            work->drainPuffInterval = temp_v0_3;
            if (temp_v0_3 < 3) {
                work->drainPuffInterval = 2;
            }
            temp_v0_4          = (u16)work->drainPuffArg + 1;
            work->drainPuffArg = temp_v0_4;
            if ((s16)temp_v0_4 >= 0x400) {
                work->drainPuffArg = 0x400;
            }
            temp_v0_5       = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            temp_t0         = temp_v0_5 >> 0x10;
            temp_arg2       = work->drainPuffArg;
            gRandomLcgState = temp_v0_5;
            Gp_SpawnEff(EFFECT_ADDITIVE_PUFF,
                        &(*gPlayerActorTasks)->extra.tmd->coords[(temp_t0 % 19) & 0xFFFF],
                        temp_arg2, NULL);
            work->drainPuffFrames = 0;
        }
    }
}

/// Steps the rig's twenty parts: restarts every part on the animation in
/// `animId` when it changed, otherwise advances the frame counter and
/// ticks each part at rate `animRate`. Animations with no entry in
/// `D_actor_403600_8016057C` are not played.
static __inline__ void _actor403600UpdateAnimation(Task* task, u8 count)
{
    Actor403600Work* work;
    s32              i;

    work = task->work;
    if (D_actor_403600_8016057C[work->animId] != 0) {
        if (work->animId != work->appliedAnimId) {
            work->appliedAnimId = work->animId;
            work->phaseFrame    = 0;
            for (i = 1; i < count; i++) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->animId, 0, work->animBlendFrames);
            }
        } else {
            work->phaseFrame++;
            for (i = 1; i < count; i++) {
                work->rig.slots[i].rate = work->animRate;
                animationTickSlot(&work->rig.anim, i);
            }
        }
    }
}

static void func_actor_403600_8013F7B8(Enemy* enemy, Task* task)
{
    SVECTOR                rot;
    TmdObject*             model;
    GfxCoord*              worldCoord;
    GfxCoord*              modelCoord;
    GfxCoord*              bodyCoord;
    WorldCollisionContact* bodyRecs;
    WorldCollisionContact* attackRecs;
    Actor403600Work*       ownerWork;
    Actor403600Work*       work;
    u32                    randomProduct;
    s32                    angle;
    s32                    i;
    u32                    randomState;
    GfxMatrix*             worldMatrix;
    GfxMatrix*             modelMatrix;

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
    worldMatrix                       = (GfxMatrix*)&work->worldCoord.coord;
    worldMatrix->rotationWords.m00M01 = ONE;
    worldMatrix->rotationWords.m02M10 = 0;
    worldMatrix->rotationWords.m11M12 = ONE;
    worldMatrix->rotationWords.m20M21 = 0;
    worldMatrix->rotationWords.m22    = ONE;
    modelMatrix                       = (GfxMatrix*)&modelCoord->coord;
    work->worldCoord.coord.t[0]       = 0;
    work->worldCoord.coord.t[1]       = 0;
    work->worldCoord.coord.t[2]       = 0;
    modelCoord->parent                = worldCoord;
    modelMatrix->rotationWords.m00M01 = ONE;
    modelMatrix->rotationWords.m02M10 = 0;
    modelMatrix->rotationWords.m11M12 = ONE;
    modelMatrix->rotationWords.m20M21 = 0;
    modelMatrix->rotationWords.m22    = ONE;
    modelCoord->coord.t[0]            = 0;
    modelCoord->coord.t[1]            = 0x744;
    modelCoord->coord.t[2]            = 0;
    work->worldCoord.composeStamp     = GRAPHICS_COORD_DIRTY;
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
    enemy->hp                     = (s16)D_actor_403600_80150ED8.hpMax;
    animationInitContext(&work->rig.anim, D_actor_403600_8016057C, model, work->rig.poses, work->rig.slots);
    i = 1;
    do {
        animationResetSlot(&work->rig.anim, i, 1);
        i += 1;
    } while (i < 0x14);
    (sceneAcquireBattleRef)(0);
    bodyRecs            = work->hitContacts;
    work->animId        = 9;
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
    worldCollisionInitContacts(bodyRecs, 4, 0);
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
    worldCollisionInitContacts(attackRecs, 1, 0);
    work->attackBody.flags     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->worldCoord.coord.t[0] = (s32)ownerWork->worldCoord.coord.t[0];
    work->worldCoord.coord.t[1] = (s32)ownerWork->worldCoord.coord.t[1];
    work->worldCoord.coord.t[2] = (s32)ownerWork->worldCoord.coord.t[2];
    gfxReadMatrixZAxis(&ownerWork->worldCoord.coord, &rot);
    angle  = ratan2((s32)rot.vx, (s32)rot.vz);
    rot.vx = 0;
    rot.vy = (s16)angle;
    rot.vz = 0;
    RotMatrix(&rot, &work->worldCoord.coord);
    randomProduct            = gRandomLcgState * RANDOM_LCG_MULTIPLIER;
    randomState              = randomProduct + RANDOM_LCG_INCREMENT;
    work->yaw                = (s16)angle;
    work->animBlendFrames    = 0;
    work->defeated           = 0;
    work->age                = 0;
    work->action             = ACTOR_403600_DOUBLE_ACTION_APPEAR;
    work->colorRefreshFrames = 0xA;
    work->chaseSpeed         = 0x14;
    work->lifetime           = (s16)(((randomState >> 0x10) % 0x32) + 0xBB8);
    gRandomLcgState          = randomState;
    if (task->spawnArg1.value != 0) {
        worldTargetSetPlayerLock(&enemy->node);
    }
    _actor403600UpdateAnimation(task, 20);
    work->animRate     = 0x10;
    task->exitCallback = func_actor_403600_80141598;
    task->state       += 1;
}

/// Colours `enemy` from the world position of the work block's own
/// coordinate, passed through a `VECTOR` taken off the scratch stack for the
/// call.
static __inline__ void _actor403600UpdateColor(Enemy* enemy, Task* task)
{
    Actor403600Work* work;
    VECTOR*          pos;

    work                         = task->work;
    pos                          = SCRATCH_STACK_CURSOR(VECTOR) - 1;
    pos->vx                      = work->worldCoord.workm.t[0];
    pos->vy                      = work->worldCoord.workm.t[1];
    SCRATCH_STACK_CURSOR(VECTOR) = pos;
    pos->vz                      = work->worldCoord.workm.t[2];
    worldCoordUpdateActorColor(enemy, pos, 0, 0);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Turns coordinate 2 by `flinchRot`, then eases the twist back
/// towards zero by 0x20 a frame.
static __inline__ void _actor403600RotateParts(Task* task)
{
    Actor403600Work* work;
    GfxCoord*        coord;
    MATRIX*          matrix;

    work = task->work;
    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    matrix = SCRATCH_STACK_CURSOR(MATRIX);
    coord  = task->extra.tmd->coords;
    RotMatrix(&work->flinchRot, matrix);
    gte_SetRotMatrix(&coord[2].coord);
    gte_ldclmv(matrix);
    gte_rtir();
    gte_stclmv(&coord[2].coord);
    gte_ldclmv(&matrix->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[2].coord.m[0][1]);
    gte_ldclmv(&matrix->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[2].coord.m[0][2]);
    if (work->flinchRot.vx != 0) {
        if (work->flinchRot.vx >= 0x20) {
            work->flinchRot.vx -= 0x20;
            if (work->flinchRot.vx <= 0) {
                work->flinchRot.vx = 0;
            }
        }
        if (work->flinchRot.vx <= 0x20) {
            work->flinchRot.vx += 0x20;
            if (work->flinchRot.vx >= 0) {
                work->flinchRot.vx = 0;
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

static void func_actor_403600_8013FC2C(Enemy* arg0, Task* arg1)
{
    TmdObject*       obj;
    Actor403600Work* work;

    obj  = arg1->extra.tmd;
    work = arg1->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor403600UpdateColor(arg0, arg1);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = (WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE);
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            func_actor_403600_80141C3C(arg1);
            func_actor_403600_8013DC7C(arg1);
            func_actor_403600_8013D15C(arg1);
            _actor403600UpdateAnimation(arg1, 20);
            _actor403600RotateParts(arg1);
            work->worldCoord.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&work->worldCoord);
            if (++work->colorRefreshFrames >= 10) {
                work->colorRefreshFrames = 0;
                _actor403600UpdateColor(arg0, arg1);
            }
            if (++work->age % 42 == 0) {
                if (++work->chaseSpeed >= 100) {
                    work->chaseSpeed = 100;
                }
            }
            if (work->age >= work->lifetime || work->defeated != 0) {
                arg0->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
                obj->shading.screenFadeDistance = 0x12C;
                arg1->killCountdown             = 0x3C;
                arg1->state++;
            }
            break;
    }
}

/// Handlers for states 0-2 of the task `func_actor_403600_80141BE0` dispatches,
/// indexed by `Task::state`. The state-0 handler sets the task up and
/// advances it.
static const EnemyTaskFuncTable3 D_actor_403600_801320A0 = { {
    func_actor_403600_8013F7B8,
    func_actor_403600_8013FC2C,
    func_actor_403600_80140488,
} };

static void func_actor_403600_801400BC(Task* arg0)
{
    u32              sp10;
    s32              sp14;
    s16              temp_v1;
    s32              temp_s2;
    s32              temp_v0;
    s32              temp_v1_2;
    s32              var_v0;
    s32              var_v0_2;
    s32              var_v0_3;
    s32              temp_s0_2;
    s32              temp_s0_4;
    u32              temp_v0_2;
    u32              temp_v0_3;
    GfxCoord*        temp_s0;
    GfxCoord*        temp_s0_3;
    Actor403600Work* work;

    work    = arg0->work;
    temp_v1 = work->action;
    switch (temp_v1) {
        case ACTOR_403600_DOUBLE_ACTION_WAIT:
            work->animId        = 1;
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
            func_actor_403600_8013DDF4(arg0, 0);
            work->animId        = 2;
            work->forwardSpeed  = work->chaseSpeed;
            temp_v1_2           = gPlayerStatus.coordMtx->t[1];
            temp_v0             = work->worldCoord.coord.t[1] + 0x3E8;
            work->verticalSpeed = (s16)((temp_v1_2 - temp_v0) / 25);
            func_actor_403600_8013E470(&work->worldCoord, (s32*)&sp10, &sp14);
            if (sp10 < 0x835U) {
                var_v0 = sp14;
                if (var_v0 < 0) {
                    var_v0 = -var_v0;
                }
                if (var_v0 < 0x400) {
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
            work->animId         = 0xC;
            work->forwardSpeed   = 0U;
            work->verticalSpeed  = 0;
            if (work->phaseFrame == 0xE) {
                temp_s0   = &work->worldCoord;
                temp_s2   = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x5416000D;
                temp_s0_2 = (s8)worldCoordGetOriginAudioPan(temp_s0);
                temp_v0_2 = worldCoordGetOriginAudioDepth(temp_s0);
                sndEvtRequestScriptStart(temp_s2, temp_s0_2,
                                         (s32)(((temp_v0_2 >> 0x1F) + temp_v0_2) << 0x17) >> 0x18);
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
                func_actor_403600_8013E470(&work->worldCoord, (s32*)&sp10, &sp14);
                if (sp10 < 0x7D1U) {
                    var_v0_2 = sp14;
                    if (var_v0_2 < 0) {
                        var_v0_2 = -var_v0_2;
                    }
                    if (var_v0_2 < 0x400) {
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
            work->animId         = 0xD;
            work->forwardSpeed   = 0U;
            work->verticalSpeed  = 0;
            if (work->phaseFrame == 9) {
                temp_s0_3 = &work->worldCoord;
                temp_s2   = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x5416000D;
                temp_s0_4 = (s8)worldCoordGetOriginAudioPan(temp_s0_3);
                temp_v0_3 = worldCoordGetOriginAudioDepth(temp_s0_3);
                sndEvtRequestScriptStart(temp_s2, temp_s0_4,
                                         (s32)(((temp_v0_3 >> 0x1F) + temp_v0_3) << 0x17) >> 0x18);
            }
            if (work->phaseFrame == 0xA) {
                work->attackBody.flags = (u16)(work->attackBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->phaseFrame == 0xE) {
                work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            if (work->phaseFrame >= 0x23) {
                work->animBlendFrames = 8;
                work->forwardSpeed    = 0U;
                work->phaseFrame      = 0;
                func_actor_403600_8013E470(&work->worldCoord, (s32*)&sp10, &sp14);
                if (sp10 < 0x7D1U) {
                    var_v0_3 = sp14;
                    if (var_v0_3 < 0) {
                        var_v0_3 = -var_v0_3;
                    }
                    if (var_v0_3 < 0x400) {
                        work->action = ACTOR_403600_DOUBLE_ACTION_SWIPE_A;
                        return;
                    }
                }
                work->action = ACTOR_403600_DOUBLE_ACTION_WAIT;
                return;
            }
            break;
        case ACTOR_403600_DOUBLE_ACTION_APPEAR:
            work->animId        = 9;
            work->forwardSpeed  = 0U;
            work->verticalSpeed = 0;
            if (work->phaseFrame >= 0x46) {
                work->action          = ACTOR_403600_DOUBLE_ACTION_CHASE;
                work->phaseFrame      = 0;
                work->animBlendFrames = 8;
            }
            break;
    }
}

static void func_actor_403600_80140488(Enemy* arg0, Task* arg1)
{
    TmdObject*       object;
    Actor403600Work* work;
    Actor403600Work* globalWork;
    Actor403600Work* cleanupWork;
    Enemy*           enemy;

    object     = arg1->extra.tmd;
    work       = arg1->work;
    globalWork = D_actor_403600_801606A8->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (globalWork->defeated != 1) {
                return;
            }
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            object->flags               |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            break;
    }
    switch (work->step) {
        case 0:
            object->shading.screenFadeDistance += 3;
            arg1->extra.tmd->flags              = 0;
            if (--arg1->killCountdown <= 0) {
                work->step        = 1;
                work->actionParam = 0;
            }
            break;
        case 1:
            Gp_ReleaseStateF0Add(arg1, 0x24);
            globalWork->childEnemy          = NULL;
            enemy                           = arg1->spawnArg2.pointer;
            cleanupWork                     = arg1->work;
            arg1->extra.tmd->coords->parent = &gGfxViewCoord;
            enemy->recs                     = 0;
            worldTargetUnlinkNode(&enemy->node);
            worldCollisionUnlinkBody(&cleanupWork->hitBody);
            worldCollisionUnlinkBody(&cleanupWork->attackBody);
            if (arg1 == D_actor_403600_801606A8) {
                worldCollisionUnlinkBody(&cleanupWork->gridBody);
            }
            enemyTaskExit(arg1);
            return;
    }
    _actor403600UpdateAnimation(arg1, 20);
}

s32 func_actor_403600_801406A4(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    SVECTOR          angles;
    u16              message;
    Enemy*           enemy;
    Actor403600Work* work;
    Actor403600Work* childWork;
    TmdObject*       childObject;

    message = request->command;
    work    = arg0->work;
    enemy   = arg0->spawnArg2.pointer;
    switch (message) {
        case 1:
            _actor403600ResetState(arg0);
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            worldTargetDisableNodeLockOn(&enemy->node);
            work->animId                = 1;
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
            D_actor_403600_80160568.animationId = 9;
            TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
            break;
        case 2:
            work->animId                        = 0x15;
            work->appliedAnimId                 = 0;
            D_actor_403600_80160568.animationId = 0xA;
            TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
            break;
        case 3:
            work->ambientBoost = 0x3E8;
            work->mode         = ACTOR_403600_MODE_SCENE_BRIGHTEN;
            break;
        case 4:
            D_actor_403600_801606B0 = taskSpawnFromTable(D_actor_303600_8016E468, 0, 0, 0);
            taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
            arg0->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->childEnemy        = Gp_SpawnEnemyFromTable(D_actor_403600_80160514, 2, 0, 0);
            break;
        case 5:
            D_actor_403600_801606E0.placement.rot.vx = 0;
            D_actor_403600_801606E0.placement.rot.vy = 0;
            D_actor_403600_801606E0.placement.rot.vz = 0;
            D_actor_403600_801606E0.placement.pos.vx = 0;
            D_actor_403600_801606E0.placement.pos.vy = 0;
            D_actor_403600_801606E0.placement.pos.vz = 0;
            TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_PLACE, &D_actor_403600_801606E0.placement, 0);
            D_actor_403600_80160568.animationId = 0xB;
            TASK_MESSAGE_DISPATCH_POINTER(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], ANIMATION_MESSAGE_INSTALL_AND_PLAY, &D_actor_403600_80160568, 0);
            childWork                               = work->childEnemy->task->work;
            childObject                             = work->childEnemy->task->extra.tmd;
            childWork->mode                         = ACTOR_403600_MODE_SCENE_FADE;
            childObject->shading.screenFadeDistance = 0;
            break;
        case 6:
            work->forwardSpeed          = -0x50;
            work->mode                  = ACTOR_403600_MODE_SCENE_ASCEND;
            work->animId                = 1;
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
            tmdAllocPrimitiveBuffer(arg0->extra.tmd);
            arg0->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            arg0->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->step              = 0;
            break;
        case 7:
            _actor403600ResetState(arg0);
            work->mode                    = ACTOR_403600_MODE_FIGHT;
            work->step                    = 0;
            enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
            work->animId                  = 1;
            work->worldCoord.coord.t[0]   = 0x196E;
            work->worldCoord.coord.t[1]   = -0x1B62;
            work->worldCoord.coord.t[2]   = 0x1630;
            work->appliedAnimId           = 0;
            angles.vx                     = 0;
            angles.vy                     = 0x200;
            angles.vz                     = 0;
            RotMatrix(&angles, &work->worldCoord.coord);
            tmdAllocPrimitiveBuffer(arg0->extra.tmd);
            arg0->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            arg0->extra.tmd->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case 8:
            work->mode                    = ACTOR_403600_MODE_PARKED;
            arg0->extra.tmd->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->extra.tmd->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case 9:
            Gp_ReleaseStateF0Add(arg0, 0x24);
            gGameSession->flowFlags                        = (u8)(gGameSession->flowFlags | GAME_SESSION_FLOW_REEQUIP_WEAPON);
            gSceneCombatState.signals.bytes.endDelayFrames = 5;
            break;
    }
    return 0;
}

static void func_actor_403600_80140B4C(Enemy* enemy, Task* actor)
{
    SVECTOR             offset;
    GfxCoord            view;
    ActorCommand        startMsg;
    ActorCommand        stopMsg;
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
    Gp_TrySpawnViewTask(&D_actor_403600_80160700);
    func_actor_403600_80141F58(&work->worldCoord, work->hitCooldown);
    work->sceneFrame++;
    if (work->sceneFrame >= ACTOR_303600_ROT_SAMPLE_COUNT) {
        work->sceneFrame = ACTOR_303600_ROT_SAMPLE_COUNT - 1;
    }
    if (work->mode != ACTOR_403600_MODE_SCENE_FADE) {
        if (work->sceneFrame == 1) {
            startMsg.context.loc.stage = 4;
            startMsg.context.loc.area  = 0x16;
            startMsg.command           = 0;
            TASK_MESSAGE_DISPATCH_POINTER(D_actor_403600_801606B0, ACTOR_COMMAND_MESSAGE_APPLY, &startMsg, 0);
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
            Gp_SpawnEff(EFFECT_EVE_LIGHT_BEAM, &view, 0x300, &offset);
        }
        if (work->phaseFrame == 0x15E) {
            stopMsg.context.loc.stage = 4;
            stopMsg.context.loc.area  = 0x16;
            stopMsg.command           = 1;
            TASK_MESSAGE_DISPATCH_POINTER(D_actor_403600_801606B0, ACTOR_COMMAND_MESSAGE_APPLY, &stopMsg, 0);
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
                Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_RISING_SPRITE, &work->worldCoord, 0x10800, &offset);
            }
        }
        if (work->phaseFrame == 0x2A8) {
            taskMessageDispatch(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER], GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
        }
        if (work->phaseFrame >= 0x2A8 && work->phaseFrame < 0x385 && (work->phaseFrame & 3) == 3) {
            Gp_SpawnEff(EFFECT_SHELTER_B2_POD_BOTTOM_RISING_SPRITE, &work->worldCoord, 0x10800, NULL);
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
            Gp_SpawnEff(EFFECT_EVE_LIGHT_BEAM, &view, -0x300, &offset);
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
/// the task. State 0 is the spawn (`func_actor_403600_80138EF8`, which
/// advances the state), state 1 the per-frame update.
void func_actor_403600_80141180(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_403600_80138EF8,
        func_actor_403600_8013938C,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_403600_801411D4(Task* arg0, s32 arg1)
{
    _actor403600UpdateAnimation(arg0, arg1);
}

static void func_actor_403600_801412D0(Enemy* arg0, Task* arg1)
{
    Actor403600Work* work;
    VECTOR*          head;
    VECTOR*          block;

    work                         = arg1->work;
    head                         = SCRATCH_STACK_CURSOR(VECTOR);
    head[-1].vx                  = work->worldCoord.workm.t[0];
    block                        = head - 1;
    block->vy                    = work->worldCoord.workm.t[1];
    SCRATCH_STACK_CURSOR(VECTOR) = block;
    block->vz                    = work->worldCoord.workm.t[2];
    worldCoordUpdateActorColor(arg0, block, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Turns coordinate 2 by `flinchRot`, then eases the twist back
/// towards zero by 0x20 a frame.
static void func_actor_403600_80141338(Task* arg0)
{
    Actor403600Work* work;
    GfxCoord*        coord;
    MATRIX*          matrix;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    matrix = SCRATCH_STACK_CURSOR(MATRIX);
    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    RotMatrix(&work->flinchRot, matrix);

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

    if (work->flinchRot.vx != 0) {
        if (work->flinchRot.vx >= 0x20) {
            work->flinchRot.vx -= 0x20;
            if (work->flinchRot.vx <= 0) {
                work->flinchRot.vx = 0;
            }
        }
        if (work->flinchRot.vx <= 0x20) {
            work->flinchRot.vx += 0x20;
            if (work->flinchRot.vx >= 0) {
                work->flinchRot.vx = 0;
            }
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

static void func_actor_403600_801414FC(Task* arg0)
{
    Actor403600Work* work;
    s16              value;
    s16              countdown;
    s32              brightness;

    work  = arg0->work;
    value = work->shakeFadeFrames;
    if (value != 0) {
        if (value < work->shakeFrames) {
            brightness = rsin(gDisplayState.animFrame << 9) << 0xD;
        } else {
            brightness = rsin(gDisplayState.animFrame << 9) << 0xC;
        }
        displaySetShakeY(brightness >> 0x18);
        countdown         = (u16)work->shakeFrames - 1;
        work->shakeFrames = countdown;
        if ((countdown << 0x10) <= 0) {
            work->shakeFadeFrames = 0;
            displaySetShakeY(0);
        }
    }
}

static void func_actor_403600_80141598(Task* task)
{
    Actor403600Work* work;
    Enemy*           enemy;

    enemy                           = task->spawnArg2.pointer;
    work                            = task->work;
    task->extra.tmd->coords->parent = &gGfxViewCoord;
    enemy->recs                     = 0;
    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->hitBody);
    worldCollisionUnlinkBody(&work->attackBody);
    if (task == D_actor_403600_801606A8) {
        worldCollisionUnlinkBody(&work->gridBody);
    }
    enemyTaskExit(task);
}

static void func_actor_403600_8014161C(Task* arg0)
{
    Actor403600Work* work;
    Enemy*           enemy;
    u16*             ticks;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->weakPhase == 1) {
        if (((work->weakFrames & 3) == 3) &&
            ((work->action != ACTOR_403600_ACTION_RUSH) || (work->step != 5))) {
            Gp_SpawnEff(EFFECT_HIT_PUFF, arg0->extra.tmd->coords + 1, 0x12800, NULL);
        }
        /* Stored through a plain halfword pointer: as a structure store it
         * makes the compiler read `gDisplayState.animFrame` again after it. */
        ticks                        = &work->weakFrames;
        *ticks                       = (u16)(work->weakFrames + 1);
        work->worldCoord.coord.t[1] += rsin(gDisplayState.animFrame << 8) >> 6;
        if ((work->hpMax / 10 < enemy->hp) &&
            (work->weakFrames >= 0x385) && (work->action == ACTOR_403600_ACTION_CHOOSE)) {
            work->mode = ACTOR_403600_MODE_RECOVER;
        }
    }
}

static void func_actor_403600_8014174C(Task* arg0)
{
    _actor403600ResetState(arg0);
}

static void func_actor_403600_801417A8(Task* arg0, s32 arg1)
{
    SVECTOR          rotation;
    Actor403600Work* work;
    s16              angle;

    work       = arg0->work;
    angle      = work->roll + (arg1 & 0xFF);
    work->roll = angle;
    if (ABS(angle) > 0x800) {
        if (angle > 0) {
            work->roll = angle - 0x1000;
        } else {
            work->roll = 0x1000 - angle;
        }
    }
    gfxMatrixToEuler(&work->worldCoord.coord, &rotation);
    rotation.vz += work->roll;
    RotMatrix(&rotation, &work->worldCoord.coord);
}

static s32 func_actor_403600_80141840(Task* arg0)
{
    Actor403600Work* work;
    s32              count;
    s32              deltaX;
    s32              deltaY;
    s32              deltaZ;
    s32              targetY;
    s32              currentY;

    count         = 0;
    work          = arg0->work;
    work->aimMode = ACTOR_403600_AIM_TARGET;
    func_actor_403600_8013DDF4(arg0, 0xA0);

    work->forwardSpeed = 0x12C;
    deltaX             = work->targetPos.vx - work->worldCoord.coord.t[0];
    if (ABS(deltaX) < 0x1F5) {
        count                       = 1;
        work->worldCoord.coord.t[0] = work->prevPos.vx;
    }

    targetY  = work->targetPos.vy;
    currentY = work->worldCoord.coord.t[1];
    deltaY   = targetY - currentY;
    if (ABS(deltaY) < 0x1F5) {
        count                      += 1;
        work->worldCoord.coord.t[1] = work->prevPos.vy;
    } else if (targetY < currentY) {
        work->verticalSpeed          = -0x12C;
        work->worldCoord.coord.t[1] += rsin(gDisplayState.animFrame << 8) >> 6;
    } else {
        work->verticalSpeed = 0x12C;
    }

    deltaZ = work->targetPos.vz - work->worldCoord.coord.t[2];
    if (ABS(deltaZ) < 0x1F5) {
        work->worldCoord.coord.t[2] = work->prevPos.vz;
        count                      += 1;
    }
    return count & 0xFF;
}

static void func_actor_403600_80141954(s32 arg0)
{
    RECT rect;

    rect.y = 0x80;
    rect.h = 0x80;
    rect.w = 0x80;
    if (arg0 == 1) {
        rect.x = 0x180;
    } else {
        rect.x = 0x1C0;
    }
    MoveImage(&rect, 0x180, 0x180);
    rect.w = 0x100;
    rect.h = 1;
    rect.x = 0;
    if (arg0 == 1) {
        rect.y = 0xFD;
    } else {
        rect.y = 0xFE;
    }
    MoveImage(&rect, 0, 0xF9);
}

static void func_actor_403600_801419E8(Task* arg0)
{
    TmdObject* obj;

    obj                      = arg0->extra.tmd;
    *&obj->texturePageOffset = -0xF;
    obj->clutRowOffset       = 2;
    if (obj->buffer != NULL) {
        tmdBuildBufferHalf(obj);
        tmdBuildBufferHalf(obj);
    }
}

static void func_actor_403600_80141A34(Task* arg0)
{
    RECT             rect;
    Actor403600Work* work;
    s16              value;

    work  = arg0->work;
    value = work->committed;
    if (work->appliedFace != value) {
        if (value == 1) {
            s16 width;

            rect.x = 0x162;
            rect.y = 0x162;
            width  = 0x15;
            rect.w = width;
            rect.h = 0xA;
            MoveImage(&rect, 0x141, 0x152);
            rect.x = 0x16F;
            rect.y = 0x102;
            rect.w = 0x17;
            rect.h = width;
            MoveImage(&rect, 0x141, 0x164);
        } else {
            s16 width;

            rect.x = 0x141;
            rect.y = 0x1F3;
            width  = 0x15;
            rect.w = width;
            rect.h = 0xA;
            MoveImage(&rect, 0x141, 0x152);
            rect.x = 0x17F;
            rect.y = 0x1AB;
            rect.w = 0x17;
            rect.h = width;
            MoveImage(&rect, 0x141, 0x164);
        }
        work->appliedFace = (u16)work->committed;
    }
}

static void func_actor_403600_80141B24(Task* arg0)
{
    Actor403600Work* work = arg0->work;

    Gp_HaltPadScripts();
    sndEvtRequestScriptStop(SOUND_SHELTER_B2_POD_BTM_ENEMY_DRAIN_WINDUP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    work->screenDistortion = 0;
}

static void func_actor_403600_80141B60(Task* arg0)
{
    s16              nextCountdown;
    u16              countdown;
    u16              currentMp;
    Actor403600Work* work;
    PlayerStatus*    config;

    work              = arg0->work;
    countdown         = (u16)work->actionTimer - 1;
    work->actionTimer = countdown;
    if ((countdown << 0x10) <= 0) {
        config     = &gPlayerStatus;
        currentMp  = config->mp + 1;
        config->mp = currentMp;
        if ((s16)currentMp >= config->mpMax) {
            config->mp = config->mpMax;
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

static void func_actor_403600_80141C3C(Task* arg0)
{
    s16 value;

    value = ((Actor403600Work*)arg0->work)->mode;
    if (value < 0) {
        return;
    }
    if (value < 2) {
        func_actor_403600_801400BC(arg0);
    }
}

static void func_actor_403600_80141C7C(Task* arg0, s32 arg1)
{
    Enemy*           enemy;
    Actor403600Work* work;

    enemy     = arg0->spawnArg2.pointer;
    work      = arg0->work;
    enemy->hp = (u16)enemy->hp - arg1;
    worldTargetAddReadoutAmount(&enemy->node, arg1, 0);
    if (enemy->hp <= 0) {
        work->defeated = 1;
    }
}

/// Handlers for states 0-2 of the task `func_actor_403600_80141CD4` dispatches,
/// indexed by `Task::state`. The state-0 handler sets the task up and
/// advances it.
static const EnemyTaskFuncTable3 D_actor_403600_801320EC = { {
    func_actor_403600_80141D30,
    func_actor_403600_80141E78,
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
    MATRIX*          matrix;
    MATRIX*          matrix2;

    coord = arg1->extra.tmd->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }

    arg1->work                    = work;
    work->worldCoord.parent       = &gGfxViewCoord;
    matrix                        = &work->worldCoord.coord;
    MATRIX_PAIR(matrix, 0, 0)     = 0x1000;
    MATRIX_PAIR(matrix, 0, 2)     = 0;
    MATRIX_PAIR(matrix, 1, 1)     = 0x1000;
    MATRIX_PAIR(matrix, 2, 0)     = 0;
    matrix->m[2][2]               = 0x1000;
    work->worldCoord.coord.t[0]   = coord->coord.t[0];
    work->worldCoord.coord.t[1]   = coord->coord.t[1];
    workCoord                     = &work->worldCoord;
    work->worldCoord.coord.t[2]   = coord->coord.t[2];
    matrix2                       = &coord->coord;
    coord->parent                 = workCoord;
    MATRIX_PAIR(matrix2, 0, 0)    = 0x1000;
    MATRIX_PAIR(matrix2, 0, 2)    = 0;
    MATRIX_PAIR(matrix2, 1, 1)    = 0x1000;
    MATRIX_PAIR(matrix2, 2, 0)    = 0;
    matrix2->m[2][2]              = 0x1000;
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
    arg1->exitCallback          = func_actor_403600_80141F28;
    work->ambientBoost          = 0x2328;
    work->hitCooldown           = 0;
    arg1->state                += 1;
}

static void func_actor_403600_80141E78(Enemy* arg0, Task* arg1)
{
    TmdObject*       obj;
    TmdObject*       obj2;
    Actor403600Work* work;
    u16              value;

    work              = arg1->work;
    value             = work->hitCooldown + 1;
    work->hitCooldown = value;
    if ((s16)value >= 2) {
        tmdAllocPrimitiveBuffer(arg1->extra.tmd);
        obj          = arg1->extra.tmd;
        obj->flags  &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        obj2         = arg1->extra.tmd;
        obj2->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        arg1->state++;
        work->phaseFrame  = 0;
        work->sceneFrame  = 0;
        work->hitCooldown = 0x1000;
    }
}

static void func_actor_403600_80141F28(Task* arg0)
{
    arg0->extra.tmd->coords->parent = &gGfxViewCoord;
    enemyTaskExit(arg0);
}

static void func_actor_403600_80141F58(GfxCoord* arg0, s32 arg1)
{
    void**   scratch;
    SVECTOR* head;
    SVECTOR* vec;
    MATRIX*  matrix;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    vec                            = head - 1;
    SCRATCH_HEAD_AT(scratch, void) = vec;
    matrix                         = &arg0->coord;

    gte_ReadMatrixColumn(matrix, 0, vec);
    gte_lddp(arg1);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    gte_WriteMatrixColumn(vec, matrix, 0);

    gte_ReadMatrixColumn(matrix, 1, vec);
    gte_lddp(arg1);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    gte_WriteMatrixColumn(vec, matrix, 1);

    gte_ReadMatrixColumn(matrix, 2, vec);
    gte_lddp(arg1);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    gte_WriteMatrixColumn(vec, matrix, 2);

    head                           = SCRATCH_HEAD_AT(scratch, void);
    arg0->composeStamp             = GRAPHICS_COORD_DIRTY;
    SCRATCH_HEAD_AT(scratch, void) = head + 1;
}
