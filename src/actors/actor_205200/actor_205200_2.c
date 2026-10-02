#include "actor_205200_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
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

#include "rooms/shelter_b6_corridor.h"

#include "rooms/shelter_b6_training_room.h"

/// Work block of the actor's own task, allocated by its spawn handler
/// `func_actor_205200_8014BAE8`. It opens with the model's animation context
/// and its 19 playback slots and pose buffer, followed by the model's colour
/// and light matrices, the two collision objects the teardown handler
/// `func_actor_205200_8014C924` unlinks, and the state of the charge and
/// attack sub-states.
typedef struct Actor205200Work {
    /* 0x000 */ ActorAnimRig19        rig;
    /* 0x43C */ MATRIX                field_43C; // color matrix, `TmdObject.colorMtx`
    /* 0x45C */ MATRIX                field_45C; // light matrix, `TmdObject.lightMtx`
    /* 0x47C */ WorldCollisionBody    field_47C;
    /* 0x49C */ WorldCollisionContact field_49C[3];
    /* 0x4E4 */ WorldCollisionBody    field_4E4;
    /* 0x504 */ WorldCollisionContact field_504;
    /* 0x51C */ byte                  pad_51C[0x38];
    /* 0x554 */ EffectSpawnArg        field_554; // record the charge's hit effect is spawned with
    /* 0x55C */ byte                  pad_55C[0x20];
    /* 0x57C */ s16                   field_57C;
    /* 0x57E */ s16                   field_57E; // animation id the work is playing
    /* 0x580 */ u16                   field_580; // id the helper slots last saw
    /* 0x582 */ u16                   field_582; // frames spent on the current id
    /* 0x584 */ s16                   field_584; // sub-state `func_actor_205200_8014C67C` dispatches on: 0 runs the idle handler, 1 the charge handler
    /* 0x586 */ s16                   field_586; // sub-state of the charge handler `func_actor_205200_8014C748`, which arms it to 1 and clears it again
    /* 0x588 */ s16                   field_588; // non-zero while the attack body `func_actor_205200_8014C0C0` is running; the body clears it when it finishes
    /* 0x58A */ s16                   field_58A; // state of the attack body `func_actor_205200_8014C0C0`
    /* 0x58C */ u16                   field_58C; // its frame counter
    /* 0x58E */ s16                   field_58E; // sign of the player offset dotted with the player's facing axis
    /* 0x590 */ s16                   field_590; // loaded with 600 by the charge handler `func_actor_205200_8014C748` when it finishes
    /* 0x592 */ s16                   field_592; // countdown to the next random roll in `func_actor_205200_8014BF28`
    /* 0x594 */ s16                   field_594; // raised by message 0x7DB; pushes the actor to state 2
    /* 0x596 */ s16                   field_596; // placement mode; selects the tick `func_actor_205200_8014C67C` runs: zero goes to `func_shelter_b6_corridor_8017EBA4`, non-zero to `func_shelter_b6_training_room_80181930`
} Actor205200Work;
STATIC_ASSERT_SIZEOF(Actor205200Work, 0x598);

/// Animation block the attack body hands the player with message 0x3F4.
extern AnimationSet* D_actor_205200_80156800[5];
extern AnimationSet* D_actor_205200_801567E8[6];
extern s16           D_actor_205200_801567B0[];
// One collision centre for each of the two placement modes.
extern SVECTOR D_actor_205200_801567B4[2];
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, ActorCommand* request);
        s32 (*call1)(Task*, s32, s32);
    } handler;
} Actor2052002MessageEntry;
STATIC_ASSERT_SIZEOF(Actor2052002MessageEntry, 8);

extern Actor2052002MessageEntry D_actor_205200_801567D0[3];

static void func_actor_205200_8014BAE8(Enemy* enemy, Task* task);
static void func_actor_205200_8014BD4C(Task* arg0);
static void func_actor_205200_8014BF28(Task* arg0);
static void func_actor_205200_8014C0C0(Task* arg0);
static void func_actor_205200_8014C59C(Enemy* arg0, Task* arg1);
static void func_actor_205200_8014C67C(Task* arg0);
static void func_actor_205200_8014C748(Task* arg0);
static void func_actor_205200_8014C7CC(Task* arg0);
static void func_actor_205200_8014C87C(Task* arg0);
static void func_actor_205200_8014C8D4(Task* arg0);
static void func_actor_205200_8014C924(Enemy* arg0, Task* arg1);

/// The actor's own state handlers - spawn, per-frame tick and teardown - that
/// `func_actor_205200_8014C540` dispatches through by state.
static const GpEnemyTaskFuncTable3 D_actor_205200_80149E30 = {
    func_actor_205200_8014BAE8,
    func_actor_205200_8014C59C,
    func_actor_205200_8014C924,
};

static AnimationSet _gActor205200Animation08B50;
static AnimationSet _gActor205200Animation0943C;
static AnimationSet _gActor205200Animation09C1C;
static AnimationSet _gActor205200Animation0A964;
static AnimationSet _gActor205200Animation0B1AC;
static AnimationSet _gActor205200Animation0B948;
static AnimationSet _gActor205200Animation0C154;
static AnimationSet _gActor205200Animation0C968;

s32         func_actor_205200_8014C980(Task*, s32, s32);
s32         func_actor_205200_8014C9A0(Task*, s32, ActorCommand* request);
static void func_actor_205200_8014C540(Task*);

static AnimationPackedPose _gActor205200Animation08B50Bank1[29] = {
#include "assets/actor_205200_animation_08B50_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation08B50Bank4[472] = {
#include "assets/actor_205200_animation_08B50_bank4.inc"
};

static AnimationRecord _gActor205200Animation08B50Records[543] = {
#include "assets/actor_205200_animation_08B50_records.inc"
};

static u16 _gActor205200Animation08B50Indices[20] = {
#include "assets/actor_205200_animation_08B50_indices.inc"
};

static AnimationSet _gActor205200Animation08B50 = {
    _gActor205200Animation08B50Records,
    _gActor205200Animation08B50Indices,
    { NULL, _gActor205200Animation08B50Bank1, NULL, NULL, _gActor205200Animation08B50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0943CBank1[16] = {
#include "assets/actor_205200_animation_0943C_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0943CBank4[223] = {
#include "assets/actor_205200_animation_0943C_bank4.inc"
};

static AnimationRecord _gActor205200Animation0943CRecords[280] = {
#include "assets/actor_205200_animation_0943C_records.inc"
};

static u16 _gActor205200Animation0943CIndices[20] = {
#include "assets/actor_205200_animation_0943C_indices.inc"
};

static AnimationSet _gActor205200Animation0943C = {
    _gActor205200Animation0943CRecords,
    _gActor205200Animation0943CIndices,
    { NULL, _gActor205200Animation0943CBank1, NULL, NULL, _gActor205200Animation0943CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation09C1CBank1[11] = {
#include "assets/actor_205200_animation_09C1C_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation09C1CBank4[167] = {
#include "assets/actor_205200_animation_09C1C_bank4.inc"
};

static AnimationRecord _gActor205200Animation09C1CRecords[284] = {
#include "assets/actor_205200_animation_09C1C_records.inc"
};

static u16 _gActor205200Animation09C1CIndices[20] = {
#include "assets/actor_205200_animation_09C1C_indices.inc"
};

static AnimationSet _gActor205200Animation09C1C = {
    _gActor205200Animation09C1CRecords,
    _gActor205200Animation09C1CIndices,
    { NULL, _gActor205200Animation09C1CBank1, NULL, NULL, _gActor205200Animation09C1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0A964Bank1[23] = {
#include "assets/actor_205200_animation_0A964_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0A964Bank4[347] = {
#include "assets/actor_205200_animation_0A964_bank4.inc"
};

static AnimationRecord _gActor205200Animation0A964Records[414] = {
#include "assets/actor_205200_animation_0A964_records.inc"
};

static u16 _gActor205200Animation0A964Indices[20] = {
#include "assets/actor_205200_animation_0A964_indices.inc"
};

static AnimationSet _gActor205200Animation0A964 = {
    _gActor205200Animation0A964Records,
    _gActor205200Animation0A964Indices,
    { NULL, _gActor205200Animation0A964Bank1, NULL, NULL, _gActor205200Animation0A964Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0B1ACBank1[21] = {
#include "assets/actor_205200_animation_0B1AC_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0B1ACBank4[193] = {
#include "assets/actor_205200_animation_0B1AC_bank4.inc"
};

static AnimationRecord _gActor205200Animation0B1ACRecords[254] = {
#include "assets/actor_205200_animation_0B1AC_records.inc"
};

static u16 _gActor205200Animation0B1ACIndices[20] = {
#include "assets/actor_205200_animation_0B1AC_indices.inc"
};

static AnimationSet _gActor205200Animation0B1AC = {
    _gActor205200Animation0B1ACRecords,
    _gActor205200Animation0B1ACIndices,
    { NULL, _gActor205200Animation0B1ACBank1, NULL, NULL, _gActor205200Animation0B1ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0B948Bank1[20] = {
#include "assets/actor_205200_animation_0B948_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0B948Bank4[172] = {
#include "assets/actor_205200_animation_0B948_bank4.inc"
};

static AnimationRecord _gActor205200Animation0B948Records[235] = {
#include "assets/actor_205200_animation_0B948_records.inc"
};

static u16 _gActor205200Animation0B948Indices[20] = {
#include "assets/actor_205200_animation_0B948_indices.inc"
};

static AnimationSet _gActor205200Animation0B948 = {
    _gActor205200Animation0B948Records,
    _gActor205200Animation0B948Indices,
    { NULL, _gActor205200Animation0B948Bank1, NULL, NULL, _gActor205200Animation0B948Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0C154Bank1[15] = {
#include "assets/actor_205200_animation_0C154_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0C154Bank4[206] = {
#include "assets/actor_205200_animation_0C154_bank4.inc"
};

static AnimationRecord _gActor205200Animation0C154Records[244] = {
#include "assets/actor_205200_animation_0C154_records.inc"
};

static u16 _gActor205200Animation0C154Indices[20] = {
#include "assets/actor_205200_animation_0C154_indices.inc"
};

static AnimationSet _gActor205200Animation0C154 = {
    _gActor205200Animation0C154Records,
    _gActor205200Animation0C154Indices,
    { NULL, _gActor205200Animation0C154Bank1, NULL, NULL, _gActor205200Animation0C154Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0C968Bank1[14] = {
#include "assets/actor_205200_animation_0C968_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0C968Bank4[207] = {
#include "assets/actor_205200_animation_0C968_bank4.inc"
};

static AnimationRecord _gActor205200Animation0C968Records[248] = {
#include "assets/actor_205200_animation_0C968_records.inc"
};

static u16 _gActor205200Animation0C968Indices[20] = {
#include "assets/actor_205200_animation_0C968_indices.inc"
};

static AnimationSet _gActor205200Animation0C968 = {
    _gActor205200Animation0C968Records,
    _gActor205200Animation0C968Indices,
    { NULL, _gActor205200Animation0C968Bank1, NULL, NULL, _gActor205200Animation0C968Bank4, NULL, NULL, NULL },
};

s16 D_actor_205200_801567B0[2] = {
    4000,
    700,
};

SVECTOR D_actor_205200_801567B4[2] = {
    { 0, -700, -3500, 0 },
    { 0, -700, 0, 0 },
};

TaskDesc D_actor_205200_801567C4 = { { { TASK_BODY_TMD, 96 } }, func_actor_205200_8014C540, { .model = &gActor205200EveBreaMaskedBody } };

Actor2052002MessageEntry D_actor_205200_801567D0[3] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, { .call1 = func_actor_205200_8014C980 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = func_actor_205200_8014C9A0 } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

AnimationSet* D_actor_205200_801567E8[6] = {
    NULL,
    &_gActor205200Animation09C1C,
    &_gActor205200Animation08B50,
    &_gActor205200Animation0943C,
    NULL,
    &_gActor205200Animation0A964,
};

AnimationSet* D_actor_205200_80156800[5] = {
    NULL,
    &_gActor205200Animation0B1AC,
    &_gActor205200Animation0B948,
    &_gActor205200Animation0C154,
    &_gActor205200Animation0C968,
};

OverlayWaveCtx* gScreenWaveCtx = NULL;

OverlayWaveRec gScreenWaveColumns[10];

OverlayWaveRec gScreenWaveRows[30];

POLY_FT4 gScreenWaveGrid[2][30][8];

OverlayWaveCtx D_actor_205200_8015B458;

/// Spawn handler: allocates the work block, binds the model's matrices to it,
/// starts animation slots 1..18 and links the two render objects, whose
/// second one takes its offset and range from the spawn place's `field_2`.
static void func_actor_205200_8014BAE8(Enemy* enemy, Task* task)
{
    TmdObject*       tmd;
    GfxCoord*        coords;
    Actor205200Work* work;
    s32              i;

    tmd    = task->extra.tmd;
    coords = tmd->coords;
    work   = memCalloc(sizeof(Actor205200Work), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work           = work;
    tmd->flags           = 0;
    coords->composeStamp = GRAPHICS_COORD_DIRTY;
    tmd->lightMtx        = &work->field_45C;
    tmd->colorMtx        = &work->field_43C;
    enemy->field_4       = &coords->coord;
    enemy->field_48      = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord                  = &task->extra.tmd->coords[3];
    enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->recs                   = work->field_49C;
    enemy->param                  = NULL;
    enemy->hp                     = 0;
    work->field_554.coord         = &task->extra.tmd->coords[3];
    work->field_554.spawnArgLo    = 0x200;
    work->field_554.spawnArgHi    = 1;
    animationInitContext(&work->rig.anim, D_actor_205200_801567E8, tmd, work->rig.poses, work->rig.slots);
    i = 1;
    do {
        animationResetSlot(&work->rig.anim, i, 1);
        i++;
    } while (i < 0x13);
    work->field_596                  = enemy->place->mode;
    work->field_47C.pos.vy           = -300;
    work->field_47C.coord            = coords;
    work->field_47C.context.contacts = work->field_49C;
    work->field_47C.pos.vx           = 0;
    work->field_47C.pos.vz           = 0;
    work->field_47C.key              = 0x3003C;
    work->field_47C.radius           = 300;
    work->field_47C.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->field_47C);
    Gp_InitRec18Table(work->field_49C, 3, 0);
    work->field_47C.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->field_4E4.coord            = coords;
    work->field_4E4.context.contacts = &work->field_504;
    work->field_4E4.pos.vx           = D_actor_205200_801567B4[work->field_596].vx;
    work->field_4E4.pos.vy           = D_actor_205200_801567B4[work->field_596].vy;
    work->field_4E4.pos.vz           = D_actor_205200_801567B4[work->field_596].vz;
    work->field_4E4.key              = 0;
    work->field_4E4.radius           = D_actor_205200_801567B0[work->field_596];
    work->field_4E4.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->field_4E4);
    Gp_InitRec18Table(&work->field_504, 1, 0);
    work->field_4E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    task->msgTable         = D_actor_205200_801567D0;
    task->state            = 1;
}

static void func_actor_205200_8014BD4C(Task* arg0)
{
    Actor205200Work* work;
    s32              i;
    s32              found;
    s32              last;
    s32              n;

    found = 0;
    work  = arg0->work;
    last  = 0;
    SCRATCH_STACK_RESERVE_BYTES(0x10);
    if (work->field_57C != 0) {
        if (--work->field_57C <= 0) {
            work->field_57C = 0;
        }
        if (work->field_57C != 0) {
            goto end;
        }
    }
    for (i = 0; i < 3; i++) {
        if ((work->field_49C[i].key.value & 0xFFFF0000) == 0x20000) {
            func_800DA6E8(&((Enemy*)arg0->spawnArg2.pointer)->node, 0, 0);
            switch (Gp_GetIdParam0(work->field_49C[i].key.value) & 0xFFFF) {
                case 1:
                    found = 1;
                    break;
                case 2:
                    break;
            }
            if (found == 0) {
                break;
            }
            work->field_584 = 1;
            work->field_586 = 0;
            if (last != work->field_49C[i].key.value) {
                last = work->field_49C[i].key.value;
                func_800FDB18(Gp_GetIdParam1(last) & 0xFFFF, &arg0->extra.tmd->coords[3], NULL,
                              &work->field_554);
            }
            if ((n = Gp_GetIdParam2(work->field_49C[i].key.value)) > 0) {
                work->field_57C = n;
            }
        }
    }
end:
    Gp_ClearRec18Occupied(work->field_49C);
    if (work->field_504.flags & 1) {
        if ((work->field_504.key.value & 0xFFFF0000) == 0x10000 && gPlayerStatus.hp > 0) {
            work->field_588      = 1;
            Gp_StateC08.field_6 |= 1;
        }
        Gp_ClearRec18Occupied(&work->field_504);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Charge-handler sub-state machine. States 0 and 3 share a random roll: every
/// 15-46 frames a 1-in-8 draw switches to state 4 with animation 5. State 2
/// waits out the animation, raising bit 2 of `gSceneCombatState.pairedEnemySignals` on frame 60,
/// and state 4 returns to 3 while the `field_590` cooldown is still running.
static void func_actor_205200_8014BF28(Task* arg0)
{
    Actor205200Work* work;
    s16              next;

    work = arg0->work;
    switch (work->field_586) {
        case 0:
            if (gSceneCombatState.pairedEnemySignals & SCENE_COMBAT_PAIRED_HEAL_REQUEST) {
                gSceneCombatState.pairedEnemySignals &= (0xFF ^ SCENE_COMBAT_PAIRED_HEAL_REQUEST);
                work->field_586                       = 1;
            }
        tick:
            if (--work->field_592 <= 0) {
                work->field_592 = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x1F) + 0xF;
                if (!(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 7)) {
                    work->field_586 = 4;
                    work->field_57E = 5;
                }
            }
            break;
        case 1:
            work->field_57E = 2;
            work->field_586 = 2;
            break;
        case 2:
            if ((s16)work->field_582 == 0x3C) {
                gSceneCombatState.pairedEnemySignals |= SCENE_COMBAT_PAIRED_HEAL_READY;
            }
            if ((s16)work->field_582 >= 0x54) {
                work->field_57E = 1;
                work->field_586 = 3;
                work->field_590 = 0x258;
            }
            break;
        case 3:
            if (--work->field_590 <= 0) {
                work->field_586 = 0;
            }
            goto tick;
        case 4:
            if ((s16)work->field_582 >= 0x40) {
                next = 0;
                if (work->field_590 > 0) {
                    next = 3;
                }
                work->field_586 = next;
                work->field_57E = 1;
            }
            break;
    }
}

/// The attack body, run while `field_588` is set. It carves an
/// `ActorAttackScratch` from the scratch stack and steps `field_58A`:
/// state 0 records which side of the player it is on (`field_58E`), plays its grab
/// animation and spawns the effect; state 1 drags the player towards the actor
/// for 0x10 frames and hands over after 0x1E/0x20; state 2 waits for the
/// animation to finish and clears `field_588`. The duplicated calls in the
/// `field_596` arms are what the target's shared tails need: jump2's
/// cross-jumping merges them, where a variable or ternary is hoisted instead.
static void func_actor_205200_8014C0C0(Task* arg0)
{
    Actor205200Work*    work;
    GfxCoord*           coord;
    Task*               player;
    GfxCoord*           target;
    ActorAttackScratch* scratch;
    void*               head;
    s32                 sound;
    s32                 count;

    work                       = arg0->work;
    player                     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = (u8*)head - sizeof(ActorAttackScratch);
    scratch                    = SCRATCH_STACK_CURSOR(ActorAttackScratch);
    coord                      = arg0->extra.tmd->coords;
    target                     = player->extra.tmd->coords;

    switch (work->field_58A) {
        case 0:
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
                scratch->delta.vx                  = target->coord.t[0] - coord->coord.t[0];
                scratch->delta.vy                  = 0;
                scratch->delta.vz                  = target->coord.t[2] - coord->coord.t[2];
                work->field_58E                    = (scratch->delta.vx * target->coord.m[0][2] + scratch->delta.vz * target->coord.m[2][2]) > 0;
                scratch->anim.source.sets          = D_actor_205200_80156800;
                scratch->anim.animationId          = work->field_58E + 1;
                scratch->anim.blend                = ANIMATION_BLEND_RESET;
                scratch->anim.blendFrames          = 0;
                scratch->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, scratch, 0);
                work->field_58A = 1;
                work->field_58C = 0;
                Gp_SpawnPadLerp(0xF, 0xFF, 0x80);
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 7;
                SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                scratch->dir.vx = 0;
                scratch->dir.vy = -1000;
                scratch->dir.vz = 0;
                if (work->field_596 == 0) {
                    Gp_SpawnEff(EFFECT_SHELTER_B6_CORRIDOR_PLAYER_HIT_RING, player->extra.tmd->coords, 0, &scratch->dir);
                } else {
                    Gp_SpawnEff(EFFECT_SHELTER_B6_TRAINING_ROOM_HIT_FLASH, player->extra.tmd->coords, 0, &scratch->dir);
                }
            } else {
                work->field_588 = 0;
            }
            break;
        case 1:
            if ((s16)work->field_58C < 0x10) {
                scratch->delta.vx = target->coord.t[0] - coord->coord.t[0];
                scratch->delta.vy = target->coord.t[1] - coord->coord.t[1];
                scratch->delta.vz = target->coord.t[2] - coord->coord.t[2];
                VectorNormalS(&scratch->delta, &scratch->dir);
                scratch->place.pos.vx = target->coord.t[0] + ((scratch->dir.vx * 25) >> 10);
                scratch->place.pos.vy = 0;
                scratch->place.pos.vz = target->coord.t[2] + ((scratch->dir.vz * 25) >> 10);
                scratch->place.rot.vx = 0;
                if (work->field_58E == 0) {
                    scratch->place.rot.vy = (ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
                } else {
                    scratch->place.rot.vy = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                }
                scratch->place.rot.vz = 0;
                TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &scratch->place, 0);
            }
            if ((s16)work->field_58C == 0x10) {
                if (work->field_596 == 0) {
                    sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x55180002;
                    SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x55190003;
                    SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            count = (s16)++work->field_58C;
            if ((work->field_58E != 0 && count >= 0x1E) || (work->field_58E == 0 && count >= 0x20)) {
                scratch->anim.source.sets          = D_actor_205200_80156800;
                scratch->anim.animationId          = work->field_58E + 3;
                scratch->anim.blend                = ANIMATION_BLEND_RESET;
                scratch->anim.blendFrames          = 0;
                scratch->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, scratch, 0);
                work->field_58A = 2;
                work->field_58C = 0;
            }
            break;
        case 2:
            if ((s16)++work->field_58C >= 0x25) {
                if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                    taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    work->field_58A = 0;
                    work->field_58C = 0;
                    work->field_588 = 0;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorAttackScratch));
}

/// Update of the actor's own task: runs the handler of
/// `D_actor_205200_80149E30` that `Task::state` selects, through a stack copy
/// of the table.
static void func_actor_205200_8014C540(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_205200_80149E30;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

static void func_actor_205200_8014C59C(Enemy* arg0, Task* arg1)
{
    GfxCoord*        coord;
    TmdObject*       obj;
    Actor205200Work* work;
    s32              state;

    work  = arg1->work;
    obj   = arg1->extra.tmd;
    coord = obj->coords;
    if (gGameSession->eventState != 0) {
        return;
    }
    if (work->field_594 != 0) {
        arg1->state = 2;
        return;
    }
    state = gSceneCombatState.actorControl;
    if (state == 1) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->flags = 0;
    goto default_body;
case2:
    obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    return;
default_body:
    func_actor_205200_8014BD4C(arg1);
    func_actor_205200_8014C67C(arg1);
    func_actor_205200_8014C7CC(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
case1:
    func_actor_205200_8014C87C(arg1);
    func_actor_205200_8014C8D4(arg1);
}

/// Per-frame tick of the live state, run from `func_actor_205200_8014C59C`'s
/// shared body. Bit 0 of `gSceneCombatState.pairedEnemySignals` is a one-shot re-arm: it clears
/// itself and drops the actor back to sub-state 1 with the sub-state-0x586
/// counter restarted, which is what `func_actor_205200_8014C748` drives. The
/// sub-state at 0x584 then picks the idle or the charge handler, the halfword at
/// 0x596 which of the two shared ticks follows, and the flag at 0x588 keeps the
/// attack body running until that body clears it itself.
static void func_actor_205200_8014C67C(Task* arg0)
{
    Actor205200Work* work;

    work = arg0->work;
    if (gSceneCombatState.pairedEnemySignals & SCENE_COMBAT_PAIRED_CHARGE_REQUEST) {
        gSceneCombatState.pairedEnemySignals &= (0xFF ^ SCENE_COMBAT_PAIRED_CHARGE_REQUEST);
        work->field_584                       = 1;
        work->field_586                       = 0;
    }
    switch (work->field_584) {
        case 0:
            func_actor_205200_8014BF28(arg0);
            break;
        case 1:
            func_actor_205200_8014C748(arg0);
            break;
    }
    if (work->field_596 == 0) {
        func_shelter_b6_corridor_8017EBA4(arg0);
    } else {
        func_shelter_b6_training_room_80181930(arg0);
    }
    if (work->field_588 != 0) {
        func_actor_205200_8014C0C0(arg0);
    }
}

/// Charge handler, sub-state 1 of `func_actor_205200_8014C67C`. On entry it
/// switches the animation to id 3 and raises bit 3 of `gSceneCombatState.pairedEnemySignals`;
/// once the frame counter reaches 35 it plays id 1, parks the charge sub-state
/// at 3, drops back to the idle handler and loads 600 into `field_590`.
static void func_actor_205200_8014C748(Task* arg0)
{
    Actor205200Work* work;
    s16              state;

    work  = arg0->work;
    state = work->field_586;
    switch (state) {
        case 0:
            work->field_57E                       = 3;
            work->field_586                       = 1;
            gSceneCombatState.pairedEnemySignals |= SCENE_COMBAT_PAIRED_RESET_REQUEST;
            return;
        case 1:
            if ((s16)work->field_582 >= 0x23) {
                work->field_586 = 3;
                work->field_57E = 1;
                work->field_584 = 0;
                work->field_590 = 0x258;
            }
            return;
    }
}

/// Keeps the work's animation id bound to its helper slots. When the id has
/// changed since the last tick the remembered id follows it, the frame counter
/// at 0x582 restarts and every slot 1..18 is pointed at the new id at weight 8;
/// otherwise the counter ticks and the slots are simply advanced.
static void func_actor_205200_8014C7CC(Task* arg0)
{
    Actor205200Work* work;
    s32              i;

    work = arg0->work;
    if (work->field_57E != (s16)work->field_580) {
        work->field_580 = work->field_57E;
        work->field_582 = 0;
        for (i = 1; i < 0x13; i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->field_57E, 0, 8);
        }
    } else {
        work->field_582++;
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// Feeds the actor's world position - the translation of its attach
/// coordinate - to `Gp_UpdateActorColor` for its enemy record, with no blend
/// parameters.
static void func_actor_205200_8014C87C(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

/// Draws the ground quad under the actor at its attach coordinate's world
/// position.
static void func_actor_205200_8014C8D4(Task* arg0)
{
    GfxCoord* coord;
    VECTOR3   vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x180, 0x80);
}

/// Teardown state: unlinks the enemy's lock-on node and the work's two
/// collision objects, then destroys the enemy.
static void func_actor_205200_8014C924(Enemy* arg0, Task* arg1)
{
    Actor205200Work* work;

    work = arg1->work;
    worldTargetUnlinkNode(&arg0->node);
    Gp_UnlinkObj(&work->field_47C);
    Gp_UnlinkObj(&work->field_4E4);
    enemyDestroy(arg0, arg1);
}

/// Message 0x7D5 handler, listed in `D_actor_205200_801567D0` beside the 0x7DB
/// one: `arg2` zero sets the 0x80 flag of the task's model and any other value
/// clears its flags. The opcode itself (`msgId`) is unused.
s32 func_actor_205200_8014C980(Task* task, s32 msgId, s32 arg2)
{
    TmdObject* tmd;

    tmd = task->extra.tmd;
    if (arg2 == 0) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        tmd->flags = 0;
    }
    return 0;
}

/// Message 0x7DB handler, listed in `D_actor_205200_801567D0` next to the
/// 0x7D5 one. A non-zero payload halfword sets `Actor205200Work.field_594`, the
/// flag `func_actor_205200_8014C59C` tests to push the actor to state 2.
/// Nothing reads the opcode itself, hence `arg1`.
s32 func_actor_205200_8014C9A0(Task* arg0, s32 arg1, ActorCommand* request)
{
    Actor205200Work* work;

    work = arg0->work;
    if (request->command != 0) {
        work->field_594 = 1;
    }
    return 0;
}
