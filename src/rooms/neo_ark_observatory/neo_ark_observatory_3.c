#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern TaskDesc       D_neo_ark_observatory_801811AC;
extern s32            D_neo_ark_observatory_801811B8;
extern SVECTOR        D_neo_ark_observatory_80181434[];
extern SVECTOR        D_neo_ark_observatory_801814E4[];
extern SVECTOR        D_neo_ark_observatory_801814F4[];
extern SVECTOR        D_neo_ark_observatory_801814FC[];
extern SVECTOR        D_neo_ark_observatory_8018150C[];
extern SVECTOR        D_neo_ark_observatory_8018151C[];
extern SVECTOR        D_neo_ark_observatory_80181524[];
extern SVECTOR        D_neo_ark_observatory_80181564[];
extern SVECTOR        D_neo_ark_observatory_80181574[];
extern SVECTOR        D_neo_ark_observatory_8018157C[];
extern s16            D_neo_ark_observatory_80187A3C;
extern GpAreaApplyRec D_neo_ark_observatory_80187A28;

void Room_Draw13(SVECTOR* v, s32 arg1, s32 arg2);
void func_neo_ark_observatory_80180534(SVECTOR* v, s32 arg1, s16 arg2, s32 arg3);

extern u8   D_8007216C;
extern void func_801322F8(void);
extern void func_neo_ark_observatory_8017FA98(s32 arg0);
extern void func_neo_ark_observatory_80180DAC(s32 arg0);
extern void func_80179B14(RoomEventMsg* in, RoomEventMsg* out);

/// Room event-script handler: mirrors the incoming message onto the outgoing
/// one and lets `func_80179B14` act on both. Once the observatory has been
/// reached from both routes (nibbles 0xD1 == 3 and 0x4C == 9) and the script
/// raises one of the two arrival ids with no sub-state pending, nibble 0x4C is
/// cleared and the room's area records are applied.
s32 func_neo_ark_observatory_8017FBE8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179B14(in, out);
    if ((GameFlag_GetNibble(0xD1) == 3) && (GameFlag_GetNibble(0x4C) == 9) &&
        ((in->msgId == 0xA) || (in->msgId == 0x13)) && (in->field_5 == 0)) {
        GameFlag_SetNibble(0x4C, 0);
        Gp_ApplyAreaRecs(&D_neo_ark_observatory_80187A28);
    }
    return 1;
}

s32 func_neo_ark_observatory_8017FCA0(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(&D_neo_ark_observatory_801811AC, 0, 1, 0);
    }
    return 0;
}

/// Room entry task tick: installs the room's message table, hands the task to
/// slot 7, then advances state.
void func_neo_ark_observatory_8017FCE0(Task* arg0)
{
    arg0->msgTable = &D_neo_ark_observatory_801811B8;
    Game_SetPtrSlot(arg0, 7);
    if ((gameGetPtrSlot(0xA) != NULL) && (gGameSession->at4.loc.place == 1)) {
        func_801322F8();
    } else {
        func_neo_ark_observatory_8017FA98(0);
    }
    if (GameFlag_GetNibble(0xE1) != 0) {
        func_neo_ark_observatory_80180DAC(0xA0);
    }
    arg0->state = arg0->state + 1;
}

/// Asks the ally message system for the observatory's arrival line: with no
/// cap running, arrival 2 only counts once the published area id (`D_8007216C`)
/// says the script is on it; otherwise the line follows the area id's 2/3
/// staging.
void func_neo_ark_observatory_8017FD7C(void)
{
    s32 var_a0;

    if (gGameSession->eventState == 0) {
        if (D_8007216C != 2) {
            Gp_MsgAlly3F3(2);
            return;
        }
    }
    var_a0 = 1;
    if (D_8007216C == 3) {
        var_a0 = 2;
    }
    Gp_MsgAlly3F3(var_a0);
}
