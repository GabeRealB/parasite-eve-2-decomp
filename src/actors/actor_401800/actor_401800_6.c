#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include "psyq/abs.h"

#include "actors/actor_401800.h"
#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

/// 0x1C-byte scratch `func_actor_401800_80133918` takes from `G_SCRATCH_HEAD`:
/// `local` takes each root translation raised by 1000, rotated into `out` (the
/// slot-3 player) and `from` (this actor); `hit` is `func_800E0308`'s result.
typedef struct Actor401800SightScratch {
    /* 0x00 */ SVECTOR out;
    /* 0x08 */ SVECTOR from;
    /* 0x10 */ SVECTOR local;
    /* 0x18 */ s32     hit;
} Actor401800SightScratch;
STATIC_ASSERT_SIZEOF(Actor401800SightScratch, 0x1C);

/// Line-of-sight test between the slot-3 player and this actor: both root
/// positions, raised by 1000, are rotated into world space and handed to
/// `func_800E0308`, whose result is returned.
s32 func_actor_401800_80133918(Actor401800* arg0)
{
    Task*                    player;
    u8*                      head;
    Actor401800SightScratch* s;
    SVECTOR*                 local;
    SVECTOR*                 v;
    SVECTOR*                 out;

    player                = gameGetPtrSlot(3);
    head                  = *(u8**)G_SCRATCH_HEAD;
    local                 = (SVECTOR*)(head - 0xC);
    s                     = (Actor401800SightScratch*)(head - 0x1C);
    s->local.vx           = ((Actor401800*)player)->field_2C->coords->coord.t[0];
    s->local.vy           = ((Actor401800*)player)->field_2C->coords->coord.t[1] - 1000;
    *(u8**)G_SCRATCH_HEAD = (u8*)s;
    s->local.vz           = ((Actor401800*)player)->field_2C->coords->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    v = local;
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(&s->out);
    s->out.vx += gGfxViewCoord.workm.t[0];
    s->out.vy += gGfxViewCoord.workm.t[1];
    s->out.vz += gGfxViewCoord.workm.t[2];

    s->local.vx = arg0->field_2C->coords->coord.t[0];
    s->local.vy = arg0->field_2C->coords->coord.t[1] - 1000;
    s->local.vz = arg0->field_2C->coords->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    out = (SVECTOR*)(head - 0x14);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(out);
    s->from.vx           += gGfxViewCoord.workm.t[0];
    s->from.vy           += gGfxViewCoord.workm.t[1];
    s->from.vz           += gGfxViewCoord.workm.t[2];
    s->hit                = func_800E0308(&s->out, out);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x1C;
    return s->hit;
}

/// Animation-event dispatcher: picks the `0x400A00xx` event the state in
/// `field_89E` and the animation index in `field_5A & 0x3FF` queue, one event
/// per index change because `field_8B4` latches the index the last one fired
/// for — a repeat of that index just stores it back. A state/index pair with
/// no event falls through to the shared tail, which latches the index, or
/// clears `field_8B4` outright in the states that need it. Same dispatcher
/// shape as `Actor01900_Fn01A7C`.
s32 func_actor_401800_80133B78(Actor401800Work* work)
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
            if (id == 0x10) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0002;
                }
                work->field_8B4 = id;
            } else if (id == 0x16) {
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
            if (id == 0xA && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0004;
            }
            id = work->field_5A & 0x3FF;
            if (id == 0x12 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0002;
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
            if (id == 9 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A000D;
            }
            id = work->field_5A & 0x3FF;
            if (id == 0x13 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A000C;
            }
            prev            = work->field_5A & 0x3FF;
            work->field_8B4 = prev;
            break;
    }
    return 0;
}
