#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "decomp/common.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d830.h"

/// Grey level of each of the three variants the ambient sprite task can be
/// spawned as, picked by bits 8..9 of `Task::spawnArg1`.
typedef struct RgSpriteLevels {
    /* 0x0 */ u8 v[3];
} RgSpriteLevels;

extern GpQuadCorner D_80111E38[];

/// Messages the room task answers, terminated by id 0x7FFFFFFF.
extern GpMsgEntry D_acropolis_roof_garden_80183BDC[];
extern Task*      D_acropolis_roof_garden_80183C0C;
extern TaskDesc   D_acropolis_roof_garden_80183C10;
extern s32        D_acropolis_roof_garden_80183D74;
extern s32        D_acropolis_roof_garden_80184194;
extern s32        D_acropolis_roof_garden_8018432C;
extern s32        D_acropolis_roof_garden_80184B08;

/// Ten spawn offsets for the roof garden's ambient effects, indexed 0..9 by
/// the effect task's first-frame burst.
extern SVECTOR D_acropolis_roof_garden_80184BF8[10];

/// Per-variant mask of camera views the ambient sprite is visible from,
/// indexed by the low nibble of `Task::spawnArg1`.
extern u16 D_acropolis_roof_garden_80184C48[];

extern s16 D_acropolis_roof_garden_80184C5C[];

/// Volume the ambience task last handed the sound driver.
extern s32 D_acropolis_roof_garden_80186E94;

/// Whole-unit X/Y/Z displacement left by `func_acropolis_roof_garden_8017F870`.
extern SVECTOR D_acropolis_roof_garden_80186E98;

void func_acropolis_roof_garden_8017DB74(Task* arg0);
void func_acropolis_roof_garden_8017DBEC(Task* task);
void func_acropolis_roof_garden_8017F560(GpCoord* arg0, s32 arg1, s16 arg2);

/// State handlers of the room task: set-up, the per-frame tick and `taskKill`.
const TaskFuncTable3 D_acropolis_roof_garden_8017D5C4 = {
    { func_acropolis_roof_garden_8017DB74, func_acropolis_roof_garden_8017DBEC, taskKill },
};

const RgSpriteLevels D_acropolis_roof_garden_8017D5D0 = { { 0x40, 0x60, 0x10 } };

/// Keeps the roof garden's ambience (sound id 0x510D0005) in step with the
/// session's weather/time state: state 5 plays it at 0x1E, state 7 at full
/// 0x64 and anything else silences it. `D_acropolis_roof_garden_80186E94`
/// latches the volume currently playing, so the task only talks to the sound
/// driver on a change - starting the loop, fading it out, or ramping it to the
/// new level. The driver wants attenuation rather than volume, hence the
/// `(0x64 - vol) * 127 / 100` conversion.
void func_acropolis_roof_garden_8017D5D4(Task* task)
{
    s32 vol;
    u8  state;
    s32 prev;

    switch (task->state) {
        case 0:
            D_acropolis_roof_garden_80186E94 = 0;
            task->state                      = task->state + 1;
            return;
        case 1:
            break;
        default:
            return;
    }

    state = (u8)gGameSession->at4.loc.view;
    if (state != 5) {
        vol = 0;
        if (state == 7) {
            vol = 0x64;
        }
    } else {
        vol = 0x1E;
    }

    prev = D_acropolis_roof_garden_80186E94;
    if (vol == prev) {
        return;
    }
    if (prev == 0) {
        SndEvt_EnqueueType6(0x510D0005, 0, (s8)(((0x64 - vol) * 127) / 100));
    } else if (vol == 0) {
        SndEvt_EnqueueType7(0x510D0005, 0x1E);
    } else {
        SndEvt_EnqueueTypeA(0x510D0005, 0, (s8)(((0x64 - vol) * 127) / 100));
    }
    D_acropolis_roof_garden_80186E94 = vol;
}

/// Message gate for the roof garden's hotspot: copies the incoming record to
/// the outgoing one, then runs the message's one-shot side effect.
///
/// Message 0xC, when not a "report only" query (`field_5 == 0`) and its nibble
/// is still clear, advances nibble 7 to 2 and sets collection bit 0x13 to 2.
/// The copy itself is unedited, so the answer is always "allowed".
s32 func_acropolis_roof_garden_8017D71C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->msgId == 0xC && in->field_5 == 0 && GameFlag_GetNibble(7) == 0) {
        GameFlag_SetNibble(7, 2);
        Gp_SetCurBit2Flag(0x13, 2);
    }
    return 1;
}

/// Room script callback with nothing to do: always answers 0.
s32 func_acropolis_roof_garden_8017D798(void)
{
    return 0;
}

s32 func_acropolis_roof_garden_8017D7A0(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    switch (in->field_2) {
        case 1:
            if (((gGameSession->at4.loc.place == 1) || (gGameSession->at4.loc.place == 7)) && (GameFlag_GetNibble(0xCB) == 0)) {
                GameFlag_SetNibble(0xCB, 1);
            }
            break;
        case 2:
            if (((gGameSession->at4.loc.place == 1) || (gGameSession->at4.loc.place == 7)) && (GameFlag_GetNibble(0xCB) == 1)) {
                func_800E8614((s32)&D_acropolis_roof_garden_80184B08, 1);
                GameFlag_SetNibble(0xCB, 2);
            }
            break;
    }
    return 1;
}

s32 func_acropolis_roof_garden_8017D868(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 3:
            SndEvt_EnqueueType6(0x510D0003, 0, 0);
            break;
        case 5:
            break;
        case 9:
            break;
    }
    return 0;
}

s32 func_acropolis_roof_garden_8017D8AC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        if ((Gp_GetCurBit2Flag(0x13) == 0) || (Gp_GetCurBit2Flag(0x13) == 1)) {
            Gp_RunCapCmd1(5);
        } else {
            Gp_StartCapSlot(2, 1, 0);
        }
    }
    if (arg2 == 4) {
        if (GameFlag_GetNibble(0x155) < 6) {
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 6);
        }
        Gp_StateF0.field_4 = 1;
        Gp_RunCapCmd(4, 0);
        func_800E3FAC(0xA2, 7);
    }
    return 0;
}

void func_acropolis_roof_garden_8017D970(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0x14:
            SndEvt_EnqueueType6(0x510D0006, 0, 0);
            break;
        case 0x27:
            SndEvt_EnqueueType6(0x510D0007, 0, 0);
            break;
        case 0x39:
            SndEvt_EnqueueType6(0x510D0008, 0, 0);
            break;
        case 0x63:
            SndEvt_EnqueueType6(0x510D0009, 0, 0);
            break;
        case 0x72:
            SndEvt_EnqueueType6(0x510D000A, 0, 0);
            D_acropolis_roof_garden_80183C0C = NULL;
            taskKill(arg0);
            break;
    }
    arg0->state += 1;
}

void func_acropolis_roof_garden_8017DA48(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0x4C:
            SndEvt_EnqueueType6(0x510D000F, 0, 0);
            break;
        case 0x64:
            SndEvt_EnqueueType6(0x510D0010, 0, 0);
            D_acropolis_roof_garden_80183C0C = NULL;
            taskKill(arg0);
            break;
    }
    arg0->state += 1;
}

void func_acropolis_roof_garden_8017DAD4(s32 arg0)
{
    switch (arg0) {
        case 0:
            D_acropolis_roof_garden_80183C0C = Task_SpawnFromTable(&D_acropolis_roof_garden_80183C10, 1, 0, 0);
            break;
        case 1:
            D_acropolis_roof_garden_80183C0C = Task_SpawnFromTable(&D_acropolis_roof_garden_80183C10, 2, 0, 0);
            break;
        case 2:
            if (D_acropolis_roof_garden_80183C0C != NULL) {
                taskKill(D_acropolis_roof_garden_80183C0C);
                D_acropolis_roof_garden_80183C0C = NULL;
            }
            break;
    }
}

void func_acropolis_roof_garden_8017DB74(Task* arg0)
{
    arg0->msgTable = D_acropolis_roof_garden_80183BDC;
    Game_SetPtrSlot(arg0, 7);
    if (Mc_SaveData.sceneEvent == 6) {
        Mc_SaveData.sceneEvent = 5;
    }
    Task_SpawnFromTable(&D_acropolis_roof_garden_80183C10, 0, 0, 0);
    arg0->state += 1;
}

void func_acropolis_roof_garden_8017DBEC(Task* task)
{
    GpAreaKey key;

    if ((gGameSession->at4.loc.warp == 2) && (D_acropolis_roof_garden_8018432C == 0)) {
        D_acropolis_roof_garden_8018432C = 1;
        func_800E8634((s32)&D_acropolis_roof_garden_80183D74, 0, (s32)&D_acropolis_roof_garden_80184194);
        GameFlag_SetNibble(6, 1);
        key.stage = 1;
        key.area  = 0xC;
        Gp_SetAreaObjId(&key, 3, 1);
    }
}

/// Runs the task's current state through a stack copy of the room's
/// three-entry state table.
void func_acropolis_roof_garden_8017DC74(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_roof_garden_8017D5C4;
    sp.funcs[task->state](task);
}

/// Room script callback: sets `Gp_StateF0.field_1E` to 1.
void func_acropolis_roof_garden_8017DCCC(void)
{
    Gp_StateF0.field_1E = 1;
}

/// Roof-garden ambient effect task. On its first frame it fires one effect per
/// entry of `D_acropolis_roof_garden_80184BF8` - two with a 0x02000000 flavour,
/// one flagged 0x04000102, then seven more - and every frame after that it adds
/// the two view-dependent effects: one while the current view is 5 or 6 (the
/// `0x30 >> view - 1` bit test) and one while it is 7.
void func_acropolis_roof_garden_8017DCDC(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    SVECTOR*   vec;
    s32        i;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (task->state == 0) {
        for (i = 0; i < 2; i++) {
            Gp_SpawnEff(0x6008A, coord, i + 0x2000000, &D_acropolis_roof_garden_80184BF8[i]);
        }
        vec = D_acropolis_roof_garden_80184BF8;
        Gp_SpawnEff(0x6008A, coord, 0x4000102, &vec[2]);
        for (i = 3; i < 10; i++) {
            Gp_SpawnEff(0x6008A, coord, i + 0x200, &vec[i]);
        }
        task->state = task->state + 1;
    }
    if (Gp_State1C->eventState < 4) {
        if ((0x30 >> ((u8)gGameSession->at4.loc.view - 1)) & 1) {
            work->move.vx = -0x12A2;
            work->move.vy = -0xDC;
            work->move.vz = -0xF19;
            Gp_SpawnEff(0x60090, coord, 0x60E, &work->move);
        }
        if ((u8)gGameSession->at4.loc.view == 7) {
            work->move.vx = -0x12A2;
            work->move.vy = -0xDC;
            work->move.vz = -0xF19;
            Gp_SpawnEff(0x60090, coord, 0x8000030E, &work->move);
        }
    }
}

/// One of the roof garden's ambient sprites. It is only drawn while the scene
/// is still on `Gp_State1C->eventState` 0 or 1 and the current camera view is one
/// the variant's mask in `D_acropolis_roof_garden_80184C48` allows; otherwise
/// the frame is skipped entirely.
///
/// When it does draw, the task's coordinate is refreshed and projected through
/// `GsWSMATRIX` into a 0x14-byte `RoomShaftScratch` block taken from
/// `G_SCRATCH_HEAD`, and the projected point becomes the centre of a
/// semi-transparent `POLY_FT4` on tpage 0x2B whose half-extent is
/// `scale * 0x27 / otz`, so the sprite shrinks with distance and is dropped
/// entirely inside `otz` 0x11. `Task::spawnArg1` is unpacked once, on the first
/// frame: bits 16..27 are the sprite's size (defaulting to 0x280 when zero),
/// bits 8..9 pick one of three 0x28x0x27 cells across the sheet -- and, through
/// `getClut`, the matching 16-colour palette -- and only the low nibble is
/// kept, as the index into the view mask. The grey level is the variant's own
/// level from `D_acropolis_roof_garden_8017D5D0`, brightened by 0x10 on odd
/// frames so the sprite flickers.
void func_acropolis_roof_garden_8017DE90(Task* arg0)
{
    GpEffWork*        mem;
    GpCoord*          coord;
    void**            scratch;
    u8*               head;
    RoomShaftScratch* blk;
    POLY_FT4*         prim;
    RgSpriteLevels    base;
    s32               param;
    s32               lvl;
    s32               flicker;
    s16               x;
    s16               y;

    mem   = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;
    if (Gp_State1C->eventState < 2) {
        if ((D_acropolis_roof_garden_80184C48[arg0->spawnArg1 & 0xF] >> ((u8)gGameSession->at4.loc.view - 1)) & 1) {
            Gp_UpdateCoord(coord);
            scratch  = (void**)G_SCRATCH_HEAD;
            head     = *scratch;
            *scratch = head - sizeof(RoomShaftScratch);
            blk      = (RoomShaftScratch*)(head - sizeof(RoomShaftScratch));
            if (arg0->state == 0) {
                base            = D_acropolis_roof_garden_8017D5D0;
                param           = arg0->spawnArg1;
                mem->scale      = (param & 0x0FFF0000) ? ((param >> 16) & 0xFFF) : 0x280;
                mem->angle      = (arg0->spawnArg1 >> 8) & 3;
                arg0->spawnArg1 = arg0->spawnArg1 & 0xF;
                mem->period     = base.v[mem->angle];
                arg0->state++;
            }
            blk->vec.vx = (u16)coord->workm.t[0];
            blk->vec.vy = (u16)coord->workm.t[1];
            blk->vec.vz = (u16)coord->workm.t[2];
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_ldv0(&blk->vec);
            gte_rtps();
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            gte_stsxy(&blk->sx);
            gte_stszotz(&blk->otz);
            if (blk->otz >= 0x11) {
                flicker     = ((u8)gDisplayState.animFrame & 1) * 0x10;
                lvl         = (u8)mem->period + flicker;
                prim->tpage = 0x2B;
                setRGB0(prim, lvl, lvl, lvl);
                prim->code |= 2;
                prim->clut  = getClut(mem->angle * 0x10, 0x10E);
                prim->u0    = mem->angle * 0x28;
                prim->v0    = 0;
                prim->u1    = mem->angle * 0x28 + 0x27;
                prim->v1    = 0;
                prim->u2    = mem->angle * 0x28;
                prim->v2    = 0x27;
                prim->u3    = mem->angle * 0x28 + 0x27;
                prim->v3    = 0x27;

                blk->halfWidth = (mem->scale * 0x27) / blk->otz;
                x              = blk->sx - (u16)blk->halfWidth;
                prim->x2       = x;
                prim->x0       = x;
                x              = blk->sx + (u16)blk->halfWidth;
                prim->x3       = x;
                prim->x1       = x;
                y              = blk->sy - (u16)blk->halfWidth;
                prim->y1       = y;
                prim->y0       = y;
                y              = blk->sy + (u16)blk->halfWidth;
                prim->y3       = y;
                prim->y2       = y;
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
            }
            SCRATCH_POP(RoomShaftScratch);
        }
    }
}

/// Pulsating roof-garden flare, projected into scratch memory and drawn with
/// Gouraud polygons and optional rays.
void func_acropolis_roof_garden_8017E29C(Task* arg0)
{
    GpCoord*         coord;
    void*            mem;
    u8*              head;
    RoomGlowScratch* blk;
    POLY_G4*         prim;
    LINE_G3*         line;
    s32              i;
    s32              pulse;
    s32              level;
    s32              h;
    s16              lvl;
    s16              flip;
    s32              z;
    u32              tag;
    u_long*          ot;
    u8               red;
    s32              shift;
    u32              depth;

    coord = arg0->extra.tmd->coords;
    mem   = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    head        = SCRATCH_HEAD(void);
    blk         = (RoomGlowScratch*)(SCRATCH_HEAD(void) = head - 0x18);
    blk->vec.vx = coord->workm.t[0];
    blk->vec.vy = coord->workm.t[1];
    blk->vec.vz = coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomGlowScratch*)(head - 0x18))->vec);
    gte_rtps();
    gte_stsxy(&((RoomGlowScratch*)(head - 0x18))->sx);
    gte_stszotz(&blk->otz);
    if (((RoomGlowScratch*)(head - 0x18))->otz >= 0x11) {
        pulse  = gDisplayState.animFrame;
        pulse *= arg0->spawnArg1 & 0xFF;
        flip   = (arg0->spawnArg1 >> 16) & 1;
        if (pulse & 0x80) {
            level  = ~pulse;
            level &= 0x7F;
        } else {
            level = pulse & 0x7F;
        }
        lvl   = level * 2;
        level = arg0->spawnArg1;
        if (level < 0) {
            h           = (level >> 8) & 0xFF;
            blk->rOuter = (h << 10) / blk->otz;
            blk->rInner = (h << 7) / blk->otz;
            for (i = 0; i < 0x10; i += 2) {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, (lvl * (flip ^ 1)) >> 1, (flip * lvl) >> 1, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i + 4]) >> 12);
                prim->y0 = blk->sy + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i]) >> 12);
                prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i + 5]) >> 12);
                prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i + 1]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i + 6]) >> 12);
                prim->y3 = blk->sy + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i + 2]) >> 12);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);

                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, lvl * (flip ^ 1), flip * lvl, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i + 4]) >> 13);
                prim->y0 = blk->sy + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i]) >> 13);
                prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i + 5]) >> 13);
                prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i + 1]) >> 13);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i + 6]) >> 13);
                prim->y3 = blk->sy + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i + 2]) >> 13);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            {
                s32 half = lvl >> 1;
                for (i = 2; i < 0x10; i += 8) {
                    do {
                        prim           = (POLY_G4*)gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        red = half * (flip ^ 1);
                        setRGB2(prim, red, flip * half, 0);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = blk->sx + ((blk->rInner * D_acropolis_roof_garden_80184C5C[i]) >> 12);
                        prim->y0 = blk->sy + ((blk->rInner * D_acropolis_roof_garden_80184C5C[i - 4]) >> 12);
                        prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i + 4]) >> 11);
                        prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i]) >> 11);
                        prim->x2 = blk->sx;
                        prim->y2 = blk->sy;
                        prim->x3 = blk->sx + ((blk->rInner * D_acropolis_roof_garden_80184C5C[i + 8]) >> 12);
                        prim->y3 = blk->sy + ((blk->rInner * D_acropolis_roof_garden_80184C5C[i + 4]) >> 12);
                        shift    = gDisplayState.otDepthShift;
                        depth    = (((u32)blk->otz << shift) >> 2) & 0xFFC;
                        // Keep the shift and its source live through the first OT address.
                        __asm__("" : "+r"(depth) : "r"(shift), "m"(gDisplayState.otDepthShift));
                        setaddr(prim, getaddr((u_long*)(depth + (s32)gGpuCurrentOt)));
                        ot  = (u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt);
                        tag = (*ot & 0xFF000000) | ((u32)prim & 0xFFFFFF);
                        *ot = tag;
                        z   = blk->otz;
                        SOFT_TOUCH_REG(z);
                        SOFT_TOUCH_REG(z);
                        SOFT_TOUCH_REG_USE(z, tag);
                        SOFT_TOUCH_REG_USE(prim, z);
                        Gp_AddTpageShift((P_TAG*)prim, 1, z);

                        prim           = (POLY_G4*)gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, red, flip * half, 0);
                    } while (0);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = blk->sx + ((blk->rInner * D_acropolis_roof_garden_80184C5C[i + 4]) >> 13);
                    prim->y0 = blk->sy + ((blk->rInner * D_acropolis_roof_garden_80184C5C[i]) >> 13);
                    prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i + 8]) >> 12);
                    prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_roof_garden_80184C5C[i + 4]) >> 12);
                    prim->x2 = blk->sx;
                    prim->y2 = blk->sy;
                    prim->x3 = blk->sx + ((blk->rInner * D_acropolis_roof_garden_80184C5C[i + 12]) >> 13);
                    prim->y3 = blk->sy + ((blk->rInner * D_acropolis_roof_garden_80184C5C[i + 8]) >> 13);
                    addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    z = blk->otz;
                    __asm__("" : "+r"(z) : "r"(red), "r"(&D_acropolis_roof_garden_80184C5C[i]));
                    SOFT_TOUCH_REG_USE(prim, z);
                    Gp_AddTpageShift((P_TAG*)prim, 1, z);
                }
            }
        } else {
            blk->rOuter = (((level >> 8) & 0xFF) << 9) / blk->otz;
            for (i = 0; i < 2; i++) {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, lvl * (flip ^ 1), flip * lvl, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx - blk->rOuter;
                prim->x1 = prim->x2 = blk->sx;
                prim->x3            = blk->sx + blk->rOuter;
                prim->y0 = prim->y2 = prim->y3 = blk->sy;
                prim->y1                       = (blk->sy - blk->rOuter) + blk->rOuter * (i + i);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            if (arg0->spawnArg1 & 0x10000000) {
                for (i = 0; i < 2; i++) {
                    line           = (LINE_G3*)gGpuPrimCursor;
                    gGpuPrimCursor = line + 1;
                    setLineG3(line);
                    setRGB0(line, 0, 0, 0);
                    setRGB1(line, lvl * (flip ^ 1), flip * lvl, 0);
                    setRGB2(line, 0, 0, 0);
                    line->x0 = blk->sx + blk->rOuter * (i * 3 - 1);
                    line->y0 = blk->sy - blk->rOuter * (i + 1);
                    line->x1 = blk->sx;
                    line->y1 = blk->sy;
                    line->x2 = blk->sx - blk->rOuter * (i * 3 - 1);
                    line->y2 = blk->sy + blk->rOuter * (i + 1);
                    addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            line);
                    Gp_AddTpageShift((P_TAG*)line, 1, blk->otz);
                }
            }
        }
    }
    SCRATCH_POP_BYTES(0x18);
    Gp_ReleaseState1CMem(mem, arg0);
}

/// One drifting mote of the room's ambient effect. The first tick seeds it
/// from `Gp_LcgState`: a size of 0x20, a random tilt pair (`period` /
/// `step`) and a random drift in `move`. While it flies, the drift
/// moves its coordinate frame and the tilt rotates it; each drift axis eases
/// back towards zero by one a tick and re-rolls a fresh multiple of 8 when it
/// gets there, and the tilt wanders by a random step. Once the frame has
/// risen past the origin the mote fades in by 0x10 a tick up to 0x80, then
/// fades back out and releases its work block.
void func_acropolis_roof_garden_8017F10C(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    s32        vy;
    s32        vx;
    s32        vz;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    Gp_UpdateCoord(coord);
    work->age++;
    switch (task->state) {
        case 0:
            work->scale   = 0x20;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period  = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1F0);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->step    = 0x80 - (((u32)Gp_LcgState >> 16) & 0xF0);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            task->state   = 1;
            /* fallthrough */
        case 1:
            coord->coord.t[0] += work->move.vx;
            coord->coord.t[1] += work->move.vy;
            coord->coord.t[2] += work->move.vz;
            Gfx_RotMatrixX(&coord->coord, work->period, 0);
            Gfx_RotMatrixZ(&coord->coord, work->step, 0);
            coord->flg = 0;

            vy = work->move.vy;
            if (vy >= 0x1D) {
                vy = vy - 1;
            } else {
                vy = vy + 1;
            }
            work->move.vy = vy;

            vx = work->move.vx;
            if (vx == 0) {
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->move.vx += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vx > 0) {
                    vx = vx - 1;
                } else {
                    vx = vx + 1;
                }
                work->move.vx = vx;
            }

            vz = work->move.vz;
            if (vz == 0) {
                work->move.vz += work->step % 32;
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->move.vz += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vz > 0) {
                    vz = vz - 1;
                } else {
                    vz = vz + 1;
                }
                work->move.vz = vz;
            }

            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 0x10;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->step   += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 8;

            if (coord->coord.t[1] > 0) {
                task->state = 2;
            }
            func_acropolis_roof_garden_8017F560(coord, work->scale, 0);
            break;
        case 2:
            if (work->angle < 0x80) {
                work->angle += 0x10;
            } else {
                task->state = 3;
            }
            func_acropolis_roof_garden_8017F560(coord, work->scale, 0);
            break;
        case 3:
            if (work->angle >= 0x11) {
                work->angle -= 0x10;
                func_acropolis_roof_garden_8017F560(coord, work->scale, work->angle);
            } else {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Draws one mote: the unit quad `D_80111E38` scaled by `arg1`, rotated and
/// placed by the mote's coordinate frame, then projected through
/// `GsWSMATRIX` into a textured quad. A mote nearer than `otz` 0x11 is not
/// drawn. `arg2` is the fade level: zero draws the texture unshaded, anything
/// else modulates it to that grey and draws it semi-transparent.
void func_acropolis_roof_garden_8017F560(GpCoord* arg0, s32 arg1, s16 arg2)
{
    register GpCoord* coord asm("t7");
    register void**   scratch asm("a0");
    u8*               head;
    RoomQuadScratch*  blk;
    POLY_FT4*         prim;
    GpQuadCorner*     tbl;
    SVECTOR*          sv;
    MATRIX*           wm;
    s32               i;

    coord   = arg0;
    scratch = (void**)G_SCRATCH_HEAD;
    i       = 0;
    wm      = &coord->workm;
    tbl     = D_80111E38;
    head    = SCRATCH_HEAD_AT(scratch, u8) - sizeof(RoomQuadScratch);
    /* `head` and `blk` have to stay separate registers: the ROM computes the
       block address into a scratch register and copies it into the one the
       rest of the function uses. */
    SOFT_TOUCH_REG(head);
    blk      = (RoomQuadScratch*)head;
    *scratch = blk;
    do {
        blk->v[i].vx = tbl[i].x * arg1;
        // Spelled as an offset rather than `&blk->v[i]` so it stays a separate
        // pointer from the one the GTE macros below take; writing both the same
        // way lets CSE fold them into one register and the loop stops matching.
        sv     = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(RoomQuadScratch, v));
        sv->vy = 0;
        sv->vz = tbl[i].y * arg1;
        gte_SetRotMatrix(wm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0();
        gte_stsv(&blk->v[i]);
        (u16) blk->v[i].vx = (u16)blk->v[i].vx + (u16)coord->workm.t[0];
        (u16) sv->vy       = (u16)sv->vy + (u16)coord->workm.t[1];
        i++;
        (u16) sv->vz = (u16)sv->vz + (u16)coord->workm.t[2];
    } while (i < 4);

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    setUV4(prim, 0, 0xE8, 7, 0xE8, 0, 0xEF, 7, 0xEF);
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    if (blk->otz >= 0x11) {
        if (arg2 != 0) {
            setRGB0(prim, arg2, arg2, arg2);
            setSemiTrans(prim, 1);
        } else {
            setShadeTex(prim, 1);
        }
        prim->tpage = 0x2B;
        prim->clut  = 0x4390;
        addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP(RoomQuadScratch);
}

/// reports one, adds its X and Z to the coordinate's translation, rounding a
/// fractional part away from zero. The whole-unit displacement is also left in
/// `D_acropolis_roof_garden_80186E98`. Returns non-zero when the X or Z
/// displacement is non-zero.
s32 func_acropolis_roof_garden_8017F870(GpCoord* coord, GpRec18* rec, s16 arg2)
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
        coord->coord.t[0]                  += ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]                  += s->delta.vz.h.hi;
        D_acropolis_roof_garden_80186E98.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_acropolis_roof_garden_80186E98.vy = s->delta.vy.w >> 16;
        D_acropolis_roof_garden_80186E98.vz = s->delta.vz.w >> 16;
        val                                 = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_acropolis_roof_garden_80186E98.vx++;
            } else {
                coord->coord.t[0]--;
                D_acropolis_roof_garden_80186E98.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_acropolis_roof_garden_80186E98.vz++;
            } else {
                coord->coord.t[2]--;
                D_acropolis_roof_garden_80186E98.vz--;
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
s32 func_acropolis_roof_garden_8017FA14(GpCoord* coord, GpRec18* recs, s16 count, s16 push)
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
    st->eye.vx = (u16)coord->coord.t[0];
    st->eye.vy = (u16)coord->coord.t[1];
    vz         = (u16)coord->coord.t[2];
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

/// Item-pickup model task step: the item's mesh is only visible from views 5
/// through 7, and stays hidden once the item's 2-bit flag reads 2 (already
/// taken). The three hidden cases are written as separate tests so the two view
/// comparisons are not folded into one unsigned range check.
void func_acropolis_roof_garden_80180160(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;
    s32         view;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = task->extra.tmd;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    view = Gp_GetViewIndex();
    if (view >= 8) {
        tmd->flags = 0x80;
    } else if (view < 5) {
        tmd->flags = 0x80;
    } else if (flag == 2) {
        tmd->flags = 0x80;
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
    }
}
