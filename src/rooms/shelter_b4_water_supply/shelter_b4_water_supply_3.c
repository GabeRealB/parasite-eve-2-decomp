#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b4_water_supply.h"

#include <psyq/abs.h>

/// Block this task receives as `spawnArg2`. Only the halfword at 0x26 is
/// touched here: an effect strength, set from how far a tracked part moved this
/// frame and used as the odds of spawning each of the two effects.
typedef struct _ShelterB4WaterSupplySplash {
    byte pad_0[0x26];
    s16  strength;
} _ShelterB4WaterSupplySplash;

extern s32 D_8007107C;
extern s8  D_8007217B;
extern u8* D_shelter_b4_water_supply_80184E50;
void       func_shelter_b4_water_supply_8017DE74(s32);
void       func_shelter_b4_water_supply_8017E5D8(s32);

extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115738;
extern s32 D_8011574C;
extern s32 D_80115754;
extern u32 Gp_LcgState;

extern SVECTOR D_shelter_b4_water_supply_80182670[];
extern SVECTOR D_shelter_b4_water_supply_80182680[];
extern SVECTOR D_shelter_b4_water_supply_80182690[];
extern SVECTOR D_shelter_b4_water_supply_801826A0[];
extern SVECTOR D_shelter_b4_water_supply_801826C0[];
extern SVECTOR D_shelter_b4_water_supply_801826D0[];
/// Last-frame world positions of the two tracked parts of the slot-3 task's
/// model, compared against this frame's to measure how far each moved.
extern SVECTOR D_shelter_b4_water_supply_801826E0[];

void func_shelter_b4_water_supply_8017EDD0(s32 arg0)
{
    if (D_8007217B == 0) {
        D_shelter_b4_water_supply_80184E50 = (u8*)D_8005C374 + D_8007107C * 0xC000;
    } else {
        D_shelter_b4_water_supply_80184E50 = (u8*)D_8005C370 + D_8007107C * 0xC000;
    }
    func_shelter_b4_water_supply_8017DE74(arg0);
    func_shelter_b4_water_supply_8017E5D8(arg0);
}

/// Room task. State 0 installs five effect ids in the shared effect-id slots,
/// records the world positions of parts 14 and 17 of the slot-3 task's model,
/// and advances. Later states, while no event is running and `waterY` is below
/// that model's root, spawn each of two effects at water level under each part
/// with odds that grow with how far the part moved since last frame. Every
/// frame it then draws the light beams the current view selects, through
/// `func_shelter_b4_water_supply_80180260`.
void func_shelter_b4_water_supply_8017EE54(Task* arg0)
{
    Task*                        ctl;
    _ShelterB4WaterSupplySplash* splash;
    GsCOORDINATE2*               ctlCoords;
    GsCOORDINATE2*               part;
    GsCOORDINATE2                surface;
    MATRIX*                      mtx;
    GsCOORDINATE2*               view;
    s32                          i;
    u32                          rnd;

    splash    = arg0->spawnArg2;
    ctl       = gameGetPtrSlot(3);
    ctlCoords = ((TmdObject*)ctl->extra)->coords;
    if (arg0->state == 0) {
        D_8011574C  = 0x60174;
        D_80115738  = 0x60175;
        D_80115734  = 0x60226;
        D_80115730  = 0x60231;
        D_80115754  = 0x6023C;
        arg0->state = 1;
        for (i = 0; i < 2; i++) {
            part                                     = &((TmdObject*)ctl->extra)->coords[14 + i * 3];
            D_shelter_b4_water_supply_801826E0[i].vx = part->workm.t[0];
            D_shelter_b4_water_supply_801826E0[i].vy = part->workm.t[1];
            D_shelter_b4_water_supply_801826E0[i].vz = part->workm.t[2];
        }
    } else if (Gp_State1C->eventState == 0 && gGameSession->waterY < ctlCoords->coord.t[1]) {
        view = &gGfxViewCoord;
        i    = 0;
        mtx  = &Gfx_ViewWorldMtx;
        for (; i < 2; i++) {
            part = &((TmdObject*)ctl->extra)->coords[14 + i * 3];
            Gp_UpdateCoord(part);
            splash->strength = ABS(D_shelter_b4_water_supply_801826E0[i].vx - part->workm.t[0]) +
                               ABS(D_shelter_b4_water_supply_801826E0[i].vy - part->workm.t[1]) +
                               ABS(D_shelter_b4_water_supply_801826E0[i].vz - part->workm.t[2]) + 0x20;
            Gp_WorldToLocal(mtx, &part->workm, &surface.coord);
            surface.sub        = view;
            surface.coord.t[1] = gGameSession->waterY;
            surface.flg        = 0;
            Gp_UpdateCoord(&surface);
            rnd = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911);
            if ((s32)((rnd >> 16) & 0x1FF) < splash->strength) {
                Gp_SpawnEff(D_8011574C, &surface, 0x40, 0);
            }
            splash->strength -= 0x20;
            rnd               = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911);
            if ((s32)((rnd >> 16) & 0x1FF) < splash->strength) {
                Gp_SpawnEff(D_80115738, &surface, 0x1202180, 0);
            }
            D_shelter_b4_water_supply_801826E0[i].vx = part->workm.t[0];
            D_shelter_b4_water_supply_801826E0[i].vy = part->workm.t[1];
            D_shelter_b4_water_supply_801826E0[i].vz = part->workm.t[2];
        }
    }
    switch ((u8)Gp_GetViewIndex()) {
        case 4:
            func_shelter_b4_water_supply_80180260(&D_shelter_b4_water_supply_80182690[0], 0x200, 0);
            func_shelter_b4_water_supply_80180260(&D_shelter_b4_water_supply_80182690[4], 0x200, 0x800);
        case 2:
        case 3:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_80182670, 0x200, 0x800);
            break;
        case 6:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_801826A0, 0x200, 0);
        case 5:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_80182680, 0x200, 0x800);
            break;
        case 7:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_801826A0, 0x200, 0);
            break;
        case 8:
            func_shelter_b4_water_supply_80180260(&D_shelter_b4_water_supply_80182690[0], 0x200, 0);
            func_shelter_b4_water_supply_80180260(&D_shelter_b4_water_supply_80182690[4], 0x200, 0x800);
            break;
        case 9:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_801826D0, 0x200, 0x800);
        case 10:
        case 11:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_801826C0, 0x200, 0x800);
            break;
    }
}
