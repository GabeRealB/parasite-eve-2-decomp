#include "gameplay/object_fields.h"

#include "common.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "gameplay/enemy.h"
#include "item_menu.h"
#include "item_pickup.h"
#include "gameplay/pairsrc.h"
#include "gameplay/weapon_data.h"

#include "main/gamemain.h"

/// 4-byte table entry at `Gp_IdField0`. `Gp_LookupIdField(idx, 0)` returns
/// `field_0` for index `(u16)idx`.
typedef struct _GpRec4 {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} GpRec4;
STATIC_ASSERT_SIZEOF(GpRec4, 0x4);

/// 6-byte table entry at `Gp_IdField1`. `Gp_LookupIdField(idx, 1)` returns
/// `field_0` for index `(u16)idx`.
typedef struct _GpRec6 {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
} GpRec6;
STATIC_ASSERT_SIZEOF(GpRec6, 0x6);

#define D_80113D38 (Gp_AttachParams[0].percentages)

/// 4-byte records selected by `Gp_LookupIdField(..., 0)`.
extern GpRec4 Gp_IdField0[];

/// 6-byte records selected by `Gp_LookupIdField(..., 1)`.
extern GpRec6 Gp_IdField1[];

/// Unreferenced nonzero halfword following the ID-field table.
extern u16 D_80114096;

GpRec4 Gp_IdField0[11] = {
    { 0, 0 },
    { 0, 0 },
    { 10, 5 },
    { 15, 0 },
    { 20, 5 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
};
GpRec6 Gp_IdField1[11] = {
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 180, 7, 7 },
    { 240, 0, 0 },
    { 520, 7, 7 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
};
/// Unreferenced nonzero halfword following the ID-field table.
u16 D_80114096 = 0x3430;

s32 Gp_LookupIdField(s32 arg0, s32 arg1)
{
    s32 ret;

    ret = 0;
    switch (arg1) {
        case 0:
            ret = Gp_IdField0[(u16)arg0].field_0;
            break;
        case 1:
            ret = Gp_IdField1[(u16)arg0].field_0;
            break;
    }
    return ret;
}

s32 Gp_GetIdParam0(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = Gp_IdParamLo[arg0 & 0x7F].params[2];
    } else {
        ret = Gp_IdParamHi.rows[arg0 & 0x7F].field[5];
    }
    return ret;
}

s32 Gp_GetIdParam1(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = Gp_IdParamLo[arg0 & 0x7F].params[3];
    } else {
        ret = Gp_IdParamHi.rows[arg0 & 0x7F].field[6];
    }
    return ret;
}

void Gp_SetObjFlag4(GpEnemy* arg0, s32 arg1, s32 arg2)
{
    s32 val;
    s32 limit;
    s32 rand;

    val         = arg0->param->flag4Chance;
    limit       = (val << 12) / 100;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    rand        = (u32)Gp_LcgState >> 16 & 0xFFF;
    if (rand < limit) {
        arg0->flag4Ticks     = 0;
        arg0->reactionFlags |= 4;
        Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
        arg0->flag4Delay     = ((u32)Gp_LcgState >> 16 & 0xF) + 0x53;
        if ((arg1 & 0x8000) == 0) {
            arg0->flag4Grade = 0;
            return;
        }
        arg0->flag4Grade = Gp_StateC08.field_0 % 10U;
    }
}

s32 Gp_TickObjFlag4(GpEnemy* arg0)
{
    s32 ret;
    s32 val;
    s32 scale;

    ret = 0;
    arg0->flag4Delay--;
    if (arg0->flag4Delay == 0) {
        arg0->flag4Ticks++;
        Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
        arg0->flag4Delay = ((u32)Gp_LcgState >> 16 & 0xF) + 0x53;
        val              = arg0->param->hpMax;
        scale            = D_80113D38[arg0->flag4Grade];
        ret              = (val * scale) / 100;
        if (ret == 0) {
            ret = 1;
        }
    }
    return ret;
}

s32 Gp_ObjFlag4Expired(GpEnemy* arg0)
{
    s32 val;
    s32 ret;

    ret = 0;
    val = arg0->param->flag4Ticks;
    if (!(arg0->reactionFlags & 4)) {
        return 1;
    }
    if (val == 0) {
        return 0;
    }
    val = (val * D_80113D28[arg0->flag4Grade]) / 100;
    if (arg0->flag4Ticks >= val) {
        ret = 1;
    }
    return ret;
}

void Gp_SetObjFlag1(GpEnemy* arg0)
{
    arg0->reactionFlags |= 1;
}

void Gp_SetObjFlag2(GpEnemy* arg0, s32 arg1, s32 arg2)
{
    arg0->flag2Steps     = 0;
    arg0->flag2Timer     = 0;
    arg0->reactionFlags |= 2;
    if ((arg1 & 0x8000) == 0) {
        arg0->flag2Grade = 0;
        return;
    }
    if ((arg1 & 0x3F) == 0x31) {
        arg0->flag2Grade = 0;
        return;
    }
    arg0->flag2Grade = Gp_StateC08.field_0 % 10U;
}

s32 Gp_TickObjFlag2(GpEnemy* arg0)
{
    s32 ret;
    s32 limit;
    s32 val;
    s32 scale;

    ret = 0;
    val = arg0->param->flag2Ticks;
    if (val == 0) {
        return ret;
    }
    scale = D_80113D30[arg0->flag2Grade];
    limit = (val * scale) / 100;
    if (arg0->flag2Steps < limit) {
        arg0->flag2Timer++;
        if (arg0->flag2Timer >= 0x1F) {
            arg0->flag2Steps++;
            if (arg0->flag2Steps >= limit) {
                Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
                arg0->flag2Timer = (u32)Gp_LcgState >> 16 & 0x3F;
            } else {
                arg0->flag2Timer = 0;
            }
        }
    } else {
        arg0->flag2Timer--;
        if (arg0->flag2Timer == 0) {
            ret = 1;
        }
    }
    return ret;
}

s32 Gp_GetIdParam2(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = Gp_IdParamLo[arg0 & 0x7F].params[4];
    } else {
        ret = Gp_IdParamHi.rows[arg0 & 0x7F].field[7];
    }
    return ret;
}
