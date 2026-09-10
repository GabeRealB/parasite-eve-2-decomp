#include "actors/coord_to_view.h"
#include "common.h"
#include "psyq/inline_c.h"

#include "main/gfx.h"
#include "main/sound.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "main/session.h"
#include "gameplay/3A34.h"

#include "gameplay/D4.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_403100.h"
#include "actors/actor_403100_rotation.h"
#include "actors/actor_403100_regions.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_403100_80131E70;
extern TaskFuncTable3 D_actor_403100_80131E7C;

/// Overlay-wide work block; `Task::extra` is a `TmdObject` whose `field_8` is
/// this actor's `GsCOORDINATE2`.
extern Actor403100Work* D_actor_403100_80155808;
extern GpEnemy*         D_actor_403100_8015580C;

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void                  func_8017E3C8(void);
void                  func_8017E4B8(void);
extern u16            D_actor_403100_80147630;
extern GsCOORDINATE2* D_actor_403100_80155630;
extern u32            D_actor_403100_801556EC[];
extern u32            D_actor_403100_8015572C[];

extern s8                   D_8007216C;
extern s8                   D_8007218A;
extern u8                   D_80073BA9;
extern Actor403100AnimTable D_actor_403100_8015570C;
extern GpAnimBlk*           Gp_PlayerAnimBlkTbl[];
extern u16                  Gp_WeaponIdBase[];

extern Actor403100Light D_80114FF8;
extern u32              Gp_LcgState;

extern s16 D_80073BA0;
extern u8  D_801153F4;
extern u8  D_80165FC0;

/* Resolved through `configs/USA/sym/actors.imports.txt`. */
void func_8017E128(s32 arg0);

/* Still `INCLUDE_ASM` in this unit. */
void func_actor_403100_801326DC(Actor403100Work* work);
void func_actor_403100_8013712C(Task* arg0);
void func_actor_403100_8013C008(s16 arg0, s16 arg1);
void func_actor_403100_8013D74C(Task* arg0);
s32  func_actor_403100_80133928(void);
void func_actor_403100_801345E0(Task* arg0, Task* arg1);

void func_actor_403100_8013E6F0(Task* arg0);
void func_actor_403100_8013F12C(void);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80132064);

void func_actor_403100_80132320(Task* arg0)
{
    D_actor_403100_80155808->field_47C.field_8  = &((TmdObject*)arg0->extra)->field_8[3];
    D_actor_403100_80155808->field_47C.field_C  = (GpRec18*)D_actor_403100_80155808->pad_49C;
    D_actor_403100_80155808->field_47C.field_14 = 0x300;
    D_actor_403100_80155808->field_47C.field_10 = 0;
    D_actor_403100_80155808->field_47C.field_12 = 0;
    D_actor_403100_80155808->field_47C.field_18 = 0x3001F;
    D_actor_403100_80155808->field_47C.field_1C = 0x400;
    D_actor_403100_80155808->field_47C.flags    = 1;
    Gp_LinkObj(2, &D_actor_403100_80155808->field_47C);
    Gp_InitRec18Table((GpRec18*)D_actor_403100_80155808->pad_49C, 8, 0);
    D_actor_403100_80155808->field_47C.flags   |= 0x8000;
    D_actor_403100_80155808->field_414.field_8  = &((TmdObject*)arg0->extra)->field_8[1];
    D_actor_403100_80155808->field_414.field_C  = (GpRec18*)D_actor_403100_80155808->pad_49C;
    D_actor_403100_80155808->field_414.field_10 = 0;
    D_actor_403100_80155808->field_414.field_12 = 0;
    D_actor_403100_80155808->field_414.field_14 = 0;
    D_actor_403100_80155808->field_414.field_18 = 0x3001F;
    D_actor_403100_80155808->field_414.field_1C = 0x800;
    D_actor_403100_80155808->field_414.flags    = 1;
    Gp_LinkObj(2, &D_actor_403100_80155808->field_414);
    Gp_InitRec18Table((GpRec18*)D_actor_403100_80155808->pad_434, 3, 0);
    D_actor_403100_80155808->field_55C.field_18 = 0x3001F;
    D_actor_403100_80155808->field_414.flags   |= 0x8000;
    D_actor_403100_80155808->field_55C.field_8  = &((TmdObject*)arg0->extra)->field_8[7];
    D_actor_403100_80155808->field_55C.field_C  = (GpRec18*)D_actor_403100_80155808->pad_57C;
    D_actor_403100_80155808->field_55C.field_10 = -0x200;
    D_actor_403100_80155808->field_55C.field_12 = 0;
    D_actor_403100_80155808->field_55C.field_14 = 0x200;
    D_actor_403100_80155808->field_55C.field_1C = 0x3A0;
    D_actor_403100_80155808->field_55C.flags    = 1;
    Gp_LinkObj(3, &D_actor_403100_80155808->field_55C);
    Gp_InitRec18Table((GpRec18*)D_actor_403100_80155808->pad_57C, 1, 0);
    D_actor_403100_80155808->field_594.field_18 = 0x3001F;
    D_actor_403100_80155808->field_55C.flags   &= 0x7FFF;
    D_actor_403100_80155808->field_594.field_8  = &((TmdObject*)arg0->extra)->field_8[6];
    D_actor_403100_80155808->field_594.field_C  = (GpRec18*)D_actor_403100_80155808->pad_5B4;
    D_actor_403100_80155808->field_594.field_10 = -0x200;
    D_actor_403100_80155808->field_594.field_12 = 0;
    D_actor_403100_80155808->field_594.field_14 = 0x180;
    D_actor_403100_80155808->field_594.field_1C = 0x3A0;
    D_actor_403100_80155808->field_594.flags    = 1;
    Gp_LinkObj(3, &D_actor_403100_80155808->field_594);
    Gp_InitRec18Table((GpRec18*)D_actor_403100_80155808->pad_5B4, 1, 0);
    D_actor_403100_80155808->field_594.flags &= 0x7FFF;
}
void func_actor_403100_80132528(Task* arg0)
{
    SVECTOR        pos;
    SVECTOR        rotation;
    MATRIX         matrix;
    GameActor*     player;
    GsCOORDINATE2* joint;
    GsCOORDINATE2* playerCoord;
    s32            angle;
    s32            anim;
    u16            savedAngle;
    GsCOORDINATE2* coords;

    anim                               = D_actor_403100_80155808->field_5DE;
    playerCoord                        = (*Gp_ActorSlots)->extra->field_8;
    coords                             = ((TmdObject*)arg0->extra)->field_8;
    player                             = (*Gp_ActorSlots)->actor;
    angle                              = D_actor_403100_80155808->field_5E2;
    savedAngle                         = (u16)D_actor_403100_80155808->field_5E2;
    D_actor_403100_80155808->field_5E2 = angle * 2;
    func_actor_403100_8013E02C(anim, D_actor_403100_80155808->field_5E2, 0);
    func_actor_403100_801327CC(arg0);
    func_actor_403100_801328DC(arg0);
    func_actor_403100_8013D770(arg0);
    Gfx_ViewCoord.flg = 0;
    Gp_UpdateCoord(&Gfx_ViewCoord);
    joint         = &coords[7];
    coords[7].flg = 0;
    Gp_UpdateCoord(joint);
    pos.vx = -0x290;
    pos.vy = 0x1E8;
    pos.vz = 0x220;
    ActorCoordToView(joint, &pos);
    func_actor_403100_8013D2F4(joint, &matrix);
    Gp_MtxToEuler(&matrix, &rotation);
    player->field_50 = rotation.vx;
    player->field_52 = rotation.vy;
    player->field_54 = rotation.vz;
    RotMatrix(&rotation, &playerCoord->coord);
    playerCoord->coord.t[0] = pos.vx;
    playerCoord->coord.t[1] = pos.vy;
    playerCoord->coord.t[2] = pos.vz;
    playerCoord->flg        = 0;
    Gp_UpdateCoord(playerCoord);
    D_actor_403100_80155808->field_5E2 = (-angle) << 1;
    func_actor_403100_8013E02C(D_actor_403100_80155808->field_5DE, D_actor_403100_80155808->field_5E2, 0);
    func_actor_403100_801327CC(arg0);
    D_actor_403100_80155808->field_5E2 = (s16)savedAngle;
    func_actor_403100_8013E02C(D_actor_403100_80155808->field_5DE, angle, 0);
}
void func_actor_403100_801326DC(Actor403100Work* work)
{
    s32 i;

    if ((s16)D_actor_403100_80155808->field_5DC == D_actor_403100_80155808->field_5DE) {
        for (i = 1; i < 15; i++) {
            D_actor_403100_80155808->field_B8.animation.slots[i].field_9 = (u8)D_actor_403100_80155808->field_5E2;
        }
    } else {
        for (i = 1; i < 15; i++) {
            D_actor_403100_80155808->field_B8.animation.slots[i].field_9 = (u8)D_actor_403100_80155808->field_5E2;
            func_800B4114(&D_actor_403100_80155808->field_B8.animation.anim, i, D_actor_403100_80155808->field_5DE, 0, D_actor_403100_80155808->field_5FC);
        }
        D_actor_403100_80155808->field_5FC = 0;
    }
    D_actor_403100_80155808->field_5DC = D_actor_403100_80155808->field_5DE;
}
void func_actor_403100_801327CC()
{
    s32 i;
    if (D_actor_403100_80155808->field_5DA == 1) {
        func_actor_403100_801326DC(D_actor_403100_80155808);
        D_actor_403100_80155808->field_5DA = 3;
        D_actor_403100_80155808->field_5E0 = 0;
    } else if (D_actor_403100_80155808->field_5DA == 2) {
        for (i = 1; i < 15; i++) {
            Gp_AnimResetSlot(&D_actor_403100_80155808->field_B8.animation.anim, i, D_actor_403100_80155808->field_5DE);
            D_actor_403100_80155808->field_B8.animation.slots[i].field_9 = (u8)D_actor_403100_80155808->field_5E2;
        }
        D_actor_403100_80155808->field_5DA = 3;
        D_actor_403100_80155808->field_5E0 = 0;
        D_actor_403100_80155808->field_5DC = D_actor_403100_80155808->field_5DE;
    } else if (D_actor_403100_80155808->field_5DA == 3) {
        D_actor_403100_80155808->field_5E0 += 1;
    }
    for (i = 1; i < 15; i++) {
        Gp_AnimTickIndex(&D_actor_403100_80155808->field_B8.animation.anim, i);
    }
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801328DC);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80132C3C);
void func_actor_403100_801331D4(Task* arg0)
{
    SVECTOR        pos;
    GsCOORDINATE2* joint;
    GsCOORDINATE2* playerCoord;
    s16            dx;
    s16            dx2;
    s16            dz;
    s16            dz2;
    GsCOORDINATE2* coords;

    coords = ((TmdObject*)arg0->extra)->field_8;
    joint  = &coords[3];
    if (*Gp_ActorSlots != NULL) {
        playerCoord                        = (*Gp_ActorSlots)->extra->field_8;
        D_actor_403100_80155808->field_90  = (u16)playerCoord->coord.t[0];
        D_actor_403100_80155808->field_92  = (u16)playerCoord->coord.t[1];
        D_actor_403100_80155808->field_94  = (u16)playerCoord->coord.t[2];
        D_actor_403100_80155808->field_98  = (u16)playerCoord->coord.t[0];
        D_actor_403100_80155808->field_9A  = (u16)playerCoord->coord.t[1];
        D_actor_403100_80155808->field_9C  = (u16)playerCoord->coord.t[2];
        dx                                 = (u16)playerCoord->coord.t[0] - (u16)coords->coord.t[0];
        pos.vx                             = dx;
        pos.vy                             = (u16)playerCoord->coord.t[1] - (u16)coords->coord.t[1];
        dz                                 = (u16)playerCoord->coord.t[2] - (u16)coords->coord.t[2];
        pos.vz                             = dz;
        D_actor_403100_80155808->field_62E = SquareRoot0((dx * dx) + (dz * dz));
        ActorCoordToView(joint, &pos);
        dx2                                = (u16)playerCoord->coord.t[0] - (u16)pos.vx;
        pos.vx                             = dx2;
        pos.vy                             = (u16)playerCoord->coord.t[1] - pos.vy;
        dz2                                = (u16)playerCoord->coord.t[2] - (u16)pos.vz;
        pos.vz                             = dz2;
        D_actor_403100_80155808->field_630 = SquareRoot0((dx2 * dx2) + (dz2 * dz2));
    }
}
INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013335C);

s32 func_actor_403100_80133928(void)
{
    s16 state;
    s8  mode;

    mode = D_actor_403100_80155808->pad_66A[2];
    if (mode == 1) {
        state = D_actor_403100_80155808->field_62A;
        if (state == mode) {
            D_actor_403100_80155808->field_62A = 0;
            func_actor_403100_8013D24C();
            return 0;
        }
        if (state == 2) {
            SndEvt_EnqueueType7(0x401F0004, 0xA);
            func_actor_403100_8013D24C();
            D_actor_403100_80155808->field_62A = 0;
            D_actor_403100_80155808->field_5F8 = 8;
            D_actor_403100_80155808->field_5FA = 0;
            return 1;
        }
        if (state == 3) {
            SndEvt_EnqueueType7(0x401F0004, 0xA);
            func_actor_403100_8013D24C();
            D_actor_403100_80155808->field_62A = 0;
            D_actor_403100_80155808->field_5F8 = 0xA;
            D_actor_403100_80155808->field_5FA = 0;
            return 1;
        }
        D_actor_403100_80155808->field_62A = 0;
        return 0;
    }
    return 0;
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801339EC);
void func_actor_403100_80133C94(void)
{
    s32 i;

    SndEvt_EnqueueType7(0x401F0004, 0xA);
    Gp_UnlinkNode(&D_actor_403100_8015580C->node);
    D_actor_403100_80155810 = 0;
    for (i = 0; i < 28; i++) {
        if (D_actor_403100_80155814[i].active != 0) {
            D_actor_403100_80155814[i].active = 0;
            Gp_UnlinkObj(&D_actor_403100_80155814[i].obj);
        }
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    D_actor_403100_80155808->field_5FC  = 0x20;
    D_actor_403100_80155808->field_5E2  = 0x10;
    D_actor_403100_80155808->field_5DE  = 0x11;
    D_actor_403100_80155808->field_5DA  = 1;
    D_actor_403100_80155808->field_5D8  = 0x1400;
    D_actor_403100_80155808->field_5EC  = 0;
    D_actor_403100_80155808->field_604  = 0;
    D_actor_403100_80155808->field_608  = 0;
    D_actor_403100_80155808->field_5F8 += 1;
}
void func_actor_403100_80133D88(Task* arg0)
{
    GsCOORDINATE2* coord;
    u16            frame;

    coord                              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame >= 0xA0) {
        coord->coord.t[1] += 0xA;
    }
    if ((s16)D_actor_403100_80155808->field_5EC >= 0x82) {
        func_actor_403100_801345E0(arg0, arg0);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x12C) {
        D_actor_403100_80155808->field_5D8 = 0x1600;
        coord->coord.t[0]                  = -0xA28;
        D_actor_403100_80155808->field_5E2 = 0x10;
        D_actor_403100_80155808->field_5DE = 0xC;
        D_actor_403100_80155808->field_5DA = 2;
        coord->coord.t[1]                  = 0x1390;
        coord->coord.t[2]                  = 0x1130;
        D_actor_403100_80155808->field_82  = -0x6B0;
        D_actor_403100_80155808->field_604 = -0x140;
        D_actor_403100_80155808->field_608 = -0x100;
        D_actor_403100_80155808->field_5EC = 0;
        D_actor_403100_80155808->field_5F8 = D_actor_403100_80155808->field_5F8 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80133E88);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801342B4);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801345E0);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013480C);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131E70);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131E7C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80134D50);
void func_actor_403100_8013506C(Task* arg0)
{
    GsCOORDINATE2* coord;
    u16            frame;
    s32            sound;
    s32            pan;
    s32            sound2;
    s32            pan2;

    coord                              = ((TmdObject*)arg0->extra)->field_8;
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    coord->coord.t[1]                  = (s16)(rcos((s16)frame * 0x20) << 0xD >> 0x10) - 0x300;
    coord->coord.t[0]                 += 0x40;
    if ((s16)D_actor_403100_80155808->field_5EC == 0x40) {
        D_actor_403100_80155808->field_5EC = 0;
        Gp_SpawnPadLerp(0x1E, 0xFF, 8);
        D_actor_403100_80155808->field_5FE = 0x1E;
        func_8017E128(0);
        sound = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0001;
        pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]) / 2));
        sound2 = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0002;
        pan2   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
        SndEvt_EnqueueType6(sound2, pan2, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]) / 2));
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_801351F8(Task* arg0)
{
    u16 frame;
    s32 sound;
    s32 pan;
    s32 sound2;
    s32 pan2;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0x3E) {
        func_8017E128(0);
        Gp_SpawnPadLerp(0x1E, 0xFF, 8);
        D_actor_403100_80155808->field_5FE = 0x1E;
        sound                              = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0001;
        pan                                = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]) / 2));
        sound2 = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0002;
        pan2   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
        SndEvt_EnqueueType6(sound2, pan2, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]) / 2));
        D_actor_403100_80155808->field_5FA += 1;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x28) {
        D_actor_403100_80155808->field_61C = 3;
    }
    if ((s16)D_actor_403100_80155808->field_5EC >= 0x28) {
        D_actor_403100_80155808->field_98 = -0x3E8;
        D_actor_403100_80155808->field_9A = 0;
        D_actor_403100_80155808->field_9C = -0x960;
    }
}
void func_actor_403100_8013539C(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            i;

    D_actor_403100_80155810 = 0;
    coord                   = ((TmdObject*)arg0->extra)->field_8;
    for (i = 0; i < 28; i++) {
        if (D_actor_403100_80155814[i].active != 0) {
            D_actor_403100_80155814[i].active = 0;
            Gp_UnlinkObj(&D_actor_403100_80155814[i].obj);
        }
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    coord->coord.t[0]                  = -0x74E;
    coord->coord.t[2]                  = -0x1C51;
    coord->coord.t[1]                  = 0;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 2;
    D_actor_403100_80155808->field_5DA = 2;
    D_actor_403100_80155808->field_82  = 0;
    D_actor_403100_80155808->field_5EC = 0;
    D_actor_403100_80155808->field_618 = 0x1910;
    func_actor_403100_801327CC(arg0);
    D_actor_403100_80155808->field_5FA += 1;
}
void func_actor_403100_801354A0(Task* arg0)
{
    s32 sound;
    s32 pan;
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0xC) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0001;
        pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]) / 2));
        Gp_SpawnPadLerp(0x1E, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x1E;
        func_8017E128(1);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x18) {
        func_8017E128(0);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x28) {
        D_actor_403100_80155808->field_5FA += 1;
    }
    func_actor_403100_801327CC(arg0);
}
void func_actor_403100_801355D4(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            i;

    D_actor_403100_80155810                   = 0;
    coord                                     = ((TmdObject*)arg0->extra)->field_8;
    D_actor_403100_80155808->field_61C        = 3;
    D_actor_403100_80155808->flags_634.h.high = 0x1C;
    for (i = 0; i < 28; i++) {
        if (D_actor_403100_80155814[i].active != 0) {
            D_actor_403100_80155814[i].active = 0;
            Gp_UnlinkObj(&D_actor_403100_80155814[i].obj);
        }
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    coord->coord.t[0]                  = -0x74E;
    coord->coord.t[2]                  = -0x1770;
    coord->coord.t[1]                  = 0;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 7;
    D_actor_403100_80155808->field_5DA = 2;
    D_actor_403100_80155808->field_82  = 0;
    D_actor_403100_80155808->field_5EC = 0;
    D_actor_403100_80155808->field_618 = 0x1400;
    func_actor_403100_801327CC(arg0);
    D_actor_403100_80155808->field_5FA += 1;
}
static __inline__ s16 Actor403100_TestFlags104(void)
{
    if (D_actor_403100_80155808->field_B8.legacy.flags_104.half & 1) {
        return 1;
    }
    if (D_actor_403100_80155808->field_B8.legacy.flags_104.word & 0x102) {
        return 1;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801356F4);
void func_actor_403100_8013588C(Task* arg0)
{
    s32 sound;
    s32 pan;
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0xE) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0001;
        pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]) / 2));
        Gp_SpawnPadLerp(0x1E, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x1E;
    }
    if ((u32)(D_actor_403100_80155808->field_5EC - 0xE) < 7U) {
        func_8017E128(D_actor_403100_801557A8[D_actor_403100_80155808->field_5EE]);
        D_actor_403100_80155808->field_5EE += 1;
    }
    if ((s16)D_actor_403100_80155808->field_5EC >= 0x15) {
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_801359DC(Task* arg0)
{
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   joint;
    s32              i;
    Actor403100Work* work;
    s32              value;

    value                   = 0x10;
    D_actor_403100_80155810 = 0;
    coord                   = ((TmdObject*)arg0->extra)->field_8;
    joint                   = &coord[6];
    for (i = 0; i < 28; i++) {
        if (D_actor_403100_80155814[i].active != 0) {
            D_actor_403100_80155814[i].active = 0;
            Gp_UnlinkObj(&D_actor_403100_80155814[i].obj);
        }
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    joint->coord.t[0] = -0x807;
    coord->coord.t[0] = -0x384;
    work              = D_actor_403100_80155808;
    coord->coord.t[2] = 0x1130;
    coord->coord.t[1] = 0;
    work->field_5E2   = value;
    work->field_5DE   = 0x13;
    work->field_5DA   = 2;
    work->field_5EC   = 0;
    work->field_618   = 0x1400;
    work->field_5FA  += 1;
    Gp_ApplyAreaRecs(&D_8018F2CC);
}
void func_actor_403100_80135AE0(Task* arg0)
{
    s32 sound;
    s32 pan;
    u16 angle;

    angle                             = (u16)D_actor_403100_80155808->field_82;
    D_actor_403100_80155808->field_82 = angle + ((s16)(-0x4000 - angle * 0x10) >> 9);
    if ((s16)D_actor_403100_80155808->field_5EC == 0xBE) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0001;
        pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]) / 2));
        Gp_SpawnPadLerp(0x1E, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x1E;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0xF0) {
        D_actor_403100_80155808->field_5FA += 1;
    }
    D_actor_403100_80155808->field_5EC += 1;
}
static __inline__ s16 Actor403100_TestFlags12C(void)
{
    if (D_actor_403100_80155808->field_B8.legacy.flags_12C & 0x100) {
        return 1;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80135C00);
void func_actor_403100_80135F30(Task* arg0)
{
    s32        sound;
    s32        sound2;
    s32        pan;
    s32        pan2;
    u16        frame;
    s32        depth;
    s32        depth2;
    TmdObject* obj;

    obj                                = arg0->extra;
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0xA) {
        func_8018257C();
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0004;
        pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
        depth = Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0xE) {
        SndEvt_EnqueueType7(0x401F0004, 0xA);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x26) {
        sound2 = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0002;
        pan2   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
        depth2 = Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
        SndEvt_EnqueueType6(sound2, (s32)pan2, (s8)(depth2 / 2));
    }
    if (Actor403100_TestFlags12C()) {
        D_actor_403100_80155808->field_5FC  = 0x18;
        D_actor_403100_80155808->field_5E2  = 0x18;
        D_actor_403100_80155808->field_5DE  = 0x16;
        D_actor_403100_80155808->field_5DA  = 1;
        D_actor_403100_80155808->field_5EC  = 0;
        D_actor_403100_80155808->field_600  = 0;
        D_actor_403100_80155808->field_5FA += 1;
        obj->field_E                        = 0;
    }
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80136100);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013631C);
void func_actor_403100_80136610(Task* arg0)
{
    Actor403100Work* work;
    TmdObject*       obj;
    GpEnemy*         enemy;
    s32              i;
    u16*             flags;
    s32              kind;
    GsCOORDINATE2*   coord;

    obj   = arg0->extra;
    coord = obj->field_8;
    if ((*(u32*)&Game_Session->field_4 & ~0xFF) != 0x031D0200 ||
        (arg0->idMap = Mem_Calloc(0x678U, false)) == NULL) {
        Gp_DestroyEnemy(D_actor_403100_8015580C, arg0);
        return;
    }
    enemy                                = ((volatile Task*)arg0)->spawnArg2;
    work                                 = ((volatile Task*)arg0)->idMap;
    obj->field_1C                        = &work->field_0.matrices.light;
    D_actor_403100_8015580C              = enemy;
    D_actor_403100_80155808              = work;
    obj->field_20                        = &work->field_0.matrices.color;
    arg0->field_24                       = &D_actor_403100_801556EC;
    work->field_622                      = (s16)Game_Session->field_4;
    enemy->field_48                      = 0;
    enemy->field_4                       = &coord->coord;
    D_actor_403100_8015580C->field_1C.vx = 0;
    D_actor_403100_8015580C->field_1C.vy = 0;
    D_actor_403100_8015580C->field_1C.vz = 0x300;
    D_actor_403100_8015580C->field_18    = &((TmdObject*)arg0->extra)->field_8[3];
    Gp_LinkNode(&D_actor_403100_8015580C->node);
    kind = 9;
    TOUCH_REG(kind);
    D_actor_403100_8015580C->node.field_4 = kind;
    D_actor_403100_8015580C->field_50     = &D_actor_403100_8014762C;
    D_actor_403100_8015580C->field_54     = (s32)D_actor_403100_80155808->pad_49C;
    D_actor_403100_80155630               = ((TmdObject*)arg0->extra)->field_8;
    flags                                 = &obj->field_C;
    *flags                                = 0;
    D_actor_403100_80155808->field_658    = -1;
    func_800B3F84(&D_actor_403100_80155808->field_B8.animation.anim, &D_actor_403100_8015572C, (GpAnimObj*)obj, &D_actor_403100_80155808->field_B8.legacy.pad_12E[0x1F6], D_actor_403100_80155808->field_B8.animation.slots);
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 1;
    D_actor_403100_80155808->field_5DA = 2;
    func_actor_403100_801327CC(arg0);
    coord->sub = &Gfx_ViewCoord;
    func_actor_403100_80132320(arg0);
    for (i = 27; i >= 0; i--) {
        D_actor_403100_80155814[i].active = 0;
    }
    func_8017E4B8();
    func_8017E3C8();
    D_actor_403100_80155810            = 0;
    D_actor_403100_8015580C->field_42  = D_actor_403100_80147630;
    D_actor_403100_8015580C->field_40  = (s16)D_actor_403100_80147630;
    arg0->state                        = 1;
    D_actor_403100_80155808->field_5F8 = 0;
    D_actor_403100_80155808->field_5FA = 0;
}
INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EB0);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EBC);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EC8);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131ED8);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EE4);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EF0);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F00);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F10);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F1C);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F34);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80136830);
void func_actor_403100_8013712C(Task* arg0)
{
    Actor403100Entry* entries;
    GpObj*            obj;
    s32               i;
    GsCOORDINATE2*    coord;
    Actor403100Work*  work;

    coord                                 = ((TmdObject*)arg0->extra)->field_8;
    D_actor_403100_8015580C->node.field_4 = 8;
    D_actor_403100_80155808->field_5E6    = 0x1E;
    D_actor_403100_80155808->field_62C    = 0x20;
    D_actor_403100_80155808->field_600    = 0;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    i                                        = 0;
    entries                                  = D_actor_403100_80155814;
    obj                                      = &entries->obj;
    work                                     = *(Actor403100Work* volatile*)&D_actor_403100_80155808;
    coord->coord.t[0]                        = -0x44C;
    coord->coord.t[2]                        = 0x980;
    *(volatile s16*)&D_actor_403100_80155810 = 0;
    coord->coord.t[1]                        = 0;
    work->field_82                           = 0xC00;
    work->field_5D0                          = 0x1518;
    work->field_5E2                          = 0x10;
    work->field_5DE                          = 1;
    work->field_80                           = 0;
    work->field_84                           = 0;
    work->field_5DA                          = 2;
    work->field_5EC                          = 0;
    for (; i < 0x1C; i++) {
        if (entries[i].active != 0) {
            entries[i].active = 0;
            Gp_UnlinkObj(obj);
        }
        obj = (GpObj*)((u8*)obj + sizeof(Actor403100Entry));
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    D_actor_403100_80155808->field_5FA += 1;
}
INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F60);
