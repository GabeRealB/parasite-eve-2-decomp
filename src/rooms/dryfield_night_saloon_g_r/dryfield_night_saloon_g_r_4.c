#include "common.h"

#include "gameplay/gameplay.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/dryfield_night_saloon_g_r.h"

extern TaskDesc D_dryfield_night_saloon_g_r_80185068;

/// The room's effect positions, one `SVECTOR` per quad. Entries 12 and 13 are
/// the pair `func_dryfield_night_saloon_g_r_8017F0A4` is handed; the runs the
/// frame hook draws are 0-10 and 20-27.
extern SVECTOR D_dryfield_night_saloon_g_r_80185074[];
extern SVECTOR D_dryfield_night_saloon_g_r_801850DC[];

/// One view bitmask per effect, tested against `1 << view`. Entries 0-10 gate
/// positions 0-10, entries 11 and 12 the two helper effects, and entries 13-20
/// gate positions 20-27.
extern s16 D_dryfield_night_saloon_g_r_80185154[];

s32 func_dryfield_night_saloon_g_r_8017E698(s32 arg0)
{
    Display_InitModeObj(&D_dryfield_night_saloon_g_r_80185068, 0, 0, 0);
    return 1;
}

/// Per-frame effect on the room's model task: recomputes the model's world
/// matrix, then draws every effect whose view mask includes the current view
/// `gGameSession->at4.loc.view`. Positions 0-5 and 20-22 are drawn with UV
/// column 0 and half-extent 0x200, 6-10 with column 1 and 0x1C0, and 23-27
/// with column 0 and 0x300; the two helpers in between take the model's coord.
void func_dryfield_night_saloon_g_r_8017E6C8(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            mask;
    s32            i;

    coord = ((TmdObject*)arg0->extra)->coords;
    mask  = 1 << gGameSession->at4.loc.view;
    Gp_UpdateCoord(coord);
    for (i = 0; i < 6; i++) {
        if (mask & D_dryfield_night_saloon_g_r_80185154[i]) {
            Room_Draw17(&D_dryfield_night_saloon_g_r_80185074[i], 0, 0x200);
        }
    }
    for (i = 6; i < 11; i++) {
        if (mask & D_dryfield_night_saloon_g_r_80185154[i]) {
            Room_Draw17(&D_dryfield_night_saloon_g_r_80185074[i], 1, 0x1C0);
        }
    }
    if (mask & D_dryfield_night_saloon_g_r_80185154[12]) {
        func_dryfield_night_saloon_g_r_8017EB38(coord);
    }
    if (mask & D_dryfield_night_saloon_g_r_80185154[11]) {
        func_dryfield_night_saloon_g_r_8017F0A4(coord, D_dryfield_night_saloon_g_r_801850DC,
                                                D_dryfield_night_saloon_g_r_801850DC - 1, 0x100);
    }
    for (i = 20; i < 23; i++) {
        if (mask & D_dryfield_night_saloon_g_r_80185154[i - 7]) {
            Room_Draw17(&D_dryfield_night_saloon_g_r_80185074[i], 0, 0x200);
        }
    }
    for (i = 23; i < 28; i++) {
        if (mask & D_dryfield_night_saloon_g_r_80185154[i - 7]) {
            Room_Draw17(&D_dryfield_night_saloon_g_r_80185074[i], 0, 0x300);
        }
    }
}
