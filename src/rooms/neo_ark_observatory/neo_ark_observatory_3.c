#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"

extern TaskDesc D_neo_ark_observatory_801811AC;
extern s32      D_neo_ark_observatory_801811B8;
extern SVECTOR  D_neo_ark_observatory_80181434[];
extern SVECTOR  D_neo_ark_observatory_801814E4[];
extern SVECTOR  D_neo_ark_observatory_801814F4[];
extern SVECTOR  D_neo_ark_observatory_801814FC[];
extern SVECTOR  D_neo_ark_observatory_8018150C[];
extern SVECTOR  D_neo_ark_observatory_8018151C[];
extern SVECTOR  D_neo_ark_observatory_80181524[];
extern SVECTOR  D_neo_ark_observatory_80181564[];
extern SVECTOR  D_neo_ark_observatory_80181574[];
extern SVECTOR  D_neo_ark_observatory_8018157C[];
extern s16      D_neo_ark_observatory_80187A3C;

void Room_Draw13(SVECTOR* v, s32 arg1, s32 arg2);
void func_neo_ark_observatory_80180534(SVECTOR* v, s32 arg1, s16 arg2, s32 arg3);

extern void func_801322F8(void);
extern void func_neo_ark_observatory_8017FA98(s32 arg0);
extern void func_neo_ark_observatory_80180DAC(s32 arg0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_3", func_neo_ark_observatory_8017FBE8);

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
    arg0->field_24 = &D_neo_ark_observatory_801811B8;
    Game_SetPtrSlot(arg0, 7);
    if ((Game_GetPtrSlot(0xA) != NULL) && (Game_Session->field_9 == 1)) {
        func_801322F8();
    } else {
        func_neo_ark_observatory_8017FA98(0);
    }
    if (GameFlag_GetNibble(0xE1) != 0) {
        func_neo_ark_observatory_80180DAC(0xA0);
    }
    arg0->state = arg0->state + 1;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_3", func_neo_ark_observatory_8017FD7C);
