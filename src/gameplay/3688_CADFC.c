#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/ui.h"

extern UiListItemFunc D_8010EA18[];

void Gp_DrawLoadCmd(DialogPrompt* arg0, UiObject* arg1);
void Gp_DrawExchangeCmd(DialogPrompt* arg0, UiObject* arg1);
void Gp_DrawMovePrompt(DialogPrompt* arg0, UiObject* arg1);
void Gp_DrawExchangeSlotCmd(DialogPrompt* arg0, UiObject* arg1);
void Gp_DrawDiscardCmd(DialogPrompt* arg0, UiObject* arg1);

void Gp_BuildItemCmdList(UiList* arg0, UiObject* arg1, s32 arg2, GpItemRec* arg3)
{
    s32 n;
    s32 mode;

    n              = 0;
    mode           = arg1->owner->spawnArg1;
    arg1->field_10 = 0x60;
    switch (mode) {
        case 0:
            if (arg2 == 0) {
                D_8010EA18[n++] = Gp_DrawMovePrompt;
            } else if ((u32)(arg2 - 0x80) < 0x20U) {
                D_8010EA18[n++] = Gp_DrawMovePrompt;
                D_8010EA18[n++] = Gp_DrawDiscardCmd;
            } else if ((u32)(arg2 - 0x60) < 0x20U) {
                D_8010EA18[n++] = Gp_DrawMovePrompt;
                D_8010EA18[n++] = Gp_DrawDiscardCmd;
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                if ((arg3->field_2 - Gp_CountEquippedRelated(&Mc_SaveData.field_5BC, arg2)) > 0) {
                    D_8010EA18[n++] = Gp_DrawLoadCmd;
                }
                D_8010EA18[n++] = Gp_DrawMovePrompt;
                D_8010EA18[n++] = Gp_DrawDiscardCmd;
            } else {
                D_8010EA18[n++] = Gp_DrawUsePrompt;
                D_8010EA18[n++] = Gp_DrawMovePrompt;
                D_8010EA18[n++] = Gp_DrawDiscardCmd;
            }
            break;
        case 1:
            if (arg2 != 0) {
                if ((u32)(arg2 - 0x80) < 0x20U) {
                    D_8010EA18[n++] = Gp_DrawExchangeCmd;
                }
            }
            break;
        case 2:
            if (arg2 == 0) {
                D_8010EA18[n++] = Gp_DrawExchangeCmd;
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                D_8010EA18[n++] = Gp_DrawExchangeCmd;
            }
            break;
        case 3:
            if (arg2 != 0) {
                if ((u32)(arg2 - 0x60) < 0x20U) {
                    D_8010EA18[n++] = Gp_DrawExchangeCmd;
                }
            }
            break;
        case 4:
            if (arg2 == 0) {
                D_8010EA18[n++] = Gp_DrawExchangeSlotCmd;
            } else if ((u32)(arg2 - 0x80) < 0x20U) {
                D_8010EA18[n++] = Gp_DrawExchangeSlotCmd;
                if ((arg2 != 0x92) && (arg2 != 0x95)) {
                    D_8010EA18[n++] = Gp_DrawLoadCmd;
                }
                D_8010EA18[n++] = Gp_DrawDiscardCmd;
            } else if ((u32)(arg2 - 0x60) < 0x20U) {
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                D_8010EA18[n++] = Gp_DrawExchangeSlotCmd;
                if ((arg3->field_2 - Gp_CountEquippedRelated(&Mc_SaveData.field_5BC, arg2)) > 0) {
                    D_8010EA18[n++] = Gp_DrawLoadCmd;
                }
                D_8010EA18[n++] = Gp_DrawDiscardCmd;
            } else {
                D_8010EA18[n++] = Gp_DrawExchangeSlotCmd;
                D_8010EA18[n++] = Gp_DrawUsePrompt;
                D_8010EA18[n++] = Gp_DrawDiscardCmd;
            }
            break;
    }
    arg0->field_4 = n;
    arg0->field_5 = n;
}
