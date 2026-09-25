#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "actors/actors_shared_80149ed0.h"
#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

#define RAND() ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16)

/// Working copy of one surface's extents, carved off the scratchpad stack.
typedef struct {
    s16 y;
    s16 dx;
    s16 dy;
    s16 step;
    s16 x;
    s16 z;
} _SurfaceScratch;

/// Parameters of the random effect burst the room's effect task
/// `func_shelter_b4_reservoir_8017FB84` runs every frame, set together by
/// `func_shelter_b4_reservoir_80182B04` and cleared when the task starts.
/// While `field_0` and `field_2` are both non-zero the task walks `field_0`
/// burst points, spawning an effect at each whose roll out of 100 falls below
/// `field_2`, with `field_4` added to the spawn argument.
typedef struct {
    u16 field_0;
    u16 field_2;
    u16 field_4;
} _ShelterB4ReservoirBurst;

/// Work block `func_shelter_b4_reservoir_8017FB84` reaches through its task's
/// `spawnArg2`. Only the three halves it uses are known.
typedef struct {
    byte pad_0[0x22];
    s16  field_22; // Frames counted while game flag 0xB7 is set; the splash pass is skipped while it is zero
    s16  field_24; // Radius of the last randomised burst point, scaled into the `rsin` / `rcos` offsets
    s16  field_26; // The angle of that point, then the splash chance for the frame's movement
} _ShelterB4ReservoirWork;

extern s32 Gp_GetViewIndex(void);
extern s32 func_80179A04(GpSaveLoc* in, GpSaveLoc* out);

extern s16 D_800691CA;
extern s32 D_80070F70;
extern s16 D_80071076;
extern u8  D_8007216C;
extern u8  D_8007216D;
extern s8  D_8007217B;
extern s16 D_80114D08;
extern u8  D_80115680;
extern u8  D_80115690;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115738;
extern s32 D_8011574C;
extern s32 D_80115754;

/// Spawn table of the screen-wave task, and the context it is spawned with.
/// The context's mode word is written through its own symbol, which is how the
/// original reached it.
extern TaskDesc     D_shelter_b4_reservoir_80184724;
extern ActorWaveCtx D_shelter_b4_reservoir_80187624;
extern s16          D_shelter_b4_reservoir_80187628;

/// Current displacement of the screen wave, recomputed every frame from the
/// context's ramp.
extern s32 D_shelter_b4_reservoir_8018473C;

/// The ramp and tint the wave task was spawned with.
extern ActorWaveCtx* D_shelter_b4_reservoir_80187504;

/// Phase records of the wave's 11 column edges and 30 row edges.
extern ActorWaveRec6 D_shelter_b4_reservoir_80187514[11];
extern ActorWaveRec6 D_shelter_b4_reservoir_80187564[30];

extern GpMsgEntry               D_shelter_b4_reservoir_801848BC[];
extern TaskDesc                 D_shelter_b4_reservoir_801848EC;
extern TaskDesc                 D_shelter_b4_reservoir_80184920[];
extern s32                      D_shelter_b4_reservoir_8018492C;
extern Task*                    D_shelter_b4_reservoir_80184930;
extern u8                       D_shelter_b4_reservoir_80184948;
extern u8                       D_shelter_b4_reservoir_80184DC8;
extern u8                       D_shelter_b4_reservoir_80184F78;
extern u8                       D_shelter_b4_reservoir_80184F79;
extern u8                       D_shelter_b4_reservoir_80184F7A;
extern s16                      D_shelter_b4_reservoir_80184F7C;
extern s16                      D_shelter_b4_reservoir_80184F80;
extern s16                      D_shelter_b4_reservoir_80184F82;
extern TaskDesc                 D_shelter_b4_reservoir_80184F84[];
extern RoomWaterSurface         D_shelter_b4_reservoir_80184F90[];
extern RoomWaterSurface         D_shelter_b4_reservoir_80184FA8[];
extern RoomWaterSurface         D_shelter_b4_reservoir_80184FCC[];
extern RoomWaterSurface         D_shelter_b4_reservoir_80184FE4[];
extern s16                      D_shelter_b4_reservoir_80185020;
extern SVECTOR                  D_shelter_b4_reservoir_80185024[];
extern SVECTOR                  D_shelter_b4_reservoir_80185034;
extern SVECTOR                  D_shelter_b4_reservoir_80185044;
extern SVECTOR                  D_shelter_b4_reservoir_80185074;
extern SVECTOR                  D_shelter_b4_reservoir_80185094;
extern SVECTOR                  D_shelter_b4_reservoir_8018509C[];
extern SVECTOR                  D_shelter_b4_reservoir_801850AC[];
extern GpAreaApplyRec           D_shelter_b4_reservoir_801874A0;
extern GpStateBD8               D_shelter_b4_reservoir_80187500;
extern GpSaveLoc                D_shelter_b4_reservoir_80187508;
extern s32                      D_shelter_b4_reservoir_80187510;
extern u8*                      D_shelter_b4_reservoir_80187630;
extern SVECTOR                  D_shelter_b4_reservoir_80187634[];
extern _ShelterB4ReservoirBurst D_shelter_b4_reservoir_80187684;

/// Per-colour right shifts for the red, green and blue channels of the disc's
/// brightness, one row per spawn argument.
extern s16 D_shelter_b4_reservoir_801850BC[][3];

void func_shelter_b4_reservoir_8017E7C8(Task* arg0);
void func_shelter_b4_reservoir_8017E864(Task* task);
void func_shelter_b4_reservoir_8017E8E4(void);
void func_shelter_b4_reservoir_8017EA00(Task* task);
void func_shelter_b4_reservoir_8017EE04(Task* task);
void func_shelter_b4_reservoir_8017F23C(Task* task);
void func_shelter_b4_reservoir_8017F674(Task* task);
void func_shelter_b4_reservoir_8017FB44(Task* arg0);
void func_shelter_b4_reservoir_80180530(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_shelter_b4_reservoir_80180D20(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b4_reservoir_8018110C(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);
void func_shelter_b4_reservoir_80181668(GsCOORDINATE2* coord, u16 frame, s16 size);
void func_shelter_b4_reservoir_801818F0(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b4_reservoir_80182134(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b4_reservoir_80182B04(s16 arg0, u16 arg1, s16 arg2);
void func_shelter_b4_reservoir_80183298(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b4_reservoir_8018351C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b4_reservoir_80183940(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_b4_reservoir_80183E80(GsCOORDINATE2* coord, s16 size);
void func_shelter_b4_reservoir_801843AC(GsCOORDINATE2* arg0, s32 arg1);

/// State handlers of the room task `func_shelter_b4_reservoir_8017E88C` runs,
/// which copies the table to the stack and calls the entry for the task's
/// state: the room's setup, the per-frame state, and `taskKill`.
const TaskFuncTable3 D_shelter_b4_reservoir_8017D5C4 = {
    { func_shelter_b4_reservoir_8017E7C8, func_shelter_b4_reservoir_8017E864, taskKill }
};

/// Task that ripples the whole screen: it redraws the frame just rendered as a
/// 10 by 30 grid of textured quads whose corners are pushed around by sine
/// waves. The first frame gives every column and row edge a random phase
/// offset and speed, takes its context from `spawnArg2` and passes -8 to
/// `Display_ClampField126`. Afterwards the context's mode ramps the strength
/// up to its limit (mode 0), back down to zero and on to mode 2 (mode 1), or
/// ends the task and passes 0 back (mode 2); the displacement is the ramp's
/// share of the context's peak. A non-zero tint flag shades the quads with the context's
/// colour instead of drawing them unlit. The grid is bracketed by draw-mode
/// packets that switch mask-bit setting on at the back of the order table and
/// off again at the front. The state is read through a plain word load at its
/// offset, which keeps it ordered after the store to `D_800691CA`.
void func_shelter_b4_reservoir_8017D650(Task* arg0)
{
    ActorWaveCtx* ctx;
    POLY_FT4*     p;
    DR_STP*       stp;
    s32           i, j, k;
    s32           drawY;
    s32           tpage0, tpage1;
    s32           u0, u1, v0, v1;
    s32           waveX0, waveY0, waveX1, waveY1;
    s32           waveX2, waveY2, waveX3, waveY3;

    D_800691CA = 2;
    switch (*(s32*)((u8*)arg0 + 0x30)) {
        case 0:
            for (i = 0; i < 11; i++) {
                D_shelter_b4_reservoir_80187514[i].phase  = 0;
                D_shelter_b4_reservoir_80187514[i].offset = (u32)rand() >> 3;
                D_shelter_b4_reservoir_80187514[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                D_shelter_b4_reservoir_80187564[i].phase  = 0;
                D_shelter_b4_reservoir_80187564[i].offset = (u32)rand() >> 3;
                D_shelter_b4_reservoir_80187564[i].speed  = (rand() * 100 + 20) >> 15;
            }
            D_shelter_b4_reservoir_8018473C          = 0;
            D_shelter_b4_reservoir_80187504          = arg0->spawnArg2;
            D_shelter_b4_reservoir_80187504->field_6 = 0;
            D_shelter_b4_reservoir_80187504->field_4 = 0;
            Display_ClampField126(-8);
            arg0->state++;
            break;
        case 1:
            ctx = D_shelter_b4_reservoir_80187504;
            switch (ctx->field_4) {
                case 0:
                    if (ctx->field_6 < ctx->field_0) {
                        ctx->field_6++;
                    }
                    break;
                case 1:
                    if (ctx->field_6 > 0) {
                        ctx->field_6--;
                    } else {
                        ctx->field_4 = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    Display_ClampField126(0);
                    break;
            }
            D_shelter_b4_reservoir_8018473C = D_shelter_b4_reservoir_80187504->field_6 * D_shelter_b4_reservoir_80187504->field_2 / D_shelter_b4_reservoir_80187504->field_0;
            for (i = 0; i < 11; i++) {
                D_shelter_b4_reservoir_80187514[i].phase += D_shelter_b4_reservoir_80187514[i].speed;
            }
            for (i = 0; i < 30; i++) {
                D_shelter_b4_reservoir_80187564[i].phase += D_shelter_b4_reservoir_80187564[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)(p + 1);
                    setPolyFT4(p);
                    if (D_shelter_b4_reservoir_80187504->field_8 == 0) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = D_shelter_b4_reservoir_80187504->field_9;
                        p->g0 = D_shelter_b4_reservoir_80187504->field_A;
                        p->b0 = D_shelter_b4_reservoir_80187504->field_B;
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
                        waveX0 = D_shelter_b4_reservoir_8018473C * (rsin((j << 9) + D_shelter_b4_reservoir_80187514[k].phase + D_shelter_b4_reservoir_80187514[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_shelter_b4_reservoir_8018473C * (rsin((k << 10) + D_shelter_b4_reservoir_80187564[j].phase + D_shelter_b4_reservoir_80187564[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_shelter_b4_reservoir_8018473C * (rsin((j << 9) + D_shelter_b4_reservoir_80187514[k + 1].phase + D_shelter_b4_reservoir_80187514[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_shelter_b4_reservoir_8018473C * (rsin(((k + 1) << 10) + D_shelter_b4_reservoir_80187564[j].phase + D_shelter_b4_reservoir_80187564[j].offset) << 3);
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

                        waveX2 = D_shelter_b4_reservoir_8018473C * (rsin(((j + 1) << 9) + D_shelter_b4_reservoir_80187514[k].phase + D_shelter_b4_reservoir_80187514[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = D_shelter_b4_reservoir_8018473C * (rsin((k << 10) + D_shelter_b4_reservoir_80187564[j + 1].phase + D_shelter_b4_reservoir_80187564[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = D_shelter_b4_reservoir_8018473C * (rsin(((j + 1) << 9) + D_shelter_b4_reservoir_80187514[k + 1].phase + D_shelter_b4_reservoir_80187514[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = D_shelter_b4_reservoir_8018473C * (rsin(((k + 1) << 10) + D_shelter_b4_reservoir_80187564[j + 1].phase + D_shelter_b4_reservoir_80187564[j + 1].offset) << 3);
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
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
}

void func_shelter_b4_reservoir_8017DE8C(Task* task)
{
    switch (task->state) {
        case 0:
            gGameSession->eventState = 1;
            gGameSession->hideHud    = 1;
            task->state++;
            break;
        case 1:
            Gp_RunCapCmd(3, 0);
            D_80115690 = 1;
            D_80115680 = 5;
            task->state++;
            break;
        case 2:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 3:
            Gp_StateF0.field_4 = 0;
            if (Gp_GetCapEventKey() == 0xC) {
                taskKill(task);
                D_8007216C = 5;
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                Gp_MsgAllyWeapon(1);
                Gp_MsgAlly3F3(1);
                gGameSession->eventState = 0;
                gGameSession->hideHud    = 0;
                D_80114D08               = 0xA;
                break;
            }
            Gp_MsgSlot4Chain(0, 0);
            func_800E8634((s32)&D_shelter_b4_reservoir_80184948, 0, (s32)&D_shelter_b4_reservoir_80184DC8);
            task->state++;
            break;
        case 4:
            if (gGameSession->eventState == 0) {
                D_8007216D                  = 2;
                gGameSession->at4.loc.room  = 2;
                gGameSession->roomObjsDirty = 1;
                GameFlag_SetNibble(0xB7, 1);
                GameFlag_SetNibble(0x1BF, 2);
                GameFlag_SetNibble(0xB6, 1);
                GameFlag_SetNibble(0x1BE, 2);
                Gp_ApplyAreaRecs(&D_shelter_b4_reservoir_801874A0);
                D_80114D08 = 0xA;
                taskKill(task);
            }
            break;
    }
}

void func_shelter_b4_reservoir_8017E068(void)
{
    D_shelter_b4_reservoir_80187510 = (D_shelter_b4_reservoir_80184F78 << 0x18) | (D_shelter_b4_reservoir_80184F7A << 0xC) | (D_shelter_b4_reservoir_80184F79 << 0x10) | D_shelter_b4_reservoir_80184F7C;
}

void func_shelter_b4_reservoir_8017E0AC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(arg0->spawnArg1, 0);
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                taskKill(arg0);
                Gp_MsgPlayerWeapon(1);
                Gp_StateF0.field_4 = 0;
                D_80114D08         = 0xA;
                break;
            }
            Gp_StateF0.field_4 = 1;
            Gp_TriggerPeIfArmed();
            D_shelter_b4_reservoir_80187500.field_0 = 0;
            D_shelter_b4_reservoir_80187500.field_1 = 0;
            D_shelter_b4_reservoir_80187500.field_2 = 0x1E;
            Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b4_reservoir_80187500);
            arg0->killCountdown = 0x1E;
            arg0->state++;
            break;
        case 3:
            if (--arg0->killCountdown == 0) {
                SndEvt_EnqueueType6(0x542D0001, 0, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(0x542D0001) == 0) {
                arg0->state++;
            }
            break;
        case 5:
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b4_reservoir_80187508.field_2;
            Mc_SaveData.at4.loc.warp = D_shelter_b4_reservoir_80187508.field_4;
            Mc_SaveData.at4.loc.room = D_shelter_b4_reservoir_80187508.field_1;
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b4_reservoir_8017E25C(void)
{
    return 0;
}

s32 func_shelter_b4_reservoir_8017E264(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179A04(src, dst);
    if (*(u16*)src == 0x2C) {
        if (GameFlag_GetNibble(0xB7) == 1) {
            if (src->field_5 == 0) {
                Gp_SetNibbleIf(src->field_6, 2);
                Gp_RunCapCmd1(2);
            }
        } else {
            if (src->field_5 == 0) {
                D_shelter_b4_reservoir_80187508.field_2 = dst->field_0;
                D_shelter_b4_reservoir_80187508.field_4 = dst->field_2;
                D_shelter_b4_reservoir_80187508.field_1 = dst->field_3;
                Task_SpawnFromTable(&D_shelter_b4_reservoir_801848EC, 3, 0xB, 0);
            }
        }
        return 0;
    }
    return 1;
}

s32 func_shelter_b4_reservoir_8017E354(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        Gp_MsgPlayer3F3(0);
        Gp_MsgAlly3F3(0);
        Gp_MsgPlayerWeapon(0);
        Gp_MsgAllyWeapon(0);
        D_8007216C         = 6;
        Gp_StateF0.field_4 = 2;
        Task_SpawnFromTable(&D_shelter_b4_reservoir_801848EC, 0, 0, 0);
    }
    return 0;
}

s32 func_shelter_b4_reservoir_8017E3C4(void)
{
    return 0;
}

s32 func_shelter_b4_reservoir_8017E3CC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        SndEvt_EnqueueType6(0x542D0000 | 2, 0, 0);
    }
    return 0;
}

void func_shelter_b4_reservoir_8017E400(Task* arg0)
{
    s16 temp_v1;
    s32 temp_v0;

    temp_v0                         = (s32)(arg0->killCountdown * 0x5DC) / (s32)arg0->spawnArg1;
    temp_v1                         = (u16)arg0->killCountdown + 1;
    arg0->killCountdown             = temp_v1;
    D_shelter_b4_reservoir_80184F80 = temp_v0 - 0x7D0;
    if (arg0->spawnArg1 < temp_v1) {
        taskKill(arg0);
        D_shelter_b4_reservoir_8018492C = 0;
    }
}

void func_shelter_b4_reservoir_8017E4B0(Task* arg0)
{
    s16 temp_v1;
    s32 temp_v0;

    temp_v0                         = (s32) - (arg0->killCountdown * 0x708) / (s32)arg0->spawnArg1;
    temp_v1                         = (u16)arg0->killCountdown + 1;
    arg0->killCountdown             = temp_v1;
    D_shelter_b4_reservoir_80184F82 = (s16)temp_v0;
    if (arg0->spawnArg1 < temp_v1) {
        taskKill(arg0);
        D_shelter_b4_reservoir_8018492C = 0;
    }
}

void func_shelter_b4_reservoir_8017E558(Task* arg0)
{
    TmdObject*     obj   = arg0->extra;
    GsCOORDINATE2* coord = obj->coords;

    if (arg0->state == 0) {
        coord->coord.t[0] = -1000;
        coord->coord.t[1] = -1000;
        coord->coord.t[2] = -5000;
        coord->flg        = 0;
        arg0->state++;
    }
    if (arg0->state == 2) {
        coord->coord.t[0] = -1000;
        coord->coord.t[1] = -1000;
        coord->coord.t[2] = -5000;
        coord->flg        = 0;
        arg0->state++;
    }
    if (arg0->state == 3) {
        coord->flg         = 0;
        coord->coord.t[1] += 4;
    }
    if (D_8007216C != 8) {
        obj->flags = 0x84;
    } else {
        obj->flags    = 0;
        obj->otOffset = 0;
    }
}

/// Event callback that runs the screen wave. Called with zero or less, it sets
/// the MDEC decode mode to 2, fills the wave's context (peak 0x60 reached in
/// one step, tinted 0x40/0x80/0x80) and spawns the wave task with it; called
/// with a positive value, it stores that value as the running wave's mode, so
/// 1 fades it out and 2 ends it.
void func_shelter_b4_reservoir_8017E610(s32 arg0)
{
    CdCmdQueue* queue = &CdCmd_Queue;

    if (arg0 <= 0) {
        queue->field_22A                        = 2;
        D_shelter_b4_reservoir_80187624.field_0 = 1;
        D_shelter_b4_reservoir_80187624.field_2 = 0x60;
        D_shelter_b4_reservoir_80187624.field_9 = 0x40;
        D_shelter_b4_reservoir_80187624.field_8 = 1;
        D_shelter_b4_reservoir_80187624.field_A = 0x80;
        D_shelter_b4_reservoir_80187624.field_B = 0x80;
        Task_SpawnFromTable(&D_shelter_b4_reservoir_80184724, 0, 0, (s32)&D_shelter_b4_reservoir_80187624);
        return;
    }
    D_shelter_b4_reservoir_80187628 = arg0;
}

void func_shelter_b4_reservoir_8017E690(s32 arg0)
{
    switch (arg0) {
        case 0:
            D_shelter_b4_reservoir_8018492C = (s32)Task_SpawnFromTable(&D_shelter_b4_reservoir_801848EC, 1, 0x96, 0);
            break;
        case 1:
            if (D_shelter_b4_reservoir_8018492C != 0) {
                taskKill((Task*)D_shelter_b4_reservoir_8018492C);
                D_shelter_b4_reservoir_8018492C = 0;
            }
            D_shelter_b4_reservoir_80184F80 = -0x1F4;
            break;
        case 2:
            D_shelter_b4_reservoir_8018492C = (s32)Task_SpawnFromTable(&D_shelter_b4_reservoir_801848EC, 2, 0x96, 0);
            break;
        case 3:
            if (D_shelter_b4_reservoir_8018492C != 0) {
                taskKill((Task*)D_shelter_b4_reservoir_8018492C);
                D_shelter_b4_reservoir_8018492C = 0;
            }
            D_shelter_b4_reservoir_80184F82 = 0;
            break;
    }
}

void func_shelter_b4_reservoir_8017E770(s32 arg0)
{
    D_shelter_b4_reservoir_80184930->state = arg0;
}

void func_shelter_b4_reservoir_8017E780(s32 arg0)
{
    func_shelter_b4_reservoir_80182B04(10, arg0, 0x140);
}

/// Callback the room's event tables name: requests the 0x100 pulse from
/// `Gp_State1C`.
void func_shelter_b4_reservoir_8017E7A8(void)
{
    Gp_PulseState1C();
}

void func_shelter_b4_reservoir_8017E7C8(Task* arg0)
{
    arg0->msgTable = D_shelter_b4_reservoir_801848BC;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_shelter_b4_reservoir_80184F84, 0, 0, 0);
    if (GameFlag_GetNibble(0xB7) != 0) {
        D_shelter_b4_reservoir_80184F80 = -0x1F4;
    } else {
        D_shelter_b4_reservoir_80184F80 = -0x7D0;
    }
    D_shelter_b4_reservoir_80184930 = Task_SpawnFromTable(D_shelter_b4_reservoir_80184920, 0, 0, 0);
    arg0->state                     = (s32)(arg0->state + 1);
}

void func_shelter_b4_reservoir_8017E864(Task* task)
{
    func_shelter_b4_reservoir_8017E068();
    func_shelter_b4_reservoir_8017E8E4();
}

/// Runs a task through the room's three-entry state table
/// `D_shelter_b4_reservoir_8017D5C4`, copied onto the stack first.
void func_shelter_b4_reservoir_8017E88C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b4_reservoir_8017D5C4;
    sp.funcs[task->state](task);
}

void func_shelter_b4_reservoir_8017E8E4(void)
{
}

void func_shelter_b4_reservoir_8017E8EC(Task* task)
{
    RoomWaterSurface* p = D_shelter_b4_reservoir_80184F90;

    if (D_8007217B == 0) {
        D_shelter_b4_reservoir_80187630 = (u8*)D_8005C374 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_shelter_b4_reservoir_80187630 = (u8*)D_8005C370 + gDisplayState.otBuffer * 0xC000;
    }
    if (gGameSession->at4.loc.view != 0xA) {
        p->depth = 0x2328 - (((D_shelter_b4_reservoir_80184F80 + 0x7D0) * 0x31) >> 5);
        func_shelter_b4_reservoir_8017EA00(task);
        func_shelter_b4_reservoir_8017EE04(task);
        func_shelter_b4_reservoir_8017F23C(task);
        return;
    }
    if (D_shelter_b4_reservoir_80184F82 < -0x708) {
        D_shelter_b4_reservoir_80184F82 = -0x708;
    } else if (D_shelter_b4_reservoir_80184F82 > 0) {
        D_shelter_b4_reservoir_80184F82 = 0;
    }
    func_shelter_b4_reservoir_8017F674(task);
}

/// Draws each surface in `D_shelter_b4_reservoir_80184F90` as a strip of 32
/// flat semi-transparent quads laid along Z, projected through the view
/// matrix, each followed by a draw-mode packet selecting blend mode 2. Quads
/// the projection flags as invalid are skipped. `task` is unused.
void func_shelter_b4_reservoir_8017EA00(Task* task)
{
    SVECTOR           v0, v1, v2, v3;
    s32               sxy0, sxy1, sxy2, sxy3;
    s32               p, flag;
    u8*               head;
    _SurfaceScratch*  s;
    RoomWaterSurface* e;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;

    e                 = D_shelter_b4_reservoir_80184F90;
    head              = *(u8**)0x1F8003FC;
    gGfxViewCoord.flg = 0;
    *(u8**)0x1F8003FC = head - 0xC;
    s                 = (_SurfaceScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    ((_SurfaceScratch*)(head - 0xC))->y = D_shelter_b4_reservoir_80184F80;
    for (; e->count != -1; e++) {
        s->dx   = e->width;
        s->step = e->depth / 32;
        s->x    = e->x + D_shelter_b4_reservoir_80185020;
        s->z    = e->z;
        for (i = 0; i < 32; i++) {
            v0.vx = s->x;
            v0.vy = s->y;
            v0.vz = s->z + s->step * i;
            v1.vx = s->x;
            v1.vy = s->y;
            v1.vz = s->z + s->step * (i + 1);
            s->dy = 0;
            v2.vx = s->x + s->dx;
            v2.vy = s->y + s->dy;
            v2.vz = s->z + s->step * i;
            s->dy = 0;
            v3.vx = s->x + s->dx;
            v3.vy = s->y + s->dy;
            v3.vz = s->z + s->step * (i + 1);
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                            = (POLY_F4*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(poly + 1);
                setlen(poly, 5);
                setcode(poly, 0x2A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0x80;
                poly->g0         = 0;
                poly->b0         = 0;
                addPrim((u_long*)(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt),
                        poly);
                dr                              = (DR_MODE*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

/// Same strip renderer as `func_shelter_b4_reservoir_8017EA00`, driven by
/// `D_shelter_b4_reservoir_80184FA8`: each surface's `field_8` gives its quad
/// count, and its X is used as stored rather than offset. `task` is unused.
void func_shelter_b4_reservoir_8017EE04(Task* task)
{
    SVECTOR           v0, v1, v2, v3;
    s32               sxy0, sxy1, sxy2, sxy3;
    s32               p, flag;
    u8*               head;
    _SurfaceScratch*  s;
    RoomWaterSurface* e;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;

    e                 = D_shelter_b4_reservoir_80184FA8;
    head              = *(u8**)0x1F8003FC;
    gGfxViewCoord.flg = 0;
    *(u8**)0x1F8003FC = head - 0xC;
    s                 = (_SurfaceScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    ((_SurfaceScratch*)(head - 0xC))->y = D_shelter_b4_reservoir_80184F80;
    for (; e->count != -1; e++) {
        s->dx   = e->width;
        s->step = e->depth / e->count;
        s->x    = e->x;
        s->z    = e->z;
        for (i = 0; i < e->count; i++) {
            v0.vx = s->x;
            v0.vy = s->y;
            v0.vz = s->z + s->step * i;
            v1.vx = s->x;
            v1.vy = s->y;
            v1.vz = s->z + s->step * (i + 1);
            s->dy = 0;
            v2.vx = s->x + s->dx;
            v2.vy = s->y + s->dy;
            v2.vz = s->z + s->step * i;
            s->dy = 0;
            v3.vx = s->x + s->dx;
            v3.vy = s->y + s->dy;
            v3.vz = s->z + s->step * (i + 1);
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                            = (POLY_F4*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(poly + 1);
                setlen(poly, 5);
                setcode(poly, 0x2A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0x80;
                poly->g0         = 0;
                poly->b0         = 0;
                addPrim((u_long*)(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt),
                        poly);
                dr                              = (DR_MODE*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

/// Strip renderer like `func_shelter_b4_reservoir_8017EE04`, driven by
/// `D_shelter_b4_reservoir_80184FCC`, but laid along X instead of Z: each
/// surface's `field_4` is divided into `field_8` quads, and `field_6` is the
/// extent along Z. The scratch fields `dx` and `step` therefore hold the X step
/// and the Z extent here. `task` is unused.
void func_shelter_b4_reservoir_8017F23C(Task* task)
{
    SVECTOR           v0, v1, v2, v3;
    s32               sxy0, sxy1, sxy2, sxy3;
    s32               p, flag;
    u8*               head;
    _SurfaceScratch*  s;
    RoomWaterSurface* e;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;

    e                 = D_shelter_b4_reservoir_80184FCC;
    head              = *(u8**)0x1F8003FC;
    gGfxViewCoord.flg = 0;
    *(u8**)0x1F8003FC = head - 0xC;
    s                 = (_SurfaceScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    ((_SurfaceScratch*)(head - 0xC))->y = D_shelter_b4_reservoir_80184F80;
    for (; e->count != -1; e++) {
        s->dx   = e->width / e->count;
        s->step = e->depth;
        s->x    = e->x;
        s->z    = e->z;
        for (i = 0; i < e->count; i++) {
            v0.vx = s->x + s->dx * i;
            v0.vy = s->y;
            v0.vz = s->z;
            v1.vx = s->x + s->dx * (i + 1);
            v1.vy = s->y;
            v1.vz = s->z;
            s->dy = 0;
            v2.vx = s->x + s->dx * i;
            v2.vy = s->y + s->dy;
            v2.vz = s->z + s->step;
            s->dy = 0;
            v3.vx = s->x + s->dx * (i + 1);
            v3.vy = s->y + s->dy;
            v3.vz = s->z + s->step;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                            = (POLY_F4*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(poly + 1);
                setlen(poly, 5);
                setcode(poly, 0x2A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0x80;
                poly->g0         = 0;
                poly->b0         = 0;
                addPrim((u_long*)(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt),
                        poly);
                dr                              = (DR_MODE*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

/// Strip renderer like `func_shelter_b4_reservoir_8017EE04`, driven by
/// `D_shelter_b4_reservoir_80184FE4` and drawn at height
/// `D_shelter_b4_reservoir_80184F82` instead of `D_shelter_b4_reservoir_80184F80`.
/// The quads are tinted by that height: blue is `-height * 16 / 225` and green
/// a quarter of it, so they brighten as the level sinks. `task` is unused.
void func_shelter_b4_reservoir_8017F674(Task* task)
{
    SVECTOR           v0, v1, v2, v3;
    s32               sxy0, sxy1, sxy2, sxy3;
    s32               p, flag;
    u8*               head;
    _SurfaceScratch*  s;
    RoomWaterSurface* e;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;
    u8                c;

    e                 = D_shelter_b4_reservoir_80184FE4;
    head              = *(u8**)0x1F8003FC;
    gGfxViewCoord.flg = 0;
    *(u8**)0x1F8003FC = head - 0xC;
    s                 = (_SurfaceScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    ((_SurfaceScratch*)(head - 0xC))->y = D_shelter_b4_reservoir_80184F82;
    c                                   = -(D_shelter_b4_reservoir_80184F82 * 16) / 225;
    for (; e->count != -1; e++) {
        s->dx   = e->width;
        s->step = e->depth / e->count;
        s->x    = e->x;
        s->z    = e->z;
        for (i = 0; i < e->count; i++) {
            v0.vx = s->x;
            v0.vy = s->y;
            v0.vz = s->z + s->step * i;
            v1.vx = s->x;
            v1.vy = s->y;
            v1.vz = s->z + s->step * (i + 1);
            s->dy = 0;
            v2.vx = s->x + s->dx;
            v2.vy = s->y + s->dy;
            v2.vz = s->z + s->step * i;
            s->dy = 0;
            v3.vx = s->x + s->dx;
            v3.vy = s->y + s->dy;
            v3.vz = s->z + s->step * (i + 1);
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                            = (POLY_F4*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(poly + 1);
                setlen(poly, 5);
                setcode(poly, 0x2A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0;
                poly->g0         = c >> 2;
                poly->b0         = c;
                addPrim((u_long*)(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt),
                        poly);
                dr                              = (DR_MODE*)D_shelter_b4_reservoir_80187630;
                D_shelter_b4_reservoir_80187630 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)(otz + 1) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

void func_shelter_b4_reservoir_8017FADC(Task* task)
{
    TaskFunc states[2] = { func_shelter_b4_reservoir_8017FB44, func_shelter_b4_reservoir_8017E8EC };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_reservoir_80184F80;
}

/// First state of the water task: clears the session counter the current
/// display mode selects (`field_80` when `D_8007217B` is zero, `field_7E`
/// otherwise) and moves on to the per-frame state.
void func_shelter_b4_reservoir_8017FB44(Task* arg0)
{
    if (D_8007217B == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_b4_reservoir_8017FB84(Task* task)
{
    _ShelterB4ReservoirWork* work;
    Task*                    player;
    GsCOORDINATE2*           root;
    GsCOORDINATE2*           c;
    GsCOORDINATE2            coord;
    MATRIX*                  view;
    GsCOORDINATE2*           parent;
    s32                      i;
    s32                      offset;
    s32                      roll;

    work   = task->spawnArg2;
    player = gameGetPtrSlot(3);
    root   = ((TmdObject*)player->extra)->coords;
    if (task->state == 0) {
        D_8011574C  = 0x60172;
        D_80115738  = 0x60173;
        D_80115734  = 0x60225;
        D_80115730  = 0x60230;
        D_80115754  = 0x6023B;
        task->state = 1;
        for (i = 0; i < 10; i++) {
            work->field_24                        = RAND() & 0x1C0;
            work->field_26                        = (RAND() & 0x1FF) + (i << 9);
            D_shelter_b4_reservoir_80187634[i].vx = D_shelter_b4_reservoir_80185094.vx + 0x800;
            D_shelter_b4_reservoir_80187634[i].vy =
                D_shelter_b4_reservoir_80185094.vy + ((work->field_24 * rsin(work->field_26)) >> 12);
            D_shelter_b4_reservoir_80187634[i].vz =
                D_shelter_b4_reservoir_80185094.vz + ((work->field_24 * rcos(work->field_26)) >> 12);
        }
        D_shelter_b4_reservoir_80187684.field_0 = 0;
        D_shelter_b4_reservoir_80187684.field_2 = 0;
        D_shelter_b4_reservoir_80187684.field_4 = 0;
        for (i = 0; i < 2; i++) {
            c                                     = &((TmdObject*)player->extra)->coords[i * 3 + 14];
            D_shelter_b4_reservoir_801850AC[i].vx = c->workm.t[0];
            D_shelter_b4_reservoir_801850AC[i].vy = c->workm.t[1];
            D_shelter_b4_reservoir_801850AC[i].vz = c->workm.t[2];
        }
    }
    if (Gp_State1C->eventState == 0) {
        if (GameFlag_GetNibble(0xB7) != 0) {
            if (gGameSession->waterY < root->coord.t[1] && work->field_22 != 0) {
                for (i = 0; i < 2; i++) {
                    parent = &gGfxViewCoord;
                    view   = &Gfx_ViewWorldMtx;
                    c      = &((TmdObject*)player->extra)->coords[i * 3 + 14];
                    Gp_UpdateCoord(c);
                    work->field_26 = ABS(D_shelter_b4_reservoir_801850AC[i].vx - c->workm.t[0]) +
                                     ABS(D_shelter_b4_reservoir_801850AC[i].vy - c->workm.t[1]) +
                                     ABS(D_shelter_b4_reservoir_801850AC[i].vz - c->workm.t[2]) + 0x20;
                    Gp_WorldToLocal(view, &c->workm, &coord.coord);
                    coord.sub        = parent;
                    coord.coord.t[1] = gGameSession->waterY;
                    coord.flg        = 0;
                    Gp_UpdateCoord(&coord);
                    if ((s32)(RAND() & 0x1FF) < work->field_26) {
                        Gp_SpawnEff(D_8011574C, &coord, 0x40, NULL);
                    }
                    work->field_26 -= 0x20;
                    if ((s32)(RAND() & 0x1FF) < work->field_26) {
                        Gp_SpawnEff(D_80115738, &coord, 0x1202180, NULL);
                    }
                    D_shelter_b4_reservoir_801850AC[i].vx = c->workm.t[0];
                    D_shelter_b4_reservoir_801850AC[i].vy = c->workm.t[1];
                    D_shelter_b4_reservoir_801850AC[i].vz = c->workm.t[2];
                }
            }
            work->field_22++;
        }
        if (D_shelter_b4_reservoir_80187684.field_0 != 0 && D_shelter_b4_reservoir_80187684.field_2 != 0) {
            for (i = 0; i < D_shelter_b4_reservoir_80187684.field_0; i++) {
                if ((RAND() & 0x1F) == 0) {
                    work->field_24                        = RAND() & 0x1C0;
                    work->field_26                        = (RAND() & 0x1FF) + (i << 9);
                    D_shelter_b4_reservoir_80187634[i].vx = D_shelter_b4_reservoir_80185094.vx + 0x800;
                    D_shelter_b4_reservoir_80187634[i].vy =
                        D_shelter_b4_reservoir_80185094.vy + ((work->field_24 * rsin(work->field_26)) >> 12);
                    D_shelter_b4_reservoir_80187634[i].vz =
                        D_shelter_b4_reservoir_80185094.vz + ((work->field_24 * rcos(work->field_26)) >> 12);
                }
                if ((u16)(RAND() % 100) < D_shelter_b4_reservoir_80187684.field_2) {
                    offset = D_shelter_b4_reservoir_80187684.field_4;
                    roll   = (RAND() & 0x10FF) + 0x502000;
                    Gp_SpawnEff(0x600AA, NULL, offset + roll, &D_shelter_b4_reservoir_80187634[i]);
                }
            }
        }
    }
    if ((u8)Gp_GetViewIndex() == 10) {
        D_shelter_b4_reservoir_8018509C[1].vy = D_shelter_b4_reservoir_80184F82;
        if ((RAND() & 1) == 0) {
            Gp_SpawnEff(D_80115738, NULL, (RAND() & 0x1000) + 0x4A03600, &D_shelter_b4_reservoir_8018509C[0]);
        }
        if ((RAND() & 1) == 0) {
            Gp_SpawnEff(D_80115738, NULL, (RAND() & 0x10FF) | 0x11602300, &D_shelter_b4_reservoir_8018509C[1]);
        }
        if ((RAND() & 3) == 0) {
            Gp_SpawnEff(D_8011574C, NULL, (RAND() & 0x7F) | 0x80, &D_shelter_b4_reservoir_8018509C[1]);
        }
    }
    switch ((u8)Gp_GetViewIndex()) {
        case 2:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185074, 0x200, 0x444);
            break;
        case 4:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[0], 0x200, 0x222);
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[6], 0x200, 0x444);
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[8], 0x200, 0x333);
            if (GameFlag_GetNibble(0xB7) != 0) {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 5:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185024[0], 0x200, 0x444);
            if (GameFlag_GetNibble(0xB7) != 0) {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 6:
            if (GameFlag_GetNibble(0xB7) != 0) {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x504C);
            } else {
                func_shelter_b4_reservoir_80182134(&D_shelter_b4_reservoir_80185024[12], 0x100, 0x5C40);
            }
            break;
        case 7:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185034, 0x200, 0x444);
            break;
        case 3:
        case 9:
            func_shelter_b4_reservoir_801818F0(&D_shelter_b4_reservoir_80185044, 0x200, 0x444);
            break;
    }
}

/// Effect task drawing a flat quad through `func_shelter_b4_reservoir_80180530`
/// that spreads and fades. Its first frame sets the size (`field_26`) from the
/// low 12 bits of the spawn argument and the brightness (`field_24`) to 0x40,
/// and turns the coordinate to a random angle about Y. Every frame after that
/// grows the size by 0x20 and dims the brightness by 2, releasing the effect
/// once the brightness drops under 2. While `Gp_State1C->eventState` is
/// non-zero it only redraws at the current values, releasing from state 4.
void func_shelter_b4_reservoir_801803DC(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_b4_reservoir_80180530(coord, (s16)work->field_26, (s16)work->field_24);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->field_22++;
        if (task->state == 0) {
            work->field_24 = 0x40;
            work->field_26 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
            coord->flg  = 0;
            task->state = 1;
        }
        work->field_26 += 0x20;
        func_shelter_b4_reservoir_80180530(coord, (s16)work->field_26, (s16)work->field_24);
        work->field_24 -= 2;
        if ((s16)work->field_24 < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a flat quad lying in the XZ plane of `arg0`: the four unit corners of
/// `D_80111E38` scaled by `arg1`, turned by the coordinate's world matrix and
/// moved to its position, then projected through `GsWSMATRIX`. Unless the
/// projection flags an error it queues one semi-transparent textured quad
/// (tpage 0x2B, clut 0x43D1) shaded grey at `arg2`.
void func_shelter_b4_reservoir_80180530(GsCOORDINATE2* arg0, s32 arg1, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    MATRIX*        wm;
    POLY_FT4*      prim;
    s32            prod;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    wm  = &arg0->workm;
    v   = block->vec;
    tbl = D_80111E38;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        TOUCH_REG(v);
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(wm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D1;
        prim->v0    = 0x38;
        prim->v1    = 0x38;
        setRGB0(prim, arg2, arg2, arg2);
        prim->u0 = 0;
        prim->u1 = 0x37;
        prim->u2 = 0;
        prim->v2 = 0x6F;
        prim->u3 = 0x37;
        prim->v3 = 0x6F;
        setSemiTrans(prim, 1);
        prim->x0 = *(u16*)&block->sxy0.vx;
        prim->y0 = *(u16*)&block->sxy0.vy;
        prim->x1 = *(u16*)&block->sxy1.vx;
        prim->y1 = *(u16*)&block->sxy1.vy;
        prim->x2 = *(u16*)&block->sxy2.vx;
        prim->y2 = *(u16*)&block->sxy2.vy;
        prim->x3 = *(u16*)&block->sxy3.vx;
        prim->y3 = *(u16*)&block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
}

/// Particle effect task, drawn as the spinning sprite of
/// `func_shelter_b4_reservoir_80180D20` or, when the spawn argument's top
/// nibble is set, the standing sprite of `func_shelter_b4_reservoir_8018110C`.
/// Its first frame takes the size from the argument's low 12 bits, a random
/// angle, and the ticks per animation frame from bits 12-15. Unless the work
/// already carries a velocity it picks one by the kind in bits 24-27 - none,
/// a random upward burst, a random spray, a narrow upward jet, or the work's
/// stored direction - scaled to the speed in bits 16-23 (0x40 when zero).
/// Every later tick draws, moves the coordinate by the velocity with gravity
/// pulling it down, and releases the effect after animation frame 7. While
/// `Gp_State1C->eventState` is non-zero it only draws, releasing from state 4.
void func_shelter_b4_reservoir_80180864(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            kind;
    s32            step;
    s32            state;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_shelter_b4_reservoir_80180D20(coord, (s16)work->field_20, (s16)work->field_24, (s16)work->field_26);
            } else {
                func_shelter_b4_reservoir_8018110C(coord, (s16)work->field_20, (s16)work->field_24);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    Gp_UpdateCoord(coord);
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->field_28 = step;
            work->field_22 = 0;
            state          = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->field_10.vx | (u16)work->field_10.vy | (u16)work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->field_2A = level;
                kind           = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->field_2A = 0;
                        break;
                    case 1:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->field_10.vx = work->field_18;
                        work->field_10.vy = work->field_1A;
                        work->field_10.vz = work->field_1C;
                        break;
                }
                vec = &work->field_10;
                VectorNormalSS(vec, vec);
                gte_lddp(work->field_2A);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->field_2A = 0x40;
            }
            return;
        case 1:
            func_shelter_b4_reservoir_80180D20(coord, (s16)work->field_20, (s16)work->field_24, (s16)work->field_26);
            break;
        case 2:
            func_shelter_b4_reservoir_8018110C(coord, (s16)work->field_20, (s16)work->field_24);
            break;
        default:
            return;
    }
    if ((s16)work->field_2A != 0) {
        coord->coord.t[0] += work->field_10.vx;
        coord->coord.t[1] += work->field_10.vy;
        coord->coord.t[2] += work->field_10.vz;
        coord->flg         = 0;
        work->field_10.vy += 6;
    }
    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a spinning sprite at the coordinate's world position: one
/// semi-transparent textured quad (tpage 0x2B, clut 0x43D3) centred on the
/// projected point, unless the projection flags an error. `arg1` picks the
/// 32-texel-wide frame at U `arg1 * 32` in the strip at V 0xE0..0xFF, `arg2` is
/// the size (a screen half-extent of `arg2 * 31 / otz`) and `arg3` the angle
/// the corners are turned by.
void func_shelter_b4_reservoir_80180D20(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw27Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    s32                u0;
    s32                ang;
    s32                ang2;
    u16                vz;

    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw27Scratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (RoomDraw27Scratch*)(head - 0x1C);
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    vz                                          = *(u16*)&arg0->workm.t[2];
    *scratch                                    = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw27Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw27Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw27Scratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ang            = (s16)arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u0          = (s16)arg1 << 5;
        setUV4(prim, u0, 0xE0, u0 + 0x1F, 0xE0, u0, 0xFF, u0 + 0x1F, 0xFF);
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

/// Draws a sprite at the coordinate's world position: one semi-transparent
/// textured quad (tpage 0x2B, clut 0x43D2) around the projected point, unless
/// the projection flags an error. `arg1` picks one of eight 56-texel frames,
/// four across and two down from V 0x70. `arg2` is the size, a screen
/// half-extent of `arg2 * 55 / otz`; the quad stands on the point, reaching one
/// and a half extents above it and half an extent below.
void func_shelter_b4_reservoir_8018110C(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw23Scratch* block;
    RoomDraw23Scratch* p;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s16                cell;
    s16                cell2;
    s32                u0;
    s32                vbase;
    s32                v0u;
    s32                x;
    s16                xy;
    u16                vy;
    u16                vz;

    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw23Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (RoomDraw23Scratch*)(head - 0x18);
    vy                                          = *(u16*)&arg0->workm.t[1];
    SOFT_TOUCH_REG_USE(block, vy);
    p = block;
    SOFT_TOUCH_REG(p);
    p->vec.vy = vy;
    vz        = *(u16*)&arg0->workm.t[2];
    *scratch  = block;
    p->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&p->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw23Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw23Scratch*)(head - 0x18))->flag);
    if (p->flag >= 0) {
        gte_stszotz(&((RoomDraw23Scratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        cell        = arg1 % 4;
        u0          = cell * 0x38;
        prim->u0    = u0;
        cell2       = arg1 % 8;
        x           = cell2;
        vbase       = (x / 4) * 0x38;
        SOFT_USE_REG(x);
        v0u = vbase + 0x70;
        SOFT_BARRIER();
        head     = (u8*)(u0 + 0x37);
        prim->v0 = v0u;
        prim->u1 = (s32)head;
        prim->v1 = v0u;
        prim->u2 = u0;
        SOFT_BARRIER();
        prim->v2 = vbase - 0x59;
        prim->u3 = (s32)head;
        SOFT_BARRIER();
        prim->v3      = vbase - 0x59;
        block->radius = (arg2 * 0x37) / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = (*(u16*)&block->sy - *(u16*)&block->radius) - (block->radius >> 1);
        ds            = &gDisplayState;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = *(u16*)&block->sy + (block->radius >> 1);
        prim->y3      = xy;
        prim->y2      = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

void func_shelter_b4_reservoir_801813F0(Task* task)
{
    RoomEffWork*   work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    s16            f2a;
    u32            rng;

    if (Gp_State1C->eventState != 0) {
        func_shelter_b4_reservoir_80181668(coord, work->field_20, (s16)work->field_24);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = (*(u16*)&task->spawnArg1) & 0xFFF;

            if (task->spawnArg1 & 0xF000) {
                work->field_28 = (task->spawnArg1 >> 12) & 0x7;
            } else {
                work->field_28 = 1;
            }

            work->field_22 = 0;
            task->state    = 1;

            if (task->spawnArg1 & 0xFF0000) {
                f2a = (task->spawnArg1 >> 16) & 0xFF;
            } else {
                f2a = 0x40;
            }

            work->field_2A    = f2a;
            work->field_10.vy = 0;
            work->field_10.vz = 0;
            rng               = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState       = rng;
            work->field_10.vx = -((rng >> 16) & 0x3F) - 0x40;
            VectorNormalSS(&work->field_10, &work->field_10);

            gte_lddp(work->field_2A);
            gte_ldsv(&work->field_10);
            gte_gpf12();
            gte_stsv(&work->field_10);
            break;
        case 1:
            func_shelter_b4_reservoir_80181668(coord, work->field_20, (s16)work->field_24);
            if ((s16)work->field_2A != 0) {
                coord->coord.t[0] += work->field_10.vx;
                coord->coord.t[1] += work->field_10.vy;
                coord->coord.t[2] += work->field_10.vz;
                coord->flg         = 0;
            }
            if (((s16)work->field_22 % (s16)work->field_28) == 0) {
                work->field_20++;
                if ((s16)work->field_20 >= 6) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues one semi-transparent shade-tex
/// `POLY_FT4` (tpage 0x2B, clut 0x4393). `frame` selects one of six 32-texel
/// UV columns at u = `(frame % 6) * 32 + 0x40`, v = 0x40..0x5F. `size` is a
/// half-extent; the on-screen radius is `size * 31 / otz`, and the quad is
/// axis-aligned about the projected point.
void func_shelter_b4_reservoir_80181668(GsCOORDINATE2* coord, u16 frame, s16 size)
{
    void**             scratch;
    u8*                head;
    RoomDraw14Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    DisplayState*      ds;
    s32                col;
    s16                xy;
    u16                vz;

    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                       = (RoomDraw14Scratch*)(head - 0x18);
    block->vec.vy                               = *(u16*)&coord->workm.t[1];
    vz                                          = *(u16*)&coord->workm.t[2];
    *scratch                                    = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage   = 0x2B;
        prim->clut    = 0x4393;
        prim->v0      = 0x40;
        prim->v1      = 0x40;
        prim->v2      = 0x5F;
        prim->v3      = 0x5F;
        col           = (u16)(frame % 6) << 5;
        prim->u0      = col + 0x40;
        prim->u2      = col + 0x40;
        prim->u1      = col + 0x5F;
        prim->u3      = col + 0x5F;
        block->radius = (size * 31) / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = *(u16*)&block->sy - *(u16*)&block->radius;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = *(u16*)&block->sy + *(u16*)&block->radius;
        prim->y3      = xy;
        prim->y2      = xy;
        ds            = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

/// Draws a glowing capsule between the points `arg0[0]` and `arg0[1]`,
/// projected through `Gfx_ViewWorldMtx`; nothing is drawn unless both project.
/// Each end is a half-disc of screen radius `arg1 * 64 / otz` and the two are
/// joined by a band, built from gouraud quads lit at the centre line and black
/// at the rim, in two 0x400 steps around the angle between the projected
/// points. `arg2` is the colour as three 4-bit channels (0xRGB), brightened
/// slightly on odd frames.
void func_shelter_b4_reservoir_801818F0(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw08Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t3;
    s32                t2;
    s32                limit;
    s32                angStart;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    s32                scaled;
    s32                sum;
    u8                 r;
    u8                 g;
    u8                 b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (RoomDraw08Scratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx0);
    gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx1);
        gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((RoomDraw08Scratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((RoomDraw08Scratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            SCHED_BARRIER();
            ang    = (s16)ang;
            blend  = (*(u8*)&ds->animFrame & 1) * 8;
            packed = arg2 << 16;
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend | tr;
            g      = blend | tg;
            b      = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim = (POLY_G4*)gGpuPrimCursor;
                    USE_REG(prim);
                    t   = ang - angStart;
                    t <<= 1;
                    TOUCH_REG(t);
                    sum            = angStart + t;
                    t              = sum;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();
                    t3   = ang + 0x800;
                    prim = (POLY_G4*)gGpuPrimCursor;
                    SOFT_BARRIER();
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Draws a star-shaped glow at the world point `arg0`, projected through
/// `Gfx_ViewWorldMtx`, unless the projection flags an error. A disc of screen
/// radius `arg1 * 64 / otz` is built from gouraud wedges lit at the centre and
/// black at the rim, alternating half brightness at full radius with full
/// brightness at half radius; four spikes at half brightness reach out
/// between them. `arg2` is the colour as three 4-bit channels (0xRGB), and its
/// top nibble is the shift of a small brightening applied on odd frames.
void func_shelter_b4_reservoir_80182134(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                ua;
    s32                ub;
    s32                uc;
    s32                frame;
    s32                packed;
    s32                blend;
    s32                r;
    s32                g;
    s32                b;
    s32                outer;
    s32                inner;
    s32                hr;
    s32                hg;
    s32                hb;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        tmp     = (*scratch = (u8*)*scratch - 0x14);
        block   = (RoomDraw05Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1        <<= 16;
        arg1        >>= 16;
        outer         = (arg1 * 64) / block->otz;
        frame         = gDisplayState.animFrame;
        block->rOuter = outer;
        inner         = (arg1 * 8) / block->otz;
        ang           = 0;
        packed        = arg2 << 16;
        blend         = (frame & 1) << (packed >> 28);
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->rInner = inner;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            hr = (u8)r >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            hg = (u8)g >> 1;
            hb = (u8)b >> 1;
            setRGB2(prim, hr, hg, hb);
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
            setRGB2(prim, r, g, b);
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
        r   = (u8)hr;
        g   = (u8)hg;
        b   = (u8)hb;
        do {
            ua             = ang - 0x400;
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ua)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(ua)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            ub       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ub)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(ub)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ub)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ub)) >> 11);
            uc       = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(uc)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(uc)) >> 12);
            ang      = uc;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}

void func_shelter_b4_reservoir_80182B04(s16 arg0, u16 arg1, s16 arg2)
{
    D_shelter_b4_reservoir_80187684.field_0 = arg0;
    D_shelter_b4_reservoir_80187684.field_2 = arg1;
    D_shelter_b4_reservoir_80187684.field_4 = arg2;
}

/// Effect task of a glowing disc attached to its parent at the work's
/// position. State 1 grows the disc and, every fourth tick, spawns effect
/// `D_80115730` at a random joint of the player's model; state 2 adds a
/// flickering second disc at half brightness; state 3 drifts the disc away,
/// shrinking and fading it inside an expanding ring, then releases the effect.
/// The spawn argument picks the disc's colour shifts from
/// `D_shelter_b4_reservoir_801850BC`.
void func_shelter_b4_reservoir_80182B1C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpEffWork*     spawned;
    MATRIX*        mtx;
    u8             col[4];

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            coord->sub                   = mem->parent;
            mtx                          = &coord->coord;
            *(s32*)&coord->coord.m[0][0] = 0x1000;
            *(s32*)&mtx->m[0][2]         = 0;
            *(s32*)&mtx->m[1][1]         = 0x1000;
            *(s32*)&mtx->m[2][0]         = 0;
            mtx->m[2][2]                 = 0x1000;
            coord->coord.t[0]            = mem->pos.vx;
            coord->coord.t[1]            = mem->pos.vy;
            coord->coord.t[2]            = mem->pos.vz;
            coord->flg                   = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &((TmdObject*)player->extra)->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], (s32)coord, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][2];
            func_shelter_b4_reservoir_80183940(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][2];
            func_shelter_b4_reservoir_80183940(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_shelter_b4_reservoir_80183940(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b4_reservoir_801850BC[arg0->spawnArg1][2];
            func_shelter_b4_reservoir_80183940(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = (u16)mem->scale >> 1;
            col[2] = (u16)mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            }
            mem->period       += 8;
            coord->workm.t[0] += mem->move.vx;
            coord->workm.t[1] += mem->move.vy;
            coord->workm.t[2] += mem->move.vz;
            func_shelter_b4_reservoir_8018351C(coord, (s16)(mem->period + 0x80), 0x100, col);
            mem->angle -= 0x10;
            if (mem->scale > 0x10) {
                mem->scale -= 0x10;
                break;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

/// Effect task that drifts toward the coordinate in `spawnArg1`. Its first
/// frame turns the world-space offset to that coordinate into the effect's
/// own frame and keeps 0xCC/0x1000 of it as the per-frame step. Every frame
/// after that moves by the step and, on odd ticks, draws the next frame of
/// `func_shelter_b4_reservoir_80183298`, releasing the effect at tick 20. While
/// `Gp_State1C->eventState` is non-zero it does nothing but release from state
/// 4.
void func_shelter_b4_reservoir_80183074(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    VECTOR         delta;

    work   = task->spawnArg2;
    coord  = ((TmdObject*)task->extra)->coords;
    target = (GsCOORDINATE2*)task->spawnArg1;
    if (Gp_State1C->eventState == 0) {
        work->field_22++;
        switch (task->state) {
            case 0:
                delta.vx = target->workm.t[0] - coord->workm.t[0];
                delta.vy = target->workm.t[1] - coord->workm.t[1];
                delta.vz = target->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &delta, &delta);
                work->field_18 = delta.vx;
                work->field_1A = delta.vy;
                work->field_1C = delta.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->field_18);
                gte_rtv0();
                gte_stsv(&work->field_18);
                gte_lddp(0xCC);
                gte_ldsv(&work->field_18);
                gte_gpf12();
                gte_stsv(&work->field_18);
                task->state = 1;
                break;
            case 1:
                coord->coord.t[0] += (s16)work->field_18;
                coord->coord.t[1] += (s16)work->field_1A;
                coord->coord.t[2] += (s16)work->field_1C;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->field_22 & 1) {
                    func_shelter_b4_reservoir_80183298(coord, (s16)++work->field_20, 0x200, 0x80);
                }
                if ((s16)work->field_22 >= 20) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    } else if (Gp_State1C->eventState >= 4) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Draws a sprite at the coordinate's world position: one semi-transparent
/// textured quad (tpage 0x2A, clut 0x42CB) centred on the projected point,
/// unless the projection flags an error. `arg1` picks one of four 24-texel
/// frames from U 0x60, `arg2` is the size (a screen half-extent of
/// `arg2 * 23 / (otz + 1)`) and `arg3` the grey level it is shaded with.
void func_shelter_b4_reservoir_80183298(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw14Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    DisplayState*      ds;
    s32                tex;
    s32                u0;
    s32                u1;
    s32                sarg;
    s32                t;
    s16                xy;
    u16                vz;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (RoomDraw14Scratch*)(head - 0x18);
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    vz                                          = *(u16*)&arg0->workm.t[2];
    *scratch                                    = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42CB;
        t           = (tex & 3) * 24;
        u0          = t + 0x60;
        u1          = t + 0x77;
        SOFT_USE_REG(u1);
        prim->u0 = u0;
        prim->u2 = u0;
        prim->v2 = 0x17;
        prim->v3 = 0x17;
        sarg     = (s16)arg2;
        prim->u1 = u1;
        prim->u3 = u1;
        t        = sarg * 24;
        setRGB0(prim, arg3, arg3, arg3);
        prim->v0      = 0;
        prim->v1      = 0;
        block->radius = (t - sarg) / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = *(u16*)&block->sy - *(u16*)&block->radius;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = *(u16*)&block->sy + *(u16*)&block->radius;
        prim->y3      = xy;
        prim->y2      = xy;
        ds            = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

/// Draws a glowing ring around the coordinate's projected position, unless the
/// projection flags an error: sixteen gouraud quads, black at screen radius
/// `arg1 * 64 / (otz + 1)` and coloured `rgb` at
/// `(arg1 + arg2) * 64 / (otz + 1)`.
void func_shelter_b4_reservoir_8018351C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw07Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    register s32       sum asm("a1");
    register s32       otz asm("v0");
    register s32       rOuter asm("a0");
    register s32       rInner asm("v1");
    register u8*       color asm("s4");
    s32                t;
    u16                vz;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw07Scratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw07Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw07Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw07Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw07Scratch*)(head - 0x1C))->otz);
        USE_REG(head);
        otz        = block->otz + 1;
        rOuter     = ((s16)saved * 64) / otz;
        rInner     = (s16)sum * 64;
        block->otz = otz;
        SOFT_BARRIER();
        rInner        = rInner / otz;
        ang           = 0;
        block->rOuter = rOuter;
        block->rInner = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(t)) >> 12);
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Draws a glowing disc at the coordinate's world position, unless the
/// projection flags an error: eight gouraud wedges coloured `rgb` at the
/// projected centre and black at the rim, of screen radius
/// `arg1 * 64 / (otz + 1)`.
void func_shelter_b4_reservoir_80183940(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
{
    void**             scratch;
    u8*                head;
    RoomDraw10Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register s32       ang2 asm("s1");
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw10Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw10Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw10Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw10Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw10Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        block->otz++;
        block->step = ((s16)arg1 * 64) / block->otz;
        ang         = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->step * rcos(ang)) >> 12);
            ang2     = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            ang2     = ang + 0x200;
            prim->x3 = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
            ang      = ang2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Burst effect task: each frame draws a disc and the glow of
/// `func_shelter_b4_reservoir_80183E80` at a growing size, and while its echo
/// brightness lasts a widening ring fading out behind them. Once the echo is
/// spent the main brightness runs down, and the effect is released when it
/// falls under 0x18.
void func_shelter_b4_reservoir_80183CD4(Task* arg0)
{
    u8             rgb[3];
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            step;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x80;
            mem->period = 0xE0;
            mem->step   = 0x80;
            arg0->state = 1;
        }
        Gp_UpdateCoord(coord);
        rgb[0]     = mem->scale;
        rgb[1]     = (u16)mem->scale >> 1;
        rgb[2]     = (u16)mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_shelter_b4_reservoir_80183940(coord, (s16)(step * 2), rgb);
        func_shelter_b4_reservoir_80183E80(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = (u16)mem->period >> 1;
            rgb[2] = (u16)mem->period >> 2;
            func_shelter_b4_reservoir_8018351C(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x30;
            return;
        }
        mem->scale -= 0x18;
        if (mem->scale < 0x18) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

/// Draws a glow at the coordinate: two camera-facing textured squares, an
/// inner one of half-extent `size` and an outer one of `size * 3 / 2` (each
/// scaled by 0x37 / otz), plus the flat quad of
/// `func_shelter_b4_reservoir_801843AC` on the ground beneath it. It also
/// points the `Gp_RoomCoords[2]` light at the coordinate with a randomly flickering
/// intensity. Nothing is drawn when the projection flags an error.
void func_shelter_b4_reservoir_80183E80(GsCOORDINATE2* coord, s16 size)
{
    GsCOORDINATE2  ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;
    void**         scratch;
    GpRingScratch* alias;
    u16            vy;
    GpRingScratch* sc;

    slot                        = &Gp_RoomCoords[2];
    slot->framesLeft            = 2;
    light                       = &slot->data.light;
    light->inner                = 0x300;
    light->outer                = 0x3000;
    random                      = (Gp_LcgState * 5) + 0x71357911;
    intensity                   = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r               = intensity;
    shifted                     = intensity << 0x10;
    light->head.g               = (s16)(shifted >> 0x11);
    light->head.b               = (s16)(shifted >> 0x12);
    light->head.u.at.local.t[0] = (s32)coord->coord.t[0];
    light->head.u.at.local.t[1] = (s32)coord->coord.t[1];
    light->head.u.at.local.t[2] = coord->coord.t[2];
    slot->data.coord.flg        = 0;
    scratch                     = (void**)G_SCRATCH_HEAD;
    block                       = (GpRingScratch*)*scratch - 1;
    block->vec.vx               = *(u16*)&coord->workm.t[0];
    alias                       = block;
    vy                          = *(u16*)&coord->workm.t[1];
    __asm__("move %0,%1" : "=r"(alias) : "r"(alias), "r"(vy), "r"(alias));
    sc          = alias;
    sc->vec.vy  = vy;
    sc->vec.vz  = *(u16*)&coord->workm.t[2];
    Gp_LcgState = random;
    *scratch    = sc;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (sc->flag >= 0) {
        gte_stszotz(&block->otz);
        prim                 = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor       = prim + 1;
        ((u8*)&prim->tag)[3] = 9;
        prim->code           = 0x2EU;
        *(u16*)&prim->tpage  = 0x29;
        if (D_80070F70 & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code = (u8)(prim->code | 1);
        }
        sc->step = (s32)((s32)((s16)size * 0x37) / (s32)sc->otz);
        left     = *(u16*)&sc->sx - (u16)sc->step;
        prim->x2 = left;
        prim->x0 = left;
        right    = *(u16*)&sc->sx + (u16)sc->step;
        prim->x3 = right;
        prim->x1 = right;
        top      = *(u16*)&sc->sy - (u16)sc->step;
        prim->y1 = top;
        prim->y0 = top;
        bottom   = *(u16*)&sc->sy + (u16)sc->step;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        prim                 = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor       = prim + 1;
        ((u8*)&prim->tag)[3] = 9;
        prim->code           = 0x2F;
        prim->tpage          = 0x29;
        prim->clut =
            (s16)(((u32)(((gDisplayState.animFrame & 1) * 0x10) + 0x120) >> 4) |
                  0x4300);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)((s16)size * 3 / 2);
        sc->step    = (s32)((s32)(outerSize * 0x37) / (s32)sc->otz);
        outerLeft   = *(u16*)&sc->sx - (u16)sc->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = *(u16*)&sc->sx + (u16)sc->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = *(u16*)&sc->sy - (u16)sc->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = *(u16*)&sc->sy + (u16)sc->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_shelter_b4_reservoir_801843AC(&ground, outerSize);
        }
    }
    *(void**)G_SCRATCH_HEAD =
        (u8*)*(void**)G_SCRATCH_HEAD + sizeof(GpRingScratch);
}

/// Draws a flat quad facing the camera around the coordinate's world
/// position: the four unit corners of `D_80111E38` scaled by `arg1`, turned by
/// `Gfx_ViewWorldMtx` and moved to that position, then projected through
/// `GsWSMATRIX`. Unless the projection flags an error it queues one
/// semi-transparent textured quad (tpage 0x28, clut 0x428C) tinted
/// (0x30, 0x20, 0x20), alternating between two 32-texel frames from U 0xC0 on
/// odd and even frames.
void func_shelter_b4_reservoir_801843AC(GsCOORDINATE2* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = block->vec;
    tbl = D_80111E38;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        TOUCH_REG(v);
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = *(u16*)&block->sxy0.vx;
        prim->y0    = *(u16*)&block->sxy0.vy;
        prim->x1    = *(u16*)&block->sxy1.vx;
        prim->y1    = *(u16*)&block->sxy1.vy;
        prim->x2    = *(u16*)&block->sxy2.vx;
        prim->y2    = *(u16*)&block->sxy2.vy;
        prim->x3    = *(u16*)&block->sxy3.vx;
        prim->y3    = *(u16*)&block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
}
