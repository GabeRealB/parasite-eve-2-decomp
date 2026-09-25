#include "common.h"

#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// 0x18-byte `G_SCRATCH_HEAD` block `func_actor_800300_80162064` takes for the
/// ground-quad heading it copies into `GameActor.field_88` / `_94` / `_A0`.
/// `func_800EA1A8` also fills the block as a `VECTOR3` from `coord->workm.t`.
typedef struct {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR vec;
} Actor800300VecScratch;
STATIC_ASSERT_SIZEOF(Actor800300VecScratch, 0x18);

/// View of `GameActor.field_973` as the unsigned byte its rotation
/// multiply sign-extends.
typedef struct {
    byte pad[0x973];
    u8   field_973;
} Actor800300DirByte;

extern s16        D_80072830;
extern s32        D_8017A99C;
extern Task*      D_80115764;
extern void       D_actor_800300_80168880;
extern GpImgRec** D_actor_800300_80168950[];
extern GpImgRec** D_actor_800300_80168960[];

s32  func_80105ED4(Task* arg0);
s32  func_8010BC70(GsCOORDINATE2* arg0);
s32  func_8010BCF4(Task* arg0, VECTOR3* arg1);
void func_8010BD88(Task* arg0, VECTOR3* arg1);
void func_8010BE5C(Task* arg0, VECTOR3* arg1);
void Gp_PlayerMode2State0(Task* arg0);
void Gp_PlayerMode2State1(Task* arg0);
void Gp_PlayerMode2State2(Task* arg0);
void Gp_PlayerMode2State6(Task* arg0);

void func_actor_800300_801623F8(Task* arg0);
void func_actor_800300_801625A8(Task* task);
void func_actor_800300_80162658(Task* arg0);
void func_actor_800300_801628D0(Task* arg0);
void func_actor_800300_80162A98(Task* arg0);
void func_actor_800300_80162C2C(Task* arg0);
void func_actor_800300_80162C98(Task* arg0);
void func_actor_800300_80162D74(Task* arg0);
void func_actor_800300_80162EEC(Task* arg0);
void func_actor_800300_80162F24(Task* arg0);
void func_actor_800300_80162F98(Task* arg0);
void func_actor_800300_80163048(Task* arg0);
void func_actor_800300_80163074(Task* arg0);

void func_actor_800300_80161E80(Task* arg0)
{
    GameActor*      actor;
    TmdObject*      extra;
    GsCOORDINATE2*  coord;
    GsCOORDINATE2*  next;
    GsCOORDINATE2** addr;
    GpActorD4*      d4;
    GpObj*          obj;
    GpRec18*        recs;
    McSaveData*     save;
    s32             packed;
    s8              fcc;

    actor = arg0->work;
    extra = arg0->extra.tmd;
    d4    = actor->field_910;
    addr  = &extra->coords;
    coord = *addr;
    arg0->state++;
    arg0->msgTable     = &D_actor_800300_80168880;
    arg0->exitCallback = &func_actor_800300_801625A8;
    actor->field_938   = 0x13;
    D_80115764         = arg0;
    coord->sub         = &gGfxViewCoord;
    coord->flg         = 0;
    extra->flags       = 0;
    RotMatrix((SVECTOR*)&actor->field_50, &coord->coord);
    func_8010BFCC(arg0);
    actor->field_985 = 0x10;
    Gp_AnimResetChildSlots(arg0, actor->field_93C);
    recs            = actor->field_17C;
    obj             = (GpObj*)actor->field_AC;
    actor->field_10 = coord->coord.t[0];
    actor->field_14 = coord->coord.t[1];
    actor->field_18 = coord->coord.t[2];
    obj->ctx.dir    = (GpObjDirRec*)actor->field_88;
    obj->coord      = coord;
    actor->field_90 = (s32)recs;
    save            = &Mc_SaveData;
    obj->pos.vx     = 0;
    obj->pos.vy     = -0x12C;
    obj->pos.vz     = 0;
    {
        s32 temp;

        temp        = save->characterId;
        obj->radius = 0x12C;
        obj->flags  = 4;
        packed      = 0x10000;
        obj->key    = temp | packed;
        Gp_LinkObj(0, obj);
    }
    Gp_InitRec18Table((GpRec18*)actor->field_90, 0x12, 0);
    obj->flags     |= 0xC200;
    obj             = (GpObj*)actor->field_CC;
    next            = arg0->extra.tmd->coords;
    obj->ctx.dir    = (GpObjDirRec*)actor->field_94;
    obj->coord      = next + 4;
    actor->field_9C = (s32)recs;
    obj->pos.vx     = 0;
    obj->pos.vy     = 0;
    obj->pos.vz     = 0;
    {
        s32 temp;

        temp        = save->characterId;
        obj->radius = 0xC8;
        obj->flags  = 4;
        obj->key    = temp | packed;
        Gp_LinkObj(0, obj);
    }
    obj->flags      |= 0xC000;
    actor->field_984 = 7;
    func_8010BF7C(arg0, 0x3C, 0x7F);
    fcc             = -0x6A;
    d4->repeatCount = fcc;
}

void func_actor_800300_80162064(Task* arg0)
{
    void**                 scratch;
    u8*                    head;
    Actor800300VecScratch* sc;
    GameActor*             actor;
    TmdObject*             obj;
    TmdObject*             extra;
    GsCOORDINATE2*         coord;
    GpObj*                 objs[2];
    s32                    dy;
    s32                    i;
    s8                     bits;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    obj                            = arg0->extra.tmd;
    SCRATCH_HEAD_AT(scratch, void) = head - 0x18;
    extra                          = obj;
    sc                             = (Actor800300VecScratch*)(head - 0x18);
    actor                          = arg0->work;
    coord                          = extra->coords;
    if (actor->field_954 != 2 &&
        (dy = coord->coord.t[1], dy = dy - actor->field_14, dy = ABS(dy), dy >= 0x200)) {
        coord->coord.t[0] = actor->field_10;
        coord->coord.t[1] = actor->field_14;
        coord->coord.t[2] = actor->field_18;
    } else {
        if (actor->field_984 & 1) {
            actor->field_992 = func_801011D0(coord, actor->field_90, 0x12, &actor->field_930);
            if ((s8)actor->field_992 == 2) {
                coord->coord.t[0] = actor->field_10;
                coord->coord.t[1] = actor->field_14;
                coord->coord.t[2] = actor->field_18;
            }
        } else {
            actor->field_992 = 0;
        }
        actor->field_10 = coord->coord.t[0];
        actor->field_14 = coord->coord.t[1];
        actor->field_18 = coord->coord.t[2];
    }
    objs[0] = (GpObj*)actor->field_AC;
    objs[1] = (GpObj*)actor->field_CC;
    for (i = 0; i < 2; i++) {
        bits = actor->field_983;
        if ((bits >> i) & 1) {
            actor->field_984 |= 1 << i;
            objs[i]->flags   |= 0x4000;
        } else if (bits & (8 << i)) {
            actor->field_984 &= ~(1 << i);
            objs[i]->flags   &= ~0x4000;
        }
    }
    actor->field_983 = 0;
    if (D_80115768 == 0) {
        func_actor_800300_80162C2C(arg0);
    }
    func_actor_800300_801623F8(arg0);
    Gp_ClearRec18Occupied(actor->field_17C);
    if (actor->field_984 & 1) {
        coord->coord.t[1] = actor->field_14 + 0x10;
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    if ((s8)actor->field_986 != 0) {
        sc->vec.vx = (u16)actor->field_30.vx;
        sc->vec.vy = (u16)actor->field_30.vy;
        sc->vec.vz = (u16)actor->field_30.vz;
    } else {
        sc->vec.vx = (u16)coord->workm.m[0][2] * (s8)((volatile Actor800300DirByte*)actor)->field_973;
        sc->vec.vy = (u16)coord->workm.m[1][2] * (s8)((volatile Actor800300DirByte*)actor)->field_973;
        sc->vec.vz = (u16)coord->workm.m[2][2] * (s8)((volatile Actor800300DirByte*)actor)->field_973;
    }
    ((SVECTOR*)actor->field_88)->vx = sc->vec.vx;
    ((SVECTOR*)actor->field_88)->vy = sc->vec.vy;
    ((SVECTOR*)actor->field_88)->vz = sc->vec.vz;
    ((SVECTOR*)actor->field_94)->vx = sc->vec.vx;
    ((SVECTOR*)actor->field_94)->vy = sc->vec.vy;
    ((SVECTOR*)actor->field_94)->vz = sc->vec.vz;
    ((SVECTOR*)actor->field_A0)->vx = sc->vec.vx;
    ((SVECTOR*)actor->field_A0)->vy = sc->vec.vy;
    ((SVECTOR*)actor->field_A0)->vz = sc->vec.vz;
    if (!(extra->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)coord->workm.t, (VECTOR3*)sc) != 0) {
            Gp_DrawEffGroundQuad((VECTOR3*)sc, 0x200, Gp_State1C->groundShade);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

void func_actor_800300_801623F8(Task* arg0)
{
    void**      scratch;
    u8*         head;
    s32         temp;
    RECT*       rect;
    GameActor*  actor;
    GpImgRec*** table;
    s32         idx;
    u32         row;
    GpImgRec*   img;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    actor                          = arg0->work;
    temp                           = (s32)(head - 8);
    SCRATCH_HEAD_AT(scratch, void) = (void*)temp;
    rect                           = (RECT*)temp;

    if ((s8)actor->field_987 != 0) {
        actor->field_988--;
        if ((s8)actor->field_988 <= 0) {
            table = D_actor_800300_80168950;
            idx   = (s8)actor->field_987 - 1;
            img   = table[idx][(s8)actor->field_989];
            if (img != NULL) {
                ((RECT*)head)[-1].x = 0;
                rect->y             = 0x40;
                rect->w             = 0x19;
                rect->h             = 0x14;
                Gp_LoadActorImage(arg0, img, rect);
                actor->field_988 = 4;
                actor->field_989++;
            } else {
                actor->field_987 = 0;
            }
        }
    }

    if ((s8)actor->field_98A != 0) {
        actor->field_98B--;
        if ((s8)actor->field_98B <= 0) {
            table = D_actor_800300_80168960;
            idx   = (row = (s8)actor->field_98A - 1);
            img   = table[row][(s8)actor->field_98C];
            if (img != NULL) {
                rect->x = 0xC;
                rect->y = 0x60;
                rect->w = 0xE;
                rect->h = 0x14;
                Gp_LoadActorImage(arg0, img, rect);
                actor->field_98B = 8;
                actor->field_98C++;
            } else {
                actor->field_98A = 0;
            }
        }
    }

    SCRATCH_POP_BYTES(8);
}

void func_actor_800300_8016259C(Task* arg0)
{
    arg0->state = 3;
}

/// Teardown of the actor's main task, run both as its exit callback and as
/// the last entry of its state table: clears the second `Gp_ActorSlots` slot,
/// unlinks the two collision objects the set-up state linked, and kills the
/// task.
void func_actor_800300_801625A8(Task* task)
{
    GameActor* actor;

    actor            = (GameActor*)task->work;
    Gp_ActorSlots[1] = NULL;
    Gp_UnlinkObj((GpObj*)actor->field_AC);
    Gp_UnlinkObj((GpObj*)actor->field_CC);
    taskKill(task);
}

/// State handlers of the actor's main task, indexed by its state: set-up, the
/// per-frame update, a step that only advances to the last state, and the
/// teardown.
const TaskFuncTable4 D_actor_800300_80161E24 = { {
    (TaskFunc)func_actor_800300_80161E80,
    (TaskFunc)func_actor_800300_80162064,
    func_actor_800300_8016259C,
    func_actor_800300_801625A8,
} };

/// Per-frame entry point of the actor's main task: runs the handler its state
/// selects. The table is a local, so it is copied from `.rodata` onto the
/// stack on every call.
void func_actor_800300_801625F4(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_800300_80161E24;
    states.funcs[task->state](task);
}

/// Handlers `func_actor_800300_80162C2C` runs, indexed by `field_954`.
const TaskFuncTable3 D_actor_800300_80161E34 = { {
    func_actor_800300_80162658,
    func_actor_800300_80162F24,
    func_actor_800300_80162F98,
} };

/// Behaviours `func_actor_800300_80162658` runs, indexed by `field_956`.
const TaskFuncTable9 D_actor_800300_80161E40 = { {
    func_actor_800300_80162C98,
    func_actor_800300_801628D0,
    func_actor_800300_80162A98,
    func_actor_800300_80162C98,
    func_actor_800300_80162C98,
    func_actor_800300_80162EEC,
    func_actor_800300_80162C98,
    func_actor_800300_80162D74,
    func_actor_800300_80162C98,
} };

void func_actor_800300_80162658(Task* arg0)
{
    TaskFuncTable9 sp;
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* obj;
    s8             cc;
    s32            pan;
    s32            depth;
    s32            anim;
    s32            sound;

    sp    = D_actor_800300_80161E40;
    actor = arg0->work;
    d4    = actor->field_910;
    obj   = arg0->extra.tmd->coords;
    if (d4->decisionTimer > 0) {
        d4->decisionTimer = (u16)d4->decisionTimer - 1;
    }
    if (D_8017A99C >= 0x30C) {
        if (actor->field_956 != 5) {
            actor->field_942++;
            if ((s16)actor->field_942 >= (s8)d4->repeatCount) {
                actor->field_942 = 0;
                d4->actionCount++;
                cc              = (u8)d4->repeatCount - 7;
                d4->repeatCount = cc;
                if (cc < 0x5A) {
                    d4->repeatCount = 0x3C;
                }
                actor->field_95C = 7;
                actor->field_956 = 5;
                actor->field_958 = 0;
                actor->field_95A = 0;
                actor->field_95E = 0;
                actor->field_97E = 1;
                Gp_PlayObjSfx(obj, (rand() & 1) + 0x55170005, 0);
                if (Gp_HurtAlly(arg0, 0, 0x40010, 0) != 0) {
                    return;
                }
                anim = 0x10;
                if ((s8)d4->actionCount >= 5) {
                    anim = 0x11;
                }
                Gp_AnimPlayChildSlotsEx(arg0, anim, 0, 3);
            }
        }
    }
    sp.funcs[actor->field_956](arg0);
    if ((s8)actor->field_97A == 0) {
        func_80109BB4(arg0, &actor->field_17C[0]);
        if ((u16)actor->field_96C != 0) {
            func_8010B9A4(arg0);
            pan   = (s8)Gp_GetObjPan(obj);
            depth = (s8)gpGetObjDepth(obj);
            sound = 7;
            if ((u16)actor->field_96C == 1) {
                sound = 6;
            }
            SndEvt_EnqueueType6(sound, pan, depth);
        }
    }
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
    if (D_80072830 <= 0) {
        Gp_StopPlayerAnim(arg0, 0);
    }
}

void func_actor_800300_801628D0(Task* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    VECTOR3*       vec;
    s32            dist;
    s32            angle;
    s32            arg;

    coord  = arg0->extra.tmd->coords;
    target = (gameGetPtrSlot(3))->extra.tmd->coords;
    actor  = arg0->work;
    switch (actor->field_95E) {
        case 0:
            actor->field_934 = 0;
            if (func_8010BC70(coord) >= 0xE00) {
                arg              = 4;
                actor->field_95E = 2;
                actor->field_958 = 3;
            } else {
            resume:
                if (actor->field_95E != 3) {
                    actor->field_95E = 1;
                }
                actor->field_958 = 7;
                arg              = 2;
            }
            Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 5);
            /* fallthrough */
        case 1:
        case 2:
        case 3:
            actor->field_973 = 1;
            dist             = func_8010BC70(coord);
            if (dist < 0x301) {
                Gp_ResetActorMove(arg0, 0);
                break;
            }
            if (actor->field_95E == 3) {
                break;
            }
            actor->field_934++;
            if (actor->field_934 == 0xB4) {
                actor->field_95E = 3;
                goto resume;
            }
            if (actor->field_93E > 0) {
                actor->field_93E = (u16)actor->field_93E - 1;
            } else {
                angle = rand() & 0x3FF;
                if ((0x800 - angle) < dist) {
                    goto in_range;
                }
                if (actor->field_95E == 2) {
                    goto reset;
                }
            in_range:
                if (dist < angle + 0xC00) {
                    break;
                }
                if (actor->field_95E != 1) {
                    break;
                }
            reset:
                actor->field_95E = 0;
                actor->field_93E = 0x3C;
            }
            break;
    }
    vec = (VECTOR3*)target->coord.t;
    func_8010BD88(arg0, vec);
    func_8010BE5C(arg0, vec);
    func_80105ED4(arg0);
}

void func_actor_800300_80162A98(Task* arg0)
{
    u8*            head;
    VECTOR3*       vec;
    GameActor*     actor;
    GpLinkNode*    node;
    TmdObject*     extra;
    GsCOORDINATE2* src;
    s32            val;
    s32            arg;
    s32            flag;

    actor            = arg0->work;
    extra            = (gameGetPtrSlot(3))->extra.tmd;
    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x10;
    vec              = (VECTOR3*)(head - 0x10);
    node             = actor->field_90C;
    src              = extra->coords;
    if (node != NULL) {
        if (!(node->state.b.flags & 1)) {
            Gp_GetLockPos(node, vec);
        } else {
            actor->field_95E = 2;
        }
    } else {
        ((VECTOR3*)(head - 0x10))->vx = src->coord.t[0];
        vec->vy                       = src->coord.t[1];
        vec->vz                       = src->coord.t[2];
    }
    switch (actor->field_95E) {
        case 0:
            flag             = 1;
            actor->field_95E = flag;
            if (func_8010BCF4((Task*)arg0, vec) < 0) {
                actor->field_93E = -1;
                arg              = 5;
            } else {
                actor->field_93E = 1;
                arg              = 6;
            }
            Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 5);
            /* fallthrough */
        case 1:
            actor->field_975 = (u8)actor->field_93E;
            val              = func_8010BCF4((Task*)arg0, vec);
            if (val < 0) {
                val = -val;
            }
            if ((val < 0x81) || (actor->field_95E == 2)) {
                Gp_ResetActorMove(arg0, 0);
            }
            break;
    }
    func_8010BE5C(arg0, (VECTOR3*)src->coord.t);
    func_80105ED4(arg0);
    SCRATCH_POP_BYTES(0x10);
}

void func_actor_800300_80162C2C(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable3 sp;

    sp               = D_actor_800300_80161E34;
    actor            = arg0->work;
    actor->field_973 = 0;
    actor->field_975 = 0;
    sp.funcs[actor->field_954](arg0);
    actor->field_986 = 0;
}

void func_actor_800300_80162C98(Task* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    s32            val;

    actor  = arg0->work;
    coord  = arg0->extra.tmd->coords;
    target = (gameGetPtrSlot(3))->extra.tmd->coords;
    if (((GameActor*)arg0->work)->field_910->decisionTimer <= 0) {
        func_8010BF7C(arg0, 0x14, 0x3F);
        if ((u32)(func_8010BC70(coord) - 0x581) < 0x87F) {
            func_actor_800300_80163048(arg0);
        }
        val = func_8010BCF4((Task*)arg0, (VECTOR3*)target->coord.t);
        if (val < 0) {
            val = -val;
        }
        if (val >= 0x200) {
            actor->field_90C = NULL;
            func_actor_800300_80163074(arg0);
        }
    }
    func_8010BE5C(arg0, (VECTOR3*)target->coord.t);
    func_80105ED4(arg0);
}

void func_actor_800300_80162D74(Task* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    GpLinkNode*    lock;
    u8*            head;
    VECTOR3*       vec;
    u16            state;

    coord            = arg0->extra.tmd->coords;
    target           = (gameGetPtrSlot(3))->extra.tmd->coords;
    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x10;
    vec              = (VECTOR3*)(head - 0x10);
    actor            = arg0->work;
    lock             = actor->field_90C;
    if (lock != NULL) {
        if (!(lock->state.b.flags & 1)) {
            Gp_GetLockPos(lock, vec);
        } else {
            actor->field_95E = 2;
        }
    } else {
        ((VECTOR3*)(head - 0x10))->vx = target->coord.t[0];
        vec->vy                       = target->coord.t[1];
        vec->vz                       = target->coord.t[2];
    }
    state = actor->field_95E;
    switch (state) {
        case 0:
            actor->field_95E = 1;
            actor->field_934 = 0;
            actor->field_958 = 3;
            Gp_AnimPlayChildSlotsEx(arg0, 0xC, 0, 5);
            /* fallthrough */
        case 1:
            actor->field_973 = 1;
            if (func_8010BC70(coord) < 0x601) {
                Gp_ResetActorMove(arg0, 0);
            }
            break;
    }
    func_8010BD88(arg0, vec);
    func_8010BE5C(arg0, vec);
    func_80105ED4(arg0);
    SCRATCH_POP_BYTES(0x10);
}

void func_actor_800300_80162EEC(Task* arg0)
{
    if (((GameActor*)arg0->work)->field_95E == 1) {
        Gp_ResetActorMove(arg0, 0);
    }
}

void func_actor_800300_80162F24(Task* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    s32            flag;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (actor->field_95E) {
        case 0:
            flag               = 1;
            actor->field_95E   = flag;
            coord->coord.t[1] += 0xC0;
        case 1:
            Gp_AnimTickChildSlots(arg0);
            Gp_TurnPlayer(arg0);
            Gp_StepPlayerMove(arg0);
            break;
    }
}

/// Handlers `func_actor_800300_80162F98` runs, indexed by `field_956`: the
/// gameplay module's own mode-2 player states.
const TaskFuncTable7 D_actor_800300_80161E64 = { {
    Gp_PlayerMode2State0,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State2,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State4,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State6,
} };

void func_actor_800300_80162F98(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable7 sp;

    sp    = D_actor_800300_80161E64;
    actor = arg0->work;
    sp.funcs[(u16)actor->field_956](arg0);
    Gp_TurnPlayer(arg0);
    if (D_80072830 <= 0) {
        Gp_StopPlayerAnim(arg0, 0);
    }
}

/// Switches the actor's update into its approach behaviour (entry 1 of the
/// behaviour table), restarting the behaviour's step and counters and setting
/// the approach timer to 60 frames.
void func_actor_800300_80163048(Task* arg0)
{
    GameActor* actor;

    actor            = arg0->work;
    actor->field_956 = 1;
    actor->field_95A = 1;
    actor->field_954 = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_942 = 0;
    actor->field_93E = 0x3C;
}

/// Switches the actor's update into its turn-to-face behaviour (entry 2 of
/// the behaviour table), restarting the behaviour's step and counters.
void func_actor_800300_80163074(Task* arg0)
{
    GameActor* actor;

    actor            = arg0->work;
    actor->field_956 = 2;
    actor->field_954 = 0;
    actor->field_958 = 0;
    actor->field_95A = 1;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_942 = 0;
}
