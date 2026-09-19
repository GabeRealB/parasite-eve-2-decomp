#include "common.h"

#include "actors/actor_511000.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern SVECTOR    D_actor_511000_80147344[];
extern SVECTOR    D_actor_511000_80147704[];
extern SVECTOR    D_actor_511000_80147AC4[];
extern u8         D_actor_511000_80147E84[];
extern GpImgRec   D_actor_511000_80147EA4;
extern u16*       D_actor_511000_80147EB0;
extern u8         D_actor_511000_80147EC4[];
extern CVECTOR    D_actor_511000_80149004[];
extern DVECTOR    D_actor_511000_80149014[];
extern GpMsgEntry D_actor_511000_80148FC4[];

void func_actor_511000_80132B14(Task* task, CVECTOR* col, s8* rgb);

/// Camera path `func_actor_511000_801330F0` walks once the session reaches
/// mode 0x18, one 0x24-byte `GpViewRec` per step of the kill countdown: the
/// rotation and projection plane repeat down the table while the translation
/// descends, so the spawn of a view task per index pans the camera as the
/// actor goes down. Handed straight to `Gp_TrySpawnViewTask`, exactly as
/// `Gp_SpawnViewTasks` hands its own stage record.
extern GpViewRec D_actor_511000_80147EE4[];

/// The three texture records the message-0x7E0 handler uploads, one per mode.
/// Each is a lone `GpImgRec` whose 0x18x0x10 source rect repeats the size the
/// handler's scratch `RECT` carries and whose `data` points at its pixel blob.
extern GpImgRec D_actor_511000_80146C74;
extern GpImgRec D_actor_511000_80146F94;
extern GpImgRec D_actor_511000_801472B4;

/// Message-0x7D5 handler: the four-way visibility/mode switch on the message's
/// mode word, run against the `TmdObject` parked in `Task::extra`. Mode 0 shows
/// the model (`field_C` bit 0x80) and clears the 4 flag, 1 hides it, frees the
/// aux buffers and clears the flag, 2 does both plus latching the mode into the
/// work block's `field_480`, and 3 hides it while setting the flag. Anything
/// else returns 1 and leaves the object alone; the handled modes return 0.
/// The handler reads `work` before the switch even though mode 2 is its only
/// use, so retail's `lw $v1,0x1C($a0)` sits in the entry block. The same body
/// shape as `func_actor_141000_80133E8C` / `func_actor_503500_80132584`.
s32 func_actor_511000_801327A0(GpActorWork* arg0, s32 arg1, s32 mode)
{
    TmdObject*        obj;
    Actor511000Work2* work;
    s32               ret;

    obj  = arg0->extra;
    work = (Actor511000Work2*)arg0->actor;
    ret  = 0;

    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags          |= 0x80;
            work->field_480.word = mode;
            obj->flags          |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message-0x7DB handler: un-hides the model its first child task carries in
/// `Task::extra` (`field_C` bit 0x80) for mode 1 and hides it for mode 0, then
/// hides the second child as well on the mode-1 path -- the same two tasks
/// `func_actor_511000_80132480` parked at `field_4C4` / `field_4C8`. Any other
/// mode leaves both alone.
/// The `default:` arm jumps straight to the shared `return 0` instead of
/// falling through the hide block: retail's single epilogue is only reached
/// that way, the hide block and the shared return merging into one block whose
/// first label sits on the value store.
s32 func_actor_511000_8013287C(GpActorWork* arg0, s32 arg1, Actor511000Msg* msg)
{
    Actor511000Work2* work;
    Task*             child;
    u16               mode;

    mode = msg->field_2;
    work = (Actor511000Work2*)arg0->actor;

    switch (mode) {
        case 0:
            child = work->field_4C4;
            break;
        case 1:
            child = work->field_4C4;
            if (child != NULL) {
                ((TmdObject*)child->extra)->flags |= 0x80;
            }
            child = work->field_4C8;
            break;
        default:
            goto out;
    }

    if (child != NULL) {
        ((TmdObject*)child->extra)->flags &= 0xFF7F;
    }
out:
    return 0;
}

/// Message-0x7E0 handler: uploads one of the actor's three texture records
/// over the 0x18x0x10 rect at y 0x28 -- `D_actor_511000_801472B4` for mode 1,
/// `D_actor_511000_80146C74` for modes 0 and 2, and `D_actor_511000_80146F94`
/// for mode 3, which sets the work block's `field_4D0` / `field_4CC` to 1
/// first. Any other mode leaves the image NULL and returns 0.
/// The mode-1 case is written first because the compiler lays the case bodies
/// out in source order and that is the order the retail image has them in.
s32 func_actor_511000_80132904(GpActorWork* arg0, s32 arg1, s32 mode)
{
    RECT      rect;
    GpImgRec* img;
    s32       ret;

    ret    = 0;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (mode) {
        case 1:
            img = &D_actor_511000_801472B4;
            break;
        case 0:
        case 2:
            img = &D_actor_511000_80146C74;
            break;
        case 3:
            ((Actor511000Work2*)arg0->actor)->field_4D0 = 1;
            ((Actor511000Work2*)arg0->actor)->field_4CC = 1;
            img                                         = &D_actor_511000_80146F94;
            break;
        default:
            img = NULL;
            break;
    }

    if (img != NULL) {
        ret = Gp_LoadActorImage(arg0, img, &rect);
    }
    return ret;
}

/// Per-frame tick for a child of the spawner: mirrors the parent model's
/// visibility bit (`field_C` 0x80) onto its own model, and once the session
/// reaches mode 0x18 poses its root coordinate from the parent's
/// `killCountdown` entry in `D_actor_511000_80147AC4`. Within three steps of
/// countdown 0x59 it also picks that distance's colour from
/// `D_actor_511000_80149004`, darkened by 0x1E per channel, and hands both to
/// `func_actor_511000_80132B14`.
/// The table is loaded into its own local before indexing: `&table[d]` on the
/// symbol directly shifts `d` ahead of the `lui`/`addiu` pair.
void func_actor_511000_801329C4(Task* task)
{
    Task*             parent;
    TmdObject*        extra;
    Actor511000Coord* coord;
    CVECTOR*          col;
    CVECTOR*          tbl;
    s32               d;
    s8                rgb[3];

    parent = (Task*)task->spawnArg2;
    extra  = (TmdObject*)task->extra;
    coord  = (Actor511000Coord*)extra->coords;

    if (!(((TmdObject*)parent->extra)->flags & 0x80)) {
        extra->flags &= 0xFF7F;
    } else {
        extra->flags |= 0x80;
    }

    if (gGameSession->at4.loc.view == 0x18) {
        coord->rot.vx = D_actor_511000_80147AC4[parent->killCountdown].vx;
        coord->rot.vy = D_actor_511000_80147AC4[parent->killCountdown].vy;
        coord->rot.vz = D_actor_511000_80147AC4[parent->killCountdown].vz;
        RotMatrix(&coord->rot, &coord->coord);
        coord->flg = 0;

        d = parent->killCountdown - 0x59;
        if (d < 0) {
            d = 0x59 - parent->killCountdown;
        }
        if (d < 4) {
            tbl    = D_actor_511000_80149004;
            col    = &tbl[d];
            rgb[0] = col->r - 0x1E;
            rgb[1] = col->g - 0x1E;
            rgb[2] = col->b - 0x1E;
            func_actor_511000_80132B14(task, col, rgb);
        }
    }
}

/// Draws a semi-transparent gradient disc at the model's root: projects the
/// parent-composed origin, scales the 16 unit offsets in
/// `D_actor_511000_80149014` by 0x12C/0x1000 around it, and fans 16 `POLY_G3`
/// from the centre (`col`) to the rim (`rgb`) into one OT slot, followed by an
/// additive draw-mode `DR_TPAGE`. Both `pts` and the offset table walk by
/// pointer and `scale` is a variable, which is what keeps the `mult` and the
/// retail induction-variable order.
void func_actor_511000_80132B14(Task* task, CVECTOR* col, s8* rgb)
{
    SVECTOR   pos;
    DVECTOR   pts[16];
    MATRIX    mtx;
    s32       sxy;
    s32       p;
    s32       flag;
    s32       otz;
    POLY_G3*  prim;
    DR_TPAGE* dr;
    u16       x;
    u16       y;
    s32       scale;
    u32*      ot;
    s32       i;
    DVECTOR*  pt;
    DVECTOR*  src;

    Gp_ComposeParentWorld(((TmdObject*)task->extra)->coords, &mtx, &pos);
    SetRotMatrix(&Gfx_ViewWorldMtx);
    SetTransMatrix(&Gfx_ViewWorldMtx);
    otz   = RotTransPers(&pos, &sxy, (long*)&p, (long*)&flag);
    x     = sxy;
    y     = sxy >> 16;
    src   = D_actor_511000_80149014;
    pt    = pts;
    scale = 0x12C;
    for (i = 0; i < 16; i++) {
        pt->vx = x + scale * src->vx / 0x1000;
        pt->vy = y + scale * src->vy / 0x1000;
        pt++;
        src++;
    }
    ot = (u32*)((u32)Gpu_CurrentOt + (((u32)(otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)) - 30;
    pt = pts;
    for (i = 0; i < 15; i++, pt++) {
        prim           = (POLY_G3*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setPolyG3(prim);
        setSemiTrans(prim, 1);
        prim->x0 = x;
        prim->y0 = y;
        prim->x1 = pt->vx;
        prim->y1 = pt->vy;
        prim->x2 = pt[1].vx;
        prim->y2 = pt[1].vy;
        prim->r0 = col->r;
        prim->g0 = col->g;
        prim->b0 = col->b;
        prim->r1 = rgb[0];
        prim->g1 = rgb[1];
        prim->b1 = rgb[2];
        prim->r2 = rgb[0];
        prim->g2 = rgb[1];
        prim->b2 = rgb[2];
        addPrim(ot, prim);
    }
    prim           = (POLY_G3*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setPolyG3(prim);
    setSemiTrans(prim, 1);
    prim->x0 = x;
    prim->y0 = y;
    prim->x1 = pt->vx;
    prim->y1 = pt->vy;
    prim->x2 = pts[0].vx;
    prim->y2 = pts[0].vy;
    prim->r0 = col->r;
    prim->g0 = col->g;
    prim->b0 = col->b;
    prim->r1 = rgb[0];
    prim->g1 = rgb[1];
    prim->b1 = rgb[2];
    prim->r2 = rgb[0];
    prim->g2 = rgb[1];
    prim->b2 = rgb[2];
    addPrim(ot, prim);
    dr             = Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    setDrawTPage(dr, 1, 0, 0x2A);
    addPrim(ot, dr);
}

/// Palette fade: steps `field_2C` up by 0x555 per frame while the
/// `field_2E` hold counter is live (counting it down once the blend saturates
/// at 0x1000), otherwise snaps it back to 0 and re-arms the hold at 0x1E. Each
/// of the 16 little-endian 15-bit colours is then blended between
/// `D_actor_511000_80147E84` and `D_actor_511000_80147EC4` by that weight into
/// the `field_C` CLUT, which `D_actor_511000_80147EA4` uploads.
/// The destination is formed as `work + i` before the field offset so the
/// `addu` keeps the index first and CSE cannot fold the 0xC into a store.
void func_actor_511000_80132E6C(Actor511000Work* work)
{
    CVECTOR col[3];
    s32     i;
    s32     c;
    s32     inv;
    s32     fade;
    u8*     src0;
    u8*     src1;
    u8*     dst;

    if (work->field_2E != 0) {
        work->field_2C += 0x555;
        i               = 0;
        if (work->field_2C >= 0x1000) {
            work->field_2C = 0x1000;
            if (--work->field_2E < 0) {
                work->field_2E = 0;
            }
        }
    } else {
        work->field_2C -= 0x1000;
        i               = 0;
        if (work->field_2C <= 0) {
            work->field_2C = 0;
            work->field_2E = 0x1E;
        }
    }
    inv  = 0x1000 - work->field_2C;
    fade = work->field_2C;
    do {
        dst      = (u8*)(i + (s32)work);
        dst      = ((Actor511000Work*)dst)->field_C;
        src0     = &D_actor_511000_80147E84[i];
        src1     = &D_actor_511000_80147EC4[i];
        c        = src0[0] | (src0[1] << 8);
        col[0].r = ((u16)c >> 10) & 0x1F;
        col[0].g = ((u16)c >> 5) & 0x1F;
        col[0].b = c & 0x1F;
        c        = src1[0] | (src1[1] << 8);
        col[1].r = ((u16)c >> 10) & 0x1F;
        col[1].g = ((u16)c >> 5) & 0x1F;
        col[1].b = c & 0x1F;
        LoadAverageCol(&col[0], &col[1], inv, fade, &col[2]);
        c      = col[2].b + ((col[2].r << 10) + (col[2].g << 5));
        dst[1] = (u32)c >> 8;
        i     += 2;
        dst[0] = c;
    } while (i < 0x20);
    Gp_LoadImages(&D_actor_511000_80147EA4);
}

/// Spawn/setup state: allocates the 0x70 work block, parks it in `work`,
/// arms the buffer-free countdown at -1, un-hides the model (`field_C` bit
/// 0x80), places it at rot/trans index 0, binds light/color, installs the
/// message table, and publishes `work->field_C` through
/// `D_actor_511000_80147EB0` before advancing to the per-frame state.
void func_actor_511000_80133034(Task* task)
{
    Actor511000Work* work;
    TmdObject*       extra;

    extra = (TmdObject*)task->extra;
    work  = (Actor511000Work*)Mem_Calloc(0x70, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work    = (TaskIdMap*)work;
    work->field_8 = -1;
    extra->flags |= 0x80;
    func_actor_511000_801336E0(task, D_actor_511000_80147344, D_actor_511000_80147704, 0);
    func_actor_511000_801337F0(task);
    do {
        task->msgTable          = D_actor_511000_80148FC4;
        D_actor_511000_80147EB0 = (u16*)work->field_C;
    } while (0);
    task->state += 1;
}

/// Per-frame state: while the model is hidden (`field_C` bit 0x80 clear) it
/// refreshes the root coordinate, rebuilds the colour matrix from that
/// coordinate's own translation, and runs the work block's follow-up. Once the
/// session reaches mode 0x18 it walks `killCountdown` up to 0x77, spawning a
/// view task for the camera record at each index and re-posing the model from
/// the matching rotations, and finally runs the `Tmd_FreeBuffers` countdown the
/// spawn state armed at -1, freeing the buffers and latching the field back to
/// -1 on the frame the countdown reaches zero.
void func_actor_511000_801330F0(Task* task)
{
    Actor511000Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              countdown;
    s16              frame;

    obj   = (TmdObject*)task->extra;
    work  = (Actor511000Work*)task->work;
    coord = obj->coords;

    if (!(obj->flags & 0x80)) {
        Gp_UpdateCoord(coord);
        func_800D7A9C(obj, (VECTOR*)coord->workm.t, 0, 3);
        func_actor_511000_80132E6C((Actor511000Work*)task->work);
    }
    if (gGameSession->at4.loc.view == 0x18) {
        frame               = task->killCountdown + 1;
        task->killCountdown = frame;
        if (frame >= 0x78) {
            task->killCountdown = 0x77;
        }
        Gp_TrySpawnViewTask((s32)&D_actor_511000_80147EE4[task->killCountdown]);
        func_actor_511000_801336E0(task, D_actor_511000_80147344, D_actor_511000_80147704, task->killCountdown);
        coord->flg = 0;
    }
    countdown = work->field_8;
    if (countdown >= 0) {
        if (countdown == 0) {
            Tmd_FreeBuffers(obj);
            countdown = work->field_8;
        }
        work->field_8 = countdown - 1;
    }
}

void func_actor_511000_80133220(Task* task)
{
    taskKill(task);
}

/// Inherits the parent model's light/color and visibility bit, chains this
/// actor's root coordinate under the parent's, places it at the spawnArg1
/// translation, and reparents the task.
void func_actor_511000_80133240(Task* task)
{
    Task*          parent;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    parentExtra     = (TmdObject*)parent->extra;
    extra           = (TmdObject*)task->extra;
    dest            = parentExtra->coords;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    extra->flags    = 0x80;
    coord           = extra->coords;
    if (!(parentExtra->flags & 0x80)) {
        extra->flags = 0;
    }
    func_actor_511000_80133760(task);
    coord->sub = dest;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// Tracks the parent model's visibility bit every frame: while the parent model
/// is hidden (`field_C` bit 0x80 clear) this clears its own bit and, for
/// spawnArg1 1 or 2, spins the root coordinate's yaw (0x46) by 0x294 or its
/// pitch (0x44) by 0x3E8, wrapping each to 0x1000. The rotation matrix is then
/// rebuilt from the angles and the coordinate's `flg` cleared. With the parent
/// visible the rotation is left alone and the visibility bit is set instead.
void func_actor_511000_801332E4(Task* task)
{
    TmdObject*        extra;
    TmdObject*        parentExtra;
    Actor511000Coord* coord;

    extra       = (TmdObject*)task->extra;
    coord       = (Actor511000Coord*)extra->coords;
    parentExtra = (TmdObject*)((Task*)task->spawnArg2)->extra;

    if (!(parentExtra->flags & 0x80)) {
        extra->flags &= 0xFF7F;

        switch (task->spawnArg1) {
            case 1:
                coord->rot.vy = ((u16)coord->rot.vy + 0x294) & 0xFFF;
                break;
            case 2:
                coord->rot.vx = ((u16)coord->rot.vx + 0x3E8) & 0xFFF;
                break;
        }

        RotMatrix(&coord->rot, &coord->coord);
        coord->flg = 0;
        return;
    }
    extra->flags |= 0x80;
}

void func_actor_511000_801333A4(Task* task)
{
    taskKill(task);
}

/// Inherits the parent model's light/color and visibility bit, chains this
/// actor's root coordinate under the parent's, places it at the spawnArg1
/// translation, copies `D_actor_511000_80147AC4` onto the Euler angles,
/// rebuilds the rotation matrix, and reparents the task.
void func_actor_511000_801333C4(Task* task)
{
    Task*             parent;
    TmdObject*        extra;
    TmdObject*        parentExtra;
    Actor511000Coord* coord;
    GsCOORDINATE2*    dest;

    parent          = (Task*)task->spawnArg2;
    parentExtra     = (TmdObject*)parent->extra;
    extra           = (TmdObject*)task->extra;
    dest            = parentExtra->coords;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    extra->flags    = 0x80;
    coord           = (Actor511000Coord*)extra->coords;
    if (!(parentExtra->flags & 0x80)) {
        extra->flags = 0;
    }
    func_actor_511000_80133760(task);
    ((GsCOORDINATE2*)coord)->sub = dest;
    Task_Reparent(parent, task);
    coord->rot.vx = D_actor_511000_80147AC4[0].vx;
    coord->rot.vy = D_actor_511000_80147AC4[0].vy;
    coord->rot.vz = D_actor_511000_80147AC4[0].vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg   = 0;
    task->state += 1;
}

void func_actor_511000_80133498(Task* task)
{
    taskKill(task);
}

s32 func_actor_511000_801334B8(Task* arg0)
{
    arg0->killCountdown = 0;
    return 0;
}
