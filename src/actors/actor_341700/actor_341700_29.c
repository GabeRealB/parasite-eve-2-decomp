#include "common.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

void func_actor_341700_801651E0(Task* arg0);
void func_actor_341700_80168A14(Task* arg0);
void func_actor_341700_80168A48(Task* arg0);
void func_actor_341700_801697B8(Task* arg0);
void func_actor_341700_801697D4(Task* arg0);
void func_actor_341700_80169888(Task* arg0);
void func_actor_341700_8016999C(Task* arg0);
void func_actor_341700_80169AB0(Task* arg0);

/* This one, by contrast, is a normal prototyped call: the redundant
 * `move $a0, $s0` is deleted again after reload because `$a0` still holds the
 * caller's own `arg0`. */
s32 func_actor_341700_80168234(Task* arg0);

void func_actor_341700_80168C4C(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->work;
    s16              angle;
    s16              speed;

    func_actor_341700_801685F0(arg0, 0x10);
    speed                                          = func_actor_341700_80168444(arg0, -0x10);
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    if (func_actor_341700_80168468(arg0)) {
        Actor341700Work* next = (Actor341700Work*)arg0->work;

        next->field_420 = 4;
        next->field_422 = 0;
    }
}
