#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/abs.h>

#include "actors/actor_401000.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/wipsys.h"

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// `Actor401300_MoveForward`: the plain forward-step helper, the shape the
/// state-8 body `func_actor_401000_801388F4` inlines. The `Nonzero` variant
/// below is the same block with an `amount != 0` test and a `gteVec` copy
/// wrapped around it.
static __inline__ void Actor401000_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12_real();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// `Actor401300_MoveForwardNonzero` and `Actor00100_MoveForwardNonzero`, down
/// to the `head[-1].vx` read-back; the step lands in `coord` directly rather
/// than being reported back through the caller's local.
static __inline__ void Actor401000_MoveForwardNonzero(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        gteVec                     = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12_real();
            gte_stsv(gteVec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801323EC);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132590);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132824);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132A84);

s32 func_actor_401000_80132BB0(Actor401000Work* work)
{
    s32 id;
    s32 prev;

    switch (work->field_89E) {
        case 20:
        case 21:
            id = work->field_5A & 0x3FF;
            if (id == 7) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0010;
                }
                work->field_8B4 = id;
            } else if (id == 0x10) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0011;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 3:
            id = work->field_5A & 0x3FF;
            if (id == 0x1A) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0004;
                }
                work->field_8B4 = id;
            } else if (id == 0x13) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0003;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 2:
            id = work->field_5A & 0x3FF;
            if (id == 0x11) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0002;
                }
                work->field_8B4 = id;
            } else if (id == 0x1B) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0001;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 9:
            id = work->field_5A & 0x3FF;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0006;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 11:
            id = work->field_5A & 0x3FF;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0005;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 12:
            id = work->field_5A & 0x3FF;
            if (id == 7 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0005;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 4:
            id = work->field_5A & 0x3FF;
            if (id == 0xA) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0004;
                }
            }
            id = work->field_5A & 0x3FF;
            if (id == 0x12) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0002;
                }
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 5:
            id = work->field_5A & 0x3FF;
            if (id == 9 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A000D;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 7:
            id = work->field_5A & 0x3FF;
            if (id == 0x16 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0003;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
        case 6:
            id = work->field_5A & 0x3FF;
            if (id == 9) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A000D;
                }
            }
            id = work->field_5A & 0x3FF;
            if (id == 0x13) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A000C;
                }
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
    }
    return 0;
}

// Animation driver is linked from actors_shared_80133eb8.c.
