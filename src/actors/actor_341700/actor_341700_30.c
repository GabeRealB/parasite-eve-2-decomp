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

void func_actor_341700_80168AC0(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    work->field_44F = D_actor_341700_80174D88[work->field_418 - 1];
    if (work->field_44F == 1) {
        Actor341700Work* w = (Actor341700Work*)arg0->work;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 6;
        w->field_414 = 1;
    } else {
        Actor341700Work* w = (Actor341700Work*)arg0->work;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 5;
        w->field_414 = 1;
    }
    work->field_422++;
}
