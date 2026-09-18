#include "common.h"
#include "actors/actor_560800.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

#include "main/fs.h"
#include "psyq/libgpu.h"

extern TaskDesc ActorsShared80136280Desc;
extern s32      D_actor_560800_8016F57C[];
extern s8       D_8007106B;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_560800_801362B0(s32 arg0)
{
    Task_SpawnFromTable(&ActorsShared80136280Desc, 3, arg0, 0);
}

void func_actor_560800_801362E0(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;
    Actor560800Msg   msg;

    msg.field_2 = arg0;
    Gp_DispatchMsg(work->field_20, 0x7DB, (s32)&msg, 0);
}

void func_actor_560800_8013631C(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;
    Actor560800Msg   msg;

    msg.field_2 = arg0;
    Gp_DispatchMsg(work->field_24, 0x7DB, (s32)&msg, 0);
}

void func_actor_560800_80136358(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;

    work->field_28 = arg0;
    work->field_2A = 0;
}

/// Latches the animation id in the 0x60 slot and plays that animation on the
/// task at `field_0`: message 0x3F4 with `field_8` 1, `field_C` 0xA and
/// `field_10` 1. The zero-extended id goes into the message while the store
/// keeps the raw halfword argument, so the two uses do not share a register.
void func_actor_560800_80136378(s16 arg0)
{
    Actor560800Work* work;
    GpAnimArg        msg;
    u16              anim;

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    if (work->field_0 != NULL) {
        anim           = arg0;
        msg.field_0    = D_actor_560800_8016EA40;
        work->field_60 = arg0;
        msg.field_4    = anim;
        msg.field_8    = 1;
        msg.field_C    = 0xA;
        msg.field_10   = 1;
        Gp_DispatchMsg(work->field_0, 0x3F4, (s32)&msg, 0);
        work->field_62 = 0;
    }
}

/// The same animation reseed as `func_actor_560800_801364A0`, reached through
/// `field_4` instead of `field_C`: the id goes to `field_4B8` with 0x10 as the
/// restart rate in `field_4C8`, `field_4BE` is cleared, and slots 1..`field_4BA`
/// are blended through `func_800B4114`.
void func_actor_560800_801363F8(u16 arg0)
{
    Actor560800Work*     work;
    Actor560800AnimWork* anim;
    u16                  i;

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    anim = (Actor560800AnimWork*)work->field_4->work;

    anim->field_4B8 = arg0;
    anim->field_4C8 = 0x10;
    anim->field_4BE = 0;
    SOFT_BARRIER();
    i = 1;
    if (i < anim->field_4BA) {
        do {
            func_800B4114(&anim->anim, i, arg0, 0, 10);
            i++;
        } while (i < anim->field_4BA);
    }
}

/// Reseeds the animation slots of the sub-task at `field_C` from `arg0`: the
/// id goes to `field_4B8` with 0x10 as the restart rate in `field_4C8`,
/// `field_4BE` is cleared, and slots 1..`field_4BA` are blended through
/// `func_800B4114`. `func_actor_560800_801363F8` is the same body reached
/// through `field_4`.
///
/// The `SOFT_BARRIER()` is a codegen pin, not a semantic one: without it sched1
/// hoists `i = 1` above the three slot stores, so the count test runs before
/// the assignment and the guard's `beqz` delay slot gets the `field_4BE` store
/// instead of the loop's `i = 1`.
void func_actor_560800_801364A0(u16 arg0)
{
    Actor560800Work*     work;
    Actor560800AnimWork* anim;
    u16                  i;

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    anim = (Actor560800AnimWork*)work->field_C->work;

    anim->field_4B8 = arg0;
    anim->field_4C8 = 0x10;
    anim->field_4BE = 0;
    SOFT_BARRIER();
    i = 1;
    if (i < anim->field_4BA) {
        do {
            func_800B4114(&anim->anim, i, arg0, 0, 10);
            i++;
        } while (i < anim->field_4BA);
    }
}

/// Copies a 64x256 strip of VRAM to (0x280, 0x100), then re-loads the chunk at
/// `D_8006C454` with `D5B498_8006C234` set to 5 for the duration (that byte is
/// the image mode `Fs_LoadImageChunk` reads for chunks whose second halfword is
/// in 0xF5..0xFF), restoring it to 0 afterwards.
void func_actor_560800_80136548(void)
{
    RECT rect;

    rect.x = 0x3C0;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x100;
    MoveImage(&rect, 0x280, 0x100);
    D5B498_8006C234 = 5;
    Fs_LoadImageChunk(D_8006C454, 1);
    D5B498_8006C234 = 0;
}

void func_actor_560800_801365B0(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;

    work->field_40 = arg0;
    work->field_42 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801365D0);

void func_actor_560800_80136678(s32 arg0)
{
    SndEvt_EnqueueType6(D_actor_560800_8016F57C[arg0], 0, 0);
}

/// Task state handler for the second spawn mode: states 1 and 2 — and state 0,
/// which first parks `D_8007106B` at 2 — only step the state, and state 3 runs
/// the hand-off. That hand-off copies a 64x256 VRAM strip from (0x380, 0) to
/// (0x200, 0x100), the same shape `func_actor_560800_80136548` uses for the
/// other strip, then re-loads the chunk at `D_8006C45C` with
/// `D5B498_8006C234` at 8 for the duration, kills this task, resets the
/// display heap and spawns `ActorsShared80136280Desc` index 0xB into the work
/// block's `field_4`. Like `func_actor_310100_801620FC`, state 3 hands the
/// finished work over rather than leaving the task alive.
void func_actor_560800_801366B0(Task* arg0)
{
    RECT             rect;
    Actor560800Work* work;

    work = (Actor560800Work*)D_actor_560800_8017578C->work;
    switch (arg0->state) {
        case 0:
            D_8007106B = 2;
            /* fallthrough */
        case 1:
        case 2:
            arg0->state++;
            return;
        case 3:
            rect.x = 0x380;
            rect.y = 0;
            rect.w = 0x40;
            rect.h = 0x100;
            MoveImage(&rect, 0x200, 0x100);
            D5B498_8006C234 = 8;
            Fs_LoadImageChunk(D_8006C45C, 1);
            D5B498_8006C234 = 0;
            Task_Kill(arg0);
            Display_ResetHeapWrapper();
            work->field_4 = Task_SpawnOnDefaultList(&ActorsShared80136280Desc, 0xB, 1, (s32)D_actor_560800_8017578C);
            break;
    }
}

void func_actor_560800_801367C0(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;

    work->field_58 = arg0;
    work->field_5A = 0;
}

void func_actor_560800_801367E0(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;

    work->field_28 = arg0;
    work->field_2A = 0;
    work->field_38 = arg0;
    work->field_3A = 0;
    work->field_30 = arg0;
    work->field_32 = 0;
    work->field_40 = arg0;
    work->field_42 = 0;
}

void func_actor_560800_80136818(void)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;
    PlayerStatus*    cfg  = &Player_Status;
    s16              hp;

    Gp_KillPlayerEffs();

    if (cfg->hp < 0x33) {
        hp = 1;
    } else {
        hp = (u16)cfg->hp - 0x32;
    }
    do {
        cfg->hp        = hp;
        work->field_64 = 1;
    } while (0);
}

/// Clears the four s16 pairs at 0x28/0x30/0x38/0x40, and the first time it runs
/// (0x64 still zero) kills the player effects and drops the current HP by 50,
/// then latches 0x64. Ends by pulsing gameplay state 0x1C, cancelling the
/// pending CD command and blanking the display.
void func_actor_560800_80136878(void)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->work;
    s16              hp;

    work->field_28 = 0;
    work->field_40 = 0;
    work->field_30 = 0;
    work->field_38 = 0;
    if ((u16)work->field_64 == 0) {
        PlayerStatus*    cfg   = &Player_Status;
        Actor560800Work* work2 = (Actor560800Work*)D_actor_560800_8017578C->work;

        Gp_KillPlayerEffs();
        if (cfg->hp < 0x33) {
            hp = 1;
        } else {
            hp = (u16)cfg->hp - 0x32;
        }
        do {
            cfg->hp         = hp;
            work2->field_64 = 1;
        } while (0);
    }
    Gp_PulseState1C();
    CdCmd_CancelReplaceAndActivate();
    SetDispMask(0);
}
