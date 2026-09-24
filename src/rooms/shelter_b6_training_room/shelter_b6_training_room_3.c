#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

/// The GTE commands with the two leading nops this code has; the `inline_c.h`
/// macros lack them, and its `rtv0` assembles to a different word.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")

/// The gameplay-resident light slot the glow writes: `mode` becomes 2 and
/// `data.light` takes the glow's world position and a randomised intensity.
typedef struct {
    s32 mode;
    union {
        GsCOORDINATE2 coord;
        GpObj44       light;
    } data;
} _ShelterB6TrainingRoomLight;

/// Scratchpad block `func_shelter_b6_training_room_80180530` takes from
/// `G_SCRATCH_HEAD`: the two ground points, their projected depths, screen
/// positions and GTE flags, and the perspective-scaled cap radius at each end.
typedef struct {
    SVECTOR base;
    SVECTOR tip;
    s32     otz0;
    s32     otz1;
    s32     flag;
    s32     r0;
    s32     r1;
    u16     sx0;
    u16     sy0;
    u16     sx1;
    u16     sy1;
} _ShelterB6TrainingRoomBeamScratch;

/// Scratchpad block `func_shelter_b6_training_room_80181FDC` takes from
/// `G_SCRATCH_HEAD`: the two world points the textured strip joins, the first
/// point's projected depth, the GTE flag of the latest projection, the strip's
/// perspective-scaled half-width rotated into screen space, and both points'
/// screen positions.
typedef struct {
    SVECTOR from;
    SVECTOR to;
    s32     otz;
    s32     flag;
    s32     dx;
    s32     dy;
    DVECTOR sxy0;
    DVECTOR sxy1;
} _ShelterB6TrainingRoomRibbonScratch;

/// Scratchpad block `func_shelter_b6_training_room_80181BAC` takes from
/// `G_SCRATCH_HEAD`: the world point the sprite is centred on, its projected
/// depth and GTE flag, the sprite's perspective-scaled half-size rotated into
/// screen space, and the point's screen position.
typedef struct {
    SVECTOR vec;
    s32     otz;
    s32     flag;
    s32     dx;
    s32     dy;
    DVECTOR sxy;
} _ShelterB6TrainingRoomFlashScratch;

/// Scratchpad block `func_shelter_b6_training_room_80181368` takes from
/// `G_SCRATCH_HEAD`: the six world-space points of the band's raised rim and of
/// its ground rim, then the projected depth, GTE flag and packed screen
/// positions of the quad being emitted (`sxy0` for `top[i]`, `sxy1`..`sxy3`
/// for `top[i + 1]`, `base[i]` and `base[i + 1]`).
typedef struct {
    SVECTOR top[6];
    SVECTOR base[6];
    s32     otz;
    s32     flag;
    u32     sxy0;
    u32     sxy1;
    u32     sxy2;
    u32     sxy3;
} _ShelterB6TrainingRoomBandScratch;

/// Per-band offsets `func_shelter_b6_training_room_80181368` adds to the effect
/// work's ring parameters: `radius` to the ground rim's radius, `lift` to the
/// raised rim's height, and `spread` to how much wider the raised rim is.
typedef struct {
    s16 radius;
    s16 lift;
    s16 spread;
} _ShelterB6TrainingRoomBandShape;

extern s32                             D_80070F70;
extern _ShelterB6TrainingRoomLight     D_80114FF8;
extern u32                             Gp_LcgState;
extern GsCOORDINATE2*                  D_shelter_b6_training_room_80185C90;
extern GsCOORDINATE2*                  D_shelter_b6_training_room_80185C94;
extern u16                             D_shelter_b6_training_room_80185C98;
extern SVECTOR                         D_shelter_b6_training_room_80184334[];
extern u16                             D_shelter_b6_training_room_801843FC[];
extern _ShelterB6TrainingRoomBandShape D_shelter_b6_training_room_80184404[];
extern u8                              D_shelter_b6_training_room_80185C60[][16];

void func_shelter_b6_training_room_8017E28C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b6_training_room_8017EAD0(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b6_training_room_8017F014(GsCOORDINATE2* coord, s16 size);
void func_shelter_b6_training_room_8017F540(GsCOORDINATE2* arg0, s32 arg1);
void func_shelter_b6_training_room_80180530(GsCOORDINATE2* from, GsCOORDINATE2* to, s16 size, u16 color);
void func_shelter_b6_training_room_80181368(GpEffWork* mem, GsCOORDINATE2* coord, s32 band);
void func_shelter_b6_training_room_80181BAC(GsCOORDINATE2* coord, s16 arg1, s16 arg2, s16 arg3);
void func_shelter_b6_training_room_80181FDC(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s32 arg2, s16 arg3);

void func_shelter_b6_training_room_8017DDE8(Task* task)
{
    s32 i;
    s32 j;

    if (task->state == 0) {
        D_shelter_b6_training_room_80185C98 = 0;
        for (j = 0; j < 3; j++) {
            for (i = 0; i < 6; i++) {
                D_shelter_b6_training_room_80185C60[task->spawnArg1][i] = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            }
        }
        task->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            func_shelter_b6_training_room_8017E28C(&D_shelter_b6_training_room_80184334[10], 0x180, 0x210);
            break;
        case 3:
            func_shelter_b6_training_room_8017E28C(&D_shelter_b6_training_room_80184334[10], 0x180, 0x210);
            func_shelter_b6_training_room_8017E28C(&D_shelter_b6_training_room_80184334[8], 0x180, 0x210);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[12], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[13], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[14], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[15], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[16], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[18], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[19], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[20], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[21], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[22], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 4:
            func_shelter_b6_training_room_8017E28C(&D_shelter_b6_training_room_80184334[8], 0x180, 0x210);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[12], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[13], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[14], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[18], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[19], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[20], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 5:
            func_shelter_b6_training_room_8017E28C(&D_shelter_b6_training_room_80184334[8], 0x180, 0x210);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 6:
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[0], 0x180, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[1], 0x200, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
        case 7:
            func_shelter_b6_training_room_8017E28C(&D_shelter_b6_training_room_80184334[10], 0x180, 0x210);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[17], 0x280, 0x444);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[23], 0x280, 0x444);
            break;
        case 8:
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[2], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[3], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[4], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[5], 0x100, 0x44);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[6], 0x180, 0x600);
            func_shelter_b6_training_room_8017EAD0(&D_shelter_b6_training_room_80184334[7], 0x200, 0x44);
            break;
    }
}

/// Draws a glowing capsule between the world points `arg0[0]` and `arg0[1]`:
/// both are projected through `Gfx_ViewWorldMtx` and, for each 0x400 step of
/// the half turn around the screen-space angle between them, three `POLY_G4`s
/// are queued - a wedge at each end and a band joining them. `arg1` is the
/// half-width, scaled by 64 over each end's depth; `arg2` packs the lit
/// vertices' colour as three 4-bit channels, OR'd with the frame-counter bit.
void func_shelter_b6_training_room_8017E28C(SVECTOR* arg0, s32 arg1, s32 arg2)
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
    gte_rtps_real();
    gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx0);
    gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps_real();
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

/// Draws a glowing disc at the world point `arg0`: projected through
/// `Gfx_ViewWorldMtx`, it becomes four `POLY_G4` wedges around the screen
/// position. `arg1` is the radius, scaled by 64 over the depth; `arg2` packs
/// the centre vertex's colour as three 4-bit channels, OR'd with the
/// frame-counter bit.
void func_shelter_b6_training_room_8017EAD0(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    register u8*       tmp asm("v0");
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (RoomDraw13Scratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1 = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        ang  = 0;
        tmp  = (u8*)&gDisplayState;
        SOFT_TOUCH_REG(tmp);
        ds            = (DisplayState*)tmp;
        blend         = (*(u8*)&ds->animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

void func_shelter_b6_training_room_8017EE70(Task* arg0)
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
            mem->angle  = 0x100;
            mem->period = 0xE0;
            mem->step   = 0x100;
            arg0->state = 1;
        }
        rgb[0]     = mem->scale;
        rgb[1]     = (u16)mem->scale >> 1;
        rgb[2]     = (u16)mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        Gp_DrawRing(coord, (s16)(step * 2), rgb);
        func_shelter_b6_training_room_8017F014(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = (u16)mem->period >> 1;
            rgb[2] = (u16)mem->period >> 2;
            Gp_DrawArc(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x80;
            return;
        }
        mem->scale -= 0x10;
        if (mem->scale < 0x10) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

/// Draws a glow at the coordinate: two camera-facing textured squares, an
/// inner one of half-extent `size` and an outer one of `size * 3 / 2` (each
/// scaled by 0x37 over the depth), plus the flat quad of
/// `func_shelter_b6_training_room_8017F540` on the ground beneath it. It also
/// points the gameplay light slot at the coordinate with a randomly flickering
/// intensity. Nothing is drawn when the GTE flags the projection.
void func_shelter_b6_training_room_8017F014(GsCOORDINATE2* coord, s16 size)
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
    GpObj44*       light;
    GpRingScratch* block;
    void**         scratch;
    GpRingScratch* alias;
    u16            vy;
    GpRingScratch* sc;

    D_80114FF8.mode           = 2;
    light                     = &D_80114FF8.data.light;
    light->field_58           = 0x300;
    light->field_5C           = 0x3000;
    random                    = (Gp_LcgState * 5) + 0x71357911;
    intensity                 = ((random >> 0x10) & 0x700) + 0x800;
    light->field_50           = intensity;
    shifted                   = intensity << 0x10;
    light->field_52           = (s16)(shifted >> 0x11);
    light->field_54           = (s16)(shifted >> 0x12);
    light->field_18.vx        = (s32)coord->coord.t[0];
    light->field_18.vy        = (s32)coord->coord.t[1];
    light->field_18.vz        = coord->coord.t[2];
    D_80114FF8.data.coord.flg = 0;
    scratch                   = (void**)G_SCRATCH_HEAD;
    block                     = (GpRingScratch*)*scratch - 1;
    block->vec.vx             = *(u16*)&coord->workm.t[0];
    alias                     = block;
    vy                        = *(u16*)&coord->workm.t[1];
    __asm__("move %0,%1" : "=r"(alias) : "r"(alias), "r"(vy), "r"(alias));
    sc          = alias;
    sc->vec.vy  = vy;
    sc->vec.vz  = *(u16*)&coord->workm.t[2];
    Gp_LcgState = random;
    *scratch    = sc;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec);
    gte_rtps_real();
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
            func_shelter_b6_training_room_8017F540(&ground, outerSize);
        }
    }
    *(void**)G_SCRATCH_HEAD =
        (u8*)*(void**)G_SCRATCH_HEAD + sizeof(GpRingScratch);
}

/// Draws a flat textured quad centred on `arg0`'s translation: the unit quad
/// `D_80111E38`, scaled by `arg1` in its X/Z plane, is rotated into view space,
/// offset by that translation and projected through `GsWSMATRIX`, then queued as one
/// semi-transparent `POLY_FT4` whose texture column alternates with the frame
/// counter.
void func_shelter_b6_training_room_8017F540(GsCOORDINATE2* arg0, s32 arg1)
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
        gte_rtv0_real();
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
    gte_rtps_real();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt_real();
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

void func_shelter_b6_training_room_8017F8B8(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState < 4) {
        work->age++;
        switch (task->state) {
            case 0:
                task->state                         = 1;
                work->scale                         = 0;
                work->angle                         = 0x100;
                D_shelter_b6_training_room_80185C90 = NULL;
                work->step                          = 0x80 / task->spawnArg1;
            case 1:
                if (Gp_State1C->eventState != 0) {
                    rgb[0] = (u16)work->scale >> 1;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = work->scale;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, (s16)((task->spawnArg1 << 5) + 0x300), 0x100, rgb);
                    break;
                }
                work->scale += work->step;
                work->angle += work->step << 3;
                task->spawnArg1--;
                rgb[0] = (u16)work->scale >> 1;
                rgb[1] = (u16)work->scale >> 2;
                rgb[2] = work->scale;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawArc(coord, (s16)((task->spawnArg1 << 5) + 0x300), 0x100, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale                         = 0xFF;
                    task->state                         = 2;
                    work->period                        = 0x600;
                    work->step                          = 0;
                    D_shelter_b6_training_room_80185C90 = coord;
                }
                break;
            case 2:
                if (Gp_State1C->eventState != 0) {
                    rgb[0] = (u16)work->scale >> 1;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = work->scale;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, 0x300, 0x100, rgb);
                    break;
                }
                if (work->scale >= 9) {
                    rgb[0] = work->scale >> 1;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = work->scale;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, 0x300, 0x100, rgb);
                    work->scale -= 8;
                    break;
                }
                task->state = 3;
                break;
            case 3:
                break;
            case 4:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    } else {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Draws a glowing capsule from the anchor coordinate
/// `D_shelter_b6_training_room_80185C90` to `coord`, doing nothing while no
/// anchor is set. Both world positions are projected, and nothing is drawn
/// unless both land on-screen. The capsule is drawn in two passes whose end
/// radii are `size * pass * 64 / otz`, each followed by the matching ground
/// capsule from `func_shelter_b6_training_room_80180530`. The fill colour comes
/// from the 4-bit-per-channel palette entry `color`, scaled by 16 and
/// brightened on alternate fields; the outer vertices are black.
void func_shelter_b6_training_room_8017FC40(GsCOORDINATE2* coord, s16 size, u16 color)
{
    void**                             scratch;
    u8*                                head;
    _ShelterB6TrainingRoomBeamScratch* block;
    POLY_G4*                           prim;
    s32                                pass;
    u8                                 r;
    u8                                 g;
    u8                                 b;
    s32                                blend;
    s32                                scaled;
    s32                                limit;
    s32                                angStart;
    s32                                ang;
    s32                                next;
    s32                                mid;
    s32                                tr;
    s32                                tg;

    if (D_shelter_b6_training_room_80185C90 == NULL) {
        return;
    }
    scratch        = (void**)G_SCRATCH_HEAD;
    head           = *scratch;
    block          = (_ShelterB6TrainingRoomBeamScratch*)(*scratch = head - 0x2C);
    block->base.vx = D_shelter_b6_training_room_80185C90->workm.t[0];
    block->base.vy = D_shelter_b6_training_room_80185C90->workm.t[1];
    block->base.vz = D_shelter_b6_training_room_80185C90->workm.t[2];
    block->tip.vx  = coord->workm.t[0];
    block->tip.vy  = coord->workm.t[1];
    block->tip.vz  = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->base);
    gte_rtps_real();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(&block->tip);
        gte_rtps_real();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        gte_stszotz(&block->otz1);
        if (block->flag >= 0) {
            color = D_shelter_b6_training_room_801843FC[color];
            tr    = ((color >> 8) & 0xF) << 4;
            tg    = ((color >> 4) & 0xF) << 4;
            blend = (*(u8*)&D_80070F70 & 1) << 4;
            r     = tr + blend;
            g     = tg + blend;
            b     = ((color & 0xF) << 4) + blend;
            for (pass = 1; pass < 3; pass++) {
                scaled    = size * (pass << 6);
                block->r0 = scaled / block->otz0;
                block->r1 = scaled / block->otz1;
                ang       = (s16)ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
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
                        prim->x0 = block->sx1 + ((block->r1 * rsin(ang + 0x800)) >> 12);
                        prim->y0 = block->sy1 + ((block->r1 * rcos(ang + 0x800)) >> 12);
                        prim->x1 = block->sx1 + ((block->r1 * rsin(ang + 0xA00)) >> 12);
                        prim->y1 = block->sy1 + ((block->r1 * rcos(ang + 0xA00)) >> 12);
                        prim->x2 = block->sx1;
                        prim->y2 = block->sy1;
                        prim->x3 = block->sx1 + ((block->r1 * rsin(ang + 0xC00)) >> 12);
                        prim->y3 = block->sy1 + ((block->r1 * rcos(ang + 0xC00)) >> 12);
                        addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);

                        prim           = (POLY_G4*)gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                        prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                        prim->x1 = block->sx0 + ((block->r0 * rsin(ang + 0x200)) >> 12);
                        prim->y1 = block->sy0 + ((block->r0 * rcos(ang + 0x200)) >> 12);
                        next     = ang + 0x400;
                        prim->x2 = block->sx0;
                        prim->y2 = block->sy0;
                        prim->x3 = block->sx0 + ((block->r0 * rsin(next)) >> 12);
                        prim->y3 = block->sy0 + ((block->r0 * rcos(next)) >> 12);
                        addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                        prim           = (POLY_G4*)gGpuPrimCursor;
                        mid            = angStart + (ang - angStart) * 2;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, r, g, b);
                        prim->x0 = block->sx0 + ((block->r0 * rsin(mid)) >> 12);
                        prim->y0 = block->sy0 + ((block->r0 * rcos(mid)) >> 12);
                        prim->x1 = block->sx1 + ((block->r1 * rsin(mid)) >> 12);
                        prim->y1 = block->sy1 + ((block->r1 * rcos(mid)) >> 12);
                        prim->x2 = block->sx0;
                        prim->y2 = block->sy0;
                        prim->x3 = block->sx1;
                        prim->y3 = block->sy1;
                        addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                        ang = next;
                    } while (ang < limit);
                }
                func_shelter_b6_training_room_80180530(D_shelter_b6_training_room_80185C90, coord, size, color);
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x2C;
}

/// Draws a glowing capsule between the ground points under `from` and `to`.
/// Both coordinates are traced down to the floor, and nothing is drawn unless
/// both traces land and both points project on-screen. Each end gets a
/// half-disc of radius `size * 64 / otz`, built from two gouraud quarter
/// fans, and one quad per half joins the two discs. The fill colour comes from
/// the 4-bit-per-channel palette entry `color`, doubled and brightened on
/// alternate fields; the outer vertices are black, so the glow fades outward.
void func_shelter_b6_training_room_80180530(GsCOORDINATE2* from, GsCOORDINATE2* to, s16 size, u16 color)
{
    GsCOORDINATE2                      c0;
    GsCOORDINATE2                      c1;
    void**                             scratch;
    u8*                                head;
    _ShelterB6TrainingRoomBeamScratch* block;
    POLY_G4*                           prim;
    u8                                 r;
    u8                                 g;
    u8                                 b;
    s32                                blend;
    s32                                scaled;
    s32                                limit;
    s32                                angStart;
    s32                                ang;
    s32                                next;
    s32                                mid;
    DisplayState*                      ds;

    if (Gp_TraceGroundCoord(from, &c0) != 1 || Gp_TraceGroundCoord(to, &c1) != 1) {
        return;
    }
    scratch        = (void**)G_SCRATCH_HEAD;
    head           = *scratch;
    block          = (_ShelterB6TrainingRoomBeamScratch*)(*scratch = head - 0x2C);
    block->base.vx = c0.workm.t[0];
    block->base.vy = c0.workm.t[1];
    block->base.vz = c0.workm.t[2];
    block->tip.vx  = c1.workm.t[0];
    block->tip.vy  = c1.workm.t[1];
    block->tip.vz  = c1.workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->base);
    gte_rtps_real();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(&block->tip);
        gte_rtps_real();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        gte_stszotz(&block->otz1);
        if (block->flag >= 0) {
            color     = D_shelter_b6_training_room_801843FC[color];
            r         = ((color >> 8) & 0xF) << 1;
            g         = ((color >> 4) & 0xF) << 1;
            b         = (color & 0xF) << 1;
            ds        = &gDisplayState;
            blend     = (*(u8*)&ds->animFrame & 1) << 1;
            r        += blend;
            g        += blend;
            b        += blend;
            scaled    = size << 6;
            block->r0 = scaled / block->otz0;
            block->r1 = scaled / block->otz1;
            ang       = (s16)ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
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
                    prim->x0 = block->sx1 + ((block->r1 * rsin(ang + 0x800)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(ang + 0x800)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(ang + 0xA00)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(ang + 0xA00)) >> 12);
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(ang + 0xC00)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(ang + 0xC00)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);

                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(ang + 0x200)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(ang + 0x200)) >> 12);
                    next     = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(next)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(next)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim           = (POLY_G4*)gGpuPrimCursor;
                    mid            = angStart + (ang - angStart) * 2;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(mid)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(mid)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(mid)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(mid)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = next;
                } while (ang < limit);
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x2C;
}

void func_shelter_b6_training_room_80180DB4(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState < 4) {
        work->age++;
        switch (task->state) {
            case 0:
                rot         = (GpMtxWords*)&coord->coord;
                rot->w0     = 0x1000;
                rot->w1     = 0;
                rot->w2     = 0x1000;
                rot->w3     = 0;
                rot->h4     = 0x1000;
                coord->flg  = 0;
                work->scale = 0;
                work->angle = 0x100;
                work->step  = 0xC0 / task->spawnArg1;
                if (work->step == 0) {
                    work->step = 1;
                }
                task->state = 1;
            case 1:
                if (Gp_State1C->eventState != 0) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 1;
                    rgb[2] = (u16)work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, (s16)((task->spawnArg1 % 15) * (work->scale << 2)), 0x100, rgb);
                    return;
                }
                work->scale += work->step;
                if (work->scale > 0xC0) {
                    work->scale = 0xC0;
                }
                work->angle = (u16)work->scale * 8 + 0x100;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = (u16)work->scale >> 1;
                rgb[2] = (u16)work->scale >> 2;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawArc(coord, (s16)((task->spawnArg1 % 15) * (work->scale << 2)), 0x100, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    Gp_SpawnEff(0x601AA, coord, 0, NULL);
                    Gp_SpawnEff(0x601AA, coord, 1, NULL);
                    Gp_SpawnEff(0x601AA, coord, 2, NULL);
                    work->period = 0x600;
                    work->step   = 0;
                }
                return;
            case 2:
                if (work->scale >= 5) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 1;
                    rgb[2] = (u16)work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    if (Gp_State1C->eventState == 0) {
                        work->scale -= 4;
                    }
                    Gp_DrawFadeQuad(rgb, 1);
                    return;
                }
                break;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

void func_shelter_b6_training_room_801811AC(Task* task)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;

    mem   = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1);
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }
    mem->age++;
    switch (task->state) {
        case 0:
            mem->scale        = 0x80;
            task->state       = task->spawnArg1 + 1;
            coord->coord.t[1] = 0;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            return;
        case 1:
            if (mem->scale < 5) {
                goto release;
            }
            if (mem->period < 0xC00) {
                mem->period += 0xC0;
            } else {
                mem->scale -= 4;
            }
            mem->angle += 0x20;
            mem->step  += 0x18;
            func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1);
            return;
        case 2:
            if (mem->scale < 4) {
                goto release;
            }
            mem->scale -= 3;
            mem->angle += 0x40;
            mem->step  += 0x18;
            func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1);
            return;
        case 3:
            if (mem->scale < 5) {
                goto release;
            }
            mem->scale -= 4;
            mem->angle += 0x180;
            mem->step  += 0x18;
            func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1);
            return;
        case 4:
        release:
            Gp_ReleaseState1CMem(mem, task);
        default:
            return;
    }
}

/// Draws band `band` of a six-sided textured ring around `coord`: six
/// `POLY_FT4` quads joining a ground rim of radius `angle + radius` to a rim
/// raised by `period + lift` and widened by `step + spread`. Both rims are
/// rotated by the coordinate's `workm`, translated by its `t[]` and projected
/// through `GsWSMATRIX`. Quad `i` takes its texture column from
/// `(D_shelter_b6_training_room_80185C60[band][i] + age) % 6`, so each quad
/// animates on its own phase, and `scale` sets its brightness.
void func_shelter_b6_training_room_80181368(GpEffWork* mem, GsCOORDINATE2* coord, s32 band)
{
    void**                             scratch;
    u8*                                head;
    _ShelterB6TrainingRoomBandScratch* block;
    SVECTOR*                           bp;
    POLY_FT4*                          prim;
    _ShelterB6TrainingRoomBandShape*   shape;
    s32                                i;
    s32                                next;
    s32                                ang;
    s32                                u;
    s16                                frame;
    s16                                rTop;
    s16                                rBase;
    u16                                height;
    u16                                period;

    shape    = &D_shelter_b6_training_room_80184404[band];
    period   = (u16)mem->period;
    rBase    = (u16)mem->angle;
    height   = period + (u16)shape->lift;
    rBase   += (u16)shape->radius;
    rTop     = rBase + (u16)mem->step + (u16)shape->spread;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch;
    *scratch = head - 0x78;
    block    = (_ShelterB6TrainingRoomBandScratch*)(head - 0x78);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 6; i++) {
        ang              = i * 0x2AA;
        block->top[i].vx = (rsin(ang) * rTop) >> 12;
        block->top[i].vy = -height;
        block->top[i].vz = (rcos(ang) * rTop) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->top[i]);
        gte_rtv0_real();
        gte_stsv(&block->top[i]);
        block->top[i].vx  = *(u16*)&block->top[i].vx + *(u16*)&coord->workm.t[0];
        block->top[i].vy  = *(u16*)&block->top[i].vy + *(u16*)&coord->workm.t[1];
        block->top[i].vz  = *(u16*)&block->top[i].vz + *(u16*)&coord->workm.t[2];
        block->base[i].vx = (rsin(ang) * rBase) >> 12;
        bp                = &block->top[i] + 6;
        bp->vy            = 0;
        bp->vz            = (rcos(ang) * rBase) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->base[i]);
        gte_rtv0_real();
        gte_stsv(&block->base[i]);
        block->base[i].vx = *(u16*)&block->base[i].vx + *(u16*)&coord->workm.t[0];
        bp->vy            = *(u16*)&bp->vy + *(u16*)&coord->workm.t[1];
        bp->vz            = *(u16*)&bp->vz + *(u16*)&coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 6; i++) {
        gte_ldv0(&block->top[i]);
        gte_rtps_real();
        gte_stsxy(&block->sxy0);
        next = i + 1;
        gte_ldv3(&block->top[next % 6], &block->base[i], &block->base[next % 6]);
        gte_rtpt_real();
        frame = ((s8)D_shelter_b6_training_room_80185C60[band][i] + mem->age) % 6;
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            prim->code = 0x2E;
            setRGB0(prim, mem->scale, mem->scale, mem->scale);
            prim->tpage = 0x2A;
            prim->clut  = 0x4282;
            u           = frame * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = block->sxy0;
            prim->y0 = block->sxy0 >> 16;
            prim->x1 = block->sxy1;
            prim->y1 = block->sxy1 >> 16;
            prim->x2 = block->sxy2;
            prim->y2 = block->sxy2 >> 16;
            prim->x3 = block->sxy3;
            prim->y3 = block->sxy3 >> 16;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x78;
}

void func_shelter_b6_training_room_80181930(Task* task)
{
    GsCOORDINATE2* coord;
    u8             rgb[3];
    u32            shade;

    coord = ((TmdObject*)task->extra)->coords + 1;
    if (Gp_State1C->eventState == 0) {
        D_shelter_b6_training_room_80185C94 = coord;
        shade                               = ((D_80070F70 & 1) << 4) + 0x40;
        rgb[0]                              = shade;
        rgb[1]                              = shade;
        rgb[2]                              = shade >> 1;
        Gp_DrawRing(coord, 0x200, rgb);
        Gp_DrawRing(coord, 0x400, rgb);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (((Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x600E0, ((TmdObject*)task->extra)->coords + (((Gp_LcgState >> 16) & 0xF) + 3), 0x10080, NULL);
        }
    }
}

void func_shelter_b6_training_room_80181A3C(Task* task)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;

    mem   = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState == 0) {
        mem->age++;
        coord->flg = 0;
        if (task->state == 0) {
            GpMtxWords* rot;
            u32         first;

            rot               = (GpMtxWords*)&coord->coord;
            coord->sub        = mem->parent;
            rot->w0           = 0x1000;
            rot->w1           = 0;
            rot->w2           = 0x1000;
            rot->w3           = 0;
            rot->h4           = 0x1000;
            coord->coord.t[0] = mem->pos.vx;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            first             = Gp_LcgState;
            coord->coord.t[1] = mem->pos.vy;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            coord->coord.t[2] = mem->pos.vz;
            coord->flg        = 0;
            mem->scale        = ((first >> 16) & 0x1FF) + 0x100;
            mem->angle        = (Gp_LcgState >> 16) & 0xFFF;
            mem->period       = ((Gp_LcgState >> 16) & 0xF) + 6;
            task->state       = 1;
        }
        func_shelter_b6_training_room_80181BAC(coord, mem->age, mem->scale, mem->angle);
        if (mem->age & 1) {
            func_shelter_b6_training_room_80181FDC(coord, D_shelter_b6_training_room_80185C94, mem->age >> 1, mem->scale);
        }
        if (mem->age > mem->period) {
            Gp_ReleaseState1CMem(mem, task);
        }
    }
}

/// Draws one spinning sprite frame: a semi-transparent `POLY_FT4` centred on
/// the coordinate's world position, projected by a single `RTPS`. Its half-size
/// is `arg2 * 39 / otz`, and the four corners are that half-size swung to
/// `arg3` and to `arg3 + 0x400`. `arg1` picks one of six 40-pixel-wide frames
/// from the texture page. Nothing is drawn if the point fails the GTE flag test.
void func_shelter_b6_training_room_80181BAC(GsCOORDINATE2* coord, s16 arg1, s16 arg2, s16 arg3)
{
    void**                              scratch;
    u8*                                 head;
    _ShelterB6TrainingRoomFlashScratch* block;
    _ShelterB6TrainingRoomFlashScratch* vecp;
    POLY_FT4*                           prim;
    s16                                 u;
    u16                                 vz;

    scratch                                                      = (void**)G_SCRATCH_HEAD;
    head                                                         = *scratch;
    ((_ShelterB6TrainingRoomFlashScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                                        = (_ShelterB6TrainingRoomFlashScratch*)(head - 0x1C);
    block->vec.vy                                                = *(u16*)&coord->workm.t[1];
    vz                                                           = *(u16*)&coord->workm.t[2];
    *scratch                                                     = block;
    block->vec.vz                                                = vz;
    vecp                                                         = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    gte_stsxy(&((_ShelterB6TrainingRoomFlashScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((_ShelterB6TrainingRoomFlashScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((_ShelterB6TrainingRoomFlashScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2A;
        prim->clut  = 0x42C9;
        prim->v0    = 0x38;
        prim->v1    = 0x38;
        prim->v2    = 0x5F;
        prim->v3    = 0x5F;
        u           = arg1 % 6;
        prim->u0    = u * 40;
        prim->u1    = u * 40 + 0x27;
        prim->u2    = u * 40;
        prim->u3    = u * 40 + 0x27;
        block->dx   = (((arg2 * 39) / block->otz) * rsin(arg3)) >> 12;
        block->dy   = (((arg2 * 39) / block->otz) * rcos(arg3)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        block->dx   = (((arg2 * 39) / block->otz) * rsin(arg3 + 0x400)) >> 12;
        block->dy   = (((arg2 * 39) / block->otz) * rcos(arg3 + 0x400)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Draws a textured `POLY_FT4` strip between the world positions of two
/// coordinates. Both ends are projected and the strip is dropped if either
/// fails the GTE flag test. Its half-width is `arg3 * 23 / otz`, laid
/// perpendicular to the screen-space line between the ends, and `arg2`
/// selects one of four 128x24 texture frames. The primitive is queued at the
/// first end's depth.
void func_shelter_b6_training_room_80181FDC(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s32 arg2, s16 arg3)
{
    void**                               scratch;
    u8*                                  head;
    _ShelterB6TrainingRoomRibbonScratch* block;
    _ShelterB6TrainingRoomRibbonScratch* vecp;
    POLY_FT4*                            prim;
    s16                                  ang;
    u16                                  vz;

    scratch                                                        = (void**)G_SCRATCH_HEAD;
    head                                                           = *scratch;
    ((_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28))->from.vx = *(u16*)&arg0->workm.t[0];
    block                                                          = (_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28);
    block->from.vy                                                 = *(u16*)&arg0->workm.t[1];
    block->from.vz                                                 = *(u16*)&arg0->workm.t[2];
    block->to.vx                                                   = *(u16*)&arg1->workm.t[0];
    block->to.vy                                                   = *(u16*)&arg1->workm.t[1];
    vz                                                             = *(u16*)&arg1->workm.t[2];
    *scratch                                                       = block;
    block->to.vz                                                   = vz;
    vecp                                                           = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->from);
    gte_rtps_real();
    gte_stsxy(&((_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28))->sxy0);
    gte_stflg(&((_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28))->otz);
        gte_ldv0(&((_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28))->to);
        gte_rtps_real();
        gte_stsxy(&((_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28))->sxy1);
        gte_stflg(&((_ShelterB6TrainingRoomRibbonScratch*)(head - 0x28))->flag);
        if (block->flag >= 0) {
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            prim->clut  = 0x42C8;
            prim->u0    = (arg2 & 1) << 7;
            prim->v0    = ((u32)(arg2 & 3) >> 1) * 24 - 0x30;
            prim->u1    = ((arg2 & 1) << 7) + 0x7F;
            prim->v1    = ((u32)(arg2 & 3) >> 1) * 24 - 0x30;
            prim->u2    = (arg2 & 1) << 7;
            prim->v2    = ((u32)(arg2 & 3) >> 1) * 24 - 0x19;
            prim->u3    = ((arg2 & 1) << 7) + 0x7F;
            prim->v3    = ((u32)(arg2 & 3) >> 1) * 24 - 0x19;
            ang         = ratan2(block->sxy1.vy - block->sxy0.vy, block->sxy1.vx - block->sxy0.vx);
            block->dx   = (((arg3 * 23) / block->otz) * rsin(ang)) >> 12;
            block->dy   = (((arg3 * 23) / block->otz) * rcos(ang)) >> 12;
            prim->x0    = *(u16*)&block->sxy0.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy1.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy0.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy1.vy + *(u16*)&block->dy;
            block->dx   = (((arg3 * 23) / block->otz) * rsin(ang + 0x400)) >> 12;
            block->dy   = (((arg3 * 23) / block->otz) * rcos(ang + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy1.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy0.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy1.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy0.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x28;
}

void func_shelter_b6_training_room_8018245C(Task* task)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            eventState;
    u8             rgb[3];

    mem        = task->spawnArg2;
    eventState = Gp_State1C->eventState;
    coord      = ((TmdObject*)task->extra)->coords;
    if (eventState != 0) {
        if (eventState < 4) {
            return;
        }
        goto release;
    }
    mem->age++;
    if (task->state == 0) {
        mem->scale = 0xC0;
        mem->angle = 0x200;
        D_shelter_b6_training_room_80185C98++;
        task->state     = 1;
        task->spawnArg1 = D_shelter_b6_training_room_80185C98;
    }
    if (task->spawnArg1 != D_shelter_b6_training_room_80185C98) {
        goto release;
    }
    rgb[0]      = mem->scale;
    rgb[1]      = mem->scale;
    rgb[2]      = (u16)mem->scale >> 1;
    mem->angle += 0x18;
    Gp_DrawArc(coord, (s16)(mem->angle * 2), 0, rgb);
    Gp_DrawRing(coord, (s16)((u16)mem->angle * 4), rgb);
    if (mem->age < 9) {
        return;
    }
    mem->scale -= 0x18;
    if (mem->scale < 0x18) {
    release:
        Gp_ReleaseState1CMem(mem, task);
    }
}

void func_shelter_b6_training_room_801825C0(Task* task)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;

    mem   = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState == 0) {
        mem->age++;
        if (task->state == 0) {
            mem->move.vx = 0;
            mem->move.vy = 8;
            mem->move.vz = 0;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->scale   = ((Gp_LcgState >> 16) & 0xFFF) | 0x1000;
            task->state  = 1;
        }
        coord->coord.t[1] += mem->move.vy;
        coord->flg         = 0;
        if (!(mem->age & 1)) {
            mem->index++;
        }
        if (mem->index < 8) {
            if (mem->age & 1) {
                Gp_DrawFxQuad(coord, mem->index, 0x400, mem->scale);
            }
        } else {
            Gp_ReleaseState1CMem(mem, task);
        }
    }
}

void func_shelter_b6_training_room_801826E0(Task* task)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;

    mem   = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState == 0) {
        mem->age++;
        if (task->state == 0) {
            mem->move.vy = 0x20;
            mem->scale   = 0x80;
            mem->move.vx = 0;
            mem->move.vz = 0;
            task->state  = 1;
        }
        coord->coord.t[1] += mem->move.vy;
        coord->flg         = 0;
        if (mem->age < 60) {
            if (mem->age & 1) {
                mem->index = (mem->index + 1) & 3;
                func_800EB6E8(coord, mem->index, 0x300, 0x80);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if (((Gp_LcgState >> 16) & 3) == 0) {
                    Gp_SpawnEff(0x601AD, coord, 0, NULL);
                }
            }
        } else {
            Gp_ReleaseState1CMem(mem, task);
        }
    }
}

void func_shelter_b6_training_room_80182804(Task* task)
{
    GpEffWork* mem;

    mem = task->spawnArg2;
    if (mem->age >= 0x15) {
        Gp_ReleaseState1CMem(mem, task);
        return;
    }
    if (Gp_State1C->eventState == 0) {
        mem->age++;
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->scale  += ((Gp_LcgState >> 16) & 0x1FF) + 0x200;
        mem->move.vx = D_shelter_b6_training_room_80184334[24].vx + ((rcos(mem->scale) * 1000) >> 12);
        mem->move.vy = D_shelter_b6_training_room_80184334[24].vy - mem->age * 200;
        mem->move.vz = D_shelter_b6_training_room_80184334[24].vz + ((rsin(mem->scale) * 1000) >> 12);
        Gp_SpawnEff(0x601AE, NULL, 0, &mem->move);
    }
}

void func_shelter_b6_training_room_8018294C(Task* task)
{
    if (Gp_State1C->eventState == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (((Gp_LcgState >> 16) & 7) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x601AB, ((TmdObject*)task->extra)->coords + ((u16)((Gp_LcgState >> 16) % 18) + 1), 0, NULL);
        }
    }
}

void func_shelter_b6_training_room_80182A14(s32 arg0, s32 arg1)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtRec* rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    GpSprtCmd* cmd;
    s32        run = arg0 & 0xFF;
    s32        flag;

    if (run == 0) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            cmd            = rec[2].field_4;
            cmd[2].field_4 = 1;
            cmd            = rec[6].field_4;
            cmd[1].field_4 = 1;
            return;
        }
        if (flag == 1) {
            cmd            = rec[2].field_4;
            cmd[2].field_4 = 0;
            cmd            = rec[6].field_4;
            cmd[1].field_4 = 0;
            return;
        }
    } else if (run == 1) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            cmd            = rec[2].field_4;
            cmd[1].field_4 = run;
            cmd            = rec[6].field_4;
            cmd[2].field_4 = run;
            return;
        }
        if (flag == run) {
            cmd            = rec[2].field_4;
            cmd[1].field_4 = 0;
            cmd            = rec[6].field_4;
            cmd[2].field_4 = 0;
        }
    }
}
