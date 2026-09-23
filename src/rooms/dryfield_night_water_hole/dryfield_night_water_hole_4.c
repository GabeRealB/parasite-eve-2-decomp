#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

#include <psyq/abs.h>

/// Block this task receives as `spawnArg2`. Only the halfword at 0x26 is
/// touched here: an effect strength, set from how far a tracked part moved this
/// frame and used as the odds of spawning each of the two effects.
typedef struct _DryfieldNightWaterHoleSplash {
    byte pad_0[0x26];
    s16  strength;
} _DryfieldNightWaterHoleSplash;

extern s32 D_80115738;
extern s32 D_8011574C;
extern u32 Gp_LcgState;

extern SVECTOR D_dryfield_night_water_hole_80180994[];
extern SVECTOR D_dryfield_night_water_hole_801809B4[];
extern SVECTOR D_dryfield_night_water_hole_801809D4[];
/// Last-frame world positions of the two tracked parts of the slot-3 task's
/// model, compared against this frame's to measure how far each moved.
extern SVECTOR D_dryfield_night_water_hole_801809F4[];

/// Room task. State 0 installs effect ids 0x600FF / 0x6011F in the two shared
/// effect-id slots while progress nibble 0xB8 is clear, records the world
/// positions of parts 14 and 17 of the slot-3 task's model, and advances.
/// State 1, while nibble 0xB8 is clear, no event is running and `waterY` is
/// below that model's root, spawns each effect at water level under each part
/// with odds that grow with how far the part moved since last frame, then, once
/// game-flag nibble 0x51 is 1, draws the `Room_Draw08` segment pairs the
/// current view selects.
void func_dryfield_night_water_hole_8017E6D0(Task* arg0)
{
    Task*                          ctl;
    s32                            mask;
    _DryfieldNightWaterHoleSplash* splash;
    GsCOORDINATE2*                 ctlCoords;
    GsCOORDINATE2*                 part;
    GsCOORDINATE2*                 view;
    GsCOORDINATE2                  surface;
    s32                            i;
    u32                            rnd;

    ctl       = gameGetPtrSlot(3);
    splash    = arg0->spawnArg2;
    mask      = 1 << gGameSession->at4.loc.view;
    ctlCoords = ((TmdObject*)ctl->extra)->coords;
    switch (arg0->state) {
        case 0:
            if (GameFlag_GetNibble(0xB8) == 0) {
                D_8011574C = 0x600FF;
                D_80115738 = 0x6011F;
            }
            arg0->state = 1;
            for (i = 0; i < 2; i++) {
                part                                       = &((TmdObject*)ctl->extra)->coords[14 + i * 3];
                D_dryfield_night_water_hole_801809F4[i].vx = part->workm.t[0];
                D_dryfield_night_water_hole_801809F4[i].vy = part->workm.t[1];
                D_dryfield_night_water_hole_801809F4[i].vz = part->workm.t[2];
            }
            break;
        case 1:
            if (GameFlag_GetNibble(0xB8) == 0 && Gp_State1C->eventState == 0 &&
                gGameSession->waterY < ctlCoords->coord.t[1]) {
                view = &gGfxViewCoord;
                for (i = 0; i < 2; i++) {
                    part = &((TmdObject*)ctl->extra)->coords[14 + i * 3];
                    Gp_UpdateCoord(part);
                    splash->strength = ABS(D_dryfield_night_water_hole_801809F4[i].vx - part->workm.t[0]) +
                                       ABS(D_dryfield_night_water_hole_801809F4[i].vy - part->workm.t[1]) +
                                       ABS(D_dryfield_night_water_hole_801809F4[i].vz - part->workm.t[2]) + 0x20;
                    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &part->workm, &surface.coord);
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
                    D_dryfield_night_water_hole_801809F4[i].vx = part->workm.t[0];
                    D_dryfield_night_water_hole_801809F4[i].vy = part->workm.t[1];
                    D_dryfield_night_water_hole_801809F4[i].vz = part->workm.t[2];
                }
            }
            if (GameFlag_GetNibble(0x51) == 1) {
                if (mask & 0x18) {
                    Room_Draw08(&D_dryfield_night_water_hole_80180994[0], 0x100);
                    Room_Draw08(&D_dryfield_night_water_hole_80180994[2], 0x100);
                }
                if (mask & 0xA50) {
                    Room_Draw08(&D_dryfield_night_water_hole_801809B4[0], 0x100);
                    Room_Draw08(&D_dryfield_night_water_hole_801809B4[2], 0x100);
                }
                if (mask & 0x80) {
                    Room_Draw08(&D_dryfield_night_water_hole_801809D4[0], 0x100);
                    Room_Draw08(&D_dryfield_night_water_hole_801809D4[2], 0x100);
                }
            }
            break;
    }
}
