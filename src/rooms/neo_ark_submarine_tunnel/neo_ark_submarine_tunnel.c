#include "rooms/neo_ark_submarine_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "gte.h"
#include "types.h"

#include "neo_ark_submarine_tunnel_private.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

/// The area-record id the event handler publishes, and the cutscene script
/// blobs `func_800E8634` / `func_800E8614` are handed as `(s32)&blob`.
extern s32 D_80135220;
extern s32 D_80135FD0;
extern s32 D_80136108;

/// Spawn table of the screen-wave task, and the context it is spawned with.
/// The context's mode word is written through its own symbol, which is how the
/// original reached it.
extern TaskDesc D_neo_ark_submarine_tunnel_80181A34[];

/// Current displacement of the screen wave, recomputed every frame from the
/// context's ramp.
extern s32 D_neo_ark_submarine_tunnel_80181A4C;

/// Message handlers this room's task answers, installed into pointer slot 7.
extern GpMsgEntry D_neo_ark_submarine_tunnel_80181A50[];

/// The tunnel's own script blob and the byte recording which of its scenes has
/// already been staged.
extern GpEvsCmd D_neo_ark_submarine_tunnel_80181AF0[];

static void func_neo_ark_submarine_tunnel_8017F3BC(Task* arg0);
static void func_neo_ark_submarine_tunnel_8017F414(Task* task);

s32 func_neo_ark_submarine_tunnel_8017F064(Task*, s32, RoomEventMsg*, GpMessageArg);
s32 func_neo_ark_submarine_tunnel_8017F27C(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_submarine_tunnel_8017F284(Task*, s32, GpSaveLoc*, GpSaveLoc*);
s32 func_neo_ark_submarine_tunnel_8017F2C8(Task*, s32, s32, s32);

void func_neo_ark_submarine_tunnel_8017E828(Task*);

extern AnimationSet D_neo_ark_submarine_tunnel_801814A0;
extern AnimationSet D_neo_ark_submarine_tunnel_80181A0C;

void func_neo_ark_submarine_tunnel_8017D634(Task*);
void func_neo_ark_submarine_tunnel_8017E288(Task*);

extern AnimationPlayRequest D_neo_ark_submarine_tunnel_80181A88;
extern AnimationPlayRequest D_neo_ark_submarine_tunnel_80181A9C;
extern GpCopyArg            D_neo_ark_submarine_tunnel_80181A80;
void                        func_neo_ark_submarine_tunnel_8017F318(s32);
void                        func_neo_ark_submarine_tunnel_8017F398(s32);

TaskDesc D_neo_ark_submarine_tunnel_801810E4 = { 0, 192, func_neo_ark_submarine_tunnel_8017D634, { .model = NULL } };

TaskDesc D_neo_ark_submarine_tunnel_801810F0 = { 0, 192, func_neo_ark_submarine_tunnel_8017E288, { .model = NULL } };

AnimationPackedPose D_neo_ark_submarine_tunnel_801810FC[6] = {
#include "assets/neo_ark_submarine_tunnel_animation_03EE0_bank1.inc"
};

AnimationPackedRotation D_neo_ark_submarine_tunnel_80181144[64] = {
#include "assets/neo_ark_submarine_tunnel_animation_03EE0_bank4.inc"
};

AnimationRecord D_neo_ark_submarine_tunnel_80181244[141] = {
#include "assets/neo_ark_submarine_tunnel_animation_03EE0_records.inc"
};

u16 D_neo_ark_submarine_tunnel_80181478[20] = {
#include "assets/neo_ark_submarine_tunnel_animation_03EE0_indices.inc"
};

AnimationSet D_neo_ark_submarine_tunnel_801814A0 = {
    D_neo_ark_submarine_tunnel_80181244,
    D_neo_ark_submarine_tunnel_80181478,
    { NULL, D_neo_ark_submarine_tunnel_801810FC, NULL, NULL, D_neo_ark_submarine_tunnel_80181144, NULL, NULL, NULL },
};

AnimationPackedPose D_neo_ark_submarine_tunnel_801814C8[10] = {
#include "assets/neo_ark_submarine_tunnel_animation_0444C_bank1.inc"
};

AnimationPackedRotation D_neo_ark_submarine_tunnel_80181540[126] = {
#include "assets/neo_ark_submarine_tunnel_animation_0444C_bank4.inc"
};

AnimationRecord D_neo_ark_submarine_tunnel_80181738[171] = {
#include "assets/neo_ark_submarine_tunnel_animation_0444C_records.inc"
};

u16 D_neo_ark_submarine_tunnel_801819E4[20] = {
#include "assets/neo_ark_submarine_tunnel_animation_0444C_indices.inc"
};

AnimationSet D_neo_ark_submarine_tunnel_80181A0C = {
    D_neo_ark_submarine_tunnel_80181738,
    D_neo_ark_submarine_tunnel_801819E4,
    { NULL, D_neo_ark_submarine_tunnel_801814C8, NULL, NULL, D_neo_ark_submarine_tunnel_80181540, NULL, NULL, NULL },
};

TaskDesc D_neo_ark_submarine_tunnel_80181A34[2] = {
    { 0, 192, func_neo_ark_submarine_tunnel_8017E828, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

s32 D_neo_ark_submarine_tunnel_80181A4C = 256;

GpMsgEntry D_neo_ark_submarine_tunnel_80181A50[5] = {
    { 5102, func_neo_ark_submarine_tunnel_8017F284 },
    { 5105, func_neo_ark_submarine_tunnel_8017F27C },
    { 5103, func_neo_ark_submarine_tunnel_8017F064 },
    { 5104, func_neo_ark_submarine_tunnel_8017F2C8 },
    { 0x7FFFFFFF, NULL },
};

AnimationSet* D_neo_ark_submarine_tunnel_80181A78[2] = {
    &D_neo_ark_submarine_tunnel_80181A0C,
    &D_neo_ark_submarine_tunnel_801814A0,
};

GpCopyArg D_neo_ark_submarine_tunnel_80181A80 = { { .sets = D_neo_ark_submarine_tunnel_80181A78 }, 2 };

AnimationPlayRequest D_neo_ark_submarine_tunnel_80181A88 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_neo_ark_submarine_tunnel_80181A9C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

GpCmdArg D_neo_ark_submarine_tunnel_80181AB0 = { { .loc = { 5, 12 } }, 0 };

GpCmdArg D_neo_ark_submarine_tunnel_80181AB4 = { { .loc = { 5, 12 } }, 1 };

GpCmdArg D_neo_ark_submarine_tunnel_80181AB8 = { { .loc = { 5, 12 } }, 2 };

AnimationPlayRequest D_neo_ark_submarine_tunnel_80181ABC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpXformArg D_neo_ark_submarine_tunnel_80181AD0 = { { 4544, 3001, 0, 0 }, { 0, -1024, 0, 0 } };

GpOverlayIds D_neo_ark_submarine_tunnel_80181AE8 = { 5, 60, 11 };

GpEvsCmd D_neo_ark_submarine_tunnel_80181AF0[32] = {
    { 12, { .overlays = &D_neo_ark_submarine_tunnel_80181AE8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 31, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_neo_ark_submarine_tunnel_80181A80 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_submarine_tunnel_80181ABC }, { .value = 0 } },
    { 41, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_neo_ark_submarine_tunnel_80181AD0 }, { .value = 0 } },
    { 13, { .callback = func_neo_ark_submarine_tunnel_8017F318 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_submarine_tunnel_80181A9C }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 30, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_neo_ark_submarine_tunnel_80181AB4 }, { .value = 2011 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1021 }, { .value = 8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_submarine_tunnel_80181A88 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 34, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_neo_ark_submarine_tunnel_80181AB8 }, { .value = 2011 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_submarine_tunnel_80181ABC }, { .value = 0 } },
    { 13, { .callback = func_neo_ark_submarine_tunnel_8017F318 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_neo_ark_submarine_tunnel_8017F398 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

/// Draws a water-refraction ripple for some views of areas 27, 14, 15, 13, 30
/// and 29 and returns at once for every other view. The view sets the row
/// range, a split row and the x at which each row's strip is cut in two, a
/// clip mode, a wave scale and an ordering-table offset. Each row's vector is
/// rotated through the transposed view matrix to find its ordering-table
/// depth, and one or two `POLY_FT4` strips per row sample the other display
/// buffer shifted vertically by a `rsin` / `rcos` wave that fades in over the
/// first 8 rows of the range and of the split. The wave phases derive from
/// `Task::killCountdown`, seeded from `rand()` on the first call and advanced
/// by 0x20 per call while `Gp_StateF0.field_4` is clear; views 6 and 7 of area 13
/// advance them by 0x20 while it is set instead.
///
/// Matching note: `wave = w` is written in both arms of the scale test and the
/// pass-1 fade starts from `v = 0x79`. jump2 merges the two copies and deletes
/// the constant set, but both change register allocation and scheduling the
/// way the retail code needs.
void func_neo_ark_submarine_tunnel_8017D634(Task* task)
{
    s32                   buf;
    s32                   sinArg;
    s32                   cosArg;
    s32                   kind;
    s32                   scale;
    s32                   zoff;
    s32                   split;
    s32                   splitX;
    s32                   otzOff;
    s32                   xLeft0;
    s32                   xRight0;
    s32                   xLeftS;
    s32                   start;
    s32                   end;
    s32                   area;
    POLY_FT4*             prim;
    OverlayRippleScratch* scratch;
    s32                   y;
    s32                   y0;
    s32                   xl;
    s32                   xr;
    s32                   passes;
    s32                   pass;
    s32                   wave;
    s32                   sinv;
    s32                   cosv;
    s32                   w;
    s32                   d;
    s32                   z;
    s32                   otz;
    s32                   v;
    s32                   dy;
    s32                   xv;
    s32                   x;
    s32                   xe;
    s32                   xMin;
    s32                   xMax;
    s32                   fadeLen;
    s32                   one;
    DisplayState*         disp;

    kind    = 0;
    scale   = 0x1000;
    zoff    = 0x21C;
    otzOff  = 0;
    xLeft0  = -0xA0;
    xRight0 = 0xA0;
    xLeftS  = -0xA0;
    split   = 0;
    splitX  = 0;
    buf     = gDisplayState.otBuffer;
    area    = gGameSession->at4.loc.area;
    if (area == 27) {
        otzOff = 10;
        switch (gGameSession->at4.loc.view) {
            case 2:
                start  = 0x7F;
                end    = 0xF0;
                split  = 0x9F;
                splitX = 0x23;
                break;
            case 3:
                start  = 0x4A;
                end    = 0xF0;
                split  = 0x68;
                splitX = 0x55;
                break;
            case 4:
                start  = 1;
                end    = 0xF0;
                split  = 0x74;
                splitX = -0xD3;
                break;
            case 5:
                start  = 0x66;
                end    = 0xF0;
                split  = 0x6B;
                splitX = 0xC2;
                break;
            case 6:
                start  = 0x93;
                end    = 0xF0;
                split  = 0xA1;
                splitX = 0xBC;
                break;
            default:
                return;
        }
    } else if (area == 14) {
        switch (gGameSession->at4.loc.view) {
            case 2:
                start  = 0x77;
                end    = 0xF0;
                split  = 0xA1;
                splitX = -0x2C;
                break;
            case 3:
                start  = 0x4C;
                end    = 0xF0;
                split  = 0x68;
                splitX = -0x4E;
                break;
            case 4:
                start  = 1;
                end    = 0xF0;
                split  = 0x3E8;
                kind   = 3;
                otzOff = 10;
                scale  = 0x800;
                break;
            default:
                return;
        }
    } else if (area == 15) {
        if (gGameSession->at4.loc.view == 2) {
            split  = 0x3E8;
            start  = 0x84;
            end    = 0xF0;
            splitX = 0x4B;
            scale  = 0x800;
        } else {
            return;
        }
    } else if (area == 13) {
        otzOff = 10;
        switch (gGameSession->at4.loc.view) {
            case 2:
            case 4:
                start = 0x52;
                end   = 0xF0;
                kind  = 1;
                split = 0x3E8;
                scale = 0x800;
                break;
            case 3:
            case 5:
                start = 0x4C;
                end   = 0xF0;
                kind  = 2;
                split = 0x3E8;
                scale = 0x800;
                break;
            case 6:
                start = 0x63;
                end   = 0xF0;
                split = 0;
                scale = 0x800;
                if (Gp_StateF0.field_4 != 0) {
                    task->killCountdown += 0x20;
                }
                break;
            case 7:
                start = 0x35;
                end   = 0xF0;
                kind  = 4;
                scale = 0x800;
                if (Gp_StateF0.field_4 != 0) {
                    task->killCountdown += 0x20;
                }
                break;
            default:
                return;
        }
    } else if (area == 30) {
        scale  = 0x800;
        otzOff = -10;
        zoff   = 0x131A;
        switch (gGameSession->at4.loc.view) {
            case 2:
                xLeft0 = 0x3B;
                start  = 0xA9;
                end    = 0xE0;
                split  = -0xC7;
                splitX = -0x43;
                break;
            case 3:
                otzOff  = 10;
                xLeft0  = -0x4F;
                xRight0 = 0x4F;
                xLeftS  = -0x3A;
                start   = 1;
                end     = 0x5E;
                split   = -0x49;
                splitX  = 0x78;
                break;
            case 4:
                xRight0 = -0x3B;
                start   = 0xA9;
                end     = 0xE0;
                split   = -0xC7;
                splitX  = 0x43;
                break;
            case 5:
                start  = 0xA1;
                end    = 0xF0;
                xLeftS = -0x8A;
                splitX = 0x114;
                split  = 0xAC;
                otzOff = 0;
                break;
            default:
                return;
        }
    } else if (area == 29) {
        zoff = 0x8C;
        switch (gGameSession->at4.loc.view) {
            case 6:
                start = 0xA5;
                end   = 0xF0;
                split = 0;
                break;
            case 7:
                start = 0xA4;
                end   = 0xF0;
                split = 0;
                break;
            default:
                return;
        }
    } else {
        return;
    }

    xMin    = -0xA0;
    xMax    = 0xA0;
    fadeLen = 8;
    one     = 1;
    if (task->state == 0) {
        gGameSession->field_80 = 0;
        task->killCountdown    = rand();
        task->state++;
    }
    prim = (POLY_FT4*)Fs_ActorLoadBase2;
    disp = &gDisplayState;
    if (disp->otBuffer != 0) {
        prim += 488;
    }
    prim--;
    if (Gp_StateF0.field_4 == 0) {
        task->killCountdown += 0x20;
    }
    sinArg = task->killCountdown * 2;
    cosArg = task->killCountdown;
    SCRATCH_PUSH(OverlayRippleScratch);
    scratch = SCRATCH_HEAD(OverlayRippleScratch);
    TransposeMatrix(&gGfxViewCoord.workm, &scratch->mtx);
    scratch->origin.vx = gGfxViewCoord.workm.t[0];
    scratch->origin.vy = gGfxViewCoord.workm.t[1];
    scratch->origin.vz = gGfxViewCoord.workm.t[2];
    gfxRotateSv(&scratch->mtx, &scratch->origin);
    scratch->depth  = scratch->origin.vy + zoff;
    scratch->depth *= disp->screenDistance;
    scratch->row.vx = 0;
    scratch->row.vz = disp->screenDistance;
    gte_SetRotMatrix(&scratch->mtx);

    for (y = start; y < end; y++) {
        y0              = y - 0x78;
        scratch->row.vy = y0;
        gte_ldv0(&scratch->row);
        gte_rtv0();
        xl     = xLeft0;
        xr     = xRight0;
        passes = 1;
        if (split > 0) {
            if (y < split + 8) {
                xr = xMax;
                if (splitX > 0) {
                    xl = xLeftS;
                    xr = xl + splitX;
                } else {
                    xl = xr + splitX;
                }
                if (split < y) {
                    passes = 2;
                }
            }
        } else if (split < 0 && -split < y) {
            xr = xMax;
            if (splitX > 0) {
                xl = xLeftS;
                xr = xl + splitX;
            } else {
                xl = xr + splitX;
            }
        }
        sinv  = rsin(sinArg);
        cosv  = rcos(cosArg + 0x134);
        sinv += 0x2000;
        w     = cosv + sinv;
        w   >>= 9;
        if (scale != 0x1000) {
            w    = (w * scale) >> 12;
            wave = w;
        } else {
            wave = w;
        }
        w++;
        if (start != 1) {
            d = y - start;
            if (d < fadeLen) {
                w  = wave >> ((fadeLen - d) >> one);
                w += one;
            }
        }
        gte_stsv(&scratch->rowView);
        if (scratch->rowView.vy > 0) {
            otz   = scratch->depth / scratch->rowView.vy;
            otz >>= 2;
        } else {
            otz = 0x3FFF;
        }
        v    = (y0 + 0x78) + w;
        z    = otz;
        otz  = ((z << gDisplayState.otDepthShift) & 0x3FFF) >> 4;
        otz += otzOff;
        if (v >= 0xEF) {
            v = 0x1DC - v;
        }
        if (kind == 1) {
            if (y < 0x7D) {
                xl = -0xA0;
                xr = 0xA0;
            } else if (y < 0xB3) {
                passes = 2;
                xl     = -0xA0;
                xr     = -0x59;
            } else {
                xl = -0xA0;
                xr = -0x59;
            }
        } else if (kind == 2) {
            if (y < 0x83) {
                xl = -0xA0;
                xr = 0xA0;
            } else if (y < 0xB7) {
                passes = 2;
                xl     = 0x57;
                xr     = 0xA0;
            } else {
                xl = 0x57;
                xr = 0xA0;
            }
        } else if (kind == 3) {
            if (y < 0x43) {
                passes = 1;
                xl     = -0xA0;
                xr     = 0xA0;
            } else {
                passes = 2;
            }
        } else if (kind == 4) {
            passes = 1;
            xr     = 0xA0;
            xl     = -9;
            if (y >= 0x42) {
                xl = -0xA0;
                if (y < 0x4D) {
                    xl = -0x6A;
                }
            }
        }
        for (pass = 0; pass < passes; pass++) {
            dy = y - split;
            if (kind == 1) {
                if (pass != 0) {
                    xl = 0x3C;
                    xr = 0xA0;
                }
            } else if (kind == 2) {
                if (pass == 1) {
                    xl = -0xA0;
                    xr = -0x69;
                }
            } else if (kind == 3) {
                if (pass == 0) {
                    if (y < 0x43) {
                        xl = -0xA0;
                        xr = 0xA0;
                    } else {
                        xl = -0xA0;
                        xr = -0x57;
                    }
                } else {
                    if (y < 0xC1) {
                        xl = 0x5D;
                        xr = 0xA0;
                    } else {
                        xl = 0x2A;
                        xr = 0xA0;
                    }
                }
            } else if (pass == 1) {
                if (dy < fadeLen) {
                    w = wave >> ((fadeLen - dy) >> 1);
                    v = 0x79;
                    v = y0 + (v + w);
                    if (v >= 0xEF) {
                        v = 0x1DC - v;
                    }
                }
                if (splitX > 0) {
                    xv = splitX - 0x140;
                } else {
                    xv = splitX + 0x140;
                }
                xl = xMin;
                if (xv > 0) {
                    xr = xv + xl;
                } else {
                    xr = xMax;
                    xl = xv + xr;
                }
            }
            if (xr > 0) {
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y0 + 1;
                prim->y2    = y0 + 1;
                prim->tpage = getTPage(2, 0, 0x80, buf << 8);
                x           = xl;
                if (xl < 0) {
                    x = 0;
                }
                prim->x2 = x;
                prim->x0 = x;
                prim->u2 = x + 0x20;
                prim->u0 = x + 0x20;
                prim->x3 = xr;
                prim->x1 = xr;
                prim->u3 = xr + 0x20;
                prim->u1 = xr + 0x20;
                prim->v1 = v + buf * 16;
                prim->v0 = v + buf * 16;
                prim->v3 = v + buf * 16 + 1;
                prim->v2 = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
            if (xl <= 0) {
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y0 + 1;
                prim->y2    = y0 + 1;
                prim->tpage = getTPage(2, 0, 0, buf << 8);
                xe          = xr;
                if (xr > 0) {
                    xe = 0;
                }
                prim->u2 = xl - 0x60;
                prim->u0 = xl - 0x60;
                prim->u3 = (xl - 0x60) + (xe - xl);
                prim->u1 = (xl - 0x60) + (xe - xl);
                prim->x2 = xl;
                prim->x0 = xl;
                prim->x3 = xe;
                prim->x1 = xe;
                prim->v1 = v + buf * 16;
                prim->v0 = v + buf * 16;
                prim->v3 = v + buf * 16 + 1;
                prim->v2 = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
        }
        sinArg += 0x1F;
        if (z > 0x300) {
            cosArg += 0xC5 + (z - 0x300) / 4;
        } else {
            cosArg += 0xC5;
        }
    }
    SCRATCH_POP(OverlayRippleScratch);
}

/// Draws a wavy screen-distortion band for some views of areas 12 and 30 and
/// returns at once for every other view. Each screen row between the view's
/// start and end rows gets one or two raw-textured `POLY_FT4` strips that
/// sample the other display buffer shifted vertically by a `rsin` / `rcos`
/// wave, faded out over the band's last 16 rows; the strips are linked into
/// `gGpuCurrentOt` one depth nearer per row. One view of area 30 draws a second
/// band. The wave phases derive from `Task::killCountdown`, seeded from
/// `rand()` on the first call and advanced every call while `Gp_StateF0.field_4` is
/// clear.
///
/// Matching note: `spare` is never assigned, so `spare >> 16` is always zero;
/// it stands in for the stack slot the retail frame carries, which the
/// register allocator needs to see.
void func_neo_ark_submarine_tunnel_8017E288(Task* task)
{
    s32              xLeft  = -0xA0;
    s32              xRight = 0xA0;
    s32              buf    = gDisplayState.otBuffer;
    s32              passes = 1;
    GameLocationKey* loc    = &gGameSession->at4.loc;
    s32              area   = loc->area;
    s32              start;
    s32              end;
    POLY_FT4*        prim;
    u8*              base;
    s32              size;
    s32              sinArg;
    s32              cosArg;
    s32              otz;
    s32              pass;
    s32              y;
    s32              y0;
    s32              wave;
    s32              sinv;
    s32              cosv;
    s32              v;
    s32              x0;
    s32              x1;
    u16              spare;

    if (area == 12) {
        switch (gGameSession->at4.loc.view) {
            case 2:
                start = 1;
                end   = 0x3F;
                break;
            case 3:
                start = 1;
                end   = 0x49;
                break;
            case 4:
                start = 1;
                end   = 0x49;
                break;
            case 5:
                start = 1;
                end   = 0x3F;
                break;
            case 8:
                start = 1;
                end   = 0x72;
                break;
            case 9:
                start = 1;
                end   = 0x45;
                break;
            default:
                return;
        }
    } else if (area == 30) {
        switch (gGameSession->at4.loc.view) {
            case 2:
                start  = 1;
                end    = 0x40;
                xRight = -0x50;
                break;
            case 4:
                start  = 0x3B;
                end    = 0x55;
                xRight = -0x67;
                break;
            case 5:
                start  = 0x22;
                end    = 0x4A;
                xRight = -0x4E;
                passes = 2;
                break;
            default:
                return;
        }
    } else {
        return;
    }

    if (task->state == 0) {
        gGameSession->field_80 = 0;
        task->killCountdown    = rand();
        task->state++;
    }

    if (area == 12 && loc->variant == 3) {
        size  = 0x30000 - Fs_ChunkOutputSizes[0];
        size &= ~7;
        base  = (u8*)Fs_ActorLoadBase0 - (size - 0x30000);
        if (size < sizeof(POLY_FT4) * 976) {
            return;
        }
        if (gDisplayState.otBuffer != 0) {
            base += size >> 1;
        }
        prim = (POLY_FT4*)base - 1;
    } else {
        prim = (POLY_FT4*)((u8*)Fs_ActorLoadBase2 + 0x9880);
        if (gDisplayState.otBuffer != 0) {
            prim += 488;
        }
        prim--;
    }

    if (Gp_StateF0.field_4 == 0) {
        task->killCountdown++;
    }
    sinArg = task->killCountdown << 5;
    cosArg = task->killCountdown << 4;
    SCRATCH_PUSH_BYTES(0x40);
    otz = ((0x3FFF << gDisplayState.otDepthShift) & 0x3FFF) >> 4;

    for (pass = 0; pass < passes; pass++) {
        if (pass == 1) {
            start  = 0x2E;
            end    = 0x54;
            xLeft  = 1;
            xRight = 0x55;
        }
        for (y = start; y < end; y++) {
            y0     = y - 0x78;
            sinv   = rsin(sinArg);
            cosv   = rcos(cosArg + 0x134);
            sinv  += 0x2000;
            wave   = cosv + sinv;
            wave >>= 10;
            if (end - 0x10 < y) {
                wave = (wave * (end - y)) >> 4;
            }
            cosv = wave + 0x79;
            v    = y0 + cosv;
            otz--;
            if (v >= 0xEF) {
                v = 0x1DC - v;
            }
            if (v < 0) {
                v = -v;
            }
            if (xRight > 0) {
                x0 = xLeft < 0 ? 0 : xLeft;
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y - 0x77;
                prim->y2    = y - 0x77;
                prim->tpage = getTPage(2, 0, 0x80, buf << 8);
                prim->x2    = x0;
                prim->x0    = x0;
                prim->u2    = x0 + 0x20;
                prim->u0    = x0 + 0x20;
                prim->x3    = xRight;
                prim->x1    = xRight;
                prim->u3    = xRight + 0x20;
                prim->u1    = xRight + 0x20;
                prim->v1    = v + buf * 16;
                prim->v0    = v + buf * 16;
                prim->v3    = v + buf * 16 + 1;
                prim->v2    = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
            if (xLeft < 0) {
                x1 = xRight;
                if (x1 > 0) {
                    x1 = 0;
                }
                prim++;
                prim->y1    = y0;
                prim->y0    = y0;
                prim->y3    = y - 0x77;
                prim->y2    = y - 0x77;
                prim->tpage = getTPage(2, 0, 0, buf << 8);
                prim->u2    = xLeft - 0x60;
                prim->u0    = xLeft - 0x60;
                prim->u3    = x1 - 0x60;
                prim->u1    = x1 - 0x60;
                prim->x2    = xLeft;
                prim->x0    = xLeft;
                prim->x3    = x1;
                prim->x1    = x1;
                prim->v1    = v + buf * 16;
                prim->v0    = v + buf * 16;
                prim->v3    = v + buf * 16 + 1;
                prim->v2    = v + buf * 16 + 1;
                setlen(prim, 9);
                prim->code = 0x2D;
                addPrim(&gGpuCurrentOt[otz], prim);
            }
            sinArg += 0x1F + (spare >> 16);
            cosArg += 0xC5;
        }
    }
    SCRATCH_POP_BYTES(0x40);
}

/// State handlers of the room task `func_neo_ark_submarine_tunnel_8017F434`
/// runs: `func_neo_ark_submarine_tunnel_8017F3BC` sets it up,
/// `func_neo_ark_submarine_tunnel_8017F414` runs every later tick, and
/// `taskKill` ends it.
static const TaskFuncTable3 D_neo_ark_submarine_tunnel_8017D614 = {
    { func_neo_ark_submarine_tunnel_8017F3BC, func_neo_ark_submarine_tunnel_8017F414, taskKill }
};

/// Task that ripples the whole screen: it redraws the frame just rendered as a
/// 10 by 30 grid of textured quads whose corners are pushed around by sine
/// waves. The first frame gives every column and row edge a random phase
/// offset and speed, takes its context from `spawnArg2` and passes -8 to
/// `displaySetShakeY`. Afterwards the context's mode ramps the strength
/// up to its limit (mode 0), back down to zero and on to mode 2 (mode 1), or
/// ends the task and passes 0 back (mode 2); the displacement is the ramp's
/// share of the context's peak. A non-zero tint flag shades the quads with the
/// context's colour instead of drawing them unlit. The grid is bracketed by
/// draw-mode packets that switch mask-bit setting on at the back of the order
/// table and off again at the front.
void func_neo_ark_submarine_tunnel_8017E828(Task* arg0)
{
    OverlayWaveCtx* ctx;
    POLY_FT4*       p;
    DR_STP*         stp;
    s32             i, j, k;
    s32             drawY;
    s32             tpage0, tpage1;
    s32             u0, u1, v0, v1;
    s32             waveX0, waveY0, waveX1, waveY1;
    s32             waveX2, waveY2, waveX3, waveY3;
    s32*            state;

    CdCmd_Queue.field_22A = 2;
    /* Through a pointer rather than as `arg0->state`: a member load is struct
       memory, which the scheduler lets rise above the store before it, and the
       original keeps the two in source order. */
    state = &arg0->state;
    switch (*state) {
        case 0:
            for (i = 0; i < 11; i++) {
                D_neo_ark_submarine_tunnel_80187910[i].phase  = 0;
                D_neo_ark_submarine_tunnel_80187910[i].offset = (u32)rand() >> 3;
                D_neo_ark_submarine_tunnel_80187910[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                D_neo_ark_submarine_tunnel_80187960[i].phase  = 0;
                D_neo_ark_submarine_tunnel_80187960[i].offset = (u32)rand() >> 3;
                D_neo_ark_submarine_tunnel_80187960[i].speed  = (rand() * 100 + 20) >> 15;
            }
            D_neo_ark_submarine_tunnel_80181A4C        = 0;
            D_neo_ark_submarine_tunnel_8018790C        = arg0->spawnArg2.pointer;
            D_neo_ark_submarine_tunnel_8018790C->frame = 0;
            D_neo_ark_submarine_tunnel_8018790C->state = 0;
            displaySetShakeY(DISPLAY_SHAKE_MIN);
            arg0->state++;
            break;
        case 1:
            ctx = D_neo_ark_submarine_tunnel_8018790C;
            switch (ctx->state) {
                case 0:
                    if (ctx->frame < ctx->span) {
                        ctx->frame++;
                    }
                    break;
                case 1:
                    if (ctx->frame > 0) {
                        ctx->frame--;
                    } else {
                        ctx->state = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    displaySetShakeY(0);
                    break;
            }
            D_neo_ark_submarine_tunnel_80181A4C = D_neo_ark_submarine_tunnel_8018790C->frame * D_neo_ark_submarine_tunnel_8018790C->scale / D_neo_ark_submarine_tunnel_8018790C->span;
            for (i = 0; i < 11; i++) {
                D_neo_ark_submarine_tunnel_80187910[i].phase += D_neo_ark_submarine_tunnel_80187910[i].speed;
            }
            for (i = 0; i < 30; i++) {
                D_neo_ark_submarine_tunnel_80187960[i].phase += D_neo_ark_submarine_tunnel_80187960[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = gGpuPrimCursor;
                    gGpuPrimCursor = p + 1;
                    setPolyFT4(p);
                    if (D_neo_ark_submarine_tunnel_8018790C->blend == ANIMATION_BLEND_RESET) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = D_neo_ark_submarine_tunnel_8018790C->r;
                        p->g0 = D_neo_ark_submarine_tunnel_8018790C->g;
                        p->b0 = D_neo_ark_submarine_tunnel_8018790C->b;
                    }
                    u0 = k * 32;
                    u1 = (k + 1) * 32;
                    if (u1 == 320)
                        u1 = 319;
                    if (u0 < 128) {
                        p->tpage = tpage0;
                    } else {
                        p->tpage = tpage1;
                        u0      -= 128;
                        u1      -= 128;
                    }
                    if (j != -1) {
                        v1     = (j + 1) * 8 + gDisplayState.drawBuffer * 16;
                        v0     = j * 8 + gDisplayState.drawBuffer * 16;
                        waveX0 = D_neo_ark_submarine_tunnel_80181A4C * (rsin((j << 9) + D_neo_ark_submarine_tunnel_80187910[k].phase + D_neo_ark_submarine_tunnel_80187910[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_neo_ark_submarine_tunnel_80181A4C * (rsin((k << 10) + D_neo_ark_submarine_tunnel_80187960[j].phase + D_neo_ark_submarine_tunnel_80187960[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_neo_ark_submarine_tunnel_80181A4C * (rsin((j << 9) + D_neo_ark_submarine_tunnel_80187910[k + 1].phase + D_neo_ark_submarine_tunnel_80187910[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_neo_ark_submarine_tunnel_80181A4C * (rsin(((k + 1) << 10) + D_neo_ark_submarine_tunnel_80187960[j].phase + D_neo_ark_submarine_tunnel_80187960[j].offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        drawY = gDisplayState.drawBuffer * 16;
                        p->x0 = k * 32 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 32 - 160;
                        p->y1 = -112;
                        v0    = drawY + 8;
                        v1    = drawY;
                    }
                    {

                        waveX2 = D_neo_ark_submarine_tunnel_80181A4C * (rsin(((j + 1) << 9) + D_neo_ark_submarine_tunnel_80187910[k].phase + D_neo_ark_submarine_tunnel_80187910[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = D_neo_ark_submarine_tunnel_80181A4C * (rsin((k << 10) + D_neo_ark_submarine_tunnel_80187960[j + 1].phase + D_neo_ark_submarine_tunnel_80187960[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = D_neo_ark_submarine_tunnel_80181A4C * (rsin(((j + 1) << 9) + D_neo_ark_submarine_tunnel_80187910[k + 1].phase + D_neo_ark_submarine_tunnel_80187910[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = D_neo_ark_submarine_tunnel_80181A4C * (rsin(((k + 1) << 10) + D_neo_ark_submarine_tunnel_80187960[j + 1].phase + D_neo_ark_submarine_tunnel_80187960[j + 1].offset) << 3);
                        p->y3  = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    p->u0 = u0;
                    p->v0 = v0;
                    p->u1 = u1;
                    p->v1 = v0;
                    p->u2 = u0;
                    p->v2 = v1;
                    p->u3 = u1;
                    p->v3 = v1;
                    addPrim(&gGpuCurrentOt[3], p);
                }
            }
            break;
    }
    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
}

s32 func_neo_ark_submarine_tunnel_8017F064(Task* arg0, s32 arg1, RoomEventMsg* arg2, GpMessageArg arg3)
{
    u8 temp_s0;
    u8 temp_s0_2;
    u8 temp_s0_3;
    u8 temp_s0_4;

    temp_s0 = arg2->field_2;
    if ((temp_s0 == 1) && (GameFlag_GetNibble(0xFF) == temp_s0) && (gGameSession->at4.loc.variant == 3)) {
        func_800E3FAC(0xA2, 0x35);
        GameFlag_SetNibble(0xFF, 2);
        GameFlag_SetNibble(0x11F, 1);
        Mc_SaveData[0].state.sceneEvent = 0x1A;
        func_800E8634(&D_80135220, 0, &D_80135FD0);
    }
    if ((arg2->field_2 == 2) && (GameFlag_GetNibble(0xBC) == 0)) {
        temp_s0_2 = gGameSession->at4.loc.variant;
        if (temp_s0_2 == 1) {
            func_800E8614(D_neo_ark_submarine_tunnel_80181AF0, 0);
            D_neo_ark_submarine_tunnel_80181DF0 = temp_s0_2;
        }
    }
    temp_s0_3 = arg2->field_2;
    if ((temp_s0_3 == 3) && (D_neo_ark_submarine_tunnel_80181DF0 == 0) && (gGameSession->at4.loc.warp == 2) && (GameFlag_GetNibble(0xFF) == 0) && (gGameSession->at4.loc.variant == temp_s0_3)) {
        GameFlag_SetNibble(0xFF, 1);
        func_800E8614(&D_80136108, 0);
        D_neo_ark_submarine_tunnel_80181DF0 = 1;
    }
    if ((arg2->field_2 == 2) && (D_neo_ark_submarine_tunnel_80181DF0 == 0)) {
        temp_s0_4 = gGameSession->at4.loc.warp;
        if (temp_s0_4 == 1) {
            Gp_MsgPlayerWeapon(1);
            D_neo_ark_submarine_tunnel_80181DF0 = temp_s0_4;
        }
    }
    if ((arg2->field_2 == 3) && (D_neo_ark_submarine_tunnel_80181DF0 == 0) && (gGameSession->at4.loc.warp == 2)) {
        Gp_MsgPlayerWeapon(1);
        D_neo_ark_submarine_tunnel_80181DF0 = 1;
    }
    return 0;
}

/// Answers 0 unconditionally.
s32 func_neo_ark_submarine_tunnel_8017F27C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Save-location message handler: copies the incoming `GpSaveLoc` onto the
/// outgoing one, forwards both to `func_map_neo_ark_80179B14` and answers 1.
s32 func_neo_ark_submarine_tunnel_8017F284(Task* arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

/// Message 0x13F0 handler: for an `arg2` of 4 or 5, and only while the
/// session's place is 1, passes it to `Gp_SpawnIfCapIdle`. Answers 0.
s32 func_neo_ark_submarine_tunnel_8017F2C8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 < 6) {
        if (arg2 >= 4) {
            if (gGameSession->at4.loc.variant == 1) {
                Gp_SpawnIfCapIdle(arg2, 0);
            }
        }
    }
    return 0;
}

/// Starts or steers the screen wave. A non-positive `arg0` sets the CD
/// queue's `field_22A` to 2, fills the wave context (ramp length 1, peak 0x60,
/// tinted 0x40/0x80/0x80) and spawns the wave task with it; a positive one is
/// written to the context's mode, where 1 ramps the running wave back down.
void func_neo_ark_submarine_tunnel_8017F318(s32 arg0)
{
    CdCmdQueue* queue = &CdCmd_Queue;

    if (arg0 <= 0) {
        queue->field_22A                          = 2;
        D_neo_ark_submarine_tunnel_80187A20.span  = 1;
        D_neo_ark_submarine_tunnel_80187A20.scale = 0x60;
        D_neo_ark_submarine_tunnel_80187A20.r     = 0x40;
        D_neo_ark_submarine_tunnel_80187A20.blend = ANIMATION_BLEND_INTERPOLATE;
        D_neo_ark_submarine_tunnel_80187A20.g     = 0x80;
        D_neo_ark_submarine_tunnel_80187A20.b     = 0x80;
        Task_SpawnFromTable(D_neo_ark_submarine_tunnel_80181A34, 0, 0, &D_neo_ark_submarine_tunnel_80187A20);
        return;
    }
    D_neo_ark_submarine_tunnel_80187A20.state = arg0;
}

void func_neo_ark_submarine_tunnel_8017F398(s32 arg0)
{
    GameFlag_SetNibble(0xBC, arg0);
}

/// First state of the room task: installs the room's message table, publishes
/// the task in pointer slot 7, plays sound event 0x550C0003 and advances.
static void func_neo_ark_submarine_tunnel_8017F3BC(Task* arg0)
{
    arg0->msgTable = D_neo_ark_submarine_tunnel_80181A50;
    Game_SetPtrSlot(arg0, 7);
    SndEvt_EnqueueType6(0x550C0003, 0, 0);
    arg0->state = arg0->state + 1;
}

/// Later states of the room task: reads pointer slot 3 and discards it.
static void func_neo_ark_submarine_tunnel_8017F414(Task* task)
{
    gameGetPtrSlot(3);
}

/// Room task tick: copies the three-entry state table
/// `D_neo_ark_submarine_tunnel_8017D614` to the stack and calls the entry for
/// the task's state.
void func_neo_ark_submarine_tunnel_8017F434(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_submarine_tunnel_8017D614;
    sp.funcs[task->state](task);
}
