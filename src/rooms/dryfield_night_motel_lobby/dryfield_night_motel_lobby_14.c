#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/dryfield_night_motel_lobby.h"
#include "rooms/room_common.h"

void func_dryfield_night_motel_lobby_8017E310(UiList* list, UiObject* obj);
void func_dryfield_night_motel_lobby_8017E60C(UiList* list, UiObject* obj);

extern UiObject*    D_80067634;
extern UiObjectDesc D_800611E4;

/// Titles of the usage panel `func_dryfield_night_motel_lobby_8017E92C` opens:
/// "Weapon Data" and "PE Data".
extern const char D_dryfield_night_motel_lobby_8017D624[];
extern const char D_dryfield_night_motel_lobby_8017D630[];

/// The usage panel's row list.
extern UiList D_dryfield_night_motel_lobby_801826FC;

/// The "Telephone" title `func_dryfield_night_motel_lobby_8017EAE0` draws over
/// its menu once the menu is open.
extern const char D_dryfield_night_motel_lobby_8017D638[];

/// That menu's entry list.
extern UiList D_dryfield_night_motel_lobby_80182784;

/// Fills the weapon-usage panel's rows from the save's per-weapon use counters
/// (`Mc_SaveData.weaponUseCounts`, item ids 0x80-0x9F).
///
/// Every id with a non-empty name (a leading 0 or 0xA marks an unused row) and
/// a non-zero counter is marked seen and appended to `itemIds`, and the
/// counters are summed. The ids are insertion-sorted by use count, most-used
/// first. Each row then gets `percents`, its share of all uses in hundredths of
/// a percent, rounded, and `barWidths`, its counter as a 12-bit fraction of the
/// top row's. The top counter, the total and the scale are halved (and the
/// shift shortened) until the top counter fits in 17 bits, so neither the
/// multiply nor the shift overflows.
void func_dryfield_night_motel_lobby_8017E310(UiList* list, UiObject* obj)
{
    RoomItemUsage* work;
    s32            count;
    s32            total;
    s32            i;
    s32            j;
    s32            k;
    s32            id;
    s32            tmp;
    s32            uses;
    s32            scale;
    s32            top;
    s32            shift;
    s16*           p;
    u8             c;

    count = 0;
    total = 0;
    work  = (RoomItemUsage*)obj->owner->work;
    p     = work->itemIds;

    for (i = 0; i < 0x20; i++) {
        id = i + 0x80;
        c  = *Gp_GetItemText(id, 0, 1);
        if ((c != 0) && (c != 0xA) && (Mc_SaveData.weaponUseCounts[i] > 0)) {
            Gp_SetItemSeenBit(id, 1);
            *p++ = id;
            count++;
            total += Mc_SaveData.weaponUseCounts[i];
        }
    }

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            uses = Mc_SaveData.weaponUseCounts[work->itemIds[i] - 0x80];
            for (j = 0; j < i; j++) {
                if (Mc_SaveData.weaponUseCounts[work->itemIds[j] - 0x80] < uses) {
                    tmp = work->itemIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->itemIds[k + 1] = work->itemIds[k];
                    }
                    work->itemIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        top   = Mc_SaveData.weaponUseCounts[work->itemIds[0] - 0x80];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            work->percents[i] =
                (u32)((Mc_SaveData.weaponUseCounts[work->itemIds[i] - 0x80] * scale) / total + 1) >> 1;
            work->barWidths[i] =
                (Mc_SaveData.weaponUseCounts[work->itemIds[i] - 0x80] << shift) / top;
        }
    }

    list->field_4  = count;
    list->field_9  = 0;
    list->field_10 = 0;
}

/// Fills the PE-usage panel's rows the same way from the save's per-slot
/// Parasite Energy use counters (`Mc_SaveData.attachUseCounts`, twelve slots).
///
/// Slot `i` owns three consecutive ids from `i * 3 + 0xF`, one per level, and
/// is listed under the id of its current level in `Mc_SaveData.attachLevels`
/// (the base id while that level is 0). The sort, percentages and bar widths
/// follow the weapon panel, with each row's slot recovered from its id.
void func_dryfield_night_motel_lobby_8017E60C(UiList* list, UiObject* obj)
{
    RoomPeUsage* work;
    s16*         p;
    s32          count;
    s32          total;
    s32          i;
    s32          j;
    s32          k;
    s32          id;
    s32          slot;
    s32          uses;
    s32          scale;
    s32          shift;
    s32          top;
    s32          tmp;
    /* Matching only. The original object frames 24 bytes it never touches and
     * materialises GCC's `/3` magic constant before the first loop for a use
     * that no longer survives, so `scratch` buys the frame size and `magic`
     * plus the (instruction-free) SOFT_USE_REG below buy the allocation. */
    s32 magic;
    s16 scratch[12];

    count = 0;
    total = 0;
    i     = 0;
    work  = (RoomPeUsage*)obj->owner->work;
    p     = work->peIds;
    magic = 0x55555556;

    for (; i < 12; i++) {
        if (Mc_SaveData.attachUseCounts[i] > 0) {
            id = i * 3 + 0xF;
            *p = id;
            if (Mc_SaveData.attachLevels[i] != 0) {
                *p = id + (Mc_SaveData.attachLevels[i] - 1u);
            }
            p++;
            count++;
            total += Mc_SaveData.attachUseCounts[i];
        }
    }
    SOFT_USE_REG(magic);

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            slot = (work->peIds[i] - 0xF) / 3;
            uses = Mc_SaveData.attachUseCounts[slot];
            for (j = 0; j < i; j++) {
                slot = (work->peIds[j] - 0xF) / 3;
                if (Mc_SaveData.attachUseCounts[slot] < uses) {
                    tmp = work->peIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->peIds[k + 1] = work->peIds[k];
                    }
                    work->peIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        slot  = (work->peIds[0] - 0xF) / 3;
        top   = Mc_SaveData.attachUseCounts[slot];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            slot               = (work->peIds[i] - 0xF) / 3;
            work->percents[i]  = (u32)((Mc_SaveData.attachUseCounts[slot] * scale) / total + 1) >> 1;
            slot               = (work->peIds[i] - 0xF) / 3;
            work->barWidths[i] = (Mc_SaveData.attachUseCounts[slot] << shift) / top;
        }
    }

    list->field_4  = count;
    list->field_9  = 0;
    list->field_10 = 0;
}

void func_dryfield_night_motel_lobby_8017E92C(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_dryfield_night_motel_lobby_801826FC;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_dryfield_night_motel_lobby_8017D624);
    } else {
        Ui_DrawText((UiPanel*)obj, D_dryfield_night_motel_lobby_8017D630);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_dryfield_night_motel_lobby_80182720, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_dryfield_night_motel_lobby_8017E310(list, obj);
        } else {
            func_dryfield_night_motel_lobby_8017E60C(list, obj);
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

void func_dryfield_night_motel_lobby_8017EAE0(Task* task)
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
    ready         = Mc_SaveData.demoScene == 1;
    list          = &D_dryfield_night_motel_lobby_80182784;
    one           = 1;
    if (Mc_SaveData.clearCount > 0) {
        ready = one;
    }
    if (ready == 0) {
        if (task->state == 0) {
            gGameSession->uiOpen = one;
            Ui_SpawnFromDesc(&D_800611E4, 0, 0, 0, obj);
            obj->status   = 0;
            obj->field_4 |= 0x80000000;
            task->state   = task->state + 1;
        }
    } else if (task->state == 0) {
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        obj->status          = one;
        gGameSession->uiOpen = one;
        Ui_SetListScrollFlag(list, 1);
        Gp_ClearPreviewItems();
        D_80067634   = NULL;
        Wip_UiHolder = NULL;
        task->state  = task->state + 1;
    } else {
        Ui_DrawText((UiPanel*)obj, D_dryfield_night_motel_lobby_8017D638);
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
        gGameSession->uiOpen = 0;
        obj->field_2E        = -1;
        obj->field_2C        = 0x34;
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

void func_dryfield_night_motel_lobby_8017EDD8(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_dryfield_night_motel_lobby_8017F4C8;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into the digit string `str` so that `decimals` digits (at most
/// the string's length) follow it, shifting them and the NUL one byte right.
/// Does nothing when `decimals` is not positive.
void func_dryfield_night_motel_lobby_8017EE34(u8* str, s32 decimals)
{
    s32 len;

    len = 0;
    if (decimals > 0) {
        if (*str != 0) {
            do {
                str += 1;
                len += 1;
            } while (*str != 0);
        }
        if (len < decimals) {
            decimals = len;
            SOFT_TOUCH_REG(decimals);
            decimals += 1;
        } else {
            decimals += 1;
        }
        len = 0;
        if (decimals > 0) {
            do {
                len   += 1;
                str[1] = str[0];
                str   -= 1;
            } while (len < decimals);
        }
        str[1] = '.';
    }
}
