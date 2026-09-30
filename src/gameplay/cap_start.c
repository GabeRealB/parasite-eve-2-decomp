#include "captions.h"

#include "types.h"

#include "gameplay/cap.h"
#include "gameplay/captions.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/task.h"

GpEvt12* Gp_CapTable;

s16 D_801155AC;

u16 D_801155AE;

s16 D_801155B0;

s16 D_801155B2;

s16 D_801155B4;

s16 D_801155B6;

u8 D_801155B8;

s8 D_801155B9;

u8 D_801155BA;

u8 D_801155BB;

s16 D_801155BC;

s16 D_801155BE;

s16 D_801155C0;

void func_807245B8(void);

s32 Gp_StartCap(GpEvt12* arg0, s16 arg1, s16 arg2)
{
    CdCmdQueue* queue;
    TaskDesc*   desc;

    queue = &gCdCmdQueue;
    if (arg0 == 0) {
        return 0;
    }

    Gp_CapEventKey = arg2;
    Gp_CapTable    = arg0;
    D_801155AC     = 0;
    D_801155AE     = 1;
    D_801155B0     = 0;
    D_801155B2     = 0x30;
    D_801155B4     = 0xC0;
    D_801155B8     = 7;
    D_801155B2     = 0x140;
    D_80115664     = 0;
    D_8011569A     = 0;
    D_80115698     = 0;
    D_8011567A     = 0;
    D_801155C0     = 0;
    D_801156A8     = 0;
    D_801155BC     = 0;
    D_8011566E     = 0;
    D_8011566F     = 0;
    D_801155BA     = 0;
    D_801155BB     = 0;
    D_80115648     = 0;
    D_8011566A     = 0;
    D_8011565A     = 0;
    D_80115688     = 0;
    D_80115690     = 0;
    D_80115680     = 1;
    D_80115659     = 0xF;
    D_8011566C     = Mc_SaveData[0].state.at4.loc.view;
    D_8011565C     = queue->imageMdecMode;
    if (gDisplayState.debugMode != 0) {
        func_807245B8();
        D_8011564A = -1;
    }

    D_801155AE = Gp_FindCapEvt((s16)D_801155AE);
    if (Gp_CapTable[(s16)D_801155AE].field_8.offset == -1) {
        Gp_CapTable = 0;
        return 0;
    }

    Gp_ApplyCapEvtFlags();
    D_801155B4 = Gp_CapTextTopY(Gp_CapTable[(s16)D_801155AE].field_8.text);
    D_801155B6 = Gp_CapTextHeight(Gp_CapTable[(s16)D_801155AE].field_8.text);
    D_80115666 = arg1;
    D_80115660 = 0;
    if (arg1 != 0) {
        desc       = Task_GetDesc(2, 7);
        Gp_CapTask = Display_InitModeObj(desc, 0, 0, 0);
        if (D_80115666 != 3) {
            return 0;
        }
        Task_SpawnFromTable(D_8010FB4C, 0, 0, 0);
        D_80115666 = 1;
    } else {
        Gp_CapTask = Task_Spawn(2, 7, 0, 0);
    }
    return 0;
}
