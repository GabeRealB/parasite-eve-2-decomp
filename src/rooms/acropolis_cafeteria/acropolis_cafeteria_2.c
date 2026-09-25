#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "rooms/acropolis_cafeteria.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d830.h"

/// Copies the third column of a rotation matrix into an `SVECTOR`.
#define COPY_MATRIX_COLUMN2(src, dst)     \
    __asm__ volatile("lhu $12, 4(%0);"    \
                     "lhu $13, 10(%0);"   \
                     "lhu $14, 16(%0);"   \
                     "sh $12, 0(%1);"     \
                     "sh $13, 2(%1);"     \
                     "sh $14, 4(%1)"      \
                     :                    \
                     : "r"(src), "r"(dst) \
                     : "$12", "$13", "$14", "memory")

/// 0xD8 work block the falling-debris task keeps at `Task::work`
/// (`memCalloc(0xD8)` in `func_acropolis_cafeteria_801818DC`, released by
/// `func_acropolis_cafeteria_80181E3C` through `Gp_UnlinkObj`).
///
/// It opens with the `GpObj` list node linked onto `Gp_ObjLists[4]`, whose
/// `field_C` points at the six `GpRec18` slots that follow it in the same
/// block. `field_B0` is the spawn-time random seed / countdown
/// (`(rand() & 0xFFF) + 0x3000`, decremented every frame);
/// `field_B4` / `field_B8` / `field_BC` are the per-axis velocities added into
/// the object's coordinate; `field_C4` is the rotation handed to `RotMatrix`
/// and `field_CC` the normalised surface direction from `Gfx_MatrixCol2` /
/// `VectorNormalSS`; `field_D4` is the task's own sub-state.
typedef struct AcropolisCafeteriaDebris {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 slots[6];
    /* 0xB0 */ s32     field_B0;
    /* 0xB4 */ s32     field_B4;
    /* 0xB8 */ s32     field_B8;
    /* 0xBC */ s32     field_BC;
    /* 0xC0 */ byte    pad_C0[4];
    /* 0xC4 */ SVECTOR field_C4;
    /* 0xCC */ SVECTOR field_CC;
    /* 0xD4 */ u16     field_D4;
    /* 0xD6 */ byte    pad_D6[2];
} AcropolisCafeteriaDebris;
STATIC_ASSERT_SIZEOF(AcropolisCafeteriaDebris, 0xD8);

extern void Stage_RequestFromAreaTable(s32 arg0);

extern s8  D_8007106B;
extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

extern GpMsgEntry D_acropolis_cafeteria_80184CEC[];
extern s32        D_acropolis_cafeteria_80184CFC;
extern SVECTOR    D_acropolis_cafeteria_80184E80[];
extern SVECTOR    D_acropolis_cafeteria_80184E88;
extern MATRIX     D_acropolis_cafeteria_8018D5A0;
extern MATRIX     D_acropolis_cafeteria_8018D5C0;
extern MATRIX     D_acropolis_cafeteria_8018D5E0;
extern MATRIX     D_acropolis_cafeteria_8018D600;
extern MATRIX     D_acropolis_cafeteria_8018D620;
extern MATRIX     D_acropolis_cafeteria_8018D640;
extern MATRIX     D_acropolis_cafeteria_8018D660;
extern MATRIX     D_acropolis_cafeteria_8018D680;
extern SVECTOR    D_acropolis_cafeteria_8018D6AC;

void func_acropolis_cafeteria_8017FBEC(GpCoord* coord, s32 arg1, s32 arg2, u8* rgb);
void func_acropolis_cafeteria_80180018(GpCoord* coord, s32 arg1, u8* rgb);
void func_acropolis_cafeteria_8018089C(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3);
void func_acropolis_cafeteria_80180F1C(GpCoord* coord, s16 arg1, u8* rgb);
void func_acropolis_cafeteria_80181E3C(Task* arg0);

void func_acropolis_cafeteria_8017E47C(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = gGameSession->at4;
    key.loc.view = 0x64;
    slotParam[0] = Stream_FindSlot(key.raw.data, 0, 0);
    CdCmd_Enqueue(0x61, 0, slotParam);
    goto advance;

L_case2:
    if (queue->field_1FA == 0) {
        return;
    }
    SetDispMask(1);
    task->killCountdown = 0;
    task->spawnArg1     = 0;
    task->state         = task->state + 1;
    return;

L_case3:
    if (++task->killCountdown == 0x443) {
        Stage_RequestFromAreaTable(0);
        task->spawnArg1 = 1;
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    if (task->spawnArg1 == 0) {
        Stage_RequestFromAreaTable(0);
    }
    taskKill(task);
    Display_ResetHeapWrapper();
}

/// Fades the screen to white, four steps of the kill countdown per frame, and
/// kills the task once the countdown reaches 0x100.
void func_acropolis_cafeteria_8017E658(Task* arg0)
{
    u16 temp_v0;

    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
    temp_v0             = arg0->killCountdown + 4;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}

void func_acropolis_cafeteria_8017E6B8(Task* arg0)
{
    Display_SpawnWithOt(&D_acropolis_cafeteria_80184178, 2, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

void func_acropolis_cafeteria_8017E708(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    SVECTOR*   vec;

    work  = (GpEffWork*)task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (task->state != 0) {
        return;
    }
    task->msgTable = D_acropolis_cafeteria_80184CEC;
    Game_SetPtrSlot(task, 5);
    vec                            = &work->move;
    D_acropolis_cafeteria_80184CFC = 0;
    work->move.vx                  = 0x220;
    work->move.vy                  = -0x12C;
    work->move.vz                  = -0x6A0;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    work->move.vx = 0x400;
    work->move.vy = -0x12C;
    work->move.vz = -0x260;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    work->move.vx = 0x370;
    work->move.vy = -0x12C;
    work->move.vz = -0x860;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    task->state   = task->state + 1;
    work->move.vx = 0xBB8;
    work->move.vy = -0x834;
    work->move.vz = -0x7D0;
    Gp_SpawnEff(0x60064, coord, 1, vec);
    work->move.vx = 0xB22;
    work->move.vy = -0x834;
    work->move.vz = -0x900;
    Gp_SpawnEff(0x60064, coord, 1, vec);
    D_80115758 = 0x6028D;
    D_8011572C = 0x6028E;
    D_80115750 = 0x6028F;
}
/// Spawns 40 effects on entry to session mode 9, then two per tick while it
/// remains active. Releases the work block when the room effect gate clears.
void func_acropolis_cafeteria_8017E89C(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    s32        i;
    u16        count;
    s32        flags;
    s32        spawnArg;
    u8         mode;
    u16        rnd;

    work  = (GpEffWork*)task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (D_acropolis_cafeteria_80184CFC == 0) {
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    mode = gGameSession->at4.loc.view;
    if (mode == 9) {
        count = 0x28;
        if (work->scale != mode) {
            flags = 0x1000;
        } else {
            count = 2;
            flags = 0;
        }
        for (i = 0; i < count; i++) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->move.vx = (u32)rnd % 2620 + 0x230;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->move.vy = -0x12C - (u16)((u32)rnd % 5) * 0x190;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->move.vz = (rnd & 0x3FF) + 0xB00;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            spawnArg      = flags + 0x180;
            Gp_SpawnEff(0x60061, coord, (rnd & 0xFF) + spawnArg, &work->move);
        }
    }
    work->scale = gGameSession->at4.loc.view;
}

/// While the room's effect gate is set and the session view is mode 9, draws
/// the effect as a semi-transparent billboard animated through a 5-column
/// sheet of 48-pixel cells, one cell every `step` frames. The first frame it
/// projects in front of the camera seeds a random spin, drift and frame
/// period; spawn flag `0x1000` starts it ten frames in. Each frame it drifts
/// along Z until Z reaches `0xB00` and along Y after that. The effect is
/// released once it has shown all ten cells, or as soon as the gate or the
/// view mode no longer hold.
void func_acropolis_cafeteria_8017EA90(Task* task)
{
    GpEffWork*                     work;
    GpCoord*                       coord;
    u8*                            head;
    OverlaySpriteScratch*          block;
    register OverlaySpriteScratch* newHead asm("v0");
    POLY_FT4*                      prim;
    u8                             mode;
    u8                             shade;
    s32                            quot;
    u16                            vz;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (D_acropolis_cafeteria_80184CFC != 0) {
        mode = gGameSession->at4.loc.view;
        if (mode == 9) {
            Gp_UpdateCoord(coord);
            head                               = SCRATCH_HEAD(u8);
            newHead                            = (OverlaySpriteScratch*)(head - 0x18);
            block                              = newHead;
            block->vec.vx                      = *(u16*)&coord->workm.t[0];
            block->vec.vy                      = *(u16*)&coord->workm.t[1];
            vz                                 = *(u16*)&coord->workm.t[2];
            SCRATCH_HEAD(OverlaySpriteScratch) = block;
            block->vec.vz                      = vz;
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_ldv0(&((OverlaySpriteScratch*)(head - 0x18))->vec);
            gte_rtps();
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setcode(prim, 0x2C);
            setlen(prim, mode);
            gte_stsxy(&((OverlaySpriteScratch*)(head - 0x18))->sxy);
            gte_stszotz(&block->otz);
            if (((OverlaySpriteScratch*)(head - 0x18))->otz > 16 && work->age == 0) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->scale   = ((u32)Gp_LcgState >> 16) & 0xFFF;
                work->angle   = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
                work->move.vx = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->move.vy = (((u32)Gp_LcgState >> 16) & 0xF) + 4;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->move.vz = -(((u32)Gp_LcgState >> 16) & 0xF) - 4;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->step    = (((u32)Gp_LcgState >> 16) & 3) + 3;
                if (task->spawnArg1 & 0x1000) {
                    work->age = 10;
                }
            }
            if (work->age < 10) {
                shade = work->age * 4;
                setRGB0(prim, shade, shade, shade);
            } else {
                shade = 40;
                setRGB0(prim, shade, shade, shade);
            }
            prim->tpage = 0x2B;
            prim->clut  = 0x4380;
            setSemiTrans(prim, 1);
            quot      = work->age / work->step;
            prim->u0  = (quot % 5) * 48;
            quot      = work->age / work->step;
            prim->v0  = (quot / 5) * 48;
            quot      = work->age / work->step;
            prim->u1  = (quot % 5) * 48 + 47;
            quot      = work->age / work->step;
            prim->v1  = (quot / 5) * 48;
            quot      = work->age / work->step;
            prim->u2  = (quot % 5) * 48;
            quot      = work->age / work->step;
            prim->v2  = (quot / 5) * 48 + 47;
            quot      = work->age / work->step;
            prim->u3  = (quot % 5) * 48 + 47;
            quot      = work->age / work->step;
            prim->v3  = (quot / 5) * 48 + 47;
            block->dx = (((work->angle * 47) / block->otz) * rsin(work->scale)) >> 12;
            block->dy = (((work->angle * 47) / block->otz) * rcos(work->scale)) >> 12;
            prim->x0  = (u16)block->sxy.vx + *(u16*)&block->dx;
            prim->x3  = (u16)block->sxy.vx - *(u16*)&block->dx;
            prim->y0  = (u16)block->sxy.vy - *(u16*)&block->dy;
            prim->y3  = (u16)block->sxy.vy + *(u16*)&block->dy;
            block->dx = (((work->angle * 47) / block->otz) * rsin(work->scale + 0x400)) >> 12;
            block->dy = (((work->angle * 47) / block->otz) * rcos(work->scale + 0x400)) >> 12;
            prim->x1  = (u16)block->sxy.vx + *(u16*)&block->dx;
            prim->x2  = (u16)block->sxy.vx - *(u16*)&block->dx;
            prim->y1  = (u16)block->sxy.vy - *(u16*)&block->dy;
            prim->y2  = (u16)block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            SCRATCH_POP_BYTES(0x18);
            if (coord->coord.t[2] > 0xB00) {
                coord->coord.t[2] += work->move.vz;
            } else {
                coord->coord.t[1] += work->move.vy;
            }
            coord->flg = 0;
            work->age++;
            if (work->age <= work->step * 10 - 1) {
                return;
            }
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

void func_acropolis_cafeteria_8017F390(Task* task)
{
    TmdObject*  obj;
    GpEffWork*  work;
    GpCoord*    coord;
    GpMtxWords* rot;
    s16         state;
    s32         v;
    s32         w;
    s32         n;
    s32         k; // one variable for both branches' LCG addend; literal constants allocate differently
    s32         pan;

    obj   = task->extra.tmd;
    work  = (GpEffWork*)task->spawnArg2;
    state = Gp_State1C->eventState;
    coord = obj->coords;
    if (state >= 4) {
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    if (state != 0) {
        return;
    }
    Gp_UpdateCoord(coord);
    work->age++;
    if (task->state == 0) {
        obj->flags &= ~0x80;
        if (task->spawnArg1 != 0) {
            work->period = 0xD90;
            work->angle  = 0;
            work->index  = 2;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->scale  = ((u32)Gp_LcgState >> 16) & 0xF00;
        } else {
            work->scale  = 0x400;
            work->angle  = 0;
            work->period = 0xB00;
        }
        Gfx_RotMatrixY(&coord->coord, work->scale, 0);
        task->state++;
        return;
    }
    switch (work->index) {
        case 0:
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->angle  = 0;
            work->scale -= (((u32)Gp_LcgState >> 16) & 0xFF) - 0x80;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            if ((u16)(((u32)Gp_LcgState >> 16) % 30) == 0) {
                work->index = 1;
            }
            if (work->age >= 0x79) {
                work->index = 4;
            }
            break;
        case 1:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (((u32)Gp_LcgState >> 16) & 1) {
                v = work->scale;
                if (v > 0x400) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    n           = v - 0x10;
                    n          -= ((u32)Gp_LcgState >> 16) & 0x3F;
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    n           = v + 0x10;
                    n          += ((u32)Gp_LcgState >> 16) & 0x3F;
                }
                work->scale = n;
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = 0x200;
            if ((u16)(((u32)Gp_LcgState >> 16) % 30) == 0) {
                work->index = 0;
            }
            if (work->age >= 0x79) {
                work->index = 4;
            }
            break;
        case 2:
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->scale -= (((u32)Gp_LcgState >> 16) & 0xFF) - 0x80;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            if ((u16)(((u32)Gp_LcgState >> 16) % 240) == 0) {
                work->scale = 0x400;
                work->angle = 0x200;
                work->index = 3;
            }
            break;
        case 3:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 7) == 0) {
                work->angle = 0;
                work->index = 2;
            }
            break;
        case 4:
            w = work->scale;
            if (w > 0x400) {
                k           = 0x71357911;
                Gp_LcgState = Gp_LcgState * 5 + k;
                w          -= 0x10;
                w          -= ((u32)Gp_LcgState >> 16) & 0x3F;
            } else {
                k           = 0x71357911;
                Gp_LcgState = Gp_LcgState * 5 + k;
                w          += 0x10;
                w          += ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            work->scale = w;
            work->angle = 0x300;
            if ((Gp_GetViewIndex() & 0xFF) == 7 && work->step == 0) {
                pan = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(0x51040006, pan, (s8)gpGetObjDepth(coord));
                work->step = 1;
            }
            break;
    }
    rot          = (GpMtxWords*)&coord->coord;
    rot->m00_m01 = 0x1000;
    rot->m02_m10 = 0;
    rot->m11_m12 = 0x1000;
    rot->m20_m21 = 0;
    rot->m22     = 0x1000;
    Gfx_RotMatrixY(&coord->coord, work->scale, 0);
    COPY_MATRIX_COLUMN2(&coord->coord, &work->move);
    work->move.vx      = (work->move.vx * work->angle) >> 16;
    work->move.vy      = (work->move.vy * work->angle) >> 16;
    work->move.vz      = (work->move.vz * work->angle) >> 16;
    coord->coord.t[0] += work->move.vx;
    coord->coord.t[1] += work->move.vy;
    coord->coord.t[2] += work->move.vz;
    coord->flg         = 0;
    if (work->period < coord->coord.t[0]) {
        Gp_ReleaseState1CMem(work, task);
    }
}

s32 func_acropolis_cafeteria_8017F908(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    GpCoord* coord;

    coord                          = task->extra.tmd->coords;
    D_acropolis_cafeteria_80184CFC = arg2;
    if (arg2 != 0) {
        Gp_SpawnEff(0x6009D, coord, 0, NULL);
    }
    return 0;
}

/// Flash effect on the object's coordinate. Over `spawnArg1` frames it
/// brightens and widens two wedge bursts and a ring, all tinted with red at
/// full, blue at half and green at quarter intensity; it then lays a fade quad
/// and shrinks a billboard glow by sixteen shades a frame until it is gone.
/// Paused while the room event state is non-zero, released once it reaches 4.
void func_acropolis_cafeteria_8017F948(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    u8         rgb[3];

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_acropolis_cafeteria_80180018(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_acropolis_cafeteria_80180018(coord, (s16)((u16)work->angle * 2), rgb);
                func_acropolis_cafeteria_8017FBEC(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_acropolis_cafeteria_80180F1C(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags an error, queues a ring of sixteen gouraud `POLY_G4` quads
/// around the projected point. `arg1` is the ring's inner radius and
/// `arg1 + arg2` its outer one, both in world units scaled by depth. The inner
/// edge takes `rgb` and the outer edge is black.
void func_acropolis_cafeteria_8017FBEC(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    s32                sum;
    s32                otz;
    register s32       rOuter asm("v0");
    s32                rInner;
    u8*                color;
    s32                t;
    u16                vz;
    u16                vx;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    vx = *(u16*)&arg0->workm.t[0];
    USE_REG(vx);
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw02Scratch*)tmp;
    }
    block->vec.vx = vx;
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw02Scratch*)(head - 0x1C))->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw02Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw02Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        USE_REG(head);
        otz                                      = ((RoomDraw02Scratch*)(head - 0x1C))->otz + 1;
        rOuter                                   = ((s16)saved * 64) / otz;
        ((RoomDraw02Scratch*)(head - 0x1C))->otz = otz;
        rInner                                   = (s16)sum * 64;
        block->rOuter                            = rOuter;
        rInner                                   = rInner / ((RoomDraw02Scratch*)(head - 0x1C))->otz;
        ang                                      = 0;
        block->rInner                            = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            maskLo   = 0xFFFFFF;
            maskHi   = 0xFF000000;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG2(maskLo, maskHi);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags an error, queues eight gouraud `POLY_G4` wedges that fill a
/// disc around the projected point. `arg1` is the radius in world units scaled
/// by depth; each wedge is `rgb` at the centre and black at the rim.
void func_acropolis_cafeteria_80180018(GpCoord* arg0, s32 arg1, u8* rgb)
{
    RoomDraw04Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    u8*                head;
    s32                otz;
    s32                radius;
    s32                t;
    s32                t2;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw04Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw04Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw04Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw04Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw04Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        otz           = block->otz + 1;
        radius        = ((s16)arg1 * 64) / otz;
        block->otz    = otz;
        block->radius = radius;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x18);
}

void func_acropolis_cafeteria_801803AC(Task* task)
{
    GpCoord    coord;
    GpCoord*   coords;
    GpCoord*   objCoord;
    GpCoord*   dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = (GpCoord*)task->work;
    work     = (GpEffWork*)task->spawnArg2;
    objCoord = task->extra.tmd->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GpCoord*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_acropolis_cafeteria_80184E80[0].vx;
                objCoord->coord.t[1] = D_acropolis_cafeteria_80184E80[0].vy;
                objCoord->coord.t[2] = D_acropolis_cafeteria_80184E80[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_acropolis_cafeteria_80184E80[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &coords[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &coords[i + 8];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->parent;
                coord.coord.t[0] = D_acropolis_cafeteria_80184E88.vx;
                coord.coord.t[1] = D_acropolis_cafeteria_80184E88.vy;
                coord.coord.t[2] = D_acropolis_cafeteria_80184E88.vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &coords[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &coords[(work->age & 7) + 8];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &coords[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &coords[i + 8];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_acropolis_cafeteria_8018089C(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the beam between two eight-slot coordinate trails as seven gouraud
/// `POLY_G4` quads, walking back from slot `arg2`. Each quad joins two adjacent
/// slots of `arg0` and `arg1`, and fades with age: the newer edge is scaled by
/// `0x40 - 9 * i` and the older one by nine less. `arg3` packs the beam colour
/// as 2-bit multipliers for red, green and blue at bits 8, 4 and 0. A quad the
/// GTE flags as invalid is skipped.
void func_acropolis_cafeteria_8018089C(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GpCoord*           a;
    GpCoord*           b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    {
        register u8* tmp asm("v0");

        tmp                = SCRATCH_HEAD(u8) - sizeof(RoomDraw03Scratch);
        blk                = (RoomDraw03Scratch*)tmp;
        SCRATCH_HEAD(void) = tmp;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = *(u16*)&a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = *(u16*)&a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = *(u16*)&b->workm.t[0];
        blk->v[1].vy = *(u16*)&b->workm.t[1];
        blk->v[1].vz = *(u16*)&b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = *(u16*)&a->workm.t[0];
        blk->v[2].vy = *(u16*)&a->workm.t[1];
        blk->v[2].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = *(u16*)&b->workm.t[0];
        blk->v[3].vy = *(u16*)&b->workm.t[1];
        blk->v[3].vz = *(u16*)&b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = (POLY_G4*)gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

void func_acropolis_cafeteria_80180C94(Task* task)
{
    GpCoord*   objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.tmd->coords;
    work     = (GpEffWork*)task->spawnArg2;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1 != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_acropolis_cafeteria_8017FBEC(objCoord, 0x100, 0x100, rgb);
            func_acropolis_cafeteria_8017FBEC(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags an error, queues a star-shaped glow of gouraud `POLY_G4`
/// wedges around the projected point: sixteen around the full circle, half of
/// them at full radius in half-intensity `rgb` and half at half radius in full
/// `rgb`, then four spikes a quarter turn apart, two reaching the full radius
/// and two twice it. `arg1` sizes it in world units scaled by depth; every
/// wedge fades to black at its rim.
void func_acropolis_cafeteria_80180F1C(GpCoord* arg0, s16 arg1, u8* arg2)
{
    register RoomBillboardScratch* block asm("s3");
    register POLY_G4*              prim asm("s2");
    register s32                   ang asm("s4");
    register void**                scratch asm("a1");
    register u8*                   head asm("a2");
    s32                            t;
    s32                            t2;
    s32                            u;
    u16                            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u16 vx asm("v0");
        vx                                             = *(u16*)&arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomBillboardScratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomBillboardScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomBillboardScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz   += 1;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

void func_acropolis_cafeteria_801818DC(Task* task)
{
    TmdObject*                obj;
    GpCoord*                  coord;
    AcropolisCafeteriaDebris* work;
    GpCoord*                  player;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0xD8, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work         = (TaskIdMap*)work;
    task->exitCallback = func_acropolis_cafeteria_80181E3C;
    task->state        = task->state + 1;
    Mem_Set(work, 0, 0xD8);
    coord->sub = &gGfxViewCoord;
    coord->flg = 0;
    obj->flags = 0;
    RotMatrix(&work->field_C4, &coord->coord);
    work->field_B0     = (rand() & 0xFFF) + 0x3000;
    player             = gameGetPtrSlot(3)->extra.tmd->coords;
    coord->coord.t[0]  = player->coord.t[0];
    coord->coord.t[1]  = player->coord.t[1] - 0x800;
    coord->coord.t[2]  = player->coord.t[2] + 0x800;
    work->obj.ctx.recs = work->slots;
    work->obj.key      = 0x50000;
    work->obj.radius   = 0xFA;
    work->obj.coord    = coord;
    work->obj.pos.vx   = 0;
    work->obj.pos.vy   = 0;
    work->obj.pos.vz   = 0;
    work->obj.flags    = 1;
    Gp_LinkObj(4, &work->obj);
    Gp_InitRec18Table(work->obj.ctx.recs, 6, 0);
    work->obj.flags |= 0x8000;
}

void func_acropolis_cafeteria_80181A3C(Task* task)
{
    MATRIX*                   head;
    AcropolisCafeteriaDebris* work;
    GpCoord*                  coord;
    SVECTOR*                  direction;
    s32                       speed;

    head                 = SCRATCH_HEAD(MATRIX);
    SCRATCH_HEAD(MATRIX) = head - 1;
    work                 = (AcropolisCafeteriaDebris*)task->work;
    coord                = task->extra.tmd->coords;
    work->field_B0--;
    coord->flg         = 0;
    coord->coord.t[1] += 0x80;
    Gp_UpdateCoord(coord);
    switch (work->field_D4) {
        case 0:
            if (Gp_FindRec18(work->obj.ctx.recs, 0)) {
                work->field_D4++;
                head[-1]  = coord->coord;
                direction = &work->field_CC;
                Gfx_MatrixCol2(Player_Status.coordMtx, direction);
                VectorNormalSS(direction, direction);
                rand();
                speed          = work->field_B0;
                speed        >>= 1;
                speed          = (speed * speed) >> 6;
                work->field_B8 = -0x100;
                work->field_B4 = (work->field_CC.vx * speed) >> 24;
                work->field_BC = (work->field_CC.vz * speed) >> 24;
            }
            break;
        case 1:
            work->field_B8 += 0x10;
            if (work->field_B8 > 0) {
                work->field_B8 = 0;
                work->field_D4++;
            } else {
                work->field_C4.vx += (work->field_B0 >> 6) + (rand() & 0x7F);
                work->field_C4.vy += (work->field_B0 >> 6) + (rand() & 0x7F);
                work->field_C4.vz += (work->field_B0 >> 6) + (rand() & 0x7F);
            }
        case 2:
            work->field_B4 = (work->field_B4 * 6) / 7;
            if (ABS(work->field_B4) < 9) {
                work->field_B4 = 0;
            }
            work->field_BC = (work->field_BC * 6) / 7;
            if (ABS(work->field_BC) < 9) {
                work->field_BC = 0;
            }
            if ((work->field_B4 | work->field_BC) == 0) {
                work->field_D4++;
            }
            coord->coord.t[0] += work->field_B4;
            coord->coord.t[1] += work->field_B8;
            coord->coord.t[2] += work->field_BC;
            break;
        case 3:
            work->field_C4.vx = (work->field_C4.vx * 2) / 3;
            if (ABS(work->field_C4.vx) < 9) {
                work->field_C4.vx = 0;
            }
            work->field_C4.vz = (work->field_C4.vz * 2) / 3;
            if (ABS(work->field_C4.vz) < 9) {
                work->field_C4.vz = 0;
            }
            if (((u16)work->field_C4.vx | (u16)work->field_C4.vz) == 0) {
                work->field_D4 = 0;
            }
            break;
    }
    RotMatrix(&work->field_C4, &coord->coord);
    Gp_ClearRec18Occupied(work->slots);
    SCRATCH_POP(MATRIX);
}

void func_acropolis_cafeteria_80181E30(Task* arg0)
{
    arg0->state = 3;
}

void func_acropolis_cafeteria_80181E3C(Task* arg0)
{
    Gp_UnlinkObj(arg0->work);
    taskKill(arg0);
}

/// State handlers of the falling-debris task: set-up, the per-frame update, a
/// step that moves the task to state 3, and the exit that unlinks and kills it.
const TaskFuncTable4 D_acropolis_cafeteria_8017D69C = { {
    func_acropolis_cafeteria_801818DC,
    func_acropolis_cafeteria_80181A3C,
    func_acropolis_cafeteria_80181E30,
    func_acropolis_cafeteria_80181E3C,
} };

/// Runs the task's current state through a stack copy of the room's
/// four-entry state table.
void func_acropolis_cafeteria_80181E70(Task* task)
{
    TaskFuncTable4 states;

    states = D_acropolis_cafeteria_8017D69C;
    states.funcs[task->state](task);
}

/// Gets a 16.16 X/Y/Z displacement for `rec` from `func_800E0C10` and, when it
/// reports one, adds its X and Z to the coordinate's translation, rounding a
/// fractional part away from zero. The whole-unit displacement is also left in
/// `D_acropolis_cafeteria_8018D6AC`. Returns non-zero when the X or Z
/// displacement is non-zero.
s32 func_acropolis_cafeteria_80181ED4(GpCoord* coord, GpRec18* rec, s16 arg2)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    register void*    p asm("v1");
    s32               val;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    p        = head - 0x14;
    s        = p;
    *scratch = p;
    s->moved = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        coord->coord.t[0]                += ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]                += s->delta.vz.h.hi;
        D_acropolis_cafeteria_8018D6AC.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_acropolis_cafeteria_8018D6AC.vy = s->delta.vy.w >> 16;
        D_acropolis_cafeteria_8018D6AC.vz = s->delta.vz.w >> 16;
        val                               = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_acropolis_cafeteria_8018D6AC.vx++;
            } else {
                coord->coord.t[0]--;
                D_acropolis_cafeteria_8018D6AC.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_acropolis_cafeteria_8018D6AC.vz++;
            } else {
                coord->coord.t[2]--;
                D_acropolis_cafeteria_8018D6AC.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP_BYTES(0x14);
    return s->moved;
}

/// Measures the bearing of each type-1 or type-3 record in `recs` (up to
/// `count`, or the first zero key) from the coordinate's world position,
/// relative to the direction it faces. For a record that has every other such
/// record within a quarter turn of it, moves the coordinate `push` units back
/// along that record's bearing, in X and Z. Returns non-zero if it moved the
/// coordinate; returns 0 at once while `gGameSession->viewReady` is 1.
s32 func_acropolis_cafeteria_80182078(GpCoord* coord, GpRec18* recs, s16 count, s16 push)
{
    void**                  scratch;
    void**                  tail;
    u8*                     head;
    OverlayBisectorScratch* st;
    u16                     vz;
    s16                     d;
    s16                     dz;
    s32                     t;
    s32                     hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp = head - sizeof(OverlayBisectorScratch);
        st  = (OverlayBisectorScratch*)tmp;
    }
    st->eye.vx = *(u16*)&coord->coord.t[0];
    st->eye.vy = *(u16*)&coord->coord.t[1];
    vz         = *(u16*)&coord->coord.t[2];
    *scratch   = st;
    st->eye.vz = vz;

    overlayToWorld(coord->sub, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    overlayToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = (u16)recs[st->i].point.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)recs[st->i].point.vy - (u16)st->eye.vy;
            dz               = (u16)recs[st->i].point.vz - (u16)st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = ratan2(st->delta.vx, dz);

            st->delta.vx     = (u16)st->aim.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)st->aim.vy - (u16)st->eye.vy;
            dz               = (u16)st->aim.vz - (u16)st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = (u16)st->angle[st->i] - ratan2(st->delta.vx, dz);

            d = st->angle[st->i];
            if (st->angle[st->i] < 0) {
            wrapUp1:
                if (d < -0x800) {
                    d += 0x1000;
                    goto wrapUp1;
                }
            } else {
            wrapDown1:
                if (d > 0x800) {
                    d -= 0x1000;
                    goto wrapDown1;
                }
            }
            st->angle[st->i] = d;
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                d        = st->diff;
                if (st->diff < 0) {
                wrapUp2:
                    if (d < -0x800) {
                        d += 0x1000;
                        goto wrapUp2;
                    }
                } else {
                wrapDown2:
                    if (d > 0x800) {
                        d -= 0x1000;
                        goto wrapDown2;
                    }
                }
                t        = d;
                st->diff = t;
                SOFT_BARRIER();
                if (t < 0) {
                    t = -t;
                }
                if (t >= 0x401) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    tail = (void**)G_SCRATCH_HEAD;
    hit  = st->hit;
    SCRATCH_POP_BYTES_AT(tail, sizeof(OverlayBisectorScratch));
    return hit;
}

void func_acropolis_cafeteria_801827C4(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;

    obj = (GpItemObj8*)task->spawnArg2;
    tmd = task->extra.tmd;
    if (Gp_GetCurBit2Flag(obj->field_8) != 2) {
        tmd->lightMtx = &D_acropolis_cafeteria_8018D5C0;
        tmd->colorMtx = &D_acropolis_cafeteria_8018D5A0;
        tmd->flags    = 0;
    } else {
        tmd->flags |= 0x80;
    }
    switch (Gp_GetViewIndex() & 0xFF) {
        case 0xC:
            tmd->otOffset = 7;
            break;
        case 0x18:
            tmd->otOffset = 4;
            break;
        default:
            tmd->otOffset = -2;
            break;
    }
}
void func_acropolis_cafeteria_8018286C(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = task->extra.tmd;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    if ((Gp_GetViewIndex() & 0xFF) != 9) {
        tmd->flags = 0x80;
        return;
    }
    if (obj->field_8 == 0xA) {
        Gfx_RotMatrixX(&task->extra.tmd->coords->coord, 0x400, 1);
    }
    tmd->lightMtx = &D_acropolis_cafeteria_8018D600;
    tmd->colorMtx = &D_acropolis_cafeteria_8018D5E0;
    if (flag == 2) {
        tmd->flags &= 0xFFF7;
        Task_CallExit(task);
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
}
void func_acropolis_cafeteria_80182954(Task* task)
{
    TmdObject* tmd;

    tmd = task->extra.tmd;
    if ((Gp_GetViewIndex() & 0xFF) != 9) {
        tmd->flags = 0x80;
        return;
    }
    tmd->lightMtx = &D_acropolis_cafeteria_8018D640;
    tmd->colorMtx = &D_acropolis_cafeteria_8018D620;
    if (Gp_GetCurBit2Flag(0xA) == 2) {
        tmd->flags |= 0x80;
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
    Gfx_RotMatrixX(&task->extra.tmd->coords->coord, 0x400, 1);
}
void func_acropolis_cafeteria_80182A08(Task* task)
{
    TmdObject* tmd;

    tmd = task->extra.tmd;
    switch (Gp_GetViewIndex() & 0xFF) {
        case 6:
        case 7:
        case 0xA:
            tmd->flags    = 8;
            tmd->lightMtx = &D_acropolis_cafeteria_8018D680;
            tmd->colorMtx = &D_acropolis_cafeteria_8018D660;
            break;
        default:
            tmd->flags |= 0x80;
            return;
    }
    if (Gp_GetCurBit2Flag(0xB) == 2) {
        tmd->flags |= 0x80;
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
}
