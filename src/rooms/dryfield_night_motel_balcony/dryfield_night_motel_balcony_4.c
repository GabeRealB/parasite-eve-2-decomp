#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32 Gp_LcgState;

extern SVECTOR D_dryfield_night_motel_balcony_80182D20;

/// Draws one axis-aligned `POLY_FT4` panel of a 0x28-pixel sprite at the packed
/// screen position `arg0` (x in the low half, y in the high half). `arg1` is
/// the ordering-table index, `arg2` the panel width and `arg3` the animation
/// step, which walks frames 2..11 of `D_80111E48`. The quad is `2 * d` wide and
/// `4 * d` tall, anchored three quarters of the way down, and both `d` and the
/// rounded weight `3 * d` are the one reused local the ROM keeps for them.
void func_dryfield_night_motel_balcony_8017F6C8(s32 arg0, s16 arg1, s16 arg2, s16 arg3)
{
    POLY_FT4* prim;
    GpEffUv8* rec;
    s16       idx;
    GpEffUv8* tbl;
    s32       d;
    s32       y;

    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2F);
    prim->tpage = 0x29;

    idx        = arg3 % 10 + 2;
    tbl        = D_80111E48;
    rec        = &tbl[idx];
    prim->clut = (rec->clutY << 6) | ((rec->clutX >> 4) & 0x3F);
    prim->u0   = rec->u;
    prim->v0   = rec->v;
    prim->u1   = rec->u + 0x27;
    prim->v1   = rec->v;
    prim->u2   = rec->u;
    prim->v2   = rec->v + 0x27;
    prim->u3   = rec->u + 0x27;
    prim->v3   = rec->v + 0x27;

    d        = (arg2 * 0x1F) >> 12;
    prim->x2 = arg0 - d;
    prim->x0 = arg0 - d;
    prim->x3 = arg0 + d;
    prim->x1 = arg0 + d;

    d        = (arg2 * 0x1F) >> 13;
    y        = arg0 >> 16;
    prim->y1 = y - d * 3;
    prim->y0 = y - d * 3;
    prim->y3 = y + d;
    prim->y2 = y + d;

    addPrim(&gGpuCurrentOt[arg1], prim);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_8017F84C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_8017FF78);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_80180580);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_801809CC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_80180C60);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_80181024);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_8018158C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_801819E0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_80181E7C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_8018221C);

/// Spawns an 8-step burst of effect 0x6007E and then a 6-step burst of 0x60070
/// around part 3 of the model owned by the slot-4 task's child. Each step rolls
/// the room LCG four times (three for the second burst) and builds the offset
/// vector from the top byte of each draw; the first burst also carries the last
/// draw's low nine bits, biased by 0x300, in the spawn argument.
void func_dryfield_night_motel_balcony_8018257C(void)
{
    Task*          task;
    GsCOORDINATE2* coord;
    SVECTOR        sv;
    s32            i;

    task  = gameGetPtrSlot(4);
    coord = ((TmdObject*)task->firstChild->extra)->coords + 3;

    for (i = 0; i < 8; i++) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sv.vx       = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sv.vy       = 0xFE80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sv.vz       = 0x680 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_SpawnEff(0x6007E, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x300, &sv);
    }

    for (i = 0; i < 6; i++) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sv.vx       = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sv.vy       = 0xFE80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        sv.vz       = 0x680 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_SpawnEff(0x60070, coord, 0xC0033800, &sv);
    }
}

/// Rolls the room LCG (`Gp_LcgState`) once and, on a draw whose upper half is
/// a multiple of three, rolls it again and spawns effect 0x6007E at part 3 of
/// the model owned by the slot-4 task's child, carrying the second draw's low
/// nine bits in the upper half of the spawn argument.
void func_dryfield_night_motel_balcony_80182730(void)
{
    Task* task;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    if ((u16)(((u32)Gp_LcgState >> 16) % 3U) == 0) {
        task        = gameGetPtrSlot(4);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gp_SpawnEff(0x6007E, ((TmdObject*)task->firstChild->extra)->coords + 3,
                    (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x80000100,
                    &D_dryfield_night_motel_balcony_80182D20);
    }
}
