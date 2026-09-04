#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/display.h"
#include "rooms/room_common.h"
#include <psyq/libgpu.h>

extern s32 Gp_LcgState;

extern SVECTOR D_acropolis_promenade_80181AFC[];
extern SVECTOR D_acropolis_promenade_80181B0C[];
extern SVECTOR D_acropolis_promenade_80181B14[];
extern u16     D_acropolis_promenade_80181B74;
extern u16     D_acropolis_promenade_80181B76;
extern u16     D_acropolis_promenade_80181B78[];

/// Per-frame effect spawner for the promenade. `D_acropolis_promenade_80181B74`
/// / `_80181B76` and the twelve-entry mask table `_80181B78` are per-view bit
/// masks: bit `view - 1` of an entry says whether that emitter is visible from
/// the camera `Gp_GetViewIndex` reports, and the parallel twelve-entry
/// `_80181B14` array holds each emitter's offset from the room's coordinate
/// frame. View 7 spawns nothing.
void func_acropolis_promenade_8017E03C(Task* task)
{
    GsCOORDINATE2* coord;
    RoomEffWork*   work;
    u8             view;
    s32            i;
    s32            mask;
    s16            prev;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = task->spawnArg2;
    view  = Gp_GetViewIndex();
    if (Gp_State1C->field_4 >= 4) {
        return;
    }
    work->field_22++;
    if (view == 7) {
        return;
    }
    mask = 1 << (view - 1);
    if (D_acropolis_promenade_80181B74 & mask) {
        Gp_SpawnEff(0x8006004B, coord, (s16)work->field_22, &D_acropolis_promenade_80181AFC[0]);
        Gp_SpawnEff(0x8006004B, coord, (s16)work->field_22, &D_acropolis_promenade_80181AFC[1]);
        Gp_SpawnEff(0x60057, coord, (s16)work->field_22, &D_acropolis_promenade_80181B0C[0]);
        Room_Draw21(&D_acropolis_promenade_80181AFC[-1], 0x100, 0x5C40);
    }
    for (i = 0; i < 3; i++) {
        if (D_acropolis_promenade_80181B78[i] & mask) {
            Gp_SpawnEff(0x60062, coord, 0, &D_acropolis_promenade_80181B14[i]);
        }
    }
    for (i = 3; i < 5; i++) {
        if (D_acropolis_promenade_80181B78[i] & mask) {
            Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[i]);
        }
        if (D_acropolis_promenade_80181B78[i + 2] & mask) {
            Gp_SpawnEff(0x60062, coord, 2, &D_acropolis_promenade_80181B14[i + 2]);
        }
        if (D_acropolis_promenade_80181B78[i + 4] & mask) {
            Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[i + 4]);
        }
        if (D_acropolis_promenade_80181B78[i + 6] & mask) {
            Gp_SpawnEff(0x60062, coord, 2, &D_acropolis_promenade_80181B14[i + 6]);
        }
    }
    if (D_acropolis_promenade_80181B78[11] & mask) {
        Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[11]);
    }
    if (D_acropolis_promenade_80181B76 & mask) {
        prev = work->field_24;
        if (prev != view) {
            for (i = 0; i < 0x28; i++) {
                Gp_SpawnEff(0x60056, coord, view, NULL);
            }
        } else {
            Gp_SpawnEff(0x60056, coord, prev, NULL);
            Gp_SpawnEff(0x60056, coord, prev, NULL);
        }
    }
    work->field_24 = view;
}

/// One falling water drip on the promenade, drawn as a `DR_MOVE` that smears a
/// one-pixel-tall strip of the frame buffer down by a pixel. The first frame
/// rolls the whole drip out of `Gp_LcgState`: `field_10.vx` is the column
/// (0..0xEF), `field_10.vy` the row it starts on (0xB0..0xEF), `field_24` the
/// lifetime in frames, `field_26` the width and `field_28` the number of frames
/// each row of fall takes. `Display_State.field_1f` picks the buffer half, and
/// the OT slot is the row scaled into the 0x500-deep range so a drip sorts
/// against the room behind it. The task releases itself once the camera turns
/// away, the lifetime runs out, or the drip falls off the bottom of the screen.
void func_acropolis_promenade_8017E394(Task* task)
{
    RoomEffWork* work;
    RECT         rect;
    DR_MOVE*     mv;
    u16          rnd;
    s32          bufferY;
    s32          x;
    s32          y;
    s32          onScreen;
    s32          depth;

    work    = task->spawnArg2;
    bufferY = Display_State.field_1f * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if ((s16)work->field_22 == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vx = ((u32)Gp_LcgState >> 16) % 240;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vy = (((u32)Gp_LcgState >> 16) & 0x3F) + 0xB0;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rnd               = (u32)Gp_LcgState >> 16;
            work->field_24    = (u32)rnd % 90 + 0x1E;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_26    = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y        = work->field_10.vy + (s16)work->field_22 / (s16)work->field_28;
        x        = work->field_10.vx;
        depth    = 0x500 - (y - 0xB0) * 10;
        onScreen = y < 0xEF;
        if (onScreen) {
            rect.x         = x;
            rect.y         = y + bufferY;
            rect.w         = work->field_26;
            rect.h         = 1;
            mv             = (DR_MOVE*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(mv + 1);
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(Gpu_CurrentOt + (depth >> 4), mv);
        }
        work->field_22++;
        if ((s16)work->field_22 <= (s16)work->field_24 && onScreen) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_4", func_acropolis_promenade_8017E634);

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_4", func_acropolis_promenade_8017ED44);

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_4", func_acropolis_promenade_8017F0BC);
