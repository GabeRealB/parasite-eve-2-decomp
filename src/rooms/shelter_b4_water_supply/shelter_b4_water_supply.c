#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

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
#include "rooms/rooms_shared_8017e4f8.h"

/// One water surface: a rectangle at (`x`, `z`) spanning `width` along X and
/// `depth` along Z. A list of them ends at an entry whose `end` is -1; `end`
/// is not otherwise read.
typedef struct ShelterB4WaterSupplySurface {
    s16 x;
    s16 z;
    s16 width;
    s16 depth;
    s16 end;
} ShelterB4WaterSupplySurface;

/// Block the room task `func_shelter_b4_water_supply_8017EE54` receives as
/// `spawnArg2`. Only the halfword at 0x26 is touched there: an effect
/// strength, set from how far a tracked part moved this frame and used as the
/// odds of spawning each of the two effects.
typedef struct _ShelterB4WaterSupplySplash {
    byte pad_0[0x26];
    s16  strength;
} _ShelterB4WaterSupplySplash;

extern s32 D_80070F70;
extern s16 D_80071076;
extern s32 D_8007107C;
extern s8  D_8007217B;
extern s16 D_80114D08;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115738;
extern s32 D_8011574C;
extern s32 D_80115754;

extern s32 func_80179A04(GpSaveLoc* in, GpSaveLoc* out);

/// Descriptor of the departure task spawned once the event block is staged.
extern TaskDesc D_shelter_b4_water_supply_801825E4;

/// The room's message table, installed by the room task's first state.
extern GpMsgEntry D_shelter_b4_water_supply_801825F0[];

/// Task table spawned by `func_shelter_b4_water_supply_8017DA30` once the
/// valve script has run.
extern TaskDesc D_shelter_b4_water_supply_80182620[];

/// Height of the water surfaces.
extern s16 D_shelter_b4_water_supply_80182638;

/// Tasks the room task's first state spawns.
extern TaskDesc D_shelter_b4_water_supply_8018263C[];

/// The room's water surfaces whose strips run along Z.
extern ShelterB4WaterSupplySurface D_shelter_b4_water_supply_80182648[];

/// The room's water surfaces whose strips run along X.
extern ShelterB4WaterSupplySurface D_shelter_b4_water_supply_8018265C[];

/// End-point pairs of the light beams the room task draws per view.
extern SVECTOR D_shelter_b4_water_supply_80182670[];
extern SVECTOR D_shelter_b4_water_supply_80182680[];
extern SVECTOR D_shelter_b4_water_supply_80182690[];
extern SVECTOR D_shelter_b4_water_supply_801826A0[];
extern SVECTOR D_shelter_b4_water_supply_801826C0[];
extern SVECTOR D_shelter_b4_water_supply_801826D0[];

/// Last-frame world positions of the two tracked parts of the slot-3 task's
/// model, compared against this frame's to measure how far each moved.
extern SVECTOR D_shelter_b4_water_supply_801826E0[];

/// Per-tint channel shifts for the glowing disc, indexed by the tint the spawn
/// argument selects.
extern RoomHaloShade D_shelter_b4_water_supply_801826F0[];

/// Spawn argument for the task `func_shelter_b4_water_supply_8017D7C0` starts
/// with `Task_Spawn(1, 0x31, ...)`.
extern GpFadeWork D_shelter_b4_water_supply_80184E34;

/// Staging save location the spawned task reads: `field_2` / `field_4` /
/// `field_1` receive the outgoing location's `field_0` / `field_2` / `field_3`.
extern GpSaveLoc D_shelter_b4_water_supply_80184E3C;

/// The staged event block, read by the departure task.
extern RoomDeparture D_shelter_b4_water_supply_80184E44;

/// Cursor into the primitive area the water surface is written to.
extern u8* D_shelter_b4_water_supply_80184E50;

void func_shelter_b4_water_supply_8017DB18(void);
void func_shelter_b4_water_supply_8017DD40(Task* arg0);
void func_shelter_b4_water_supply_8017DD9C(Task* task);
s32  func_shelter_b4_water_supply_8017DDFC(RoomEventMsg* in, RoomEventMsg* out);
void func_shelter_b4_water_supply_8017DE74(Task* task);
void func_shelter_b4_water_supply_8017E5D8(Task* task);
void func_shelter_b4_water_supply_8017ED90(Task* arg0);
void func_shelter_b4_water_supply_8017EDD0(Task* task);
void func_shelter_b4_water_supply_8017F3A0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_shelter_b4_water_supply_8017FB90(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b4_water_supply_8017FF7C(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);
void func_shelter_b4_water_supply_80180260(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b4_water_supply_80181158(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b4_water_supply_801813DC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b4_water_supply_80181800(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_b4_water_supply_80181D40(GsCOORDINATE2* coord, s16 size);
void func_shelter_b4_water_supply_8018226C(GsCOORDINATE2* arg0, s32 arg1);

/// The task the staged event block `D_shelter_b4_water_supply_80184E44`
/// spawns. State 0 sends the block's `facing` to the slot-3 game pointer as
/// message 0x3EE, skipping to state 2 when it is -1; state 1 waits until
/// that pointer answers 0x3F0 with 0. States 2 and 3 play the block's sound
/// event `sndEvent`, if any, and wait for its voice to go quiet. State 4 queues
/// type-7 sound event 0x80000000, commits the save location in the block's
/// first four bytes (stage, area, warp, room), re-spawns the player task as
/// type 0x11 and kills itself.
void func_shelter_b4_water_supply_8017D650(Task* arg0)
{
    GpXformArg msg;
    void*      slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            msg.rot.vy = D_shelter_b4_water_supply_80184E44.facing;
            if (msg.rot.vy == -1) {
                arg0->state = 2;
                break;
            }
            Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (D_shelter_b4_water_supply_80184E44.sndEvent == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_shelter_b4_water_supply_80184E44.sndEvent, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_shelter_b4_water_supply_80184E44.sndEvent) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            D_80071076                = 1;
            Mc_SaveData.at4.loc.stage = D_shelter_b4_water_supply_80184E44.stage;
            Mc_SaveData.at4.loc.area  = D_shelter_b4_water_supply_80184E44.area;
            Mc_SaveData.at4.loc.warp  = D_shelter_b4_water_supply_80184E44.warp;
            Mc_SaveData.at4.loc.room  = D_shelter_b4_water_supply_80184E44.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

/// The room task's state table, dispatched by
/// `func_shelter_b4_water_supply_8017DDA4` from a stack copy: install the
/// message table and spawn the room's tasks, idle, then kill.
const TaskFuncTable3 D_shelter_b4_water_supply_8017D5D8 = {
    {
        func_shelter_b4_water_supply_8017DD40,
        func_shelter_b4_water_supply_8017DD9C,
        taskKill,
    },
};

void func_shelter_b4_water_supply_8017D7C0(Task* arg0)
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
            D_shelter_b4_water_supply_80184E34.field_0 = 0;
            D_shelter_b4_water_supply_80184E34.field_1 = 0;
            D_shelter_b4_water_supply_80184E34.field_2 = 0x1E;
            Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b4_water_supply_80184E34);
            arg0->killCountdown = 0x1E;
            arg0->state++;
            break;
        case 3:
            if (--arg0->killCountdown == 0) {
                SndEvt_EnqueueType6(0x542E0005, 0, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(0x542E0005) == 0) {
                arg0->state++;
            }
            break;
        case 5:
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b4_water_supply_80184E3C.field_2;
            Mc_SaveData.at4.loc.warp = D_shelter_b4_water_supply_80184E3C.field_4;
            Mc_SaveData.at4.loc.room = D_shelter_b4_water_supply_80184E3C.field_1;
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b4_water_supply_8017D970(void)
{
    return 0;
}

/// Copies the location at `src` into `dst` and passes both to `func_80179A04`.
/// When the leading halfword of `src` is 0x2C it returns 0, first staging three
/// bytes of `dst` and spawning from the task table unless `src->field_5` is set;
/// any other location returns 1.
s32 func_shelter_b4_water_supply_8017D978(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179A04(src, dst);
    if (*(u16*)src == 0x2C) {
        if (src->field_5 == 0) {
            D_shelter_b4_water_supply_80184E3C.field_2 = dst->field_0;
            D_shelter_b4_water_supply_80184E3C.field_4 = dst->field_2;
            D_shelter_b4_water_supply_80184E3C.field_1 = dst->field_3;
            Task_SpawnFromTable(D_shelter_b4_water_supply_80182620, 1, 4, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_shelter_b4_water_supply_8017DA28(void)
{
    return 0;
}

/// Handler for slot-7 msg `0x13EF` in `D_shelter_b4_water_supply_801825F0`:
/// the directed action on the water-supply valve (`field_2` 0xA / `field_3`
/// 0x20).
s32 func_shelter_b4_water_supply_8017DA30(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 0xA) {
        if (arg2->field_3 == 0x20) {
            if (GameFlag_GetNibble(0xB8) != 0) {
                if (GameFlag_GetNibble(0x139) != 0) {
                    func_shelter_b4_water_supply_8017DB18();
                } else {
                    GameFlag_SetNibble(0x139, 1);
                    Gp_MsgPlayerWeapon(0);
                    Gp_RunCapCmd1(3);
                    Task_SpawnFromTable(D_shelter_b4_water_supply_80182620, 0, 0, 0);
                }
            } else {
                Gp_RunCapCmd1(1);
                GameFlag_SetNibble(0x1BD, 2);
            }
        }
    }
    return 0;
}

s32 func_shelter_b4_water_supply_8017DAE4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 6) {
        SndEvt_EnqueueType6(0x542E0000 | 6, 0, 0);
    }
    return 0;
}

void func_shelter_b4_water_supply_8017DB18(void)
{
    RoomDeparture  work;
    RoomEventMsg   param;
    RoomDeparture* wp;
    s32            (*resolve)(RoomEventMsg*, RoomEventMsg*) = func_shelter_b4_water_supply_8017DDFC;

    work.stage    = 3;
    work.area     = 0x20;
    work.warp     = 3;
    work.room     = 1;
    work.sndEvent = 0x542E0003;
    work.facing   = 0x400;
    Gp_MsgPlayerWeapon(0);
    wp            = &work;
    param.msgId   = wp->area;
    param.field_2 = wp->warp;
    param.field_3 = wp->room;
    param.field_5 = 0;
    resolve(&param, &param);
    wp->area                           = param.msgId;
    wp->warp                           = param.field_2;
    wp->room                           = param.field_3;
    D_shelter_b4_water_supply_80184E44 = work;
    Task_SpawnFromTable(&D_shelter_b4_water_supply_801825E4, 0, 0, 0);
    if (gameGetPtrSlot(0xA) != NULL && GameFlag_GetNibble(0xCF) == 0) {
        GameFlag_SetNibble(0x4C, 6);
    }
}

void func_shelter_b4_water_supply_8017DC28(Task* arg0)
{
    RoomDeparture work;
    RoomEventMsg  param;
    s32           (*resolve)(RoomEventMsg*, RoomEventMsg*);

    if (Gp_CapBusy() == 0) {
        resolve       = func_shelter_b4_water_supply_8017DDFC;
        work.stage    = 3;
        work.area     = 0x20;
        work.warp     = 3;
        work.room     = 1;
        work.sndEvent = 0x542E0003;
        work.facing   = 0x400;
        Gp_MsgPlayerWeapon(0);
        param.msgId   = work.area;
        param.field_2 = work.warp;
        param.field_3 = work.room;
        param.field_5 = 0;
        resolve(&param, &param);
        work.area                          = param.msgId;
        work.warp                          = param.field_2;
        work.room                          = param.field_3;
        D_shelter_b4_water_supply_80184E44 = work;
        Task_SpawnFromTable(&D_shelter_b4_water_supply_801825E4, 0, 0, 0);
        if (gameGetPtrSlot(0xA) != NULL && GameFlag_GetNibble(0xCF) == 0) {
            GameFlag_SetNibble(0x4C, 6);
        }
        taskKill(arg0);
    }
}

/// The room task's first state: installs the room's message table, registers
/// the task in game pointer slot 7, spawns the room's tasks and advances.
void func_shelter_b4_water_supply_8017DD40(Task* arg0)
{
    arg0->msgTable = D_shelter_b4_water_supply_801825F0;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_shelter_b4_water_supply_8018263C, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

/// The room task's idle state.
void func_shelter_b4_water_supply_8017DD9C(Task* task)
{
}

/// The room task: copies the three-state table
/// `D_shelter_b4_water_supply_8017D5D8` onto the stack and runs the entry for
/// the task's current state.
void func_shelter_b4_water_supply_8017DDA4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b4_water_supply_8017D5D8;
    sp.funcs[task->state](task);
}

/// Message 0x20: unless a report-only query, answers in `field_3` from
/// nibbles 0x51 (1 when set, 2 when clear) and 0x53 (adds 2 when set).
/// Always returns 1 (not consumed).
s32 func_shelter_b4_water_supply_8017DDFC(RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->msgId == 0x20 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0x51) == 0) {
            out->field_3 = 2;
        } else {
            out->field_3 = 1;
        }
        if (GameFlag_GetNibble(0x53) != 0) {
            out->field_3 = (u8)out->field_3 + 2;
        }
    }
    return 1;
}

/// Draws each surface in `D_shelter_b4_water_supply_80182648` at height
/// `D_shelter_b4_water_supply_80182638` as two strips of 16 semi-transparent
/// Gouraud quads laid side by side along X, each strip running along Z and
/// projected through the view matrix. The seam between the strips is lifted by
/// a sine wave that runs along Z and scrolls with the display frame counter.
/// The outer edges are coloured (0x80, 0, 0) and the seam (0x20, 0x20, 0x20);
/// each quad is followed by a draw-mode packet selecting blend mode 2. Quads
/// the projection flags as invalid are skipped. The per-surface values live in
/// a work block pushed on the scratchpad stack for the duration of the call.
/// `task`, the water task whose drawing state calls it, is unused.
void func_shelter_b4_water_supply_8017DE74(Task* task)
{
    SVECTOR                      v0, v1, v2, v3;
    s32                          sxy0, sxy1, sxy2, sxy3;
    s32                          p, flag;
    s32                          phase;
    ShelterB4WaterSupplySurface* e;
    RoomWaterScratch*            w;
    u8*                          head;
    POLY_G4*                     poly;
    DR_MODE*                     dr;
    s32                          otz;
    s32                          i;

    e                 = D_shelter_b4_water_supply_80182648;
    gGfxViewCoord.flg = 0;
    head              = SCRATCH_HEAD(u8);
    phase             = -(gDisplayState.animFrame * 16);
    SCRATCH_HEAD(u8)  = head - 0xC;
    w                 = (RoomWaterScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    w->y = D_shelter_b4_water_supply_80182638;
    for (; e->end != -1; e++) {
        w->dx = e->width / 2;
        w->dz = e->depth / 16;
        w->x  = e->x;
        w->z  = e->z;
        for (i = 0; i < 16; i++) {
            v0.vx   = w->x;
            v0.vy   = w->y;
            v0.vz   = w->z + w->dz * i;
            v1.vx   = w->x;
            v1.vy   = w->y;
            v1.vz   = w->z + w->dz * (i + 1);
            w->wave = (u32)rsin(phase + (i << 9)) >> 6;
            v2.vx   = w->x + w->dx;
            v2.vy   = w->y + w->wave;
            v2.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v3.vx   = w->x + w->dx;
            v3.vy   = w->y + w->wave;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                               = (POLY_G4*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0x80;
                poly->r1         = 0x80;
                poly->g0         = 0;
                poly->b0         = 0;
                poly->g1         = 0;
                poly->b1         = 0;
                poly->r2         = 0x20;
                poly->g2         = 0x20;
                poly->b2         = 0x20;
                poly->r3         = 0x20;
                poly->g3         = 0x20;
                poly->b3         = 0x20;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                                 = (DR_MODE*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
        for (i = 0; i < 16; i++) {
            w->wave = (u32)rsin(phase + (i << 9)) >> 6;
            v0.vx   = w->x + w->dx;
            v0.vy   = w->y + w->wave;
            v0.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v1.vx   = w->x + w->dx;
            v1.vy   = w->y + w->wave;
            v1.vz   = w->z + w->dz * (i + 1);
            v2.vx   = w->x + w->dx * 2;
            v2.vy   = w->y;
            v2.vz   = w->z + w->dz * i;
            v3.vx   = w->x + w->dx * 2;
            v3.vy   = w->y;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                               = (POLY_G4*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r2         = 0x80;
                poly->r3         = 0x80;
                poly->g2         = 0;
                poly->b2         = 0;
                poly->g3         = 0;
                poly->b3         = 0;
                poly->r0         = 0x20;
                poly->g0         = 0x20;
                poly->b0         = 0x20;
                poly->r1         = 0x20;
                poly->g1         = 0x20;
                poly->b1         = 0x20;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                                 = (DR_MODE*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

/// Draws each surface in `D_shelter_b4_water_supply_8018265C` at height
/// `D_shelter_b4_water_supply_80182638` as two strips of 16 semi-transparent
/// Gouraud quads laid side by side along Z, projected through the view matrix.
/// The seam between the strips is lifted by a sine wave that runs along X and
/// scrolls with the display frame counter. The outer edges are coloured
/// (0x80, 0, 0) and the seam (0x20, 0x20, 0x20); each quad is followed by a
/// draw-mode packet selecting blend mode 2. Quads the projection flags as
/// invalid are skipped. The per-surface values live in a work block pushed on
/// the scratchpad stack for the duration of the call. `task`, the water task
/// whose drawing state calls it, is unused.
void func_shelter_b4_water_supply_8017E5D8(Task* task)
{
    SVECTOR                      v0, v1, v2, v3;
    s32                          sxy0, sxy1, sxy2, sxy3;
    s32                          p, flag;
    s32                          phase;
    ShelterB4WaterSupplySurface* e;
    RoomWaterScratch*            w;
    u8*                          head;
    POLY_G4*                     poly;
    DR_MODE*                     dr;
    s32                          otz;
    s32                          i;

    e                 = D_shelter_b4_water_supply_8018265C;
    gGfxViewCoord.flg = 0;
    head              = SCRATCH_HEAD(u8);
    phase             = -(gDisplayState.animFrame * 16);
    SCRATCH_HEAD(u8)  = head - 0xC;
    w                 = (RoomWaterScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    w->y = D_shelter_b4_water_supply_80182638;
    for (; e->end != -1; e++) {
        w->dx = e->width / 16;
        w->dz = e->depth / 2;
        w->x  = e->x;
        w->z  = e->z;
        for (i = 0; i < 16; i++) {
            v0.vx   = w->x + w->dx * i;
            v0.vy   = w->y;
            v0.vz   = w->z;
            v1.vx   = w->x + w->dx * (i + 1);
            v1.vy   = w->y;
            v1.vz   = w->z;
            w->wave = (u32)rsin(phase + (i << 9)) >> 6;
            v2.vx   = w->x + w->dx * i;
            v2.vy   = w->y + w->wave;
            v2.vz   = w->z + w->dz;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v3.vx   = w->x + w->dx * (i + 1);
            v3.vy   = w->y + w->wave;
            v3.vz   = w->z + w->dz;
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                               = (POLY_G4*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r0         = 0x80;
                poly->r1         = 0x80;
                poly->g0         = 0;
                poly->b0         = 0;
                poly->g1         = 0;
                poly->b1         = 0;
                poly->r2         = 0x20;
                poly->g2         = 0x20;
                poly->b2         = 0x20;
                poly->r3         = 0x20;
                poly->g3         = 0x20;
                poly->b3         = 0x20;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                                 = (DR_MODE*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
        for (i = 0; i < 16; i++) {
            w->wave = (u32)rsin(phase + (i << 9)) >> 6;
            v0.vx   = w->x + w->dx * i;
            v0.vy   = w->y + w->wave;
            v0.vz   = w->z + w->dz;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v1.vx   = w->x + w->dx * (i + 1);
            v1.vy   = w->y + w->wave;
            v1.vz   = w->z + w->dz;
            v2.vx   = w->x + w->dx * i;
            v2.vy   = w->y;
            v2.vz   = w->z + w->dz * 2;
            v3.vx   = w->x + w->dx * (i + 1);
            v3.vy   = w->y;
            v3.vz   = w->z + w->dz * 2;
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                               = (POLY_G4*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                poly->r2         = 0x80;
                poly->r3         = 0x80;
                poly->g2         = 0;
                poly->b2         = 0;
                poly->g3         = 0;
                poly->b3         = 0;
                poly->r0         = 0x20;
                poly->g0         = 0x20;
                poly->b0         = 0x20;
                poly->r1         = 0x20;
                poly->g1         = 0x20;
                poly->b1         = 0x20;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                                 = (DR_MODE*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

/// The water task: runs its current state - `func_shelter_b4_water_supply_8017ED90`
/// once, then `func_shelter_b4_water_supply_8017EDD0`, which draws the surfaces -
/// and each tick publishes the room's water height to the session.
void func_shelter_b4_water_supply_8017ED28(Task* task)
{
    TaskFunc states[2] = { func_shelter_b4_water_supply_8017ED90, func_shelter_b4_water_supply_8017EDD0 };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_water_supply_80182638;
}

/// The water task's opening state: clears the session's `field_80` or
/// `field_7E`, chosen by `D_8007217B`, and advances the task to its next state.
void func_shelter_b4_water_supply_8017ED90(Task* arg0)
{
    if (D_8007217B == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// The water task's drawing state: points the primitive cursor
/// `D_shelter_b4_water_supply_80184E50` at the current buffer's 0xC000-byte
/// slice of one of two primitive areas, chosen by `D_8007217B`, then draws both
/// lists of water surfaces.
void func_shelter_b4_water_supply_8017EDD0(Task* task)
{
    if (D_8007217B == 0) {
        D_shelter_b4_water_supply_80184E50 = (u8*)D_8005C374 + D_8007107C * 0xC000;
    } else {
        D_shelter_b4_water_supply_80184E50 = (u8*)D_8005C370 + D_8007107C * 0xC000;
    }
    func_shelter_b4_water_supply_8017DE74(task);
    func_shelter_b4_water_supply_8017E5D8(task);
}

/// Room task. State 0 installs five effect ids in the shared effect-id slots,
/// records the world positions of parts 14 and 17 of the slot-3 task's model,
/// and advances. Later states, while no event is running and `waterY` is below
/// that model's root, spawn each of two effects at water level under each part
/// with odds that grow with how far the part moved since last frame. Every
/// frame it then draws the light beams the current view selects, through
/// `func_shelter_b4_water_supply_80180260`.
void func_shelter_b4_water_supply_8017EE54(Task* arg0)
{
    Task*                        ctl;
    _ShelterB4WaterSupplySplash* splash;
    GsCOORDINATE2*               ctlCoords;
    GsCOORDINATE2*               part;
    GsCOORDINATE2                surface;
    MATRIX*                      mtx;
    GsCOORDINATE2*               view;
    s32                          i;
    u32                          rnd;

    splash    = arg0->spawnArg2;
    ctl       = gameGetPtrSlot(3);
    ctlCoords = ctl->extra.tmd->coords;
    if (arg0->state == 0) {
        D_8011574C  = 0x60174;
        D_80115738  = 0x60175;
        D_80115734  = 0x60226;
        D_80115730  = 0x60231;
        D_80115754  = 0x6023C;
        arg0->state = 1;
        for (i = 0; i < 2; i++) {
            part                                     = &ctl->extra.tmd->coords[14 + i * 3];
            D_shelter_b4_water_supply_801826E0[i].vx = part->workm.t[0];
            D_shelter_b4_water_supply_801826E0[i].vy = part->workm.t[1];
            D_shelter_b4_water_supply_801826E0[i].vz = part->workm.t[2];
        }
    } else if (Gp_State1C->eventState == 0 && gGameSession->waterY < ctlCoords->coord.t[1]) {
        view = &gGfxViewCoord;
        i    = 0;
        mtx  = &Gfx_ViewWorldMtx;
        for (; i < 2; i++) {
            part = &ctl->extra.tmd->coords[14 + i * 3];
            Gp_UpdateCoord(part);
            splash->strength = ABS(D_shelter_b4_water_supply_801826E0[i].vx - part->workm.t[0]) +
                               ABS(D_shelter_b4_water_supply_801826E0[i].vy - part->workm.t[1]) +
                               ABS(D_shelter_b4_water_supply_801826E0[i].vz - part->workm.t[2]) + 0x20;
            Gp_WorldToLocal(mtx, &part->workm, &surface.coord);
            surface.sub        = view;
            surface.coord.t[1] = gGameSession->waterY;
            surface.flg        = 0;
            Gp_UpdateCoord(&surface);
            rnd = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911);
            if ((s32)((rnd >> 16) & 0x1FF) < splash->strength) {
                Gp_SpawnEff(D_8011574C, &surface, 0x40, 0);
            }
            splash->strength -= 0x20;
            rnd               = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911);
            if ((s32)((rnd >> 16) & 0x1FF) < splash->strength) {
                Gp_SpawnEff(D_80115738, &surface, 0x1202180, 0);
            }
            D_shelter_b4_water_supply_801826E0[i].vx = part->workm.t[0];
            D_shelter_b4_water_supply_801826E0[i].vy = part->workm.t[1];
            D_shelter_b4_water_supply_801826E0[i].vz = part->workm.t[2];
        }
    }
    switch ((u8)Gp_GetViewIndex()) {
        case 4:
            func_shelter_b4_water_supply_80180260(&D_shelter_b4_water_supply_80182690[0], 0x200, 0);
            func_shelter_b4_water_supply_80180260(&D_shelter_b4_water_supply_80182690[4], 0x200, 0x800);
        case 2:
        case 3:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_80182670, 0x200, 0x800);
            break;
        case 6:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_801826A0, 0x200, 0);
        case 5:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_80182680, 0x200, 0x800);
            break;
        case 7:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_801826A0, 0x200, 0);
            break;
        case 8:
            func_shelter_b4_water_supply_80180260(&D_shelter_b4_water_supply_80182690[0], 0x200, 0);
            func_shelter_b4_water_supply_80180260(&D_shelter_b4_water_supply_80182690[4], 0x200, 0x800);
            break;
        case 9:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_801826D0, 0x200, 0x800);
        case 10:
        case 11:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_801826C0, 0x200, 0x800);
            break;
    }
}

/// Per-frame driver of an expanding, fading flash. While the room's event
/// state is 0 it updates the task's coordinate, ticks the age counter and
/// draws the flash through `func_shelter_b4_water_supply_8017F3A0` at size
/// `angle`, seeded from the spawn argument and grown by 0x20 a frame, and
/// brightness `scale`, which starts at 0x40 and drops by 2 a frame; the
/// first frame also turns the coordinate about Y by a random angle. The work
/// block is released once the brightness falls under 2. Once the event state
/// is non-zero it only draws, releasing the block from event state 4 on.
void func_shelter_b4_water_supply_8017F24C(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_b4_water_supply_8017F3A0(coord, work->angle, work->scale);
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
        func_shelter_b4_water_supply_8017F3A0(coord, work->angle, work->scale);
        work->scale -= 2;
        if (work->scale < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a flat textured quad at `arg0`: the four corners of the unit quad
/// `D_80111E38`, scaled by `arg1`, are rotated by the coordinate's world
/// matrix and offset by its translation, then projected through `GsWSMATRIX`.
/// If the projection is valid, one semi-transparent `POLY_FT4` (tpage 0x2B,
/// clut 0x43D1, UV 0,0x38 to 0x37,0x6F) is queued with all three colour
/// channels set to `arg2`. The work block lives on the scratchpad stack.
void func_shelter_b4_water_supply_8017F3A0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2)
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
    SCRATCH_POP_BYTES(0x38);
}

/// Per-frame driver of a particle, drawn as the spinning sprite of
/// `func_shelter_b4_water_supply_8017FB90` or, when the spawn argument's top
/// nibble is set, the upright sprite of `func_shelter_b4_water_supply_8017FF7C`.
/// The first frame takes the size from the argument's low 12 bits, a random
/// angle, and the ticks per animation frame from bits 12-15. Unless the work
/// block already carries a velocity it picks one by the kind in bits 24-27 -
/// none, a random upward burst, a random spray, a narrow upward jet, or the
/// work block's stored direction - scaled to the speed in bits 16-23 (0x40
/// when zero). Every later tick draws, moves the coordinate by the velocity
/// with gravity pulling it down, and releases the block after animation frame
/// 7. While the room's event state is non-zero it only draws, releasing the
/// block from event state 4 on.
void func_shelter_b4_water_supply_8017F6D4(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            kind;
    s32            step;
    s32            state;
    s32            level;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_shelter_b4_water_supply_8017FB90(coord, work->index, work->scale, work->angle);
            } else {
                func_shelter_b4_water_supply_8017FF7C(coord, work->index, work->scale);
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
            func_shelter_b4_water_supply_8017FB90(coord, work->index, work->scale, work->angle);
            break;
        case 2:
            func_shelter_b4_water_supply_8017FF7C(coord, work->index, work->scale);
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

/// Draws a spinning sprite at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, one
/// semi-transparent, unshaded `POLY_FT4` (tpage 0x2B, clut 0x43D3) is queued
/// as a square rotated by angle `arg3` about the projected point, with
/// on-screen half-diagonal `(s16)arg2 * 31 / otz`. `arg1` picks the 32-texel
/// frame at u = `arg1 * 32`, v 0xE0 to 0xFF.
void func_shelter_b4_water_supply_8017FB90(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
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
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = *(u16*)&arg0->workm.t[1];
    vz                                        = *(u16*)&arg0->workm.t[2];
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
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}

/// Draws an upright sprite at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, one
/// semi-transparent, unshaded `POLY_FT4` (tpage 0x2B, clut 0x43D2) is queued
/// as an axis-aligned square of half-side `r = arg2 * 55 / otz`, raised so the
/// projected point sits three quarters of the way down it. `arg1` picks one of
/// eight 56-texel frames in a grid four wide, starting at v 0x70.
void func_shelter_b4_water_supply_8017FF7C(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
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
    ((GpRingScratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    vy                                      = *(u16*)&arg0->workm.t[1];
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
        xy          = *(u16*)&block->sx - *(u16*)&block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = *(u16*)&block->sx + *(u16*)&block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = (*(u16*)&block->sy - *(u16*)&block->step) - (block->step >> 1);
        ds          = &gDisplayState;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = *(u16*)&block->sy + (block->step >> 1);
        prim->y3    = xy;
        prim->y2    = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Draws a flickering grey light beam from `arg0[0]` to `arg0[1]`. Both points
/// are projected through the view matrix; unless the far end is nearer than
/// OTZ 0x11, gouraud `POLY_G4` wedges around each end (radius
/// `(s16)arg1 * 64 / otz` at that end) are joined by quads between the two,
/// each fading from grey on the axis to black at the rim. `arg2` turns the
/// wedges about the axis. The grey alternates between 0x20 and 0x28 with the
/// display frame counter.
void func_shelter_b4_water_supply_80180260(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg1 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        ang       = 0;
        base      = (s16)arg2;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = rgb;
            p->g2    = rgb;
            prim->b2 = rgb;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(base + t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            SCHED_BARRIER();
            t3             = ang - 0x1000;
            prim           = (POLY_G4*)gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->r1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Per-frame driver of a glowing disc anchored to its parent at the work
/// block's position. State 1 grows the disc and, every fourth tick, spawns the
/// effect `D_80115730` names at a random one of joints 3-18 of the slot-3
/// task's model and adopts it as a child task; state 2 keeps growing it and
/// adds a half-bright second disc on odd ticks; state 3 drifts the disc away
/// while it fades inside an expanding ring, then releases the work block, as
/// does state 4. The spawn argument picks the disc's tint from
/// `D_shelter_b4_water_supply_801826F0`. Nothing runs while the room's event
/// state is set, and the block is released once that state reaches 4.
void func_shelter_b4_water_supply_801809DC(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpEffWork*     spawned;
    MATRIX*        mtx;
    u8             col[4];

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
            col[0] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1].r;
            col[1] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1].g;
            col[2] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1].b;
            func_shelter_b4_water_supply_80181800(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1].r;
            col[1] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1].g;
            col[2] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1].b;
            func_shelter_b4_water_supply_80181800(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_shelter_b4_water_supply_80181800(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1].r;
            col[1] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1].g;
            col[2] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1].b;
            func_shelter_b4_water_supply_80181800(coord, mem->angle, col);
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
            func_shelter_b4_water_supply_801813DC(coord, (s16)(mem->period + 0x80), 0x100, col);
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

/// Per-frame driver of a sprite drifting toward the coordinate passed as the
/// spawn argument. The first frame takes the displacement from the task's
/// coordinate to that target, brings it into the coordinate's parent frame
/// and scales it to 0xCC/0x1000 of its length; each later frame adds that step
/// to the coordinate and, on every other tick, draws the sprite through
/// `func_shelter_b4_water_supply_80181158` at the next animation frame. The
/// work block is released at tick 20, or once the room's event state reaches
/// 4; from event state 1 on the sprite is neither moved nor drawn.
void func_shelter_b4_water_supply_80180F34(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    VECTOR         delta;

    work   = task->spawnArg2;
    coord  = task->extra.tmd->coords;
    target = (GsCOORDINATE2*)task->spawnArg1;
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
                    func_shelter_b4_water_supply_80181158(coord, ++work->index, 0x200, 0x80);
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

/// Draws a sprite at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, one
/// semi-transparent `POLY_FT4` (tpage 0x2A, clut 0x42CB) is queued as an
/// axis-aligned square of half-side `(s16)arg2 * 23 / (otz + 1)`, shaded grey
/// at `arg3`. `arg1` picks one of four 24-texel frames at u
/// `(arg1 & 3) * 24 + 0x60`, v 0 to 0x17.
void func_shelter_b4_water_supply_80181158(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
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

    tex = arg1;
    CLOBBER_REG(a1);
    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = *(u16*)&arg0->workm.t[1];
    vz                                      = *(u16*)&arg0->workm.t[2];
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
        xy          = *(u16*)&block->sx - *(u16*)&block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = *(u16*)&block->sx + *(u16*)&block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = *(u16*)&block->sy - *(u16*)&block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = *(u16*)&block->sy + *(u16*)&block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

/// Draws a ring around the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, sixteen gouraud
/// `POLY_G4` segments are queued between on-screen radii `(s16)arg1 * 64 /
/// (otz + 1)` and `(s16)(arg1 + arg2) * 64 / (otz + 1)`, black at the first
/// and coloured `rgb` at the second.
void func_shelter_b4_water_supply_801813DC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    GpArcScratch*   block;
    POLY_G4*        prim;
    s32             ang;
    register void** scratch asm("a1");
    register s32    saved asm("t1");
    register u8*    head asm("t0");
    register s32    sum asm("a1");
    register s32    otz asm("v0");
    register s32    rOuter asm("a0");
    register s32    rInner asm("v1");
    register u8*    color asm("s4");
    s32             t;
    u16             vz;
    u32             maskLo;
    u32             maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                     = *(u16*)&arg0->workm.t[0];
        ((GpArcScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (GpArcScratch*)tmp;
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
    gte_stsxy(&((GpArcScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpArcScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpArcScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        otz        = block->otz + 1;
        rOuter     = ((s16)saved * 64) / otz;
        rInner     = (s16)sum * 64;
        block->otz = otz;
        SOFT_BARRIER();
        rInner       = rInner / otz;
        ang          = 0;
        block->inner = rOuter;
        block->outer = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = *(u16*)&block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->inner * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->inner * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->inner * rcos(t)) >> 12);
            prim->x2 = *(u16*)&block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = *(u16*)&block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = *(u16*)&block->sx + ((block->outer * rsin(t)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->outer * rcos(t)) >> 12);
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

/// Draws a round glow at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, eight gouraud
/// `POLY_G4` wedges of on-screen radius `(s16)arg1 * 64 / (otz + 1)` are
/// queued around the projected point, coloured `rgb` at the centre and black
/// at the rim.
void func_shelter_b4_water_supply_80181800(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
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
        vx                                      = *(u16*)&arg0->workm.t[0];
        ((GpRingScratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (GpRingScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
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
    SCRATCH_POP_BYTES(0x18);
}

/// Per-frame driver of a flare: every tick it draws an orange round glow
/// through `func_shelter_b4_water_supply_80181800` and the textured glow of
/// `func_shelter_b4_water_supply_80181D40`, both growing by 0x10 a tick. While
/// the ring's brightness stays above 0x18 an expanding orange ring is drawn
/// around them, fading by 0x18 a tick; after that the glow's own brightness
/// fades by 0x18 a tick and the work block is released once it drops under
/// 0x18. Nothing runs while the room's event state is set, and the block is
/// released once that state reaches 4.
void func_shelter_b4_water_supply_80181B94(Task* arg0)
{
    u8             rgb[3];
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            step;

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
        func_shelter_b4_water_supply_80181800(coord, (s16)(step * 2), rgb);
        func_shelter_b4_water_supply_80181D40(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_shelter_b4_water_supply_801813DC(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
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
/// inner one of half-extent `size` and an outer one of `size * 3 / 2`
/// (each scaled by 0x37 / otz), plus the flat quad of
/// `func_shelter_b4_water_supply_8018226C` on the ground beneath it. It also
/// points the `Gp_RoomCoords[2]` light at the coordinate with a randomly flickering
/// intensity. Nothing is drawn when the GTE flags the projection.
void func_shelter_b4_water_supply_80181D40(GsCOORDINATE2* coord, s16 size)
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
    block                       = SCRATCH_HEAD_AT(scratch, GpRingScratch) - 1;
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
            func_shelter_b4_water_supply_8018226C(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Draws a flat textured quad at `arg0`: the four corners of the unit quad
/// `D_80111E38`, scaled by `arg1`, are rotated by the view matrix and offset
/// by the coordinate's world translation, then projected through `GsWSMATRIX`.
/// If the projection is valid, one semi-transparent `POLY_FT4` (tpage 0x28,
/// clut 0x428C) shaded (0x30, 0x20, 0x20) is queued; the display frame counter
/// flips it between two 32-texel frames at u 0xC0 and 0xE0, v 0x38 to 0x57.
/// The work block lives on the scratchpad stack.
void func_shelter_b4_water_supply_8018226C(GsCOORDINATE2* arg0, s32 arg1)
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
    SCRATCH_POP_BYTES(0x38);
}
