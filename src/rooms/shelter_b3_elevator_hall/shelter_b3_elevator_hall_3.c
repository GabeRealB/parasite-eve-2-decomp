#include "common.h"

#include "gameplay/D4.h"

#include "rooms/room_common.h"

#include "main/task.h"

extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_8011573C;
extern s32 D_80115744;
extern s32 D_80115754;

extern SVECTOR D_shelter_b3_elevator_hall_80182A74[];
extern SVECTOR D_shelter_b3_elevator_hall_80182AB4[];
extern SVECTOR D_shelter_b3_elevator_hall_80182AF4[];

void func_shelter_b3_elevator_hall_8017DE70(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x6024D;
        D_80115744  = 0x60259;
        D_8011573C  = 0x60264;
        D_80115720  = 0x60270;
        D_80115734  = 0x60223;
        D_80115730  = 0x6022E;
        D_80115754  = 0x60239;
        arg0->state = 1;
    }
    switch ((u8)Gp_GetViewIndex()) {
        case 3: {
            SVECTOR* p = D_shelter_b3_elevator_hall_80182A74;
            Room_Draw01(&p[0], 0x180, 0x222);
            Room_Draw01(&p[2], 0x180, 0x222);
            Room_Draw01(&p[4], 0x180, 0x222);
            Room_Draw01(&p[6], 0x180, 0x222);
        } break;
        case 4: {
            SVECTOR* p = D_shelter_b3_elevator_hall_80182AB4;
            Room_Draw01(&p[0], 0x180, 0x222);
            Room_Draw01(&p[2], 0x180, 0x222);
            Room_Draw01(&p[4], 0x180, 0x222);
            Room_Draw01(&p[6], 0x180, 0x222);
        } break;
        case 6: {
            SVECTOR* p = D_shelter_b3_elevator_hall_80182AF4;
            Room_Draw01(&p[0], 0x180, 0x222);
            Room_Draw01(&p[2], 0x180, 0x222);
            Room_Draw01(&p[4], 0x180, 0x222);
            Room_Draw01(&p[6], 0x180, 0x222);
        } break;
    }
}
