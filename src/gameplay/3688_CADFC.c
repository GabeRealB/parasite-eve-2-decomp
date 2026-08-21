#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/ui.h"

extern UiListItemFunc D_8010EA18[];

void func_800CAB40(DialogPrompt* arg0, UiObject* arg1);
void func_800CAC88(DialogPrompt* arg0, UiObject* arg1);
void func_800CF88C(DialogPrompt* arg0, UiObject* arg1);
void func_800CF940(DialogPrompt* arg0, UiObject* arg1);
void func_800D5338(DialogPrompt* arg0, UiObject* arg1);

void func_800CADFC(UiList* arg0, UiObject* arg1, s32 arg2, GpItemRec* arg3)
{
    s32 n;
    s32 mode;

    n              = 0;
    mode           = arg1->owner->spawnArg1;
    arg1->field_10 = 0x60;
    switch (mode) {
        case 0:
            if (arg2 == 0) {
                D_8010EA18[n++] = func_800CF88C;
            } else if ((u32)(arg2 - 0x80) < 0x20U) {
                D_8010EA18[n++] = func_800CF88C;
                D_8010EA18[n++] = func_800D5338;
            } else if ((u32)(arg2 - 0x60) < 0x20U) {
                D_8010EA18[n++] = func_800CF88C;
                D_8010EA18[n++] = func_800D5338;
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                if ((arg3->field_2 - func_800BAFF4(&Mc_SaveData.field_5BC, arg2)) > 0) {
                    D_8010EA18[n++] = func_800CAB40;
                }
                D_8010EA18[n++] = func_800CF88C;
                D_8010EA18[n++] = func_800D5338;
            } else {
                D_8010EA18[n++] = func_800CF7C4;
                D_8010EA18[n++] = func_800CF88C;
                D_8010EA18[n++] = func_800D5338;
            }
            break;
        case 1:
            if (arg2 != 0) {
                if ((u32)(arg2 - 0x80) < 0x20U) {
                    D_8010EA18[n++] = func_800CAC88;
                }
            }
            break;
        case 2:
            if (arg2 == 0) {
                D_8010EA18[n++] = func_800CAC88;
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                D_8010EA18[n++] = func_800CAC88;
            }
            break;
        case 3:
            if (arg2 != 0) {
                if ((u32)(arg2 - 0x60) < 0x20U) {
                    D_8010EA18[n++] = func_800CAC88;
                }
            }
            break;
        case 4:
            if (arg2 == 0) {
                D_8010EA18[n++] = func_800CF940;
            } else if ((u32)(arg2 - 0x80) < 0x20U) {
                D_8010EA18[n++] = func_800CF940;
                if ((arg2 != 0x92) && (arg2 != 0x95)) {
                    D_8010EA18[n++] = func_800CAB40;
                }
                D_8010EA18[n++] = func_800D5338;
            } else if ((u32)(arg2 - 0x60) < 0x20U) {
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                D_8010EA18[n++] = func_800CF940;
                if ((arg3->field_2 - func_800BAFF4(&Mc_SaveData.field_5BC, arg2)) > 0) {
                    D_8010EA18[n++] = func_800CAB40;
                }
                D_8010EA18[n++] = func_800D5338;
            } else {
                D_8010EA18[n++] = func_800CF940;
                D_8010EA18[n++] = func_800CF7C4;
                D_8010EA18[n++] = func_800D5338;
            }
            break;
    }
    arg0->field_4 = n;
    arg0->field_5 = n;
}
