#include "common.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "rooms/room_common.h"
#include "rooms/mine_cavern.h"

extern s32 D_mine_cavern_8018EB54;
extern u8  D_8007216D;

extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011573C;
extern s32 D_80115744;
extern u32 Gp_LcgState;

extern SVECTOR D_mine_cavern_80188F64[];
extern SVECTOR D_mine_cavern_80188F7C[];
extern SVECTOR D_mine_cavern_80188F84[];
extern SVECTOR D_mine_cavern_80188F8C[];
extern SVECTOR D_mine_cavern_80188F94[];
extern SVECTOR D_mine_cavern_80188F9C[];
extern SVECTOR D_mine_cavern_80188FB4[];
extern SVECTOR D_mine_cavern_80188FBC;
extern SVECTOR D_mine_cavern_80188FC4[];

void func_mine_cavern_8017E330(void)
{
    D_8007216D                  = 2;
    gGameSession->at4.loc.room  = 2;
    gGameSession->roomObjsDirty = 1;
}

void func_mine_cavern_8017E358(void)
{
}

void func_mine_cavern_8017E360(void)
{
    gGameSession->at4.loc.place = 4;
    Gp_StateF0.field_0          = 0;
    Gp_StateF0.field_5          = 0;
    Gp_StateF0.field_6          = 0;
    Gp_StateF0.field_8          = 0;
    Gp_StateF0.field_C          = 0;
    Gp_StateF0.field_10         = 0;
}

void func_mine_cavern_8017E394(void)
{
    D_mine_cavern_8018EB54 = 0;
}

void func_mine_cavern_8017E3A0(s32 arg0)
{
    GpAreaKey*         sess;
    MineCavernSprtRec* rec;
    s32                v;

    sess = &gGameSession->at4.loc;
    rec  = (MineCavernSprtRec*)Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    v    = arg0 & 0xFF;

    if (v == 1) {
        rec->field_28->field_2C  = v;
        rec->field_34->field_34  = v;
        rec->field_100->field_2C = v;
        rec->field_10C->field_1C = v;
        rec->field_118->field_24 = v;
        return;
    }
    if (v == 0) {
        rec->field_28->field_2C  = 0;
        rec->field_34->field_34  = 0;
        rec->field_100->field_2C = 0;
        rec->field_10C->field_1C = 0;
        rec->field_118->field_24 = 0;
    }
}

void func_mine_cavern_8017E474(Task* arg0)
{
    u32 rnd;

    if (arg0->state == 0) {
        D_80115728                 = 0x60244;
        D_80115744                 = 0x60250;
        D_8011573C                 = 0x6023F;
        D_80115720                 = 0x60267;
        Gp_State1C->roomEffectMode = 2;
        arg0->state                = 1;
    }

    if (GameFlag_GetNibble(0xC4) == 1) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (((Gp_LcgState >> 16) & 7) == 0) {
            Gp_LcgState               = Gp_LcgState * 5 + 0x71357911;
            D_mine_cavern_80188FBC.vx = ((Gp_LcgState >> 16) & 0x3F) + 0x1766;
            Gp_LcgState               = Gp_LcgState * 5 + 0x71357911;
            D_mine_cavern_80188FBC.vy = ((Gp_LcgState >> 16) & 0x3F) - 0x5B4;
            Gp_LcgState               = Gp_LcgState * 5 + 0x71357911;
            D_mine_cavern_80188FBC.vz = ((Gp_LcgState >> 16) & 0x3F) - 0x14A;
            Gp_SpawnEff(0x600E0, NULL, 0x300, &D_mine_cavern_80188FBC);
        }
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
        case 3:
        case 9: {
            SVECTOR* p = D_mine_cavern_80188F84;
            Room_Draw17(&p[0], 1, 0x300);
            Room_Draw17(&p[2], 1, 0x300);
            break;
        }
        case 4: {
            SVECTOR* p = D_mine_cavern_80188F7C;
            Room_Draw17(&p[0], 1, 0x300);
            Room_Draw17(&p[5], 1, 0x300);
            break;
        }
        case 5:
            Room_Draw17(D_mine_cavern_80188F8C, 1, 0x300);
        case 23: {
            SVECTOR* p = D_mine_cavern_80188F64;
            Room_Draw01(&p[0], 0x180, 0x222);
            Room_Draw01(&p[1], 0x180, 0x222);
            Room_Draw17(&p[3], 1, 0x300);
            break;
        }
        case 6: {
            SVECTOR* p = D_mine_cavern_80188F8C;
            Room_Draw17(&p[0], 1, 0x300);
            Room_Draw17(&p[2], 1, 0x300);
            Room_Draw17(&p[4], 1, 0x300);
            break;
        }
        case 7: {
            SVECTOR* p = D_mine_cavern_80188F84;
            Room_Draw17(&p[0], 1, 0x300);
            Room_Draw17(&p[2], 1, 0x300);
            Room_Draw17(&p[3], 1, 0x300);
            break;
        }
        case 8:
        case 20:
            Room_Draw17(D_mine_cavern_80188F94, 1, 0x300);
            break;
        case 10:
            Room_Draw17(D_mine_cavern_80188FB4, 1, 0x300);
            break;
        case 11:
            Room_Draw17(D_mine_cavern_80188F8C, 1, 0x300);
        case 13: {
            SVECTOR* p = D_mine_cavern_80188F7C;
            Room_Draw17(&p[0], 1, 0x300);
            Room_Draw17(&p[5], 1, 0x300);
            Room_Draw17(&p[6], 1, 0x300);
            break;
        }
        case 14:
        case 16:
        case 21:
            Room_Draw17(D_mine_cavern_80188F8C, 1, 0x300);
            break;
        case 17:
            Room_Draw17(D_mine_cavern_80188F9C, 1, 0x300);
            break;
        case 24:
            Room_Draw17(D_mine_cavern_80188FC4, 1, 0x300);
        case 22:
            Room_Draw17(D_mine_cavern_80188F7C, 1, 0x300);
            break;
        case 25: {
            SVECTOR* p = D_mine_cavern_80188F7C;
            Room_Draw17(&p[0], 1, 0x300);
            Room_Draw17(&p[2], 1, 0x300);
            break;
        }
    }
}
