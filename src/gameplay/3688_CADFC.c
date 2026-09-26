#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/ui.h"

extern UiListItemFunc Gp_ItemCmdFns[];

void Gp_DrawLoadCmd(DialogPrompt* arg0, UiObject* arg1);
void Gp_DrawExchangeCmd(DialogPrompt* arg0, UiObject* arg1);
void Gp_DrawMovePrompt(DialogPrompt* arg0, UiObject* arg1);
void Gp_DrawExchangeSlotCmd(DialogPrompt* arg0, UiObject* arg1);
void Gp_DrawDiscardCmd(DialogPrompt* arg0, UiObject* arg1);

void Gp_BuildItemCmdList(UiList* arg0, UiObject* arg1, s32 arg2, McItemRec* arg3)
{
    s32 n;
    s32 mode;

    n              = 0;
    mode           = arg1->owner->spawnArg1;
    arg1->field_10 = 0x60;
    switch (mode) {
        case 0:
            if (arg2 == 0) {
                Gp_ItemCmdFns[n++] = Gp_DrawMovePrompt;
            } else if ((u32)(arg2 - 0x80) < 0x20U) {
                Gp_ItemCmdFns[n++] = Gp_DrawMovePrompt;
                Gp_ItemCmdFns[n++] = Gp_DrawDiscardCmd;
            } else if ((u32)(arg2 - 0x60) < 0x20U) {
                Gp_ItemCmdFns[n++] = Gp_DrawMovePrompt;
                Gp_ItemCmdFns[n++] = Gp_DrawDiscardCmd;
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                if ((arg3->qty - Gp_CountEquippedRelated(&Mc_SaveData.carriedItems, arg2)) > 0) {
                    Gp_ItemCmdFns[n++] = Gp_DrawLoadCmd;
                }
                Gp_ItemCmdFns[n++] = Gp_DrawMovePrompt;
                Gp_ItemCmdFns[n++] = Gp_DrawDiscardCmd;
            } else {
                Gp_ItemCmdFns[n++] = Gp_DrawUsePrompt;
                Gp_ItemCmdFns[n++] = Gp_DrawMovePrompt;
                Gp_ItemCmdFns[n++] = Gp_DrawDiscardCmd;
            }
            break;
        case 1:
            if (arg2 != 0) {
                if ((u32)(arg2 - 0x80) < 0x20U) {
                    Gp_ItemCmdFns[n++] = Gp_DrawExchangeCmd;
                }
            }
            break;
        case 2:
            if (arg2 == 0) {
                Gp_ItemCmdFns[n++] = Gp_DrawExchangeCmd;
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                Gp_ItemCmdFns[n++] = Gp_DrawExchangeCmd;
            }
            break;
        case 3:
            if (arg2 != 0) {
                if ((u32)(arg2 - 0x60) < 0x20U) {
                    Gp_ItemCmdFns[n++] = Gp_DrawExchangeCmd;
                }
            }
            break;
        case 4:
            if (arg2 == 0) {
                Gp_ItemCmdFns[n++] = Gp_DrawExchangeSlotCmd;
            } else if ((u32)(arg2 - 0x80) < 0x20U) {
                Gp_ItemCmdFns[n++] = Gp_DrawExchangeSlotCmd;
                if ((arg2 != 0x92) && (arg2 != 0x95)) {
                    Gp_ItemCmdFns[n++] = Gp_DrawLoadCmd;
                }
                Gp_ItemCmdFns[n++] = Gp_DrawDiscardCmd;
            } else if ((u32)(arg2 - 0x60) < 0x20U) {
            } else if ((u32)(arg2 - 0xA0) < 0x20U) {
                Gp_ItemCmdFns[n++] = Gp_DrawExchangeSlotCmd;
                if ((arg3->qty - Gp_CountEquippedRelated(&Mc_SaveData.carriedItems, arg2)) > 0) {
                    Gp_ItemCmdFns[n++] = Gp_DrawLoadCmd;
                }
                Gp_ItemCmdFns[n++] = Gp_DrawDiscardCmd;
            } else {
                Gp_ItemCmdFns[n++] = Gp_DrawExchangeSlotCmd;
                Gp_ItemCmdFns[n++] = Gp_DrawUsePrompt;
                Gp_ItemCmdFns[n++] = Gp_DrawDiscardCmd;
            }
            break;
    }
    arg0->field_4 = n;
    arg0->field_5 = n;
}

const char             Gp_StrStatus[]  = "Status";
const GpUseCreateTable D_80097184      = { {
    { 0x8F, 0x00 },
    { 0x93, 0x0A },
    { 0x94, 0x0A },
    { 0x98, 0x42 },
    { 0x9A, 0x45 },
    { 0x99, 0x46 },
    { 0x9B, 0x43 },
    { 0x9C, 0x44 },
} };
const char             Gp_StrInvoke[]  = "Invoke";
const char             Gp_StrPeList[]  = "PE LIST";
const u8               D_800971A4      = 0;
const char             Gp_StrTotal2[]  = "TOTAL";
const char             Gp_StrMessage[] = "Message";
const char             Gp_StrWarning[] = "Warning";

const TaskFuncTable4 Gp_MapTaskStates = { {
    Gp_MapPanelInit,
    Gp_MapFirstDrawTask,
    Gp_MapTaskState2,
    Gp_MapDrawTask,
} };

/// "Help". The three bytes after the terminator are not zero: the original
/// toolchain left them in the alignment gap.
const char Gp_StrHelp[8]      = "Help\0a~\xC6";
const char Gp_StrUse2[]       = "Use";
const char Gp_StrKeyItem2[]   = "Key Item";
const char Gp_StrMap[]        = "Map";
const char Gp_StrAttention2[] = "Attention";
const char Gp_StrNotice3[]    = "Notice";
const char Gp_StrNextLevel[]  = "Next Level";
const char D_8009720C[]       = "EXP";
const char Gp_StrCost[]       = "COST";
const char Gp_StrBonus[]      = "BONUS";
const char D_80097220[]       = "MP";
/// "Specifications". The byte after the terminator is not zero: the original
/// toolchain left it in the alignment gap.
const char Gp_StrSpecs2[16] = "Specifications\0\"";
