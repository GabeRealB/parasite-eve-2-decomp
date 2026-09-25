#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "gameplay/pairsrc.h"

#include "actors/actors_shared_80133cd0.h"
#include "actors/actors_shared_80134810.h"
#include "actors/actors_shared_80135b58.h"
#include "actors/actors_shared_801673f8.h"

/// The 0x2E4-byte work block of the package's first enemy, which both of its
/// spawn handlers allocate with `memCalloc` and park in `Task::work`. After the
/// animation context and its three slots come the colour and light matrices the
/// model is pointed at, then four `GpObj` bodies, each followed by the
/// `GpRec18` table its `ctx.recs` names.
typedef struct Actor104600Work {
    /* 0x000 */ GpAnimCtx  context;
    /* 0x014 */ GpAnimSlot slots[3];
    /* 0x08C */ byte       field_8C[0x30]; // pose buffer handed to func_800B3F84
    /* 0x0BC */ MATRIX     field_BC;       // colour matrix, TmdObject::colorMtx
    /* 0x0DC */ MATRIX     field_DC;       // light matrix, TmdObject::lightMtx
    /* 0x0FC */ GpObj      objFC;
    /* 0x11C */ GpRec18    rec11C;
    /* 0x134 */ GpObj      obj134;
    /* 0x154 */ GpRec18    rec154[4]; // the body's contact table; also the enemy's `recs`
    /* 0x1B4 */ GpObj      obj1B4;
    /* 0x1D4 */ GpRec18    rec1D4;
    /* 0x1EC */ GpObj      obj1EC;
    /* 0x20C */ GpRec18    rec20C;
    /* 0x224 */ byte       pad_224[0x50];
    /* 0x274 */ VECTOR3    field_274; // root translation before the last step
    /* 0x280 */ byte       pad_280[4];
    /* 0x284 */ GpEffArg   field_284; // hit-effect coordinate and parameters
    /* 0x28C */ MATRIX     field_28C; // root transform saved when the enemy dies
    /* 0x2AC */ s32        field_2AC; // scale factor of the model's second part
    /* 0x2B0 */ s16        field_2B0; // heading, stepped 0x20 a frame toward the player
    /* 0x2B2 */ s16        field_2B2; // reaction state the per-frame dispatch switches on
    /* 0x2B4 */ s16        field_2B4; // phase of the death sequence
    /* 0x2B6 */ s16        field_2B6; // frames spent in the death phase
    /* 0x2B8 */ s16        field_2B8; // animation id the work is playing
    /* 0x2BA */ s16        field_2BA; // id the two helper slots last saw
    /* 0x2BC */ u16        field_2BC; // frames spent on the current id
    /* 0x2BE */ s16        field_2BE; // step length along the facing
    /* 0x2C0 */ byte       pad_2C0[6];
    /* 0x2C6 */ s16        field_2C6;
    /* 0x2C8 */ s16        field_2C8; // live stage: 1 alive, 2 dying
    /* 0x2CA */ s16        field_2CA; // Y scale folded onto the saved transform
    /* 0x2CC */ s16        field_2CC;
    /* 0x2CE */ s16        field_2CE; // remaining hit cooldown
    /* 0x2D0 */ u16        field_2D0; // frames until the next idle sound
    /* 0x2D2 */ s16        field_2D2; // non-zero: the animation rebind is suppressed
    /* 0x2D4 */ u16        field_2D4; // frame or event counter of the dying stages
    /* 0x2D6 */ s16        field_2D6; // spawn arg's low half; picks the sound set
    /* 0x2D8 */ s16        field_2D8; // latched once the dormant enemy is touched
    /* 0x2DA */ s16        field_2DA; // non-zero: the death spawns a final effect
    /* 0x2DC */ s16        field_2DC; // spawn arg's high half
    /* 0x2DE */ s16        field_2DE; // fall speed while dropping into place
    /* 0x2E0 */ s16        field_2E0; // non-zero once the drop has hit something
    /* 0x2E2 */ s16        field_2E2; // non-zero: the drop has been armed
} Actor104600Work;
STATIC_ASSERT_SIZEOF(Actor104600Work, 0x2E4);

/// The 0x18 bytes `Actor04600_Fn00FD8` takes off the scratch stack: `vec` is
/// the offset from the model's coordinate to the player and `rot` the rotation
/// rebuilt from the new heading.
typedef struct Actor104600RotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR rot;
} Actor104600RotScratch;
STATIC_ASSERT_SIZEOF(Actor104600RotScratch, 0x18);

/// The 0x4C bytes `Actor04600_Fn00978` takes off the scratch stack: `delta`
/// receives the `func_800E0C10` push-back and is then reused for offsets,
/// `normal` is the normalized wall offset, and `result` is the word
/// `func_800E0C10` reports through its last argument.
typedef struct Actor104600ContactScratch {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ GpDeltaScratch delta;
    /* 0x30 */ byte           pad_30[8];
    /* 0x38 */ VECTOR         normal;
    /* 0x48 */ s32            result;
} Actor104600ContactScratch;
STATIC_ASSERT_SIZEOF(Actor104600ContactScratch, 0x4C);

/// The 0x2B0-byte work block of the package's second enemy, allocated by its
/// spawn handler and parked in `Task::work`. It carries three `GpObj` bodies:
/// the first points its `ctx.d4rec` at the `GpActorD4Rec` after it, the other
/// two point at their own `GpRec18` tables.
typedef struct Actor104600Enemy2Work {
    /* 0x000 */ GpAnimCtx    context;
    /* 0x014 */ GpAnimSlot   slots[3];
    /* 0x08C */ byte         field_8C[0x30]; // pose buffer handed to func_800B3F84
    /* 0x0BC */ MATRIX       field_BC;       // colour matrix, TmdObject::colorMtx
    /* 0x0DC */ MATRIX       field_DC;       // light matrix, TmdObject::lightMtx
    /* 0x0FC */ GpObj        field_FC;
    /* 0x11C */ GpActorD4Rec field_11C;
    /* 0x134 */ GpRec18      field_134[1];
    /* 0x14C */ GpObj        field_14C;
    /* 0x16C */ GpRec18      field_16C[1];
    /* 0x184 */ GpObj        field_184;
    /* 0x1A4 */ GpRec18      field_1A4[4]; // the enemy's `recs`
    /* 0x204 */ byte         pad_204[0x50];
    /* 0x254 */ s32          field_254;    // position restored when the push-back conflicts
    /* 0x258 */ s32          field_258;
    /* 0x25C */ s32          field_25C;
    /* 0x260 */ byte         pad_260[4];
    /* 0x264 */ MATRIX       field_264; // root transform the dying enemy refolds
    /* 0x284 */ byte         pad_284[2];
    /* 0x286 */ s16          field_286; // reaction state
    /* 0x288 */ s16          field_288; // non-zero once the death has unlinked the bodies
    /* 0x28A */ s16          field_28A; // frames spent in the current state
    /* 0x28C */ s16          field_28C; // animation id the work is playing
    /* 0x28E */ s16          field_28E; // id the two helper slots last saw
    /* 0x290 */ s16          field_290; // frames spent on the current id
    /* 0x292 */ s16          field_292;
    /* 0x294 */ byte         pad_294[6];
    /* 0x29A */ s16          field_29A;
    /* 0x29C */ byte         pad_29C[4];
    /* 0x2A0 */ s16          field_2A0; // Y scale folded onto the saved transform
    /* 0x2A2 */ byte         pad_2A2[2];
    /* 0x2A4 */ s16          field_2A4; // light blend, 0..0x12
    /* 0x2A6 */ s16          field_2A6; // non-zero: the blend is rising
    /* 0x2A8 */ s16          field_2A8; // frames until the next blend turn
    /* 0x2AA */ s16          field_2AA; // latched by a hit
    /* 0x2AC */ s16          field_2AC; // placement mode; picks the sound set
    /* 0x2AE */ byte         pad_2AE[2];
} Actor104600Enemy2Work;
STATIC_ASSERT_SIZEOF(Actor104600Enemy2Work, 0x2B0);

/// 0x38-byte block `Actor04600_Fn0346C` takes from `G_SCRATCH_HEAD`:
/// `delta` receives the `func_800E0C10` push-back and is then reused for the
/// offset to the player.
typedef struct Actor104600HitScratch {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ GpDeltaScratch delta;
    /* 0x30 */ byte           pad_30[8];
} Actor104600HitScratch;
STATIC_ASSERT_SIZEOF(Actor104600HitScratch, 0x38);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

#define ACTOR_COPY_MATRIX_COLUMN_TO_SV(r0, r1, o0, o1, o2) \
    __asm__ volatile(                                      \
        "lhu $12, %2(%0);"                                 \
        "lhu $13, %3(%0);"                                 \
        "lhu $14, %4(%0);"                                 \
        "sh $12, 0(%1);"                                   \
        "sh $13, 2(%1);"                                   \
        "sh $14, 4(%1)"                                    \
        :                                                  \
        : "r"(r0), "r"(r1), "i"(o0), "i"(o1), "i"(o2)      \
        : "$12", "$13", "$14", "memory")

#define ACTOR_COPY_SV_TO_MATRIX_COLUMN(r0, r1, o0, o1, o2) \
    __asm__ volatile(                                      \
        "lhu $12, 0(%0);"                                  \
        "lhu $13, 2(%0);"                                  \
        "lhu $14, 4(%0);"                                  \
        "sh $12, %2(%1);"                                  \
        "sh $13, %3(%1);"                                  \
        "sh $14, %4(%1)"                                   \
        :                                                  \
        : "r"(r0), "r"(r1), "i"(o0), "i"(o1), "i"(o2)      \
        : "$12", "$13", "$14", "memory")

/// Payload a message sender hands the first enemy's message handler as its
/// third argument; the handler reads only the halfword at 0x2, as a command
/// word whose low byte is the mode and whose bits 8..11 pick a spawn point.
typedef struct Actor104600Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor104600Msg;

extern u8 D_801153F2[2];
extern u8 D_801153F4;

/// The first enemy's pair table, packed into its third body's key, and the
/// enemy record whose `pairTable` names it; `hpMax` seeds the enemy's HP.
extern GpU16Pair  Actor04600_D0415C;
extern GpPairSrcE Actor04600_D04160;

/// The two script arguments the first enemy's death hands to
/// `Gp_SpawnScript18`.
extern u32 Actor04600_D04170[];
extern u32 Actor04600_D0417C[];

/// Message table the dropping first enemy's spawn parks in `Task::msgTable`.
extern u8 Actor04600_D05868[];

/// The animation data `func_800B3F84` seeds the first enemy's slots from.
extern u8 Actor04600_D05890[];

/// Offset of the 0x60030 effect the first enemy's death spawns.
extern SVECTOR Actor04600_D058A0;

/// Offset of the 0x60080 effect the collapsing first enemy spawns.
extern SVECTOR Actor04600_D058A8;

/// The second enemy's record; `hpMax` seeds its HP.
extern GpPairSrcE Actor04600_D058B4;

/// The animation data `func_800B3F84` seeds the second enemy's slots from.
extern u8 Actor04600_D064A8[];

/// Offsets of the spark and hit effects the second enemy's hit handler spawns.
extern SVECTOR Actor04600_D064B4;
extern SVECTOR Actor04600_D064BC;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);
void    Gp_UpdateCoord(GsCOORDINATE2* arg0);

void Actor04600_Fn005B0(Task* arg0);
void Actor04600_Fn007B0(Task* arg0);
void Actor04600_Fn00EC8(Task* arg0, s32 arg1);
void Actor04600_Fn00FD8(Task* arg0);
void Actor04600_Fn01110(GpEnemy* enemy, Task* task);
void Actor04600_Fn01604(Task* arg0, u8 arg1);
void Actor04600_Fn017CC(GpEnemy* arg0, Task* arg1);
void Actor04600_Fn01AFC(GpEnemy* arg0, Task* arg1);
void Actor04600_Fn01E0C(Task* arg0);
void Actor04600_Fn02500(GpEnemy* arg0, Task* arg1);
void Actor04600_Fn02618(Task* arg0);
void Actor04600_Fn0272C(Task* task);
void Actor04600_Fn027BC(Task* arg0);
void Actor04600_Fn02870(GpEnemy* arg0, Task* task);
void Actor04600_Fn028E0(Task* task);
void Actor04600_Fn0294C(Task* arg0, GsCOORDINATE2* arg1);
void Actor04600_Fn02B14(Task* arg0);
void Actor04600_Fn02C08(Task* task);
void Actor04600_Fn02CD4(Task* task);
void Actor04600_Fn03BDC(GpEnemy* arg0, Task* arg1);
void Actor04600_Fn03CEC(Task* arg0);
void Actor04600_Fn03D54(Task* task);
void Actor04600_Fn03E10(Task* arg0);
void Actor04600_Fn03EC0(GpEnemy* arg0, Task* task);
void Actor04600_Fn03F30(Task* task);
void Actor04600_Fn0400C(Task* arg0);
void Actor04600_Fn04100(Task* task);

/// Rebinds the first enemy's animation id to its two helper slots unless
/// `field_2D2` suppresses it: a changed id is remembered, its frame count
/// restarts and both slots switch to it; otherwise the count ticks and the
/// slots advance.
static __inline__ void Actor04600_TickAnim(Task* task)
{
    Actor104600Work* work = (Actor104600Work*)task->work;
    s32              i;
    if (work->field_2D2 == 0) {
        if (work->field_2B8 != work->field_2BA) {
            work->field_2BA = work->field_2B8;
            work->field_2BC = 0;
            for (i = 1; i < 3; i++) {
                func_800B4114((GpAnimCtx*)work, i, work->field_2B8, 0, 0);
            }
        } else {
            work->field_2BC++;
            for (i = 1; i < 3; i++) {
                Gp_AnimTickIndex((GpAnimCtx*)work, i);
            }
        }
    }
}

/// Colours the enemy from the world position of `coord`, staged in a `VECTOR`
/// taken off the scratch stack.
static __inline__ void Actor04600_UpdateColor(GpEnemy* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block         = (VECTOR*)(*(u8**)0x1F8003FC - 0x10);
    block->vx             = coord->workm.t[0];
    block->vy             = coord->workm.t[1];
    block->vz             = coord->workm.t[2];
    *(VECTOR**)0x1F8003FC = block;
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)0x1F8003FC += 0x10;
}

/// Spawn handler of the first enemy, entry 0 of `Actor04600_D00004`. A spawn
/// arg whose high halfword is 1 destroys the enemy instead. Otherwise it
/// allocates the 0x2E4-byte work block, points the model's light and colour
/// matrices into it, links the enemy's node, seeds the animation context and
/// resets slots 1 and 2, then links the four bodies with their contact tables
/// and installs `Actor04600_Fn02C08` as the exit callback. The spawn arg's two
/// halves are kept in `field_2DC`/`field_2D6`; a low half of 1 matching the
/// task's `spawnType` steps the model's texture page and CLUT row and
/// re-streams it twice.
void Actor04600_Fn00048(GpEnemy* arg0, Task* arg1)
{
    Actor104600Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   part;
    u16              v;
    s32              i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->coords;
    part  = &coord[1];
    if ((s16)(arg1->spawnArg1 >> 16) == 1) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    work = memCalloc(0x2E4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = (TaskIdMap*)work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_DC;
    obj->colorMtx  = &work->field_BC;
    arg0->field_4  = &coord[1].coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord      = part;
    arg0->node.flags = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &Actor04600_D04160;
    arg0->recs       = &work->rec154[0];
    arg0->hp         = Actor04600_D04160.hpMax;
    func_800B3F84((GpAnimCtx*)work, Actor04600_D05890, obj, work->field_8C, (GpAnimSlot*)work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
        i += 1;
    } while (i < 3);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_2B8            = 1;
    work->field_2BA            = 1;
    work->field_2AC            = 0x1000;
    work->field_2DA            = 0;
    work->field_2CE            = 0;
    work->field_2D4            = 0;
    work->field_2D2            = 0;
    work->field_2CC            = 0;
    arg1->killCountdown        = 0;
    work->field_284.coord      = &((TmdObject*)arg1->extra)->coords[1];
    work->field_284.spawnArgLo = 0x100;
    work->field_284.spawnArgHi = 1;
    work->objFC.coord          = coord;
    work->objFC.ctx.recs       = &work->rec11C;
    work->objFC.pos.vx         = 0;
    work->objFC.pos.vy         = 0;
    work->objFC.pos.vz         = 0;
    work->objFC.key            = 0;
    work->objFC.radius         = 0xBB8;
    work->objFC.flags          = 1U;
    Gp_LinkObj(3, &work->objFC);
    Gp_InitRec18Table(&work->rec11C, 1, 0);
    work->obj134.coord    = coord;
    work->obj134.ctx.recs = &work->rec154[0];
    work->obj134.pos.vx   = 0;
    work->obj134.pos.vy   = -0xC8;
    work->obj134.pos.vz   = 0;
    work->obj134.key      = 0x3002E;
    work->obj134.radius   = 0xC8;
    work->obj134.flags    = 1U;
    work->objFC.flags     = (u16)(work->objFC.flags | 0x8000);
    Gp_LinkObj(2, &work->obj134);
    Gp_InitRec18Table(&work->rec154[0], 4, 0);
    work->obj1B4.coord    = coord;
    work->obj1B4.ctx.recs = &work->rec1D4;
    work->obj1B4.pos.vx   = 0;
    work->obj1B4.pos.vy   = 0;
    work->obj1B4.pos.vz   = 0;
    work->obj134.flags    = (u16)(work->obj134.flags | 0xC200);
    work->obj1B4.key      = Gp_PackPair(&Actor04600_D0415C, 0);
    work->obj1B4.radius   = 0x3E8;
    work->obj1B4.flags    = 1U;
    Gp_LinkObj(3, &work->obj1B4);
    Gp_InitRec18Table(&work->rec1D4, 1, 0);
    work->obj1EC.coord    = coord;
    work->obj1EC.ctx.recs = &work->rec20C;
    work->obj1EC.pos.vx   = 0;
    work->obj1EC.pos.vy   = 0;
    work->obj1EC.pos.vz   = 0;
    work->obj1EC.key      = 0x22323;
    work->obj1EC.radius   = 0x3E8;
    work->obj1EC.flags    = 1U;
    work->obj1B4.flags    = (u16)(work->obj1B4.flags & 0x7FFF);
    Gp_LinkObj(8, &work->obj1EC);
    Gp_InitRec18Table(&work->rec20C, 1, 0);
    work->obj1EC.flags = (u16)(work->obj1EC.flags & 0x7FFF);
    work->field_2DC    = (s16)(arg1->spawnArg1 >> 16);
    v                  = (u16)arg1->spawnArg1;
    work->field_2D6    = v;
    if ((s16)v == 1 && arg1->spawnType == (s16)v) {
        obj->tpage = obj->tpage + 1;
        obj->clut  = obj->clut + 1;
        if (obj->buffer != 0) {
            tmdProcessStream(obj);
            tmdProcessStream(obj);
        }
    }
    work->field_2B4    = 0;
    arg1->exitCallback = Actor04600_Fn02C08;
    arg1->state       += 1;
}

/// Task states of the first enemy as `Actor04600_Fn024A4` dispatches them:
/// spawn, per-frame update and death.
const GpEnemyTaskFuncTable3 Actor04600_D00004 = {
    { Actor04600_Fn00048, Actor04600_Fn02500, Actor04600_Fn01110 },
};

/// Task states of the dropping first enemy as `Actor04600_Fn02C6C` dispatches
/// them: the same update and death after a spawn that parks the enemy hidden,
/// and a fourth state for its drop into place.
const GpEnemyTaskFuncTable4 Actor04600_D00010 = {
    { Actor04600_Fn017CC, Actor04600_Fn02500, Actor04600_Fn01110, Actor04600_Fn01AFC },
};

/// Per-frame dispatch of the first enemy on its reaction state `field_2B2`:
/// 0 is the dormant arm `Actor04600_Fn005B0` and 1 the live handler
/// `Actor04600_Fn007B0`. State 3 suppresses the rebind until
/// `Gp_TickObjFlag2` reports the reaction over, then returns the enemy to the
/// live stage, and ends with a step of the root. States 4 and 5 collapse the
/// enemy: both scale its second part at the base factor, count frames and
/// spawn the 0x60080 effect every 0x10; state 5 also counts those spawns and,
/// on the third, arms the death - a five-frame countdown, the death phase
/// reset and task state 2, with the enemy's HP cleared. Both collapse states
/// end by suppressing the rebind.
void Actor04600_Fn003D4(Task* arg0)
{
    Actor104600Work* work;
    GpEnemy*         enemy;
    u16              frames;

    work = (Actor104600Work*)arg0->work;
    switch (work->field_2B2) {
        case 0:
            Actor04600_Fn005B0(arg0);
            return;
        case 1:
            Actor04600_Fn007B0(arg0);
            return;
        case 3:
            work->field_2D2 = 1;
            if (Gp_TickObjFlag2((GpObj5D*)arg0->spawnArg2) != 0) {
                work->field_2D2 = 0;
                work->field_2B2 = 1;
                work->field_2C8 = 1;
                work->field_2BE = 0;
            }
            Actor04600_Fn0272C(arg0);
            return;
        case 4:
            work->field_2AC = 0x1000;
            Actor04600_Fn0294C(arg0, &((TmdObject*)arg0->extra)->coords[1]);
            frames          = work->field_2BC + 1;
            work->field_2BC = frames;
            if ((s16)frames >= 0x10) {
                Gp_SpawnEff(0x60080, ((TmdObject*)arg0->extra)->coords, 0x400, &Actor04600_D058A8);
                work->field_2BC = 0;
            }
            goto suppress_rebind;
        default:
            return;
        case 5:
            work->field_2AC = 0x1000;
            Actor04600_Fn0294C(arg0, &((TmdObject*)arg0->extra)->coords[1]);
            frames          = work->field_2BC + 1;
            work->field_2BC = frames;
            if ((s16)frames >= 0x10) {
                Gp_SpawnEff(0x60080, ((TmdObject*)arg0->extra)->coords, 0x400, &Actor04600_D058A8);
                work->field_2BC = 0;
                frames          = work->field_2D4 + 1;
                work->field_2D4 = frames;
                if ((s16)frames >= 3) {
                    enemy               = arg0->spawnArg2;
                    arg0->killCountdown = 5;
                    work->field_2B4     = 0;
                    arg0->state         = 2;
                    enemy->hp           = 0;
                }
            }
        suppress_rebind:
            work->field_2D2 = 1;
    }
}

/// Dormant arm of the first enemy's reaction dispatch. A 0x10000-class contact
/// on the record at `rec11C` latches `field_2D8`; a latched enemy moves to the
/// live stage, drops the 0x8000 bit of its first body and arms state 0xF0. The
/// record is released either way. While animation 1 plays, `field_2D0` counts
/// down to an idle sound (re-rolled to 0x50..0xB3 frames, `field_2D6` picking
/// the sound set), the step length follows the frame count - 0x14 in the first
/// window, -0x14 in the second - the count wraps at 0x63, and the root takes one
/// step. Eight bytes of the scratch stack are held across the whole arm.
void Actor04600_Fn005B0(Task* arg0)
{
    Actor104600Work* work;
    GsCOORDINATE2*   coord;
    u16              countdown;
    s32              soundId;
    u32              rng;

    coord              = ((TmdObject*)arg0->extra)->coords;
    work               = (Actor104600Work*)arg0->work;
    *(u32*)0x1F8003FC -= 8;
    if (Gp_CountRec18Hi(&work->rec11C, 0x10000) != 0) {
        work->field_2D8 = 1;
    }
    if (work->field_2D8 != 0) {
        work->field_2B2   = 1;
        work->field_2C8   = 1;
        work->objFC.flags = (u16)(work->objFC.flags & 0x7FFF);
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(&work->rec11C);
    if (work->field_2B8 == 1) {
        countdown       = work->field_2D0 - 1;
        work->field_2D0 = countdown;
        if ((countdown << 16) <= 0) {
            rng             = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState     = rng;
            work->field_2D0 = (u16)((rng >> 16) % 100 + 0x50);
            if (work->field_2D6 != 0) {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x40460009;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            } else {
                soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | 0x402E0001;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
        }
        work->field_2C6 = 1;
        work->field_2BE = 0;
        if ((u32)(work->field_2BC - 1) < 0x29) {
            work->field_2BE = 0x14;
        }
        if ((u32)(work->field_2BC - 0x33) < 0x29) {
            work->field_2BE = -0x14;
        }
        if ((s16)work->field_2BC >= 0x63) {
            work->field_2BC = 0;
        }
        Actor04600_Fn0272C(arg0);
    }
    *(u32*)0x1F8003FC += 8;
}

/// Live handler of the first enemy, dispatched on its stage `field_2C8`.
/// Stage 1 counts `field_2D0` down to an idle sound, re-rolled to 0x50..0xB3
/// frames with `field_2D6` picking the sound set, then walks: step length 0x14,
/// animation 2, a turn toward the player and a step of the root, with the
/// animation's frame count restarting at 0x1D. Stage 2 counts `field_2D4` up
/// and grows the scale factor by 0xC8 a frame; on the fifth frame the enemy is
/// killed through `Actor04600_Fn01604`, with a five-frame countdown, the death
/// phase reset, the task put into the stage's state and the HP cleared.
void Actor04600_Fn007B0(Task* arg0)
{
    Actor104600Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    u16              countdown;
    s16              mode;
    s32              soundId;
    u32              rng;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor104600Work*)arg0->work;
    enemy = arg0->spawnArg2;
    mode  = work->field_2C8;
    switch (mode) {
        case 1:
            countdown       = work->field_2D0 - 1;
            work->field_2D0 = countdown;
            if ((countdown << 16) <= 0) {
                rng             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rng;
                work->field_2D0 = (u16)((rng >> 16) % 100 + 0x50);
                if (work->field_2D6 != 0) {
                    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x40460009;
                    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402E0001;
                    SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
            }
            work->field_2BE = 0x14;
            work->field_2B8 = 2;
            Actor04600_Fn00FD8(arg0);
            Actor04600_Fn0272C(arg0);
            if ((s16)work->field_2BC >= 0x1D) {
                work->field_2BC = 0;
            }
            break;
        case 2:
            work->field_2D4 = work->field_2D4 + 1;
            work->field_2AC = work->field_2AC + 0xC8;
            if ((s16)work->field_2D4 >= 5) {
                Actor04600_Fn01604(arg0, 0);
                arg0->killCountdown = 5;
                work->field_2B4     = 0;
                arg0->state         = mode;
                enemy->hp           = 0;
            }
            break;
    }
}

/// Contact handler of the first enemy, with 0x4C bytes of scratch. The
/// `func_800E0C10` push-back from its contact table moves the root (response
/// 1) or restores the position the last step started from (response 2), and
/// the hit cooldown ticks down. Coming within 0x320 of the player moves a live
/// enemy to its dying stage. Each of the four contacts is then handled by
/// class: 0x10000 does the same, 0x20000 (outside the cooldown) either kills
/// the enemy outright on a critical roll or applies the damage, the id's side
/// effect, the hit effect and the id's cooldown, and 0x30000 pushes the root
/// out of the wall along the contact normal while the enemy walks. The table
/// is released, and a flagged hit on the third body's record clears that
/// body's 0x8000 bit.
void Actor04600_Fn00978(Task* arg0)
{
    s32                        damageState;
    TmdObject*                 object;
    GpEnemy*                   enemy;
    GpRec18*                   effectRec;
    VECTOR*                    normal;
    VECTOR*                    delta;
    s16                        cooldown;
    s32                        stage;
    s32                        contactStage;
    s32                        effect;
    s32                        pushY;
    s32                        movement;
    s32                        dx;
    s32                        dz;
    s32                        wallDx;
    s32                        wallDz;
    s32                        hitCooldown;
    s32                        boundedDepth;
    s32                        distance;
    s32                        z;
    u32                        id;
    u32                        damage;
    Actor104600Work*           work;
    GsCOORDINATE2*             coord;
    void*                      scratchHead;
    Actor104600ContactScratch* scratch;
    Actor104600Work*           contact;

    work        = (Actor104600Work*)arg0->work;
    scratchHead = (void*)(*(u32*)0x1F8003FC -= 0x4C);
    enemy       = arg0->spawnArg2;
    object      = arg0->extra;
    SOFT_TOUCH_REG_USE(object, scratchHead);
    coord    = object->coords;
    scratch  = scratchHead;
    movement = func_800E0C10(work->rec154, &scratch->delta, 4, &scratch->result);
    switch (movement) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] = (s32)(coord->coord.t[0] + scratch->delta.vx.h.hi);
            coord->coord.t[1] = (s32)(coord->coord.t[1] + scratch->delta.vy.h.hi);
            z                 = coord->coord.t[2] + scratch->delta.vz.h.hi;
            coord->coord.t[2] = z;
            break;
        case 2:
            coord->coord.t[0] = (s32)work->field_274.vx;
            coord->coord.t[1] = (s32)work->field_274.vy;
            z                 = work->field_274.vz;
            coord->coord.t[2] = z;
            break;
    }
    if (work->field_2CE != 0) {
        cooldown        = (u16)work->field_2CE - 1;
        work->field_2CE = cooldown;
        if ((cooldown << 0x10) <= 0) {
            work->field_2CE = 0;
        }
    }
    dx                  = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    scratch->delta.vx.w = dx;
    scratch->delta.vy.w = (s32)(Player_Status.coordMtx->t[1] - coord->coord.t[1]);
    dz                  = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    scratch->delta.vz.w = dz;
    distance            = SquareRoot0((dx * dx) + (dz * dz));
    if (distance < 0x320) {
        stage = work->field_2C8;
        delta = (VECTOR*)&scratch->delta;
        if (stage == 1) {
            work->field_2D2 = stage;
            work->field_2C8 = 2;
            goto delta_ready;
        }
    } else {
    delta_ready:
        delta = (VECTOR*)&scratch->delta;
    }
    damageState = 2;
    contact     = work;
contact_loop:
    do {
        id = contact->rec154[0].key;
        switch (id & 0xFFFF0000) {
            case 0x10000:
                contactStage = work->field_2C8;
                if (contactStage == 1) {
                    work->field_2D2 = contactStage;
                    work->field_2C8 = 2;
                }
                break;
            case 0x20000:
                if (work->field_2CE == 0) {
                    damage = Gp_ComputeDamage(id, (u32)distance, 0, 0);
                    if (Gp_RollEnemyChance(arg0->spawnArg2, contact->rec154[0].key, 0) != 0) {
                        Actor04600_Fn01604(arg0, 1U);
                        arg0->killCountdown = 5;
                        arg0->state         = damageState;
                        work->field_2B4     = 0;
                        enemy->hp           = -1;
                    } else {
                        func_800E2C78((GpObj40*)enemy, (s32)contact->rec154[0].key, (s32)damage, 0);
                        Actor04600_Fn00EC8(arg0, (s32)damage);
                        effect = Gp_GetIdParam0((s32)contact->rec154[0].key) & 0xFFFF;
                        if (effect == damageState)
                            goto effect_flag2;
                        if (effect < 3) {
                            if (effect == 1)
                                goto effect_react;
                        } else {
                            if (effect == 3)
                                goto effect_flag4;
                            if (effect == 9)
                                goto effect_flag2;
                        }
                        goto effect_done;
                    effect_react:
                        work->field_2D2 = effect;
                        work->field_2C8 = damageState;
                        goto effect_done;
                    effect_flag4:
                        Gp_SetObjFlag4((GpObj5C*)enemy, contact->rec154[0].key, 0);
                        goto effect_done;
                    effect_flag2:
                        Gp_SetObjFlag2((GpObj5D*)enemy, contact->rec154[0].key, 0);
                    effect_done:
                        if (enemy->hp > 0) {
                            func_800FDB18(Gp_GetIdParam1((s32)contact->rec154[0].key) & 0xFFFF, ((TmdObject*)arg0->extra)->coords + 1, NULL, &work->field_284);
                        }
                        hitCooldown = Gp_GetIdParam2((s32)contact->rec154[0].key);
                        if ((hitCooldown << 0x10) > 0) {
                            work->field_2CE = (s16)hitCooldown;
                        }
                    }
                }
                break;
            case 0x30000:
                wallDx              = coord->workm.t[0] - contact->rec154[0].point.vx;
                scratch->delta.vy.w = 0;
                scratch->delta.vx.w = wallDx;
                wallDz              = coord->workm.t[2] - contact->rec154[0].point.vz;
                scratch->delta.vz.w = wallDz;
                distance            = contact->rec154[0].depth - SquareRoot0((wallDx * wallDx) + (wallDz * wallDz));
                boundedDepth        = distance;
                if (distance <= 0) {
                    boundedDepth = 0;
                }
                SOFT_TOUCH_REG_USE(boundedDepth, distance);
                distance            = boundedDepth;
                scratch->delta.vx.w = (s32)(coord->workm.t[0] - contact->rec154[0].point.vx);
                normal              = &scratch->normal;
                scratch->delta.vy.w = (s32)(coord->workm.t[1] - contact->rec154[0].point.vy);
                scratch->delta.vz.w = (s32)(coord->workm.t[2] - contact->rec154[0].point.vz);
                VectorNormal(delta, normal);
                ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, normal, delta);
                if ((u32)((u16)work->field_2B8 - 1) < 2U) {
                    coord->coord.t[0] = (s32)(coord->coord.t[0] + ((s32)(distance * scratch->delta.vx.w) >> 0xC));
                    pushY             = distance * scratch->delta.vy.w;
                    if (pushY < 0) {
                        coord->coord.t[1] = (s32)(coord->coord.t[1] + (pushY >> 0xC));
                    }
                    coord->coord.t[2] = (s32)(coord->coord.t[2] + ((s32)(distance * scratch->delta.vz.w) >> 0xC));
                }
                break;
        }
        contact = (Actor104600Work*)((u8*)contact + 0x18);
        if ((s32)contact < (s32)((u8*)work + 0x60))
            goto contact_loop;
    } while (0);
    Gp_ClearRec18Occupied(work->rec154);
    effectRec = &work->rec1D4;
    if ((work->field_2C8 != 0) && (Gp_FindRec18(effectRec, 0) != 0)) {
        work->obj1B4.flags = (u16)((u16)work->obj1B4.flags & 0x7FFF);
        Gp_ClearRec18Occupied(effectRec);
    }
    *(u32*)0x1F8003FC += 0x4C;
}

/// Damage reaction of the first enemy: `arg1` comes off its HP and goes
/// through `func_800DA6E8`. A depleted enemy is killed through
/// `Actor04600_Fn01604` and put into its death state with a five-frame
/// countdown. A live one plays the hurt sound from the set `field_2D6` picks,
/// re-arms `field_2CC`, and while animation 1 plays latches `field_2D8`.
void Actor04600_Fn00EC8(Task* arg0, s32 arg1)
{
    Actor104600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              anim;
    s32              soundId;

    enemy      = arg0->spawnArg2;
    obj        = arg0->extra;
    coord      = obj->coords;
    work       = (Actor104600Work*)arg0->work;
    enemy->hp -= arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->hp < 0) {
        Actor04600_Fn01604(arg0, 0);
        arg0->state         = 2;
        arg0->killCountdown = 5;
        work->field_2B4     = 0;
        return;
    }
    if (work->field_2D6 != 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4046000A;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    } else {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402E0002;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    }
    anim            = work->field_2B8;
    work->field_2CC = 0xF;
    if (anim == 1) {
        work->field_2D8 = anim;
    }
}

/// Turns the first enemy toward the player by at most 0x20 a frame: the heading
/// `field_2B0` takes the XZ direction to the player outright when within 0x20,
/// and otherwise steps 0x20 the short way round the 0x1000 circle. The root's
/// rotation is then rebuilt from that heading alone, in 0x18 bytes of the
/// scratch stack.
void Actor04600_Fn00FD8(Task* arg0)
{
    Actor104600Work*       work;
    GsCOORDINATE2*         coord;
    Actor104600RotScratch* sc;
    s16                    cur;
    s32                    want;
    s16                    diff;
    s32                    adiff;
    s16                    turn;
    s16                    wrap;
    s32                    current;

    coord      = ((TmdObject*)arg0->extra)->coords;
    work       = (Actor104600Work*)arg0->work;
    sc         = (Actor104600RotScratch*)(*(u32*)0x1F8003FC -= 0x18);
    sc->vec.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    sc->vec.vy = 0;
    sc->vec.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    want       = ratan2((s16)sc->vec.vx, (s16)sc->vec.vz) & 0xFFF;
    cur        = work->field_2B0 & 0xFFF;
    diff       = want - cur;
    adiff      = diff >= 0 ? diff : -diff;
    turn       = diff;
    if (adiff < 0x21) {
        work->field_2B0 = want;
    } else {
        if (adiff >= 0x801) {
            wrap = diff - 0x1000;
            if (diff <= 0) {
                wrap = 0x1000 - diff;
            }
            turn = wrap;
        }
        current = work->field_2B0;
        if (turn <= 0) {
            cur = current - 0x20;
        } else {
            cur = current + 0x20;
        }
        work->field_2B0 = cur;
    }
    sc->rot.vx = 0;
    sc->rot.vy = work->field_2B0;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    *(u32*)0x1F8003FC += 0x18;
}

/// Death-state handler of the first enemy, under the `D_801153F4` mode byte:
/// mode 2 hides the model and mode 1 does nothing. Otherwise `field_2B4` steps
/// the death through three phases. Phase 0 shrinks the model and counts the
/// kill countdown down; when it runs out the death sound plays, state 0xF0 is
/// released, an optional final effect is spawned, the root transform is saved
/// and the enemy's node and four bodies are unlinked. Phase 1 folds the saved
/// transform back with a decaying Y scale for up to 0x3D frames, and phase 2
/// destroys the enemy once that count is spent. Outside reaction states 5 and 6
/// the first two phases also tick the animation, scale and recompute the
/// second part and re-colour the enemy.
void Actor04600_Fn01110(GpEnemy* enemy, Task* task)
{
    TmdObject*       model;
    Actor104600Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              soundId;

    obj   = task->extra;
    work  = (Actor104600Work*)task->work;
    coord = obj->coords;
    model = obj;
    switch (D_801153F4) {
        case 1:
            break;
        case 2:
            model->flags     |= 0x80;
            enemy->node.flags = 1;
            break;
        case 0:
        default:
            switch (work->field_2B4) {
                case 0:
                    work->obj1EC.flags &= 0x7FFF;
                    work->field_2AC    -= 0x12C;
                    task->killCountdown--;
                    if ((u32)((u16)work->field_2B2 - 5) >= 2 && task->killCountdown == 3) {
                        model->flags = 0x80;
                    }
                    if (work->field_2B2 == 6) {
                        work->field_2B8 = 1;
                        Actor04600_TickAnim(task);
                    }
                    if (task->killCountdown <= 0) {
                        if (work->field_2D6 != 0) {
                            soundId = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x4046000D;
                            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                        } else {
                            soundId = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x402E0005;
                            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                        }
                        task->killCountdown = 0;
                        Gp_ReleaseStateF0Add(task, 0x2E);
                        if (work->field_2DA != 0) {
                            Gp_SpawnEff(0x6009E, ((TmdObject*)task->extra)->coords, 0, NULL);
                        }
                        work->field_2B4 = 1;
                        work->field_2B6 = 0;
                        work->field_2CA = 0x1000;
                        work->field_28C = coord->coord;
                        enemy->recs     = NULL;
                        Gp_UnlinkNode(&enemy->node);
                        Gp_UnlinkObj(&work->objFC);
                        Gp_UnlinkObj(&work->obj134);
                        Gp_UnlinkObj(&work->obj1B4);
                        Gp_UnlinkObj(&work->obj1EC);
                    }
                    break;
                case 1:
                    if ((u32)((u16)work->field_2B2 - 5) >= 2) {
                        work->field_2B4 = 2;
                    }
                    work->field_2B6++;
                    if (work->field_2B6 >= 0x3D) {
                        work->field_2B4 = 2;
                    }
                    Actor04600_Fn02B14(task);
                    if (work->field_2B6 == 0xA) {
                        ((TmdObject*)task->extra)->flags = 2;
                    }
                    break;
                case 2:
                    work->field_2B6++;
                    if (work->field_2B6 >= 0x3D) {
                        Gp_DestroyEnemy(enemy, task);
                    }
                    return;
            }
            if ((u32)((u16)work->field_2B2 - 5) >= 2) {
                Actor04600_TickAnim(task);
                Actor04600_Fn0294C(task, &((TmdObject*)task->extra)->coords[1]);
                ((TmdObject*)task->extra)->coords[0].flg = 0;
                ((TmdObject*)task->extra)->coords[1].flg = 0;
                Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
                Actor04600_UpdateColor(enemy, &((TmdObject*)task->extra)->coords[1]);
            }
            break;
    }
}

/// Kills the first enemy: its HP is cleared and a random draw (or a non-zero
/// `arg1`) picks the death. The violent one plays its sound, sets the 0x8000
/// bit of the last two bodies, spawns the 0x6009C and 0x60030 effects and the
/// death script, and asks for a final effect through `field_2DA`; the other
/// plays a second sound and moves the reaction state to 6. `field_2D6` picks
/// the sound set either way.
void Actor04600_Fn01604(Task* arg0, u8 arg1)
{
    Actor104600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              soundId;

    obj         = arg0->extra;
    enemy       = arg0->spawnArg2;
    work        = (Actor104600Work*)arg0->work;
    coord       = obj->coords;
    enemy->hp   = 0;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if (((Gp_LcgState >> 0x10) & 2) || (arg1 & 0xFF)) {
        if (work->field_2D6 != 0) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4046000B;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402E0003;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        work->obj1B4.flags |= 0x8000;
        work->obj1EC.flags |= 0x8000;
        Gp_SpawnEff(0x6009C, ((TmdObject*)arg0->extra)->coords, 1, NULL);
        Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x300, &Actor04600_D058A0);
        Gp_SpawnScript18((s32)&Actor04600_D04170, (s32)&Actor04600_D0417C);
        work->field_2DA = 1;
    } else {
        if (work->field_2D6 != 0) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x4046000C;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        } else {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402E0004;
            SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
        }
        work->field_2B2 = 6;
    }
}

/// Spawn handler of the dropping first enemy, entry 0 of `Actor04600_D00010`.
/// A spawn arg whose high halfword is 1 destroys the enemy instead. Otherwise
/// it builds the same work block as `Actor04600_Fn00048` with the model hidden
/// and the node flag set, keeps the spawn arg's two halves, leaves the first
/// body's 0x8000 bit and the second's 0xC200 bits clear, parks
/// `Actor04600_D05868` as the task's message table and moves the task to state
/// 3, the drop.
void Actor04600_Fn017CC(GpEnemy* arg0, Task* arg1)
{
    Actor104600Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   part;
    TmdObject*       obj;
    s32              one;
    s32              i;

    obj   = (TmdObject*)arg1->extra;
    coord = obj->coords;
    part  = &coord[1];
    one   = 1;
    if ((s16)(arg1->spawnArg1 >> 16) == one) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    work = memCalloc(0x2E4U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work      = (TaskIdMap*)work;
    work->field_2DC = (s16)(arg1->spawnArg1 >> 16);
    work->field_2D6 = (u16)arg1->spawnArg1;
    obj->flags      = 0x80;
    coord->flg      = 0;
    obj->lightMtx   = &work->field_DC;
    obj->colorMtx   = &work->field_BC;
    arg0->field_4   = &coord[1].coord;
    arg0->field_48  = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord      = part;
    arg0->node.flags = one;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &Actor04600_D04160;
    arg0->recs       = &work->rec154[0];
    arg0->hp         = Actor04600_D04160.hpMax;
    func_800B3F84((GpAnimCtx*)work, Actor04600_D05890, obj, work->field_8C, (GpAnimSlot*)work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
        i += 1;
    } while (i < 3);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_2B8            = 1;
    work->field_2BA            = 1;
    work->field_2AC            = 0x1000;
    work->field_2DA            = 0;
    work->field_2CE            = 0;
    work->field_2D4            = 0;
    work->field_2D2            = 0;
    work->field_2CC            = 0;
    arg1->killCountdown        = 0;
    work->field_284.coord      = &((TmdObject*)arg1->extra)->coords[1];
    work->field_284.spawnArgLo = 0x100;
    work->field_284.spawnArgHi = 1;
    work->objFC.coord          = coord;
    work->objFC.ctx.recs       = &work->rec11C;
    work->objFC.pos.vx         = 0;
    work->objFC.pos.vy         = 0;
    work->objFC.pos.vz         = 0;
    work->objFC.key            = 0;
    work->objFC.radius         = 0xBB8;
    work->objFC.flags          = 1U;
    Gp_LinkObj(3, &work->objFC);
    Gp_InitRec18Table(&work->rec11C, 1, 0);
    work->obj134.coord    = coord;
    work->obj134.ctx.recs = &work->rec154[0];
    work->obj134.pos.vx   = 0;
    work->obj134.pos.vy   = -0xC8;
    work->obj134.pos.vz   = 0;
    work->obj134.key      = 0x3002E;
    work->obj134.radius   = 0xC8;
    work->obj134.flags    = 1U;
    work->objFC.flags     = (u16)(work->objFC.flags & 0x7FFF);
    Gp_LinkObj(2, &work->obj134);
    Gp_InitRec18Table(&work->rec154[0], 4, 0);
    work->obj1B4.coord    = coord;
    work->obj1B4.ctx.recs = &work->rec1D4;
    work->obj1B4.pos.vx   = 0;
    work->obj1B4.pos.vy   = 0;
    work->obj1B4.pos.vz   = 0;
    work->obj134.flags    = (u16)(work->obj134.flags & 0x3DFF);
    work->obj1B4.key      = Gp_PackPair(&Actor04600_D0415C, 0);
    work->obj1B4.radius   = 0x3E8;
    work->obj1B4.flags    = 1U;
    Gp_LinkObj(3, &work->obj1B4);
    Gp_InitRec18Table(&work->rec1D4, 1, 0);
    work->obj1EC.coord    = coord;
    work->obj1EC.ctx.recs = &work->rec20C;
    work->obj1EC.pos.vx   = 0;
    work->obj1EC.pos.vy   = 0;
    work->obj1EC.pos.vz   = 0;
    work->obj1EC.key      = 0x22323;
    work->obj1EC.radius   = 0x3E8;
    work->obj1EC.flags    = 1U;
    work->obj1B4.flags    = (u16)(work->obj1B4.flags & 0x7FFF);
    Gp_LinkObj(8, &work->obj1EC);
    Gp_InitRec18Table(&work->rec20C, 1, 0);
    work->field_2E2    = 0;
    work->obj1EC.flags = (u16)(work->obj1EC.flags & 0x7FFF);
    arg1->msgTable     = Actor04600_D05868;
    arg1->state        = 3;
}

/// Per-frame handler of the first enemy while it drops into place. Mode 1 of
/// `D_801153F4` only re-colours it and mode 2 hides the model. Otherwise, once
/// `field_2E2` has armed the drop, the root steps along its facing and by the
/// fall speed `field_2DE`, the collision response is applied, the animation
/// ticks and the root is recomputed, with the step length decaying by 2 a
/// frame. Reaching the floor (Y at or above 0) plays the landing sound, pins
/// the root at 0 and moves the enemy to the live stage with animation 2 and
/// task state 1; until then the fall speed grows by 10 a frame, or by 20 once
/// the drop has hit something.
void Actor04600_Fn01AFC(GpEnemy* arg0, Task* arg1)
{
    Actor104600Work* work;
    GsCOORDINATE2*   coord;
    s32              soundId;

    work = (Actor104600Work*)arg1->work;
    switch (D_801153F4) {
        case 1:
            Actor04600_UpdateColor(arg0, &((TmdObject*)arg1->extra)->coords[1]);
            break;
        case 2:
            ((TmdObject*)arg1->extra)->flags = 0x80;
            arg0->node.flags                 = 1;
            break;
        case 0:
        default:
            if (work->field_2E2 == 0) {
                return;
            }
            Actor04600_Fn02CD4(arg1);
            Actor04600_Fn01E0C(arg1);
            Actor04600_TickAnim(arg1);
            Actor04600_UpdateColor(arg0, &((TmdObject*)arg1->extra)->coords[1]);
            ((TmdObject*)arg1->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
            work->field_2BE -= 2;
            if (work->field_2BE < 0) {
                work->field_2BE = 0;
            }
            coord = ((TmdObject*)arg1->extra)->coords;
            if (coord->coord.t[1] >= 0) {
                soundId = ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 12) << 8) | 0x402C0008;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                work->field_2B2                               = 1;
                work->field_2C8                               = 1;
                work->field_2BE                               = 0;
                work->field_2DE                               = 0;
                work->field_2B8                               = 2;
                work->field_2BA                               = 0;
                ((TmdObject*)arg1->extra)->coords->coord.t[1] = 0;
                arg1->state                                   = 1;
            } else if (work->field_2E0 == 0) {
                work->field_2DE += 10;
            } else {
                work->field_2DE += 20;
            }
            break;
    }
}

/// Collision response of the dropping first enemy: the contact table at
/// `rec154` is run through `func_800E0C10` with a 0x48-byte scratch. Response 1
/// adds the returned X and Z offsets to the root; only the first one also adds
/// Y, latches `field_2E0`, sets the fall speed to -0x64 and takes a quarter off
/// the step length. Response 2 puts the root back where the last step started.
/// The table is released either way.
void Actor04600_Fn01E0C(Task* arg0)
{
    ActorsShared80133cd0Scratch* scratch;
    Actor104600Work*             work;
    GsCOORDINATE2*               coord;
    s32                          movement;

    work     = (Actor104600Work*)arg0->work;
    scratch  = (ActorsShared80133cd0Scratch*)(SCRATCH_SP -= 0x48);
    coord    = ((TmdObject*)arg0->extra)->coords;
    movement = func_800E0C10(&work->rec154[0], &scratch->delta, 4, NULL);
    switch (movement) {
        case 0:
            break;
        case 1:
            if (work->field_2E0 == 0) {
                coord->coord.t[1] += scratch->delta.vy.h.hi;
                work->field_2DE    = -0x64;
                work->field_2BE    = (u16)work->field_2BE - work->field_2BE / 4;
                work->field_2E0    = movement;
            }
            coord->coord.t[0] += scratch->delta.vx.h.hi;
            coord->coord.t[2] += scratch->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_274.vx;
            coord->coord.t[1] = work->field_274.vy;
            coord->coord.t[2] = work->field_274.vz;
            break;
    }
    Gp_ClearRec18Occupied(&work->rec154[0]);
    SCRATCH_SP += 0x48;
}

/// Message handler of the first enemy. While the task is in state 1, modes 4
/// and 5 start the collapse: the 0x60080 effect is spawned, animation 1 is
/// bound and the reaction state moves to 4 (mode 5 also restarts the spawn
/// count). Mode 1 reveals a dormant or dropping enemy: on maps 0x27 and 0x28
/// the root is placed at the spawn point the command selects (playing the
/// appearance sound on 0x27), the heading is taken from it and folded into
/// -0x800..0x800, the model's buffers are allocated and shown, the bodies are
/// re-armed and the drop begins at the live stage. Mode 3 hides the model,
/// disarms the bodies, resets the root and returns the task to state 3.
s32 Actor04600_Fn01F54(Task* arg0, s32 arg1, Actor104600Msg* arg2)
{
    Actor104600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          rot;
    u16              word;
    s16              heading;
    s32              magnitude;
    s32              mode;
    s32              state;
    s32              sound;
    s32              pan;

    obj   = (TmdObject*)arg0->extra;
    enemy = arg0->spawnArg2;
    state = arg0->state;
    work  = (Actor104600Work*)arg0->work;
    coord = obj->coords;
    if (state == 1) {
        mode = arg2->field_2;
        if (mode == 4) {
            Gp_SpawnEff(0x60080, coord, 0x400, &Actor04600_D058A8);
            work->field_2B8 = 1;
            Actor04600_TickAnim(arg0);
            work->field_2BC = 0;
            work->field_2B2 = 4;
            return 0;
        }
        if (mode == 5) {
            Gp_SpawnEff(0x60080, coord, 0x400, &Actor04600_D058A8);
            work->field_2B8 = 1;
            Actor04600_TickAnim(arg0);
            work->field_2BC = 0;
            work->field_2D4 = 0;
            work->field_2B2 = 4;
            return 0;
        }
    }
    word = arg2->field_2 & 0xFF;
    if ((word & 0xFF) == 1) {
        if ((u32)(arg0->state - 1) >= 2U) {
            if (gGameSession->at4.loc.area == 0x27) {
                rot.vx            = 0;
                rot.vy            = D_8018B74C[arg2->field_2 >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_8018B74C[arg2->field_2 >> 8].x;
                coord->coord.t[1] = D_8018B74C[arg2->field_2 >> 8].y;
                coord->coord.t[2] = D_8018B74C[arg2->field_2 >> 8].z;
                sound             = (((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54270006);
                pan               = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            } else if (gGameSession->at4.loc.area == 0x28) {
                rot.vx            = 0;
                rot.vy            = D_801874C4[arg2->field_2 >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_801874C4[arg2->field_2 >> 8].x;
                coord->coord.t[1] = D_801874C4[arg2->field_2 >> 8].y;
                coord->coord.t[2] = D_801874C4[arg2->field_2 >> 8].z;
            }
            heading         = rot.vy;
            work->field_2B0 = heading;
            magnitude       = heading >= 0 ? heading : -heading;
            if (magnitude >= 0x801) {
                if (heading >= 0x801) {
                    work->field_2B0 = heading - 0x1000;
                } else if (heading < -0x800) {
                    work->field_2B0 = heading + 0x1000;
                }
            }
            Tmd_AllocBuffers((TmdObject*)arg0->extra);
            ((TmdObject*)arg0->extra)->flags &= 0xFF7F;
            ((TmdObject*)arg0->extra)->flags &= 0xFFFB;
            enemy->node.flags                 = 0;
            work->objFC.flags                |= 0x8000;
            work->obj134.flags               |= 0xC200;
            RotMatrix(&rot, &coord->coord);
            work->field_2BE                        = 0xC8;
            work->field_2E2                        = 1;
            work->field_2DE                        = 0x64;
            work->field_2E0                        = 0;
            work->field_2B2                        = 1;
            work->field_2C8                        = 1;
            ((TmdObject*)arg0->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        }
        return 0;
    }
    if ((word & 0xFF) == 3) {
        ((TmdObject*)arg0->extra)->flags |= 0x80;
        ((TmdObject*)arg0->extra)->flags |= 4;
        enemy->node.flags                 = 1;
        work->objFC.flags                &= 0x7FFF;
        work->obj134.flags               &= 0x3DFF;
        rot.vz                            = 0;
        rot.vy                            = 0;
        rot.vx                            = 0;
        RotMatrix(&rot, &coord->coord);
        coord->coord.t[2]                      = 0;
        coord->coord.t[1]                      = 0;
        coord->coord.t[0]                      = 0;
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        arg0->state     = 3;
        work->field_2E2 = 0;
        work->field_2B2 = 0;
        work->field_2C8 = 0;
    }
    return 0;
}

/// Task handler of the first enemy: runs the entry of `Actor04600_D00004` for
/// the task's state with the enemy and the task, from a copy of the table on
/// the stack.
void Actor04600_Fn024A4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor04600_D00004;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Per-frame handler of the first enemy under the `D_801153F4` mode byte: mode
/// 1 runs only the tail, mode 2 hides the model and sets the node flag and
/// returns, mode 0 clears both before falling into the update, and any other
/// mode updates directly. The update runs the reaction dispatch, the flag
/// reactions, the contact handler and the animation rebind, scales the second
/// part, clears the display flags of the first two parts and recomputes the
/// second one's world matrix; the tail colours the enemy from that part and
/// draws its ground shadow.
void Actor04600_Fn02500(GpEnemy* arg0, Task* arg1)
{
    s32 state;
    s32 one;

    state = D_801153F4;
    one   = 1;
    if (state == one) {
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
    ((TmdObject*)arg1->extra)->flags = 0;
    arg0->node.flags                 = 0;
    goto default_body;
case2:
    ((TmdObject*)arg1->extra)->flags = 0x80;
    arg0->node.flags                 = one;
    return;
default_body:
    Actor04600_Fn003D4(arg1);
    Actor04600_Fn02618(arg1);
    Actor04600_Fn00978(arg1);
    Actor04600_Fn027BC(arg1);
    Actor04600_Fn0294C(arg1, &((TmdObject*)arg1->extra)->coords[1]);
    ((TmdObject*)arg1->extra)->coords[0].flg = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
case1:
    Actor04600_Fn02870(arg0, arg1);
    Actor04600_Fn028E0(arg1);
}

/// Per-frame reaction dispatch of the first enemy, on its `reactionFlags`:
/// bit 0x1 counts the death frames and grows the scale factor, killing the
/// enemy on the fifth; bit 0x2 is consumed and moves the reaction state to 3
/// with the step and the rebind stopped; bits 0xC tick the flag-4 helper,
/// feed the damage it reports to `Actor04600_Fn00EC8` and are cleared once it
/// expires.
void Actor04600_Fn02618(Task* arg0)
{
    Actor104600Work* work;
    GpEnemy*         enemy;
    s32              tick;
    u8               flags;

    enemy = arg0->spawnArg2;
    flags = enemy->reactionFlags;
    work  = (Actor104600Work*)arg0->work;
    if (flags != 0) {
        if (flags & 1) {
            work->field_2D4 += 1;
            work->field_2AC += 0xC8;
            if ((s16)work->field_2D4 >= 5) {
                Actor04600_Fn01604(arg0, 0);
                arg0->killCountdown = 5;
                work->field_2B4     = 0;
                arg0->state         = 2;
                enemy->hp           = 0;
            }
        }
        if (enemy->reactionFlags & 2) {
            enemy->reactionFlags &= 0xFD;
            work->field_2B2       = 3;
            work->field_2B6       = 0;
            work->field_2BE       = 0;
            work->field_2D2       = 1;
        }
        if (enemy->reactionFlags & 0xC) {
            tick = Gp_TickObjFlag4((GpObj5C*)enemy);
            if (tick != 0) {
                Actor04600_Fn00EC8(arg0, tick);
            }
            if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
                enemy->reactionFlags &= 0xF3;
            }
        }
    }
}

/// Steps the first enemy's root one frame along its own facing: saves the
/// current translation in `field_274`, advances X and Z along the rotation's Z
/// column scaled by the step length `field_2BE`, and Y by a fixed 0x80.
void Actor04600_Fn0272C(Task* task)
{
    GsCOORDINATE2*   coord;
    Actor104600Work* work;

    coord              = &((TmdObject*)task->extra)->coords[0];
    work               = (Actor104600Work*)task->work;
    work->field_274.vx = coord->coord.t[0];
    work->field_274.vy = coord->coord.t[1];
    work->field_274.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_2BE) >> 12;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_2BE) >> 12;
    coord->coord.t[1] += 0x80;
}

/// Rebinds the first enemy's animation id `field_2B8` to its two helper slots
/// unless `field_2D2` suppresses the rebind. A changed id is remembered in
/// `field_2BA`, the frame count `field_2BC` restarts and both slots switch to
/// it; otherwise the count ticks and the slots advance by one frame.
void Actor04600_Fn027BC(Task* arg0)
{
    Actor104600Work* work;
    s32              i;

    work = (Actor104600Work*)arg0->work;
    if (work->field_2D2 == 0) {
        i = 1;
        if (work->field_2B8 != work->field_2BA) {
            work->field_2BA = work->field_2B8;
            work->field_2BC = 0;
            do {
                func_800B4114((GpAnimCtx*)work, i, work->field_2B8, 0, 0);
                i++;
            } while (i < 3);
            return;
        }
        TOUCH_REG(i);
        work->field_2BC = (u16)(work->field_2BC + i);
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 3);
    }
}

/// Colours the first enemy from the world position of its model's second
/// coordinate, staged in a `VECTOR` taken off the scratch stack.
void Actor04600_Fn02870(GpEnemy* arg0, Task* task)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)task->extra)->coords[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(arg0, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// Draws the first enemy's ground shadow under the model root, at the world
/// translation of the root part staged in a `VECTOR3` on the scratch stack.
void Actor04600_Fn028E0(Task* task)
{
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    coord   = ((TmdObject*)task->extra)->coords;
    vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
    vec->vx = coord->workm.t[0];
    vec->vy = coord->workm.t[1];
    vec->vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(vec, 0x1C0, 0);
    SCRATCH_SP += 0x18;
}

/// Scales the rotation of `arg1`'s matrix by the first enemy's scale factor
/// `field_2AC`, clamped first to 0x1000..0x13E8: each of the matrix's three
/// columns is copied into an `SVECTOR` on the scratch stack, multiplied by the
/// factor on the GTE and written back.
void Actor04600_Fn0294C(Task* arg0, GsCOORDINATE2* arg1)
{
    ActorScaleScratchHead* scratch;
    SVECTOR*               vec;
    MATRIX*                matrix;
    Actor104600Work*       work;

    scratch = (ActorScaleScratchHead*)G_SCRATCH_HEAD;
    vec     = scratch->head;
    work    = arg0->work;
    vec--;
    scratch->head = vec;
    if ((u32)work->field_2AC >= 0x13E8U) {
        work->field_2AC = 0x13E8;
    }
    if ((u32)work->field_2AC < 0x1001U) {
        work->field_2AC = 0x1000;
    }
    matrix = &arg1->coord;

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 0, 6, 12);
    gte_lddp(work->field_2AC);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 0, 6, 12);

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 2, 8, 14);
    gte_lddp(work->field_2AC);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 2, 8, 14);

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 4, 10, 16);
    gte_lddp(work->field_2AC);
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 4, 10, 16);

    scratch->head = (u8*)scratch->head + 8;
}

/// Rebuilds the first enemy's root coordinate from the transform saved in
/// `field_28C`, scaled along Y by `field_2CA`, which decays by 0x50 a frame
/// while it stays above 0x200. The scale matrix and its `VECTOR` live in 0x30
/// bytes of the scratch stack; the node's `flg` is cleared so the next
/// `Gp_UpdateCoord` recomputes it.
void Actor04600_Fn02B14(Task* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor104600Work*            work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->work;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = (*(TmdObject**)&arg0->extra)->coords;
    if (work->field_2CA >= 0x201) {
        work->field_2CA = (u16)work->field_2CA - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_2CA;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_28C;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg         = 0;
    *(u8**)0x1F8003FC += 0x30;
}

/// Exit callback of the first enemy: detaches the enemy's contact records,
/// unlinks its node and the work's four bodies, then runs the common enemy
/// task exit.
void Actor04600_Fn02C08(Task* task)
{
    Actor104600Work* work;
    GpEnemy*         enemy;

    enemy = task->spawnArg2;
    work  = (Actor104600Work*)task->work;

    enemy->recs = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->objFC);
    Gp_UnlinkObj(&work->obj134);
    Gp_UnlinkObj(&work->obj1B4);
    Gp_UnlinkObj(&work->obj1EC);
    Gp_EnemyTaskExit(task);
}

/// Task handler of the dropping first enemy: runs the entry of
/// `Actor04600_D00010` for the task's state with the enemy and the task, from
/// a copy of the table on the stack.
void Actor04600_Fn02C6C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = Actor04600_D00010;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Steps the dropping first enemy's root one frame: saves the current
/// translation in `field_274`, advances X and Z along the rotation's Z column
/// scaled by the step length `field_2BE`, and Y by the fall speed `field_2DE`.
void Actor04600_Fn02CD4(Task* task)
{
    GsCOORDINATE2*   coord;
    Actor104600Work* work;

    coord              = ((TmdObject*)task->extra)->coords;
    work               = (Actor104600Work*)task->work;
    work->field_274.vx = coord->coord.t[0];
    work->field_274.vy = coord->coord.t[1];
    work->field_274.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_2BE) >> 12;
    coord->coord.t[1] += work->field_2DE;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_2BE) >> 12;
}

/// Spawn handler of the second enemy, entry 0 of `Actor04600_D0003C`. It
/// allocates the 0x2B0-byte work block, points the model's light and colour
/// matrices into it, links the enemy's node, seeds the animation context and
/// resets slots 1 and 2, starts animation 1 with the light blend fully up and
/// rolls the first 0x64..0xA3 frame wait, then links the three bodies with
/// their contact tables. The placement's mode is kept in `field_2AC`; mode 1
/// matching the task's `spawnType` steps the model's texture page and CLUT
/// row and re-streams it twice. `Actor04600_Fn04100` becomes the exit
/// callback.
void Actor04600_Fn02D68(GpEnemy* arg0, Task* arg1)
{
    Actor104600Enemy2Work* work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         part;
    u32                    seed;
    GpRec18*               records1;
    GpRec18*               records2;
    GpRec18*               records3;
    s32                    i;

    obj   = arg1->extra;
    coord = obj->coords;
    part  = &coord[1];
    work  = memCalloc(0x2B0U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work     = (TaskIdMap*)work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->field_DC;
    obj->colorMtx  = &work->field_BC;
    arg0->field_4  = &coord[1].coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord      = part;
    arg0->node.flags = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->param      = &Actor04600_D058B4;
    arg0->recs       = work->field_1A4;
    arg0->hp         = Actor04600_D058B4.hpMax;
    func_800B3F84(&work->context, Actor04600_D064A8, obj, work->field_8C, work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot(&work->context, i, 1);
        i += 1;
    } while (i < 3);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_28C  = 1;
    work->field_28E  = 1;
    work->field_2A6  = 1;
    work->field_2A4  = 0x12;
    arg0->node.flags = 1;
    obj->flags       = 0x80;
    seed             = Gp_LcgState * 5 + 0x71357911;
    work->field_2A8  = ((seed >> 16) & 0x3F) + 0x64;
    Gp_LcgState      = seed;
    Gp_SetLightMode((GpObj4C*)arg1->spawnArg2, 2);
    work->field_11C.end0.vz    = 0x1388;
    work->field_11C.end0Radius = 0xFA0;
    work->field_11C.end1Radius = 0x7D0;
    records1                   = work->field_134;
    work->field_11C.recs       = records1;
    work->field_FC.ctx.d4rec   = &work->field_11C;
    work->field_FC.coord       = coord;
    work->field_FC.pos.vx      = 0;
    work->field_FC.pos.vy      = 0;
    work->field_FC.pos.vz      = 0;
    work->field_FC.key         = 0;
    work->field_FC.radius      = 0;
    work->field_FC.flags       = 3;
    Gp_LinkObj(3, &work->field_FC);
    Gp_InitRec18Table(records1, 1, 0);
    work->field_14C.coord    = coord;
    records2                 = work->field_16C;
    work->field_14C.ctx.recs = records2;
    work->field_14C.pos.vx   = 0;
    work->field_14C.pos.vy   = 0;
    work->field_14C.pos.vz   = 0;
    work->field_14C.key      = 0;
    work->field_14C.radius   = 0x7D0;
    work->field_14C.flags    = 1;
    work->field_FC.flags     = work->field_FC.flags | 0x8000;
    Gp_LinkObj(3, &work->field_14C);
    Gp_InitRec18Table(records2, 1, 0);
    records3                 = work->field_1A4;
    work->field_184.coord    = coord;
    work->field_184.ctx.recs = records3;
    work->field_184.pos.vx   = 0;
    work->field_184.pos.vy   = -0xC8;
    work->field_184.pos.vz   = 0;
    work->field_184.key      = 0x3002F;
    work->field_184.radius   = 0xC8;
    work->field_184.flags    = 1;
    work->field_14C.flags    = work->field_14C.flags | 0x8000;
    Gp_LinkObj(2, &work->field_184);
    Gp_InitRec18Table(records3, 4, 0);
    work->field_184.flags = work->field_184.flags | 0xC200;
    work->field_2AC       = arg0->place->mode;
    if (work->field_2AC == 1 && arg1->spawnType == work->field_2AC) {
        obj->tpage++;
        obj->clut++;
        if (obj->buffer != NULL) {
            tmdProcessStream(obj);
            tmdProcessStream(obj);
        }
    }
    arg1->exitCallback = Actor04600_Fn04100;
    arg1->state++;
}

/// Idle tick of the second enemy. A 0x10000-class hit on either of its two
/// single-record tables sets `D_801153F2[1]`, latches `field_2AA` and selects
/// animation 2; if the light blend is fully up, one sound plays, the blend is
/// turned to fall and a new 0x12..0x31 frame wait is rolled. A latched hit
/// plays a second sound, clears the 0x8000 bit of the first two bodies and arms
/// state 0xF0. Under animation 1 the frame count reaching `field_2A8` turns the
/// blend down (with the first sound) when it is fully up, or back up after a
/// new 0x64..0xA3 frame wait once it has bottomed out; under animation 2 the
/// second sound repeats every 0x28 frames. `field_2AC` picks between two sets
/// of sound ids.
void Actor04600_Fn030A8(Task* arg0)
{
    Actor104600Enemy2Work* work;
    GsCOORDINATE2*         obj;
    s32                    snd;
    s16                    mode;
    s32                    id;
    GpEnemy*               ctx;

    work                   = (Actor104600Enemy2Work*)arg0->work;
    *(u8**)G_SCRATCH_HEAD -= 8;
    obj                    = ((TmdObject*)arg0->extra)->coords;
    if (Gp_CountRec18Hi(work->field_16C, 0x10000) != 0 || Gp_CountRec18Hi(work->field_134, 0x10000) != 0) {
        D_801153F2[1]   = 1;
        work->field_2AA = 1;
        work->field_28C = 2;
        if (work->field_2A6 != 0 && work->field_2A4 == 0x12) {
            if (work->field_2AC != 0) {
                ctx = arg0->spawnArg2;
                id  = 0x40480007;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            } else {
                ctx = arg0->spawnArg2;
                id  = 0x402E0006;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            }
            work->field_290 = 0;
            work->field_2A6 = 0;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_2A8 = ((Gp_LcgState >> 16) & 0x1F) + 0x12;
        }
    }
    if (work->field_2AA != 0) {
        if (work->field_2AC != 0) {
            ctx = arg0->spawnArg2;
            id  = 0x40480008;
            snd = ((ctx->placeKey >> 12) << 8) | id;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
        } else {
            ctx = arg0->spawnArg2;
            id  = 0x402E0007;
            snd = ((ctx->placeKey >> 12) << 8) | id;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
        }
        work->field_14C.flags &= 0x7FFF;
        work->field_FC.flags  &= 0x7FFF;
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(work->field_16C);
    mode = work->field_28C;
    if (mode == 1) {
        work->field_29A = 1;
        work->field_292 = 0;
        if (work->field_290 > work->field_2A8) {
            if (work->field_2A6 != 0 && work->field_2A4 == 0x12) {
                if (work->field_2AC != 0) {
                    ctx = arg0->spawnArg2;
                    id  = 0x40480007;
                    snd = ((ctx->placeKey >> 12) << 8) | id;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
                } else {
                    ctx = arg0->spawnArg2;
                    id  = 0x402E0006;
                    snd = ((ctx->placeKey >> 12) << 8) | id;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
                }
                work->field_290 = 0;
                work->field_2A6 = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_2A8 = ((Gp_LcgState >> 16) & 0x1F) + 0x12;
            } else if (*(s32*)&work->field_2A4 == 0) {
                work->field_290 = 0;
                work->field_2A6 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_2A8 = ((Gp_LcgState >> 16) & 0x3F) + 0x64;
            }
        }
    } else if (mode == 2) {
        work->field_2AA = 0;
        if (work->field_290 >= 0x28) {
            if (work->field_2AC != 0) {
                ctx = arg0->spawnArg2;
                id  = 0x40480008;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            } else {
                ctx = arg0->spawnArg2;
                id  = 0x402E0007;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            }
            work->field_290 = 0;
        }
    }
    *(u8**)G_SCRATCH_HEAD += 8;
}

/// Per-frame hit handler. Applies the `func_800E0C10` push-back from the four
/// `field_1A4` records to the root coordinate (restoring `field_254` when two
/// records conflict), then walks the records: a kind-1 hit or a kind-2 hit
/// whose distance-scaled damage is nonzero plays the hit sound and sparks and
/// puts the task into its death state after 5 frames; a zero-damage kind-2 hit
/// applies the id's side effect instead.
void Actor04600_Fn0346C(Task* arg0)
{
    Actor104600Enemy2Work* work;
    Actor104600HitScratch* sc;
    Actor104600HitScratch* head;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GpEnemy*               enemy;
    s32                    i;
    s32                    sndHit;
    s32                    sndHit2;
    u32                    damage;
    s32                    snd;

    work                                     = (Actor104600Enemy2Work*)arg0->work;
    head                                     = *(Actor104600HitScratch**)G_SCRATCH_HEAD;
    *(Actor104600HitScratch**)G_SCRATCH_HEAD = head - 1;
    sc                                       = head - 1;
    obj                                      = arg0->extra;
    coord                                    = obj->coords;
    enemy                                    = arg0->spawnArg2;

    switch (func_800E0C10(work->field_1A4, &head[-1].delta, 4, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += sc->delta.vx.h.hi;
            coord->coord.t[1] += sc->delta.vy.h.hi;
            coord->coord.t[2] += sc->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_254;
            coord->coord.t[1] = work->field_258;
            coord->coord.t[2] = work->field_25C;
            break;
    }
    i       = 0;
    sndHit  = 0x40480009;
    sndHit2 = 0x402E0008;
    do {
        switch (work->field_1A4[i].key & 0xFFFF0000) {
            case 0x10000:
                if (work->field_2AC != 0) {
                    snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | sndHit;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | sndHit2;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &Actor04600_D064B4);
                Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &Actor04600_D064B4);
                Gp_SpawnEff(0x6009E, ((TmdObject*)arg0->extra)->coords, 0, &Actor04600_D064BC);
                Gp_SpawnPadLerp(0xA, 0x60, 0x60);
                obj->flags          = 0x80;
                work->field_2A0     = 0x500;
                work->field_28C     = 1;
                enemy->hp           = 0;
                work->field_2A6     = 1;
                arg0->killCountdown = 5;
                arg0->state         = 2;
                break;
            case 0x20000:
                sc->delta.vx.w = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                sc->delta.vy.w = Player_Status.coordMtx->t[1] - coord->coord.t[1];
                sc->delta.vz.w = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                damage         = Gp_ComputeDamage(work->field_1A4[i].key,
                                                  SquareRoot0(sc->delta.vx.w * sc->delta.vx.w +
                                                              sc->delta.vy.w * sc->delta.vy.w +
                                                              sc->delta.vz.w * sc->delta.vz.w),
                                                  0, 0);
                if (Gp_RollEnemyChance(arg0->spawnArg2, work->field_1A4[i].key, 0) != 0) {
                    damage *= 4;
                }
                func_800E2C78((GpObj40*)enemy, work->field_1A4[i].key, damage, 0);
                func_800DA6E8(&enemy->node, damage, 0);
                if (damage != 0) {
                    if (work->field_2AC != 0) {
                        snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | sndHit;
                        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    } else {
                        snd = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 12) << 8) | sndHit2;
                        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    }
                    Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &Actor04600_D064B4);
                    Gp_SpawnEff(0x60030, ((TmdObject*)arg0->extra)->coords, 0x200, &Actor04600_D064B4);
                    Gp_SpawnEff(0x6009E, ((TmdObject*)arg0->extra)->coords, 0, &Actor04600_D064BC);
                    obj->flags          = 0x80;
                    work->field_2A0     = 0x1000;
                    work->field_2A6     = 1;
                    work->field_28C     = 1;
                    enemy->hp           = 0;
                    arg0->killCountdown = 5;
                    arg0->state         = 2;
                    break;
                }
                switch ((u16)Gp_GetIdParam0(work->field_1A4[i].key)) {
                    case 2:
                    case 9:
                        Gp_SetObjFlag2((GpObj5D*)enemy, work->field_1A4[i].key, 0);
                        break;
                    case 8:
                        work->field_2A6 = 1;
                        break;
                }
                break;
        }
        i++;
    } while (i < 4);
    Gp_ClearRec18Occupied(work->field_1A4);
    *(Actor104600HitScratch**)G_SCRATCH_HEAD = *(Actor104600HitScratch**)G_SCRATCH_HEAD + 1;
}

/// Dying-state tick of the second enemy, under the `D_801153F4` mode byte: 1
/// does nothing and 2 hides the model. Otherwise the root's matrix is saved
/// into `field_264` and refolded with the decaying Y scale. Once `field_288` is
/// set the enemy is destroyed after 0x3D frames; before that, the kill
/// countdown running out releases state 0xF0, sets `field_288` and unlinks the
/// enemy's node and its three bodies, and the two animation slots are rebound
/// or advanced.
void Actor04600_Fn03958(GpEnemy* arg0, Task* arg1)
{
    Actor104600Enemy2Work* work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    s32                    i;
    Actor104600Enemy2Work* anim;

    work  = arg1->work;
    obj   = arg1->extra;
    coord = obj->coords;
    switch (D_801153F4) {
        case 0:
            break;
        case 1:
            return;
        case 2:
            obj->flags      |= 0x80;
            arg0->node.flags = 1;
            return;
    }
    if (work->field_288 != 0) {
        work->field_264 = coord->coord;
        Actor04600_Fn0400C(arg1);
        work->field_28A++;
        if (work->field_28A >= 0x3D) {
            Gp_DestroyEnemy(arg0, arg1);
        }
        return;
    }
    work->field_264 = coord->coord;
    Actor04600_Fn0400C(arg1);
    arg1->killCountdown--;
    if (arg1->killCountdown <= 0) {
        Gp_ReleaseStateF0Add((GpObj20E*)arg1, 0x2F);
        work->field_288 = 1;
        work->field_28A = 0;
        arg0->recs      = 0;
        Gp_UnlinkNode(&arg0->node);
        Gp_UnlinkObj(&work->field_14C);
        Gp_UnlinkObj(&work->field_FC);
        Gp_UnlinkObj(&work->field_184);
    }
    anim = arg1->work;
    i    = 1;
    if (anim->field_28C != (s16)anim->field_28E) {
        anim->field_28E = anim->field_28C;
        anim->field_290 = 0;
        do {
            func_800B4114((GpAnimCtx*)anim, i, anim->field_28C, 0, 8);
            i++;
        } while (i < 3);
        return;
    }
    TOUCH_REG(i);
    anim->field_290 = (u16)(anim->field_290 + i);
    do {
        Gp_AnimTickIndex((GpAnimCtx*)anim, i);
        i++;
    } while (i < 3);
}

/// Task states of the second enemy as `Actor04600_Fn03B80` dispatches them:
/// spawn, per-frame update and the dying tick.
const GpEnemyTaskFuncTable3 Actor04600_D0003C = {
    { Actor04600_Fn02D68, Actor04600_Fn03BDC, Actor04600_Fn03958 },
};

/// Task handler of the second enemy: runs the entry of `Actor04600_D0003C` for
/// the task's state with the enemy and the task, from a copy of the table on
/// the stack.
void Actor04600_Fn03B80(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor04600_D0003C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Per-frame handler of the second enemy under the `D_801153F4` mode byte:
/// mode 1 runs only the tail, mode 2 hides the model, sets the node flag and
/// returns, mode 0 clears the node flag before falling into the update, and
/// any other mode updates directly. The update raises the root's Y translation
/// by 0x80, runs the reaction dispatch, the light blend, the flag reactions,
/// the hit handler and the animation, clears the first two parts' flags and
/// recomputes the second one's matrix; the tail colours the enemy.
void Actor04600_Fn03BDC(GpEnemy* arg0, Task* arg1)
{
    s32 state;
    s32 one;

    state = D_801153F4;
    one   = 1;
    if (state == one) {
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
    arg0->node.flags = 0;
    goto default_body;
case2:
    ((TmdObject*)arg1->extra)->flags = 0x80;
    arg0->node.flags                 = one;
    return;
default_body:
    ((TmdObject*)arg1->extra)->coords[0].coord.t[1] += 0x80;
    Actor04600_Fn03D54(arg1);
    Actor04600_Fn03F30(arg1);
    Actor04600_Fn03CEC(arg1);
    Actor04600_Fn0346C(arg1);
    Actor04600_Fn03E10(arg1);
    ((TmdObject*)arg1->extra)->coords[0].flg = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
case1:
    Actor04600_Fn03EC0(arg0, arg1);
}

/// Consumes the pending bits of the second enemy's `reactionFlags`: bit 0x1 is
/// dropped on its own, bit 0x2 puts the work into reaction state 3 with its
/// frame count cleared, and bits 0xC are dropped last, after re-reading the
/// byte.
void Actor04600_Fn03CEC(Task* arg0)
{
    GpEnemy*               enemy;
    Actor104600Enemy2Work* work;
    u8                     flags;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = arg0->work;
    flags = enemy->reactionFlags;
    if (flags != 0) {
        if (flags & 1) {
            enemy->reactionFlags = flags & 0xFE;
        }
        if (enemy->reactionFlags & 2) {
            enemy->reactionFlags = enemy->reactionFlags & 0xFD;
            work->field_286      = 3;
            work->field_28A      = 0;
        }
        flags = enemy->reactionFlags;
        if (flags & 0xC) {
            enemy->reactionFlags = flags & 0xF3;
        }
    }
}

/// Per-frame dispatch on the second enemy's reaction state `field_286`: state
/// 0 runs the idle tick and state 2 does nothing. State 3 clears `field_292`
/// and turns the light blend down, resets the remembered animation id to 1 and
/// the counters every fourth frame, and returns to state 0 once
/// `Gp_TickObjFlag2` reports the reaction over.
void Actor04600_Fn03D54(Task* task)
{
    Actor104600Enemy2Work* work;

    work = task->work;
    switch (work->field_286) {
        case 0:
            Actor04600_Fn030A8(task);
            break;
        case 2:
            break;
        case 3:
            work->field_292 = 0;
            work->field_2A6 = 0;
            work->field_28A = work->field_28A + 1;
            if (work->field_28A >= 4) {
                work->field_28E = 1;
                work->field_290 = 0;
                work->field_28A = 0;
            }
            if (Gp_TickObjFlag2((GpObj5D*)task->spawnArg2) != 0) {
                work->field_286 = 0;
            }
            break;
    }
}

/// Drives the second enemy's animation slots 1 and 2 from its animation id
/// `field_28C`. When it differs from the remembered `field_28E` it is
/// remembered, the frame count restarts and both slots switch to it with a
/// blend of 8; otherwise the count ticks and both slots advance.
void Actor04600_Fn03E10(Task* arg0)
{
    Actor104600Enemy2Work* work;
    s32                    i;

    work = arg0->work;
    i    = 1;
    if (work->field_28C != (s16)work->field_28E) {
        work->field_28E = work->field_28C;
        work->field_290 = 0;
        do {
            func_800B4114((GpAnimCtx*)work, i, work->field_28C, 0, 8);
            i++;
        } while (i < 3);
        return;
    }
    TOUCH_REG(i);
    work->field_290 = (u16)(work->field_290 + i);
    do {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
        i++;
    } while (i < 3);
}

/// Colours the second enemy from the world position of its model's second
/// coordinate, staged in a `VECTOR` taken off the scratch stack.
void Actor04600_Fn03EC0(GpEnemy* arg0, Task* task)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)task->extra)->coords[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(arg0, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// Ramps the second enemy's light blend `field_2A4` up or down as `field_2A6`
/// says. Rising, the first frame switches the enemy to light mode 2 and the
/// blend saturates at 0x12, where the enemy's node flag and the model's 0x80
/// bit are set. Falling, leaving 0x12 clears the node flag and returns to light
/// mode 0, and the blend bottoms out at 0 with the model bits cleared.
void Actor04600_Fn03F30(Task* task)
{
    Actor104600Enemy2Work* work;
    GpEnemy*               enemy;
    TmdObject*             obj;

    work  = (Actor104600Enemy2Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    obj   = (TmdObject*)task->extra;

    if (work->field_2A6 != 0) {
        if (work->field_2A4 == 0) {
            work->field_2A4++;
            obj->flags = 2;
            Gp_SetLightMode(task->spawnArg2, 2);
        } else {
            work->field_2A4++;
            if (work->field_2A4 >= 0x12) {
                work->field_2A4   = 0x12;
                enemy->node.flags = 1;
                obj->flags        = 0x80;
            }
        }
    } else {
        if (work->field_2A4 == 0x12) {
            work->field_2A4--;
            enemy->node.flags = 0;
            obj->flags        = 2;
            Gp_SetLightMode(task->spawnArg2, 0);
        } else {
            work->field_2A4--;
            if (work->field_2A4 <= 0) {
                work->field_2A4 = 0;
                obj->flags      = 0;
            }
        }
    }
}

/// Rebuilds the second enemy's root coordinate from the matrix saved in
/// `field_264`, scaled along Y by `field_2A0`, which decays by 0x50 a frame
/// while above 0x200. The scaling matrix and its vector are staged in 0x30
/// bytes of the scratch stack; the node's `flg` is cleared so the next
/// `Gp_UpdateCoord` recomputes it.
void Actor04600_Fn0400C(Task* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor104600Enemy2Work*      work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->work;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = (*(TmdObject**)&arg0->extra)->coords;
    if (work->field_2A0 >= 0x201) {
        work->field_2A0 = (u16)work->field_2A0 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_2A0;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_264;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg         = 0;
    *(u8**)0x1F8003FC += 0x30;
}

/// Exit callback of the second enemy: detaches the enemy's contact records,
/// unlinks its node and the work's three bodies, then runs the common enemy
/// task exit.
void Actor04600_Fn04100(Task* task)
{
    Actor104600Enemy2Work* work;
    GpEnemy*               enemy;

    enemy = task->spawnArg2;
    work  = (Actor104600Enemy2Work*)task->work;

    enemy->recs = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_14C);
    Gp_UnlinkObj(&work->field_FC);
    Gp_UnlinkObj(&work->field_184);
    Gp_EnemyTaskExit(task);
}
