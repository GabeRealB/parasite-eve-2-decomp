#include "common.h"

#include <psyq/libgte.h>

#include "main/session.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_trailer_coach/dryfield_night_trailer_coach_6", func_dryfield_night_trailer_coach_801826EC);

s32 func_dryfield_night_trailer_coach_80182800(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_trailer_coach/dryfield_night_trailer_coach_6", func_dryfield_night_trailer_coach_80182808);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_trailer_coach/dryfield_night_trailer_coach_6", func_dryfield_night_trailer_coach_8018283C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_trailer_coach/dryfield_night_trailer_coach_6", func_dryfield_night_trailer_coach_80182864);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_trailer_coach/dryfield_night_trailer_coach_6", func_dryfield_night_trailer_coach_80182898);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_trailer_coach/dryfield_night_trailer_coach_6", func_dryfield_night_trailer_coach_801828CC);

extern SVECTOR D_dryfield_night_trailer_coach_801893F8[];
extern SVECTOR D_dryfield_night_trailer_coach_80189400[];
extern SVECTOR D_dryfield_night_trailer_coach_80189480[];

void func_dryfield_night_trailer_coach_801838B4(SVECTOR* v, s32 arg1);
void Room_Draw18(SVECTOR* v, s32 arg1, s32 arg2);
void Room_Draw05(SVECTOR* v, s32 arg1, s32 arg2);

void func_dryfield_night_trailer_coach_80182924(void)
{
    SVECTOR* p;
    SVECTOR* q;
    SVECTOR* r;
    u8       view;

    view = Game_Session->field_4;
    switch (view) {
        case 3:
            p = D_dryfield_night_trailer_coach_80189400;
            func_dryfield_night_trailer_coach_801838B4(&p[0], 0x100);
            func_dryfield_night_trailer_coach_801838B4(&p[2], 0x100);
            func_dryfield_night_trailer_coach_801838B4(&p[4], 0x100);
            func_dryfield_night_trailer_coach_801838B4(&p[6], 0x100);
            func_dryfield_night_trailer_coach_801838B4(&p[8], 0x100);
            func_dryfield_night_trailer_coach_801838B4(&p[10], 0x100);
            func_dryfield_night_trailer_coach_801838B4(&p[12], 0x100);
            func_dryfield_night_trailer_coach_801838B4(&p[14], 0x100);
            /* fallthrough */
        case 4:
            q = D_dryfield_night_trailer_coach_80189480;
            func_dryfield_night_trailer_coach_801838B4(&q[0], 0x100);
            func_dryfield_night_trailer_coach_801838B4(&q[2], 0x100);
            func_dryfield_night_trailer_coach_801838B4(&q[4], 0x100);
            func_dryfield_night_trailer_coach_801838B4(&q[6], 0x100);
            func_dryfield_night_trailer_coach_801838B4(&q[8], 0xC0);
            func_dryfield_night_trailer_coach_801838B4(&q[10], 0xC0);
            func_dryfield_night_trailer_coach_801838B4(&q[12], 0xC0);
            func_dryfield_night_trailer_coach_801838B4(&q[14], 0xC0);
            break;
        case 2:
        case 5:
        case 7:
            r = D_dryfield_night_trailer_coach_801893F8;
            Room_Draw18(&r[0], 0x60, 0xA0);
            func_dryfield_night_trailer_coach_801838B4(&r[1], 0x100);
            func_dryfield_night_trailer_coach_801838B4(&r[3], 0x100);
            func_dryfield_night_trailer_coach_801838B4(&r[5], 0x100);
            func_dryfield_night_trailer_coach_801838B4(&r[7], 0x100);
            break;
        case 8:
            Room_Draw05(&D_dryfield_night_trailer_coach_801893F8[0], 0x60, 0x30);
            break;
    }
}
