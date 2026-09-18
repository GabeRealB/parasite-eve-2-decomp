#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

/// Main-executable globals with no module header yet: `D_80071075` gates the
/// plant's power-on cutscene, `D_8007216C` is the loaded area id, `D_8007272D`
/// the area-record id the cutscene publishes and `D_80114C12` the cutscene
/// mode flag.
extern u8 D_80071075;
extern u8 D_8007216C;
extern s8 D_8007272D;
extern s8 D_80114C12;

/// Script blobs in the overlay's `.data`, handed to `func_800E8634` /
/// `func_800E8614` (which forward them to `Task_Spawn`) as raw addresses.
extern s32 D_neo_ark_power_plant_1_8017EB7C;
extern s32 D_neo_ark_power_plant_1_8017EDBC;
extern s32 D_neo_ark_power_plant_1_8017EEE4;

/// Countdown the power-on cutscene arms to 4 and then ticks down; reaching 0
/// plays the plant's switch-on sound event.
extern s32 D_neo_ark_power_plant_1_8017F01C;

/// 0xFF-terminated area-record list applied when the plant powers on.
extern GpAreaApplyRec D_neo_ark_power_plant_1_80181C00;

/// Power plant tick: while the plant is not powered yet (nibble 0xDE clear),
/// asks the slot-4 task for message 0x7D6 and, unless a cutscene is already
/// running, latches the plant as powered - setting nibbles 0xDE/0xF6, clearing
/// 0x1B2, applying the area records above, publishing area-record id 0x16 and
/// spawning the power-on script. Clears the "powered" state once the area id
/// reads 3 with nibble 0xFB clear. Re-arms the countdown to 4 whenever the
/// player leaves the area the plant was powered in with 0xDE set and 0xDF
/// clear, and plays the switch sound once that countdown runs out.
void func_neo_ark_power_plant_1_8017D5EC(void)
{
    Task* slot;

    if (GameFlag_GetNibble(0xDE) == 0) {
        slot = (Task*)Gp_LookupSlot4(0);
        if (slot != 0) {
            if (Gp_DispatchMsg(slot, 0x7D6, 0, 0) == 0) {
                if (D_80114C12 != 1) {
                    if (D_80071075 == 0) {
                        GameFlag_SetNibble(0xDE, 1);
                        GameFlag_SetNibble(0xF6, 1);
                        GameFlag_SetNibble(0x1B2, 0);
                        Gp_ApplyAreaRecs(&D_neo_ark_power_plant_1_80181C00);
                        D_8007272D = 0x16;
                        func_800E8634((s32)&D_neo_ark_power_plant_1_8017EB7C, 0, (s32)&D_neo_ark_power_plant_1_8017EDBC);
                    }
                }
            }
        }
    }
    if ((D_8007216C == 3) && (GameFlag_GetNibble(0xFB) == 0)) {
        GameFlag_SetNibble(0xFB, 1);
        gGameSession->field_126 = 0;
        Gp_StateF0.field_0      = 0;
        func_800E8614((s32)&D_neo_ark_power_plant_1_8017EEE4, 0);
    }
    if ((D_8007216C != gGameSession->at4.loc.view) && (GameFlag_GetNibble(0xDE) != 0) && (GameFlag_GetNibble(0xDF) == 0)) {
        D_neo_ark_power_plant_1_8017F01C = 4;
        return;
    }
    if (D_neo_ark_power_plant_1_8017F01C != 0) {
        if (--D_neo_ark_power_plant_1_8017F01C == 0) {
            if (gGameSession->at4.loc.view == 7) {
                SndEvt_EnqueueType6(0x5511000A, 0, 0);
                return;
            }
            SndEvt_EnqueueType7(0x5511000A, 1);
        }
    }
}

s32 func_neo_ark_power_plant_1_8017D7AC(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_power_plant_1/neo_ark_power_plant_1", D_neo_ark_power_plant_1_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_power_plant_1/neo_ark_power_plant_1", RoomsShared8017d878Table);
