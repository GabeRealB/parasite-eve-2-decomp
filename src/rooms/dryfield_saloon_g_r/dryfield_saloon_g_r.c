#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

/// Scratch block `func_dryfield_saloon_g_r_8017DEC4` takes from
/// `G_SCRATCH_HEAD` for one light shaft. The four vectors are the shaft's
/// corners in world space: the two roots, then the tip reached from each.
typedef struct {
    s32     otz;
    SVECTOR rootA;
    SVECTOR rootB;
    SVECTOR tipA;
    SVECTOR tipB;
} _DryfieldSaloonGRShaftScratch;

extern s16 D_80071076;

/// The event message and request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventMsg D_dryfield_saloon_g_r_80181BDC;
extern RoomEventReq D_dryfield_saloon_g_r_80181BE8;
extern u8           D_dryfield_saloon_g_r_80181BE4;

/// Descriptor of the event task `func_dryfield_saloon_g_r_8017D74C`.
extern TaskDesc D_dryfield_saloon_g_r_8017ECB0;

/// The room's message table, installed on the room task by its entry state.
extern GpMsgEntry D_dryfield_saloon_g_r_8017ECBC[];

/// The room's effect positions in the model's local space. The frame hook
/// draws a sprite at each of 0-10; 12 and 13 are the two ends of the beam,
/// and 14-19 the roots and tip directions of the two light shafts.
extern SVECTOR D_dryfield_saloon_g_r_8017ECE4[];

/// Entry 13 of `D_dryfield_saloon_g_r_8017ECE4`, reached under a label of its
/// own.
extern SVECTOR D_dryfield_saloon_g_r_8017ED4C[];

/// Entries 14 and 17 of `D_dryfield_saloon_g_r_8017ECE4`, the two shaft roots,
/// which the code also reaches under labels of their own.
extern SVECTOR D_dryfield_saloon_g_r_8017ED54;
extern SVECTOR D_dryfield_saloon_g_r_8017ED6C;

/// One view bitmask per effect, tested against `1 << view`: entries 0-10 gate
/// the sprites, 11 the beam and 12 the light shafts.
extern s16 D_dryfield_saloon_g_r_8017ED84[];

void func_dryfield_saloon_g_r_8017D9CC(Task* task);
void func_dryfield_saloon_g_r_8017DA10(Task* task);
void func_dryfield_saloon_g_r_8017DBB4(GsCOORDINATE2* arg0, SVECTOR* arg1, s32 arg2, s32 arg3);
void func_dryfield_saloon_g_r_8017DEC4(GsCOORDINATE2* coord);
void func_dryfield_saloon_g_r_8017E430(GsCOORDINATE2* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3);

/// Event gate for the room's exit. Returns 1 when game-flag nibble
/// `req->flagId` already reads set (clear, for a negative id). Otherwise, when
/// `req->itemId` has been collected or is 0, it returns 2 and - unless
/// `msg->field_5` asks for a dry run - latches `msg` and `req`, sets the
/// nibble and spawns the event task. When the item is missing it returns 0
/// and, outside a dry run, runs cap command `req->field_4`.
s32 func_dryfield_saloon_g_r_8017D5E8(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                           = req->flagId;
    D_dryfield_saloon_g_r_80181BE4 = 0;
    neg                            = flag < 0;
    got                            = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->field_5 == 0) {
                D_dryfield_saloon_g_r_80181BDC = *msg;
                D_dryfield_saloon_g_r_80181BE8 = *req;
                id                             = req->flagId;
                mode                           = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_saloon_g_r_8017ECB0, 0, 0, 0);
                D_dryfield_saloon_g_r_80181BE4 = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// The event task the gate spawns: runs the latched request's cap command,
/// plays its two sound ids in turn, each waited out, then warps to the area,
/// warp point and room the latched message names.
void func_dryfield_saloon_g_r_8017D74C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_saloon_g_r_80181BE8.field_0);
            if (D_dryfield_saloon_g_r_80181BE8.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_saloon_g_r_80181BE8.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_saloon_g_r_80181BE8.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_saloon_g_r_80181BE8.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_saloon_g_r_80181BE8.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_saloon_g_r_80181BE8.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_saloon_g_r_80181BDC.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_saloon_g_r_80181BDC.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_dryfield_saloon_g_r_80181BDC.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_saloon_g_r_8017DA18`: the entry tick
/// `func_dryfield_saloon_g_r_8017D9CC`, the idle state
/// `func_dryfield_saloon_g_r_8017DA10`, then `taskKill`.
const TaskFuncTable3 D_dryfield_saloon_g_r_8017D5DC = {
    { func_dryfield_saloon_g_r_8017D9CC, func_dryfield_saloon_g_r_8017DA10, taskKill },
};

/// Handler for message 0x13EE in the room's message table, which filters a
/// warp request: copies `in` to `out`, and for area 0xF picks the destination
/// room from game-flag nibble 0x61 (unless `in->field_5` asks for a dry run),
/// then passes the warp through the event gate with the room's own request -
/// nibble 0x35, no item, cap command 2 and two stage sound ids. Any other area
/// answers 1.
s32 func_dryfield_saloon_g_r_8017D8BC(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u16          msgId;

    *out  = *in;
    msgId = in->msgId;
    if (msgId == 0xF) {
        if (in->field_5 == 0) {
            out->field_3 = GameFlag_GetNibble(0x61) + 1;
        }
        if (in->msgId == msgId) {
            req.field_0 = 2;
            req.field_4 = 2;
            req.field_8 = Gp_PackStageSndId(0x52120005);
            req.field_C = Gp_PackStageSndId(0x52120003);
            req.flagId  = 0x35;
            req.itemId  = 0;
            return func_dryfield_saloon_g_r_8017D5E8(&req, in);
        }
    }
    return 1;
}

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_saloon_g_r_8017D994(void)
{
    return 0;
}

/// Handler for message 0x13F0 in the room's message table: on action 4 it
/// runs cap command 4. Always returns 0.
s32 func_dryfield_saloon_g_r_8017D99C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 4) {
        Gp_RunCapCmd1(4);
    }
    return 0;
}

/// Handler for message 0x13EF in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_saloon_g_r_8017D9C4(void)
{
    return 0;
}

/// Entry state of the room task: installs the room's message table, registers
/// the task in pointer slot 7 and advances to the idle state.
void func_dryfield_saloon_g_r_8017D9CC(Task* task)
{
    task->msgTable = D_dryfield_saloon_g_r_8017ECBC;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Idle state of the room task.
void func_dryfield_saloon_g_r_8017DA10(Task* task)
{
}

/// The room task: runs the state the task is in from a stack copy of the
/// room's three-state table.
void func_dryfield_saloon_g_r_8017DA18(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_saloon_g_r_8017D5DC;
    sp.funcs[task->state](task);
}

/// Draws the room's light effects under the coordinate of the model in
/// `arg0->extra`, each only when its mask in `D_dryfield_saloon_g_r_8017ED84`
/// includes the current view `gGameSession->at4.loc.view`: sprites at
/// positions 0-5 with frame 0 and 6-10 with frame 2, the two light shafts,
/// and the beam from position 13 to position 12.
void func_dryfield_saloon_g_r_8017DA70(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            mask;
    s32            i;

    coord = ((TmdObject*)arg0->extra)->coords;
    mask  = 1 << gGameSession->at4.loc.view;
    for (i = 0; i < 6; i++) {
        if (mask & D_dryfield_saloon_g_r_8017ED84[i]) {
            func_dryfield_saloon_g_r_8017DBB4(coord, &D_dryfield_saloon_g_r_8017ECE4[i], 0, 0x200);
        }
    }
    for (i = 6; i < 11; i++) {
        if (mask & D_dryfield_saloon_g_r_8017ED84[i]) {
            func_dryfield_saloon_g_r_8017DBB4(coord, &D_dryfield_saloon_g_r_8017ECE4[i], 2, 0x200);
        }
    }
    if (mask & D_dryfield_saloon_g_r_8017ED84[12]) {
        func_dryfield_saloon_g_r_8017DEC4(coord);
    }
    if (mask & D_dryfield_saloon_g_r_8017ED84[11]) {
        func_dryfield_saloon_g_r_8017E430(coord, D_dryfield_saloon_g_r_8017ED4C, D_dryfield_saloon_g_r_8017ED4C - 1, 0x100);
    }
}

/// Queues one flickering, screen-aligned textured sprite at `arg1` in
/// `arg0`'s local space. The point is rotated by the coordinate's `workm`,
/// offset by its translation and projected through `GsWSMATRIX`, using a
/// 0x14-byte block taken from `G_SCRATCH_HEAD`; nothing is drawn when the
/// projected `otz` is below 0x11.
///
/// The primitive is a semi-transparent `POLY_FT4` on tpage 0x2B. `arg2` picks
/// one of the 40-texel-wide frames along the top row of the texture page (u
/// `arg2 * 40 .. arg2 * 40 + 39`, v 0..0x27) and the clut
/// `(arg2 & 0x3F) | 0x4380`. `arg3` is a half-extent: the square reaches
/// `(s16)arg3 * 39 / otz` from the projected centre in each direction. The
/// flat colour is 0x20 or 0x30 on the parity of `gDisplayState.animFrame`.
void func_dryfield_saloon_g_r_8017DBB4(GsCOORDINATE2* arg0, SVECTOR* arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw35Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                su;
    s32                sv;
    s32                u0;
    s32                u1;
    s32                flip;
    s32                rgb;
    s16                xy;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x14;
    block    = (RoomDraw35Scratch*)(head - 0x14);

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&((RoomDraw35Scratch*)(head - 0x14))->vec);
    block->vec.vx = *(u16*)&block->vec.vx + *(u16*)&arg0->workm.t[0];
    block->vec.vy = *(u16*)&block->vec.vy + *(u16*)&arg0->workm.t[1];
    block->vec.vz = *(u16*)&block->vec.vz + *(u16*)&arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw35Scratch*)(head - 0x14))->vec);
    gte_rtps();

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomDraw35Scratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw35Scratch*)(head - 0x14))->otz >= 0x11) {
        ds          = &gDisplayState;
        flip        = (u8)ds->animFrame;
        su          = (s16)arg2;
        sv          = (s16)arg3;
        prim->tpage = 0x2B;
        prim->clut  = (su & 0x3F) | 0x4380;
        u0          = su * 0x28;
        u1          = u0 + 0x27;
        prim->u1    = u1;
        prim->u3    = u1;
        prim->u0    = u0;
        prim->u2    = u0;
        prim->v0    = 0;
        prim->v1    = 0;
        prim->v2    = 0x27;
        prim->v3    = 0x27;
        rgb         = (flip & 1) << 4;
        rgb        += 0x20;
        setSemiTrans(prim, 1);
        prim->r0         = rgb;
        prim->g0         = rgb;
        prim->b0         = rgb;
        block->halfWidth = (sv * 0x27) / block->otz;
        xy               = *(u16*)&block->sx - *(u16*)&block->halfWidth;
        prim->x2         = xy;
        prim->x0         = xy;
        xy               = *(u16*)&block->sx + *(u16*)&block->halfWidth;
        prim->x3         = xy;
        prim->x1         = xy;
        xy               = *(u16*)&block->sy - *(u16*)&block->halfWidth;
        prim->y1         = xy;
        prim->y0         = xy;
        xy               = *(u16*)&block->sy + *(u16*)&block->halfWidth;
        prim->y3         = xy;
        prim->y2         = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}

/// Draws the room's two light shafts as Gouraud quads. Both shafts share the
/// roots at positions 14 and 17 of `D_dryfield_saloon_g_r_8017ECE4`; each
/// root's tip lies at four times its offset to a later entry (15 and 18 for
/// the first shaft, 16 and 19 for the second). All four corners are moved to
/// world space through `coord->workm` and projected through `GsWSMATRIX`. The
/// roots take a grey of 0x20 or 0x30 on the parity of
/// `gDisplayState.animFrame` and the tips are black, so the shaft fades
/// outward. The quad is sorted by `tipB`'s `otz` and skipped when that is
/// below 0x11.
void func_dryfield_saloon_g_r_8017DEC4(GsCOORDINATE2* coord)
{
    u8*                            head;
    _DryfieldSaloonGRShaftScratch* block;
    POLY_G4*                       prim;
    SVECTOR*                       dirA;
    SVECTOR*                       dirB;
    s32                            i;
    s32                            j;
    s32                            rgb;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x24;
        *scratch = tmp;
        block    = (_DryfieldSaloonGRShaftScratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_saloon_g_r_8017ED54);
    gte_rtv0();
    gte_stsv(&((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->rootA);
    *(u16*)&block->rootA.vx = *(u16*)&block->rootA.vx + *(u16*)&coord->workm.t[0];
    *(u16*)&block->rootA.vy = *(u16*)&block->rootA.vy + *(u16*)&coord->workm.t[1];
    *(u16*)&block->rootA.vz = *(u16*)&block->rootA.vz + *(u16*)&coord->workm.t[2];

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_saloon_g_r_8017ED6C);
    gte_rtv0();
    gte_stsv(&((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->rootB);
    *(u16*)&block->rootB.vx = *(u16*)&block->rootB.vx + *(u16*)&coord->workm.t[0];
    *(u16*)&block->rootB.vy = *(u16*)&block->rootB.vy + *(u16*)&coord->workm.t[1];
    *(u16*)&block->rootB.vz = *(u16*)&block->rootB.vz + *(u16*)&coord->workm.t[2];

    for (i = 0; i < 2; i++) {
        j                      = i + 15;
        dirA                   = &D_dryfield_saloon_g_r_8017ECE4[j];
        *(u16*)&block->tipA.vx = *(u16*)&D_dryfield_saloon_g_r_8017ECE4[14].vx +
                                 (*(u16*)&dirA->vx - *(u16*)&D_dryfield_saloon_g_r_8017ECE4[14].vx) * 4;
        *(u16*)&block->tipA.vy = *(u16*)&D_dryfield_saloon_g_r_8017ECE4[14].vy +
                                 (*(u16*)&dirA->vy - *(u16*)&D_dryfield_saloon_g_r_8017ECE4[14].vy) * 4;
        *(u16*)&block->tipA.vz = *(u16*)&D_dryfield_saloon_g_r_8017ECE4[14].vz +
                                 (*(u16*)&dirA->vz - *(u16*)&D_dryfield_saloon_g_r_8017ECE4[14].vz) * 4;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->tipA);
        gte_rtv0();
        gte_stsv(&((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->tipA);
        *(u16*)&block->tipA.vx = *(u16*)&block->tipA.vx + *(u16*)&coord->workm.t[0];
        *(u16*)&block->tipA.vy = *(u16*)&block->tipA.vy + *(u16*)&coord->workm.t[1];
        *(u16*)&block->tipA.vz = *(u16*)&block->tipA.vz + *(u16*)&coord->workm.t[2];

        j                      = i + 18;
        dirB                   = &D_dryfield_saloon_g_r_8017ECE4[j];
        *(u16*)&block->tipB.vx = *(u16*)&D_dryfield_saloon_g_r_8017ECE4[17].vx +
                                 (*(u16*)&dirB->vx - *(u16*)&D_dryfield_saloon_g_r_8017ECE4[17].vx) * 4;
        *(u16*)&block->tipB.vy = *(u16*)&D_dryfield_saloon_g_r_8017ECE4[17].vy +
                                 (*(u16*)&dirB->vy - *(u16*)&D_dryfield_saloon_g_r_8017ECE4[17].vy) * 4;
        *(u16*)&block->tipB.vz = *(u16*)&D_dryfield_saloon_g_r_8017ECE4[17].vz +
                                 (*(u16*)&dirB->vz - *(u16*)&D_dryfield_saloon_g_r_8017ECE4[17].vz) * 4;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->tipB);
        gte_rtv0();
        gte_stsv(&((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->tipB);
        *(u16*)&block->tipB.vx = *(u16*)&block->tipB.vx + *(u16*)&coord->workm.t[0];
        *(u16*)&block->tipB.vy = *(u16*)&block->tipB.vy + *(u16*)&coord->workm.t[1];
        *(u16*)&block->tipB.vz = *(u16*)&block->tipB.vz + *(u16*)&coord->workm.t[2];

        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->rootA);
        gte_rtps();
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&block->rootB, &((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->tipA,
                 &((_DryfieldSaloonGRShaftScratch*)(head - 0x24))->tipB);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&block->otz);
        if (block->otz >= 0x11) {
            rgb = ((u8)gDisplayState.animFrame & 1) * 16 + 0x20;
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            setRGB0(prim, rgb, rgb, rgb);
            setRGB1(prim, rgb, rgb, rgb);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x24;
}

/// Draws a flickering tapered beam between `arg1` and `arg2` in `arg0`'s
/// local space. Both points are rotated by the coordinate's `workm`, offset by
/// its translation and projected through `GsWSMATRIX`, using a 0x28-byte block
/// taken from `G_SCRATCH_HEAD`. Nothing is drawn when the far end's `otz` is
/// below 0x11; the near end's is raised to at least 0x10. The ends get the
/// screen radii `(s16)arg3 * 64 / otz`.
///
/// Two passes, a quarter turn apart, each queue three `POLY_G4`s: a wedge of
/// the near end's disc, a quad joining the two ends, and a wedge of the far
/// end's disc walked backwards from a full turn, so the near end covers one
/// half turn and the far end the other. Centre vertices take a grey of 0x20
/// or 0x30 on the parity of `gDisplayState.animFrame`, rim vertices are black.
/// Each primitive goes into the OT bucket of its own end's `otz` with a
/// `Gp_AddTpageShift` tpage.
void func_dryfield_saloon_g_r_8017E430(GsCOORDINATE2* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    u8*                head;
    RoomDraw24Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x28;
        *scratch = tmp;
        block    = (RoomDraw24Scratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    *(u16*)&block->vec0.vx = *(u16*)&block->vec0.vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&block->vec0.vy = *(u16*)&block->vec0.vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&block->vec0.vz = *(u16*)&block->vec0.vz + *(u16*)&arg0->workm.t[2];

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg2);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    *(u16*)&block->vec1.vx = *(u16*)&block->vec1.vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&block->vec1.vy = *(u16*)&block->vec1.vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&block->vec1.vz = *(u16*)&block->vec1.vz + *(u16*)&arg0->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx1);
    gte_stszotz(&((RoomDraw24Scratch*)(head - 0x28))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw24Scratch*)(head - 0x28))->otz0 < 0x10) {
            ((RoomDraw24Scratch*)(head - 0x28))->otz0 = 0x10;
        }
        extent    = (s16)arg3 * 64;
        r0        = extent / ((RoomDraw24Scratch*)(head - 0x28))->otz0;
        r1        = extent / block->otz1;
        ang       = 0;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 16) | 0x20;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
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
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang * 2)) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang * 2)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(ang * 2)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(ang * 2)) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
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
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(0x1000 - ang)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(0x1000 - ang)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(0xE00 - ang)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(0xE00 - ang)) >> 12);
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            prim->x3 = block->sx1 + ((block->r1 * rsin(0xC00 - ang)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(0xC00 - ang)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x28;
}
