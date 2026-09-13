#include "common.h"
#include "gameplay/3A34.h"
#include "main/mem.h"

s32 Actor00300_Fn04B14(SVECTOR* arg0, SVECTOR* arg1)
{
    void**   scratch;
    u8*      head;
    VECTOR*  vec;
    GpObj3A* node;
    s32      ret;

    ret                          = 0;
    scratch                      = (void**)G_SCRATCH_HEAD;
    node                         = D_80115550;
    head                         = *scratch;
    ((VECTOR*)(head - 0x10))->vx = arg1->vx - arg0->vx;
    head                         = head - 0x10;
    TOUCH_REG_USE(head, node);
    vec      = (VECTOR*)head;
    vec->vy  = arg1->vy - arg0->vy;
    *scratch = vec;
    vec->vz  = arg1->vz - arg0->vz;
    VectorNormal(vec, vec);
    for (; node != NULL; node = node->next) {
        if (node->field_3A & 0x40) {
            ret = func_800DFCCC(node, arg0, arg1, vec);
            if (ret == 1) {
                break;
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
    return ret;
}
