#include "common.h"
#include "main/task.h"
#include "main/tmd.h"
#include "actors/actor_342400.h"
#include "actors/actors_shared_8016974c.h"
#include "actors/actors_shared_801698d4.h"
#include "actors/actors_shared_8016bef0.h"

void func_actor_342400_801664C4(Task* arg0);
void func_actor_342400_80169CF8(Task* arg0);
void func_actor_342400_80169D2C(Task* arg0);
void func_actor_342400_8016AA9C(Task* arg0);
void func_actor_342400_8016AAB8(Task* arg0);
void func_actor_342400_8016AB6C(Task* arg0);
void func_actor_342400_8016AC80(Task* arg0);
void func_actor_342400_8016AD94(Task* arg0);

void func_actor_342400_80169F30(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->work;
    s16              angle;
    s16              speed;

    ActorsShared801698d4(arg0, 0x10);
    speed                                          = func_actor_342400_80169728(arg0, -0x10);
    angle                                          = work->field_7A;
    ((TmdObject*)arg0->extra)->coords->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->coords->flg         = 0;
    if (ActorsShared8016974c(arg0)) {
        Actor342400Work* next = (Actor342400Work*)arg0->work;

        next->field_420 = 4;
        next->field_422 = 0;
    }
}
