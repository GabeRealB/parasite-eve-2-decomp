#ifndef ACTOR_401300_H
#define ACTOR_401300_H

#include "common.h"

#include "actors/actors_shared_80169f74.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Private work block of the actor 401300 task, hanging off `Task::idMap`.
///
/// Only the fields the matched code touches are named so far: `yaw` at 0x18
/// (the heading `func_actor_401300_80141614` reads back from the root
/// coordinate, one halfword later than `ActorsShared80169f74Work::yaw`), the
/// three `GpObj` display nodes `func_actor_401300_80141758` hands back to
/// `Gp_UnlinkObj`, the two child tasks it kills, and the halfword the
/// teardown-ish `func_actor_401300_80141EF8` tests before it stamps the enemy's
/// `field_40` with the -999 sentinel. The block is a good deal larger - sibling
/// `func_actor_401300_80141C88` reads animation state at 0x89C..0xC0E of the
/// same pointer - so the struct stays open-ended.
///
/// The display nodes do *not* sit at the same addresses as the same-shaped
/// teardown of actor 01900/401800, which keeps its three at 0x8C8/0xA08/0xB48.
/// Animation halfwords sit 4 bytes later than `Actor01900Work` (0x89C vs
/// 0x898); the three `GpObj` nodes sit 0xA8 later (0x970/0xAB0/0xBF0).
typedef struct Actor401300Work {
    /* 0x000 */ s16   field_0;
    /* 0x002 */ s16   field_2;
    /* 0x004 */ s16   field_4;
    /* 0x006 */ byte  pad_6[0x12];
    /* 0x018 */ s16   yaw;
    /* 0x01A */ byte  pad_1A[0x882];
    /* 0x89C */ s16   field_89C;
    /* 0x89E */ s16   field_89E;
    /* 0x8A0 */ byte  pad_8A0[2];
    /* 0x8A2 */ s16   field_8A2;
    /* 0x8A4 */ byte  pad_8A4[2];
    /* 0x8A6 */ s16   field_8A6;
    /* 0x8A8 */ byte  pad_8A8[0xC8];
    /* 0x970 */ GpObj field_970;
    /* 0x990 */ byte  pad_990[0x120];
    /* 0xAB0 */ GpObj field_AB0;
    /* 0xAD0 */ byte  pad_AD0[0x120];
    /* 0xBF0 */ GpObj field_BF0;
    /* 0xC10 */ byte  pad_C10[0x7A];
    /* 0xC8A */ s16   field_C8A;
    /* 0xC8C */ byte  pad_C8C[0x80];
    /// The two helper tasks killed before the nodes are unlinked; the same
    /// pair `Actor01900Work` keeps at +0xC38 / +0xC3C.
    /* 0xD0C */ Task* field_D0C;
    /* 0xD10 */ Task* field_D10;
} Actor401300Work;

/// Per-task actor context: `field_1C` is the work block (`Task::idMap`) and
/// `field_2C` is the actor's `TmdObject`. Same shape as `Actor01900`.
typedef struct Actor401300 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor401300Work* field_1C;
    /* 0x20 */ GpEnemy*         field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ TmdObject*       field_2C;
} Actor401300;

s32 func_actor_401300_80141614(Task* task, s32 arg1, ActorShared80169f74Placement* placement);

void func_actor_401300_80133A3C(Actor401300* arg0);

void func_actor_401300_80141758(Task* task);

void func_actor_401300_801419B8(Actor401300* arg0);

void func_actor_401300_80141EF8(Task* task);

#endif // ACTOR_401300_H
