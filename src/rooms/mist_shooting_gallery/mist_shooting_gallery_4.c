#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

#include "rooms/mist_shooting_gallery.h"

#include <psyq/libgte.h>

extern u8 D_80062737;
/// Screen-fade "overlay owns the display" flag, first byte of the flag block
/// at 0x80071068. Declared as an array on purpose: GCC 2.8.1 exempts a
/// *fixed-address scalar* store from aliasing with a varying-address struct
/// load, so a plain `extern s8` here lets the scheduler hoist the following
/// `arg0->state` load above the store. Indexing an array makes the store a
/// struct reference and keeps the two in order.
extern s8  D_80071068[];
extern s8  D_8007106B;
extern s16 D_8007A396;
extern s8  D_80072176;
extern s8  D_80072177;
/// "SELECT" — the panel title, owned by `mist_shooting_gallery.c`'s rodata.
extern char         RoomsShared8017e28cTitle[];
extern void*        D_mist_shooting_gallery_801853C0;
extern u32          D_mist_shooting_gallery_8018D1B4[];
extern u32          D_mist_shooting_gallery_8018DF38[];
extern UiObjectDesc D_mist_shooting_gallery_8018535C;
extern UiList       RoomsShared8017e28cMenu;
extern TaskDesc     D_mist_shooting_gallery_80185378;
extern TaskDesc     D_mist_shooting_gallery_80185384;
extern SVECTOR      D_mist_shooting_gallery_80185550[];
extern SVECTOR      D_mist_shooting_gallery_80185570[];
extern SVECTOR      D_mist_shooting_gallery_801855C0[];
extern SVECTOR      D_mist_shooting_gallery_801855F0[];
extern SVECTOR      D_mist_shooting_gallery_80185610[];
extern SVECTOR      D_mist_shooting_gallery_80185670[];
extern SVECTOR      D_mist_shooting_gallery_80185678[];
extern SVECTOR      D_mist_shooting_gallery_80185680[];
extern SVECTOR      D_mist_shooting_gallery_80185688[];
extern SVECTOR      D_mist_shooting_gallery_80185690[];
extern SVECTOR      D_mist_shooting_gallery_801856B0[];
void                Room_Draw01(SVECTOR* v, s32 arg1, s32 arg2);
void                Room_Draw31(SVECTOR* v, s32 arg1, s32 arg2);

void func_mist_shooting_gallery_801811EC(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            Room_Draw01(&D_mist_shooting_gallery_80185550[0], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185550[8], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185550[10], 0x200, 0x222);
            break;
        case 3:
            Room_Draw01(&D_mist_shooting_gallery_80185570[0], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185570[2], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185570[10], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185570[12], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185570[14], 0x200, 0x222);
            break;
        case 7:
            Room_Draw01(&D_mist_shooting_gallery_801855C0[0], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855C0[2], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855C0[4], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855C0[6], 0x200, 0x222);
            break;
        case 8:
            Room_Draw01(&D_mist_shooting_gallery_80185610[0], 0x200, 0x222);
            break;
        case 9:
        case 18:
            Room_Draw01(&D_mist_shooting_gallery_801855F0[0], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855F0[2], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855F0[6], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855F0[8], 0x200, 0x222);
            Room_Draw31(&D_mist_shooting_gallery_801855F0[16], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_801856B0[0], 0x300, 0x111);
            break;
        case 10:
            Room_Draw31(&D_mist_shooting_gallery_80185678[0], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185678[2], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185678[4], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185678[6], 0x300, 0x111);
            break;
        case 11:
            Room_Draw31(&D_mist_shooting_gallery_80185680[0], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185680[1], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185680[3], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185680[4], 0x300, 0x111);
            break;
        case 12:
            Room_Draw31(&D_mist_shooting_gallery_80185690[0], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185690[1], 0x300, 0x111);
            break;
        case 13:
            Room_Draw31(&D_mist_shooting_gallery_80185688[0], 0x300, 0x111);
            break;
        case 14:
            Room_Draw31(&D_mist_shooting_gallery_80185670[0], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185670[1], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185670[3], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185670[5], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185670[7], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_801856B0[0], 0x300, 0x111);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_4", D_mist_shooting_gallery_8017DB80);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_4", D_mist_shooting_gallery_8017DB8C);
