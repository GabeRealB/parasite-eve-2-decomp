#include "common.h"

#include <psyq/libgte.h>

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

extern u8 D_80062737;
/// Screen-fade "overlay owns the display" flag, first byte of the flag block
/// at 0x80071068. Declared as an array on purpose: GCC 2.8.1 exempts a
/// *fixed-address scalar* store from aliasing with a varying-address struct
/// load, so a plain `extern s8` here lets the scheduler hoist the following
/// `arg0->state` load above the store. Indexing an array makes the store a
/// struct reference and keeps the two in order.
extern s8       D_80071068[];
extern s8       D_8007106B;
extern s8       D_8007272D;
extern s16      D_8007A396;
extern s8       D_80073BAE;
extern s8       D_80072176;
extern s8       D_80072177;
extern s16      D_80114D08;
extern s32      D_8014D038;
extern TaskDesc D_8014E13C;
extern s32      D_80153274;
extern s32      D_80153D6C;

extern s32  func_8014AA54(GpSaveLoc* loc);
extern void func_8014AB6C(void);
extern void func_8014AF0C(void);

extern MistShootingGalleryCourseMenu D_mist_shooting_gallery_8017DADC;

/// "SELECT" — the panel title, owned by `mist_shooting_gallery.c`'s rodata.
extern char D_mist_shooting_gallery_8017DB04[];

extern MistShootingGalleryLayout D_mist_shooting_gallery_80185198;
extern MistShootingGalleryLayout D_mist_shooting_gallery_801851F8;
extern MistShootingGalleryLayout D_mist_shooting_gallery_80189968;

extern void*        D_mist_shooting_gallery_801853C0;
extern u32          D_mist_shooting_gallery_8018D1B4[];
extern u32          D_mist_shooting_gallery_8018DF38[];
extern UiObjectDesc D_mist_shooting_gallery_8018535C;
extern UiList       D_mist_shooting_gallery_80185338;
extern TaskDesc     D_mist_shooting_gallery_801850DC;
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

void Room_Draw01(SVECTOR* v, s32 arg1, s32 arg2);
void Room_Draw31(SVECTOR* v, s32 arg1, s32 arg2);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_8017FEB8);
s32 func_mist_shooting_gallery_80180000(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 >= 5) {
        if (arg2 >= 9) {
            if (arg2 < 0x23) {
                if (arg2 >= 0x21) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(&D_mist_shooting_gallery_801850DC, 0, arg2, 3);
                }
            }
        } else {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_mist_shooting_gallery_801850DC, 0, arg2, 1);
        }
    }
    return 0;
}
s32 func_mist_shooting_gallery_8018008C(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if ((arg2->field_2 == 1) && (D_8014D038 == 0)) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(&D_8014E13C, 1, 1, 0);
        D_80114D08 = 0xA;
    }
    if ((arg2->field_2 == 2) && (GameFlag_GetNibble(0xED) == 0)) {
        func_8014AF0C();
    }
    if (arg2->field_2 == 3) {
        func_8014AB6C();
    }
    if ((arg2->field_2 == 4) && (GameFlag_GetNibble(0x106) == 0)) {
        func_800E3FAC(0xA2, 0x3B);
        GameFlag_SetNibble(0x106, 1);
        func_800E8634((s32)&D_80153274, 0, (s32)&D_80153D6C);
    }
    return 0;
}
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_8018018C);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_801801E4);
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_80180390);
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_8018055C);
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_80180728);
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

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_80180B64);
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
void func_mist_shooting_gallery_801811C0(s16 arg0)
{
    if (arg0 == 0) {
        D_mist_shooting_gallery_801853C0 = D_mist_shooting_gallery_8018D1B4;
        return;
    }
    D_mist_shooting_gallery_801853C0 = D_mist_shooting_gallery_8018DF38;
}
void func_mist_shooting_gallery_801811EC(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            Room_Draw01(&D_mist_shooting_gallery_80185550[0], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185550[8], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185550[10], 0x200, 0x222);
            break;
        case 3:
            Room_Draw01(&D_mist_shooting_gallery_80185570[0], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185570[2], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185570[10], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185570[12], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_80185570[14], 0x200, 0x222);
            break;
        case 7:
            Room_Draw01(&D_mist_shooting_gallery_801855C0[0], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855C0[2], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855C0[4], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855C0[6], 0x200, 0x222);
            break;
        case 8:
            Room_Draw01(&D_mist_shooting_gallery_80185610[0], 0x200, 0x222);
            break;
        case 9:
        case 18:
            Room_Draw01(&D_mist_shooting_gallery_801855F0[0], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855F0[2], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855F0[6], 0x200, 0x222);
            Room_Draw01(&D_mist_shooting_gallery_801855F0[8], 0x200, 0x222);
            Room_Draw31(&D_mist_shooting_gallery_801855F0[16], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_801856B0[0], 0x300, 0x111);
            break;
        case 10:
            Room_Draw31(&D_mist_shooting_gallery_80185678[0], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185678[2], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185678[4], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185678[6], 0x300, 0x111);
            break;
        case 11:
            Room_Draw31(&D_mist_shooting_gallery_80185680[0], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185680[1], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185680[3], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185680[4], 0x300, 0x111);
            break;
        case 12:
            Room_Draw31(&D_mist_shooting_gallery_80185690[0], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185690[1], 0x300, 0x111);
            break;
        case 13:
            Room_Draw31(&D_mist_shooting_gallery_80185688[0], 0x300, 0x111);
            break;
        case 14:
            Room_Draw31(&D_mist_shooting_gallery_80185670[0], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185670[1], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185670[3], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185670[5], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_80185670[7], 0x300, 0x111);
            Room_Draw31(&D_mist_shooting_gallery_801856B0[0], 0x300, 0x111);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", D_mist_shooting_gallery_8017DB80);

INCLUDE_RODATA("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", D_mist_shooting_gallery_8017DB8C);
