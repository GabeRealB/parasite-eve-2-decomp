#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3688.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/pad.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

extern void Room_SaveUi01(Task* task);

extern UiObject*    D_80067634;
extern UiObjectDesc D_800611E4;
extern UiList       D_acropolis_fire_escape_80181CF4;
void                func_acropolis_fire_escape_8017E298(UiList* list, UiObject* obj);
void                func_acropolis_fire_escape_8017E594(UiList* list, UiObject* obj);

extern UiList       D_acropolis_fire_escape_80181C6C;
extern UiObjectDesc D_acropolis_fire_escape_80181C90;

extern u16 D_80072174;
extern s8  D_80072176;
extern u16 D_80072834;
extern u16 D_80072836;
extern u8  D_80072A93;
extern s32 D_80072A94;
extern s32 D_80072A98;

extern u8 D_acropolis_fire_escape_80181A20[];
extern u8 D_acropolis_fire_escape_80181A28[];
extern u8 D_acropolis_fire_escape_80181A2C[];
extern u8 D_acropolis_fire_escape_80181A34[];
extern u8 D_acropolis_fire_escape_80181A40[];
extern u8 D_acropolis_fire_escape_80181A50[];
extern u8 D_acropolis_fire_escape_80181A58[];
extern u8 D_acropolis_fire_escape_80181A60[];
extern u8 D_acropolis_fire_escape_80181A68[];
extern u8 D_acropolis_fire_escape_80181A70[];
extern u8 D_acropolis_fire_escape_80181A78[];
extern u8 D_acropolis_fire_escape_80181A7C[];
extern u8 D_acropolis_fire_escape_80181AA8[];
extern u8 D_acropolis_fire_escape_80181ACC[];
extern u8 D_acropolis_fire_escape_80181AFC[];
extern u8 D_acropolis_fire_escape_80181B30[];
extern u8 D_acropolis_fire_escape_80181B64[];
extern u8 D_acropolis_fire_escape_80181B9C[];
extern u8 D_acropolis_fire_escape_80181BD0[];
extern u8 D_acropolis_fire_escape_80181C08[];

void func_acropolis_fire_escape_8017D6D0(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_acropolis_fire_escape_80181A7C,
                D_acropolis_fire_escape_80181AA8,
                D_acropolis_fire_escape_80181ACC,
                D_acropolis_fire_escape_80181AFC,
                D_acropolis_fire_escape_80181B30,
                D_acropolis_fire_escape_80181B64,
                D_acropolis_fire_escape_80181B9C,
                D_acropolis_fire_escape_80181BD0,
                D_acropolis_fire_escape_80181C08,
            };

            Ui_SetHolderParam((s32)tbl[arg0->field_8], 0, 0);
        }
    }

    switch (arg0->field_8) {
        case 0: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_fire_escape_80181A20);
            Text_FormatTime(p, D_80072174);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 1: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_fire_escape_80181A50);
            Text_ItoaUnsigned(p, D_80072A93);
            Text_Strcat(p, D_acropolis_fire_escape_80181A70);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 2: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_fire_escape_80181A28);
            Text_ItoaUnsigned(p, D_80072834);
            Text_Strcat(p, D_acropolis_fire_escape_80181A70);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 3: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_fire_escape_80181A2C);
            Text_ItoaUnsigned(p, D_80072836);
            Text_Strcat(p, D_acropolis_fire_escape_80181A70);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 4: {
            TextDrawReq req;
            s32         y;
            s32         pct;
            s32         len;
            s32         n;
            s32         i;
            u8*         q;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_fire_escape_80181A34);
            if (Mc_SaveData.field_6CC == 0) {
                pct = 0;
            } else {
                pct = (Mc_SaveData.field_6CC * 10000) / (Mc_SaveData.field_6CC + Mc_SaveData.field_6CE);
            }
            if (pct < 100) {
                func_8002F44C(p, pct, 3);
            } else {
                Text_ItoaUnsigned(p, pct);
            }
            n   = 2;
            q   = p;
            len = 0;
            while (*q != 0) {
                q++;
                len++;
            }
            if (len < n) {
                n = len;
            }
            n++;
            for (i = 0; i < n; i++) {
                q[1] = q[0];
                q--;
            }
            q[1] = 0x2E;
            Text_Strcat(p, D_acropolis_fire_escape_80181A78);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 5: {
            TextDrawReq req;
            s32         y;
            s32         pct;
            s32         total;
            s32         cnt;
            s32         len;
            s32         n;
            s32         i;
            u8*         q;

            total          = D_80072834;
            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_fire_escape_80181A40);
            cnt   = 326;
            total = total + (GameFlag_GetNibble(0x167) + GameFlag_GetNibble(0x168));
            if (total == 0) {
                pct = 0;
            } else {
                pct = (total * 10000) / cnt;
            }
            if (pct < 100) {
                func_8002F44C(p, pct, 3);
            } else {
                Text_ItoaUnsigned(p, pct);
            }
            n   = 2;
            q   = p;
            len = 0;
            while (*q != 0) {
                q++;
                len++;
            }
            if (len < n) {
                n = len;
            }
            n++;
            for (i = 0; i < n; i++) {
                q[1] = q[0];
                q--;
            }
            q[1] = 0x2E;
            Text_Strcat(p, D_acropolis_fire_escape_80181A78);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            Ui_DrawHBar((UiPanel*)arg1, arg1->field_1C, (s16)arg1->field_1E, arg0->field_1A + 3);
            arg0->field_1A = (u16)arg0->field_1A + 5;
            break;
        }
        case 6: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_fire_escape_80181A58);
            Text_ItoaUnsigned(p, D_80072176);
            Text_Strcat(p, D_acropolis_fire_escape_80181A70);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 7: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_fire_escape_80181A60);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A94), arg0->field_1C, 3, 2);
            break;
        }
        case 8: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_fire_escape_80181A68);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A98), arg0->field_1C, 3, 2);
            break;
        }
    }
}
INCLUDE_RODATA("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", D_acropolis_fire_escape_8017D610);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", func_acropolis_fire_escape_8017DE9C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", func_acropolis_fire_escape_8017E298);

INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", func_acropolis_fire_escape_8017E594);

void func_acropolis_fire_escape_8017E8B4(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_acropolis_fire_escape_80181C6C;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, "Weapon Data");
    } else {
        Ui_DrawText((UiPanel*)obj, "PE Data");
    }
    if (task->state == 0) {
        work = Mem_Calloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->idMap = work;
        Ui_SpawnFromDesc(&D_acropolis_fire_escape_80181C90, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_acropolis_fire_escape_8017E298(list, obj);
        } else {
            func_acropolis_fire_escape_8017E594(list, obj);
        }
        Ui_InitList(list, (UiMiniObj*)obj);
        list->field_A = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
    if (task->firstChild != NULL) {
        child = task->firstChild;
        do {
            childObj = child->spawnArg2;
            next     = child->nextSibling;
            if (childObj->field_2E == -1 || childObj->field_2E == 6) {
                Ui_TeardownTree(childObj, childObj->owner);
                obj->status = 1;
            }
            child = next;
        } while (child != task->firstChild);
    }
}
void func_acropolis_fire_escape_8017EA68(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s32       ready;
    s32       sel;
    s32       kind;
    s32       mode;
    s32       one;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    ready         = Mc_SaveData.field_23 == 1;
    list          = &D_acropolis_fire_escape_80181CF4;
    one           = 1;
    if (Mc_SaveData.field_E > 0) {
        ready = one;
    }
    if (ready == 0) {
        if (task->state == 0) {
            Game_Session->field_2 = one;
            Ui_SpawnFromDesc(&D_800611E4, 0, 0, 0, obj);
            obj->status   = 0;
            obj->field_4 |= 0x80000000;
            task->state   = task->state + 1;
        }
    } else if (task->state == 0) {
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        obj->status           = one;
        Game_Session->field_2 = one;
        Ui_SetListScrollFlag(list, 1);
        Gp_ClearPreviewItems();
        D_80067634   = NULL;
        Wip_UiHolder = NULL;
        task->state  = task->state + 1;
    } else {
        /* The literal carries its trailing "\0\1" - the room's rodata has
         * those two bytes right after the string and nothing else claims them. */
        Ui_DrawText((UiPanel*)obj, "Telephone\000\001");
        Ui_UpdateListNoAnim(list, obj);
    }
    if (obj->field_2E == 6) {
        obj->field_2E = 0;
        Ui_SetState4((Task*)obj, task);
        obj->status = 0;
    }
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        if (task->state != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
        }
        Game_Session->field_2 = 0;
        obj->field_2E         = -1;
        obj->field_2C         = 0x34;
    }
    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
        sel      = childObj->field_2E;
        switch (sel) {
            case 6:
                if (task->state == 1) {
                    kind = childObj->field_2C;
                    Ui_TeardownTree(childObj, childObj->owner);
                    mode = 0xF;
                    if (kind == 0x33) {
                        mode = 0x11;
                    }
                    Gp_SpawnItemPrompt(obj, mode, 0, 1);
                    if (ready == 0) {
                        task->state = 3;
                    } else {
                        task->state = 2;
                    }
                } else if (task->state == 3) {
                    obj->field_2E = -1;
                    obj->field_2C = 0x34;
                } else {
                    Ui_TeardownTree(childObj, childObj->owner);
                    SndEvt_EnqueueType6(0x3B, 0, 0);
                    Ui_StartCloseAnim((UiPanel*)obj, task);
                    obj->status = 1;
                }
                break;
            case -1:
                if (task->state == 1) {
                    kind = childObj->field_2C;
                    Ui_TeardownTree(childObj, childObj->owner);
                    mode = 0xF;
                    if (kind == 0x33) {
                        mode = 0x11;
                    }
                    Gp_SpawnItemPrompt(obj, mode, 0, 1);
                    if (ready == 0) {
                        task->state = 3;
                    } else {
                        task->state = 2;
                    }
                } else {
                    obj->field_2E = -1;
                    obj->field_2C = 0x34;
                }
                break;
        }
    }
}
INCLUDE_ASM("rooms/nonmatchings/acropolis_fire_escape/acropolis_fire_escape", func_acropolis_fire_escape_8017ED60);