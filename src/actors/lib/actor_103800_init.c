#include "common.h"

#include "actors/actor_103800.h"
#include "gameplay/3CD8.h"
#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "main/wipsys.h"

void Actor03800_Fn00974(Actor103800* arg0);
void Actor03800_Fn00A98(Actor103800* arg0);
void Actor03800_Fn026F8(Actor103800* arg0);
void Actor03800_Fn02848(Actor103800* arg0);
void Actor03800_Fn02E50(Actor103800* arg0);
void Actor03800_Fn03594(Actor103800* arg0);
void Actor03800_Fn03628(Actor103800* arg0);
void Actor03800_Fn036EC(Actor103800* arg0);
void Actor03800_Fn03744(Actor103800* arg0);
void Actor03800_Fn037E0(Actor103800* arg0);
void Gp_ArmStateF0(s32 arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
s32  SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern u8  D_801153F2;
extern u8  D_801153F4;
extern s32 Gp_LcgState;

void              Actor03800_Fn003B8(Task* arg0);
extern GpPairSrcE Actor03800_D05F44;
extern u8         Actor03800_D05F60[];

void Actor03800_Fn000B8(GpEnemy* arg0, Task* arg1)
{
    GpObj*            obj;
    GpRec18*          records1;
    GpRec18*          records2;
    GpRec18*          records3;
    Actor103800Work*  work;
    s32               i;
    Actor103800Obj2C* extra;

    extra = arg1->extra;
    work  = Mem_Calloc(0x384, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->idMap     = (TaskIdMap*)work;
    extra->field_1C = &work->field_184;
    extra->field_C  = 0;
    extra->field_20 = &work->field_164;
    ((void (*)())Gp_IncStateF0Ref)(0);
    Actor03800_Fn003B8(arg1);
    arg0->field_4  = &work->field_344->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->field_18     = &((Actor103800Obj2C*)arg1->extra)->field_8[3];
    arg0->node.field_4 = 0;
    arg0->field_1C.vx  = 0;
    arg0->field_54     = (s32)work->field_1C4;
    arg0->field_1C.vy  = 0;
    arg0->field_1C.vz  = 0;
    arg0->field_50     = &Actor03800_D05F44;
    arg0->field_40     = (s16)Actor03800_D05F44.field_4;
    work->field_2C4    = &((Actor103800Obj2C*)arg1->extra)->field_8[3];
    work->field_2C8    = 0x200;
    work->field_2CA    = 1;
    func_800B3F84(&work->anim, Actor03800_D05F60, (GpAnimObj*)extra, work->field_104, work->slots);
    for (i = 1; i < 6; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    work->field_1AC = ((Actor103800Obj2C*)arg1->extra)->field_8;
    records1        = work->field_1C4;
    work->field_1B6 = -0xFA;
    work->field_1B0 = records1;
    work->field_1B4 = 0;
    work->field_1B8 = 0;
    work->field_1BC = 0x30026;
    work->field_1C0 = 0xFA;
    work->field_1C2 = 1U;
    Gp_LinkObj(2, (GpObj*)work->field_1A4);
    Gp_InitRec18Table(records1, 3, 0);
    work->field_214 = ((Actor103800Obj2C*)arg1->extra)->field_8;
    records2        = work->field_22C;
    work->field_21E = -0x12C;
    work->field_218 = records2;
    work->field_21C = 0;
    work->field_220 = 0;
    work->field_224 = 0x30026;
    work->field_228 = 0x12C;
    work->field_22A = 1U;
    Gp_LinkObj(2, (GpObj*)work->field_20C);
    Gp_InitRec18Table(records2, 4, 0);
    switch (work->field_350) {
        case 0:
            work->field_1C2 |= 0x8000;
            work->field_22A |= 0x4200;
            break;
        case 1:
            work->field_1C2 |= 0x8000;
            work->field_22A &= ~0x4200;
            break;
        case 2:
            work->field_1C2 |= 0x8000;
            work->field_22A &= ~0x4200;
            break;
        case 3:
            work->field_1C2 &= ~0x8000;
            work->field_22A &= ~0x4200;
            break;
    }
    obj             = (GpObj*)work->field_28C;
    work->field_294 = ((Actor103800Obj2C*)arg1->extra)->field_8;
    records3        = work->field_2AC;
    work->field_29E = -0xFA;
    work->field_2A0 = 0xFA;
    work->field_2A8 = 0xC8;
    work->field_298 = records3;
    work->field_29C = 0;
    work->field_2A4 = 0;
    work->field_2AA = 1U;
    Gp_LinkObj(3, obj);
    Gp_InitRec18Table(records3, 1, 0);
    work->field_2AA = (u16)(work->field_2AA | 0x8000);
    arg1->state     = 1;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_init", Actor03800_Fn003B8);

void Actor03800_Fn00974(Actor103800* arg0)
{
    Actor103800Ctx*  ctx;
    Actor103800Work* work;
    s16              damage;
    u16              remaining;
    u8               flags;

    ctx   = arg0->field_20;
    flags = ctx->field_4C;
    work  = arg0->field_1C;
    if (flags & 2) {
        if (work->field_350 == 0) {
            ctx->field_4C   = flags & 0xFD;
            work->field_352 = 6;
            work->field_354 = 0;
            work->field_356 = 0;
            work->field_37E = 1;
        } else if ((work->field_352 != 0xA) || (work->field_354 >= 4)) {
            work->field_352 = 0xA;
            work->field_354 = 0;
        }
    }
    if (ctx->field_4C & 0xC) {
        damage = Gp_TickObjFlag4((GpObj5C*)ctx);
        if (damage != 0) {
            func_800DA6E8(&ctx->node, (s32)damage, 0);
            remaining     = ctx->field_40 - damage;
            ctx->field_40 = remaining;
            if ((s16)remaining <= 0) {
                work->field_352 = 7;
            } else {
                work->field_352 = 5;
            }
            work->field_354 = 0;
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)ctx) != 0) {
            ctx->field_4C &= 0xF3;
        }
    }
}
