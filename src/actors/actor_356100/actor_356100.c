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

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80167818);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80167A7C);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_8016804C);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801684F0);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80168AFC);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80168E44);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80169180);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80169854);

s32 func_actor_356100_80169E5C(void)
{
    return 0;
}
