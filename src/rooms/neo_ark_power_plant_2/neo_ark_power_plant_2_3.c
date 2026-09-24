#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/neo_ark_power_plant_2.h"
#include "rooms/room_common.h"

extern u8 D_80071075;
extern s8 D_8007272D;
extern s8 D_80114C12;

extern s32            D_neo_ark_power_plant_2_801802A8;
extern s32            D_neo_ark_power_plant_2_80180560;
extern GpAreaApplyRec D_neo_ark_power_plant_2_80182F70[];
extern GpAreaApplyRec D_neo_ark_power_plant_2_80182F94[];

extern s32       D_8011572C;
extern s32       D_80115750;
extern s32       D_80115758;
extern AhlpLight D_801150C0[];
extern SVECTOR   D_neo_ark_power_plant_2_80180668;
extern SVECTOR   D_neo_ark_power_plant_2_80180678;
extern u32       Gp_LcgState;

void func_neo_ark_power_plant_2_8017D8AC(Task* arg0)
{
    u32            rnd;
    u16            intensity;
    AhlpLightWork* work;

    if (arg0->state == 0) {
        D_80115758  = 0x601DC;
        D_8011572C  = 0x601F8;
        D_80115750  = 0x60214;
        arg0->state = 1;
    }
    switch ((u8)Gp_GetViewIndex()) {
        case 6:
            if (GameFlag_GetNibble(0x147) != 0) {
                if (Gp_State1C->eventState == 0 && GameFlag_GetNibble(0xDF) == 0) {
                    rnd         = Gp_LcgState * 5 + 0x71357911;
                    Gp_LcgState = rnd;
                    if (((rnd >> 16) & 7) == 0) {
                        Gp_SpawnEff(0x600E0, NULL, 0x400, &D_neo_ark_power_plant_2_80180678);
                    }
                }
            } else {
                func_neo_ark_power_plant_2_8017DA54(&D_neo_ark_power_plant_2_80180678, 0x300, 0x334);
            }
            break;
        case 8:
            D_801150C0[0].state        = 4;
            work                       = &D_801150C0[0].work;
            work->field_58             = 0x400;
            work->field_5C             = 0x4000;
            D_801150C0[0].work.field_0 = 0;
            rnd                        = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState                = rnd;
            intensity                  = ((rnd >> 16) & 0x700) + 0x800;
            work->field_54             = intensity;
            work->field_50             = intensity >> 1;
            work->field_52             = intensity >> 1;
            work->x                    = D_neo_ark_power_plant_2_80180668.vx;
            work->y                    = D_neo_ark_power_plant_2_80180668.vy;
            work->z                    = D_neo_ark_power_plant_2_80180668.vz;
            break;
    }
}
