#include "common.h"

#include "aya/replay_bonus.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
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
extern u8           D_80071072;
extern McItemRec    D_80072314[];
extern s32          D_replay_bonus_80119284;

void func_800C5F70(Task* arg0);

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

s32 func_replay_bonus_80115CA4(void)
{
    u8* levels;
    s32 spend;
    s32 i;
    s32 idx;
    s32 j;
    s32 base;
    s32 val;
    s32 flag;

    levels = Mc_SaveData.unknown_850;
    spend  = Wip_SysConfig.field_8;
    SOFT_USE_REG(spend);
    i = 0;
    do {
        idx = i * 3;
        if (*levels != 0) {
            for (j = 0; j < *levels; ++j) {
                base = idx;
                SOFT_TOUCH_REG(base);
                val  = Gp_IdParamHi[base + j + 1].field[0];
                flag = Mc_SaveData.field_F;
                if (flag > 0) {
                    val = (val * 4) / 5;
                } else if (Mc_SaveData.field_E > 0) {
                    val = (val * 2) / 5;
                }
                spend += val;
            }
        }
        i      += 1;
        levels += 1;
    } while (i < 0xC);
    return spend;
}

void func_replay_bonus_80115D60(UiList* list, ReplayBonusCtx* ctx)
{
    McItemRec* rec;
    s16*       ids;
    s16*       dest;
    s32        count;
    s32        i;
    s32        found;
    s32        j;
    u16*       p;
    u8         item;
    u8         id;
    s32        vis;
    s8         tmp;

    rec   = D_80072314;
    count = 0;
    ids   = ctx->itemList->itemIds;
    dest  = ids;
    i     = count;
    do {
        item = rec->field_0;
        if (item != 0) {
            item += 0x60;
            if ((u8)item >= 0x20U) {
                found = 1;
                id    = rec->field_0;
                p     = D_replay_bonus_8011908C;
                j     = 0;
            loop_4:
                if (*p != id) {
                    j += 1;
                    p += 1;
                    if (j >= 0x4E) {
                        found = 0;
                    } else {
                        goto loop_4;
                    }
                }
                if (found != 0) {
                    count += 1;
                    *dest  = rec->field_0;
                    dest  += 1;
                }
            }
        }
        i   += 1;
        rec += 1;
    } while (i < 0x100);

    i = 0x101;
    do {
        if (Gp_HasCollectedBit(i) != 0) {
            found = 1;
            p     = D_replay_bonus_8011908C;
            j     = 0;
        loop_13:
            if (*p != i) {
                j += 1;
                p += 1;
                if (j >= 0x4E) {
                    found = 0;
                } else {
                    goto loop_13;
                }
            }
            if (found != 0) {
                ids[count] = i;
                count     += 1;
            }
        }
        i += 1;
    } while (i < 0x200);

    vis           = 9;
    list->field_5 = vis;
    TOUCH_REG(vis);
    tmp           = count - vis;
    list->field_9 = tmp;
    list->field_4 = count;
    if (tmp < 0) {
        list->field_9 = 0;
    }
    list->field_10 = (s8)list->field_9;
}

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

void func_replay_bonus_80116AC0(Task* arg0)
{
    ReplayBonusShopTier* p;
    u32                  spend;
    s32                  idx;
    s32                  i;
    McSaveData*          save;
    s32                  mask;
    s32                  one;
    s32                  result;
    s32                  col;
    s32                  col2;
    s32                  item;
    s32                  item2;
    s32                  remaining;
    s32                  dt;
    s32                  temp;
    s32                  tmp;
    UiObject*            obj;

    obj           = arg0->spawnArg2;
    obj->field_2E = 0;
    if (arg0->state == 0) {
        if (D_replay_bonus_80119284 < 0) {
            obj->field_2E = 6;
            return;
        }
        arg0->killCountdown = 0xBC;
        temp                = arg0->spawnArg1;
        arg0->extraState    = temp;
        SCHED_BARRIER();
        col = temp;
        tmp = func_replay_bonus_80115CA4();
        SCHED_BARRIER();
        p     = D_replay_bonus_80118F78;
        spend = tmp;
        idx   = 0;
        if (D_80072A9C == 0x1FFF) {
            result = -1;
        } else {
            i = 0;
            do {
            loop:
                if (!(p->spendThreshold < spend)) {
                    idx = i;
                    break;
                }
                i++;
                p++;
                if (i < 0xD) {
                    goto loop;
                }
            } while (0);

            save = &Mc_SaveData;
            idx += save->field_F;
            i    = 0;
            if (idx >= 0xD) {
                idx = 0xC;
            }
            one  = 1;
            mask = save->field_934;
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
        item = 0;
        if (result >= 0) {
            item = D_replay_bonus_80118F78[result].items[col];
        }
        Gp_SetPreviewItem(item, 0);

        col2 = arg0->extraState;
        tmp  = func_replay_bonus_80115CA4();
        SCHED_BARRIER();
        p     = D_replay_bonus_80118F78;
        spend = tmp;
        idx   = 0;
        if (D_80072A9C == 0x1FFF) {
            result = -1;
        } else {
            i = 0;
            do {
            loop3:
                if (!(p->spendThreshold < spend)) {
                    idx = i;
                    break;
                }
                i++;
                p++;
                if (i < 0xD) {
                    goto loop3;
                }
            } while (0);

            save = &Mc_SaveData;
            idx += save->field_F;
            i    = 0;
            if (idx >= 0xD) {
                idx = 0xC;
            }
            one  = 1;
            mask = save->field_934;
            do {
            loop4:
                if ((mask & (one << idx)) == 0) {
                    break;
                }
                idx += 1;
                if (idx >= 0xD) {
                    idx -= 0xD;
                }
                i += 1;
                if (i < 0xD) {
                    goto loop4;
                }
            } while (0);
            result = idx;
        }
        if (result < 0) {
            item2 = 0;
        } else {
            item2 = D_replay_bonus_80118F78[result].items[col2];
        }
        arg0->spawnArg1 = item2 + 0x20000;
    }
    func_800C5F70(arg0);
    dt                  = D_80071072;
    remaining           = (u16)arg0->killCountdown - dt;
    arg0->killCountdown = remaining;
    if ((remaining << 0x10) <= 0) {
        obj->field_2E       = 6;
        arg0->killCountdown = 0x7FFF;
    }
}

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

void func_replay_bonus_80116EC0(void)
{
    McSaveData    copy;
    McSaveData*   dst;
    McSaveData*   save;
    WipSysConfig* cfg;
    u8*           p;
    s32           i;
    s32           j;
    s32           val92c;
    s32           val930;
    s32           val934;
    u32           val938;
    u8            val92a;
    s32           sum;
    s32           shift;
    s8            tmp;
    s32           exp;
    u32           bits;
    u32           masked;
    s32           n;

    cfg  = &Wip_SysConfig;
    copy = Mc_SaveData;
    Mc_InitBufferSlots();
    dst            = &Mc_SaveData;
    dst->field_E   = (u8)copy.field_E;
    dst->field_21  = (u8)copy.field_21;
    dst->field_23  = (u8)copy.field_23;
    dst->field_25  = copy.field_25;
    dst->field_1a8 = (u8)copy.field_1a8;
    dst->field_1aa = copy.field_1aa;
    dst->field_1ab = (u8)copy.field_1ab;
    dst->field_1a9 = (u8)copy.field_1a9;
    i              = 0;
    do {
        dst->field_6D0[i] = copy.field_6D0[i];
        i                += 1;
    } while (i < 0x60);
    i = 0;
    do {
        Mc_SaveData.field_862[i] = (u16)copy.field_862[i];
        i                       += 1;
    } while (i < 0x12);
    i = 0;
    do {
        Mc_SaveData.field_888[i] = copy.field_888[i];
        i                       += 1;
    } while (i < 0x20);

    save   = &Mc_SaveData;
    val92c = copy.field_92C;
    val930 = copy.field_930;
    val934 = copy.field_934;
    val938 = copy.field_938;
    val92a = (u8)copy.field_92A;
    SOFT_BARRIER();
    tmp             = (u8)save->field_E;
    save->field_92B = 0xFF;
    tmp             = tmp + 1;
    save->field_E   = tmp;
    save->field_92C = val92c;
    save->field_930 = val930;
    save->field_934 = val934;
    save->field_938 = val938;
    save->field_92A = val92a;
    if (tmp >= 0x64) {
        save->field_E = 0x63;
    }
    if (val92c < D_replay_bonus_80119274.unk0) {
        save->field_92C = D_replay_bonus_80119274.unk0;
    }
    if (val930 < D_replay_bonus_80119274.field_4) {
        save->field_930 = D_replay_bonus_80119274.field_4;
    }
    sum = D_replay_bonus_80119274.field_C + D_replay_bonus_80119274.field_14;
    if (sum > 0x98967F) {
        sum = 0x98967F;
    }
    cfg->field_C   = sum;
    save->field_12 = 0xF;
    exp            = D_replay_bonus_80119274.field_8;
    cfg->field_8   = exp;
    save->field_14 = exp;
    save->field_18 = cfg->field_C;
    if (copy.field_F >= 2) {
        save->field_92A = 2;
    } else if (save->field_92A <= 0) {
        if (D_replay_bonus_80119274.unk0 > 0x10D88) {
            save->field_92A = 1;
        }
    }
    p = copy.unknown_850;
    j = 0;
    do {
        shift = j * 2;
        bits  = Mc_SaveData.field_938;
        n     = *p;
        if ((s32)((bits >> shift) & 3) < n) {
            masked                = bits & ~(3 << shift);
            Mc_SaveData.field_938 = masked;
            Mc_SaveData.field_938 = masked | (*p << shift);
        }
        j += 1;
        p += 1;
    } while (j < 0xC);
    if (D_replay_bonus_80119284 >= 0) {
        Mc_SaveData.field_934 |= 1 << D_replay_bonus_80119284;
    }
}
