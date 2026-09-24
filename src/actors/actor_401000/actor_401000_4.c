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
