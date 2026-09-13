#include "common.h"

#include "actors/actor_461800.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern s16 D_actor_461800_801437B8;

/// Ticks animation slots 1..0x13 of the second variant's animation context.
void func_actor_461800_80133724(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_461800_801438A0->anim, i);
        i++;
    } while (i < 0x13);
}

/// Restarts animation slots 1..0x12 from `field_480`, flagging each slot's
/// `field_9` before the reset so it replays from the top, and latches that id
/// into `field_47E` as the one now playing.
void func_actor_461800_80133770(void)
{
    s32 i;

    D_actor_461800_801438A0->field_4B8 = 0;
    i                                  = 1;
    do {
        D_actor_461800_801438A0->slots[i].field_9 = 1;
        Gp_AnimResetSlot(&D_actor_461800_801438A0->anim, i, D_actor_461800_801438A0->field_480);
        i++;
    } while (i < 0x13);
    D_actor_461800_801438A0->field_47E = D_actor_461800_801438A0->field_480;
}

/// Reseeds animation slots 1..0x12 from `field_480` and latches that id into
/// `field_47E` as the one now playing.
void func_actor_461800_8013380C(void)
{
    s32 i;

    D_actor_461800_801438A0->field_4B8 = 0;
    i                                  = 1;
    do {
        func_800B4114(&D_actor_461800_801438A0->anim, i, (s16)D_actor_461800_801438A0->field_480, 0,
                      D_actor_461800_801437B8);
        i++;
    } while (i < 0x13);
    D_actor_461800_801438A0->field_47E = D_actor_461800_801438A0->field_480;
}

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_3", func_actor_461800_80133898);
