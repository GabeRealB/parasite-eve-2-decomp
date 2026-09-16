#include "common.h"

#include "main/sound.h"

#include "actors/actor_521100.h"
#include "actors/actors_shared_80134934.h"

void func_actor_521100_801322F8(Actor521100* arg0, Actor521100Obj2C* arg1, s32 arg2);
void func_actor_521100_80134C38(Actor521100* arg0);
void func_actor_521100_80134D88(Actor521100* arg0);
void func_actor_521100_80134EDC(Actor521100* arg0);
void func_actor_521100_80135024(Actor521100* arg0);
void func_actor_521100_80132958(void);
void func_actor_521100_80132DE8(void);
void func_actor_521100_801339B0(void);
void func_actor_521100_80134658(void);
void func_actor_521100_80134774(void);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", ActorsShared80131e24Sub0);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_801322F8);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80132958);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80132C70);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80132DE8);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80133104);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_8013334C);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_801335B4);

INCLUDE_RODATA("actors/nonmatchings/actor_521100/actor_521100", D_actor_521100_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_801339B0);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80134658);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80134774);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80134C38);

/// Plays the actor's footstep cues: while the animation record the cue body
/// reads carries `field_3` bit 0x20 (or 0x10), a sound is queued on the frame
/// that bit has just dropped from `Actor521100Work::field_6B4`, panned and
/// depth-attenuated from the actor's display coordinate. The record is the one
/// `Gp_AnimGetRec` returns for the slot at 0x3C - the second of the 0x28-byte
/// slots the actor work blocks lay out from 0x14, the same one the other actor
/// overlays' cue bodies play from. The cue id is the `GpEnemy` work id's bits
/// 12+ placed in bits 8-11 with the overlay's 0x401C tag, 1 for the 0x20 foot
/// and 2 for the 0x10 one, and the record's two bits are latched for the next
/// frame at the end.
void func_actor_521100_80134D88(Actor521100* arg0)
{
    s32               snd;
    s32               pan;
    s32               pan2;
    Actor521100Work*  work;
    Actor521100Coord* coord;
    GpAnimRec*        rec;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    rec   = Gp_AnimGetRec((GpAnimCtx*)work, (GpAnimSlot*)&work->pad_0[0x3C]);
    if (rec != NULL) {
        if (!(rec->field_3 & 0x20) && (work->field_6B4 & 0x20)) {
            snd = ((arg0->field_20->field_8 >> 12) << 8) | 0x401C0001;
            pan = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
        if (!(rec->field_3 & 0x10) && (work->field_6B4 & 0x10)) {
            snd  = ((arg0->field_20->field_8 >> 12) << 8) | 0x401C0002;
            pan2 = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan2, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
        work->field_6B4 = (u16)(rec->field_3 & 0x30);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80134EDC);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80135024);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100", func_actor_521100_80135230);

INCLUDE_RODATA("actors/nonmatchings/actor_521100/actor_521100", ActorsShared80135df4Table);
