#include "common.h"
#include "main/task.h"
#include "actors/actor_342400.h"

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_20", func_actor_342400_8016A664);

void func_actor_342400_8016A724(Task* arg0)
{
    Actor342400Work* work;
    Actor342400Work* work2;
    Actor342400Work* work3;
    Actor342400Work* work4;
    s16              anim;
    s16              next;

    work = (Actor342400Work*)arg0->idMap;
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    anim = work->field_418;
    if (anim == 8) {
        if (work->field_440 == 0) {
            work2            = (Actor342400Work*)arg0->idMap;
            work2->field_426 = 4;
            work2->field_41C = 0x10;
            work2->field_418 = 5;
            work2->field_414 = 1;
        } else {
            work3            = (Actor342400Work*)arg0->idMap;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 6;
            work3->field_414 = 1;
        }
    } else {
        next             = D_actor_342400_80173A98[anim - 1];
        work4            = (Actor342400Work*)arg0->idMap;
        work4->field_426 = 4;
        work4->field_41C = 0x10;
        work4->field_418 = next;
        work4->field_414 = 1;
    }
    func_actor_342400_80165CC0(arg0);
    work->field_420++;
}

INCLUDE_ASM("actors/nonmatchings/actor_342400/actor_342400_20", func_actor_342400_8016A804);
