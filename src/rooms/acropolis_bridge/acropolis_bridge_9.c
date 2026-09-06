#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_bridge.h"
#include "rooms/room_common.h"
#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// `rtps` / `rtpt` / `mvmva 1, 0, 0, 3, 0` / `gpf 1`. The `inline_c.h` macros
/// of those names assemble to different words, so spell the instructions out.
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern s32 Gp_LcgState;
extern s32 D_80115738;
extern s32 D_8011574C;

extern GpMsgEntry D_acropolis_bridge_801898FC[];
extern SVECTOR    D_acropolis_bridge_8018991C[7];
extern SVECTOR    D_acropolis_bridge_80189954[7];
extern SVECTOR    D_acropolis_bridge_8018998C[11];
extern SVECTOR    D_acropolis_bridge_801899E4;
extern u16        D_acropolis_bridge_801899EC[8];
extern u16        D_acropolis_bridge_801899FC[16];
extern u16        D_acropolis_bridge_80189A1C[11];
extern u16        D_acropolis_bridge_80189A32;
extern SVECTOR    D_acropolis_bridge_80189A34[2];
extern SVECTOR    D_acropolis_bridge_80189A44;
extern SVECTOR    D_acropolis_bridge_80189A4C;

extern void func_acropolis_bridge_801827EC(GsCOORDINATE2* arg0, s32 arg1, s16 arg2);
extern void func_acropolis_bridge_80182F8C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
extern void func_acropolis_bridge_801833A0(GsCOORDINATE2* arg0, u16 arg1, s16 arg2);

/// Per-frame driver for the bridge's ambient effect field, and the room's
/// message-table owner. On the first frame it publishes
/// `D_acropolis_bridge_801898FC` as slot 5's `Gp_DispatchMsg` table and seeds
/// `D_acropolis_bridge_80189A34` with the two tracked cable joints' world
/// positions.
///
/// Each frame it re-spawns the effects the current camera can see: the two
/// per-view bitmask tables (`D_acropolis_bridge_801899EC` /
/// `D_acropolis_bridge_80189A1C`) say which of the placed emitters are visible
/// from view `Gp_GetViewIndex()`, and each visible entry spawns its dust
/// (0x600B1 / 0x600B2) or spark (0x600B3) at the matching `SVECTOR`. View 9
/// lifts the dust 0x240 above the placed point.
///
/// On views 2, 5 and 6 (`bit & 0x62`) it also trails debris off the two moving
/// joints: `field_26` is the Manhattan distance the joint travelled since last
/// frame, biased by 0x20, and two `Gp_LcgState` rolls against that distance
/// decide whether this frame emits `D_8011574C` / `D_80115738`. The joint's
/// new position is written back for the next frame's delta.
///
/// `D_acropolis_bridge_801899FC` finally maps the view onto one of five
/// looping ambience effects (0x600B4..0x600B8): entering the view bursts 30
/// copies at once, staying in it emits one per frame, or one in two / one in
/// three while `Gp_State1C->field_16` says the scene is quiet.
void func_acropolis_bridge_8017F868(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* part;
    Task*          owner;
    SVECTOR        pos;
    u8             view;
    s32            bit;
    s32            i;
    s32            delta;
    s32            axis;
    s32            dist;
    s32            prev;
    s16            lastView;
    u16            rnd;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    owner = Game_GetPtrSlot(3);
    part  = ((TmdObject*)owner->extra)->field_8;
    view  = Gp_GetViewIndex();
    if (Gp_State1C->field_4 >= 4) {
        return;
    }

    if (task->state == 0) {
        Game_Session->field_80 = 0;
        task->field_24         = D_acropolis_bridge_801898FC;
        Game_SetPtrSlot(task, 5);
        D_8011574C  = 0x600B9;
        D_80115738  = 0x600BA;
        task->state = task->state + 1;
        for (i = 0; i < 2; i++) {
            part                              = &((TmdObject*)owner->extra)->field_8[14 + i * 3];
            D_acropolis_bridge_80189A34[i].vx = part->workm.t[0];
            D_acropolis_bridge_80189A34[i].vy = part->workm.t[1];
            D_acropolis_bridge_80189A34[i].vz = part->workm.t[2];
        }
    }

    work->field_22 = work->field_22 + 1;
    switch (view) {
        case 6:
            Room_Draw21(&D_acropolis_bridge_80189A44, 0x100, 0x5C20);
            break;
        case 7:
            Room_Draw21(&D_acropolis_bridge_80189A44, 0x100, 0x5C20);
            break;
        case 3:
        case 4:
        case 9:
            Room_Draw21(&D_acropolis_bridge_80189A4C, 0x100, 0x50C2);
            break;
    }

    bit = 1 << (view - 1);
    if (view == 9) {
        for (i = 0; i < 7; i++) {
            if (D_acropolis_bridge_801899EC[i] & bit) {
                pos.vx = 0;
                pos.vy = -0x240;
                pos.vz = 0;
                pos.vx = D_acropolis_bridge_8018991C[i].vx;
                pos.vy = D_acropolis_bridge_8018991C[i].vy - 0x240;
                pos.vz = D_acropolis_bridge_8018991C[i].vz;
                Gp_SpawnEff(0x800600B1, coord, (s16)work->field_22 + i, &pos);
            }
        }
    } else {
        for (i = 0; i < 7; i++) {
            if (D_acropolis_bridge_801899EC[i] & bit) {
                Gp_SpawnEff(0x800600B1, coord, (s16)work->field_22 + i, &D_acropolis_bridge_8018991C[i]);
                Gp_SpawnEff(0x600B2, coord, (s16)work->field_22 + i, &D_acropolis_bridge_80189954[i]);
            }
        }
    }

    for (i = 0; i < 3; i++) {
        if (D_acropolis_bridge_80189A1C[i] & bit) {
            Gp_SpawnEff(0x600B3, coord, 0, &D_acropolis_bridge_8018998C[i]);
        }
    }
    for (i = 3; i < 5; i++) {
        if (D_acropolis_bridge_80189A1C[i] & bit) {
            Gp_SpawnEff(0x600B3, coord, 1, &D_acropolis_bridge_8018998C[i]);
        }
        if (D_acropolis_bridge_80189A1C[i + 2] & bit) {
            Gp_SpawnEff(0x600B3, coord, 2, &D_acropolis_bridge_8018998C[i + 2]);
        }
    }
    if (D_acropolis_bridge_80189A32 & bit) {
        Gp_SpawnEff(0x600B3, coord, 1, &D_acropolis_bridge_801899E4);
    }

    if ((bit & 0x62) && Gp_State1C->field_4 == 0 && part->coord.t[1] >= 0x201) {
        for (i = 0; i < 2; i++) {
            part  = &((TmdObject*)owner->extra)->field_8[14 + i * 3];
            delta = D_acropolis_bridge_80189A34[i].vx - part->workm.t[0];
            dist  = delta < 0;
            if (dist) {
                delta = part->workm.t[0] - D_acropolis_bridge_80189A34[i].vx;
            }
            prev = D_acropolis_bridge_80189A34[i].vy;
            axis = prev - part->workm.t[1];
            if (axis < 0) {
                axis = part->workm.t[1] - prev;
            }
            dist           = delta + axis;
            prev           = D_acropolis_bridge_80189A34[i].vz;
            axis           = part->workm.t[2];
            delta          = prev - axis;
            delta          = ((delta >= 0) ? (dist + delta) : (dist + (axis - prev))) + 0x20;
            work->field_26 = delta;

            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            rnd         = (u32)Gp_LcgState >> 16;
            if ((rnd & 0x1FF) < (s16)work->field_26) {
                Gp_SpawnEff(D_8011574C, part, 0x40, NULL);
            }
            work->field_26 = work->field_26 - 0x20;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            rnd            = (u32)Gp_LcgState >> 16;
            if ((rnd & 0x1FF) < (s16)work->field_26) {
                Gp_SpawnEff(D_80115738, part, 0x1202180, NULL);
            }

            D_acropolis_bridge_80189A34[i].vx = part->workm.t[0];
            D_acropolis_bridge_80189A34[i].vy = part->workm.t[1];
            D_acropolis_bridge_80189A34[i].vz = part->workm.t[2];
        }
    }

    D_acropolis_bridge_801917AC =
        (DR_MOVE*)((u8*)D_8005C374 + (Display_State.field_114 * 0x7000 + 0xA000));

    switch (D_acropolis_bridge_801899FC[view - 1]) {
        case 0:
            break;
        case 1:
            lastView = work->field_24;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(0x600B4, coord, view, NULL);
                }
            } else if (Gp_State1C->field_16 != 1) {
                if (work->field_22 & 0x200) {
                    Gp_SpawnEff(0x600B4, coord, lastView, NULL);
                    Gp_SpawnEff(0x600B4, coord, lastView, NULL);
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 1) == 0) {
                        Gp_SpawnEff(0x600B4, coord, lastView, NULL);
                    }
                }
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((u16)(((u32)Gp_LcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(0x600B4, coord, lastView, NULL);
                }
            }
            break;
        case 2:
            lastView = work->field_24;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(0x600B5, coord, view, NULL);
                }
            } else if (Gp_State1C->field_16 != 1) {
                Gp_SpawnEff(0x600B5, coord, lastView, NULL);
                Gp_SpawnEff(0x600B5, coord, lastView, NULL);
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((u16)(((u32)Gp_LcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(0x600B5, coord, lastView, NULL);
                }
            }
            break;
        case 3:
            lastView = work->field_24;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(0x600B6, coord, view, NULL);
                }
            } else if (Gp_State1C->field_16 != 1) {
                Gp_SpawnEff(0x600B6, coord, lastView, NULL);
                Gp_SpawnEff(0x600B6, coord, lastView, NULL);
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((u16)(((u32)Gp_LcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(0x600B6, coord, lastView, NULL);
                }
            }
            break;
        case 5:
            lastView = work->field_24;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(0x600B7, coord, view, NULL);
                }
            } else if (Gp_State1C->field_16 != 1) {
                Gp_SpawnEff(0x600B7, coord, lastView, NULL);
                Gp_SpawnEff(0x600B7, coord, lastView, NULL);
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((u16)(((u32)Gp_LcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(0x600B7, coord, lastView, NULL);
                }
            }
            break;
        case 6:
            lastView = work->field_24;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(0x600B8, coord, view, NULL);
                }
            } else if (Gp_State1C->field_16 != 1) {
                Gp_SpawnEff(0x600B8, coord, lastView, NULL);
                Gp_SpawnEff(0x600B8, coord, lastView, NULL);
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if ((u16)(((u32)Gp_LcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(0x600B8, coord, lastView, NULL);
                }
            }
            break;
    }

    work->field_24 = view;
}

/// The wide variant of the bridge's falling dust streak: same one-pixel `DR_MOVE`
/// smear as `func_acropolis_bridge_80180FF0`, rolled over the whole drop height
/// instead of the upper band. The first frame rolls the streak out of
/// `Gp_LcgState`: `field_10.vy` is the row it starts on (0x60..0xEF),
/// `field_24` the lifetime in frames, `field_26` the width and `field_28` the
/// number of frames each row of fall takes. The column window widens with the
/// starting row - it runs from `0x40 - (vy - 0x60) / 3` to `0xD0 + spread`,
/// where `spread` is half the drop from 0x60 capped at 0x20 - so streaks that
/// begin higher up stay nearer the middle of the screen.
/// `Display_State.field_1f` picks the buffer half, and the OT slot is the row
/// scaled into the 0x800-deep range so a streak sorts against the room behind
/// it. The task releases itself once the camera turns away, the lifetime runs
/// out, or the streak falls off the bottom of the screen.
void func_acropolis_bridge_80180320(Task* task)
{
    RoomEffWork* work;
    RECT         rect;
    DR_MOVE*     mv;
    u16          rnd;
    s32          rndx;
    s32          range;
    s32          bufferY;
    s32          x;
    s32          y;
    s32          depth;

    work    = task->spawnArg2;
    bufferY = Display_State.field_1f * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if ((s16)work->field_22 == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rnd               = (u32)Gp_LcgState >> 16;
            work->field_10.vy = (u32)rnd % 144 + 0x60;
            /* x and y double as the drift and spread of the column window here */
            x                 = (work->field_10.vy - 0x60) / 3;
            y                 = work->field_10.vy < 0xA0 ? (work->field_10.vy - 0x60) / 2 : 0x20;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rndx              = (u32)Gp_LcgState >> 16;
            range             = y + 0x90;
            work->field_10.vx = rndx % (x + range) + (0x40 - x);
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rnd               = (u32)Gp_LcgState >> 16;
            work->field_24    = (u32)rnd % 90 + 0x1E;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_26    = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->field_10.vy + (s16)work->field_22 / (s16)work->field_28;
        x     = work->field_10.vx;
        depth = 0x840 - (y - 0x60) * 8;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->field_26;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(Gpu_CurrentOt + (depth >> 4), mv);
        }
        work->field_22++;
        if ((s16)work->field_22 <= (s16)work->field_24 && y < 0xEF) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// The mid variant of the bridge's falling dust streak: the same one-pixel
/// `DR_MOVE` smear as `func_acropolis_bridge_80180FF0`, rolled over the whole
/// drop height and sorted by a squared depth ramp like
/// `func_acropolis_bridge_80180CC0`, but nearer the camera. The first frame
/// rolls the streak out of `Gp_LcgState`: `field_10.vy` is the row it starts on
/// (0x48..0xEF), `field_24` the lifetime in frames, `field_26` the width and
/// `field_28` the number of frames each row of fall takes. The column window
/// widens with the starting row - it runs from `0x58 - drift` to
/// `0xA0 + spread`, where `drift` is the whole drop from 0x48 capped at 0x58
/// and `spread` five thirds of it capped at 0x50 - so streaks that begin higher
/// up stay nearer the middle of the screen. `Display_State.field_1f` picks the
/// buffer half, and the OT slot grows with the *square* of the distance left to
/// fall, so a streak near the bottom of the screen sorts sharply in front of
/// one still high up. The task releases itself once the camera turns away, the
/// lifetime runs out, or the streak falls off the bottom of the screen.
void func_acropolis_bridge_8018063C(Task* task)
{
    RoomEffWork* work;
    RECT         rect;
    DR_MOVE*     mv;
    u16          rnd;
    s32          rndx;
    s32          col;
    s32          range;
    s32          bufferY;
    s32          x;
    s32          y;
    s32          depth;

    work    = task->spawnArg2;
    bufferY = Display_State.field_1f * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if ((s16)work->field_22 == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rnd               = (u32)Gp_LcgState >> 16;
            work->field_10.vy = (u32)rnd % 168 + 0x48;
            /* x and y double as the drift and spread of the column window here */
            x                 = work->field_10.vy < 0xA0 ? work->field_10.vy - 0x48 : 0x58;
            y                 = work->field_10.vy < 0x78 ? (work->field_10.vy - 0x48) * 5 / 3 : 0x50;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rndx              = (u32)Gp_LcgState >> 16;
            range             = y + 0x48;
            col               = rndx % (x + range) + 0x58;
            col              -= x;
            work->field_10.vx = col;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rnd               = (u32)Gp_LcgState >> 16;
            work->field_24    = (u32)rnd % 90 + 0x1E;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_26    = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->field_10.vy + (s16)work->field_22 / (s16)work->field_28;
        x     = work->field_10.vx;
        depth = (0xF0 - y) * (0xF0 - y) / 15 + 0x2C0;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->field_26;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(Gpu_CurrentOt + (depth >> 4), mv);
        }
        work->field_22++;
        if ((s16)work->field_22 <= (s16)work->field_24 && y < 0xEF) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// The narrow variant of the bridge's falling dust streak: the same one-pixel
/// `DR_MOVE` smear as `func_acropolis_bridge_80180FF0`, but rolled over the
/// lower part of the drop and sorted nearer the camera. The first frame rolls
/// the streak out of `Gp_LcgState`: `field_10.vy` is the row it starts on
/// (0x68..0xEF), `field_24` the lifetime in frames, `field_26` the width and
/// `field_28` the number of frames each row of fall takes. The column window
/// widens with the starting row - it runs from `0x20 - drift` to
/// `0x60 + spread`, where `drift` is twice and `spread` nine times the drop
/// from 0x68, both capped once the streak starts at 0x78 or below - so streaks
/// that begin higher up stay nearer the middle of the screen.
/// `Display_State.field_1f` picks the buffer half, and the OT slot is the row
/// scaled into the 0x600-deep range so a streak sorts against the room behind
/// it. The task releases itself once the camera turns away, the lifetime runs
/// out, or the streak falls off the bottom of the screen.
void func_acropolis_bridge_8018099C(Task* task)
{
    RoomEffWork* work;
    RECT         rect;
    DR_MOVE*     mv;
    u16          rnd;
    s32          rndx;
    s32          col;
    s32          range;
    s32          bufferY;
    s32          x;
    s32          y;
    s32          depth;

    work    = task->spawnArg2;
    bufferY = Display_State.field_1f * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if ((s16)work->field_22 == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rnd               = (u32)Gp_LcgState >> 16;
            work->field_10.vy = (u32)rnd % 136 + 0x68;
            /* x and y double as the drift and spread of the column window here */
            x                 = work->field_10.vy < 0x78 ? (work->field_10.vy - 0x68) * 2 : 0x20;
            y                 = work->field_10.vy < 0x78 ? (work->field_10.vy - 0x68) * 9 : 0x90;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rndx              = (u32)Gp_LcgState >> 16;
            range             = y + 0x40;
            col               = rndx % (x + range) + 0x20;
            col              -= x;
            work->field_10.vx = col;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rnd               = (u32)Gp_LcgState >> 16;
            work->field_24    = (u32)rnd % 90 + 0x1E;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_26    = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->field_10.vy + (s16)work->field_22 / (s16)work->field_28;
        x     = work->field_10.vx;
        depth = 0x600 - (y - 0x68) * 8;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->field_26;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(Gpu_CurrentOt + (depth >> 4), mv);
        }
        work->field_22++;
        if ((s16)work->field_22 <= (s16)work->field_24 && y < 0xEF) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// The tallest variant of the bridge's falling dust streak: the same one-pixel
/// `DR_MOVE` smear as `func_acropolis_bridge_80180FF0`, but rolled over the
/// whole screen height and sorted by a squared depth ramp. The first frame
/// rolls the streak out of `Gp_LcgState`: `field_10.vy` is the row it starts on
/// (0x48..0xEF), `field_24` the lifetime in frames, `field_26` the width and
/// `field_28` the number of frames each row of fall takes. The column window
/// widens with the starting row - it runs from `0x58 - drift` to
/// `0xA0 + spread`, where `drift` is a third and `spread` a half of the drop
/// from 0x48 - so streaks that begin higher up stay nearer the middle of the
/// screen. `Display_State.field_1f` picks the buffer half, and the OT slot
/// grows with the *square* of the distance left to fall, so a streak near the
/// bottom of the screen sorts sharply in front of one still high up. The task
/// releases itself once the camera turns away, the lifetime runs out, or the
/// streak falls off the bottom of the screen.
void func_acropolis_bridge_80180CC0(Task* task)
{
    RoomEffWork* work;
    RECT         rect;
    DR_MOVE*     mv;
    u16          rnd;
    s32          rndx;
    s32          col;
    s32          range;
    s32          bufferY;
    s32          x;
    s32          y;
    s32          depth;

    work    = task->spawnArg2;
    bufferY = Display_State.field_1f * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if ((s16)work->field_22 == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rnd               = (u32)Gp_LcgState >> 16;
            work->field_10.vy = (u32)rnd % 168 + 0x48;
            /* x and y double as the drift and spread of the column window here */
            x                 = (work->field_10.vy - 0x48) / 3;
            y                 = (work->field_10.vy - 0x48) / 2;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rndx              = (u32)Gp_LcgState >> 16;
            range             = y + 0x48;
            col               = rndx % (x + range) + 0x58;
            col              -= x;
            work->field_10.vx = col;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rnd               = (u32)Gp_LcgState >> 16;
            work->field_24    = (u32)rnd % 90 + 0x1E;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_26    = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->field_10.vy + (s16)work->field_22 / (s16)work->field_28;
        x     = work->field_10.vx;
        depth = (0xF0 - y) * (0xF0 - y) / 15 + 0x400;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->field_26;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(Gpu_CurrentOt + (depth >> 4), mv);
        }
        work->field_22++;
        if ((s16)work->field_22 <= (s16)work->field_24 && y < 0xEF) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// One falling dust streak on the bridge, drawn as a `DR_MOVE` that smears a
/// one-pixel-tall strip of the frame buffer down by a pixel. The first frame
/// rolls the whole streak out of `Gp_LcgState`: `field_10.vy` is the row it
/// starts on (0x68..0xE7), `field_10.vx` the column, `field_24` the lifetime in
/// frames, `field_26` the width and `field_28` the number of frames each row of
/// fall takes. The column is drawn from a range that widens with the starting
/// row - `(vy - 0x58) * 6`, capped at the full 240-pixel width once the streak
/// starts at 0x80 or below the horizon - so streaks that begin higher up stay
/// nearer the middle of the screen. `Display_State.field_1f` picks the buffer
/// half, and the OT slot is the row scaled into the 0x800-deep range so a
/// streak sorts against the room behind it. The task releases itself once the
/// camera turns away, the lifetime runs out, or the streak falls off the bottom
/// of the screen.
void func_acropolis_bridge_80180FF0(Task* task)
{
    RoomEffWork* work;
    RECT         rect;
    DR_MOVE*     mv;
    u16          rnd;
    s32          rndx;
    s32          bufferY;
    s32          x;
    s32          y;
    s32          depth;

    work    = task->spawnArg2;
    bufferY = Display_State.field_1f * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if ((s16)work->field_22 == 0) {
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_10.vy = (((u32)Gp_LcgState >> 16) & 0x7F) + 0x68;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rndx              = (u32)Gp_LcgState >> 16;
            work->field_10.vx = work->field_10.vy < 0x80 ? rndx % ((work->field_10.vy - 0x58) * 6) : rndx % 240;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            rnd               = (u32)Gp_LcgState >> 16;
            work->field_24    = (u32)rnd % 90 + 0x1E;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_26    = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            work->field_28    = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->field_10.vy + (s16)work->field_22 / (s16)work->field_28;
        x     = work->field_10.vx;
        depth = 0x800 - (y - 0x68) * 8;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->field_26;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(Gpu_CurrentOt + (depth >> 4), mv);
        }
        work->field_22++;
        if ((s16)work->field_22 <= (s16)work->field_24 && y < 0xEF) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// One frame of the bridge's twinkling dust spark: the task coordinate's
/// translation is projected through `GsWSMATRIX` with a single `RTPS` into an
/// `AcropolisBridgeTwinkleScratch` block taken from `G_SCRATCH_HEAD`, and two
/// `POLY_FT4`s are linked into the OT at that depth. The first is an upright
/// 0x1680 / otz square whose 0x10-wide texture cell is picked by
/// `work->field_22 % 6`, drawn with texture blending off (`code |= 3`). The
/// second is the same point drawn as a spinning semi-transparent grey quad:
/// its two half-diagonals are `(0x3A80 / otz) * rsin` / `rcos` of
/// `work->field_24`, which advances with `Display_State.field_8`, and its tint
/// is a fresh random grey (0x20..0x7F) every frame. Depths under 0x11 drop
/// both quads. The task releases its work block each tick, so the spark lasts
/// one frame.
void func_acropolis_bridge_801812F4(Task* task)
{
    GsCOORDINATE2*                 coord;
    RoomEffWork*                   work;
    void**                         scratch;
    u8*                            head;
    AcropolisBridgeTwinkleScratch* blk;
    s32*                           otzp;
    POLY_FT4*                      prim;
    s32                            grey;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = task->spawnArg2;
    Gp_UpdateCoord(coord);
    work->field_22 = task->spawnArg1;
    scratch        = (void**)G_SCRATCH_HEAD;
    head           = *scratch;
    blk            = (AcropolisBridgeTwinkleScratch*)(head - 0x18);
    otzp           = &blk->otz;
    blk->pos.vx    = coord->workm.t[0];
    blk->pos.vy    = coord->workm.t[1];
    *scratch       = blk;
    blk->pos.vz    = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->pos);
    gte_rtps_real();
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&blk->sxy);
    gte_stszotz(otzp);
    if (blk->otz >= 0x11) {
        prim->tpage = 0x2B;
        prim->clut  = 0x4380;
        prim->code |= 3;
        prim->u0    = ((s16)work->field_22 % 6) * 16;
        prim->v0    = 0;
        prim->u1    = ((s16)work->field_22 % 6) * 16 + 0xF;
        prim->v1    = 0;
        prim->u2    = ((s16)work->field_22 % 6) * 16;
        prim->v2    = 0xF;
        prim->u3    = ((s16)work->field_22 % 6) * 16 + 0xF;
        prim->v3    = 0xF;
        blk->dx     = 0x1680 / blk->otz;
        prim->x0 = prim->x2 = blk->sxy.vx - blk->dx;
        prim->x1 = prim->x3 = blk->sxy.vx + blk->dx;
        prim->y0 = prim->y1 = blk->sxy.vy - blk->dx;
        prim->y2 = prim->y3 = blk->sxy.vy + blk->dx;
        addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);

        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->clut  = 0x4381;
        prim->tpage = 0x2B;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        grey        = ((u32)Gp_LcgState >> 16) % 96 + 0x20;
        prim->u0    = 0;
        prim->v0    = 0x10;
        prim->u1    = 0x27;
        prim->v1    = 0x10;
        prim->u2    = 0;
        prim->v2    = 0x37;
        prim->u3    = 0x27;
        prim->v3    = 0x37;
        prim->code |= 2;
        prim->r0    = grey;
        prim->g0    = grey;
        prim->b0    = grey;

        work->field_24 = Display_State.field_8 + work->field_22;
        blk->dx        = ((0x3A80 / blk->otz) * rsin((s16)work->field_24)) >> 12;
        blk->dy        = ((0x3A80 / blk->otz) * rcos((s16)work->field_24)) >> 12;
        prim->x0       = blk->sxy.vx + blk->dx;
        prim->x3       = blk->sxy.vx - blk->dx;
        prim->y0       = blk->sxy.vy - blk->dy;
        prim->y3       = blk->sxy.vy + blk->dy;
        blk->dx        = ((0x3A80 / blk->otz) * rsin((s16)work->field_24 + 0x400)) >> 12;
        blk->dy        = ((0x3A80 / blk->otz) * rcos((s16)work->field_24 + 0x400)) >> 12;
        prim->x1       = blk->sxy.vx + blk->dx;
        prim->x2       = blk->sxy.vx - blk->dx;
        prim->y1       = blk->sxy.vy - blk->dy;
        prim->y2       = blk->sxy.vy + blk->dy;
        addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
    Gp_ReleaseState1CMem(work, task);
}

/// The bridge's dust cloud: one semi-transparent `POLY_FT4` billboard placed at
/// the task's world position. The four corners are taken from the unit quad in
/// `D_acropolis_bridge_8018990C`, scaled by 0x300 and rotated by the
/// coordinate's `workm` - and then each rotated corner is overwritten with that
/// same `workm` translation, so all four collapse onto the object origin. The
/// quad is projected with one `RTPS` plus one `RTPT` directly into the
/// primitive, tinted a random grey, and linked into the OT at the `RTPS` depth
/// biased by 0x20; depths under 0x11 are dropped rather than drawn. The task
/// releases its work block on every tick, so the puff lasts one frame.
void func_acropolis_bridge_801819C8(Task* task)
{
    void**                      scratch;
    u8*                         head;
    AcropolisBridgeQuadScratch* block;
    AcropolisBridgeQuadCorner*  tbl;
    POLY_FT4*                   prim;
    GsCOORDINATE2*              coord;
    RoomEffWork*                work;
    MATRIX*                     m;
    SVECTOR*                    v;
    s32                         i;
    u8                          col;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = task->spawnArg2;
    Gp_UpdateCoord(coord);

    scratch        = (void**)G_SCRATCH_HEAD;
    i              = 0;
    m              = &coord->workm;
    tbl            = D_acropolis_bridge_8018990C;
    head           = (u8*)*scratch - sizeof(AcropolisBridgeQuadScratch);
    work->field_22 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0;
    *scratch       = head;
    block          = (AcropolisBridgeQuadScratch*)*scratch;
    do {
        v                = ((AcropolisBridgeQuadScratch*)((SVECTOR*)block + i))->vec;
        block->vec[i].vx = tbl[i].x * 0x300;
        v->vy            = 0;
        v->vz            = tbl[i].y * 0x300;
        gte_SetRotMatrix(m);
        gte_ldv0(&block->vec[i]);
        gte_rtv0_real();
        gte_stsv(&block->vec[i]);
        *(u16*)&block->vec[i].vx = *(u16*)&coord->workm.t[0];
        i++;
        *(u16*)&v->vy = *(u16*)&coord->workm.t[1];
        *(u16*)&v->vz = *(u16*)&coord->workm.t[2];
    } while (i < 4);

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps_real();
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt_real();
    setUV4(prim, 0, 0x10, 0x27, 0x10, 0, 0x37, 0x27, 0x37);
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&block->otz);
    block->otz += 0x20;
    if (block->otz >= 0x11) {
        prim->tpage = 0x2B;
        prim->clut  = 0x4381;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        col         = ((u32)Gp_LcgState >> 16) & 0xF;
        setRGB0(prim, col, col, col);
        setSemiTrans(prim, 1);
        addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(AcropolisBridgeQuadScratch);
    Gp_ReleaseState1CMem(work, task);
}
