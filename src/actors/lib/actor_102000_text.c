#include "common.h"

#include "actors/actor_102000.h"
#include "main/mem.h"

void Gp_ArmStateF0(s32 arg0);
void Actor02000_Fn00CD0(Actor02000* arg0);
s32  Gp_TickObjFlag2(void* arg0);
s32  Gp_GetObjPan(void* arg0);
s32  Gp_GetObjDepth(void* arg0);
s32  SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s32 Gp_LcgState;
extern s8  D_80115419;
extern s16 Actor02000_D03784[];
extern s32 Actor02000_D15DEC[];

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn00078);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00124);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0015C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00178);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00180);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L001D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L001FC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0020C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00214);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00238);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0023C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00268);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L003F0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0041C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00424);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00484);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L004B4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L004B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L004BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L004EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L004F0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00534);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00548);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00574);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00578);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L005CC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00618);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00638);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0064C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00674);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L006B4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L006EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00720);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00750);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00770);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00794);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L007B4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L007EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0080C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0081C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00828);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00834);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00840);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00858);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00860);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00874);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00888);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0088C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L008B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00948);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00984);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00988);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L009E0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00A10);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00A9C);

/// Per-frame tick for the actor's approach cycle, sharing the `field_6A8`
/// state with `Actor02000_Fn03268`. State 0 drains the `field_6DA` budget by
/// `field_69C` (0 while `field_698` is still under the per-animation entry of
/// `Actor02000_D03784`, 0x14 once it is past it) and runs
/// `Actor02000_Fn00CD0` every frame; when the budget runs out it switches to
/// animation 4 and state 1. State 1 waits for `field_698` to reach 0x60, then
/// either falls back to animation 2 (budget left) or starts the lunge:
/// animation 3, state 2, a fresh budget of 1000 per unit of the spawn record's
/// byte 1, and `field_6A2` / `field_6A4` set to the actor's current yaw and its
/// opposite. State 2 holds `field_69E` at 0x3B until `field_698` reaches 0x23,
/// then returns to animation 2 and state 0. As in `Actor02000_Fn03268`, a set
/// `field_6B2` or `D_80115419` overrides everything with animation 2 and the
/// shared state-F0 slot.
void Actor02000_Fn00AEC(Actor02000* arg0)
{
    Actor02000Work*  work;
    Actor02000Spawn* spawn;
    GsCOORDINATE2*   self;
    u8*              head;
    s16              state;
    s16              delta;
    s32              ang;
    s32              param;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x10;

    self  = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    spawn = arg0->field_20;
    state = work->field_6A8;

    switch (state) {
        case 0:
            delta = 0;
            if (work->field_698 >= Actor02000_D03784[work->field_694]) {
                delta = 0x14;
            }
            work->field_69C  = delta;
            work->field_69E  = 0;
            work->field_6DA -= work->field_69C;
            if (work->field_6DA <= 0) {
                work->field_694 = 4;
                work->field_6AE = 0;
                work->field_6A8 = 1;
                work->field_69C = 0;
            }
            Actor02000_Fn00CD0(arg0);
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            if (work->field_698 >= 0x60) {
                if (work->field_6DA <= 0) {
                    param           = spawn->field_3C->field_1;
                    work->field_694 = 3;
                    work->field_6A8 = 2;
                    work->field_6DA = param * 1000;
                    ang             = ratan2(self->coord.m[0][2], self->coord.m[2][2]) & 0xFFF;
                    work->field_6A2 = ang;
                    work->field_6A4 = (ang + 0x800) & 0xFFF;
                } else {
                    work->field_694 = 2;
                    work->field_6A8 = 0;
                }
            }
            break;
        case 2:
            work->field_69C = 0;
            work->field_69E = 0x3B;
            if (work->field_698 >= 0x23) {
                work->field_694 = 2;
                work->field_6A8 = 0;
            }
            break;
    }

    if ((work->field_6B2 != 0) || (D_80115419 != 0)) {
        work->field_6A6 = 2;
        work->field_6A8 = 0;
        work->field_694 = 2;
        work->field_6AE = 0;
        Gp_ArmStateF0(1);
    }

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + 0x10;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn00CD0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00D94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00DAC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00DC0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00DE0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn00E0C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00E64);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00E8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00EA4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00EEC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00F60);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L00FC0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01024);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0108C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L010E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L010F4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L010F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01100);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01118);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01148);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01160);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01190);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L011C8);

/// Per-frame tick. State 0 picks the animation from `field_6B8`: 1 selects
/// animation 0x17 and hands over to state 1, anything else selects 0x1B and
/// hands over to state 2. State 1 waits for `field_698` to reach 0x10 and
/// state 2 waits for it to reach 0x16; both then play the matching idle
/// (0x19 / 0x1D), park `field_6A6` at 0xB, move to state 3 and roll a fresh
/// 6-bit dwell into `field_6AE`.
void Actor02000_Fn011E8(Actor02000* arg0)
{
    Actor02000Work* work;
    s16             state;
    s32             next;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            next = work->field_6B8;
            if (next == 1) {
                work->field_694 = 0x17;
                work->field_6A8 = next;
            } else {
                work->field_694 = 0x1B;
                work->field_6A8 = 2;
            }
            break;
        case 1:
            if (work->field_698 >= 0x10) {
                work->field_694 = 0x19;
                work->field_6A6 = 0xB;
                work->field_6A8 = 3;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6AE = ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            break;
        case 2:
            if (work->field_698 >= 0x16) {
                work->field_694 = 0x1D;
                work->field_6A6 = 0xB;
                work->field_6A8 = 3;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6AE = ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            break;
    }
}

void Actor02000_Fn012E0(Actor02000* arg0)
{
    Actor02000Work* work;
    GsCOORDINATE2*  self;
    s32             snd;
    s16             state;

    work  = arg0->field_1C;
    self  = arg0->field_2C->field_8;
    state = work->field_6A8;

    switch (state) {
        case 0:
            if (work->field_6AA == 0) {
                work->field_694 = 0x16;
                work->field_6A8 = 1;
                work->field_6B8 = 1;
                work->field_6AE = 0x42;
                work->field_4E0 = -0xA7;
            } else {
                work->field_694 = 0x1A;
                work->field_6A8 = 1;
                work->field_6B8 = 2;
                work->field_6AE = 0x31;
                work->field_4E0 = 0x109;
            }
            work->field_4E8          = 0x15E;
            work->field_69C          = 0;
            work->field_69E          = 0;
            work->field_6DE          = 1;
            work->field_4EA         |= 0x4000;
            work->field_582         &= 0xBFFF;
            arg0->field_20->field_4C = 0;
            work->field_6D4          = 1;
            break;
        case 1:
            if (work->field_6DE == 1) {
                work->field_6DE = 2;
            }
            if (work->field_6B8 == 1) {
                if (work->field_698 == 0x14) {
                    s32 pan;

                    snd = Actor02000_D15DEC[work->field_6D6 + 0xC] | ((arg0->field_20->field_8 >> 12) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(self));
                }
                if (work->field_698 == 0x2C) {
                    s32 pan;

                    snd = Actor02000_D15DEC[work->field_6D6 + 8] | ((arg0->field_20->field_8 >> 12) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(self));
                }
            } else if (work->field_698 == 0x19) {
                s32 pan;

                snd = Actor02000_D15DEC[work->field_6D6 + 8] | ((arg0->field_20->field_8 >> 12) << 8);
                pan = (s8)Gp_GetObjPan(self);

                SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth(self));
            }
            work->field_6AE--;
            if (work->field_6AE <= 0) {
                arg0->field_30  = 2;
                work->field_6A8 = 0;
                work->field_6D4 = 0;
            }
            break;
    }
}

void Actor02000_Fn0150C(Actor02000* arg0)
{
    Actor02000Work* work;
    GsCOORDINATE2*  coord;
    SVECTOR*        rot;
    s32             ang;
    u16             want;
    s16             diff;
    s32             adiff;
    s32             step;
    s32             ustep;
    s32             wstep;
    s32             cur;
    s32             next;
    s32             wrapStep;

    rot   = (SVECTOR*)(SCRATCH_SP -= 8);
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_6A4;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_6A2 = ang;
    if (adiff < 0x800) {
        step  = work->field_69E;
        ustep = (u16)work->field_69E;
        if (step >= adiff) {
            work->field_6A2 = want;
        } else {
            if (work->field_694 == 3) {
                next = ang - ustep;
            } else {
                next = work->field_6A2;
                if (diff <= 0) {
                    next -= step;
                } else {
                    next += step;
                }
            }
            work->field_6A2 = next;
        }
    } else {
        wstep = work->field_69E;
        if (diff > 0) {
            if (wstep >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (wstep >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_6A2 = work->field_6A4;
        goto done;
    turn:
        if (work->field_694 == 3) {
            work->field_6A2 = (u16)work->field_6A2 - (u16)work->field_69E;
        } else {
            wrapStep = work->field_69E;
            cur      = work->field_6A2;
            if (diff > 0) {
                work->field_6A2 = cur - wrapStep;
            } else {
                work->field_6A2 = cur + wrapStep;
            }
        }
    }
done:
    rot->vx = 0;
    rot->vy = work->field_6A2;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    SCRATCH_SP += 8;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn01698);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L017F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0180C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01818);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01820);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0183C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01850);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0185C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01864);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01870);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn018A4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01970);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L019EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L019FC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn01A20);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01A94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01AA8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01ABC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01B5C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01B70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01B90);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01BDC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01BFC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01C24);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01C88);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01CC8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01CF4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01D08);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01D24);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01DD0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn01DF0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01E5C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01E6C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01F00);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01F04);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01F20);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01F64);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L01F9C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02054);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02084);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L020A0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L020A4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L020D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02100);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02120);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0212C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L021AC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L021C4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02248);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02258);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0225C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02260);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02264);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn02294);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L022F4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L023A4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L023A8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0240C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02474);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L024BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L024E4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L024E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L024EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn0251C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02580);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L025F0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L026B0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L026D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L026EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02764);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02798);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L027E0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02824);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L029E4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02A00);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02A0C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn02A34);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02A90);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02AA0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02AAC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02AB8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02B14);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02B58);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02BD4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02C38);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02C64);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02C78);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02C90);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02CA8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02CCC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02D38);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn02D5C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02DB4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02DD0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02E00);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02E90);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02EA4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02EB4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02EB8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02EBC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02EF0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02F00);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02F04);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02F40);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02FCC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L02FE8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L03018);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L030B0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L030C4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L030D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L030D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L030DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L03100);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0310C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L03130);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L03134);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L03138);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn0315C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L031EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0321C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0322C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L03230);

/// Per-frame tick, entry 0 of `Actor02000_D16064`. State 0 counts `field_6AE`
/// up to 0x5B frames and then hands over to state 1 with animation 4, running
/// `Actor02000_Fn00CD0` every frame meanwhile; state 1 waits for `field_698`
/// to reach 0x5E and drops back to state 0 with animation 1. Either way, once
/// `field_6B2` or the global `D_80115419` is set the actor switches to
/// animation 2 and arms the shared state-F0 slot.
void Actor02000_Fn03268(Actor02000* arg0)
{
    Actor02000Work* work;
    s16             state;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            work->field_6AE++;
            if (work->field_6AE >= 0x5B) {
                work->field_694 = 4;
                work->field_6AE = 0;
                work->field_6A8 = 1;
            }
            Actor02000_Fn00CD0(arg0);
            break;
        case 1:
            if (work->field_698 >= 0x5E) {
                work->field_694 = 1;
                work->field_6A8 = 0;
            }
            break;
    }

    if ((work->field_6B2 != 0) || (D_80115419 != 0)) {
        work->field_6A6 = 2;
        work->field_6A8 = 0;
        work->field_694 = 2;
        work->field_6AE = 0;
        Gp_ArmStateF0(1);
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn03348);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L03370);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L0338C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L033BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L033C4);

void Actor02000_L033CC(void)
{
}

/// Per-frame tick. State 0 picks the animation from `field_6AA`: 1 selects
/// animation 0x12 and hands over to state 1, anything else selects 0x13 and
/// hands over to state 2; either way the pair of counters at `field_69C` is
/// cleared. State 1 waits for `field_698` to reach 0x50 and state 2 waits for
/// it to reach 0x3B, both dropping back to state 0 with animation 2.
void Actor02000_Fn033D4(Actor02000* arg0)
{
    Actor02000Work* work;
    s32             state;
    s32             next;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            next = work->field_6AA;
            if (next == 1) {
                work->field_694 = 0x12;
                work->field_6A8 = next;
            } else {
                work->field_694 = 0x13;
                work->field_6A8 = 2;
            }
            work->field_69C = 0;
            work->field_69E = 0;
            break;
        case 1:
            if (work->field_698 >= 0x50) {
                work->field_694 = 2;
                work->field_6A6 = 2;
                work->field_6A8 = 0;
            }
            break;
        case 2:
            if (work->field_698 >= 0x3B) {
                work->field_694 = state;
                work->field_6A6 = state;
                work->field_6A8 = 0;
            }
            break;
    }
}

/// Per-frame tick. State 0 waits for `Gp_TickObjFlag2` on the spawn block to
/// fire, then selects animation 0x13, clears `field_6E0` and advances to state
/// 1. State 1 waits for `field_698` to reach 0x3B and drops back to state 0
/// with animation 2.
void Actor02000_Fn0349C(Actor02000* arg0)
{
    Actor02000Work* work;
    s16             state;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            if (Gp_TickObjFlag2(arg0->field_20) != 0) {
                work->field_694 = 0x13;
                work->field_6A8 = 1;
                work->field_6E0 = 0;
            }
            break;
        case 1:
            if (work->field_698 >= 0x3B) {
                work->field_694 = 2;
                work->field_6A6 = 2;
                work->field_6A8 = 0;
            }
            break;
    }
}

/// Per-frame tick. State 0 picks the animation from `field_6B8`: 1 selects
/// animation 0x17 and hands over to state 1, anything else selects 0x1B and
/// hands over to state 2. State 1 waits for `field_698` to reach 0x10 and
/// state 2 waits for it to reach 0x16; both park the actor by writing its
/// dwell code to `field_30` and drop back to state 0.
void Actor02000_Fn03528(Actor02000* arg0)
{
    Actor02000Work* work;
    s16             state;
    s32             next;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            next = work->field_6B8;
            if (next == 1) {
                work->field_694 = 0x17;
                work->field_6A8 = next;
            } else {
                work->field_694 = 0x1B;
                work->field_6A8 = 2;
            }
            break;
        case 1:
            if (work->field_698 >= 0x10) {
                arg0->field_30  = 2;
                work->field_6A8 = 0;
            }
            break;
        case 2:
            if (work->field_698 >= 0x16) {
                arg0->field_30  = state;
                work->field_6A8 = 0;
            }
            break;
    }
}

void Actor02000_Fn035E0(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn035E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn03644);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn03690);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_L03718);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102000_text", Actor02000_Fn03728);
