#include "common.h"

#include "actors/actor_102100.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn00048);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn004C4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn00ADC);

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Gp_ArmStateF0(s32 arg0);

s32  Actor02100_Fn0337C(SVECTOR* arg0, SVECTOR* arg1);
void Actor02100_Fn011C4(Actor02100* arg0);

/// Line-of-sight scan. Takes a 0x20-byte block from `G_SCRATCH_HEAD`, builds
/// the world-space delta from this actor's coordinate to the player's (entry 0
/// of the player's coordinate array in mode 4, entry 3 otherwise) and, when the
/// player is in front of the actor, checks the distance against the sight range
/// in `Actor02100_D03E00` and asks `Actor02100_Fn0337C` whether the segment is
/// clear. A hit latches the player onto `field_140` and switches the state
/// machine to 2 (or 3 in mode 4). `field_186` throttles the scan to one run
/// every 5 frames while the session is in state 1.
void Actor02100_Fn00DCC(Actor02100* arg0)
{
    Actor02100Work*  work;
    Actor02100Sight* blk;
    GsCOORDINATE2*   self;
    GsCOORDINATE2*   target;
    u8*              head;
    u32              dist;
    s32              mode;

    self = arg0->field_2C->field_8;
    work = arg0->field_1C;

    if (Game_Session->field_4D == 1) {
        work->field_186 = 5;
    }
    if (work->field_186 != 0) {
        work->field_186--;
        return;
    }

    work->field_140 = NULL;
    work->field_164 = 0;
    work->field_180 = 0;
    self->flg       = 0;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x20;
    blk                   = (Actor02100Sight*)(head - 0x20);

    if (work->field_178 == 4) {
        target = &((Actor02100*)Game_GetPtrSlot(3))->field_2C->field_8[0];
    } else {
        target = &((Actor02100*)Game_GetPtrSlot(3))->field_2C->field_8[3];
    }
    target->flg = 0;
    Gp_UpdateCoord(target);

    if (work->field_17E == 0) {
        blk->delta.vx = target->workm.t[0] - self->workm.t[0];
        blk->delta.vy = target->workm.t[1] - self->workm.t[1];
        blk->delta.vz = target->workm.t[2] - self->workm.t[2];

        if (((blk->delta.vx * self->workm.m[0][2]) + (blk->delta.vy * self->workm.m[1][2]) +
             (blk->delta.vz * self->workm.m[2][2])) > 0) {
            dist = SquareRoot0((blk->delta.vx * blk->delta.vx) + (blk->delta.vy * blk->delta.vy) +
                               (blk->delta.vz * blk->delta.vz));
            if (dist < Actor02100_D03E00[arg0->field_20->field_3C->field_F & 7]) {
                blk->from.vx = target->workm.t[0];
                blk->from.vy = target->workm.t[1];
                blk->from.vz = target->workm.t[2];
                blk->to.vx   = self->workm.t[0];
                blk->to.vy   = self->workm.t[1];
                blk->to.vz   = self->workm.t[2];
                if (Actor02100_Fn0337C(&blk->from, &blk->to) == 0) {
                    work->field_140 = Game_GetPtrSlot(3);
                    work->field_164 = dist;
                    work->field_180 = 1;
                }
            }
        }
        Actor02100_Fn011C4(arg0);
    } else {
        blk->delta.vx = target->workm.t[0] - self->workm.t[0];
        blk->delta.vy = target->workm.t[1] - self->workm.t[1];
        blk->delta.vz = target->workm.t[2] - self->workm.t[2];

        if (((blk->delta.vx * self->workm.m[0][2]) + (blk->delta.vy * self->workm.m[1][2]) +
             (blk->delta.vz * self->workm.m[2][2])) > 0) {
            blk->from.vx = target->workm.t[0];
            blk->from.vy = target->workm.t[1];
            blk->from.vz = target->workm.t[2];
            blk->to.vx   = self->workm.t[0];
            blk->to.vy   = self->workm.t[1];
            blk->to.vz   = self->workm.t[2];
            if (Actor02100_Fn0337C(&blk->from, &blk->to) == 0) {
                work->field_140 = Game_GetPtrSlot(3);
                work->field_164 = 1;
                work->field_180 = 1;
            }
        }
    }

    if (work->field_180 != 0) {
        Gp_ArmStateF0(1);
        if (work->field_178 == 4) {
            mode = 3;
        } else {
            mode = 2;
        }
        work->field_120 = work->field_118;
        work->field_122 = work->field_11A;
        work->field_124 = work->field_11C;
        work->field_172 = mode;
        work->field_174 = 0;
        work->field_17A = 0;
        work->field_17C = 0;
        work->field_118 = 0;
        work->field_11A = 0;
        work->field_11C = 0;
        if (work->field_188 == 1) {
            SndEvt_EnqueueType7(work->field_168, 1);
            work->field_188 = 0;
        }
    }

    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + 0x20;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn011C4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn014E4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn016EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn01FF0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn02924);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn03168);

void Actor02100_Fn004C4(Actor02100* arg0);
void Actor02100_Fn03488(Actor02100* arg0);

extern u8 D_801153F4;
extern s8 D_80115416;

/// Per-frame tick, entry 1 of `Actor02100_D00004`. `D_801153F4` is the global
/// gameplay mode: mode 1 only refreshes the actor colour, mode 2 parks the
/// actor (`field_C` 0x80, node flag 1) and returns, and mode 0 re-shows it
/// (`field_C` 0, node flag 8) before falling into the normal body. The body
/// drains the pending translation delta at `field_118` into the actor's
/// coordinate, runs the state machine, and switches to state 4 - handing the
/// task over to `Actor02100_Fn035D4` - once `D_80115416` reports the kill.
void Actor02100_Fn031C4(Actor02100Ctx* arg0, Actor02100* arg1)
{
    Actor02100Obj2C* obj;
    Actor02100Work*  work;
    GsCOORDINATE2*   coord;
    s32              mode;
    s32              one;

    obj   = arg1->field_2C;
    mode  = D_801153F4;
    work  = arg1->field_1C;
    coord = obj->field_8;
    one   = 1;
    if (mode == one) {
        goto case1;
    }
    if (mode >= 2) {
        goto ge2;
    }
    if (mode == 0) {
        goto case0;
    }
    goto body;
ge2:
    if (mode == 2) {
        goto case2;
    }
    goto body;
case0:
    obj->field_C       = 0;
    arg0->node.field_4 = 8;
    goto body;
case1:
    Actor02100_Fn03488(arg1);
    return;
case2:
    obj->field_C       = 0x80;
    arg0->node.field_4 = one;
    return;
body:
    Actor02100_Fn004C4(arg1);
    coord->coord.t[0] += work->field_118;
    coord->coord.t[1] += work->field_11A;
    coord->coord.t[2] += work->field_11C;
    coord->flg         = 0;
    Gp_UpdateCoord(coord);
    Actor02100_Fn032E4(arg1);
    Actor02100_Fn03488(arg1);
    if (D_80115416 == 1) {
        work->field_172 = 4;
        work->field_174 = 0;
        arg1->field_30  = 2;
    }
}

void Actor02100_Fn00ADC(Actor02100* arg0);
void Actor02100_Fn016EC(Actor02100* arg0);
void Actor02100_Fn01FF0(Actor02100* arg0);

void Actor02100_Fn032E4(Actor02100* arg0)
{
    s16 state;

    state = arg0->field_1C->field_172;
    switch (state) {
        case 1:
            Actor02100_Fn00ADC(arg0);
        case 0:
            if (GameFlag_GetNibble(0xD2) == 0) {
                Actor02100_Fn00DCC(arg0);
            }
            break;
        case 2:
            Actor02100_Fn016EC(arg0);
            break;
        case 3:
            Actor02100_Fn01FF0(arg0);
            break;
        case 4:
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn0337C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn03488);

INCLUDE_ASM("actors/nonmatchings/lib/actor_102100_text", Actor02100_Fn034E0);

void Gp_UnlinkNode(void* node);
void Gp_UnlinkObj(void* node);
void Gp_ReleaseStateF0Add(void* arg0, s32 arg1);
void Gp_DestroyEnemy(void* enemy, void* task);

void Actor02100_Fn035D4(Actor02100Ctx* arg0, Actor02100* arg1)
{
    Actor02100Work* work;
    s16             state;
    u16             timer;

    work  = arg1->field_1C;
    state = work->field_174;
    if (state == 0) {
        goto case0;
    }
    if (state == 1) {
        goto case1;
    }
    goto epilogue;
case0:
    arg1->field_2C->field_C = 0x80;
    Gp_UnlinkNode(&arg0->node);
    Gp_UnlinkObj(work->field_40);
    Gp_UnlinkObj(work->field_78);
    Gp_UnlinkObj(work->field_C8);
    arg0->field_54 = 0;
    Gp_ReleaseStateF0Add(arg1, 0x15);
    work->field_174 = 1;
    work->field_17A = 0x3C;
    if (work->field_188 != 0) {
        SndEvt_EnqueueType7(work->field_168, 1);
    }
    goto epilogue;
case1:
    timer = work->field_17A;
    timer--;
    work->field_17A = timer;
    if ((s16)timer > 0) {
        goto epilogue;
    }
    Gp_DestroyEnemy(arg0, arg1);
epilogue:
    return;
}
