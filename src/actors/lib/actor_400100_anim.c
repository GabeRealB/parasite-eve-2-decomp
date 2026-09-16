#include "common.h"
#include "actors/actor_400100.h"
#include "actors/actor_400100_facing.h"
#include "gameplay/1BC.h"

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_anim", Actor00100_Fn001FC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_anim", Actor00100_Fn00508);

/// Steps `coord` by the movement the first `arg2` `GpRec18` records of `movement`
/// resolve to, and latches the integer part of that delta into
/// `Actor00100_D1BA90`. Returns the "moved" flag: set when the X or Z delta is
/// nonzero, and also when its fractional half is, in which case the coordinate
/// and the latched step are nudged one unit further away from zero.
s32 Actor00100_Fn00A54(GsCOORDINATE2* coord, GpRec18* movement, s16 arg2)
{
    void**               scratch;
    u8*                  head;
    Actor00100DeltaFlag* s;
    register void*       p asm("v1");
    s32                  val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(movement, &s->delta, (s32)arg2, NULL) != 0) {
        coord->coord.t[0]    = coord->coord.t[0] + ((Actor00100DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]    = coord->coord.t[2] + s->delta.vz.h.hi;
        Actor00100_D1BA90.vx = ((Actor00100DeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        Actor00100_D1BA90.vy = s->delta.vy.w >> 16;
        Actor00100_D1BA90.vz = s->delta.vz.w >> 16;
        val                  = ((Actor00100DeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                Actor00100_D1BA90.vx++;
            } else {
                coord->coord.t[0]--;
                Actor00100_D1BA90.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                Actor00100_D1BA90.vz++;
            } else {
                coord->coord.t[2]--;
                Actor00100_D1BA90.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_anim", Actor00100_Fn00BF8);

/// Message handler for the walking animation. `field_0` is the opcode:
/// 0x109 drives the aim state machine, 0x104 picks one of the four poses in
/// `Actor00100_D00004` with the global LCG, 0x1602 plays the step sound with
/// the pose latched out of `Actor00100_D0BDB4`, and 0x202 writes the fixed
/// crouch pose. Every opcode except 0x104/0x1602/0x202 returns 0.
///
/// Each LCG arm keeps its own `value` local: they are separate variables
/// because the arms are separate blocks and one local shared between them
/// changes which register the allocator picks in every arm.
s32 Actor00100_Fn00E58(Actor00100* arg0, s32 arg1, Actor00100Msg* arg2)
{
    Actor00100PoseTable table;
    Actor00100PoseRow*  row;
    Actor00100Work*     work;
    Actor00100Ctx*      ctx;
    s8                  rnd;
    s32                 view;
    u32                 value2;
    u32                 value4;
    u32                 value5;
    u32                 value38;
    u32                 valueDefault;
    s32                 kind;
    s32                 cmd;
    s32                 sub;
    s32                 req;
    s32                 sound;
    s32                 pan;

    work = arg0->field_1C;
    ctx  = arg0->field_20;

    if (arg2->field_0.word == 0x109) {
        kind = arg2->field_2.word;
        switch (kind) {
            case 1:
                work->field_C26 = 0x5A;
                break;
            case 2:
                if (work->field_0 == 0x26) {
                    work->field_0 = 0x26;
                }
                break;
            case 3:
                work->field_C26 = work->field_C24;
                break;
            case 4:
                if (work->field_0 == 0x21) {
                    work->field_0 = 0x22;
                }
                if (work->field_0 == 0x18) {
                    work->field_0 = 0x26;
                }
                break;
        }
        return 1;
    } else {
        work->field_C0C = arg2->field_0.bytes[0];
        work->field_C0D = arg2->field_0.bytes[1];
        work->field_C0E = arg2->field_2.bytes[0];
        if (arg2->field_0.word == 0x104) {
            table = Actor00100_D00004;
            cmd   = arg2->field_2.word;
            switch (cmd) {
                case 0:
                    work->field_0 = 0;
                    break;
                case 1:
                    view = Gp_GetViewIndex() & 0xFF;
                    switch (view) {
                        case 2:
                            value2      = (Gp_LcgState * 5) + 0x71357911;
                            Gp_LcgState = value2;
                            rnd         = ((value2 >> 0x10) % 3) + 1;
                            break;
                        case 4:
                            value4      = (Gp_LcgState * 5) + 0x71357911;
                            Gp_LcgState = value4;
                            rnd         = 1;
                            if (((value4 >> 0x10) & 1) == 0) {
                                rnd = 3;
                            }
                            break;
                        case 5:
                            value5      = (Gp_LcgState * 5) + 0x71357911;
                            rnd         = (value5 >> 0x10) & 1;
                            Gp_LcgState = value5;
                            break;
                        case 3:
                        case 8:
                            value38     = (Gp_LcgState * 5) + 0x71357911;
                            rnd         = ((value38 >> 0x10) & 1) | 2;
                            Gp_LcgState = value38;
                            break;
                        default:
                            valueDefault = (Gp_LcgState * 5) + 0x71357911;
                            rnd          = (valueDefault >> 0x10) & 3;
                            Gp_LcgState  = valueDefault;
                    }
                    row                                 = &table.rows[rnd];
                    arg0->field_2C->field_8->coord.t[0] = row->vx;
                    arg0->field_2C->field_8->coord.t[1] = (s16)row->vy;
                    arg0->field_2C->field_8->coord.t[2] = (s16)row->vz;
                    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, (s16)row->yaw, 1);
                    arg0->field_2C->field_8->flg = 0;
                    work->field_0                = 5;
                    break;
            }
        }
        if (arg2->field_0.word == 0x1602) {
            sub = arg2->field_2.word;
            switch (sub) {
                case 0:
                    work->field_0 = 0;
                    break;
                case 2:
                    sound = (((u16)ctx->field_8 >> 0xC) << 8) | 0x52160009;
                    pan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
                    SndEvt_EnqueueType6(sound, pan,
                                        (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
                    work->field_82E = sub;
                    work->field_828 = sub;
                    Actor00100_Fn02788(arg0);
                    Actor00100_Fn02788(arg0);
                    work->field_0   = 0x1C;
                    work->field_C1E = Actor00100_D0BDB4.field_1A;
                    work->field_C20 = Actor00100_D0BDB4.field_18;
                    work->field_C22 = Actor00100_D0BDB4.field_1C;
                    work->field_C24 = Actor00100_D0BDB4.field_1E;
                    break;
            }
        }
        if (arg2->field_0.word == 0x202) {
            req = arg2->field_2.word;
            switch (req) {
                case 0:
                    work->field_0 = 0;
                block_46:
                    return 0;
                case 2:
                    work->field_0                       = 0x26;
                    arg0->field_2C->field_8->coord.t[0] = -0x896;
                    arg0->field_2C->field_8->coord.t[1] = 0;
                    arg0->field_2C->field_8->coord.t[2] = 0x5AF;
                    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, -0x3F4, 1);
                    goto block_46;
                default:
                    return 0;
            }
        } else {
            return 0;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_anim", Actor00100_Fn01388);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_anim", Actor00100_Fn01900);

void Actor00100_Fn01D74(Actor00100* arg0)
{
    GpAnimPose pose;
    GpAnimPose otherPose;
    GpAnimCtx* anim;
    s16        part;
    s16        index;
    s16        next;
    s32        blend;
    s32        invBlend;
    s32        offset;
    u8*        work;
    u8*        slotBase;

    index = 1;
    work  = (u8*)arg0->field_1C;
    anim  = (GpAnimCtx*)(work + 0x1C);
    do {
        part = index - 1;
        switch (part) {
            case 0:
                blend = 0xC00;
                break;
            case 1:
                blend = 0xC00;
                break;
            case 2:
                blend = 0xC00;
                break;
            case 3:
                blend = 0x5DE;
                break;
            case 4:
                blend = 0x5DE;
                break;
            default:
                blend = 0xBD0;
                break;
        }
        invBlend = 0x1000 - blend;
        if (index < 0xB) {
            slotBase                 = work + (index * 0x28);
            *(u8*)(slotBase + 0x43D) = (u8)((Actor00100Work*)work)->field_83A;
            *(s8*)(slotBase + 0x39)  = (s8)(((Actor00100Work*)work)->field_832 - 3);
            func_800B3448(anim, (s32)index, (s32)&pose, 0);
            func_800B3448((GpAnimCtx*)(work + 0x420), (s32)index, (s32)&otherPose, 0);
            Gp_AnimWritePoseCopy(anim, (s32)index, &pose, &otherPose, blend, invBlend);
            next = index + 1;
        } else {
            offset                       = index * 0x28;
            *(s8*)(work + offset + 0x39) = (s8)(((Actor00100Work*)work)->field_832 - 3);
            Gp_AnimTickIndex((GpAnimCtx*)(work + 0x1C), (s32)index);
            next = index + 1;
        }
        index = next;
        /* Keep the next-index value separate from the copied loop index. */
        SOFT_TOUCH_REG(next);
    } while (next < 0x12);
}
