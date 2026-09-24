#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_1f_vehicular_airlock.h"

extern s32     D_8011572C;
extern s32     D_80115750;
extern s32     D_80115758;
extern SVECTOR D_shelter_1f_vehicular_airlock_8018205C[];
extern SVECTOR D_shelter_1f_vehicular_airlock_8018206C[];

void func_shelter_1f_vehicular_airlock_8017E80C(SVECTOR* v, s32 arg1, s32 arg2, s32 arg3);

void func_shelter_1f_vehicular_airlock_8017DAA0(Task* task)
{
    u8       view;
    SVECTOR* p;

    if (task->state == 0) {
        D_80115758  = 0x601D7;
        D_8011572C  = 0x601F3;
        D_80115750  = 0x6020F;
        task->state = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            p = D_shelter_1f_vehicular_airlock_8018206C;
            SOFT_TOUCH_REG(p);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[0], 0x200, 0x800, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[2], 0x200, 0x800, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[6], 0x200, 0, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[8], 0x200, 0, 0x210);
            func_shelter_1f_vehicular_airlock_8017E468(&p[12], 0x200, 0x200);
            break;
        case 3:
            p = D_shelter_1f_vehicular_airlock_8018205C;
            func_shelter_1f_vehicular_airlock_8017DC80(&p[0], 0x200, 0x800, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[2], 0x200, 0x800, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[6], 0x200, 0, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[8], 0x200, 0, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[12], 0x200, 0, 0x111);
            if (GameFlag_GetNibble(0xB2) == 1) {
                func_shelter_1f_vehicular_airlock_8017E80C(&p[15], 0x804, 0x140, 0x21);
                func_shelter_1f_vehicular_airlock_8017E80C(&p[16], 0xC0, 0x120, 0x210);
                func_shelter_1f_vehicular_airlock_8017E80C(&p[17], -0xC0, 0x120, 0x210);
            } else {
                func_shelter_1f_vehicular_airlock_8017E468(&p[15], 0x180, 0x21);
                func_shelter_1f_vehicular_airlock_8017E468(&p[16], 0x140, 0x210);
                func_shelter_1f_vehicular_airlock_8017E468(&p[17], 0x140, 0x210);
            }
            break;
    }
}
