#include "common.h"

#include "actors/actor_356100.h"
#include "gameplay/1A8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include <psyq/inline_c.h>

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80162258);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801625A0);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80162AEC);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80162C90);

void func_actor_356100_801633DC(Actor356100* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor356100AnimWork* work;

    work   = (Actor356100AnimWork*)arg0->field_1C;
    weight = work->field_98C;
    anim   = &work->anim;
    for (i = 1; i < 0x15; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_98A;
            work->slots[i].field_9      = (u8)(work->field_982 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_982 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80163508);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_8016382C);

/// Runs the clip the work block's `field_978` halfword selects and holds this
/// state until it ends: while the actor is live, reset the model (`node.field_4`
/// / `obj->field_C`, `Tmd_AllocBuffers`), start clip 2 at speed 0x10, and tick
/// until clip 0xB has reached frame 6 or clip 0xC frame 9, then park `field_982`
/// at 0x20. Once the actor is no longer live the same slot is halved per frame as
/// a scale ramp that bounces between 0x10 and -0x10 — ending the state with
/// `field_0 = 0x11` when `Gp_TickObjFlag2` reports the flag has expired.
void func_actor_356100_80163CD4(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    s16              animA;
    s16              animB;
    s32              value;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        animA             = 0xB;
        animB             = 0xC;
        obj               = arg0->field_2C;
        ctx->node.field_4 = 0;
        obj->field_C      = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_982 = 0x10;
        do {
            func_actor_356100_80163508(arg0);
        } while (((work->field_97E != animA) || ((u32)(work->field_5A & 0x3FF) < 6U)) &&
                 ((work->field_97E != animB) || ((u32)(work->field_5A & 0x3FF) < 9U)));
        work->field_982 = 0x20;
        return;
    }
    arg0->field_2C->field_8->flg = 0;
    value                        = (s16)work->field_982 / 2;
    work->field_982              = (u16)value;
    if (value == 1) {
        work->field_982 = -0x10U;
    }
    if ((s16)work->field_982 == -1) {
        work->field_982 = 0x10;
    }
    func_actor_356100_80163508(arg0);
    if (Gp_TickObjFlag2((GpObj5D*)ctx) == 1) {
        ctx->field_4C &= 0xFD;
        work->field_0  = 0x11;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80163E2C);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80164158);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80164ACC);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801653F4);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80165B30);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80166018);

void func_actor_356100_801666B4(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;
    Task*            player;
    SVECTOR*         vecp;
    SVECTOR          vec;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        player                                    = Game_GetPtrSlot(3);
        work->field_9BC                           = 0x180;
        enemy->node.field_4                       = 0;
        work->field_978                           = 1;
        work->field_982                           = 0x10;
        work->field_97E                           = 5;
        ((TmdObject*)player->extra)->field_8->flg = 0;
        Gp_UpdateCoord(((TmdObject*)player->extra)->field_8);
        D_actor_356100_801732B0.x = ((TmdObject*)player->extra)->field_8->coord.t[0];
        D_actor_356100_801732B0.y = ((TmdObject*)player->extra)->field_8->coord.t[1];
        D_actor_356100_801732B0.z = ((TmdObject*)player->extra)->field_8->coord.t[2];
        vecp                      = &vec;
        /* Order matters: the vy store must follow the vx loads in RTL, or
           sched1 fills its anti-dependency chain from the earlier stores and
           hoists it above the D.z store. */
        vec.vx = ((GpCoordXZ*)arg0->field_2C->field_8)->field_18 - ((GpCoordXZ*)((TmdObject*)player->extra)->field_8)->field_18;
        vec.vy = 0;
        vec.vz = ((GpCoordXZ*)arg0->field_2C->field_8)->field_20 - ((GpCoordXZ*)((TmdObject*)player->extra)->field_8)->field_20;
        VectorNormalSS(vecp, vecp);
        gte_lddp(0x3E8);
        gte_ldsv(vecp);
        gte_gpf12_real();
        gte_stsv(vecp);
        arg0->field_2C->field_8->coord.t[0] = ((TmdObject*)player->extra)->field_8->coord.t[0] + vec.vx;
        arg0->field_2C->field_8->coord.t[2] = ((TmdObject*)player->extra)->field_8->coord.t[2] + vec.vz;
        arg0->field_2C->field_8->flg        = 0;
        D_actor_356100_801732B0.field_10    = 0;
        D_actor_356100_801732B0.field_12    = ratan2(vec.vx, vec.vz);
        D_actor_356100_801732B0.field_14    = 0;
        Gp_DispatchMsg(player, 0x3E9, (s32)&D_actor_356100_801732B0, 0);
    }
    func_actor_356100_80163508(arg0);
    if (work->field_97E == 5 && (work->field_68 & 1)) {
        work->field_0 = 0xD;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801668FC);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80166CF0);

INCLUDE_RODATA("actors/nonmatchings/actor_356100/actor_356100", D_actor_356100_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80167358);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80167584);

void func_actor_356100_80167818(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    s32              sound;
    s32              pan;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj                     = arg0->field_2C;
        D_actor_356100_801731B0 = 0;
        work->field_97E         = 0x10;
        work->field_978         = 2;
        obj->field_C            = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC     = 0x180;
        enemy->node.field_4 = 0;
        work->field_990     = 0;
        work->field_982     = 0x10;
        work->field_98E     = 0;
        work->field_6       = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->field_8 >> 0xC) << 8) | 0x51030008;
        pan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
        work->field_6 = 1;
    }
    func_actor_356100_80163508(arg0);
    if ((work->field_5A & 0x3FF) == 4 && work->field_994 != (work->field_5A & 0x3FF)) {
        D_actor_356100_801732A8.field_0 = arg0->field_2C->field_8;
        D_actor_356100_801732A8.field_4 = 0x100;
        D_actor_356100_801732A8.field_6 = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->field_2C->field_8 + 5, NULL,
                      &D_actor_356100_801732A8);
    }
    work->field_994 = work->field_5A & 0x3FF;
    coord           = arg0->field_2C->field_8;
    d               = &delta;
    Actor356100_PositionDelta(coord, d);
    if (!Actor356100_OutOfRange(d, 3000)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        work->field_0 = 6;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80167A7C);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_8016804C);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801684F0);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80168AFC);

/// Turn the actor's facing onto the player in 0x28 steps and rescale the root
/// coordinate to 0x1194: the live branch resets the model and starts clip 2 at
/// speed 0x10 with the 0x13 state parked in `field_97E`, otherwise the aim
/// scratch takes the player offset, `Actor356100_PositionYaw` gives the wrapped
/// turn, `field_98E` walks toward it by at most 0x28 and the state flips to 0xB
/// once it has caught up. Same body as `func_actor_401300_8013AE48`.
void func_actor_356100_80168E44(Actor356100* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor356100AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 1;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 2;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 0x13;
        func_actor_356100_80163508(arg0);
        func_actor_356100_80163508(arg0);
        work->field_6   = 0;
        work->field_990 = 0;
        return;
    }
    *(Actor356100AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor356100AimScratch**)G_SCRATCH_HEAD;
    aim->angle                                = Actor356100_PositionYaw(arg0, &aim->delta, &Wip_SysConfig);
    if (work->field_98E < aim->angle) {
        if (aim->angle - work->field_98E > 0x28) {
            work->field_98E += 0x28;
        } else {
            work->field_98E = aim->angle;
        }
    } else if (work->field_98E - aim->angle > 0x28) {
        work->field_98E -= 0x28;
    } else {
        work->field_98E = aim->angle;
    }
    if (work->field_98E == aim->angle) {
        work->field_0 = 0xB;
    }
    coord      = arg0->field_2C->field_8;
    aim->angle = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, aim->angle, 1);
    Actor356100_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    work->field_978 = 2;
    func_actor_356100_80163508(arg0);
    *(Actor356100AimScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80169180);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80169854);

s32 func_actor_356100_80169E5C(void)
{
    return 0;
}
