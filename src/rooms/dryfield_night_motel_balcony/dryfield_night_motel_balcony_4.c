#include "common.h"

#include <psyq/inline_c.h>

#include "rooms/room_common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32 Gp_LcgState;

extern SVECTOR D_dryfield_night_motel_balcony_80182D20;

#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void func_dryfield_night_motel_balcony_801819E0(Task* task, s32 arg);
void func_dryfield_night_motel_balcony_8018221C(Task* task, u8* color, s16 tick);

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

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
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

void func_dryfield_night_motel_balcony_80180580(Task* task)
{
    void*          work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    s32            i;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }
    switch (task->state) {
        case 0:
            task->state = task->spawnArg1 * 2 + 1;
            break;
        case 1:
            for (i = 0; i < 8; i++) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003D, coord, (((u32)Gp_LcgState >> 16) & 0xFF) | 0x100, NULL);
            }
            task->state = 2;
            break;
        case 2:
            for (i = 0; i < 4; i++) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003D, coord, (((u32)Gp_LcgState >> 16) & 0xFF) | 0x10100, NULL);
                Gp_SpawnEff(0x60095, coord, 0x400, NULL);
            }
            task->state = 10;
            break;
        case 3:
            for (i = 0; i < 6; i++) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003D, coord, (((u32)Gp_LcgState >> 16) & 0x7F) | 0x80, NULL);
            }
            task->state = 4;
            break;
        case 4:
            for (i = 0; i < 3; i++) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003D, coord, (((u32)Gp_LcgState >> 16) & 0x7F) | 0x10080, NULL);
                Gp_SpawnEff(0x60095, coord, 0x400, NULL);
            }
            task->state = 10;
            break;
        case 5:
            for (i = 0; i < 4; i++) {
                Gp_SpawnEff(0x60095, coord, 0x40000300, NULL);
            }
            task->state = 6;
            break;
        case 6:
            for (i = 0; i < 4; i++) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003D, coord, (((u32)Gp_LcgState >> 16) & 0x7F) | 0x80000080, NULL);
            }
            for (i = 0; i < 2; i++) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003D, coord, (((u32)Gp_LcgState >> 16) & 0x7F) | 0x80010080, NULL);
            }
            task->state = 10;
            break;
        case 7:
            for (i = 0; i < 8; i++) {
                Gp_SpawnEff(0x60095, coord, 0x10400, NULL);
            }
            task->state = 8;
            break;
        case 8:
            for (i = 0; i < 8; i++) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003D, coord, (((u32)Gp_LcgState >> 16) & 0xFF) | 0x100, NULL);
            }
            task->state = 10;
            break;
        case 9:
            for (i = 0; i < 8; i++) {
                Gp_SpawnEff(0x60095, coord, 0x10400, NULL);
            }
            task->state = 10;
            break;
        case 10:
        release:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_801809CC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_80180C60);

/// Per-frame handler of an effect-spawning room task. Any non-zero event state
/// suspends it, and 4 or above releases it. In view 0x27 it makes three
/// independent LCG rolls each frame: 1 in 4 spawns effect 0x6003D and 1 in 3
/// spawns 0x60093, both with an offset of up to 0x100 on every axis, and 1 in 7
/// spawns 0x60095 with a horizontal offset of up to 0x80. In any other view it
/// counts `field_22` up to 150 frames and then releases itself. Until then it
/// makes two rolls that fire less often as the count grows (the count must be
/// below a draw modulo 150, then modulo 120), each followed by a 1-in-4 roll
/// that spawns 0x60095, first with a vertical offset of up to 0x7FF and then at
/// a fixed height of 0xC00.
void func_dryfield_night_motel_balcony_80181024(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    s32            lo;
    s32            arg;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }
    if (gGameSession->at4.loc.view == 0x27) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            lo                = ((u32)Gp_LcgState >> 16) & 0x1FF;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            arg               = ((((u32)Gp_LcgState >> 16) % 3) << 16) + 0x80000100;
            Gp_SpawnEff(0x6003D, coord, lo + arg, &work->field_10);
        }
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((u16)(((u32)Gp_LcgState >> 16) % 3U) == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60093, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x100, &work->field_10);
        }
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((u16)(((u32)Gp_LcgState >> 16) % 7U) == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
            work->field_10.vy = 0;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60095, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0xA0000400, &work->field_10);
        }
    } else {
        work->field_22++;
        if ((s16)work->field_22 < 150) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((s16)work->field_22 < (u16)(((u32)Gp_LcgState >> 16) % 150U)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vy = ((u32)Gp_LcgState >> 16) & 0x7FF;
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60095, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x80000400,
                                &work->field_10);
                }
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((s16)work->field_22 < (u16)(((u32)Gp_LcgState >> 16) % 120U)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    work->field_10.vy = 0xC00;
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60095, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x20010400,
                                &work->field_10);
                }
            }
        } else {
        release:
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Per-frame handler of a drifting room effect task, a variant of
/// `func_dryfield_night_motel_balcony_80181E7C`. The first frame resets the
/// model's rotation to identity, keeps the low twelve bits of
/// `Task::spawnArg1` in `field_18`, rolls a frame period (1..4 ticks) into
/// `field_1A` and a value into `field_1C`, and, when the spawner left no drift,
/// rolls one whose ranges depend on `spawnArg1` (bit 30: +-0x80 on every axis;
/// negative: +-0x10 across and 0..-0xFF in y; otherwise +-0x80 across and
/// 0..15 in y) and turns it into `field_8`'s frame. The drift is normalised and scaled to `field_24`
/// (0x40 with bit 30 or bit 29, else 0x80), and `spawnArg1` is replaced by
/// two bits of its upper half. Later frames advance `field_20` once per period,
/// move the model by the drift, decrementing its y by one a tick, and hand
/// the task to `func_dryfield_night_motel_balcony_801819E0` until `field_20`
/// reaches 12, when it is released. Event states 2 and 3 suspend it, 4 and
/// above release it at once, and state 1 freezes the drift and the tick.
void func_dryfield_night_motel_balcony_8018158C(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    MATRIX*        m;
    s32            half; // default drift length and the centre of the wide drift rolls

    if (Gp_State1C->eventState >= 2) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }

    Gp_UpdateCoord(coord);
    half = 0x80;
    work->field_22++;

    switch (task->state) {
        case 0:
            m                  = &coord->coord;
            *(s32*)&m->m[0][0] = 0x1000;
            *(s32*)&m->m[0][2] = 0;
            *(s32*)&m->m[1][1] = 0x1000;
            *(s32*)&m->m[2][0] = 0;
            m->m[2][2]         = 0x1000;
            work->field_18     = (u16)task->spawnArg1 & 0xFFF;
            work->field_24     = half;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_1A     = (((u32)Gp_LcgState >> 16) & 3) + 1;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_1C     = ((u32)Gp_LcgState >> 16) & 0xFFF;
            work->field_20     = 0;
            if ((work->field_10.vx | work->field_10.vy | work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0x40000000) {
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vx = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vy = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                    work->field_10.vz = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    work->field_24    = 0x40;
                } else {
                    if (task->spawnArg1 < 0) {
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                    } else {
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = ((u32)Gp_LcgState >> 16) & 0xF;
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = half - (((u32)Gp_LcgState >> 16) & 0xFF);
                    }
                    if (task->spawnArg1 & 0x20000000) {
                        work->field_24 = 0x40;
                    }
                }
                gte_SetRotMatrix(&work->field_8->coord);
                gte_ldv0(&work->field_10);
                gte_rtv0_real();
                gte_stsv(&work->field_10);
            }
            VectorNormalSS(&work->field_10, &work->field_10);
            gte_lddp(work->field_24);
            gte_ldsv(&work->field_10);
            gte_gpf12_real();
            gte_stsv(&work->field_10);
            coord->flg      = 0;
            task->state     = 1;
            task->spawnArg1 = (s16)(task->spawnArg1 >> 16) & 3;
            break;
        case 1:
            if (Gp_State1C->eventState == 0) {
                if ((s16)work->field_22 % (s16)work->field_1A == 0) {
                    work->field_20++;
                }
                coord->coord.t[0] += work->field_10.vx;
                coord->coord.t[1] += work->field_10.vy;
                coord->coord.t[2] += work->field_10.vz;
                coord->flg         = 0;
                work->field_10.vy--;
            } else {
                work->field_22--;
            }
            if ((s16)work->field_20 < 12) {
                func_dryfield_night_motel_balcony_801819E0(task, task->spawnArg1);
            } else {
            release:
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_balcony/dryfield_night_motel_balcony_4", func_dryfield_night_motel_balcony_801819E0);

/// Per-frame handler of a drifting room effect task. The first frame resets the
/// model's rotation to identity, rolls a starting animation step (0..9) and a
/// lifetime (5..14 ticks), and, when the spawner left no drift, rolls one and
/// turns it into `field_8`'s frame. The drift is then normalised and scaled to a
/// length chosen by `Task::spawnArg1` (8 when negative, 0x80 with bit 30, 0x20
/// otherwise). Later frames move the model by the drift, bending it by one
/// unit a tick, and draw it through `func_dryfield_night_motel_balcony_8018221C`,
/// fading its colour over the last ten ticks before releasing the task. Event
/// states 2 and 3 suspend it; 4 and above release it at once, and any non-zero
/// state below that freezes the drift and the lifetime tick.
void func_dryfield_night_motel_balcony_80181E7C(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    MATRIX*        m;
    s32            seed;
    s16            tick;
    s16            end;
    u8             color[3];

    if (Gp_State1C->eventState >= 2) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }

    Gp_UpdateCoord(coord);
    work->field_22++;

    switch (task->state) {
        case 0:
            seed               = Gp_LcgState * 5 + 0x71357911;
            m                  = &coord->coord;
            *(s32*)&m->m[0][0] = 0x1000;
            *(s32*)&m->m[0][2] = 0;
            *(s32*)&m->m[1][1] = 0x1000;
            *(s32*)&m->m[2][0] = 0;
            m->m[2][2]         = 0x1000;
            work->field_18     = task->spawnArg1 & 0xFFF;
            Gp_LcgState        = seed;
            work->field_20     = ((u32)Gp_LcgState >> 16) % 10;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_26     = ((u32)Gp_LcgState >> 16) % 10 + 5;
            if ((work->field_10.vx | work->field_10.vy | work->field_10.vz) == 0) {
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                if (task->spawnArg1 < 0) {
                    work->field_24 = 8;
                } else if (task->spawnArg1 & 0x40000000) {
                    work->field_24 = 0x80;
                } else {
                    work->field_24 = 0x20;
                }
                gte_SetRotMatrix(&work->field_8->coord);
                gte_ldv0(&work->field_10);
                gte_rtv0_real();
                gte_stsv(&work->field_10);
            }
            VectorNormalSS(&work->field_10, &work->field_10);
            gte_lddp(work->field_24);
            gte_ldsv(&work->field_10);
            gte_gpf12_real();
            gte_stsv(&work->field_10);
            coord->flg  = 0;
            task->state = 1;
            break;
        case 1:
            if (Gp_State1C->eventState == 0) {
                work->field_20++;
                work->field_10.vy--;
                coord->coord.t[0] += work->field_10.vx;
                coord->coord.t[1] += work->field_10.vy;
                coord->coord.t[2] += work->field_10.vz;
                coord->flg         = 0;
            } else {
                work->field_22--;
            }
            tick = work->field_22;
            end  = work->field_26;
            if (tick < end - 10) {
                func_dryfield_night_motel_balcony_8018221C(task, NULL, tick);
            } else if (tick < end) {
                color[0] = color[1] = color[2] = (end - tick) * 12;
                func_dryfield_night_motel_balcony_8018221C(task, color, tick);
            } else {
            release:
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

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
