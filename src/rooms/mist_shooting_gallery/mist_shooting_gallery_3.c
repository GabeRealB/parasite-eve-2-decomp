#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

#include "rooms/mist_shooting_gallery.h"

#include <psyq/libgte.h>

extern u8 D_80062737;
/// Screen-fade "overlay owns the display" flag, first byte of the flag block
/// at 0x80071068. Declared as an array on purpose: GCC 2.8.1 exempts a
/// *fixed-address scalar* store from aliasing with a varying-address struct
/// load, so a plain `extern s8` here lets the scheduler hoist the following
/// `arg0->state` load above the store. Indexing an array makes the store a
/// struct reference and keeps the two in order.
extern s8  D_80071068[];
extern s8  D_8007106B;
extern s16 D_8007A396;
extern s8  D_80072176;
extern s8  D_80072177;
/// "SELECT" — the panel title, owned by `mist_shooting_gallery.c`'s rodata.
extern char         RoomsShared8017e28cTitle[];
extern UiObjectDesc D_mist_shooting_gallery_8018535C;
extern UiList       RoomsShared8017e28cMenu;
extern TaskDesc     D_mist_shooting_gallery_80185378;
extern TaskDesc     D_mist_shooting_gallery_80185384;
extern SVECTOR      D_mist_shooting_gallery_80185550[];
extern SVECTOR      D_mist_shooting_gallery_80185570[];
extern SVECTOR      D_mist_shooting_gallery_801855C0[];
extern SVECTOR      D_mist_shooting_gallery_801855F0[];
extern SVECTOR      D_mist_shooting_gallery_80185610[];
extern SVECTOR      D_mist_shooting_gallery_80185670[];
extern SVECTOR      D_mist_shooting_gallery_80185678[];
extern SVECTOR      D_mist_shooting_gallery_80185680[];
extern SVECTOR      D_mist_shooting_gallery_80185688[];
extern SVECTOR      D_mist_shooting_gallery_80185690[];
extern SVECTOR      D_mist_shooting_gallery_801856B0[];
void                Room_Draw01(SVECTOR* v, s32 arg1, s32 arg2);
void                Room_Draw31(SVECTOR* v, s32 arg1, s32 arg2);

void func_mist_shooting_gallery_80180A00(Task* task)
{
    UiObject* obj;
    s16       result;

    if (task->state == 0) {
        Display_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_mist_shooting_gallery_8018535C, task->spawnArg1, 1, 1, NULL);
        if (obj == NULL) {
            return;
        }
        GameMain_SetFrameTiming(0);
        Game_Session->field_2 = 1;
        task->spawnArg2       = obj;
        task->state          += 1;
    }

    if (task->state == 1) {
        obj    = task->spawnArg2;
        result = obj->field_2E;
        if ((result == -1) || (result == 6)) {
            Ui_TeardownTree(obj, obj->owner);
            task->killCountdown = 0xA;
            task->state         = 2;
        }
    }

    if (task->state == 2) {
        task->killCountdown -= 1;
        if (task->killCountdown <= 0) {
            GameMain_SetFrameTiming(1);
            Game_Session->field_2 = 0;
            Task_Kill(task);
            Stage_ReleasePrimBuf();
            Stage_SetEndingFlag();
        }
    }
}

s32 func_mist_shooting_gallery_80180B34(void)
{
    Display_InitModeObj(&D_mist_shooting_gallery_80185378, 0, 0, 0);
    return 1;
}

void func_mist_shooting_gallery_80180B64(Task* arg0)
{
    u8 param1[8];
    u8 param2[8];

    switch (arg0->state) {
        case 0:
            param1[3] = 0;
            param1[2] = 0;
            param2[0] = 0;
            param2[1] = 0;
            param2[2] = 0;
            param2[3] = 0;
            switch (D_80072177) {
                case 0:
                    param1[0] = 0x29;
                    break;
                case 1:
                    param1[0] = 0x2A;
                    break;
                case 2:
                    param1[0] = 0x2B;
                    break;
                case 3:
                    param1[0] = 0x2C;
                    break;
            }
            CdCmd_Enqueue(0x21, param1, param2);
            arg0->state++;
            return;

        case 1:
            if (CdCmd_IsIdle() & 0xFFFF) {
                D_80071068[0]       = 1;
                arg0->killCountdown = 0;
                arg0->state++;
                return;
            }
            return;

        case 2: {
            TILE*     p;
            DR_TPAGE* dr;
            u8        color;

            p              = (TILE*)Gpu_PrimCursor;
            color          = ~(u8)arg0->killCountdown;
            Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
            setlen(p, 3);
            setcode(p, 0x62);
            p->r0 = color;
            p->g0 = color;
            p->b0 = color;
            p->x0 = -0xA0;
            p->y0 = -0x78;
            p->w  = 0x140;
            p->h  = 0xF0;

            addPrim(Gpu_CurrentOt, p);
            dr             = Gpu_PrimCursor;
            Gpu_PrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
            addPrim(Gpu_CurrentOt, dr);

            arg0->killCountdown += 8;
            if (arg0->killCountdown >= 0x11) {
                SetDispMask(1);
            }
            if (arg0->killCountdown < 0x100) {
                return;
            }
            arg0->killCountdown = 0;
            arg0->state++;
            return;
        }

        case 3:
            arg0->killCountdown += 1;
            if (arg0->killCountdown < 0x97 && Pad_CheckFlag800() == 0) {
                return;
            }
            arg0->killCountdown = 0;
            arg0->state++;
            return;

        case 4: {
            TILE*     p;
            DR_TPAGE* dr;
            u8        color;

            p              = (TILE*)Gpu_PrimCursor;
            color          = (u8)arg0->killCountdown;
            Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
            setlen(p, 3);
            setcode(p, 0x62);
            p->r0 = color;
            p->g0 = color;
            p->b0 = color;
            p->x0 = -0xA0;
            p->y0 = -0x78;
            p->w  = 0x140;
            p->h  = 0xF0;

            addPrim(Gpu_CurrentOt, p);
            dr             = Gpu_PrimCursor;
            Gpu_PrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
            addPrim(Gpu_CurrentOt, dr);

            arg0->killCountdown += 8;
            if (arg0->killCountdown < 0x100) {
                return;
            }
            Mem_Set(Fs_ImgBuffers, 0, 0x25800);
            SetDispMask(0);
            arg0->state++;
            return;
        }

        case 5:
            D_80071068[0] = 0;
            Task_Kill(arg0);
            Display_ResetHeapWrapper();
            return;

        default:
            return;
    }
}

void func_mist_shooting_gallery_80180F2C(Task* arg0)
{
    u8          slotParam[4];
    GBytes8     key;
    s16         slot;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = ((SessionBytesAt4*)Game_Session)->field_4;
    key.data[0]  = 0x64;
    slot         = Stream_FindSlot(key.data, 0, 0);
    slotParam[0] = slot;
    CdCmd_Enqueue(0x61, 0, slotParam);
    goto advance;

L_case2:
    if (queue->field_1FA == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    Display_State.field_100 = 1;
    Mem_Set(Fs_ImgBuffers, 0, 0x25800);
    Task_Kill(task);
    Display_ResetHeapWrapper();
}

void func_mist_shooting_gallery_801810D8(Task* task)
{
    switch (task->state) {
        case 0:
            SetDispMask(0);
            if (D_80072176 == 0) {
                task->state = 2;
                return;
            }
            Display_SpawnWithOt(&D_mist_shooting_gallery_80185384, 2, 0, 0);
            D_8007106B = 0;
            Gp_SpawnViewTasks();
        case 1:
            task->state = task->state + 1;
            return;
        case 2:
            Display_SpawnWithOt(&D_mist_shooting_gallery_80185384, 1, 0, 0);
            D_8007106B = 1;
            Gp_SpawnViewTasks();
            Task_Kill(task);
            return;
    }
}
