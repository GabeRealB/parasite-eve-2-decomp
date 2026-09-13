#include "common.h"
#include "actors/actor_400100.h"
#include "gameplay/1BC.h"

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_anim", Actor00100_Fn001FC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_anim", Actor00100_Fn00508);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_anim", Actor00100_Fn00A54);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_anim", Actor00100_Fn00BF8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_400100_anim", Actor00100_Fn00E58);

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
