#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_forked_road.h"
#include "rooms/room_common.h"

/// `rtps` on v0. The `inline_c.h` macro of that name assembles to a different
/// word, so spell the instruction out.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

/// The fourteen spawn offsets of the forked road's ambient effects, indexed
/// 0..13 by the first-frame burst below.
extern SVECTOR D_acropolis_forked_road_80182178[14];

/// One bit per in-game day (shifted by `GameSession::field_4 - 1`) for each of
/// the sixteen ambient-effect slots: which of the room's lamps are lit today.
extern u16 D_acropolis_forked_road_801821E8[16];

/// Forked-road ambient effect task. On its first frame it fires one effect per
/// entry of `D_acropolis_forked_road_80182178`, in four runs that differ only
/// in the flavour bits added to the entry's index - two 0x02000000, two
/// 0x03000000, eight 0x02000100 and two 0x00000200 - and then publishes the
/// room's three ambient sound events before marking itself done.
void func_acropolis_forked_road_8017E298(Task* task)
{
    GsCOORDINATE2* coord;
    s32            i;

    coord = ((TmdObject*)task->extra)->field_8;
    if (task->state == 0) {
        for (i = 0; i < 2; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x2000000, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 2; i < 4; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x3000000, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 4; i < 0xC; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x2000100, &D_acropolis_forked_road_80182178[i]);
        }
        for (i = 0xC; i < 0xE; i++) {
            Gp_SpawnEff(0x60089, coord, i + 0x200, &D_acropolis_forked_road_80182178[i]);
        }
        D_80115758  = 0x60290;
        D_8011572C  = 0x60291;
        D_80115750  = 0x60292;
        task->state = task->state + 1;
    }
}

/// Draws one frame of a forked-road wall lamp: a flickering, screen-aligned
/// sprite at the task's own coordinate frame. The lamp is skipped entirely
/// while the effect pool is busy (`Gp_State1C::field_4` at 4 or more) and on
/// the days whose bit is clear in `D_acropolis_forked_road_801821E8`, indexed
/// by the low nibble of `Task::spawnArg1`.
///
/// On the first frame the task unpacks the rest of `spawnArg1` into its work
/// object - half extent, animation column and that column's grey level - and
/// leaves only the day index behind. Every frame it then projects the
/// coordinate's translation through `GsWSMATRIX` with a single `RTPS` into a
/// 0x14-byte `G_SCRATCH_HEAD` block and, for anything at `otz` 0x11 or
/// further, queues one semi-transparent `POLY_FT4` on tpage 0x2B whose
/// half extent is `width * 39 / otz`, so the lamp shrinks with distance. The
/// grey alternates by 0x10 on the parity of `DisplayState::field_8`, which is
/// what makes it flicker.
void func_acropolis_forked_road_8017E410(Task* task)
{
    void**            scratch;
    RoomShaftScratch* block;
    AfrLampWork*      work;
    GsCOORDINATE2*    coord;
    POLY_FT4*         prim;
    DisplayState*     ds;
    s32               rgb;
    s32               flip;
    s16               xy;

    work  = (AfrLampWork*)task->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    if (Gp_State1C->field_4 < 4 &&
        ((D_acropolis_forked_road_801821E8[task->spawnArg1 & 0xF] >> ((u8)Game_Session->field_4 - 1)) & 1)) {
        Gp_UpdateCoord(coord);
        scratch  = (void**)G_SCRATCH_HEAD;
        *scratch = (u8*)*scratch - 0x14;
        block    = (RoomShaftScratch*)*scratch;
        if (task->state == 0) {
            u8 levels[3] = { 0x50, 0x30, 0x10 };

            if (task->spawnArg1 & 0xFFF0000) {
                work->width = (task->spawnArg1 >> 16) & 0xFFF;
            } else {
                work->width = 0x280;
            }
            work->frame     = (task->spawnArg1 >> 8) & 3;
            task->spawnArg1 = task->spawnArg1 & 0xF;
            work->color     = levels[work->frame];
            task->state     = task->state + 1;
        }
        block->vec.vx = *(u16*)&coord->workm.t[0];
        block->vec.vy = *(u16*)&coord->workm.t[1];
        block->vec.vz = *(u16*)&coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps_real();
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2C);
        gte_stsxy(&block->sx);
        gte_stszotz(&block->otz);
        if (block->otz >= 0x11) {
            ds   = &Display_State;
            flip = (u8)ds->field_8;
            SOFT_BARRIER();
            rgb         = (u8)work->color;
            prim->tpage = 0x2B;
            rgb        += (flip & 1) << 4;
            prim->r0    = rgb;
            prim->g0    = rgb;
            prim->b0    = rgb;
            setSemiTrans(prim, 1);
            setClut(prim, work->frame * 16, 0x10E);
            prim->u0 = work->frame * 0x28;
            prim->v0 = 0;
            prim->u1 = work->frame * 0x28 + 0x27;
            prim->v1 = 0;
            prim->u2 = work->frame * 0x28;
            prim->v2 = 0x27;
            prim->u3 = work->frame * 0x28 + 0x27;
            prim->v3 = 0x27;

            block->halfWidth = (work->width * 0x27) / block->otz;
            xy               = block->sx - *(u16*)&block->halfWidth;
            prim->x2         = xy;
            prim->x0         = xy;
            xy               = block->sx + *(u16*)&block->halfWidth;
            prim->x3         = xy;
            prim->x1         = xy;
            xy               = block->sy - *(u16*)&block->halfWidth;
            prim->y1         = xy;
            prim->y0         = xy;
            xy               = block->sy + *(u16*)&block->halfWidth;
            prim->y3         = xy;
            prim->y2         = xy;
            addPrim((u_long*)(((((u32)block->otz << ds->field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt), prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    }
}
