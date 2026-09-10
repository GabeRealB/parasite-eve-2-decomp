#include "common.h"
#include "psyq/inline_c.h"

#include "main/sound.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "gameplay/D4.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_403100.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_403100_80131E70;
extern TaskFuncTable3 D_actor_403100_80131E7C;

/// Overlay-wide work block; `Task::extra` is a `TmdObject` whose `field_8` is
/// this actor's `GsCOORDINATE2`.
extern Actor403100Work* D_actor_403100_80155808;
extern GpEnemy*         D_actor_403100_8015580C;

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void                        func_8017E3C8(void);
void                        func_8017E4B8(void);
extern u16                  D_actor_403100_80147630;
extern GsCOORDINATE2*       D_actor_403100_80155630;
extern u32                  D_actor_403100_801556EC[];
extern u32                  D_actor_403100_8015572C[];
extern Actor403100QuadEntry D_actor_403100_801557E0[2];

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
void func_80182730(void);

/* Still `INCLUDE_ASM` in this unit. */
void func_actor_403100_801326DC(Actor403100Work* work);
void func_actor_403100_8013712C(Task* arg0);
void func_actor_403100_8013C008(s16 arg0, s16 arg1);
void func_actor_403100_8013D74C(Task* arg0);
s32  func_actor_403100_80133928(void);
void func_actor_403100_801345E0(Task* arg0, Task* arg1);

void func_actor_403100_8013E6F0(Task* arg0);
void func_actor_403100_8013F12C(void);

static __inline__ s16 Actor403100_TestFlags(void)
{
    if (D_actor_403100_80155808->flags_634.half & 1) {
        return 1;
    }
    if (D_actor_403100_80155808->flags_634.word & 0x102) {
        return 1;
    }
    return 0;
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

static __inline__ s16 Actor403100_TestFlags12C(void)
{
    if (D_actor_403100_80155808->field_B8.legacy.flags_12C & 0x100) {
        return 1;
    }
    return 0;
}

void func_actor_403100_80137268(void)
{
    s16 state;

    state = D_actor_403100_80155808->field_628 - 1;
    switch (state) {
        case 0:
            if (D_actor_403100_80155808->field_62E < 0x1F40) {
                D_actor_403100_80155808->field_5FA += 1;
            }
            break;
        case 1:
        case 5:
            D_actor_403100_80155808->field_5FA += 1;
            break;
        case 2:
        case 3:
        case 4:
            if (D_actor_403100_80155808->field_62E < 0x17D4) {
                D_actor_403100_80155808->field_5FA += 1;
            }
            break;
    }
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80137310);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_801375B8);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_801376D8);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_801379B4);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80137CA8);
void func_actor_403100_80137DC4(Task* arg0)
{
    s32 sound;
    s32 sound2;
    s32 pan;
    s32 pan2;
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0x31) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0002;
        pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
        SndEvt_EnqueueType6(sound, pan, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]) / 2));
        sound2 = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0001;
        pan2   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]);
        SndEvt_EnqueueType6(sound2, pan2, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]) / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x12;
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
void func_actor_403100_80137F4C(void)
{
    Actor403100Entry* entry;
    GpObj*            obj;
    Actor403100Entry* entries;
    s32               i;

    D_actor_403100_80155808->field_5F6 = 0;
    if (D_actor_403100_80155808->field_65C == 0) {
        D_actor_403100_80155808->field_62C = 0x1C;
    } else {
        D_actor_403100_80155808->field_62C = 0x30;
    }
    i                                         = 0;
    entries                                   = D_actor_403100_80155814;
    obj                                       = &D_actor_403100_80155814->obj;
    entry                                     = entries;
    D_actor_403100_80155810                   = 0;
    D_actor_403100_80155808->flags_634.h.high = 0x14;
    for (; i < 0x1C; i++) {
        if (entry->active != 0) {
            entry->active = 0;
            Gp_UnlinkObj(obj);
        }
        obj = (GpObj*)((u8*)obj + sizeof(Actor403100Entry));
        entry++;
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    D_actor_403100_80155808->field_5E2  = 0x10;
    D_actor_403100_80155808->field_5DE  = 7;
    D_actor_403100_80155808->field_5DA  = 2;
    D_actor_403100_80155808->field_61C  = 1;
    D_actor_403100_80155808->field_5EC  = 0;
    D_actor_403100_80155808->field_5FA += 1;
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80138048);
void func_actor_403100_8013842C(Task* arg0)
{
    SVECTOR  offset;
    s32      sound;
    s32      sound2;
    s32      pan;
    GpObj38* effectCoord;
    s32      pan2;
    u16      frame;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0x31) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0002;
        pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
        SndEvt_EnqueueType6(sound, pan, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]) / 2));
        sound2 = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0001;
        pan2   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]);
        SndEvt_EnqueueType6(sound2, pan2, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]) / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE             = 0x12;
        D_actor_403100_80155808->field_664.b.field_666 = 1;
    }
    if ((u32)(D_actor_403100_80155808->field_5EC - 0x32) < 0xBU) {
        effectCoord = (GpObj38*)&((TmdObject*)arg0->extra)->field_8[3];
        offset.vy   = -0x140;
        offset.vx   = 0;
        offset.vz   = 0x400;
        Gp_SpawnEff(0x60070, effectCoord, -0x3FFCB400, &offset);
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
void func_actor_403100_80138610(Task* arg0)
{
    u16            counter;
    u16            timer;
    u16            angle;
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        coord->coord.t[1]                 += 0x30;
        timer                              = D_actor_403100_80155808->field_5EC;
        D_actor_403100_80155808->field_5EC = timer + 1;
        if ((s16)timer >= 0x11) {
            counter                                             = *(volatile u16*)&D_actor_403100_80155808->field_5FA;
            *(volatile s16*)&D_actor_403100_80155808->field_61E = 0;
            *(volatile s16*)&D_actor_403100_80155808->field_61E = 2;
            angle                                               = *(volatile u16*)&D_actor_403100_80155808->field_B2;
            D_actor_403100_80155808->field_5EC                  = 0;
            D_actor_403100_80155808->field_620                  = 0;
            D_actor_403100_80155808->field_626                  = (s16)angle;
            D_actor_403100_80155808->field_5FA                  = counter + 1;
            if ((s16)angle >= 0xD1) {
                D_actor_403100_80155808->field_626 = 0xD0;
            }
            if (D_actor_403100_80155808->field_626 < -0x160) {
                D_actor_403100_80155808->field_626 = -0x160;
            }
        }
    }
}
void func_actor_403100_801386DC(Task* arg0)
{
    u16            velocity;
    u16            accel;
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)arg0->extra)->field_8;
    func_actor_403100_8013D24C();
    D_actor_403100_80155808->field_608 =
        (u16)D_actor_403100_80155808->field_608 +
        ((s32)(((u16)D_actor_403100_80155808->field_626 - (u16)D_actor_403100_80155808->field_608) << 0x14) >> 0x17);
    D_actor_403100_80155808->field_5EC += 1;
    accel                               = D_actor_403100_80155808->field_61E + 4;
    velocity                            = D_actor_403100_80155808->field_620 + accel;
    D_actor_403100_80155808->field_620  = velocity;
    D_actor_403100_80155808->field_61E  = accel;
    coord->coord.t[1]                  -= (s16)velocity;
    if ((s16)D_actor_403100_80155808->field_5EC >= 6) {
        D_actor_403100_80155808->field_5EC  = 0;
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_80138790(Task* arg0)
{
    u16            velocity;
    u16            accel;
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)arg0->extra)->field_8;
    func_actor_403100_8013D24C();
    D_actor_403100_80155808->field_608 =
        (u16)D_actor_403100_80155808->field_608 +
        ((s32)(((u16)D_actor_403100_80155808->field_626 - (u16)D_actor_403100_80155808->field_608) << 0x14) >> 0x17);
    D_actor_403100_80155808->field_5EC += 1;
    accel                               = D_actor_403100_80155808->field_61E - 4;
    velocity                            = D_actor_403100_80155808->field_620 + accel;
    D_actor_403100_80155808->field_620  = velocity;
    D_actor_403100_80155808->field_61E  = accel;
    coord->coord.t[1]                  -= (s16)velocity;
    if ((s16)D_actor_403100_80155808->field_5EC >= 6) {
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_80138844(Task* arg0)
{
    Task*          player;
    s32            sound;
    s32            sound2;
    s32            y;
    s32            pan;
    s32            pan2;
    u16            velocity;
    u16            accel;
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)arg0->extra)->field_8;
    func_actor_403100_8013D24C();
    if ((D_actor_403100_80155808->field_668.flags != 0) && (D_actor_403100_80155808->field_5F2 == 0)) {
        Gp_StateC08.field_6 |= 1;
        func_actor_403100_8013D1B8(5, 0x3F4);
        D_actor_403100_80155808->field_5F4 = 0x17;
        D_actor_403100_80155808->field_5F2 = 1;
        player                             = Game_GetPtrSlot(3);
        if (Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(&D_actor_403100_80147614, 0), 0) == 1) {
            (*Gp_ActorSlots)->actor->field_956 = 0xA;
        }
    }
    func_actor_403100_8013C7B4(arg0);
    D_actor_403100_80155808->field_5EC += 1;
    D_actor_403100_80155808->field_608 =
        (u16)D_actor_403100_80155808->field_608 +
        ((s32)(((u16)D_actor_403100_80155808->field_626 - (u16)D_actor_403100_80155808->field_608) << 0x14) >> 0x17);
    accel                              = D_actor_403100_80155808->field_61E - 4;
    velocity                           = D_actor_403100_80155808->field_620 + accel;
    D_actor_403100_80155808->field_620 = velocity;
    D_actor_403100_80155808->field_61E = accel;
    y                                  = coord->coord.t[1] - (s16)velocity;
    coord->coord.t[1]                  = y;
    if (y >= 0) {
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        D_actor_403100_80155808->field_668.b.field_669 = 0;
        func_actor_403100_801342B4(arg0);
        Gp_SpawnPadLerp(0x1E, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x1E;
        sound                              = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0001;
        pan                                = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]) / 2));
        sound2 = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0003;
        pan2   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[7]);
        SndEvt_EnqueueType6(sound2, pan2, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[7]) / 2));
        coord->coord.t[1]                   = 0;
        D_actor_403100_80155808->field_5EC  = 0;
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_80138AB4(void)
{
    Task* player;
    u16   frame;

    func_actor_403100_8013D24C();
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if (((s16)frame < 0x20) && ((u8)D_actor_403100_80155808->field_668.b.field_668 != 0) && (D_actor_403100_80155808->field_5F2 == 0)) {
        Gp_StateC08.field_6 |= 1;
        func_actor_403100_8013D1B8(5, 0x3F4);
        D_actor_403100_80155808->field_5F4 = 0x17;
        D_actor_403100_80155808->field_5F2 = 1;
        player                             = Game_GetPtrSlot(3);
        if (Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(&D_actor_403100_80147614, 0), 0) == 1) {
            (*Gp_ActorSlots)->actor->field_956 = 0xA;
        }
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5E2  = 0x10;
        D_actor_403100_80155808->field_5DE  = 5;
        D_actor_403100_80155808->field_5F6  = 0;
        D_actor_403100_80155808->field_5DA  = 2;
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_80138C18(void)
{
    u16 velocityZ;
    u16 velocityX;
    u32 random;

    func_actor_403100_8013D24C();
    velocityX                          = (u16)D_actor_403100_80155808->field_604;
    velocityZ                          = (u16)D_actor_403100_80155808->field_608;
    D_actor_403100_80155808->field_604 = velocityX + ((s32) - (velocityX << 0x14) >> 0x17);
    D_actor_403100_80155808->field_608 = velocityZ + ((s32) - (velocityZ << 0x14) >> 0x17);
    if (Actor403100_TestFlags104()) {
        random      = (Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState = random;
        if ((random >> 0x10) & 1) {
            func_actor_403100_8013D2A0(1);
        }
        D_actor_403100_80155808->field_5FC  = 8;
        D_actor_403100_80155808->field_5E2  = 0x10;
        D_actor_403100_80155808->field_5EC  = 0;
        D_actor_403100_80155808->field_5DE  = 1;
        D_actor_403100_80155808->field_5DA  = 1;
        D_actor_403100_80155808->field_5FA += 1;
    }
}
void func_actor_403100_80138D08(Task* arg0)
{
    TmdObject*       obj;
    Actor403100Work* work;

    obj = arg0->extra;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        obj->field_E                                   = 0;
        work                                           = D_actor_403100_80155808;
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        work->field_62C                                = 0x40;
        work->field_5E2                                = 0x10;
        work->field_5DE                                = 9;
        work->field_5F6                                = 0;
        work->field_5DA                                = 2;
        work->field_61C                                = 2;
        work->field_5EC                                = 0;
        work->field_604                                = 0;
        work->field_608                                = 0;
        work->field_632                                = 0;
        D_actor_403100_80155808->field_668.b.field_669 = 0;
        D_actor_403100_80155808->pad_670[3]            = 0;
        D_actor_403100_80155808->field_5FA            += 1;
    }
}
void func_actor_403100_80138DB0(Task* arg0)
{
    s32              sound;
    s32              pan;
    s32              depth;
    Task*            player;
    Actor403100Work* work;

    player                             = (Task*)Gp_ActorSlots[0];
    D_actor_403100_80155808->field_5EC = (u16)(D_actor_403100_80155808->field_5EC + 1);
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        if ((s16)D_actor_403100_80155808->field_5EC < 0x101) {
            func_actor_403100_8013C7B4(arg0);
            work = D_actor_403100_80155808;
            if ((u8)work->field_668.b.field_668 != 0) {
                work->pad_670[3]                    = 1;
                D_actor_403100_80155808->pad_670[1] = 1;
                Gp_StateC08.field_6                 = (u8)(Gp_StateC08.field_6 | 1);
                sound                               = (((u16)((GpEnemy*)player->spawnArg2)->field_8 >> 0xC) << 8) | 7;
                pan                                 = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)player->extra)->field_8[1]);
                depth                               = Gp_GetObjDepth((GpObj38*)&((TmdObject*)player->extra)->field_8[1]);
                SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
                D_actor_403100_80155808->field_65F  = 0;
                D_actor_403100_80155808->pad_660[0] = 1;
                func_actor_403100_8013D1B8(1, 0x3F4);
                D_actor_403100_80155808->field_5EC  = 0U;
                D_actor_403100_80155808->pad_65E[0] = 0;
                D_actor_403100_80155808->field_5FA += 1;
            } else if ((u8)work->field_668.b.field_669 != 0) {
                work->field_5E2 = -0x10;
                work->field_5FA = 0xA;
            }
            D_actor_403100_80155808->field_61C = 0;
        } else {
            D_actor_403100_80155808->field_61C = 2;
        }
        if (Actor403100_TestFlags104()) {
            D_actor_403100_80155808->field_5F8 = 1;
            D_actor_403100_80155808->field_5FA = 0U;
        }
    }
}
void func_actor_403100_80138F88(Task* arg0)
{
    s32              message[6];
    Actor403100Work* work;
    s32              sound;
    s32              y;
    s32              pan;
    s32              depth;
    GsCOORDINATE2*   coords;
    GsCOORDINATE2*   part;

    coords = ((TmdObject*)arg0->extra)->field_8;
    part   = coords + 6;
    func_actor_403100_8013C008(D_actor_403100_80155808->field_60E, D_actor_403100_80155808->field_610);
    D_actor_403100_80155808->field_60E = (u16)D_actor_403100_80155808->field_60E - 1;
    D_actor_403100_80155808->field_610 = (u16)D_actor_403100_80155808->field_610 + 6;
    y                                  = *(s32*)(u32)&coords->coord.t[1];
    *(s32*)(u32)&coords->coord.t[1]    = y + (-y >> 6);
    D_actor_403100_80155808->field_80  = 0;
    D_actor_403100_80155808->field_82  = 0xA00;
    D_actor_403100_80155808->field_84  = 0;
    func_actor_403100_80132528(arg0);
    if (Actor403100_TestFlags104()) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F000C;
        pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
        depth = Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
        SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
        D_8007216C                      = 0xB;
        *(s32*)(u32)&coords->coord.t[0] = -0x44C;
        work                            = D_actor_403100_80155808;
        SOFT_TOUCH_REG(work);
        *(s32*)(u32)&coords->coord.t[2] = 0x1770;
        *(s32*)(u32)&coords->coord.t[1] = 0;
        work->field_82                  = 0xC00;
        work->field_5E2                 = 0x10;
        work->field_5DE                 = 0xC;
        work->field_5DA                 = 2;
        work->field_604                 = 0xD0;
        work->field_608                 = -0x350;
        work->field_A0                  = -0x110;
        work->field_A2                  = 0x290;
        work->field_80                  = 0;
        work->field_84                  = 0;
        work->field_5EC                 = 0;
        work->field_A4                  = 0x60;
        work->field_5FA                += 1;
        part->coord.t[0]                = -0xBD0;
        work->field_604                 = 0x30;
        work->field_608                 = -0xD0;
        work->field_A0                  = -0x150;
        work->field_A2                  = 0x270;
        work->field_A4                  = -0xA0;
        part->coord.t[0]                = -0x1120;
        work->field_47C.field_1C        = 0x500;
        func_actor_403100_8013D74C(arg0);
        D_actor_403100_80155808->field_664.b.field_667 = 0;
        D_actor_403100_80155808->field_65F             = 0;
        D_actor_403100_80155808->pad_660[0]            = 0;
        D_actor_403100_80155808->field_5E8             = 0;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
        func_actor_403100_8013D1B8(1, 0x3FF);
        message[5] = 0x28;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)message, 0);
        D_actor_403100_80155808->field_656 = (u16)D_actor_403100_8015580C->field_40;
    }
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013922C);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_801395EC);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80139818);
void func_actor_403100_80139E80(Task* arg0)
{
    s16            angle;
    s32            y;
    s32            x;
    u16            velocityX;
    u16            rotationX;
    u16            rotationZ;
    u16            velocityZ;
    u16            rotationY;
    u16            frame;
    GsCOORDINATE2* part;
    GsCOORDINATE2* coords;

    coords                             = ((TmdObject*)arg0->extra)->field_8;
    D_actor_403100_80155808->field_60E = (u16)D_actor_403100_80155808->field_60E - 1;
    angle                              = (u16)D_actor_403100_80155808->field_610 + 6;
    D_actor_403100_80155808->field_610 = angle;
    func_actor_403100_8013C008(D_actor_403100_80155808->field_60E, angle);
    part                               = coords + 6;
    y                                  = coords->coord.t[1];
    coords->coord.t[1]                 = (s32)(y + ((s32)-y >> 6));
    velocityX                          = (u16)D_actor_403100_80155808->field_604;
    velocityZ                          = (u16)D_actor_403100_80155808->field_608;
    D_actor_403100_80155808->field_604 = velocityX + ((s32)(-0x1100 - (s16)(velocityX * 0x10)) >> 7);
    rotationX                          = (u16)D_actor_403100_80155808->field_A0;
    D_actor_403100_80155808->field_608 = velocityZ + ((s32)(0x4E00 - (s16)(velocityZ * 0x10)) >> 7);
    rotationY                          = (u16)D_actor_403100_80155808->field_A2;
    D_actor_403100_80155808->field_A0  = rotationX + ((s32)(0x2400 - (s16)(rotationX * 0x10)) >> 7);
    rotationZ                          = (u16)D_actor_403100_80155808->field_A4;
    D_actor_403100_80155808->field_A2  = rotationY + ((s32)(-0x1D00 - (s16)(rotationY * 0x10)) >> 7);
    D_actor_403100_80155808->field_A4  = rotationZ + ((s32)(0x2E00 - (s16)(rotationZ * 0x10)) >> 7);
    x                                  = part->coord.t[0];
    part->coord.t[0]                   = (s32)(x + ((s32)(-0x807 - x) >> 3));
    func_actor_403100_80132528(arg0);
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame >= 0x1F) {
        D_actor_403100_80155808->field_5E2              = 8;
        D_actor_403100_80155808->field_5DE              = 0xE;
        D_actor_403100_80155808->field_5DA              = 2;
        *(s16*)(u32)&D_actor_403100_80155808->field_5EC = 0;
        D_8007216C                                      = 0xC;
        *(s32*)(u32)&coords->coord.t[0]                 = -0x44C;
        *(s32*)(u32)&coords->coord.t[2]                 = 0x1770;
        *(s32*)(u32)&coords->coord.t[1]                 = 0;
        D_actor_403100_80155808->field_82               = 0xC00;
        D_actor_403100_80155808->field_80               = 0;
        D_actor_403100_80155808->field_84               = 0;
        D_actor_403100_80155808->field_604              = 0;
        D_actor_403100_80155808->field_608              = 0;
        D_actor_403100_80155808->field_A0               = 0;
        D_actor_403100_80155808->field_A2               = 0;
        D_actor_403100_80155808->field_A4               = 0;
        *(s32*)(u32)&part->coord.t[0]                   = -0x877;
        D_actor_403100_80155808->field_5FA             += 1;
    }
}
void func_actor_403100_8013A064(Task* arg0)
{
    GameActor* actor;
    Task*      task;
    s16        state;
    s16        message;
    s32        sound;
    s32        pan;
    u16        frame;
    s32        depth;

    actor                              = (*Gp_ActorSlots)->actor;
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0x3C) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F000F;
        pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[8]);
        depth = Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[8]);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)(depth / 2));
    }
    D_actor_403100_80155808->field_80 = 0;
    D_actor_403100_80155808->field_82 = 0xC00;
    D_actor_403100_80155808->field_84 = 0;
    func_actor_403100_80132528(arg0);
    if ((s16)D_actor_403100_80155808->field_5EC >= 0x44) {
        D_actor_403100_80155808->field_5FC  = 0x14;
        D_actor_403100_80155808->field_5E2  = 0x10;
        D_actor_403100_80155808->field_5DE  = 5;
        D_actor_403100_80155808->field_5DA  = 1;
        Mc_SaveData.field_4                 = 0x17;
        D_actor_403100_80155808->field_5FA += 1;
        func_actor_403100_8013D0B8(-0x1BBC, -0xC80, -0x4B0, 0x400);
        task = Game_GetPtrSlot(3);
        if (Gp_DispatchMsg(task, 0x3F9, Gp_PackPair(&D_actor_403100_80147614, 5), 0) != 0) {
            Game_Session->field_127 = 1;
            state                   = 7;
            if (D_actor_403100_8015580C->field_40 <= 0) {
                D_actor_403100_8015580C->field_40 = 0x3E8;
            }
            message                             = 0x3FF;
            D_actor_403100_80155808->pad_670[0] = 1;
            actor->field_956                    = 0xA;
        } else {
            state   = 2;
            message = 0x3F4;
        }
        func_actor_403100_8013D1B8(state, message);
        D_actor_403100_80155808->field_5EC = 0;
    }
}
void func_actor_403100_8013A254(void)
{
    GpActorWork*     actor;
    Actor403100Work* work;
    s32              sound;
    s32              sound2;
    s32              pan;
    s32              pan2;
    u16              frame;
    s32              depth;
    s32              depth2;

    actor                              = *Gp_ActorSlots;
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0xB) {
        if (D_actor_403100_80155808->regions.fields.field_63E == 0) {
            func_8017E250(1, 1);
            D_actor_403100_80155808->regions.fields.field_63E = 1;
        } else {
            func_8017E250(1, 2);
        }
        Gp_SpawnPadLerp(8, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 8;
        sound                              = (((u16)((GpEnemy*)((Task*)actor)->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0006;
        pan                                = (s8)Gp_GetObjPan((GpObj38*)(actor->extra->field_8 + 1));
        depth                              = Gp_GetObjDepth((GpObj38*)(actor->extra->field_8 + 1));
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x1C) {
        sound2 = (((u16)((GpEnemy*)((Task*)actor)->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0007;
        pan2   = (s8)Gp_GetObjPan((GpObj38*)(actor->extra->field_8 + 1));
        depth2 = Gp_GetObjDepth((GpObj38*)(actor->extra->field_8 + 1));
        SndEvt_EnqueueType6(sound2, (s32)pan2, (s8)(depth2 / 2));
    }
    D_actor_403100_80155808->field_65F = 0;
    if ((u8)D_actor_403100_80155808->pad_670[0] == 0) {
        if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
            if (D_actor_403100_8015580C->field_40 > 0) {
                D_8007216C = 6;
            }
            D_actor_403100_80155808->field_632             = 0;
            D_actor_403100_80155808->field_668.b.field_668 = 0;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
            work                                = D_actor_403100_80155808;
            work->pad_670[3]                    = 0;
            work->field_47C.field_1C            = 0x400;
            D_actor_403100_80155808->pad_670[1] = 0;
            D_actor_403100_80155808->field_5F8  = 1;
            D_actor_403100_80155808->field_5FA  = 0;
        }
    } else if ((s16)D_actor_403100_80155808->field_5EC >= 0x1E) {
        Game_Session->field_127 = 0;
    }
}
void func_actor_403100_8013A4C8(Task* arg0)
{
    Actor403100Entry* entries;
    GpObj*            obj;
    s32               i;
    Actor403100Work*  work;

    i                                          = 0;
    entries                                    = D_actor_403100_80155814;
    obj                                        = &entries->obj;
    ((TmdObject*)arg0->extra)->field_E         = 0x1F;
    work                                       = *(Actor403100Work* volatile*)&D_actor_403100_80155808;
    (*(volatile s16*)&D_actor_403100_80155810) = 0;
    work->field_62C                            = 0x30;
    work->field_5F6                            = 0;
    work->flags_634.h.high                     = 0x1C;
    for (; i < 0x1C; i++) {
        if (entries[i].active != 0) {
            entries[i].active = 0;
            Gp_UnlinkObj(obj);
        }
        obj = (GpObj*)((u8*)obj + sizeof(Actor403100Entry));
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    D_actor_403100_80155808->field_5E2  = 0x10;
    D_actor_403100_80155808->field_5DE  = 7;
    D_actor_403100_80155808->field_5DA  = 2;
    D_actor_403100_80155808->field_5EC  = 0;
    D_actor_403100_80155808->field_5FA += 1;
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013A5AC);
void func_actor_403100_8013A81C(Task* arg0)
{
    SVECTOR  offset;
    s32      sound;
    s32      sound2;
    s32      pan;
    GpObj38* effectCoord;
    s32      pan2;
    u16      frame;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0x31) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0002;
        pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
        SndEvt_EnqueueType6(sound, pan, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]) / 2));
        sound2 = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0001;
        pan2   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]);
        SndEvt_EnqueueType6(sound2, pan2, (s8)(Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[1]) / 2));
        D_actor_403100_80155808->field_664.b.field_666 = 1;
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x12;
    }
    if ((u32)(D_actor_403100_80155808->field_5EC - 0x32) < 0xBU) {
        effectCoord = (GpObj38*)&((TmdObject*)arg0->extra)->field_8[3];
        offset.vy   = -0x140;
        offset.vx   = 0;
        offset.vz   = 0x400;
        Gp_SpawnEff(0x60070, effectCoord, -0x3FFCB400, &offset);
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
void func_actor_403100_8013AA04(Task* arg0)
{
    s32 sound;
    s32 pan;
    u16 angle;
    u16 frame;
    s32 depth;

    angle                              = (u16)D_actor_403100_80155808->field_82;
    frame                              = D_actor_403100_80155808->field_5EC;
    D_actor_403100_80155808->field_82  = angle + ((s32)((-0x4000 - (angle * 0x10)) << 0x10) >> 0x16);
    D_actor_403100_80155808->field_5EC = frame + 1;
    if ((u32)((frame - 0x33) & 0xFFFF) < 0x16U) {
        func_actor_403100_8013C7B4(arg0);
    } else if ((func_actor_403100_80133928() << 0x10) != 0) {
        return;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x44) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0008;
        pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[7]);
        depth = Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[7]);
        SndEvt_EnqueueType6(sound, (s32)pan, (s8)(depth / 2));
    }
    if (D_actor_403100_80155808->field_668.flags != 0) {
        D_actor_403100_80155808->pad_670[3] = 1;
        D_actor_403100_80155808->pad_670[1] = 1;
        Gp_StateC08.field_6                |= 1;
        func_actor_403100_8013D1B8(3, 0x3F4);
        func_actor_403100_8013D0B8(D_actor_403100_80155808->field_90, D_actor_403100_80155808->field_92, (s16)((u16)D_actor_403100_80155808->field_94 + 0xBB8), 0x800);
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        D_actor_403100_80155808->field_5EC             = 0;
        Game_Session->field_12C                        = 1;
        D_actor_403100_80155808->field_5FA            += 1;
        return;
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        D_actor_403100_80155808->field_5F8             = 1;
        D_actor_403100_80155808->field_5FA             = 0;
    }
}
void func_actor_403100_8013AC04(void)
{
    s32              state;
    s32              message;
    s32              sound;
    Task*            task;
    s32              finished;
    s32              pan;
    s32              depth;
    Task*            player;
    GameActor*       actor;
    u8               completed;
    Actor403100Work* work;

    player   = (Task*)*Gp_ActorSlots;
    actor    = (GameActor*)player->idMap;
    finished = 0;
    if ((s16)D_actor_403100_80155808->field_5EC == 0) {
        Gp_SpawnPadLerp(0xC, 0xFF, 0x80);
        sound = (((u16)((GpEnemy*)player->spawnArg2)->field_8 >> 0xC) << 8) | 7;
        pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)player->extra)->field_8[1]);
        depth = Gp_GetObjDepth((GpObj38*)&((TmdObject*)player->extra)->field_8[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
    }
    work = D_actor_403100_80155808;
    if ((s16)work->field_5EC < 5) {
        func_actor_403100_8013D0B8(work->field_90, work->field_92, (s16)(work->field_94 + 0x3E8), 0x800);
    }
    if (((s16)D_actor_403100_80155808->field_5EC >= 6) || (D_actor_403100_80155808->field_94 >= 0x1B58)) {
        finished = 1;
    }
    D_actor_403100_80155808->field_5EC = (s16)((u16)D_actor_403100_80155808->field_5EC + 1);
    if ((completed = finished != 0)) {
        D_8007216C = 0x14;
        task       = Game_GetPtrSlot(3);
        if (Gp_DispatchMsg(task, 0x3F9, Gp_PackPair(&D_actor_403100_80147614, 2), 0) != 0) {
            Game_Session->field_127             = 1;
            Game_Session->field_12D             = 0x7F;
            D_actor_403100_80155808->pad_670[0] = 1U;
        }
        func_actor_403100_8013D0B8(-0x1928, -0xC7C, 0x29D6, 0x800);
        if ((u8)D_actor_403100_80155808->pad_670[0] != 0) {
            actor->field_956 = 0xA;
            state            = 7;
            message          = 0x3FF;
        } else {
            state   = 2;
            message = 0x3F4;
        }
        func_actor_403100_8013D1B8(state, message);
        D_actor_403100_80155808->field_5EC = 0;
        D_actor_403100_80155808->field_5FA = (u16)(D_actor_403100_80155808->field_5FA + 1);
    }
}
void func_actor_403100_8013AE28(void)
{
    Task* player;
    s32   sound;
    s32   sound2;
    s32   sound3;
    s32   pan;
    s32   pan2;
    s32   pan3;
    s32   depth;
    u16   frame;

    player                             = (Task*)*Gp_ActorSlots;
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0xA) {
        if (D_actor_403100_80155808->regions.fields.field_63C == 0) {
            func_8017E250(0, 1);
            D_actor_403100_80155808->regions.fields.field_63C = 1;
        } else {
            func_8017E250(0, 2);
        }
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0xB) {
        sound = (((u16)((GpEnemy*)player->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0006;
        pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)player->extra)->field_8[1]);
        depth = Gp_GetObjDepth((GpObj38*)&((TmdObject*)player->extra)->field_8[1]);
        SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->field_5FE = 0x12;
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x1C) {
        sound2 = (((u16)((GpEnemy*)player->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0007;
        pan2   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)player->extra)->field_8[1]);
        depth  = Gp_GetObjDepth((GpObj38*)&((TmdObject*)player->extra)->field_8[1]);
        SndEvt_EnqueueType6(sound2, pan2, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x20) {
        if ((u8)D_actor_403100_80155808->pad_670[0] != 0) {
            sound3 = (((u16)((GpEnemy*)player->spawnArg2)->field_8 >> 0xC) << 8) | 0x531D000B;
            pan3   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)player->extra)->field_8[1]);
            depth  = Gp_GetObjDepth((GpObj38*)&((TmdObject*)player->extra)->field_8[1]);
            SndEvt_EnqueueType6(sound3, pan3, (s8)(depth / 2));
        }
    }
    if ((u8)D_actor_403100_80155808->pad_670[0] == 0) {
        if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
            D_actor_403100_80155808->field_668.b.field_668 = 0;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
            if (D_actor_403100_8015580C->field_40 > 0) {
                D_8007216C = 4;
            }
            Game_Session->field_12C             = 0;
            D_actor_403100_80155808->pad_670[3] = 0;
            D_actor_403100_80155808->pad_670[1] = 0;
            D_actor_403100_80155808->field_5F8  = 1;
            D_actor_403100_80155808->field_5FA  = 0;
        }
    } else if ((s16)D_actor_403100_80155808->field_5EC >= 0x32) {
        Game_Session->field_127 = 0;
    }
}
void func_actor_403100_8013B128(Task* arg0)
{
    Actor403100Entry* entries;
    Actor403100Work*  work;
    Actor403100Work*  finalWork;
    GpObj*            obj;
    s32               sound;
    s32               i;
    s32               pan;
    s32               depth;
    GsCOORDINATE2*    coords;
    TmdObject*        model;

    model                             = arg0->extra;
    coords                            = model->field_8;
    Game_Session->field_12C           = 0;
    model->field_E                    = 0;
    D_actor_403100_8015580C->field_4C = 0;
    Gp_SetLightMode(arg0->spawnArg2, 0);
    D_actor_403100_8015580C->node.field_4 = 9;
    i                                     = 0;
    if (D_actor_403100_80155808->field_5D0 < 0) {
        arg0->state                        = 4;
        D_actor_403100_80155808->field_5F8 = 0;
        D_actor_403100_80155808->field_5FA = 0U;
        return;
    }
    Gp_UnlinkNode(&D_actor_403100_8015580C->node);
    entries                 = D_actor_403100_80155814;
    obj                     = &entries->obj;
    D_actor_403100_80155810 = 0;
    for (; i < 0x1C; i++) {
        if (entries[i].active != 0) {
            entries[i].active = 0;
            Gp_UnlinkObj(obj);
        }
        obj = (GpObj*)((u8*)obj + sizeof(Actor403100Entry));
    }
    SndEvt_EnqueueType7(0x401F0004, 1);
    D_actor_403100_80155808->field_622              = (s16)Game_Session->field_4;
    D_actor_403100_80155808->field_0.matrices.coord = coords->coord;
    D_actor_403100_80155808->savedRotation          = *(SVECTOR*)&D_actor_403100_80155808->field_80;
    Mc_SaveData.field_4                             = 0x18;
    work                                            = D_actor_403100_80155808;
    work->pad_660[1]                                = 1;
    work->field_5F6                                 = 5;
    work->field_5E2                                 = 0x10;
    work->field_5DE                                 = 0x10;
    work->field_5DA                                 = 2;
    work->field_61C                                 = 2;
    D_actor_403100_80155808->field_604              = 0;
    D_actor_403100_80155808->field_608              = 0;
    D_actor_403100_80155808->field_A0               = 0;
    D_actor_403100_80155808->field_A2               = 0;
    D_actor_403100_80155808->field_A4               = 0;
    coords->coord.t[0]                              = -0x44C;
    coords->coord.t[1]                              = -0x1388;
    coords->coord.t[2]                              = 0x2710;
    D_actor_403100_80155808->field_80               = 0;
    D_actor_403100_80155808->field_82               = 0xA00;
    D_actor_403100_80155808->field_84               = 0;
    Game_Session->field_68                          = 1;
    Gp_MsgPlayerWeapon(0);
    sound = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F000B;
    pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
    depth = Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
    SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
    finalWork             = D_actor_403100_80155808;
    finalWork->field_5EC  = 0;
    finalWork->pad_670[3] = 1;
    finalWork->field_5FA  = (u16)(finalWork->field_5FA + 1);
}
void func_actor_403100_8013B3C4(Task* arg0)
{
    s32            sound;
    s32            pan;
    s32            depth;
    GsCOORDINATE2* coords;

    coords                              = ((TmdObject*)arg0->extra)->field_8;
    D_actor_403100_80155808->field_5EC += 1;
    Mc_SaveData.field_4                 = 0x18;
    if ((s16)D_actor_403100_80155808->field_5EC == 0x64) {
        Gp_LoadImages(&D_actor_403100_801555EC);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x10E) {
        sound = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0005;
        pan   = (s8)Gp_GetObjPan((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
        depth = Gp_GetObjDepth((GpObj38*)&((TmdObject*)arg0->extra)->field_8[4]);
        SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
    }
    coords->coord.t[0]                = -0x44C;
    coords->coord.t[1]                = -0x1388;
    coords->coord.t[2]                = 0x2710;
    D_actor_403100_80155808->field_80 = 0;
    D_actor_403100_80155808->field_82 = 0xA00;
    D_actor_403100_80155808->field_84 = 0;
    if (Actor403100_TestFlags()) {
        D_actor_403100_80155808->pad_670[3] = 0;
        Gp_LinkNode(&D_actor_403100_8015580C->node);
        D_actor_403100_8015580C->node.field_4 = 8;
        Game_Session->field_68                = 0;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 2, 0);
        coords->coord                                 = D_actor_403100_80155808->field_0.matrices.coord;
        *(SVECTOR*)&D_actor_403100_80155808->field_80 = D_actor_403100_80155808->savedRotation;
        Mc_SaveData.field_4                           = (u8)D_actor_403100_80155808->field_622;
        D_actor_403100_80155808->field_5F8            = 1;
        D_actor_403100_80155808->field_5FA            = 0;
    }
}
extern MATRIX Gfx_ViewWorldMtx;

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013B5E0);
INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80131F84);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80131F9C);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80131FB0);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80131FD4);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80132000);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80132014);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80132024);

extern TaskFuncTable6 D_actor_403100_80132030;

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80132030);

void func_actor_403100_8013BA64(Task* arg0)
{
    GsCOORDINATE2* coords   = ((TmdObject*)arg0->extra)->field_8;
    TaskFuncTable6 handlers = D_actor_403100_80132030;

    D_actor_403100_80155808->field_602 = (u16)D_actor_403100_80155808->field_600;
    handlers.funcs[D_actor_403100_80155808->field_5F6](arg0);
    if (((Gp_GetViewIndex() & 0xFF) == 7) || ((Gp_GetViewIndex() & 0xFF) == 8)) {
        if ((s16)D_actor_403100_80155808->field_5F8 == 6) {
            if (D_actor_403100_80155808->field_628 == 2) {
                coords->coord.t[0] += (-4000 - coords->coord.t[0]) >> 4;
            } else {
                coords->coord.t[0] += (-2700 - coords->coord.t[0]) >> 4;
            }
        }
    } else {
        coords->coord.t[0] += (-1100 - coords->coord.t[0]) >> 3;
    }
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013BB8C);
void func_actor_403100_8013BDE4(Task* arg0)
{
    s16            angle;
    s32            sound;
    s32            pan;
    s32            depth;
    GsCOORDINATE2* coords;

    coords = ((TmdObject*)arg0->extra)->field_8;
    if (D_actor_403100_80155808->field_600 != 0) {
        angle                              = ((u16)D_actor_403100_80155808->field_600 + 0x20) & 0x7FF;
        D_actor_403100_80155808->field_600 = angle;
        if (angle == 0) {
            Gp_SpawnPadLerp(0x1E, 0xFF, 8);
            D_actor_403100_80155808->field_5FE = 0x1E;
            sound                              = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0001;
            pan                                = (s8)Gp_GetObjPan((GpObj38*)(((TmdObject*)arg0->extra)->field_8 + 1));
            depth                              = Gp_GetObjDepth((GpObj38*)(((TmdObject*)arg0->extra)->field_8 + 1));
            SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
        }
    } else {
        D_actor_403100_80155808->field_5F6 = 0;
    }
    coords->coord.t[1] = -((rsin(D_actor_403100_80155808->field_600) << 13) >> 16);
}
void func_actor_403100_8013BEF0(Task* arg0)
{
    s16            angle;
    s32            sound;
    s32            pan;
    s32            depth;
    GsCOORDINATE2* coords;

    coords = ((TmdObject*)arg0->extra)->field_8;
    if (D_actor_403100_80155808->field_600 != 0) {
        angle                              = ((u16)D_actor_403100_80155808->field_600 + 0x20) & 0x7FF;
        D_actor_403100_80155808->field_600 = angle;
        if (angle == 0) {
            Gp_SpawnPadLerp(0x1E, 0xFF, 8);
            D_actor_403100_80155808->field_5FE = 0x1E;
            sound                              = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x401F0001;
            pan                                = (s8)Gp_GetObjPan((GpObj38*)(((TmdObject*)arg0->extra)->field_8 + 1));
            depth                              = Gp_GetObjDepth((GpObj38*)(((TmdObject*)arg0->extra)->field_8 + 1));
            SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
        }
    } else {
        D_actor_403100_80155808->field_5F6++;
    }
    coords->coord.t[1] = -((rsin(D_actor_403100_80155808->field_600) << 13) >> 16);
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013C008);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013C214);
static inline s32 Actor403100CoordToViewInline(GsCOORDINATE2* coord, SVECTOR* pos, GsCOORDINATE2* view)
{
    SVECTOR        local;
    VECTOR         result;
    s32            flag;
    GsCOORDINATE2* current;

    current  = coord;
    local.vx = pos->vx;
    local.vy = pos->vy;
    local.vz = pos->vz;
    while (1) {
        if (current->sub == NULL) {
            return 0;
        }
        if (current == view) {
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            return 1;
        }
        gte_SetTransMatrix(&current->coord);
        gte_SetRotMatrix(&current->coord);
        gte_ldv0(&local);
        __asm__ volatile("nop; nop; .word 0x4A480012");
        gte_stlvnl(&result);
        gte_stflg(&flag);
        local.vx = result.vx;
        local.vy = result.vy;
        local.vz = result.vz;
        current  = current->sub;
    }
}

static inline void Actor403100ResetStateInline(s16 anim, s16 angle, s16 frame)
{
    D_actor_403100_80155808->field_5FC = frame;
    D_actor_403100_80155808->field_5E2 = angle;
    D_actor_403100_80155808->field_5DE = anim;
    D_actor_403100_80155808->field_5DA = 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013C7B4);
void func_actor_403100_8013CBE0(Task* arg0)
{
    s16 next;
    s16 next2;
    s32 sound;
    s32 soundId;
    s32 pan;
    u32 random;
    s32 depth;
    u8  request;
    u8  state;

    state = D_actor_403100_80155808->field_664.b.field_665;
    switch (state) {
        case 0:
            D_actor_403100_80155808->field_5E8 = (s16)((u16)D_actor_403100_80155808->field_5E8 + ((s32) - (D_actor_403100_80155808->field_5E8 * 0x10) >> 7));
            return;
        case 1:
            request = D_actor_403100_80155808->field_66F;
            if (request == state) {
                soundId = 0x401F0009;
                goto play_sound;
            }
            if (request == 2) {
                random      = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState = random;
                if ((random >> 16) & 1) {
                    soundId = 0x401F0000;
                    TOUCH_REG(soundId);
                    soundId |= 2;
                } else {
                    soundId = 0x401F0000;
                    TOUCH_REG(soundId);
                    soundId |= 5;
                }
            play_sound:
                sound = (((u16)((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | soundId;
                pan   = (s8)((s32 (*)(GpObj38*, s32))Gp_GetObjPan)((GpObj38*)(((TmdObject*)arg0->extra)->field_8 + 4), soundId);
                depth = Gp_GetObjDepth((GpObj38*)(((TmdObject*)arg0->extra)->field_8 + 4));
                SndEvt_EnqueueType6(sound, pan, (s8)(depth / 2));
            }
            D_actor_403100_80155808->field_66F             = 0U;
            D_actor_403100_80155808->field_664.b.field_665 = (u8)(D_actor_403100_80155808->field_664.b.field_665 + 1);
            return;
        case 2:
            next                               = (u16)D_actor_403100_80155808->field_5E8 + ((s32)(-0x2200 - (D_actor_403100_80155808->field_5E8 * 0x10)) >> 7);
            D_actor_403100_80155808->field_5E8 = next;
            if (next < -0x1FF) {
                D_actor_403100_80155808->field_664.b.field_665 = (u8)(D_actor_403100_80155808->field_664.b.field_665 + 1);
                return;
            }
            return;
        case 3:
            next2                              = (u16)D_actor_403100_80155808->field_5E8 + 0xC;
            D_actor_403100_80155808->field_5E8 = next2;
            if ((next2 << 16) >= 0) {
                D_actor_403100_80155808->field_664.b.field_665 = 0U;
            }
            break;
    }
}
void func_actor_403100_8013CDC0(void)
{
    s16 next;
    s16 next2;
    u8  state;

    state = D_actor_403100_80155808->field_664.b.field_666;
    switch (state) { /* irregular */
        case 0:
            D_actor_403100_80155808->field_5EA =
                (u16)D_actor_403100_80155808->field_5EA +
                ((s32) - (D_actor_403100_80155808->field_5EA * 0x10) >> 7);
            return;
        case 1:
            D_actor_403100_80155808->field_664.b.field_666 = 2;
            return;
        case 2:
            next = (u16)D_actor_403100_80155808->field_5EA +
                   ((s32)(0x1E00 - (D_actor_403100_80155808->field_5EA * 0x10)) >> 7);
            D_actor_403100_80155808->field_5EA = next;
            if (next >= 0x1C0) {
                D_actor_403100_80155808->field_664.b.field_666 =
                    D_actor_403100_80155808->field_664.b.field_666 + 1;
                return;
            }
            return;
        case 3:
            next2                              = (u16)D_actor_403100_80155808->field_5EA - 0xC;
            D_actor_403100_80155808->field_5EA = next2;
            if ((next2 << 0x10) <= 0) {
                D_actor_403100_80155808->field_664.b.field_666 = 0;
            }
            break;
    }
}

void func_actor_403100_8013CEAC(u16* arg0, s32 arg1, s32 arg2, s16 arg3)
{
    s16 facing;
    s16 target;
    s16 angle;
    u16 targetU;
    u16 facingU;

    angle = *arg0 - 0x140;
    *arg0 = angle;
    if ((angle < (s16)arg2) && (arg3 < (s16)angle)) {
        target  = D_actor_403100_80155808->field_B0;
        targetU = (u16)D_actor_403100_80155808->field_B0;
        if ((u32)(((s16)angle - target) + 0x20) >= 0x41U) {
            if (target < (s16)angle) {
                D_actor_403100_80155808->field_B0 = (s16)(targetU + arg1);
                return;
            }
            D_actor_403100_80155808->field_B0 = (s16)(targetU - arg1);
        }
    } else {
        facing  = D_actor_403100_80155808->field_B0;
        facingU = (u16)D_actor_403100_80155808->field_B0;
        if (facing >= 0x21) {
            D_actor_403100_80155808->field_B0 = (s16)(facingU - 0x18);
            return;
        }
        if (facing < -0x20) {
            D_actor_403100_80155808->field_B0 = (s16)(facingU + 0x18);
        }
    }
}
void func_actor_403100_8013CF60(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    s16 facing;
    s16 target;
    u16 yaw;
    u16 targetU;
    u16 facingU;

    yaw = (u16)arg0->vy;
    if ((u32)((yaw + 0x27F) & 0xFFFF) < 0x4FFU) {
        target  = D_actor_403100_80155808->field_B2;
        targetU = (u16)D_actor_403100_80155808->field_B2;
        if ((u32)(((s16)yaw - target) + 0x20) >= 0x41U) {
            if (target < (s16)yaw) {
                D_actor_403100_80155808->field_B2 = targetU + arg1;
                D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 + arg2;
                return;
            }
            D_actor_403100_80155808->field_B2 = targetU - arg1;
            D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 - arg2;
            return;
        }
        facing  = (s16)D_actor_403100_80155808->field_82;
        facingU = D_actor_403100_80155808->field_82;
        if (facing < target) {
            D_actor_403100_80155808->field_82 = facingU + arg3;
            return;
        }
        if (target < facing) {
            D_actor_403100_80155808->field_82 = facingU - arg3;
        }
    } else {
        if ((s16)yaw >= 0x281) {
            D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 + arg4;
        }
        if (arg0->vy < -0x280) {
            D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 - arg4;
        }
    }
}

void func_actor_403100_8013D06C(void)
{
    if (D_actor_403100_80155808->field_B4 >= 0x11) {
        D_actor_403100_80155808->field_B4 = (u16)D_actor_403100_80155808->field_B4 - 8;
    }
    if (D_actor_403100_80155808->field_B4 < -0x10) {
        D_actor_403100_80155808->field_B4 = (u16)D_actor_403100_80155808->field_B4 + 8;
    }
}

void func_actor_403100_8013D0B8(s16 arg0, s16 arg1, s16 arg2, s16 arg3)
{
    Actor403100MsgPos msg;

    msg.pos.vx = arg0;
    msg.pos.vy = arg1;
    msg.pos.vz = arg2;
    msg.rot.vx = 0;
    msg.rot.vy = arg3;
    msg.rot.vz = 0;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E9, (s32)&msg, 0);
}
void func_actor_403100_8013D11C(Task* arg0)
{
    GsCOORDINATE2* coords;
    GpObj44*       light;
    s16            value;
    u32            random;

    coords                       = ((TmdObject*)arg0->extra)->field_8;
    D_80114FF8.field_0           = 8;
    light                        = &D_80114FF8.field_4.light;
    light->field_58              = 0x300;
    random                       = Gp_LcgState * 5 + 0x71357911;
    light->field_5C              = 0x3000;
    value                        = ((random >> 16) & 0x700) + 0x800;
    light->field_50              = value;
    light->field_52              = value >> 3;
    light->field_54              = value >> 4;
    coords                      += 3;
    light->field_18.vx           = coords->coord.t[0];
    light->field_18.vy           = coords->coord.t[1];
    light->field_18.vz           = coords->coord.t[2];
    Gp_LcgState                  = random;
    D_80114FF8.field_4.coord.flg = 0;
}
void func_actor_403100_8013D1B8(s16 arg0, s16 arg1)
{
    GpAnimArg msg;

    msg.field_0                        = &D_actor_403100_8015570C;
    msg.field_4                        = (s32)arg0;
    msg.field_8                        = 0;
    msg.field_C                        = 0;
    msg.field_10                       = 0;
    D_actor_403100_80155808->field_65D = (s8)arg0;
    if (arg1 == 0x3FF) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&msg, 0);
    } else if (arg1 == 0x3F4) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F4, (s32)&msg, 0);
    }
}
void func_actor_403100_8013D24C(void)
{
    s32 state;

    state = (s8)D_actor_403100_80155808->pad_66A[2];
    if ((state == 1) && !(D_actor_403100_80155808->field_664.word & 0xFFFF00)) {
        D_actor_403100_80155808->field_664.b.field_665 = state;
        D_actor_403100_80155808->field_664.b.field_666 = (u8)state;
        D_actor_403100_80155808->field_66F             = state;
    }
}
void func_actor_403100_8013D2A0(s16 arg0)
{
    if (!(D_actor_403100_80155808->field_664.word & 0xFFFF00)) {
        if (arg0 == 1) {
            D_actor_403100_80155808->field_66F = 2;
        }
        D_actor_403100_80155808->field_664.b.field_665 = 1;
        D_actor_403100_80155808->field_664.b.field_666 = 1;
    }
}

s32 func_actor_403100_8013D2F4(GsCOORDINATE2* coord, MATRIX* matrix)
{
    MATRIX         result;
    MATRIX         parent;
    GsCOORDINATE2* current;

    current = coord->sub;
    *matrix = coord->coord;
    while (1) {
        if (current == NULL) {
            return 0;
        }
        if (current == &Gfx_ViewCoord) {
            return 1;
        }
        parent = current->coord;
        MatrixNormal(&parent, &parent);
        gte_SetRotMatrix(&parent);
        MulRotMatrix(matrix);
        MatrixNormal(matrix, &result);
        *matrix = result;
        current = current->sub;
    }
}