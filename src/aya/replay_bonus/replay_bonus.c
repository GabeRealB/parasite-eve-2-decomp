#include "common.h"

#include "aya/replay_bonus.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/4CC.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/text.h"
#include "main/wipsys.h"
#include "psyq/libpress.h"
extern u8           D_replay_bonus_801157A8[];
extern u8           D_replay_bonus_801157B0[];
extern u8           D_replay_bonus_801157C4[];
extern u8           D_replay_bonus_801157C8[];
extern u8           D_replay_bonus_80119014[];
extern u8           D_replay_bonus_8011906C[];
extern UiObjectDesc D_replay_bonus_80119154;
extern UiObjectDesc D_replay_bonus_801191A8;
extern s32          D_replay_bonus_80119288;
extern s32          D_replay_bonus_8011928C;
extern u8           D_replay_bonus_801192AC;
extern GpItemDesc   D_8010DE38[];
extern s32          D_80072A9C;

s32 func_replay_bonus_80118B6C(s32 arg0, s32 index);

void func_replay_bonus_801158C0(void)
{
    RECT rect;
    s32  height;
    s32  next;
    s32  row;
    s32  xoff;

    rect.w = 0x10;
    row    = D_replay_bonus_8011926E;
    xoff   = row * 0x10;
    SOFT_TOUCH_REG(row);
    rect.x = D_replay_bonus_80119268 + xoff;
    rect.h = D_replay_bonus_80119266;
    rect.y = D_replay_bonus_8011926A;
    LoadImage(&rect, (u_long*)(D_replay_bonus_8011925C + ((D_replay_bonus_80119270 << 5) * (s16)D_replay_bonus_80119266)));
    height                  = D_replay_bonus_80119264;
    D_replay_bonus_8011926C = 0;
    D_replay_bonus_80119270 = D_replay_bonus_80119270 ^ 1;
    next                    = (D_replay_bonus_8011926E = (u16)D_replay_bonus_8011926E + 1);
    next                    = (s16)next;
    if (height < 0) {
        height += 0xF;
    }
    if (next == (height >> 4) - 1) {
        DecDCToutCallback(NULL);
    }
}

void func_replay_bonus_801159A0(Task* arg0)
{
    ReplayBonusStream* stream;
    s32                state;
    s32                width;
    s32                bufSize;
    s32                imgWidth;
    s32                strip;
    s32                next;
    u16                w;
    u16                x;
    u16                h;
    u16                y;
    void*              vlcBuf;
    u_long*            bs;

    state  = arg0->state;
    stream = arg0->spawnArg2;
    switch (state) {
        case 0:
            D_replay_bonus_80119270 = 0;
            w                       = stream->w;
            x                       = stream->x;
            D_replay_bonus_80119264 = w;
            h                       = stream->h;
            D_replay_bonus_80119266 = h;
            D_replay_bonus_80119268 = x;
            y                       = stream->y;
            D_replay_bonus_8011926A = y;
            DecDCTReset(0);
            D_replay_bonus_8011925C = Mem_Malloc((s16)D_replay_bonus_80119266 << 6, 1);
            vlcBuf                  = Mem_Malloc(D_replay_bonus_80119264 * (s16)D_replay_bonus_80119266 * 2, 1);
            bs                      = (u_long*)D_8006C338[stream->fileId].field_4;
            D_replay_bonus_80119260 = vlcBuf;
            bufSize                 = DecDCTBufSize(bs);
            width                   = D_replay_bonus_80119264;
            if (width < 0) {
                width += 0xF;
            }
            DecDCTvlcSize2((bufSize / (width >> 4)) + 2);
            if ((DecDCTvlc2((u_long*)D_8006C338[stream->fileId].field_4, D_replay_bonus_80119260, stream->table) << 0x10) == 0) {
                arg0->state = 2;
                return;
            }
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (DecDCTvlc2(NULL, NULL, stream->table) != 0) {
                return;
            }
            arg0->state = arg0->state + 1;
        case 2:
            DecDCToutCallback(func_replay_bonus_801158C0);
            DecDCTin(D_replay_bonus_80119260, 2);
            DecDCTout((u_long*)D_replay_bonus_8011925C, (s16)D_replay_bonus_80119266 * 8);
            D_replay_bonus_8011926E = 0;
            next                    = arg0->state;
            D_replay_bonus_8011926C = 1;
            arg0->state             = next + 1;
            return;
        case 3:
            if (D_replay_bonus_8011926C == 0) {
                imgWidth = D_replay_bonus_80119264;
                strip    = D_replay_bonus_8011926E;
                if (imgWidth < 0) {
                    imgWidth += 0xF;
                }
                if (strip == (imgWidth >> 4) - 1) {
                    Mem_Free2(D_replay_bonus_8011925C, 1);
                    Mem_Free2(D_replay_bonus_80119260, 1);
                    Task_RequestKill(arg0, 0);
                    return;
                }
                DecDCTout((u_long*)(D_replay_bonus_8011925C + ((D_replay_bonus_80119270 << 5) * (s16)D_replay_bonus_80119266)), (s16)D_replay_bonus_80119266 * 8);
                D_replay_bonus_8011926C = 1;
            }
            break;
    }
}

u16* func_replay_bonus_80115C68(void)
{
    u16* table = Mem_Malloc(0x11000, 1);

    DecDCTvlcBuild(table);
    return table;
}

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80115CA4);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80115D60);

INCLUDE_RODATA("aya/nonmatchings/replay_bonus/replay_bonus", D_replay_bonus_80115770);

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80115ED0);

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl D_replay_bonus_801157A8\n"
        "D_replay_bonus_801157A8:\n"
        "\t.asciz \"Balance\"\n"
        "\t.align 2\n"
        "\t.globl D_replay_bonus_801157B0\n"
        "D_replay_bonus_801157B0:\n"
        "\t.asciz \"NEXT REPLAY BONUS\"\n"
        "\t.align 2\n"
        "\t.globl D_replay_bonus_801157C4\n"
        "D_replay_bonus_801157C4:\n"
        "\t.asciz \"EXP\"\n"
        "\t.align 2\n"
        ".section .text\n");
#endif

void func_replay_bonus_801166AC(Task* arg0)
{
    u8            buf[0x20];
    TextDrawReq   req;
    TextDrawReq   req2;
    UiObject*     obj;
    UiObject*     childObj;
    Task*         child;
    WipSysConfig* cfg;
    s32           xOff;
    s32           negX;
    s32           color;
    s32           value;
    s32           remaining;
    s32           ot;
    s32           ot2;
    s32           flag;

    cfg = &Wip_SysConfig;
    obj = arg0->spawnArg2;
    if (arg0->state == 0) {
        arg0->killCountdown = 0xBC;
        arg0->state         = arg0->state + 1;
    }
    obj->field_2E = 0;
    if (arg0->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_replay_bonus_801157A8);
    } else {
        Ui_DrawText((UiPanel*)obj, D_replay_bonus_801157B0);
    }
    color          = 0x606060;
    xOff           = obj->field_1C + 2;
    req.x          = obj->baseX + xOff;
    req.y          = obj->baseY - 8;
    ot             = (s16)obj->drawOrder;
    req.field_8    = color;
    req.glyphTable = 5;
    req.centerMode = 0;
    req.field_E    = 1;
    req.otIndex    = ot + 1;
    func_8002E53C(&req, D_replay_bonus_801157C4);
    value = cfg->field_8;
    if (arg0->spawnArg1 == 1) {
        value = D_replay_bonus_8011927C;
    }
    negX = -xOff;
    Text_DrawPrompt(obj, negX, -2, Text_ItoaSigned(buf, value), color, 3, 2);
    req2.x          = obj->baseX + xOff;
    req2.y          = obj->baseY + 0xB;
    ot2             = (s16)obj->drawOrder;
    req2.field_8    = color;
    req2.glyphTable = 5;
    req2.centerMode = 0;
    req2.field_E    = 1;
    req2.otIndex    = ot2 + 1;
    func_8002E53C(&req2, D_replay_bonus_801157C8);
    value = D_replay_bonus_80119274.field_4;
    if (arg0->spawnArg1 == 1) {
        value = D_replay_bonus_80119274.field_C;
    }
    Text_DrawPrompt(obj, negX, 0x11, Text_ItoaSigned(buf, value), color, 3, 2);
    if (arg0->state == 1) {
        remaining           = (u16)arg0->killCountdown - 1;
        arg0->killCountdown = remaining;
        if ((remaining << 0x10) <= 0) {
            if (arg0->spawnArg1 == 0) {
                Ui_SpawnFromDesc(&D_replay_bonus_801191A8, 1, 1, 1, obj);
                obj->status = 0;
                arg0->state = arg0->state + 1;
            } else {
                obj->field_2E = 6;
            }
        }
    }
    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu) != 0) {
            obj->field_2E = 6;
        }
    }
    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
        flag     = childObj->field_2E;
        if (flag == 6) {
            obj->field_2E = flag;
        }
    }
}

INCLUDE_RODATA("aya/nonmatchings/replay_bonus/replay_bonus", D_replay_bonus_801157C8);

void func_replay_bonus_80116964(Task* arg0)
{
    UiObject* obj;
    UiObject* spawned;
    Task*     child;
    UiObject* childObj;
    s16       flag;
    u16       copied;
    s32       color;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawTitle((UiPanel*)obj, "WARNING");
    if (arg0->state == 0) {
        obj->field_2C = 0x34;
        Ui_SizeFromText((UiPanel*)obj, D_replay_bonus_8011906C, 0, 0);
        SOFT_BARRIER();
        Ui_UpdateLayoutSize((UiPanel*)obj, 0, Ui_Scale15(3) + 4);
        arg0->state = arg0->state + 1;
    } else if (arg0->state == 1) {
        spawned = func_800CD89C(obj);
        if (spawned != NULL) {
            spawned->owner->spawnArg1 |= 0x10;
            spawned->field_E          += 0x10;
            arg0->state                = arg0->state + 1;
        }
    }
    color = 0x606060;
    Text_DrawMultiLine(obj, obj->field_1C + 2, (s16)obj->field_18 + 0xF, D_replay_bonus_80119014, color, 1, 0);
    child = arg0->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
        flag     = childObj->field_2E;
        if ((flag == -1) || (flag == 6)) {
            copied        = childObj->field_2C;
            obj->field_2E = 6;
            obj->field_2C = copied;
        }
    }
}

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80116AC0);

void func_replay_bonus_80116D68(Task* arg0)
{
    u8          buf[0x20];
    TextDrawReq req;
    UiObject*   obj;
    s32         xOff;
    s32         bonus;
    s32         color;
    s32         remaining;
    s32         ot;

    obj   = arg0->spawnArg2;
    bonus = D_replay_bonus_80119288;
    Ui_DrawText((UiPanel*)obj, "EXTRA BONUS\0\0\0\0");
    if (arg0->state == 0) {
        arg0->killCountdown = 0xBC;
        arg0->state         = arg0->state + 1;
    }
    color          = 0x606060;
    xOff           = obj->field_1C + 2;
    req.x          = obj->baseX + xOff;
    req.y          = obj->baseY;
    ot             = (s16)obj->drawOrder;
    req.glyphTable = 5;
    req.field_8    = color;
    req.centerMode = 0;
    req.field_E    = 1;
    req.otIndex    = ot + 1;
    func_8002E53C(&req, D_replay_bonus_801157C8);
    Text_DrawPrompt(obj, -xOff, 6, Text_ItoaSigned(buf, bonus), color, 3, 2);
    remaining           = (u16)arg0->killCountdown - 1;
    arg0->killCountdown = remaining;
    if (obj->status == 1) {
        if (((remaining << 0x10) <= 0) || (Pad_CheckButtons(0, 1, Pad_MaskCancel | Pad_MaskMenu) != 0)) {
            obj->field_2E = 6;
        }
    }
}

INCLUDE_ASM("aya/nonmatchings/replay_bonus/replay_bonus", func_replay_bonus_80116EC0);
