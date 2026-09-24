#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b2_operating_room.h"

extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115754;

extern SVECTOR D_shelter_b2_operating_room_801809BC[];
extern SVECTOR D_shelter_b2_operating_room_80180ABC[];
extern SVECTOR D_shelter_b2_operating_room_80180ADC[];
extern SVECTOR D_shelter_b2_operating_room_80180B44[];
extern SVECTOR D_shelter_b2_operating_room_80180B5C[];
extern SVECTOR D_shelter_b2_operating_room_80180B6C[];

void func_shelter_b2_operating_room_8017DDB8(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115734  = 0x60222;
        D_80115730  = 0x6022D;
        D_80115754  = 0x60238;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B6C[0], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B6C[4], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B6C[8], 0x100, 0x444);
            break;
        case 3:
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B5C[0], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B5C[8], 0x100, 0x444);
            break;
        case 4:
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[0], 0x380, 0x444);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-48], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-44], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-41], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-38], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-37], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-36], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-35], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-34], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-31], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-30], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-27], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-26], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-23], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-22], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-21], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B44[-7], 0x100, 0x400);
            break;
        case 5:
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[0], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[6], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[8], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[9], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[10], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[13], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[14], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[15], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[16], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[17], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[20], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[21], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[24], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[25], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[29], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[30], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[31], 0x200, 0x400);
            break;
        case 6:
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ABC[0], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ABC[2], 0x100, 0x444);
            break;
        case 7:
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ADC[0], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ADC[2], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ADC[4], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ADC[8], 0x100, 0x444);
            break;
    }
}
