#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

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

/// One water surface: a rectangle at (`x`, `z`) spanning `width` along X and
/// `depth` along Z. A list of them ends at an entry whose `end` is -1; `end`
/// is not otherwise read.
typedef struct ShelterB2SepticTankSurface {
    s16 x;
    s16 z;
    s16 width;
    s16 depth;
    s32 end;
} ShelterB2SepticTankSurface;

/// Per-surface values the water drawer keeps in a block taken from the
/// scratchpad stack at `0x1F8003FC` rather than in registers. `dx` and `dz`
/// are the spacing between vertices along X and Z, and `wave` the height the
/// sine wave adds to the vertex being placed.
typedef struct ShelterB2SepticTankWaterWork {
    s16 y;
    s16 dx;
    s16 wave;
    s16 dz;
    s16 x;
    s16 z;
} ShelterB2SepticTankWaterWork;

extern u8  D_80071075;
extern s16 D_80071076;
extern s32 D_8007107C;
extern s8  D_8007217B;
extern s8  D_80114C12;
extern s32 D_8011572C;
extern s32 D_80115738;
extern s32 D_8011574C;
extern s32 D_80115750;
extern s32 D_80115758;
extern u8  D_80115690;

/// The room's message table, installed by its first task state.
extern GpMsgEntry D_shelter_b2_septic_tank_80182F4C[];
extern s16        D_shelter_b2_septic_tank_80182FFE;
extern u8         D_shelter_b2_septic_tank_80183004;
extern u8         D_shelter_b2_septic_tank_8018310C;

/// Height of the water surfaces.
extern s16 D_shelter_b2_septic_tank_801832BC;

/// Tasks the room's first task state spawns.
extern TaskDesc D_shelter_b2_septic_tank_801832C0[];

/// The room's water surfaces, as two lists drawn by separate functions.
extern ShelterB2SepticTankSurface D_shelter_b2_septic_tank_801832CC[];
extern ShelterB2SepticTankSurface D_shelter_b2_septic_tank_801832F0[];

extern SVECTOR D_shelter_b2_septic_tank_80183314[];
extern SVECTOR D_shelter_b2_septic_tank_80183344[];
extern SVECTOR D_shelter_b2_septic_tank_80183374[];
extern SVECTOR D_shelter_b2_septic_tank_80183514[];
extern SVECTOR D_shelter_b2_septic_tank_80183524[];
extern SVECTOR D_shelter_b2_septic_tank_80183534[];

/// The two points, relative to the effect's parent coordinate, that the trail
/// effect's two edges follow. The second is also read by its own name.
extern SVECTOR D_shelter_b2_septic_tank_8018355C[];
extern SVECTOR D_shelter_b2_septic_tank_80183564;

extern u8 D_shelter_b2_septic_tank_80187045;

/// Cursor into the primitive area the water surface is written to.
extern u8* D_shelter_b2_septic_tank_80187054;

void func_shelter_b2_septic_tank_8017DB68(Task* task);
void func_shelter_b2_septic_tank_8017E2DC(Task* task);
void func_shelter_b2_septic_tank_8017EAB8(Task* arg0);
void func_shelter_b2_septic_tank_8017EAF8(Task* task);
void func_shelter_b2_septic_tank_8017F194(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_shelter_b2_septic_tank_8017F984(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b2_septic_tank_8017FD70(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);
void func_shelter_b2_septic_tank_80180054(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b2_septic_tank_8018083C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b2_septic_tank_80180E84(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b2_septic_tank_801812B0(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_b2_septic_tank_80181B34(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);
void func_shelter_b2_septic_tank_801821B4(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

extern s32              func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern TaskDesc         D_shelter_b2_septic_tank_80182F40;
extern GpStateBD8       D_shelter_b2_septic_tank_80187034;
extern RoomEventMsg     D_shelter_b2_septic_tank_8018703C;
extern u8               D_shelter_b2_septic_tank_80187044;
extern RoomLatchedEvent D_shelter_b2_septic_tank_80187048;

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _shelterB2SepticTankStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b2_septic_tank_80187044 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b2_septic_tank_8018703C = *dst;
            D_shelter_b2_septic_tank_80187048 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b2_septic_tank_80182F40, 0, 0, 0);
            D_shelter_b2_septic_tank_80187044 = 1;
        }
        return 2;
    }
    return 1;
}

/// The room's event task, spawned by `_shelterB2SepticTankStartEvent` for the
/// event it latched in `D_shelter_b2_septic_tank_80187048`. State 0 runs the
/// event's CAP command; state 1 waits for it to finish and, when the event
/// asks for it, starts helper task 0x31; states 2 and 3 play the event's stage
/// sound, if any, and wait for it to end. State 4 copies the latched message's
/// destination into the save data and spawns the room-load task 0x11.
void func_shelter_b2_septic_tank_8017D614(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b2_septic_tank_80187048.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b2_septic_tank_80187048.fade != 0) {
                    D_shelter_b2_septic_tank_80187034.field_0 = 0;
                    D_shelter_b2_septic_tank_80187034.field_1 = 0;
                    D_shelter_b2_septic_tank_80187034.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b2_septic_tank_80187034);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b2_septic_tank_80187048.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b2_septic_tank_80187048.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b2_septic_tank_80187048.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b2_septic_tank_8018703C.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b2_septic_tank_8018703C.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_b2_septic_tank_8018703C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b2_septic_tank_8017D7AC(void)
{
    return 0;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Message 0x21 starts the room's event on flag 0x131; any
/// other message answers 1.
s32 func_shelter_b2_septic_tank_8017D7B4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId != 0x21) {
        return 1;
    }
    event.capCmd   = 3;
    event.stageSnd = 0x54220001;
    event.flagId   = 0x131;
    event.fade     = 0;
    return _shelterB2SepticTankStartEvent(out, &event);
}

s32 func_shelter_b2_septic_tank_8017D904(void)
{
    return 0;
}

s32 func_shelter_b2_septic_tank_8017D90C(s32 arg0, s32 arg1, RoomEventMsg* arg2)
{
    u8  kind;
    s32 flag;

    kind = arg2->field_2;
    if (kind == 2) {
        flag = GameFlag_GetNibble(0xEB);
        if (flag == 1 && D_shelter_b2_septic_tank_80187045 == flag) {
            func_800E8614((s32)&D_shelter_b2_septic_tank_8018310C, 0);
            D_shelter_b2_septic_tank_80187045 = kind;
        }
    }
    return 0;
}

void func_shelter_b2_septic_tank_8017D97C(s32 arg0)
{
    GameFlag_SetNibble(0xEB, arg0);
}

void func_shelter_b2_septic_tank_8017D9A0(void)
{
    Task*          target;
    GsCOORDINATE2* player;
    GsCOORDINATE2* coords;

    target = (Task*)Gp_LookupSlot4(0);
    player = ((TmdObject*)gameGetPtrSlot(3)->extra)->coords;
    if (target != NULL) {
        coords = ((TmdObject*)target->extra)->coords;
        D_shelter_b2_septic_tank_80182FFE =
            (ratan2(coords->coord.t[0] - player->coord.t[0], coords->coord.t[2] - player->coord.t[2]) + 0x1000) & 0xFFF;
    }
}

void func_shelter_b2_septic_tank_8017DA18(Task* arg0)
{
    arg0->msgTable = D_shelter_b2_septic_tank_80182F4C;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_shelter_b2_septic_tank_801832C0, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_b2_septic_tank_8017DA74(Task* task)
{
    s32 place;

    if (gGameSession->at4.loc.view == 4) {
        place = gGameSession->at4.loc.place;
        if (place == 1 && D_80114C12 != place && D_80071075 == 0 && D_shelter_b2_septic_tank_80187045 == 0) {
            if (GameFlag_GetNibble(0xEB) == 0) {
                func_800E8614((s32)&D_shelter_b2_septic_tank_80183004, 0);
            }
            D_shelter_b2_septic_tank_80187045 = place;
        }
    }
}

/// The room task's state table, dispatched by
/// `func_shelter_b2_septic_tank_8017DB10` from a stack copy.
const TaskFuncTable3 D_shelter_b2_septic_tank_8017D5D8 = {
    {
        func_shelter_b2_septic_tank_8017DA18,
        func_shelter_b2_septic_tank_8017DA74,
        taskKill,
    },
};

/// The room task: copies the three-state table `D_shelter_b2_septic_tank_8017D5D8`
/// onto the stack and runs the entry for the task's current state.
void func_shelter_b2_septic_tank_8017DB10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_septic_tank_8017D5D8;
    sp.funcs[task->state](task);
}

/// Draws each surface in `D_shelter_b2_septic_tank_801832CC` exactly as
/// `func_shelter_b2_septic_tank_8017E2DC` draws its own list: two strips of 16
/// wave-lifted semi-transparent Gouraud quads per surface at height
/// `D_shelter_b2_septic_tank_801832BC`.
void func_shelter_b2_septic_tank_8017DB68(Task* task)
{
    SVECTOR                       v0, v1, v2, v3;
    s32                           sxy0, sxy1, sxy2, sxy3;
    s32                           p, flag;
    s32                           phase;
    ShelterB2SepticTankSurface*   e;
    ShelterB2SepticTankWaterWork* w;
    u8*                           head;
    POLY_G4*                      poly;
    DR_MODE*                      dr;
    s32                           otz;
    s32                           i;

    e                 = D_shelter_b2_septic_tank_801832CC;
    gGfxViewCoord.flg = 0;
    head              = *(u8**)0x1F8003FC;
    phase             = -(gDisplayState.animFrame * 16);
    *(u8**)0x1F8003FC = head - 0xC;
    w                 = (ShelterB2SepticTankWaterWork*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    w->y = D_shelter_b2_septic_tank_801832BC;
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
            w->wave = (u32)rsin(phase + (i << 9)) >> 5;
            v2.vx   = w->x + w->dx;
            v2.vy   = w->y + w->wave;
            v2.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 5;
            v3.vx   = w->x + w->dx;
            v3.vy   = w->y + w->wave;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                setRGB0(poly, 0, 0x40, 0x80);
                setRGB1(poly, 0, 0x40, 0x80);
                setRGB2(poly, 0, 0x10, 0x20);
                setRGB3(poly, 0, 0x10, 0x20);
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
        for (i = 0; i < 16; i++) {
            w->wave = (u32)rsin(phase + (i << 9)) >> 5;
            v0.vx   = w->x + w->dx;
            v0.vy   = w->y + w->wave;
            v0.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 5;
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
                poly                              = (POLY_G4*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                setRGB0(poly, 0, 0x40, 0x80);
                setRGB1(poly, 0, 0x40, 0x80);
                setRGB2(poly, 0, 0x10, 0x20);
                setRGB3(poly, 0, 0x10, 0x20);
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

/// Draws each surface in `D_shelter_b2_septic_tank_801832F0` at height
/// `D_shelter_b2_septic_tank_801832BC` as two strips of 16 semi-transparent
/// Gouraud quads laid side by side along X, each strip running along Z and
/// projected through the view matrix. The seam between the strips is lifted by
/// a sine wave that runs along Z and scrolls with the display frame counter.
/// In both strips the quad's lower-X edge is coloured (0, 0x40, 0x80) and its
/// upper-X edge (0, 0x10, 0x20); each quad is followed by a draw-mode packet
/// selecting blend mode 2. Quads the projection flags as invalid are skipped.
/// The per-surface values live in a work block pushed on the scratchpad stack
/// for the duration of the call.
void func_shelter_b2_septic_tank_8017E2DC(Task* task)
{
    SVECTOR                       v0, v1, v2, v3;
    s32                           sxy0, sxy1, sxy2, sxy3;
    s32                           p, flag;
    s32                           phase;
    ShelterB2SepticTankSurface*   e;
    ShelterB2SepticTankWaterWork* w;
    u8*                           head;
    POLY_G4*                      poly;
    DR_MODE*                      dr;
    s32                           otz;
    s32                           i;

    e                 = D_shelter_b2_septic_tank_801832F0;
    gGfxViewCoord.flg = 0;
    head              = *(u8**)0x1F8003FC;
    phase             = -(gDisplayState.animFrame * 16);
    *(u8**)0x1F8003FC = head - 0xC;
    w                 = (ShelterB2SepticTankWaterWork*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    w->y = D_shelter_b2_septic_tank_801832BC;
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
            w->wave = (u32)rsin(phase + (i << 9)) >> 5;
            v2.vx   = w->x + w->dx;
            v2.vy   = w->y + w->wave;
            v2.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 5;
            v3.vx   = w->x + w->dx;
            v3.vy   = w->y + w->wave;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                setRGB0(poly, 0, 0x40, 0x80);
                setRGB1(poly, 0, 0x40, 0x80);
                setRGB2(poly, 0, 0x10, 0x20);
                setRGB3(poly, 0, 0x10, 0x20);
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
        for (i = 0; i < 16; i++) {
            w->wave = (u32)rsin(phase + (i << 9)) >> 5;
            v0.vx   = w->x + w->dx;
            v0.vy   = w->y + w->wave;
            v0.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 5;
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
                poly                              = (POLY_G4*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                *(s32*)&poly->x0 = sxy0;
                *(s32*)&poly->x1 = sxy1;
                *(s32*)&poly->x2 = sxy2;
                *(s32*)&poly->x3 = sxy3;
                setRGB0(poly, 0, 0x40, 0x80);
                setRGB1(poly, 0, 0x40, 0x80);
                setRGB2(poly, 0, 0x10, 0x20);
                setRGB3(poly, 0, 0x10, 0x20);
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        dr);
            }
        }
    }
    *(u8**)0x1F8003FC += 0xC;
}

/// The water task: runs its current state - `func_shelter_b2_septic_tank_8017EAB8`
/// once, then `func_shelter_b2_septic_tank_8017EAF8`, which draws the surfaces -
/// and each tick publishes the room's water height to the session.
void func_shelter_b2_septic_tank_8017EA50(Task* task)
{
    TaskFunc states[2] = { func_shelter_b2_septic_tank_8017EAB8, func_shelter_b2_septic_tank_8017EAF8 };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b2_septic_tank_801832BC;
}

/// Clears the session's `field_80` or `field_7E`, chosen by `D_8007217B`, and
/// advances the task to its next state.
void func_shelter_b2_septic_tank_8017EAB8(Task* arg0)
{
    if (D_8007217B == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// The water task's drawing state: points the primitive cursor
/// `D_shelter_b2_septic_tank_80187054` at the current buffer's 0xC000-byte
/// slice of one of two primitive areas, chosen by `D_8007217B`, then draws both
/// lists of water surfaces.
void func_shelter_b2_septic_tank_8017EAF8(Task* task)
{
    if (D_8007217B == 0) {
        D_shelter_b2_septic_tank_80187054 = (u8*)D_8005C374 + D_8007107C * 0xC000;
    } else {
        D_shelter_b2_septic_tank_80187054 = (u8*)D_8005C370 + D_8007107C * 0xC000;
    }
    func_shelter_b2_septic_tank_8017DB68(task);
    func_shelter_b2_septic_tank_8017E2DC(task);
}

void func_shelter_b2_septic_tank_8017EB7C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_80115758  = 0x601D4;
            D_8011572C  = 0x601F0;
            D_80115750  = 0x6020C;
            D_8011574C  = 0x6016C;
            D_80115738  = 0x6016D;
            arg0->state = 1;
        case 1:
            switch (Gp_GetViewIndex() & 0xFF) {
                case 2: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183314;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[2], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[16], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[60], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[62], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_8018083C(&p[70], 0x300, 0x10);
                    func_shelter_b2_septic_tank_8018083C(&p[72], 0x300, 0x100);
                    break;
                }
                case 3: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183314;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[2], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[4], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[6], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[16], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[18], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[20], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[28], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[30], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[44], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[46], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[48], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[60], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[62], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_8018083C(&p[70], 0x300, 0x10);
                    func_shelter_b2_septic_tank_8018083C(&p[71], 0x300, 0x100);
                    func_shelter_b2_septic_tank_8018083C(&p[72], 0x300, 0x100);
                    break;
                }
                case 4: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183344;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[2], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[4], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[6], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[16], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[18], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[20], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[34], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[36], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[48], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[50], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[52], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183514, 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183524, 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183534, 0x200, 0x800, 0x100);
                    break;
                }
                case 5: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183374;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183514, 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183524, 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183534, 0x200, 0x800, 0x100);
                    break;
                }
                case 6: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183314;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[2], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[4], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[28], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[30], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[60], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[62], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_8018083C(&p[70], 0x300, 0x10);
                    func_shelter_b2_septic_tank_8018083C(&p[71], 0x300, 0x100);
                    func_shelter_b2_septic_tank_8018083C(&p[72], 0x300, 0x100);
                    break;
                }
            }
            break;
    }
}

/// Per-frame driver of an expanding, fading flash. While the room's event
/// state is 0 it updates the task's coordinate, ticks the age counter and
/// draws the flash through `func_shelter_b2_septic_tank_8017F194` at size
/// `field_26`, seeded from the spawn argument and grown by 0x20 a frame, and
/// brightness `field_24`, which starts at 0x40 and drops by 2 a frame; the
/// first frame also turns the coordinate about Y by a random angle. The work
/// block is released once the brightness falls under 2. Once the event state
/// is non-zero it only draws, releasing the block from event state 4 on.
void func_shelter_b2_septic_tank_8017F040(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_b2_septic_tank_8017F194(coord, (s16)work->field_26, (s16)work->field_24);
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
        func_shelter_b2_septic_tank_8017F194(coord, (s16)work->field_26, (s16)work->field_24);
        work->field_24 -= 2;
        if ((s16)work->field_24 < 2) {
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
void func_shelter_b2_septic_tank_8017F194(GsCOORDINATE2* arg0, s32 arg1, s32 arg2)
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

/// Per-frame driver of a particle, drawn as the spinning sprite of
/// `func_shelter_b2_septic_tank_8017F984` or, when the spawn argument's top
/// nibble is set, the upright sprite of `func_shelter_b2_septic_tank_8017FD70`.
/// The first frame takes the size from the argument's low 12 bits, a random
/// angle, and the ticks per animation frame from bits 12-15. Unless the work
/// block already carries a velocity it picks one by the kind in bits 24-27 -
/// none, a random upward burst, a random spray, a narrow upward jet, or the
/// work block's stored direction - scaled to the speed in bits 16-23 (0x40
/// when zero). Every later tick draws, moves the coordinate by the velocity
/// with gravity pulling it down, and releases the block after animation frame
/// 7. While the room's event state is non-zero it only draws, releasing the
/// block from event state 4 on.
void func_shelter_b2_septic_tank_8017F4C8(Task* task)
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
                func_shelter_b2_septic_tank_8017F984(coord, (s16)work->field_20, (s16)work->field_24, (s16)work->field_26);
            } else {
                func_shelter_b2_septic_tank_8017FD70(coord, (s16)work->field_20, (s16)work->field_24);
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
            func_shelter_b2_septic_tank_8017F984(coord, (s16)work->field_20, (s16)work->field_24, (s16)work->field_26);
            break;
        case 2:
            func_shelter_b2_septic_tank_8017FD70(coord, (s16)work->field_20, (s16)work->field_24);
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

/// Draws a spinning sprite at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, one
/// semi-transparent, unshaded `POLY_FT4` (tpage 0x2B, clut 0x43D3) is queued
/// as a square rotated by angle `arg3` about the projected point, with
/// on-screen half-diagonal `(s16)arg2 * 31 / otz`. `arg1` picks the 32-texel
/// frame at u = `arg1 * 32`, v 0xE0 to 0xFF.
void func_shelter_b2_septic_tank_8017F984(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
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

/// Draws an upright sprite at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, one
/// semi-transparent, unshaded `POLY_FT4` (tpage 0x2B, clut 0x43D2) is queued
/// as an axis-aligned square of half-side `r = arg2 * 55 / otz`, raised so the
/// projected point sits three quarters of the way down it. `arg1` picks one of
/// eight 56-texel frames in a grid four wide, starting at v 0x70.
void func_shelter_b2_septic_tank_8017FD70(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
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

/// Draws a flickering light beam from `arg0[0]` to `arg0[1]`. Both points are
/// projected through the view matrix; unless the far end is nearer than OTZ
/// 0x11, gouraud `POLY_G4` wedges around each end (radius `(s16)arg1 * 64 /
/// otz` at that end) are joined by quads between the two, each fading from the
/// beam colour on the axis to black at the rim. `arg2` turns the wedges about
/// the axis. `arg3` packs the colour: the red factor in bits 8-15 and the
/// green and blue factors in bits 4 and 0, each multiplying an intensity that
/// alternates between 0x20 and 0x28 with the display frame counter.
void func_shelter_b2_septic_tank_80180054(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
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
    s32                packed;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

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
        packed    = arg3 << 16;
        blend     = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        r         = blend * (packed >> 24);
        g         = blend * ((packed >> 20) & 1);
        base      = (s16)arg2;
        b         = blend * (arg3 & 1);
        ang       = 0;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = r;
            p->g2    = g;
            prim->b2 = b;
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
            setRGB2(prim, r, g, b);
            setRGB3(prim, r, g, b);
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
            setRGB2(prim, r, g, b);
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Draws a flickering glow at the world-space point `arg0`. The point is
/// projected through the view matrix; unless it is nearer than OTZ 0x11, four
/// gouraud `POLY_G4` wedges of on-screen radius `(s16)arg1 * 64 / otz` are
/// queued around it, coloured at the centre and black at the rim. `arg2` packs
/// the colour: the red factor in bits 8-15 and the green and blue factors in
/// bits 4 and 0, each multiplying an intensity that alternates between 0x20
/// and 0x28 with the display frame counter.
void func_shelter_b2_septic_tank_8018083C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    u8*                ds_ptr;
    DisplayState*      ds;
    s32                radius;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0xC);
        SOFT_TOUCH_REG(tmp);
        block = (RoomDraw25Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw25Scratch*)(head - 0xC))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - 0xC))->otz >= 0x11) {
        radius        = ((s16)arg1 * 64) / ((RoomDraw25Scratch*)(head - 0xC))->otz;
        ds_ptr        = (u8*)&gDisplayState;
        packed        = arg2 << 16;
        blend         = ((*(u8*)&((DisplayState*)ds_ptr)->animFrame & 1) * 8) | 0x20;
        r             = blend * (packed >> 24);
        g             = blend * ((packed >> 20) & 1);
        b             = blend * (arg2 & 1);
        ang           = 0;
        ds            = (DisplayState*)ds_ptr;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0xC;
}

/// Per-frame driver of a burst of light in red, half blue and quarter green.
/// Over the number of frames given by the spawn argument the burst's
/// brightness `scale` and size `angle` grow together, drawn as a glow at that
/// size, a dimmer glow at twice it and a ring closing in around them. At the
/// peak the screen is flashed in the burst's colour, and the burst then fades
/// out through a larger billboard glow, shrinking by 8 and dimming by 0x10 a
/// frame until its brightness drops to 0x10. The work block is then released,
/// as it is once the room's event state reaches 4; from event state 1 on the
/// burst is no longer advanced or drawn.
void func_shelter_b2_septic_tank_80180BE0(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
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
                rgb[1] = (u16)work->scale >> 2;
                rgb[2] = (u16)work->scale >> 1;
                func_shelter_b2_septic_tank_801812B0(coord, (s16)work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_b2_septic_tank_801812B0(coord, (s16)((u16)work->angle * 2), rgb);
                func_shelter_b2_septic_tank_80180E84(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = (u16)work->scale >> 2;
                    rgb[2]      = (u16)work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if ((s16)work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = (u16)work->scale >> 1;
                    func_shelter_b2_septic_tank_801821B4(coord, (s16)((s16)work->angle * 3), rgb);
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

/// Draws a ring around the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, sixteen gouraud
/// `POLY_G4` segments are queued between on-screen radii `(s16)arg1 * 64 /
/// (otz + 1)` and `(s16)(arg1 + arg2) * 64 / (otz + 1)`, black at the first
/// and coloured `rgb` at the second.
void func_shelter_b2_septic_tank_80180E84(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
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

/// Draws a round glow at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, eight gouraud
/// `POLY_G4` wedges of on-screen radius `(s16)arg1 * 64 / (otz + 1)` are
/// queued around the projected point, coloured `rgb` at the centre and black
/// at the rim.
void func_shelter_b2_septic_tank_801812B0(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
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
            prim->x0 = *(u16*)&block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Per-frame driver of a ribbon trail swept by two points of a moving parent.
/// The first frame allocates two eight-slot histories of coordinates, places
/// the task's own coordinate at the first point under the parent and fills
/// every slot with the two points' current positions. Each later frame records
/// the two positions in the slot the age counter selects and draws the ribbon
/// through `func_shelter_b2_septic_tank_80181B34`. The work block is
/// released once the age reaches the spawn argument; from the room's event
/// state 2 on the effect is frozen.
void func_shelter_b2_septic_tank_80181644(Task* task)
{
    GsCOORDINATE2  coord;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* objCoord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    SVECTOR*       vec;
    s32            i;

    coords   = (GsCOORDINATE2*)task->work;
    work     = (GpEffWork*)task->spawnArg2;
    objCoord = ((TmdObject*)task->extra)->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GsCOORDINATE2*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_shelter_b2_septic_tank_8018355C[0].vx;
                objCoord->coord.t[1] = D_shelter_b2_septic_tank_8018355C[0].vy;
                objCoord->coord.t[2] = D_shelter_b2_septic_tank_8018355C[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_shelter_b2_septic_tank_8018355C[1];
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
                coord.coord.t[0] = D_shelter_b2_septic_tank_80183564.vx;
                coord.coord.t[1] = D_shelter_b2_septic_tank_80183564.vy;
                coord.coord.t[2] = D_shelter_b2_septic_tank_80183564.vz;
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
                func_shelter_b2_septic_tank_80181B34(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the ribbon between two eight-slot coordinate histories `arg0` and
/// `arg1` as seven gouraud `POLY_G4` quads, walking back from the newest slot
/// `arg2`; each quad joins the positions of two consecutive slots in both
/// histories and is skipped when its projection is invalid. The ribbon fades
/// from intensity 0x40 at the newest slot by 9 per slot. `arg3` packs the
/// colour: the red factor in bits 8 up and the green and blue factors in bits
/// 4-5 and 0-1, each multiplying that intensity.
void func_shelter_b2_septic_tank_80181B34(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GsCOORDINATE2*     a;
    GsCOORDINATE2*     b;
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

        tmp                     = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomDraw03Scratch);
        blk                     = (RoomDraw03Scratch*)tmp;
        *(void**)G_SCRATCH_HEAD = tmp;
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomDraw03Scratch);
}

/// Per-frame driver of an explosion at the task's coordinate. The first frame
/// spawns effect 0x60076 and then either, with a non-zero spawn argument,
/// effect 0x60070 and a spray of further 0x60070 sparks thrown at random
/// velocities over the next frames, or two 0x6007C effects and two expanding
/// rings drawn through `func_shelter_b2_septic_tank_80180E84` in a fading
/// orange. The work block is released once the age reaches 7, or when the
/// room's event state reaches 4; from event state 1 on nothing is advanced or
/// drawn.
void func_shelter_b2_septic_tank_80181F2C(Task* task)
{
    GsCOORDINATE2* objCoord;
    GpEffWork*     work;
    u8             rgb[4];

    objCoord = ((TmdObject*)task->extra)->coords;
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
            rgb[1]       = (u16)work->angle >> 1;
            rgb[2]       = (u16)work->angle >> 2;
            func_shelter_b2_septic_tank_80180E84(objCoord, 0x100, 0x100, rgb);
            func_shelter_b2_septic_tank_80180E84(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Draws a star-shaped flare at the coordinate's world position. The position
/// is projected through `GsWSMATRIX`; if the projection is valid, gouraud
/// `POLY_G4` wedges are queued around the projected point: a glow of on-screen
/// radius `r = arg1 * 64 / (otz + 1)` at half the colour `arg2`, a glow of
/// radius `r / 2` at the full colour, and four spikes at half the colour,
/// reaching alternately to `r` and `2 * r`. Every wedge fades from its colour
/// at the centre to black.
void func_shelter_b2_septic_tank_801821B4(GsCOORDINATE2* arg0, s16 arg1, u8* arg2)
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
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 12);
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
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 13);
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
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
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
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
