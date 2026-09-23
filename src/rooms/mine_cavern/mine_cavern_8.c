#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>

/// The `inline_c.h` `gte_rtps` lacks the two leading nops this code has.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

/// The gameplay-resident light slot the glow below writes: `mode` becomes 2 and
/// `data.light` takes the glow's world position and a randomised intensity.
/// Actor overlays declare their own views of the same object.
typedef struct {
    s32 mode;
    union {
        GsCOORDINATE2 coord;
        GpObj44       light;
    } data;
} _MineCavernLight;

extern _MineCavernLight D_80114FF8;
extern s32              D_80070F70;
extern u32              Gp_LcgState;

/// Glow at a coordinate: two camera-facing textured quads, an inner one of
/// half-extent `size` and an outer one of `size * 3 / 2`, plus a `Room_Draw06`
/// mark on the ground under it. Also feeds the gameplay light slot a flickering
/// intensity at the coordinate's position. Draws nothing when the point fails
/// to project.
extern void func_mine_cavern_801804CC(GsCOORDINATE2* coord, s16 size);

/// Sixteen-wedge gouraud ring, declared locally the way
/// `dryfield_motel_balcony_5.c` does.
extern void Room_Draw09(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, u8* arg3);

/// Frame callback for one of the cavern's expanding-ring effects. `Gp_State1C`'s
/// `field_4` gates the whole room-effect family: 1-3 park the effect for the
/// frame and 4 or more tear its work block down, so a task that sees them either
/// returns or releases. Otherwise the effect ticks its lifetime counter, stages
/// `field_24` into an RGB triple, advances the coordinate, and draws the
/// eight-wedge `Room_Draw04` ring at twice `field_26` plus the cavern's own
/// glow quads at half-extent `field_26`. Once `field_28` reaches 0x19 the
/// two ramps swap roles - a `Room_Draw09` ring is drawn at `field_2A * 3 / 2` and
/// then `field_28` shrinks by 0x18 and `field_2A` grows by 0x30 - and the effect
/// otherwise fades `field_24` by 0x18 a frame until it drops under 0x18 and the
/// work block is handed back with `Gp_ReleaseState1CMem`.
void func_mine_cavern_80180320(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    u8             sp10[3];
    u16            temp;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->field_22++;
        if (task->state == 0) {
            work->field_22 = 1;
            work->field_24 = 0xE0;
            work->field_26 = 0x80;
            work->field_28 = 0xE0;
            work->field_2A = 0x80;
            task->state    = 1;
        }
        Gp_UpdateCoord(coord);
        sp10[0]        = (u8)work->field_24;
        sp10[1]        = (u8)(work->field_24 >> 1);
        sp10[2]        = (u8)(work->field_24 >> 2);
        temp           = work->field_26 + 0x10;
        work->field_26 = temp;
        Room_Draw04(coord, (s16)(temp * 2), sp10);
        func_mine_cavern_801804CC(coord, (s16)work->field_26);
        if ((s16)work->field_28 >= 0x19) {
            u32 temp_a1;
            sp10[0] = (u8)work->field_28;
            sp10[1] = (u8)(work->field_28 >> 1);
            sp10[2] = (u8)(work->field_28 >> 2);
            temp_a1 = (s16)work->field_2A * 3;
            Room_Draw09(coord, (s32)((temp_a1 + (temp_a1 >> 0x1F)) << 0xF) >> 0x10, 0x60, sp10);
            work->field_28 -= 0x18;
            work->field_2A += 0x30;
            return;
        }
        temp           = work->field_24 - 0x18;
        work->field_24 = temp;
        if ((s16)temp < 0x18) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

void func_mine_cavern_801804CC(GsCOORDINATE2* coord, s16 size)
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
            Room_Draw06(&ground, outerSize);
        }
    }
    *(void**)G_SCRATCH_HEAD =
        (u8*)*(void**)G_SCRATCH_HEAD + sizeof(GpRingScratch);
}
