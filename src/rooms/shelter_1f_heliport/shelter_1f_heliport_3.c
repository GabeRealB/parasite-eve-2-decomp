#include "common.h"

#include "gameplay/D4.h"
#include "main/display.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

extern void func_8013230C(void);
extern void func_80149E80(void);
extern void func_80149FA4(void);
extern void func_shelter_1f_heliport_801802AC(s32 arg0);
extern void func_shelter_1f_heliport_801807C0(void);

extern GpMsgEntry D_shelter_1f_heliport_801811A0[];

void func_shelter_1f_heliport_80180658(Task* arg0)
{
    arg0->msgTable = D_shelter_1f_heliport_801811A0;
    Game_SetPtrSlot(arg0, 7);
    if (gGameSession->at4.loc.place == 1) {
        func_80149E80();
    }
    if (gGameSession->at4.loc.place == 2) {
        func_80149FA4();
    }
    if (gameGetPtrSlot(0xA) != NULL) {
        func_8013230C();
    }
    func_shelter_1f_heliport_801802AC(0);
    func_shelter_1f_heliport_801807C0();
    Gpu_ResetGraphAndOt();
    Tmd_AllocMissingBuffers();
    SndEvt_EnqueueType6(0x55040006, 0, 0);
    SndEvt_EnqueueType6(0x55040007, 0, 0);
    arg0->state = arg0->state + 1;
}

void func_shelter_1f_heliport_80180748(void)
{
    func_shelter_1f_heliport_801807C0();
}
