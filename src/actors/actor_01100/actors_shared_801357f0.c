#include "common.h"

#include "actors/actors_shared_801357f0.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>

#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

void ActorsShared801357f0(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work)
{
    SVECTOR        local;
    GsCOORDINATE2* self;
    GsCOORDINATE2* other;
    MATRIX*        selfWorkm;
    s32            angle;
    s32            target;
    s16            cur;
    void*          head;
    void*          head2;
    s8*            deltaX;
    s32            otherY;
    s32            selfY;
    void*          vec;
    void*          matrix;

    self = ((TmdObject*)task->extra)->coords;
    if (Gp_ActorSlots[0] == NULL) {
        SCHED_BARRIER();
        angle = 0;
    } else {
        other     = Gp_ActorSlots[0]->extra->coords;
        selfWorkm = &self->workm;
        __asm__("lui %0, 0x1F80" : "=r"(head) : "r"(other));
        head   = *(void**)(head + 0x3FC);
        deltaX = (s8*)head - 0x40;
        vec    = head - 0x40;

        *(s16*)deltaX = (s16)(other->workm.t[0] - self->workm.t[0]);
        otherY        = (u16)other->workm.t[1];
        selfY         = (u16)self->workm.t[1];
        __asm__("addiu %0, %1, -0x20" : "=r"(matrix) : "r"(head), "r"(otherY), "r"(selfY));
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(vec) : "memory");
        *(s16*)((s8*)vec + 2) = (s16)(otherY - selfY);
        *(s16*)((s8*)vec + 4) = (s16)(other->workm.t[2] - self->workm.t[2]);
        TransposeMatrix(selfWorkm, matrix);

        local = *(SVECTOR*)vec;
        gte_SetRotMatrix(matrix);
        __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
        gte_rtv0_real();
        gte_stsv(vec);

        angle = ratan2(*(s16*)deltaX, *(s16*)((s8*)vec + 4));
        if (angle >= 0x801) {
            angle -= 0x1000;
        } else if (angle < -0x800) {
            angle += 0x1000;
        }
        __asm__("lui %0, 0x1F80" : "=r"(head2));
        head2 = *(void**)(head2 + 0x3FC);
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"((void*)((u8*)head2 + 0x40)) : "memory");
    }

    work->field_B90 = angle;
    angle           = (s16)angle;
    if (angle < -0x300) {
        angle = -0x300;
    } else if (angle >= 0x301) {
        angle = 0x300;
    }

    target = angle - 0xC0;
    cur    = work->field_B8E;
    if (cur < target) {
        work->field_B8E = (u16)work->field_B8E + 0xC0;
    } else if (angle + 0xC0 < cur) {
        work->field_B8E = (u16)work->field_B8E - 0xC0;
    } else {
        work->field_B8E = angle;
    }
}
