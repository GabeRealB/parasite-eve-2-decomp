#include "common.h"

#include "actors/actor_510900.h"

void func_actor_510900_80135E90(void);
void func_actor_510900_80136184(void);
void func_actor_510900_80136B70(void);
void func_actor_510900_80137008(void);
void func_actor_510900_801373B8(void);
void func_actor_510900_801375D8(void);
void func_actor_510900_80137868(void);
void func_actor_510900_80137E20(void);
void func_actor_510900_80137FBC(void);
void func_actor_510900_80138250(void);
void func_actor_510900_801384C4(void);
void func_actor_510900_8013B988(void);
void func_actor_510900_8013BA58(void);

void func_actor_510900_8013B870(Actor510900* arg0)
{
    s16 temp_v1;

    temp_v1 = arg0->field_1C->field_58E;
    switch (temp_v1) {
        case 0:
            func_actor_510900_80135E90();
            return;
        case 1:
            func_actor_510900_80136184();
            return;
        case 2:
            func_actor_510900_80136B70();
            return;
        case 3:
            func_actor_510900_80137008();
            return;
        case 4:
            func_actor_510900_801373B8();
            return;
        case 5:
            func_actor_510900_801375D8();
            return;
        case 6:
            func_actor_510900_80137868();
            return;
        case 7:
            func_actor_510900_8013B988();
            return;
        case 8:
            func_actor_510900_8013BA58();
            return;
        case 9:
            func_actor_510900_80137E20();
            return;
        case 10:
            func_actor_510900_80137FBC();
            return;
        case 11:
            func_actor_510900_80138250();
            return;
        case 12:
            func_actor_510900_801384C4();
        default:
            return;
    }
}
