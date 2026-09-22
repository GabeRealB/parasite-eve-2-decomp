#include "common.h"

#include "actors/actors_shared_80138774.h"
#include "psyq/inline_c.h"

#define gte_mvmva_10030() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Angle from `arg0`'s coordinate to the coordinate at
/// `Gp_ActorSlots[arg1]->extra->field_8`, measured in `arg0`'s own frame. The
/// three translation components, target minus self, are written into the
/// 8-byte scratch vector at `head - 0x40`, `TransposeMatrix` builds the inverse
/// of `arg0->workm` into the 0x20 bytes above it, `mvmva` rotates the delta
/// through that inverse and the three results are read back over the vector;
/// `ratan2` of its X and Z then gives the angle, wrapped into (-0x800, 0x800].
/// The 0x40-byte scratch block is handed back before returning.
///
/// The scratch pointer is published between the second and third halfword
/// stores rather than after all three: consecutive stores carry an output
/// dependency, so their program order survives both schedulers, and moving it
/// later lets it take the call's delay slot instead of the third store.
s32 ActorsShared80138774(GsCOORDINATE2* arg0, s32 arg1)
{
    SVECTOR        local;
    GpActorWork*   actor;
    GsCOORDINATE2* coord;
    s32            angle;
    s32            result;
    void*          head;
    void*          vec;
    void*          matrix;

    actor = Gp_ActorSlots[arg1];
    if (actor == NULL) {
        return 0;
    }
    coord = actor->extra->coords;
    head  = *(void**)0x1F8003FC;
    vec   = head - 0x40;

    *(s16*)((s8*)head - 0x40) = (s16)(coord->workm.t[0] - arg0->workm.t[0]);
    *(s16*)((s8*)vec + 2)     = (s16)(coord->workm.t[1] - arg0->workm.t[1]);
    *(void**)0x1F8003FC       = vec;
    *(s16*)((s8*)vec + 4)     = (s16)(coord->workm.t[2] - arg0->workm.t[2]);

    matrix = head - 0x20;
    TransposeMatrix(&arg0->workm, matrix);

    local = *(SVECTOR*)vec;
    gte_SetRotMatrix(matrix);
    __asm__ volatile("addiu $2, $sp, 0x10; lwc2 $0, 0($2); lwc2 $1, 4($2)");
    gte_mvmva_10030();
    gte_stsv(vec);

    angle  = ratan2(*(s16*)((s8*)head - 0x40), *(s16*)((s8*)vec + 4));
    result = angle;
    if (angle >= 0x801) {
        result = angle - 0x1000;
    } else if (angle < -0x800) {
        result = angle + 0x1000;
    }
    *(void**)0x1F8003FC = (void*)((u8*)*(void**)0x1F8003FC + 0x40);
    return result;
}
