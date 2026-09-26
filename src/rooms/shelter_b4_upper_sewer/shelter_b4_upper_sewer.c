#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

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
#include "rooms/rooms_shared_8017dcb8.h"
#include "rooms/rooms_shared_8017e4f8.h"
#include "rooms/shelter_b4_upper_sewer.h"

extern s16 D_80114D08;
extern u8  D_80115680;
extern u8  D_80115690;
extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011572C;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115738;
extern s32 D_8011573C;
extern s32 D_80115744;
extern s32 D_8011574C;
extern s32 D_80115750;
extern s32 D_80115754;
extern s32 D_80115758;

extern s32 func_80179A04(GpSaveLoc* in, GpSaveLoc* out);

extern u8         D_shelter_b4_upper_sewer_80186318;
extern u8         D_shelter_b4_upper_sewer_80188D2C;
extern TaskDesc   D_shelter_b4_upper_sewer_80186300;
extern GpMsgEntry D_shelter_b4_upper_sewer_801862D0[];
extern s16        D_shelter_b4_upper_sewer_80186438;
extern TaskDesc   D_shelter_b4_upper_sewer_8018643C[];
/// Save location filled from the outgoing location just before a table task is
/// spawned: `field_2` / `field_4` / `field_1` take its `field_0` / `field_2` /
/// `field_3`.
extern GpSaveLoc D_shelter_b4_upper_sewer_80188D24;
/// Spawn argument for the task `func_shelter_b4_upper_sewer_8017D80C` starts
/// with `Task_Spawn(1, 0x31, ...)`.
extern GpFadeWork D_shelter_b4_upper_sewer_80188D1C;

extern ShelterB4UpperSewerSurface D_shelter_b4_upper_sewer_80186448[];
extern ShelterB4UpperSewerSurface D_shelter_b4_upper_sewer_80186454[];
extern u8*                        D_shelter_b4_upper_sewer_80188D30;

extern SVECTOR D_shelter_b4_upper_sewer_80186490[];
extern SVECTOR D_shelter_b4_upper_sewer_801864B0[];
extern SVECTOR D_shelter_b4_upper_sewer_801864D0[];
extern SVECTOR D_shelter_b4_upper_sewer_801864F0[];
extern SVECTOR D_shelter_b4_upper_sewer_80186520[];

/// Colour shifts per spawn variant: each channel is the fade level shifted
/// right by the entry's value.
extern RoomHaloShade D_shelter_b4_upper_sewer_80186560[];

/// The two points the trail is emitted from, relative to the effect's parent:
/// the first positions the effect's own coordinate, the second is the other
/// end of the trail.
extern SVECTOR D_shelter_b4_upper_sewer_80186574[];
/// The second of those points, which the per-frame state reaches through its
/// own label rather than by indexing the pair.
extern SVECTOR D_shelter_b4_upper_sewer_8018657C;

/// Per-variant right shifts applied to the red, green and blue channels of a
/// disc's brightness.
extern s16 D_shelter_b4_upper_sewer_80186584[][3];

void func_shelter_b4_upper_sewer_8017DBA8(Task* task);
void func_shelter_b4_upper_sewer_8017DC28(Task* task);
void func_shelter_b4_upper_sewer_8017DD98(Task* task, ShelterB4UpperSewerSurface* e, s16 y, u8 c);
void func_shelter_b4_upper_sewer_8017E55C(Task* arg0);
void func_shelter_b4_upper_sewer_8017E59C(s32 arg0);
void func_shelter_b4_upper_sewer_8017EA0C(GpCoord* arg0, s32 arg1, s32 arg2);
void func_shelter_b4_upper_sewer_8017F1FC(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b4_upper_sewer_8017F5E8(GpCoord* arg0, s16 arg1, s16 arg2);
void func_shelter_b4_upper_sewer_8017F8CC(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b4_upper_sewer_801803DC(GpCoord* arg0, u16 arg1, u16 arg2, u16 arg3);
void func_shelter_b4_upper_sewer_8018139C(GpCoord* coord, s16 size);
void func_shelter_b4_upper_sewer_801818C8(GpCoord* arg0, s32 arg1);
void func_shelter_b4_upper_sewer_80181C40(GpCoord* arg0, s16 arg1, u8* arg2);
void func_shelter_b4_upper_sewer_801829D8(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b4_upper_sewer_80182E04(GpCoord* arg0, s16 arg1, u8* arg2);
void func_shelter_b4_upper_sewer_80183688(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3);
void func_shelter_b4_upper_sewer_80183D08(GpCoord* arg0, s16 arg1, u8* arg2);
void func_shelter_b4_upper_sewer_80184E44(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b4_upper_sewer_801850C8(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b4_upper_sewer_801854EC(GpCoord* arg0, s32 arg1, u8* rgb);
void func_shelter_b4_upper_sewer_80185A2C(GpCoord* coord, s16 size);
void func_shelter_b4_upper_sewer_80185F58(GpCoord* arg0, s32 arg1);

/// State handlers of the task `func_shelter_b4_upper_sewer_8017DC30` runs,
/// which copies the table to the stack and calls the entry for the task's
/// state: the room's setup (message table, pointer slot, water level), an idle
/// state, and `taskKill`.
const TaskFuncTable3 D_shelter_b4_upper_sewer_8017D5C4 = {
    { func_shelter_b4_upper_sewer_8017DBA8, func_shelter_b4_upper_sewer_8017DC28, taskKill }
};

void func_shelter_b4_upper_sewer_8017D660(Task* task)
{
    switch (task->state) {
        case 0:
            func_shelter_b4_upper_sewer_8017E59C(0);
            gGameSession->eventState = 1;
            task->state++;
            break;
        case 1:
            gGameSession->hideHud = 1;
            Gp_RunCapCmd(1, 0);
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
            if (Gp_GetCapEventKey() == 0xC) {
                taskKill(task);
                Mc_SaveData.at4.loc.view = D_shelter_b4_upper_sewer_80188D2C;
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                Gp_MsgAllyWeapon(1);
                Gp_MsgAlly3F3(1);
                gGameSession->eventState = 0;
                gGameSession->hideHud    = 0;
                Gp_StateF0.field_4       = 0;
                D_80114D08               = 0xA;
                break;
            }
            func_800E8614((s32)&D_shelter_b4_upper_sewer_80186318, 0);
            GameFlag_SetNibble(0xB8, 1);
            GameFlag_SetNibble(0x1BD, 0);
            task->state++;
            break;
        case 4:
            if (gGameSession->eventState == 0) {
                D_80114D08         = 0xA;
                Gp_StateF0.field_4 = 0;
                taskKill(task);
            }
            break;
    }
}

void func_shelter_b4_upper_sewer_8017D80C(Task* arg0)
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
            D_shelter_b4_upper_sewer_80188D1C.field_0 = 0;
            D_shelter_b4_upper_sewer_80188D1C.field_1 = 0;
            D_shelter_b4_upper_sewer_80188D1C.field_2 = 0x1E;
            Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b4_upper_sewer_80188D1C);
            arg0->killCountdown = 0x1E;
            arg0->state++;
            break;
        case 3:
            if (--arg0->killCountdown == 0) {
                SndEvt_EnqueueType6(0x542C0003, 0, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(0x542C0003) == 0) {
                arg0->state++;
            }
            break;
        case 5:
            gDisplayState.roomVariant = 1;
            Mc_SaveData.at4.loc.area  = D_shelter_b4_upper_sewer_80188D24.field_2;
            Mc_SaveData.at4.loc.warp  = D_shelter_b4_upper_sewer_80188D24.field_4;
            Mc_SaveData.at4.loc.room  = D_shelter_b4_upper_sewer_80188D24.field_1;
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b4_upper_sewer_8017D9BC(void)
{
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017D9C4(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179A04(src, dst);
    if (*(u16*)src == 0x2D) {
        if (src->field_5 == 0) {
            D_shelter_b4_upper_sewer_80188D24.field_2 = dst->field_0;
            D_shelter_b4_upper_sewer_80188D24.field_4 = dst->field_2;
            D_shelter_b4_upper_sewer_80188D24.field_1 = dst->field_3;
            Task_SpawnFromTable(&D_shelter_b4_upper_sewer_80186300, 1, 7, 0);
        }
        return 0;
    }
    if (*(u16*)src == 0x2E) {
        if (src->field_5 == 0) {
            D_shelter_b4_upper_sewer_80188D24.field_2 = dst->field_0;
            D_shelter_b4_upper_sewer_80188D24.field_4 = dst->field_2;
            D_shelter_b4_upper_sewer_80188D24.field_1 = dst->field_3;
            Task_SpawnFromTable(&D_shelter_b4_upper_sewer_80186300, 1, 8, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_shelter_b4_upper_sewer_8017DAB0(Task* task, s32 msgId, s32 arg2)
{
    u8 temp_a1;

    if (arg2 == 1) {
        if (GameFlag_GetNibble(0xB8) == 0) {
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            Gp_StateF0.field_4                = 2;
            temp_a1                           = Mc_SaveData.at4.loc.view;
            Mc_SaveData.at4.loc.view          = 0xD;
            D_shelter_b4_upper_sewer_80188D2C = temp_a1;
            Task_SpawnFromTable(&D_shelter_b4_upper_sewer_80186300, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(6);
        }
    }
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017DB50(void)
{
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017DB58(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 4) {
        func_shelter_b4_upper_sewer_8017E59C(1);
        SndEvt_EnqueueType6(0x542C0004, 0, 0);
    }
    return 0;
}

void func_shelter_b4_upper_sewer_8017DB94(void)
{
    Mc_SaveData.at4.loc.view = D_shelter_b4_upper_sewer_80188D2C;
}

void func_shelter_b4_upper_sewer_8017DBA8(Task* task)
{
    task->msgTable = D_shelter_b4_upper_sewer_801862D0;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0xB7) != 0) {
        D_shelter_b4_upper_sewer_80186438 = -0x708;
        Task_SpawnFromTable(D_shelter_b4_upper_sewer_8018643C, 0, 0, 0);
    } else {
        D_shelter_b4_upper_sewer_80186438 = 0;
    }
    task->state = (s32)(task->state + 1);
}

void func_shelter_b4_upper_sewer_8017DC28(Task* task)
{
}

void func_shelter_b4_upper_sewer_8017DC30(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b4_upper_sewer_8017D5C4;
    sp.funcs[task->state](task);
}

/// Water state of the room's water task: clamps the water level
/// `D_shelter_b4_upper_sewer_80186438` to -0x640..0, publishes it as the
/// session's water height, and draws the surfaces of the current camera view at
/// that height, in a blue that brightens as the level moves away from 0.
void func_shelter_b4_upper_sewer_8017DC88(Task* task)
{
    s16 w;
    u8  c;
    s32 h;

    if (Mc_SaveData.companionType == 0) {
        D_shelter_b4_upper_sewer_80188D30 = (u8*)D_8005C374 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_shelter_b4_upper_sewer_80188D30 = (u8*)D_8005C370 + gDisplayState.otBuffer * 0xC000;
    }
    if (D_shelter_b4_upper_sewer_80186438 < -0x640) {
        D_shelter_b4_upper_sewer_80186438 = -0x640;
    } else if (D_shelter_b4_upper_sewer_80186438 > 0) {
        D_shelter_b4_upper_sewer_80186438 = 0;
    }
    w                    = D_shelter_b4_upper_sewer_80186438;
    gGameSession->waterY = w;
    h                    = w;
    c                    = (-h * 16) / 225;
    if (gGameSession->at4.loc.view != 0xC) {
        func_shelter_b4_upper_sewer_8017DD98(task, D_shelter_b4_upper_sewer_80186448, h, c);
    } else {
        func_shelter_b4_upper_sewer_8017DD98(task, D_shelter_b4_upper_sewer_80186454, h, c);
    }
}

/// Draws each surface in `e` at height `y` as a strip of `count` flat
/// semi-transparent quads coloured (0, `c` / 4, `c`), projected through the
/// view matrix and each followed by a draw-mode packet selecting blend mode 2.
/// Quads are linked 0x60 deeper in the ordering table than their projected
/// depth, and those the projection flags as invalid are skipped. The
/// per-surface values live in a work block pushed on the scratchpad stack for
/// the duration of the call. `task` is unused.
void func_shelter_b4_upper_sewer_8017DD98(Task* task, ShelterB4UpperSewerSurface* e, s16 y, u8 c)
{
    SVECTOR           v0, v1, v2, v3;
    s32               sxy0, sxy1, sxy2, sxy3;
    s32               p, flag;
    RoomWaterScratch* w;
    u8*               head;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;

    gGfxViewCoord.flg = 0;
    head              = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8)  = head - 0xC;
    w                 = (RoomWaterScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    w->y = y;
    for (; e->count != -1; e++) {
        if (e->alongZ == 0) {
            w->dx = e->width / e->count;
            w->dz = e->depth;
            w->x  = e->x;
            w->z  = e->z;
            for (i = 0; i < e->count; i++) {
                v0.vx   = w->x + w->dx * i;
                v0.vy   = w->y;
                v0.vz   = w->z;
                v1.vx   = w->x + w->dx * (i + 1);
                v1.vy   = w->y;
                v1.vz   = w->z;
                w->wave = 0;
                v2.vx   = w->x + w->dx * i;
                v2.vy   = w->y + w->wave;
                v2.vz   = w->z + w->dz;
                w->wave = 0;
                v3.vx   = w->x + w->dx * (i + 1);
                v3.vy   = w->y + w->wave;
                v3.vz   = w->z + w->dz;
                otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
                if (flag >= 0) {
                    poly                              = (POLY_F4*)D_shelter_b4_upper_sewer_80188D30;
                    D_shelter_b4_upper_sewer_80188D30 = (u8*)(poly + 1);
                    setlen(poly, 5);
                    setcode(poly, 0x2A);
                    PRIM_XY_WORD(poly, 0) = sxy0;
                    PRIM_XY_WORD(poly, 1) = sxy1;
                    PRIM_XY_WORD(poly, 2) = sxy2;
                    PRIM_XY_WORD(poly, 3) = sxy3;
                    poly->r0              = 0;
                    poly->g0              = c >> 2;
                    poly->b0              = c;
                    addPrim((u_long*)(((((u32)(otz + 0x60) << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            poly);
                    dr                                = (DR_MODE*)D_shelter_b4_upper_sewer_80188D30;
                    D_shelter_b4_upper_sewer_80188D30 = (u8*)(dr + 1);
                    setlen(dr, 1);
                    dr->code[0] = 0xE100004A;
                    addPrim((u_long*)(((((u32)(otz + 0x60) << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            dr);
                }
            }
        } else {
            w->dx = e->width;
            w->dz = e->depth / e->count;
            w->x  = e->x;
            w->z  = e->z;
            for (i = 0; i < e->count; i++) {
                v0.vx   = w->x;
                v0.vy   = w->y;
                v0.vz   = w->z + w->dz * i;
                v1.vx   = w->x;
                v1.vy   = w->y;
                v1.vz   = w->z + w->dz * (i + 1);
                w->wave = 0;
                v2.vx   = w->x + w->dx;
                v2.vy   = w->y + w->wave;
                v2.vz   = w->z + w->dz * i;
                w->wave = 0;
                v3.vx   = w->x + w->dx;
                v3.vy   = w->y + w->wave;
                v3.vz   = w->z + w->dz * (i + 1);
                otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
                if (flag >= 0) {
                    poly                              = (POLY_F4*)D_shelter_b4_upper_sewer_80188D30;
                    D_shelter_b4_upper_sewer_80188D30 = (u8*)(poly + 1);
                    setlen(poly, 5);
                    setcode(poly, 0x2A);
                    PRIM_XY_WORD(poly, 0) = sxy0;
                    PRIM_XY_WORD(poly, 1) = sxy1;
                    PRIM_XY_WORD(poly, 2) = sxy2;
                    PRIM_XY_WORD(poly, 3) = sxy3;
                    poly->r0              = 0;
                    poly->g0              = c >> 2;
                    poly->b0              = c;
                    addPrim((u_long*)(((((u32)(otz + 0x60) << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            poly);
                    dr                                = (DR_MODE*)D_shelter_b4_upper_sewer_80188D30;
                    D_shelter_b4_upper_sewer_80188D30 = (u8*)(dr + 1);
                    setlen(dr, 1);
                    dr->code[0] = 0xE100004A;
                    addPrim((u_long*)(((((u32)(otz + 0x60) << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            dr);
                }
            }
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

/// The room's water task: runs its state (`func_shelter_b4_upper_sewer_8017E55C`
/// once, then `func_shelter_b4_upper_sewer_8017DC88` every frame) and publishes
/// the water level as the session's water height.
void func_shelter_b4_upper_sewer_8017E4F4(Task* task)
{
    TaskFunc states[2] = { func_shelter_b4_upper_sewer_8017E55C, func_shelter_b4_upper_sewer_8017DC88 };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_upper_sewer_80186438;
}

void func_shelter_b4_upper_sewer_8017E55C(Task* arg0)
{
    if (Mc_SaveData.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Sets or clears `field_4` of the second sprite command in view 13 of the
/// current room's sprite table, from the low byte of `arg0` (zero clears it,
/// anything else sets it to 1).
void func_shelter_b4_upper_sewer_8017E59C(s32 arg0)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtCmd* cmd;

    cmd = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1][12].field_4;
    if ((arg0 & 0xFF) == 0) {
        cmd[1].field_4 = 0;
    } else {
        cmd[1].field_4 = 1;
    }
}

/// Publishes the sewer's effect ids on the task's first tick - two extra ids
/// only while `GameFlag_GetNibble(0xB7)` is 1 - then draws the
/// `func_shelter_b4_upper_sewer_8017F8CC` capsules the current camera view
/// shows. Views 4 and 13 both end on
/// `D_shelter_b4_upper_sewer_801864F0[12]`, and writing that address off the
/// array (rather than through its own symbol) is what keeps view 4's array
/// base live across the first call while the shared tail is cross-jumped.
void func_shelter_b4_upper_sewer_8017E5F8(Task* arg0)
{
    if (arg0->state == 0) {
        if (GameFlag_GetNibble(0xB7) == 1) {
            D_8011574C = 0x60170;
            D_80115738 = 0x60171;
        }
        D_80115728  = 0x6024E;
        D_80115744  = 0x6025A;
        D_8011573C  = 0x60265;
        D_80115720  = 0x60271;
        D_80115758  = 0x600F0;
        D_8011572C  = 0x600F1;
        D_80115750  = 0x600F2;
        D_80115734  = 0x60224;
        D_80115730  = 0x6022F;
        D_80115754  = 0x6023A;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
            func_shelter_b4_upper_sewer_8017F8CC(D_shelter_b4_upper_sewer_801864F0, 0x200, 0x444);
            break;
        case 4:
            func_shelter_b4_upper_sewer_8017F8CC(&D_shelter_b4_upper_sewer_801864F0[0], 0x200, 0x222);
            func_shelter_b4_upper_sewer_8017F8CC(&D_shelter_b4_upper_sewer_801864F0[12], 0x200, 0x124);
            break;
        case 8: {
            SVECTOR* p = D_shelter_b4_upper_sewer_801864D0;
            func_shelter_b4_upper_sewer_8017F8CC(&p[0], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[2], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[4], 0x200, 0x222);
            func_shelter_b4_upper_sewer_8017F8CC(&p[6], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[16], 0x200, 0x124);
            break;
        }
        case 9: {
            SVECTOR* p = D_shelter_b4_upper_sewer_801864D0;
            func_shelter_b4_upper_sewer_8017F8CC(&p[0], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[2], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[4], 0x200, 0x222);
            func_shelter_b4_upper_sewer_8017F8CC(&p[6], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[16], 0x200, 0x124);
            break;
        }
        case 10: {
            SVECTOR* p = D_shelter_b4_upper_sewer_801864B0;
            func_shelter_b4_upper_sewer_8017F8CC(&p[0], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[2], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[18], 0x200, 0x343);
            break;
        }
        case 11: {
            SVECTOR* p = D_shelter_b4_upper_sewer_80186490;
            func_shelter_b4_upper_sewer_8017F8CC(&p[0], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[6], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[18], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[20], 0x200, 0x343);
            break;
        }
        case 12:
            func_shelter_b4_upper_sewer_8017F8CC(D_shelter_b4_upper_sewer_80186520, 0x200, 0x444);
            break;
        case 13:
            func_shelter_b4_upper_sewer_8017F8CC(&D_shelter_b4_upper_sewer_801864F0[12], 0x200, 0x124);
            break;
    }
}

/// Effect task drawing a flat quad through `func_shelter_b4_upper_sewer_8017EA0C`
/// that grows and fades out. On its first frame it starts the size (`angle`)
/// at the low 12 bits of the spawn argument and the brightness (`scale`) at
/// 0x40, and turns its coordinate to a random angle about Y. Each frame it then
/// grows the size by 0x20 and dims the brightness by 2, releasing the effect
/// once the brightness falls under 2. While `Gp_State1C->eventState` is
/// non-zero it only redraws at the current values, and releases from state 4.
void func_shelter_b4_upper_sewer_8017E8B8(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_b4_upper_sewer_8017EA0C(coord, work->angle, work->scale);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        if (task->state == 0) {
            work->scale = 0x40;
            work->angle = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
            coord->flg  = 0;
            task->state = 1;
        }
        work->angle += 0x20;
        func_shelter_b4_upper_sewer_8017EA0C(coord, work->angle, work->scale);
        work->scale -= 2;
        if (work->scale < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a flat quad lying in the XZ plane of `arg0`: the four unit corners of
/// `D_80111E38` scaled by `arg1`, turned by the coordinate's world matrix and
/// moved to its position, then projected through `GsWSMATRIX`. Queues one
/// semi-transparent textured quad (tpage 0x2B, clut 0x43D1) shaded grey at
/// `arg2`, unless the projection flags an error.
void func_shelter_b4_upper_sewer_8017EA0C(GpCoord* arg0, s32 arg1, s32 arg2)
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
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(wm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        (u16) v->vx = (u16)v->vx + (u16)arg0->workm.t[0];
        tbl++;
        (u16) v->vy = (u16)v->vy + (u16)arg0->workm.t[1];
        i++;
        (u16) v->vz = (u16)v->vz + (u16)arg0->workm.t[2];
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
        prim->x0 = (u16)block->sxy0.vx;
        prim->y0 = (u16)block->sxy0.vy;
        prim->x1 = (u16)block->sxy1.vx;
        prim->y1 = (u16)block->sxy1.vy;
        prim->x2 = (u16)block->sxy2.vx;
        prim->y2 = (u16)block->sxy2.vy;
        prim->x3 = (u16)block->sxy3.vx;
        prim->y3 = (u16)block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Particle task drawn as the spinning sprite of
/// `func_shelter_b4_upper_sewer_8017F1FC` (state 1) or the standing sprite of
/// `func_shelter_b4_upper_sewer_8017F5E8` (state 2), which the top nibble of the
/// spawn argument selects. The first frame takes the size from the argument's
/// low 12 bits, a random angle, and the ticks per animation frame from bits
/// 12-15. Unless the work already carries a velocity it picks one by the kind
/// in bits 24-27, scaled to the speed in bits 16-23 (0x40 when zero). Each later tick draws, moves the coordinate by the velocity with
/// gravity pulling it down, and releases the task after animation frame 7.
/// While an event runs it only draws, and is released once the event state
/// reaches 4.
void func_shelter_b4_upper_sewer_8017ED40(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    SVECTOR*   vec;
    s32        kind;
    s32        step;
    s32        state;
    s32        level;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_shelter_b4_upper_sewer_8017F1FC(coord, work->index, work->scale, work->angle);
            } else {
                func_shelter_b4_upper_sewer_8017F5E8(coord, work->index, work->scale);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    Gp_UpdateCoord(coord);
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            return;
        case 1:
            func_shelter_b4_upper_sewer_8017F1FC(coord, work->index, work->scale, work->angle);
            break;
        case 2:
            func_shelter_b4_upper_sewer_8017F5E8(coord, work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0] += work->move.vx;
        coord->coord.t[1] += work->move.vy;
        coord->coord.t[2] += work->move.vz;
        coord->flg         = 0;
        work->move.vy     += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
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
void func_shelter_b4_upper_sewer_8017F1FC(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
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
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}

/// Draws a sprite at the coordinate's world position: one semi-transparent
/// textured quad (tpage 0x2B, clut 0x43D2) around the projected point, unless
/// the projection flags an error. `arg1` picks one of eight 56-texel frames,
/// four across and two down from V 0x70. `arg2` is the size, a screen
/// half-extent of `arg2 * 55 / otz`; the quad is that wide on each side and
/// stands on the point, reaching one and a half extents above it and half an
/// extent below.
void func_shelter_b4_upper_sewer_8017F5E8(GpCoord* arg0, s16 arg1, s16 arg2)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    GpRingScratch* p;
    POLY_FT4*      prim;
    DisplayState*  ds;
    s16            cell;
    s16            cell2;
    s32            u0;
    s32            vbase;
    s32            v0u;
    s32            x;
    s16            xy;
    u16            vy;
    u16            vz;

    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    vy                                      = (u16)arg0->workm.t[1];
    SOFT_TOUCH_REG_USE(block, vy);
    p = block;
    SOFT_TOUCH_REG(p);
    p->vec.vy = vy;
    vz        = (u16)arg0->workm.t[2];
    *scratch  = block;
    p->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&p->vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (p->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
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
        prim->v3    = vbase - 0x59;
        block->step = (arg2 * 0x37) / block->otz;
        xy          = (u16)block->sx - (u16)block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = (u16)block->sx + (u16)block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = ((u16)block->sy - (u16)block->step) - (block->step >> 1);
        ds          = &gDisplayState;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = (u16)block->sy + (block->step >> 1);
        prim->y3    = xy;
        prim->y2    = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Draws a glowing capsule between the points `arg0[0]` and `arg0[1]`,
/// projected through `gGfxViewCoord.workm`; nothing is drawn unless both project.
/// Each end is a half-disc of screen radius `arg1 * 64 / otz` and the two are
/// joined by a band, built from gouraud quads bright at the centre line and
/// black at the rim, in two 0x400 steps around the angle between the projected
/// points. `arg2` is the colour as three 4-bit channels (0xRGB), brightened
/// slightly on odd frames.
void func_shelter_b4_upper_sewer_8017F8CC(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      sum;
    u8                       r;
    u8                       g;
    u8                       b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (OverlayPointPairScratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)ang;
            blend     = ((u8)ds->animFrame & 1) * 8;
            packed    = arg2 << 16;
            tr        = (packed >> 20) & 0xF0;
            tg        = (packed >> 16) & 0xF0;
            r         = blend | tr;
            g         = blend | tg;
            b         = blend | ((arg2 & 0xF) << 4);
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
                    t3             = ang + 0x800;
                    prim           = (POLY_G4*)gGpuPrimCursor;
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
    SCRATCH_POP_BYTES(0x1C);
}

void func_shelter_b4_upper_sewer_80180110(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    s32        lifetime;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                if (task->spawnArg1 & 3) {
                    work->scale   = 0x80;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = ((RoomMoteArg*)&task->spawnArg1)->speed;
                    work->move.vz = 0;
                    if (task->spawnArg1 & 2) {
                        work->move.vy = -work->move.vy;
                    }
                    task->state = 2;
                } else {
                    work->scale   = 0x20;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = -((RoomMoteArg*)&task->spawnArg1)->speed - (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F);
                    work->move.vz = 0;
                    task->state   = (task->spawnArg1 & 1) + 1;
                }
                break;
            case 1:
                coord->coord.t[1] += work->move.vy;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_shelter_b4_upper_sewer_801803DC(coord, work->index, work->angle | 0x1000, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    } else if (work->scale < 0x80) {
                        work->scale += 0x20;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
            case 2:
                coord->coord.t[1] += work->move.vy;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_shelter_b4_upper_sewer_801803DC(coord, work->index, work->angle, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws a sprite at the coordinate's world position: one semi-transparent
/// textured quad (tpage 0x2A) centred on the projected point, unless the
/// projection flags an error. `arg1` picks one of four 24-texel frames in the
/// strip `arg2 >> 12` selects, and the low 12 bits of `arg2` are the size, a
/// screen half-extent of `size * 23 / (otz + 1)`. The low byte of `arg3` is the
/// grey level it is shaded with, and its top nibble picks the palette on CLUT
/// row 0x10B (column 0xB0 when zero).
void func_shelter_b4_upper_sewer_801803DC(GpCoord* arg0, u16 arg1, u16 arg2, u16 arg3)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    GpRingScratch* next;
    POLY_FT4*      prim;
    DisplayState*  ds;
    u16            row;
    u16            pal;
    s32            u0;
    s32            u1;
    s16            xy;
    u16            vz;

    row                                     = arg2 >> 12;
    arg2                                   &= 0xFFF;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    pal                                     = arg3 >> 12;
    arg3                                   &= 0xFF;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)arg0->workm.t[0];
    next                                    = (GpRingScratch*)(head - 0x18);
    __asm__("move %0,%1" : "=r"(block) : "r"(next));
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        setRGB0(prim, arg3, arg3, arg3);
        if (pal != 0) {
            prim->clut = getClut(pal * 16 + 0xF0, 0x10B);
        } else {
            prim->clut = getClut(0xB0, 0x10B);
        }
        u0 = row * 0x60 + (arg1 & 3) * 24;
        u1 = u0 + 0x17;
        setUV4(prim, u0, 0, u1, 0, u0, 0x17, u1, 0x17);
        block->step = arg2 * 23 / block->otz;
        xy          = (u16)block->sx - (u16)block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = (u16)block->sx + (u16)block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = (u16)block->sy - (u16)block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = (u16)block->sy + (u16)block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Draws a ring around the coordinate's world position, projected through
/// `GsWSMATRIX`, unless the projection flags an error: sixteen gouraud quads,
/// black at screen radius `arg1 * 64 / (otz + 1)` and coloured `rgb` at
/// `(arg1 + arg2) * 64 / (otz + 1)`.
void func_shelter_b4_upper_sewer_801806A0(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   next;
    s32                   inner;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    inner         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = ((s16)arg1 * 64) / block->otz;
        block->rInner = ((s16)inner * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(next)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(RoomBillboardScratch);
}

/// Draws a glowing disc at the coordinate's world position, unless the
/// projection flags an error: eight gouraud wedges coloured `arg2` at the
/// projected centre and black at the rim, of screen radius
/// `arg1 * 64 / (otz + 1)`.
void func_shelter_b4_upper_sewer_80180AC4(GpCoord* arg0, s16 arg1, u8* arg2)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

void func_shelter_b4_upper_sewer_80180E58(Task* arg0)
{
    u8          rgb[3];
    GpEffWork*  mem;
    GpCoord*    coord;
    GpMtxWords* rot;
    s16         flag;
    s32         shift;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        switch (arg0->state) {
            case 0:
                rot               = (GpMtxWords*)&coord->coord;
                coord->sub        = mem->parent;
                rot->m00_m01      = 0x1000;
                rot->m02_m10      = 0;
                rot->m11_m12      = 0x1000;
                rot->m20_m21      = 0;
                rot->m22          = 0x1000;
                coord->coord.t[0] = mem->pos.vx;
                coord->coord.t[1] = mem->pos.vy;
                coord->coord.t[2] = mem->pos.vz;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                shift           = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
                mem->index      = shift;
                arg0->spawnArg1 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0;
                arg0->state     = 1;
                mem->step       = 0x100 / arg0->spawnArg1;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                mem->scale      += mem->step;
                mem->angle      += mem->step;
                arg0->spawnArg1 -= 1;
                rgb[0]           = mem->scale >> D_shelter_b4_upper_sewer_80186560[mem->index].r;
                rgb[1]           = mem->scale >> D_shelter_b4_upper_sewer_80186560[mem->index].g;
                rgb[2]           = mem->scale >> D_shelter_b4_upper_sewer_80186560[mem->index].b;
                func_shelter_b4_upper_sewer_80180AC4(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_shelter_b4_upper_sewer_80180AC4(coord, mem->angle + 0x100, rgb);
                }
                func_shelter_b4_upper_sewer_801806A0(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1 == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_shelter_b4_upper_sewer_80186560[mem->index].r;
                    rgb[1] = mem->scale >> D_shelter_b4_upper_sewer_80186560[mem->index].g;
                    rgb[2] = mem->scale >> D_shelter_b4_upper_sewer_80186560[mem->index].b;
                    func_shelter_b4_upper_sewer_80181C40(coord, (u16)mem->angle * 4, rgb);
                    mem->scale -= 0x10;
                    mem->angle += 8;
                    return;
                }
                /* fallthrough */
            case 3:
                goto kill;
            default:
                return;
        }
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_shelter_b4_upper_sewer_801811F0(Task* arg0)
{
    u8         rgb[3];
    GpEffWork* mem;
    GpCoord*   coord;
    s16        flag;
    s16        step;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
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
        rgb[1]     = mem->scale >> 1;
        rgb[2]     = mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_shelter_b4_upper_sewer_80180AC4(coord, step * 2, rgb);
        func_shelter_b4_upper_sewer_8018139C(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_shelter_b4_upper_sewer_801806A0(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
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

/// Glow at a coordinate: two camera-facing textured quads, an inner one of
/// half-extent `size` and an outer one of `size * 3 / 2`, plus a ground mark
/// under it. Also feeds the `Gp_RoomCoords[2]` light a flickering intensity at the
/// coordinate's position. Draws nothing when the point fails to project.
void func_shelter_b4_upper_sewer_8018139C(GpCoord* coord, s16 size)
{
    GpCoord        ground;
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
    block                       = SCRATCH_HEAD_AT(scratch, GpRingScratch) - 1;
    block->vec.vx               = (u16)coord->workm.t[0];
    alias                       = block;
    vy                          = (u16)coord->workm.t[1];
    __asm__("move %0,%1" : "=r"(alias) : "r"(alias), "r"(vy), "r"(alias));
    sc          = alias;
    sc->vec.vy  = vy;
    sc->vec.vz  = (u16)coord->workm.t[2];
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
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
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
        left     = sc->sx - sc->step;
        prim->x2 = left;
        prim->x0 = left;
        right    = sc->sx + sc->step;
        prim->x3 = right;
        prim->x1 = right;
        top      = sc->sy - sc->step;
        prim->y1 = top;
        prim->y0 = top;
        bottom   = sc->sy + sc->step;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (s16)(((u32)(((gDisplayState.animFrame & 1) * 0x10) + 0x120) >> 4) |
                  0x4300);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)((s16)size * 3 / 2);
        sc->step    = (s32)((s32)(outerSize * 0x37) / (s32)sc->otz);
        outerLeft   = sc->sx - sc->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = sc->sx + sc->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = sc->sy - sc->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = sc->sy + sc->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_shelter_b4_upper_sewer_801818C8(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Draws a flat mark: a quad of half-size `arg1` laid flat in view space
/// through `gGfxViewCoord.workm` and moved to the coordinate's position, then
/// projected through `GsWSMATRIX`. Queues one semi-transparent textured quad
/// (tpage 0x28, clut 0x428C) tinted (0x30, 0x20, 0x20), alternating between two
/// texture frames on odd and even frames, unless the projection flags an error.
void func_shelter_b4_upper_sewer_801818C8(GpCoord* arg0, s32 arg1)
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
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        (u16) v->vx = (u16)v->vx + (u16)arg0->workm.t[0];
        tbl++;
        (u16) v->vy = (u16)v->vy + (u16)arg0->workm.t[1];
        i++;
        (u16) v->vz = (u16)v->vz + (u16)arg0->workm.t[2];
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
        prim->x0    = (u16)block->sxy0.vx;
        prim->y0    = (u16)block->sxy0.vy;
        prim->x1    = (u16)block->sxy1.vx;
        prim->y1    = (u16)block->sxy1.vy;
        prim->x2    = (u16)block->sxy2.vx;
        prim->y2    = (u16)block->sxy2.vy;
        prim->x3    = (u16)block->sxy3.vx;
        prim->y3    = (u16)block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Draws a flash at the coordinate's world position, unless the projection
/// flags an error: a disc of eight gouraud wedges of screen radius
/// `arg1 * 64 / (otz + 1)` in half the colour `arg2`, a disc of half that radius
/// in the full colour, and four pointed rays in half the colour reaching
/// alternately to that radius and to twice it. Every wedge fades to black at its
/// rim.
void func_shelter_b4_upper_sewer_80181C40(GpCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
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
    SCRATCH_POP(RoomBillboardScratch);
}

void func_shelter_b4_upper_sewer_80182600(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;
    s16        flag;
    s16        ang;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        Gp_UpdateCoord(coord);
        mem->age++;
        if (mem->age >= 0x15) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        }
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        ang          = mem->scale + ((((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200);
        mem->scale   = ang;
        mem->move.vx = (u32)(rcos(ang) * 3) >> 4;
        mem->move.vy = -mem->age * 128;
        mem->move.vz = (u32)(rsin(mem->scale) * 3) >> 4;
        Gp_SpawnEff(D_80115728, coord, 0x30080201, &mem->move);
    }
}

void func_shelter_b4_upper_sewer_80182734(Task* arg0)
{
    u8                rgb[3];
    GpEffWork*        mem;
    register GpCoord* coord asm("s2");
    s16               flag;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        Gp_UpdateCoord(coord);
        mem->age++;
        switch (arg0->state) {
            case 0:
                mem->scale  = 0;
                mem->angle  = 0x80;
                mem->step   = 0x100 / arg0->spawnArg1;
                arg0->state = 1;
                return;
            case 1:
                mem->scale      += mem->step;
                mem->angle      += mem->step;
                arg0->spawnArg1 -= 1;
                rgb[0]           = mem->scale;
                rgb[1]           = mem->scale >> 2;
                rgb[2]           = mem->scale >> 1;
                func_shelter_b4_upper_sewer_80182E04(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                func_shelter_b4_upper_sewer_80182E04(coord, (u16)mem->angle * 2, rgb);
                func_shelter_b4_upper_sewer_801829D8(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1 == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    rgb[0]      = mem->scale;
                    rgb[1]      = mem->scale >> 2;
                    rgb[2]      = mem->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                    return;
                }
                return;
            case 2:
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale;
                    rgb[1] = mem->scale >> 2;
                    rgb[2] = mem->scale >> 1;
                    func_shelter_b4_upper_sewer_80183D08(coord, mem->angle * 3, rgb);
                    mem->scale -= 0x10;
                    mem->angle -= 8;
                    return;
                }
                /* fallthrough */
            case 3:
                goto kill;
            default:
                return;
        }
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// The ring `func_shelter_b4_upper_sewer_801806A0` draws, compiled with a
/// different scratch block layout: sixteen gouraud quads around the
/// coordinate's projected position, black at screen radius
/// `arg1 * 64 / (otz + 1)` and coloured `rgb` at `(arg1 + arg2) * 64 / (otz + 1)`.
void func_shelter_b4_upper_sewer_801829D8(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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
    vx      = (u16)arg0->workm.t[0];
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw02Scratch*)tmp;
    }
    block->vec.vx = vx;
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
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
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Draws a glowing disc at the coordinate's world position, unless the
/// projection flags an error: eight gouraud wedges coloured `arg2` at the
/// projected centre and black at the rim, of screen radius
/// `arg1 * 64 / (otz + 1)`.
void func_shelter_b4_upper_sewer_80182E04(GpCoord* arg0, s16 arg1, u8* arg2)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

void func_shelter_b4_upper_sewer_80183198(Task* task)
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
                objCoord->coord.t[0] = D_shelter_b4_upper_sewer_80186574[0].vx;
                objCoord->coord.t[1] = D_shelter_b4_upper_sewer_80186574[0].vy;
                objCoord->coord.t[2] = D_shelter_b4_upper_sewer_80186574[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_shelter_b4_upper_sewer_80186574[1];
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
                coord.coord.t[0] = D_shelter_b4_upper_sewer_8018657C.vx;
                coord.coord.t[1] = D_shelter_b4_upper_sewer_8018657C.vy;
                coord.coord.t[2] = D_shelter_b4_upper_sewer_8018657C.vz;
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
                func_shelter_b4_upper_sewer_80183688(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws a ribbon between two eight-slot rings of coordinates, `arg0` and
/// `arg1`: seven gouraud quads walking back from slot `arg2`, each joining two
/// consecutive slots of both rings, skipped where the projection flags an
/// error. The brightness falls from `0x40 - 9 * i` at a quad's leading edge to
/// nine less at its trailing one, and `arg3` holds the multipliers the red,
/// green and blue channels apply to it, at bits 8, 4 and 0.
void func_shelter_b4_upper_sewer_80183688(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3)
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
        blk->v[0].vx = (u16)a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = (u16)a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = (u16)a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = (u16)b->workm.t[0];
        blk->v[1].vy = (u16)b->workm.t[1];
        blk->v[1].vz = (u16)b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = (u16)a->workm.t[0];
        blk->v[2].vy = (u16)a->workm.t[1];
        blk->v[2].vz = (u16)a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = (u16)b->workm.t[0];
        blk->v[3].vy = (u16)b->workm.t[1];
        blk->v[3].vz = (u16)b->workm.t[2];
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

void func_shelter_b4_upper_sewer_80183A80(Task* task)
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
            func_shelter_b4_upper_sewer_801829D8(objCoord, 0x100, 0x100, rgb);
            func_shelter_b4_upper_sewer_801829D8(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Draws a flash at the coordinate's world position, unless the projection
/// flags an error: a disc of eight gouraud wedges of screen radius
/// `arg1 * 64 / (otz + 1)` in half the colour `arg2`, a disc of half that radius
/// in the full colour, and four pointed rays in half the colour reaching
/// alternately to that radius and to twice it. Every wedge fades to black at its
/// rim.
void func_shelter_b4_upper_sewer_80183D08(GpCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
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
    SCRATCH_POP(RoomBillboardScratch);
}

/// Effect task of a glowing disc attached to its parent at the work's
/// position. In state 1 it spawns effect `D_80115730` every fourth tick at a
/// random joint of the player's model, passing its own coordinate, and adopts
/// the new task, while the disc grows; state 2 adds a flickering second disc; state 3 drifts the disc
/// away while it fades and an expanding ring surrounds it, then releases the
/// task. The disc's colour is its brightness shifted per channel by the row of
/// `D_shelter_b4_upper_sewer_80186584` that the spawn argument selects. While
/// an event runs it does nothing, and is released once the event state reaches
/// 4.
void func_shelter_b4_upper_sewer_801846C8(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;
    GpEffWork* spawned;
    MATRIX*    mtx;
    u8         col[4];

    mem   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
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
            coord->sub                       = mem->parent;
            mtx                              = &coord->coord;
            MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
            MATRIX_PAIR(mtx, 0, 2)           = 0;
            MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
            MATRIX_PAIR(mtx, 2, 0)           = 0;
            mtx->m[2][2]                     = 0x1000;
            coord->coord.t[0]                = mem->pos.vx;
            coord->coord.t[1]                = mem->pos.vy;
            coord->coord.t[2]                = mem->pos.vz;
            coord->flg                       = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &player->extra.tmd->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], (s32)coord, NULL);
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
            col[0] = mem->scale >> D_shelter_b4_upper_sewer_80186584[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b4_upper_sewer_80186584[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b4_upper_sewer_80186584[arg0->spawnArg1][2];
            func_shelter_b4_upper_sewer_801854EC(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b4_upper_sewer_80186584[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b4_upper_sewer_80186584[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b4_upper_sewer_80186584[arg0->spawnArg1][2];
            func_shelter_b4_upper_sewer_801854EC(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_shelter_b4_upper_sewer_801854EC(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_shelter_b4_upper_sewer_80186584[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_shelter_b4_upper_sewer_80186584[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_shelter_b4_upper_sewer_80186584[arg0->spawnArg1][2];
            func_shelter_b4_upper_sewer_801854EC(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = mem->scale >> 1;
            col[2] = mem->scale >> 2;
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
            func_shelter_b4_upper_sewer_801850C8(coord, (s16)(mem->period + 0x80), 0x100, col);
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

void func_shelter_b4_upper_sewer_80184C20(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    GpCoord*   target;
    VECTOR     delta;

    work   = task->spawnArg2;
    coord  = task->extra.tmd->coords;
    target = (GpCoord*)task->spawnArg1;
    if (Gp_State1C->eventState == 0) {
        work->age++;
        switch (task->state) {
            case 0:
                delta.vx = target->workm.t[0] - coord->workm.t[0];
                delta.vy = target->workm.t[1] - coord->workm.t[1];
                delta.vz = target->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &delta, &delta);
                work->pos.vx = delta.vx;
                work->pos.vy = delta.vy;
                work->pos.vz = delta.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->pos);
                gte_rtv0();
                gte_stsv(&work->pos);
                gte_lddp(0xCC);
                gte_ldsv(&work->pos);
                gte_gpf12();
                gte_stsv(&work->pos);
                task->state = 1;
                break;
            case 1:
                coord->coord.t[0] += work->pos.vx;
                coord->coord.t[1] += work->pos.vy;
                coord->coord.t[2] += work->pos.vz;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    func_shelter_b4_upper_sewer_80184E44(coord, ++work->index, 0x200, 0x80);
                }
                if (work->age >= 20) {
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
void func_shelter_b4_upper_sewer_80184E44(GpCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            tex;
    s32            u0;
    s32            u1;
    s32            sarg;
    s32            t;
    s16            xy;
    u16            vz;

    tex                                     = arg1;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = (u16)arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = (u16)arg0->workm.t[1];
    vz                                      = (u16)arg0->workm.t[2];
    *scratch                                = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
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
        prim->v0    = 0;
        prim->v1    = 0;
        block->step = (t - sarg) / block->otz;
        xy          = (u16)block->sx - (u16)block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = (u16)block->sx + (u16)block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = (u16)block->sy - (u16)block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = (u16)block->sy + (u16)block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

/// The ring `func_shelter_b4_upper_sewer_801806A0` draws, compiled with a
/// different scratch block layout: sixteen gouraud quads around the
/// coordinate's projected position, black at screen radius
/// `arg1 * 64 / (otz + 1)` and coloured `rgb` at `(arg1 + arg2) * 64 / (otz + 1)`.
void func_shelter_b4_upper_sewer_801850C8(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    GpArcScratch* block;
    POLY_G4*      prim;
    s32           ang;
    s32           next;
    s32           litRadius = arg1 + arg2;

    SCRATCH_PUSH(GpArcScratch);
    block         = SCRATCH_HEAD(GpArcScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->inner = ((s16)arg1 * 64) / block->otz;
        block->outer = ((s16)litRadius * 64) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->inner * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->inner * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->inner * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->outer * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->outer * rcos(next)) >> 12);
            ang      = next;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(GpArcScratch);
}

/// Draws a glowing disc at the coordinate's world position, unless the
/// projection flags an error: eight gouraud wedges coloured `rgb` at the
/// projected centre and black at the rim, of screen radius
/// `arg1 * 64 / (otz + 1)`.
void func_shelter_b4_upper_sewer_801854EC(GpCoord* arg0, s32 arg1, u8* rgb)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;
    register s32   ang2 asm("s1");
    u16            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                      = (u16)arg0->workm.t[0];
        ((GpRingScratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (GpRingScratch*)tmp;
    }
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
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
            prim->x0 = (u16)block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = (u16)block->sy + ((block->step * rcos(ang)) >> 12);
            ang2     = ang + 0x100;
            prim->x1 = (u16)block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y1 = (u16)block->sy + ((block->step * rcos(ang2)) >> 12);
            prim->x2 = (u16)block->sx;
            prim->y2 = (u16)block->sy;
            ang2     = ang + 0x200;
            prim->x3 = (u16)block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y3 = (u16)block->sy + ((block->step * rcos(ang2)) >> 12);
            ang      = ang2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Effect task of a burst of light: each frame it draws an orange disc and the
/// glow of `func_shelter_b4_upper_sewer_80185A2C`, both growing, and while the
/// ring's brightness lasts an expanding orange ring that dims each frame. Once
/// the ring is gone the disc dims instead, and the task is released when it is
/// nearly black. While an event runs it does nothing, and is released once the
/// event state reaches 4.
void func_shelter_b4_upper_sewer_80185880(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    u8         sp10[3];
    u16        temp;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        if (task->state == 0) {
            work->age    = 1;
            work->scale  = 0xE0;
            work->angle  = 0x80;
            work->period = 0xE0;
            work->step   = 0x80;
            task->state  = 1;
        }
        Gp_UpdateCoord(coord);
        sp10[0]     = (u8)work->scale;
        sp10[1]     = (u8)(work->scale >> 1);
        sp10[2]     = (u8)(work->scale >> 2);
        temp        = work->angle + 0x10;
        work->angle = temp;
        func_shelter_b4_upper_sewer_801854EC(coord, (s16)(temp * 2), sp10);
        func_shelter_b4_upper_sewer_80185A2C(coord, work->angle);
        if (work->period >= 0x19) {
            u32 temp_a1;
            sp10[0] = (u8)work->period;
            sp10[1] = (u8)(work->period >> 1);
            sp10[2] = (u8)(work->period >> 2);
            temp_a1 = work->step * 3;
            func_shelter_b4_upper_sewer_801850C8(coord, (s32)((temp_a1 + (temp_a1 >> 0x1F)) << 0xF) >> 0x10, 0x60, sp10);
            work->period -= 0x18;
            work->step   += 0x30;
            return;
        }
        temp        = work->scale - 0x18;
        work->scale = temp;
        if ((s16)temp < 0x18) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Glow at a coordinate: two camera-facing textured quads, an inner one of
/// half-extent `size` and an outer one of `size * 3 / 2`, plus a ground mark
/// under it. Also feeds the `Gp_RoomCoords[2]` light a flickering intensity at the
/// coordinate's position. Draws nothing when the point fails to project.
void func_shelter_b4_upper_sewer_80185A2C(GpCoord* coord, s16 size)
{
    GpCoord        ground;
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
    block                       = SCRATCH_HEAD_AT(scratch, GpRingScratch) - 1;
    block->vec.vx               = (u16)coord->workm.t[0];
    alias                       = block;
    vy                          = (u16)coord->workm.t[1];
    __asm__("move %0,%1" : "=r"(alias) : "r"(alias), "r"(vy), "r"(alias));
    sc          = alias;
    sc->vec.vy  = vy;
    sc->vec.vz  = (u16)coord->workm.t[2];
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
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
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
        left     = sc->sx - sc->step;
        prim->x2 = left;
        prim->x0 = left;
        right    = sc->sx + sc->step;
        prim->x3 = right;
        prim->x1 = right;
        top      = sc->sy - sc->step;
        prim->y1 = top;
        prim->y0 = top;
        bottom   = sc->sy + sc->step;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (s16)(((u32)(((gDisplayState.animFrame & 1) * 0x10) + 0x120) >> 4) |
                  0x4300);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)((s16)size * 3 / 2);
        sc->step    = (s32)((s32)(outerSize * 0x37) / (s32)sc->otz);
        outerLeft   = sc->sx - sc->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = sc->sx + sc->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = sc->sy - sc->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = sc->sy + sc->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_shelter_b4_upper_sewer_80185F58(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Draws a flat mark: a quad of half-size `arg1` laid flat in view space
/// through `gGfxViewCoord.workm` and moved to the coordinate's position, then
/// projected through `GsWSMATRIX`. Queues one semi-transparent textured quad
/// (tpage 0x28, clut 0x428C) tinted (0x30, 0x20, 0x20), alternating between two
/// texture frames on odd and even frames, unless the projection flags an error.
void func_shelter_b4_upper_sewer_80185F58(GpCoord* arg0, s32 arg1)
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
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        (u16) v->vx = (u16)v->vx + (u16)arg0->workm.t[0];
        tbl++;
        (u16) v->vy = (u16)v->vy + (u16)arg0->workm.t[1];
        i++;
        (u16) v->vz = (u16)v->vz + (u16)arg0->workm.t[2];
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
        prim->x0    = (u16)block->sxy0.vx;
        prim->y0    = (u16)block->sxy0.vy;
        prim->x1    = (u16)block->sxy1.vx;
        prim->y1    = (u16)block->sxy1.vy;
        prim->x2    = (u16)block->sxy2.vx;
        prim->y2    = (u16)block->sxy2.vy;
        prim->x3    = (u16)block->sxy3.vx;
        prim->y3    = (u16)block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}
