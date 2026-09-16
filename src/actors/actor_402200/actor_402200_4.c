#include "common.h"

#include "main/sound.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_402200.h"

/// Cue-id table: `Actor402200Work::field_712` picks two adjacent words,
/// `[field_712 * 2 - 1]` for the `field_3` bit 0x20 cue and `[field_712 * 2]`
/// for the 0x10 one.
extern s32 D_actor_402200_80138420[];

INCLUDE_RODATA("actors/nonmatchings/actor_402200/actor_402200_4", ActorsShared80135df4Table);

void func_actor_402200_80135A24(Actor402200* arg0)
{
    Actor402200Work*  work;
    Actor402200Coord* coord;
    s32               state;
    s32               snd;
    s32               pan;
    s32               frames;
    s16               timer;

    work  = arg0->field_1C;
    state = work->field_6CE;
    coord = arg0->field_2C->field_8;
    switch (state) {
        case 0:
            if (work->field_6D2 == 0) {
                work->field_6C0 = 0xD;
                work->field_6CE = 1;
                work->field_6F0 = 1;
                work->field_6D4 = 0x42;
                work->field_490 = -0xA7;
            } else {
                work->field_6C0 = 0x11;
                work->field_6CE = 1;
                work->field_6F0 = 2;
                work->field_6D4 = 0x31;
                work->field_490 = 0x109;
            }
            work->field_498  = 0x15E;
            work->field_714  = 1;
            work->field_6DA  = 1;
            work->field_6DC  = 0x14;
            work->field_6DE  = 0xA;
            work->field_6F2  = 2;
            work->field_6C8  = 0;
            work->field_49A |= 0x4000;
            work->field_502 &= 0xBFFF;
            break;
        case 1:
            if (work->field_714 == state) {
                work->field_714 = 2;
            }
            frames = 0x19;
            if (work->field_6F0 == state) {
                frames = 0x2C;
            }
            if (work->field_6C4 == frames) {
                snd = D_actor_402200_80138420[work->field_712 + 8] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                arg0->field_30  = 2;
                work->field_6CE = 0;
                work->field_6F2 = 0;
            }
            break;
    }
}

/// Fires the cue pair the work block's `field_712` selects: while the second
/// animation slot carries `field_3` bit 0x20 or 0x10, a sound is queued on the
/// frame that bit has just dropped from `Actor402200Work::field_6CA`, panned
/// and depth-attenuated from the actor's display object. The cue id is the
/// matching word of `D_actor_402200_80138420` with the `GpEnemy` work id's high
/// nibble in bits 8-11, and a zero `field_712` disarms the body. The record's
/// two bits are latched for the next frame at the end.
void func_actor_402200_80135BE0(Actor402200* arg0)
{
    s32               snd;
    s32               pan;
    s32               pan2;
    Actor402200Work*  work;
    Actor402200Coord* coord;
    GpAnimRec*        rec;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    if (work->field_712 != 0) {
        rec = Gp_AnimGetRec((GpAnimCtx*)work, (GpAnimSlot*)&work->field_3C);
        if (rec != NULL) {
            if (!(rec->field_3 & 0x20) && (work->field_6CA & 0x20)) {
                snd = D_actor_402200_80138420[work->field_712 * 2 - 1] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            if (!(rec->field_3 & 0x10) && (work->field_6CA & 0x10)) {
                snd  = D_actor_402200_80138420[work->field_712 * 2] | (((u16)arg0->field_20->field_8 >> 0xC) << 8);
                pan2 = (s8)Gp_GetObjPan((GpObj38*)coord);
                SndEvt_EnqueueType6(snd, pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            work->field_6CA = (u16)(rec->field_3 & 0x30);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_4", func_actor_402200_80135D5C);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_4", func_actor_402200_80136184);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_4", func_actor_402200_801368E0);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_4", func_actor_402200_80136D9C);

INCLUDE_ASM("actors/nonmatchings/actor_402200/actor_402200_4", func_actor_402200_80137444);
