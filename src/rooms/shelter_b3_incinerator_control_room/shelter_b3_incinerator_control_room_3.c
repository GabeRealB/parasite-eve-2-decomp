#include "common.h"

#include <psyq/libgte.h>

#include "rooms/shelter_b3_incinerator_control_room.h"

extern s32 Gp_GetViewIndex(void);

extern SVECTOR D_shelter_b3_incinerator_control_room_80181868[];
extern SVECTOR D_shelter_b3_incinerator_control_room_80181888[];
extern SVECTOR D_shelter_b3_incinerator_control_room_801818E8[];

void func_shelter_b3_incinerator_control_room_8017FD10(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
        case 3:
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181868[0], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181868[2], 0x180, 0x111);
            break;
        case 4:
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[0], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[4], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[8], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[10], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_801806F8(&D_shelter_b3_incinerator_control_room_80181888[18], 0x60, 0x80);
            break;
        case 5:
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[0], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[4], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[2], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[6], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[12], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[14], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[16], 0x180, 0x421);
            break;
        case 6:
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_801818E8[0], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_801818E8[2], 0x180, 0x111);
            break;
        case 8:
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[0], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_8017FEB4(&D_shelter_b3_incinerator_control_room_80181888[4], 0x180, 0x111);
            func_shelter_b3_incinerator_control_room_80180B6C(&D_shelter_b3_incinerator_control_room_80181888[18], 0x60, 0x80);
            break;
    }
}
