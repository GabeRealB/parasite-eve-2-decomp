#include "common.h"

#include <psyq/libgte.h>

extern s32 Gp_GetViewIndex(void);
void       Room_Draw01(SVECTOR* v, s32 arg1, s32 arg2);
void       Room_Draw05(SVECTOR* v, s32 arg1, s32 arg2);
void       Room_Draw18(SVECTOR* v, s32 arg1, s32 arg2);

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
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181868[0], 0x180, 0x111);
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181868[2], 0x180, 0x111);
            break;
        case 4:
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181888[0], 0x180, 0x111);
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181888[4], 0x180, 0x111);
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181888[8], 0x180, 0x111);
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181888[10], 0x180, 0x111);
            Room_Draw18(&D_shelter_b3_incinerator_control_room_80181888[18], 0x60, 0x80);
            break;
        case 5:
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181888[0], 0x180, 0x111);
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181888[4], 0x180, 0x111);
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181888[2], 0x180, 0x111);
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181888[6], 0x180, 0x111);
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181888[12], 0x180, 0x111);
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181888[14], 0x180, 0x111);
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181888[16], 0x180, 0x421);
            break;
        case 6:
            Room_Draw01(&D_shelter_b3_incinerator_control_room_801818E8[0], 0x180, 0x111);
            Room_Draw01(&D_shelter_b3_incinerator_control_room_801818E8[2], 0x180, 0x111);
            break;
        case 8:
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181888[0], 0x180, 0x111);
            Room_Draw01(&D_shelter_b3_incinerator_control_room_80181888[4], 0x180, 0x111);
            Room_Draw05(&D_shelter_b3_incinerator_control_room_80181888[18], 0x60, 0x80);
            break;
    }
}
