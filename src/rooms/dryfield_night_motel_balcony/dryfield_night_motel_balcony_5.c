#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

extern SVECTOR D_dryfield_night_motel_balcony_80182C60[];
extern SVECTOR D_dryfield_night_motel_balcony_80182C70;
extern SVECTOR D_dryfield_night_motel_balcony_80182C80;
extern SVECTOR D_dryfield_night_motel_balcony_80182C90;
extern SVECTOR D_dryfield_night_motel_balcony_80182CA0;
extern SVECTOR D_dryfield_night_motel_balcony_80182CF0;
extern SVECTOR D_dryfield_night_motel_balcony_80182D00;
extern SVECTOR D_dryfield_night_motel_balcony_80182D08;
extern SVECTOR D_dryfield_night_motel_balcony_80182D10;
extern SVECTOR D_dryfield_night_motel_balcony_80182D18;
extern SVECTOR D_dryfield_night_motel_balcony_80182D28;
extern SVECTOR D_dryfield_night_motel_balcony_80182D30;
extern SVECTOR D_dryfield_night_motel_balcony_80182D38;
extern s32     D_dryfield_night_motel_balcony_80182D40[2][20];
extern u32     Gp_LcgState;

void func_dryfield_night_motel_balcony_8017E554(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    s32            hi;
    s32            mask;
    s32            i;
    s32            n;
    s16            cnt;
    SVECTOR        pos;
    SVECTOR        ofs;

    work                    = task->spawnArg2;
    coord                   = ((TmdObject*)task->extra)->coords;
    Gp_State1C->groundShade = 0xFF;
    hi                      = 0;
    if (gGameSession->at4.loc.view < 0x20) {
        mask = 1 << gGameSession->at4.loc.view;
    } else {
        mask = 1 << (gGameSession->at4.loc.view - 0x20);
        hi   = 1;
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][0]) {
        Room_Draw08(&D_dryfield_night_motel_balcony_80182C60[0], 0x180);
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][2]) {
        Room_Draw08(&D_dryfield_night_motel_balcony_80182C70, 0x180);
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][4]) {
        Room_Draw08(&D_dryfield_night_motel_balcony_80182C80, 0x180);
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][6]) {
        Room_Draw08(&D_dryfield_night_motel_balcony_80182C90, 0x180);
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][8]) {
        Room_Draw08(&D_dryfield_night_motel_balcony_80182CA0, 0x180);
    }
    for (i = 10; i < 18; i++) {
        if (mask & D_dryfield_night_motel_balcony_80182D40[hi][i]) {
            Room_Draw17(&D_dryfield_night_motel_balcony_80182C60[i], 1, 0x380);
        }
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][18]) {
        Room_Draw08(&D_dryfield_night_motel_balcony_80182CF0, 0x180);
    }
    if (GameFlag_GetNibble(0x7F) == 1) {
        D_dryfield_night_motel_balcony_80182D40[0][3] = 0;
        D_dryfield_night_motel_balcony_80182D40[0][2] = 0;
        D_dryfield_night_motel_balcony_80182D40[1][3] = 0;
        D_dryfield_night_motel_balcony_80182D40[1][2] = 0;
        Gp_SpawnEff(0x60094, coord, 0, &D_dryfield_night_motel_balcony_80182C70);
        GameFlag_SetNibble(0x7F, 2);
    } else if (GameFlag_GetNibble(0x7F) == 2) {
        D_dryfield_night_motel_balcony_80182D40[0][3] = 0;
        D_dryfield_night_motel_balcony_80182D40[0][2] = 0;
        D_dryfield_night_motel_balcony_80182D40[1][3] = 0;
        D_dryfield_night_motel_balcony_80182D40[1][2] = 0;
    }
    switch (gGameSession->at4.loc.view) {
        case 17:
            if ((s16)++work->field_26 == 0x5C) {
                Gp_SpawnEff(0x60095, coord, 0x40000300, &D_dryfield_night_motel_balcony_80182D28);
                Gp_SpawnEff(0x60095, coord, 0x40000300, &D_dryfield_night_motel_balcony_80182D28);
                for (i = 0; i < 3; i++) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x6003D, coord, ((Gp_LcgState >> 16) & 0xFF) | 0x80010100,
                                &D_dryfield_night_motel_balcony_80182D28);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x6003D, coord, ((Gp_LcgState >> 16) & 0x7F) | 0x80000080,
                                &D_dryfield_night_motel_balcony_80182D28);
                }
            }
            break;
        case 18:
            if ((s16)++work->field_24 == 0x3F) {
                Gp_SpawnEff(0x60050, coord, 3, &D_dryfield_night_motel_balcony_80182D08);
                work->field_26 = 0;
            }
            break;
        case 21:
            cnt = ++work->field_26;
            if (cnt >= 0x47) {
                n = cnt - 0x46;
                if ((s16)(cnt % 6) == 0) {
                    memset(&ofs, 0, sizeof(ofs));
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    ofs.vx      = -((s32)(Gp_LcgState >> 16) % (n * 10));
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    ofs.vy      = (s32)(Gp_LcgState >> 16) % (n * 10);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    ofs.vz      = (s32)(Gp_LcgState >> 16) % (n * 10);
                    pos         = ofs;
                    pos.vx     += D_dryfield_night_motel_balcony_80182D30.vx;
                    pos.vy     += D_dryfield_night_motel_balcony_80182D30.vy;
                    pos.vz     += D_dryfield_night_motel_balcony_80182D30.vz;
                    Gp_SpawnEff(0x6007E, coord, n * 0x28 + 0x40000600, &pos);
                }
                work->field_24 = 0;
            }
            break;
        case 19:
            if ((s16)++work->field_24 == 0x2B) {
                Gp_SpawnEff(0x60050, coord, 4, &D_dryfield_night_motel_balcony_80182D10);
            }
            break;
        case 20:
            if ((s16)++work->field_24 == 0xC) {
                Gp_SpawnEff(0x60050, coord, 2, &D_dryfield_night_motel_balcony_80182D00);
            }
            break;
        case 23:
            if ((s16)++work->field_24 == 0xC) {
                Gp_SpawnEff(0x60050, coord, 2, &D_dryfield_night_motel_balcony_80182D38);
            }
            break;
        case 29:
            if ((s16)++work->field_24 == 0x41) {
                for (i = 0; i < 3; i++) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x6003D, coord, ((Gp_LcgState >> 16) & 0x7F) | 0x80000080,
                                &D_dryfield_night_motel_balcony_80182D18);
                }
            }
            break;
        default:
            work->field_24 = 0;
            work->field_26 = 0;
            break;
    }
}

/// Closes this unit's `.rodata` so the next unit's jump table starts on its
/// 8-byte boundary. Nothing reads it.
const u32 D_dryfield_night_motel_balcony_8017D664 = 0;
