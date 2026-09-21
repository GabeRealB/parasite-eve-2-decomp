#include "common.h"

#include "actors/actors_shared_801345fc.h"
#include "actors/actors_shared_801359cc.h"
#include "actors/actors_shared_80138efc.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"

#include <psyq/inline_c.h>

extern u32 Gp_LcgState;

/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix with no translation
/// vector added. The `inline_c.h` macro of that name assembles to a different
/// word, so spell the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

void ActorsShared801359cc(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work)
{
    SVECTOR        local;
    GsCOORDINATE2* self0;
    GsCOORDINATE2* self;
    GsCOORDINATE2* other;
    GsCOORDINATE2* self2;
    TmdObject*     playerObj;
    MATRIX*        selfWorkm;
    Task*          player;
    SVECTOR*       sv;
    s32            angle;
    s32            wrapped;
    s32            angle3;
    s32            target;
    s32            yaw1;
    s32            yaw;
    s32            dist;
    s32            otherY;
    s32            selfY;
    s32            state;
    s16            cur;
    s16            next;
    s8             latch;
    u16            timer;
    s32            t;
    s32            xSelf;
    s32            xOther;
    void*          head0;
    void*          head3;
    void*          pop0;
    void*          pop3;
    void*          pop8;
    s8*            deltaX0;
    s8*            deltaX3;
    void*          vec0;
    void*          vec3;
    void*          matrix0;
    void*          matrix3;

    if (work->field_BA8 == 0) {
        work->field_BA4 = 1;
        self0           = ((TmdObject*)task->extra)->coords;
        if (Gp_ActorSlots[0] == NULL) {
            SCHED_BARRIER();
            angle = 0;
        } else {
            other     = Gp_ActorSlots[0]->extra->coords;
            selfWorkm = &self0->workm;
            __asm__("lui %0, 0x1F80" : "=r"(head0) : "r"(other));
            head0   = *(void**)(head0 + 0x3FC);
            deltaX0 = (s8*)head0 - 0x40;
            vec0    = head0 - 0x40;

            *(s16*)deltaX0 = (s16)(other->workm.t[0] - self0->workm.t[0]);
            otherY         = (u16)other->workm.t[1];
            selfY          = (u16)self0->workm.t[1];
            __asm__("addiu %0, %1, -0x20" : "=r"(matrix0) : "r"(head0), "r"(otherY), "r"(selfY));
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(vec0) : "memory");
            *(s16*)((s8*)vec0 + 2) = (s16)(otherY - selfY);
            *(s16*)((s8*)vec0 + 4) = (s16)(other->workm.t[2] - self0->workm.t[2]);
            TransposeMatrix(selfWorkm, matrix0);

            local = *(SVECTOR*)vec0;
            gte_SetRotMatrix(matrix0);
            __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
            gte_rtv0_real();
            gte_stsv(vec0);

            wrapped = ratan2(*(s16*)deltaX0, *(s16*)((s8*)vec0 + 4));
            if (wrapped >= 0x801) {
                wrapped -= 0x1000;
            } else if (wrapped < -0x800) {
                wrapped += 0x1000;
            }
            __asm__("lui %0, 0x1F80" : "=r"(pop0));
            pop0  = *(void**)(pop0 + 0x3FC);
            angle = wrapped;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"((void*)((u8*)pop0 + 0x40)) : "memory");
        }
        work->field_B90 = angle;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_B8C = ((Gp_LcgState >> 0x10) & 0x1F) + 2;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }

    latch = work->field_BA8;
    if (latch == 1) {
        yaw1 = work->field_B90;
        if (yaw1 < -0x600) {
            yaw1 = -0x600;
        } else if (yaw1 >= 0x601) {
            yaw1 = 0x600;
        }
        cur = work->field_B8E;
        if (cur < yaw1) {
            next            = (u16)work->field_B8E + 0xC0;
            work->field_B8E = next;
            if (yaw1 < next) {
                work->field_B8E = yaw1;
            }
        } else if (yaw1 < cur) {
            next            = (u16)work->field_B8E - 0xC0;
            work->field_B8E = next;
            if (next < yaw1) {
                work->field_B8E = yaw1;
            }
        } else {
            Gp_ArmStateF0(1);
            work->field_BA8 = (u8)work->field_BA8 + 1;
        }
    } else if (latch == 2) {
        timer           = (u16)work->field_B8C - 1;
        work->field_B8C = timer;
        if ((s16)timer < 0) {
            work->field_BA4 = 4;
            work->field_BA8 = (u8)work->field_BA8 + 1;
        }
    }

    if (work->field_BA8 == 3) {
        self = ((TmdObject*)task->extra)->coords;
        if (Gp_ActorSlots[0] == NULL) {
            SCHED_BARRIER();
            angle3 = 0;
        } else {
            other     = Gp_ActorSlots[0]->extra->coords;
            selfWorkm = &self->workm;
            __asm__("lui %0, 0x1F80" : "=r"(head3) : "r"(other));
            head3   = *(void**)(head3 + 0x3FC);
            deltaX3 = (s8*)head3 - 0x40;
            vec3    = head3 - 0x40;

            *(s16*)deltaX3 = (s16)(other->workm.t[0] - self->workm.t[0]);
            otherY         = (u16)other->workm.t[1];
            selfY          = (u16)self->workm.t[1];
            __asm__("addiu %0, %1, -0x20" : "=r"(matrix3) : "r"(head3), "r"(otherY), "r"(selfY));
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(vec3) : "memory");
            *(s16*)((s8*)vec3 + 2) = (s16)(otherY - selfY);
            *(s16*)((s8*)vec3 + 4) = (s16)(other->workm.t[2] - self->workm.t[2]);
            TransposeMatrix(selfWorkm, matrix3);

            local = *(SVECTOR*)vec3;
            gte_SetRotMatrix(matrix3);
            __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
            gte_rtv0_real();
            gte_stsv(vec3);

            angle3 = ratan2(*(s16*)deltaX3, *(s16*)((s8*)vec3 + 4));
            if (angle3 >= 0x801) {
                angle3 -= 0x1000;
            } else if (angle3 < -0x800) {
                angle3 += 0x1000;
            }
            __asm__("lui %0, 0x1F80" : "=r"(pop3));
            pop3 = *(void**)(pop3 + 0x3FC);
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"((void*)((u8*)pop3 + 0x40)) : "memory");
        }

        yaw             = (s16)angle3;
        work->field_B90 = angle3;
        if (yaw < -0x600) {
            yaw = -0x600;
        } else if (yaw >= 0x601) {
            yaw = 0x600;
        }
        target = yaw - 0xC0;
        cur    = work->field_B8E;
        if (cur < target) {
            work->field_B8E = (u16)work->field_B8E + 0xC0;
        } else if (yaw + 0xC0 < cur) {
            work->field_B8E = (u16)work->field_B8E - 0xC0;
        } else {
            work->field_B8E = yaw;
        }

        yaw = work->field_B90;
        if (yaw >= 0x11) {
            ((GpCoordPose*)self)->field_46 = (u16)((GpCoordPose*)self)->field_46 + 0x10;
        } else if (yaw < -0x10) {
            ((GpCoordPose*)self)->field_46 = (u16)((GpCoordPose*)self)->field_46 - 0x10;
        } else {
            ((GpCoordPose*)self)->field_46 = (u16)((GpCoordPose*)self)->field_46 + yaw;
        }
        timer                          = (u16)((GpCoordPose*)self)->field_46 & 0xFFF;
        ((GpCoordPose*)self)->field_46 = timer;
        Gfx_RotMatrixY(&self->coord, timer, 1);
        self->flg = 0;

        if ((u32)(((u16)work->field_B90 + 0x7F) & 0xFFFF) < 0xFFU) {
            if (task->spawnArg1 != 0) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                t           = ((Gp_LcgState >> 0x10) % 3) & 0xFFFF;
                if (t <= 0) {
                    SCHED_BARRIER();
                    state = 0xC;
                } else if (t < 2) {
                    state = 0xD;
                } else {
                    SOFT_BARRIER();
                    state = 0xB;
                }
            } else {
                self2  = ((TmdObject*)task->extra)->coords;
                player = gameGetPtrSlot(3);
                if (player == NULL) {
                    dist = 0x7FFFFFFF;
                } else {
                    playerObj = (TmdObject*)player->extra;
                    TOUCH_REG(playerObj);
                    other  = playerObj->coords;
                    xSelf  = (u16)self2->workm.t[0];
                    xOther = (u16)other->workm.t[0];
                    __asm__("lui %0, 0x1F80" : "=r"(sv) : "r"(xSelf), "r"(xOther));
                    sv        = *(SVECTOR**)((u8*)sv + 0x3FC);
                    sv[-1].vx = (s16)(xOther - xSelf);
                    otherY    = (u16)other->workm.t[1];
                    selfY     = (u16)self2->workm.t[1];
                    __asm__("addiu %0, %1, -8" : "=r"(sv) : "r"(sv), "r"(otherY), "r"(selfY));
                    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(sv) : "memory");
                    sv->vy = (s16)(otherY - selfY);
                    sv->vz = (s16)((u16)other->workm.t[2] - (u16)self2->workm.t[2]);
                    dist   = Gfx_ApplyMatrixNoSf(sv, sv);
                    __asm__("lui %0, 0x1F80" : "=r"(pop8));
                    pop8 = *(void**)(pop8 + 0x3FC);
                    __asm__ volatile("sw %0, 0x1F8003FC" ::"r"((void*)((u8*)pop8 + 8)) : "memory");
                }
                if (dist <= 0xA62B0F) {
                    state = 0xD;
                    if (yaw < 0) {
                        state = 0xC;
                    }
                } else {
                    state = 0xE;
                }
            }
            work->state     = state;
            work->field_BA8 = 0;
        }
    }
}
