#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/strings.h>

#include "common.h"

#include "replay_bonus_private.h"

#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

void func_replay_bonus_801176A8(UiList* prompt, UiObject* obj);

extern u8           D_replay_bonus_801157A8[];
extern u8           D_replay_bonus_801157B0[];
extern u8           D_replay_bonus_801157C4[];
extern u8           D_replay_bonus_801157C8[];
extern u8           D_replay_bonus_80119014[];
extern u8           D_replay_bonus_8011906C[];
extern UiObjectDesc D_replay_bonus_80119154;
extern UiObjectDesc D_replay_bonus_801191A8;
extern s32          D_replay_bonus_80119288;

extern UiObjectDesc D_800611E4;
extern UiObjectDesc D_replay_bonus_80119170;
extern UiObjectDesc D_replay_bonus_8011918C;
extern UiObjectDesc D_replay_bonus_801191C4;
extern UiObjectDesc D_replay_bonus_801191E0;
extern UiObjectDesc D_replay_bonus_801191FC;
extern s32          D_replay_bonus_80119284;

/// Where a credits picture is decoded to and how large it is drawn.
///
/// Every picture has this size. Two VRAM rows at this column hold the picture
/// on screen and the one being decoded, so that one can fade into the other.
enum {
    REPLAY_BONUS_PICTURE_VRAM_X = 640, // Left edge of both picture buffers in VRAM
    REPLAY_BONUS_PICTURE_WIDTH  = 240, // Picture width in pixels
    REPLAY_BONUS_PICTURE_HEIGHT = 176, // Picture height in pixels
};

static void func_replay_bonus_80117E04(void);
static void func_replay_bonus_801183B8(s32 y, ReplayBonusStfCommand* cmds);
static void func_replay_bonus_80118F00(s32 arg0);

static void        func_replay_bonus_80117194(Task* arg0);
static s32         func_replay_bonus_801173A8(void);
static s16         func_replay_bonus_80117484(s32 arg0, s32 arg1);
static s32         func_replay_bonus_80117598(s32 arg0);
static s16         func_replay_bonus_801175D0(UiList* list, UiObject* ctx, s32 index);
static s32         func_replay_bonus_801175F0(UiList* list, UiObject* ctx);
static inline void _replayBonusDrawItemRow(UiList* prompt, UiObject* obj, s32 id);
static s32         func_replay_bonus_801177A0(void);
static void        func_replay_bonus_80117848(Task* arg0);
static void        func_replay_bonus_801178C0(Task* arg0);
static void        func_replay_bonus_80117924(Task* arg0);
void               func_replay_bonus_8011797C(Task* arg0);
void               func_replay_bonus_80117A08(Task* arg0);
static void        func_replay_bonus_80117DE0(u8 arg0);
static s32         func_replay_bonus_80118B6C(ReplayBonusStfFile* file, s32 index);
void               func_replay_bonus_80118C64(Task* arg0);
void               func_replay_bonus_80118D7C(Task* arg0);
void               func_replay_bonus_80118E3C(Task* arg0);

static void func_replay_bonus_80117194(Task* arg0)
{
    UiObject* obj;
    Task*     owner;
    s32       copied;
    s16       flag;

    obj  = arg0->spawnArg2.pointer;
    flag = obj->result;
    if ((flag == USER_INTERFACE_RESULT_CANCEL) || (flag == USER_INTERFACE_RESULT_CONFIRM)) {
        owner  = obj->owner;
        copied = obj->resultValue;
        Ui_TeardownTree(obj, owner);
        switch (arg0->state) {
            case 2:
                arg0->spawnArg2.pointer = Ui_SpawnFromDesc(&D_replay_bonus_8011918C, 0, 1, 1, NULL);
                break;
            case 3:
                if (D_replay_bonus_80119284 < 0) {
                    arg0->spawnArg2.pointer = Ui_SpawnFromDesc(&D_replay_bonus_801191FC, 0, 1, 1, NULL);
                    arg0->state             = arg0->state + 2;
                } else {
                    arg0->spawnArg2.pointer = Ui_SpawnFromDesc(&D_replay_bonus_801191C4, 0, 1, 1, NULL);
                }
                break;
            case 4:
                arg0->spawnArg2.pointer = Ui_SpawnFromDesc(&D_replay_bonus_801191E0, 1, 1, 1, NULL);
                break;
            case 5:
                arg0->spawnArg2.pointer = Ui_SpawnFromDesc(&D_replay_bonus_801191C4, 2, 1, 1, NULL);
                break;
            case 6:
                GameMain_SetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
                func_replay_bonus_80116EC0();
                gDisplayState.gameMode  = DISPLAY_GAME_MODAL;
                arg0->spawnArg2.pointer = Ui_SpawnFromDesc(&D_800611E4, 0, 1, 1, NULL);
                break;
            case 7:
                if (copied == 0x33) {
                    arg0->spawnArg2.pointer = Gp_SpawnItemPrompt(NULL, 0x11, 0, 1);
                    arg0->state             = arg0->state + 1;
                } else {
                    arg0->spawnArg2.pointer = Ui_SpawnFromDesc(&D_replay_bonus_80119170, 0, 1, 2, NULL);
                }
                break;
            case 8:
                if (copied == 0x33) {
                    arg0->spawnArg2.pointer = Gp_SpawnItemPrompt(NULL, 0xF, 0, 1);
                } else {
                    arg0->spawnArg2.pointer = Ui_SpawnFromDesc(&D_800611E4, 1, 1, 1, NULL);
                    arg0->state             = arg0->state - 2;
                }
                break;
        }
        arg0->state = arg0->state + 1;
        return;
    }
    arg0->killCountdown = 0x10;
}

static s32 func_replay_bonus_801173A8(void)
{
    ShopTier*   p;
    u32         spend;
    s32         idx;
    s32         i;
    McSaveData* save;
    s32         mask;
    s32         one;

    spend = func_replay_bonus_80115CA4();
    p     = D_replay_bonus_80118F78;
    idx   = 0;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopTiers == 0x1FFF) {
        return -1;
    }
    i = 0;
    do {
    loop:
        if (!(p->expCeiling < spend)) {
            idx = i;
            break;
        }
        i++;
        p++;
        if (i < 0xD) {
            goto loop;
        }
    } while (0);

    save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    idx += save->state.gameMode;
    i    = 0;
    if (idx >= 0xD) {
        idx = 0xC;
    }
    one  = 1;
    mask = save->state.shopTiers;
    do {
        if ((mask & (one << idx)) == 0) {
            return idx;
        }
        idx += 1;
        if (idx >= 0xD) {
            idx -= 0xD;
        }
        i += 1;
    } while (i < 0xD);
    return idx;
}

static s16 func_replay_bonus_80117484(s32 arg0, s32 arg1)
{
    ShopTier*   p;
    u32         spend;
    s32         idx;
    s32         i;
    McSaveData* save;
    s32         mask;
    s32         one;
    s32         result;

    spend = func_replay_bonus_80115CA4();
    p     = D_replay_bonus_80118F78;
    idx   = 0;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.shopTiers == 0x1FFF) {
        result = -1;
    } else {
        i = 0;
        do {
        loop:
            if (!(p->expCeiling < spend)) {
                idx = i;
                break;
            }
            i++;
            p++;
            if (i < 0xD) {
                goto loop;
            }
        } while (0);

        save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        idx += save->state.gameMode;
        i    = 0;
        if (idx >= 0xD) {
            idx = 0xC;
        }
        one  = 1;
        mask = save->state.shopTiers;
        do {
        loop2:
            if ((mask & (one << idx)) == 0) {
                break;
            }
            idx += 1;
            if (idx >= 0xD) {
                idx -= 0xD;
            }
            i += 1;
            if (i < 0xD) {
                goto loop2;
            }
        } while (0);
        result = idx;
    }
    if (result < 0) {
        return 0;
    }
    return D_replay_bonus_80118F78[result].items[arg1];
}

static s32 func_replay_bonus_80117598(s32 arg0)
{
    u16* p;
    s32  i;

    p = D_replay_bonus_8011908C;
    i = 0;
    do {
        i++;
        if (*p != arg0) {
            p++;
        } else {
            return 1;
        }
    } while (i < 0x4E);
    return 0;
}

static s16 func_replay_bonus_801175D0(UiList* list, UiObject* ctx, s32 index)
{
    s16* p = ((s16*)ctx->owner->work) + index;

    return *p;
}

static s32 func_replay_bonus_801175F0(UiList* list, UiObject* ctx)
{
    s32           i;
    s32           sum;
    PlayerStatus* cfg;

    cfg = &gPlayerStatus;
    sum = 0;
    for (i = list->firstVisibleItemIndex.signedValue; i < list->itemCount; i++) {
        sum += replayBonusItemBp(((s16*)ctx->owner->work)[i]);
    }
    sum += cfg->bp;
    if (sum > 99999999) {
        sum = 99999999;
    }
    return sum;
}

/// Draws one row of the replay-bonus item list: marks the item as seen, then
/// draws its label at the prompt's position and its BP value at the mirrored x.
static inline void _replayBonusDrawItemRow(UiList* prompt, UiObject* obj, s32 id)
{
    u8 buf[0x20];

    Gp_SetItemSeenBit(id, 1);
    Gp_DrawItemLabel(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, id, 0x606060, 0);
    Text_DrawPrompt(obj, -prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, Text_ItoaSigned(buf, replayBonusItemBp(id)), 0x606060, TEXT_DRAW_TRANSLUCENT_OUTLINED,
                    TEXT_ALIGNMENT_RIGHT);
}

void func_replay_bonus_801176A8(UiList* prompt, UiObject* obj)
{
    s16* p;

    p = &((s16*)obj->owner->work)[prompt->currentItemIndex];
    _replayBonusDrawItemRow(prompt, obj, *p);
}

static s32 func_replay_bonus_801177A0(void)
{
    ShopTier* tier;
    s32       j;
    s32       sum;
    s32       i;

    sum  = 0;
    tier = D_replay_bonus_80118F78;
    i    = sum;
    do {
        j = 0;
        do {
            sum += Gp_ItemDescs[tier->items[j]].price;
            j++;
        } while (j < 3);
        i++;
        tier++;
    } while (i < 0xD);
    sum += 0x1869F;
    sum  = sum / 100000;
    return sum * 0x186A0;
}

static void func_replay_bonus_80117848(Task* arg0)
{
    u16 timer = arg0->killCountdown + 1;

    arg0->killCountdown = timer;
    if ((s16)timer >= 0x78) {
        gGameSession->uiOpen = 1;
        GameMain_SetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        CdCmd_EnqueueLoadFile(1, 0x3E, 3);
        arg0->state = arg0->state + 1;
    }
}

static void func_replay_bonus_801178C0(Task* arg0)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        Text_LoadClutImages();
        arg0->spawnArg2.pointer = Ui_SpawnFromDesc(&D_replay_bonus_80119154, 0, 1, 1, NULL);
        arg0->state             = (s32)(arg0->state + 1);
    }
}

static void func_replay_bonus_80117924(Task* arg0)
{
    u16 remaining = arg0->killCountdown - 1;

    arg0->killCountdown = remaining;
    if ((s16)remaining < 0) {
        gDisplayState.gameMode = DISPLAY_GAME_ACTIVE;
        gGameSession->uiOpen   = 0;
        Task_CallExit(arg0);
        gDisplayState.gameMode = DISPLAY_GAME_RESTART;
    }
}

void func_replay_bonus_8011797C(Task* arg0)
{
    TaskFunc states[11] = {
        func_replay_bonus_80117848,
        func_replay_bonus_801178C0,
        func_replay_bonus_80117194,
        func_replay_bonus_80117194,
        func_replay_bonus_80117194,
        func_replay_bonus_80117194,
        func_replay_bonus_80117194,
        func_replay_bonus_80117194,
        func_replay_bonus_80117194,
        func_replay_bonus_80117194,
        func_replay_bonus_80117924,
    };

    states[arg0->state](arg0);
}

void func_replay_bonus_80117A08(Task* arg0)
{
    enum {
        DISPLAY_SETUP_INTERLACED_640X480 = 0x1141,
    };

    RECT                  rect;
    s32                   temp_a1;
    s32                   temp_v1_2;
    s32                   temp_v1_3;
    u16*                  temp_v0;
    u16                   temp_v0_2;
    u16                   temp_v0_3;
    u32                   temp_v1;
    ReplayBonusStfParams* params;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            D_replay_bonus_80119226           = 0;
            D_replay_bonus_80119227           = 0;
            D_replay_bonus_801192AC           = 0;
            D_replay_bonus_801192BC           = memCalloc(sizeof(ReplayBonusPictureDecode), false);
            temp_v0                           = func_replay_bonus_80115C68();
            D_replay_bonus_80119228           = NULL;
            D_replay_bonus_80119225           = 0;
            D_replay_bonus_801192BC->vlcTable = temp_v0;
            func_replay_bonus_80118F00(0);
            GameMain_SetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
            SetDispMask(1);
            Display_SetMode(DISPLAY_SETUP_INTERLACED_640X480);
            rect.x = REPLAY_BONUS_PICTURE_VRAM_X;
            rect.w = REPLAY_BONUS_PICTURE_WIDTH;
            rect.y = 0;
            rect.h = REPLAY_BONUS_PICTURE_HEIGHT;
            ClearImage(&rect, 0, 0, 0);
            rect.y = 0x100;
            ClearImage(&rect, 0, 0, 0);
            D_replay_bonus_801192A4                 = -0x1E0;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            D_replay_bonus_801192B0                 = 0;
            arg0->killCountdown                     = D_replay_bonus_80119294->startHold * REPLAY_BONUS_STF_HOLD_UNIT_FRAMES;
            CdCmd_StartOverlay(0U, 1U, 0xBU);
            CdCmd_EnqueueOverlay82();
            goto advance;
        case 1:
            if (CdCmd_IsIdle() & 0xFFFF) {
                CdCmd_EnqueueOverlay81();
                goto advance;
            }
            return;
        case 2:
            D_replay_bonus_801192B0 += 1;
            func_replay_bonus_80117E04();
            temp_v0_2           = arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0_2;
            if ((temp_v0_2 << 0x10) <= 0) {
                Task_SpawnFromTable(&D_replay_bonus_8011922C, 1, 0xB4, 0);
                arg0->state = 0xA;
            }
            if (Pad_CheckFlag800() != 0) {
                Task_SpawnFromTable(&D_replay_bonus_8011922C, 2, 0x1E, 0);
                CdCmd_CancelReplaceAndActivate();
                arg0->state         = 0xB;
                arg0->killCountdown = 0x1E;
                return;
            }
            break;
        case 10:
            D_replay_bonus_801192B0 += 1;
            func_replay_bonus_80117E04();
            params                  = D_replay_bonus_80119294;
            temp_v1_2               = D_replay_bonus_801192A8 + params->scrollSpeed;
            temp_a1                 = D_replay_bonus_801192A4 + (temp_v1_2 >> 8);
            D_replay_bonus_801192A8 = temp_v1_2;
            D_replay_bonus_801192A4 = temp_a1;
            D_replay_bonus_801192A8 = temp_v1_2 & 0xFF;
            temp_v1_3               = D_replay_bonus_80119298[D_replay_bonus_801192A0 - 1].y - 0x1E0;
            if (temp_v1_3 < temp_a1) {
                D_replay_bonus_801192A4 = temp_v1_3;
                arg0->state            += 1;
                arg0->killCountdown     = params->endHold * REPLAY_BONUS_STF_HOLD_UNIT_FRAMES;
            }
            if (Pad_CheckFlag800() != 0) {
                CdCmd_CancelReplaceAndActivate();
                Task_SpawnFromTable(&D_replay_bonus_8011922C, 2, 0x1E, 0);
                arg0->killCountdown = 0x1E;
                arg0->state        += 1;
                return;
            }
            break;
        case 11:
            D_replay_bonus_801192B0 += 1;
            func_replay_bonus_80117E04();
            temp_v0_3           = arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0_3;
            if ((s16)temp_v0_3 == 0xB4) {
                Task_SpawnFromTable(&D_replay_bonus_8011922C, 2, 0xB4, 0);
            }
            if ((s16)arg0->killCountdown <= 0) {
                arg0->state = 0x14;
                return;
            }
            break;
        case 20:
            if (D_replay_bonus_80119225 != 1) {
                SetDispMask(0);
                goto advance;
            }
            break;
        case 21:
            Gp_RestoreStreamRng();
            memFree(D_replay_bonus_801192BC);
            Display_SetMode(DISPLAY_SETUP_DEFAULT);
            goto advance;
        case 24:
            func_800B2968();
        case 22:
        case 23:
        advance:
            arg0->state += 1;
            return;
        case 25:
            SetDispMask(1);
            taskKill(arg0);
            break;
    }
}

static void func_replay_bonus_80117DE0(u8 arg0)
{
    char pad[0x10];

    D_replay_bonus_801192AC = 0x7F - (arg0 >> 1);
}

static void func_replay_bonus_80117E04(void)
{
    s32                          start;
    s32                          i;
    s32                          end;
    s32                          count;
    s32                          n;
    s32                          scrollY;
    s32                          visEnd;
    s32                          y;
    s32                          rgb;
    s32                          code;
    u8                           wait;
    volatile ReplayBonusStfLine* line;
    LINE_F2*                     lf2;
    SPRT*                        sprt;
    DR_TPAGE*                    tpage;

    start                   = 0;
    i                       = start;
    count                   = D_replay_bonus_801192A0;
    D_replay_bonus_801192B4 = 0;
    D_replay_bonus_801192C0 =
        Gpu_PrimHeapBase + Gpu_PrimHeapSize + (D_replay_bonus_80119224 << 16);
    D_replay_bonus_80119224 ^= 1;
    end                      = count - 1;

    if (count > 0) {
        n       = count;
        line    = D_replay_bonus_80119298;
        scrollY = D_replay_bonus_801192A4;
        visEnd  = scrollY + 0x1E0;
        do {
            if (line->y < scrollY) {
                start = 0;
                if (i != 0) {
                    start = i - 1;
                }
            }
            if (visEnd < line->y) {
                end = i;
                break;
            }
            i++;
            line++;
        } while (i < n);
        i = start;
    }

    for (; i < end + 1; i++) {
        lf2                      = (LINE_F2*)(D_replay_bonus_801192C0 + D_replay_bonus_801192B4);
        D_replay_bonus_801192B4 += 0x10;
        setLineF2(lf2);
        y       = D_replay_bonus_80119298[i].y - (D_replay_bonus_801192A4 & 0xFFFFFE) - 0xF0;
        lf2->x0 = -0x140;
        lf2->x1 = 0x140;
        lf2->r0 = 0x40;
        lf2->g0 = 0x80;
        lf2->b0 = 0x40;
        lf2->y0 = y;
        lf2->y1 = y;
        func_replay_bonus_801183B8(y, D_replay_bonus_80119298[i].cmds.pointer);
    }

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 0, 1, 0);
    addPrim(gGpuCurrentOt + 10, tpage);

    if (D_replay_bonus_80119225 != 2) {
        sprt           = gGpuPrimCursor;
        gGpuPrimCursor = sprt + 1;
        setSprt(sprt);
        sprt->x0 = D_replay_bonus_801192B8 - 0x140;
        sprt->y0 = -0x8C;
        sprt->w  = REPLAY_BONUS_PICTURE_WIDTH;
        sprt->h  = REPLAY_BONUS_PICTURE_HEIGHT;
        sprt->r0 = D_replay_bonus_801192AC;
        sprt->g0 = D_replay_bonus_801192AC;
        sprt->b0 = D_replay_bonus_801192AC;
        sprt->u0 = 0;
        sprt->v0 = 0;
        addPrim(gGpuCurrentOt + 11, sprt);

        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setDrawTPage(tpage, 0, 1, getTPage(2, 0, REPLAY_BONUS_PICTURE_VRAM_X, D_replay_bonus_80119226 << 8));
        addPrim(gGpuCurrentOt + 11, tpage);
        return;
    }

    code           = 0x64;
    sprt           = gGpuPrimCursor;
    gGpuPrimCursor = sprt + 1;
    setlen(sprt, 4);
    setcode(sprt, code);
    rgb      = (D_replay_bonus_801192AC * (0x78 - D_replay_bonus_80119227)) / 120;
    sprt->x0 = D_replay_bonus_801192B8 - 0x140;
    sprt->y0 = -0x8C;
    sprt->w  = REPLAY_BONUS_PICTURE_WIDTH;
    sprt->h  = REPLAY_BONUS_PICTURE_HEIGHT;
    sprt->u0 = 0;
    sprt->v0 = 0;
    setSemiTrans(sprt, 1);
    sprt->r0 = rgb;
    sprt->g0 = rgb;
    sprt->b0 = rgb;
    addPrim(gGpuCurrentOt + 11, sprt);

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 0, 1, getTPage(2, 1, REPLAY_BONUS_PICTURE_VRAM_X, D_replay_bonus_80119226 << 8));
    addPrim(gGpuCurrentOt + 11, tpage);

    sprt           = gGpuPrimCursor;
    gGpuPrimCursor = sprt + 1;
    setlen(sprt, 4);
    setcode(sprt, code);
    rgb      = (D_replay_bonus_801192AC * D_replay_bonus_80119227) / 120;
    sprt->x0 = D_replay_bonus_801192B8 - 0x140;
    sprt->y0 = -0x8C;
    sprt->w  = REPLAY_BONUS_PICTURE_WIDTH;
    sprt->h  = REPLAY_BONUS_PICTURE_HEIGHT;
    sprt->u0 = 0;
    sprt->v0 = 0;
    sprt->r0 = rgb;
    sprt->g0 = rgb;
    sprt->b0 = rgb;
    addPrim(gGpuCurrentOt + 11, sprt);

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 0, 1, getTPage(2, 0, REPLAY_BONUS_PICTURE_VRAM_X, (D_replay_bonus_80119226 ^ 1) << 8));
    addPrim(gGpuCurrentOt + 11, tpage);

    wait                    = D_replay_bonus_80119227 - 1;
    D_replay_bonus_80119227 = wait;
    if (!(wait & 0xFF)) {
        D_replay_bonus_80119225 = 0;
    }
}

static void func_replay_bonus_801183B8(s32 y, ReplayBonusStfCommand* cmds)
{
    s32                  tpageId;
    s32                  col;
    s32                  align;
    s16                  i;
    s16                  j;
    s32                  width;
    s16                  x;
    s16                  x1;
    s16                  y0;
    s32                  glyphFlag;
    s16                  shift;
    s16                  piece;
    s32                  gu;
    s16                  gv;
    u8                   idx;
    s32                  tpageX;
    s32                  clut;
    s32                  clutY;
    s32                  gh;
    s32                  pixelMode;
    u16                  page;
    u8                   pageFlags;
    ReplayBonusStfGlyph* glyph;
    POLY_FT4*            p;
    SPRT*                sprt;
    DR_TPAGE*            dr;
    u_long*              glyphOt;
    u_long               glyphTag;
    u_long               drawTag;

    tpageId = 7;
    col     = 0;
    align   = col;
    i       = col;
    if (cmds->op != REPLAY_BONUS_STF_COMMAND_END) {
        do {
            switch (cmds[i].op) {
                case REPLAY_BONUS_STF_COMMAND_GLYPH:
                    width = 0;
                    j     = 0;
                    while (cmds[i + j].op == REPLAY_BONUS_STF_COMMAND_GLYPH) {
                        width += D_replay_bonus_80119290[cmds[i + j].arg].width;
                        j++;
                    }
                    x = 0;
                    switch ((u8)align) {
                        case 0:
                            x = D_replay_bonus_80119294->columns[col].centerX - ((s16)width / 2);
                            break;
                        case 1:
                            x = D_replay_bonus_80119294->columns[col].leftX;
                            break;
                        case 2:
                            x = D_replay_bonus_80119294->columns[col].rightX - width;
                            break;
                    }
                    x -= 0x140;
                    j  = 0;
                    while (cmds[i + j].op == REPLAY_BONUS_STF_COMMAND_GLYPH) {
                        glyph                    = &D_replay_bonus_80119290[cmds[i + j].arg];
                        width                    = glyph->width;
                        gh                       = glyph->heightAndPage;
                        gu                       = glyph->u;
                        gv                       = glyph->v;
                        p                        = (POLY_FT4*)(D_replay_bonus_801192C0 + D_replay_bonus_801192B4);
                        D_replay_bonus_801192B4 += 0x28;
                        setPolyFT4(p);
                        y0        = gh & REPLAY_BONUS_STF_GLYPH_HEIGHT_MASK;
                        p->u0     = gu;
                        p->v0     = gv;
                        p->u1     = width + gu;
                        p->v1     = gv;
                        p->u2     = gu;
                        p->v2     = gv + y0;
                        p->u3     = width + gu;
                        p->v3     = gv + y0;
                        p->r0     = D_replay_bonus_801192AC;
                        p->x0     = x;
                        p->x2     = x;
                        p->y2     = y;
                        p->y3     = y;
                        p->g0     = D_replay_bonus_801192AC;
                        p->b0     = D_replay_bonus_801192AC;
                        y0        = y - y0;
                        x1        = x + width;
                        p->y0     = y0;
                        p->x1     = x1;
                        p->y1     = y0;
                        p->x3     = x1;
                        glyphFlag = (u32)gh >> REPLAY_BONUS_STF_GLYPH_PAGE_SHIFT;
                        p->clut   = (((tpageId / 4) + 0x1FE) << 6) | ((tpageId & 3) | 0x38);
                        p->tpage  = 0x1E;
                        if (glyphFlag) {
                            p->tpage = 0xF;
                        }
                        x = x1;
                        setaddr(p, getaddr(gGpuCurrentOt + 10));
                        glyphOt  = gGpuCurrentOt + 10;
                        glyphTag = (*glyphOt & 0xFF000000) | ((u_long)p & 0xFFFFFF);
                        SOFT_USE_REG(gv);
                        *glyphOt = glyphTag;
                        j++;
                    }
                    i += j - 1;
                default:
                case 9:
                    break;
                case REPLAY_BONUS_STF_COMMAND_PALETTE:
                    tpageId = cmds[i].arg;
                    break;
                case REPLAY_BONUS_STF_COMMAND_COLUMN_CENTER:
                    align = 0;
                    col   = cmds[i].arg;
                    break;
                case REPLAY_BONUS_STF_COMMAND_COLUMN_LEFT:
                    align = 1;
                    col   = cmds[i].arg;
                    break;
                case REPLAY_BONUS_STF_COMMAND_COLUMN_RIGHT:
                    align = 2;
                    col   = cmds[i].arg;
                    break;
                case REPLAY_BONUS_STF_COMMAND_PICTURE:
                    idx = cmds[i].arg;
                    x   = 0;
                    if (idx != REPLAY_BONUS_STF_PICTURE_STARTED) {
                        cmds[i].arg = REPLAY_BONUS_STF_PICTURE_STARTED;
                        switch ((u8)align) {
                            case 0:
                                x = D_replay_bonus_80119294->columns[col].centerX - (REPLAY_BONUS_PICTURE_WIDTH / 2);
                                break;
                            case 1:
                                x = D_replay_bonus_80119294->columns[col].leftX;
                                break;
                            case 2:
                                x = D_replay_bonus_80119294->columns[col].rightX - REPLAY_BONUS_PICTURE_WIDTH;
                                break;
                        }
                        D_replay_bonus_801192B8 = x;
                        Task_SpawnFromTable(&D_replay_bonus_8011922C, 3, idx + 1, 0);
                    }
                    break;
                case REPLAY_BONUS_STF_COMMAND_SPRITE:
                    idx   = cmds[i].arg;
                    width = D_replay_bonus_8011929C[idx].width;
                    x     = 0;
                    switch ((u8)align) {
                        case 0:
                            x = D_replay_bonus_80119294->columns[col].centerX - ((s16)width / 2);
                            break;
                        case 1:
                            x = D_replay_bonus_80119294->columns[col].leftX;
                            break;
                        case 2:
                            x = D_replay_bonus_80119294->columns[col].rightX - width;
                            break;
                    }
                    shift     = 2;
                    tpageX    = D_replay_bonus_8011929C[idx].tpageX;
                    gu        = D_replay_bonus_8011929C[idx].u;
                    gv        = D_replay_bonus_8011929C[idx].v;
                    gh        = D_replay_bonus_8011929C[idx].height;
                    clutY     = D_replay_bonus_8011929C[idx].clutY << 6;
                    clut      = clutY | ((D_replay_bonus_8011929C[idx].clutX >> 4) & 0x3F);
                    pixelMode = D_replay_bonus_8011929C[idx].pixelMode;
                    switch (pixelMode) {
                        case 1:
                            shift = 1;
                            break;
                        case 2:
                            shift = 0;
                            break;
                    }
                    while ((s16)width > 0) {
                        piece = width;
                        if (gu + (s16)width >= 0x101) {
                            piece = 0x100 - gu;
                            width = width - piece;
                            gu    = 0;
                        } else {
                            width = 0;
                        }
                        sprt           = gGpuPrimCursor;
                        gGpuPrimCursor = sprt + 1;
                        setlen(sprt, 4);
                        setcode(sprt, 0x64);
                        sprt->r0   = D_replay_bonus_801192AC;
                        sprt->x0   = x - 0x140;
                        sprt->g0   = D_replay_bonus_801192AC;
                        sprt->b0   = D_replay_bonus_801192AC;
                        sprt->y0   = y - gh;
                        sprt->w    = piece;
                        sprt->h    = gh;
                        sprt->clut = clut;
                        sprt->u0   = gu;
                        sprt->v0   = gv;
                        setaddr(sprt, getaddr(gGpuCurrentOt + 10));
                        dr             = gGpuPrimCursor;
                        gGpuPrimCursor = dr + 1;
                        setaddr(gGpuCurrentOt + 10, sprt);
                        setlen(dr, 1);
                        pageFlags   = D_replay_bonus_8011929C[idx].pixelMode;
                        page        = (u32)(tpageX & 0x3FF) >> 6;
                        dr->code[0] = ((pageFlags & 3) << 7) | (s16)((s32)((D_replay_bonus_8011929C[idx].tpageY & 0x100) << 16) >> 20) | page | ((s16)(D_replay_bonus_8011929C[idx].tpageY & 0x200) * 4) | 0xE1000200;
                        tpageX     += 0x100 >> shift;
                        x           = x + piece;
                        drawTag     = (dr->tag & 0xFF000000) | (getaddr(gGpuCurrentOt + 10) & 0xFFFFFF);
                        SOFT_USE_REG2(piece, gh);
                        dr->tag = drawTag;
                        setaddr(gGpuCurrentOt + 10, dr);
                    }
                    break;
            }
            i++;
        } while (cmds[i].op != REPLAY_BONUS_STF_COMMAND_END);
    }
}

static s32 func_replay_bonus_80118B6C(ReplayBonusStfFile* file, s32 index)
{
    ReplayBonusStfLine* rec;
    s32                 i;

    if (strncmp(file->magic, "STF", 3) != 0) {
        return 0;
    }

    if (file->glyphs.offset > 0) {
        file->glyphs.offset    += (s32)file;
        file->params.offset    += (s32)file;
        file->lineTable.offset += (s32)file;
        file->sprites.offset   += (s32)file;
        D_replay_bonus_80119298 = (file->lineTable.pointer)->lines;
        D_replay_bonus_801192A0 = (file->lineTable.pointer)->count;
        for (i = 0; i < D_replay_bonus_801192A0; i++) {
            rec                     = D_replay_bonus_80119298;
            D_replay_bonus_80119298 = rec + 1;
            rec->cmds.offset       += (s32)file;
        }
    }

    D_replay_bonus_80119290 = file->glyphs.pointer;
    D_replay_bonus_80119294 = file->params.pointer;
    D_replay_bonus_80119298 = (file->lineTable.pointer)->lines;
    D_replay_bonus_8011929C = file->sprites.pointer;
    return 1;
}

void func_replay_bonus_80118C64(Task* arg0)
{
    s32                       poll;
    s32                       temp_v1;
    ReplayBonusPictureDecode* picture;
    Task*                     t;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            if ((u32)(D_replay_bonus_80119225 - 1) < 2U) {
                taskKill(arg0);
                break;
            }
            picture                 = D_replay_bonus_801192BC;
            picture->resourceIndex  = arg0->spawnArg1.value;
            picture->vramX          = REPLAY_BONUS_PICTURE_VRAM_X;
            picture->vramY          = (D_replay_bonus_80119226 ^ 1) << 8;
            picture->width          = REPLAY_BONUS_PICTURE_WIDTH;
            picture->height         = REPLAY_BONUS_PICTURE_HEIGHT;
            D_replay_bonus_80119228 = Task_SpawnFromTable(&D_replay_bonus_80118F6C, 0, 0, picture);
            D_replay_bonus_80119225 = 1;
            arg0->state            += 1;
            break;
        case 1:
            if (Task_PollKill(D_replay_bonus_80119228, &poll) != 0) {
                t                        = arg0;
                D_replay_bonus_80119227  = 0x78;
                D_replay_bonus_80119226 ^= 1;
                D_replay_bonus_80119225  = 2;
                taskKill(t);
            }
            break;
    }
}

void func_replay_bonus_80118D7C(Task* arg0)
{
    s32 temp_v1;
    u16 temp_v0;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            arg0->killCountdown = (u16)arg0->spawnArg1.value;
            arg0->state        += 1;
            break;
        case 1:
            temp_v0             = arg0->killCountdown - 1;
            arg0->killCountdown = temp_v0;
            if ((temp_v0 << 0x10) <= 0) {
                taskKill(arg0);
            }
            break;
    }
    func_replay_bonus_80117DE0(((s32)(arg0->killCountdown * 0xFF) / (s32)arg0->spawnArg1.value) & 0xFF);
}

void func_replay_bonus_80118E3C(Task* arg0)
{
    s32 temp_v1;
    u16 temp_v0;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            arg0->killCountdown = 0;
            arg0->state        += 1;
            break;
        case 1:
            temp_v0             = arg0->killCountdown + 1;
            arg0->killCountdown = temp_v0;
            if ((s16)temp_v0 >= arg0->spawnArg1.value) {
                taskKill(arg0);
            }
            break;
    }
    func_replay_bonus_80117DE0(((s32)(arg0->killCountdown * 0xFF) / (s32)arg0->spawnArg1.value) & 0xFF);
}

static void func_replay_bonus_80118F00(s32 arg0)
{
    FsResourceSlot*     slot;
    s32                 count;
    s32                 i;
    s32                 resourceKind;
    ReplayBonusStfFile* stfFile;

    count        = 0;
    i            = count;
    resourceKind = FILE_SYSTEM_RESOURCE_DATA;
    do {
        slot = &D_8006C338[i];
        if (slot->kind == resourceKind) {
            if (count == arg0) {
                stfFile                 = slot->data;
                D_replay_bonus_8011928C = stfFile;
                func_replay_bonus_80118B6C(stfFile, i);
                return;
            }
            count++;
        }
        i++;
    } while (i < ARRAY_SIZE(D_8006C338));
}
