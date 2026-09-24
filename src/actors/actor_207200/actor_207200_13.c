#include "common.h"
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_207200.h"

/// Measures the model held in pointer slot 3 from coordinate `arg0`: returns
/// the heading to it in `arg0`'s own frame, folded into -0x800..0x800, and
/// stores the horizontal world distance in `*arg1`. The offset is staged in
/// 0x40 bytes of the scratch stack.
///
/// `base` and `vec` hold the same address on purpose: the explicit `move`
/// reproduces the original's `addiu`/`addu` pair, and the head-relative
/// spelling of the vector copy and of the reloaded x keeps the scratch
/// pointer in `head`'s register. `COMPILER_BARRIER` stops cse from forwarding
/// the reload of x from the store just above it.
s32 func_actor_207200_8014CE20(GsCOORDINATE2* arg0, u32* arg1)
{
    SVECTOR        local;
    GsCOORDINATE2* coord;
    s32            angle;
    s32            x;
    s16            z;
    void*          base;
    void*          head;
    void*          vec;
    void*          matrix;

    coord = ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords;
    head  = *(void**)0x1F8003FC;
    base  = head - 0x40;
    __asm__("move %0,%1" : "=r"(vec) : "r"(base));
    *(s16*)((s8*)base + 0) = (s16)(coord->workm.t[0] - arg0->workm.t[0]);
    *(s16*)((s8*)vec + 2)  = (s16)(coord->workm.t[1] - arg0->workm.t[1]);
    *(void**)0x1F8003FC    = vec;
    *(s16*)((s8*)vec + 4)  = (s16)(coord->workm.t[2] - arg0->workm.t[2]);
    matrix                 = head - 0x20;
    TransposeMatrix(&arg0->workm, matrix);
    local = *(SVECTOR*)((s8*)head - 0x40);
    gte_SetRotMatrix(matrix);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    gte_rtv0();
    gte_stsv(vec);
    angle = ratan2(*(s16*)((s8*)head - 0x40), *(s16*)((s8*)vec + 4));
    if (angle >= 0x801) {
        angle -= 0x1000;
    } else if (angle < -0x800) {
        angle += 0x1000;
    }
    *(s16*)((s8*)vec + 0) = (s16)(coord->coord.t[0] - arg0->coord.t[0]);
    COMPILER_BARRIER();
    x                     = *(s16*)((s8*)vec + 0);
    z                     = coord->coord.t[2] - arg0->coord.t[2];
    *(s16*)((s8*)vec + 4) = z;
    *arg1                 = SquareRoot0((x * x) + (z * z));
    *(void**)0x1F8003FC  += 0x40;
    return angle;
}
