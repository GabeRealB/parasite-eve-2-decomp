#include "common.h"

#include <psyq/inline_c.h>
#include "psyq/abs.h"

#include "actors/actor_401800.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_5", func_actor_401800_80133558);

/// Per-frame animation tick: walk the actor's pose slots, copy each slot's
/// state byte down by three and, for the first ten slots, blend the pose pair
/// `func_800B3448` builds out of the pose and blend contexts — weighted by
/// `field_8AC` against its `0x1000` complement. Slots `0xB` and up only carry
/// the state byte and are advanced by `Gp_AnimTickIndex`.
/// Same body as `func_actor_401000_80132A84`.
void func_actor_401800_801337EC(Actor401800* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor401800AnimWork* work;

    work   = (Actor401800AnimWork*)arg0->field_1C;
    weight = work->field_8AC;
    anim   = &work->anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].rate = (u8)work->field_8AA;
            work->slots[i].rate      = (u8)(work->field_8A2 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].rate = (u8)(work->field_8A2 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_5", func_actor_401800_80133918);

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
