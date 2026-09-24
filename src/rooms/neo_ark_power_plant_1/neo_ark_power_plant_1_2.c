#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/neo_ark_power_plant_1.h"
#include "rooms/room_common.h"

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s32 Gp_LcgState;

/// World positions `func_neo_ark_power_plant_1_8017DA18` draws its glows at;
/// the second name is the one emitter it may spawn an effect at instead.
extern SVECTOR D_neo_ark_power_plant_1_8017F020[52];
extern SVECTOR D_neo_ark_power_plant_1_8017F1C0;

void func_neo_ark_power_plant_1_8017E184(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Draws the glows of the current view: a fixed set of emitter positions per
/// view, each with its own size and tint. In views 6 and 7 the extra emitter
/// `D_neo_ark_power_plant_1_8017F1C0` glows while nibble 0x148 is clear;
/// once it is set, and while no event runs and nibble 0xDE is clear, it
/// instead spawns effect 0x600E0 there on one frame in eight at random.
void func_neo_ark_power_plant_1_8017DA18(void)
{
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[30], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[32], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[35], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[36], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[37], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[38], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[39], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[40], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[41], 0x200, 0x344);
            break;
        case 3:
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[51], 0x300, 0x334);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[26], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[27], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[28], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[29], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[30], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[31], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[32], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[33], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[34], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[35], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[36], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[37], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[39], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[41], 0x200, 0x122);
            break;
        case 4:
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[42], 0x300, 0x334);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[45], 0x300, 0x334);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[46], 0x300, 0x334);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[48], 0x300, 0x334);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[49], 0x300, 0x334);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[16], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[17], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[18], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[19], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[20], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[21], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[22], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[23], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[24], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[25], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[26], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[27], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[28], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[29], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[30], 0x200, 0x344);
            break;
        case 5:
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[48], 0x300, 0x334);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[49], 0x300, 0x334);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[12], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[13], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[14], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[15], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[16], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[17], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[18], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[19], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[20], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[21], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[22], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[23], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[24], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[25], 0x200, 0x122);
            break;
        case 6:
            if (GameFlag_GetNibble(0x148) != 0) {
                if (Gp_State1C->eventState == 0 && GameFlag_GetNibble(0xDE) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 7) == 0) {
                        Gp_SpawnEff(0x600E0, NULL, 0x400, &D_neo_ark_power_plant_1_8017F1C0);
                    }
                }
            } else {
                func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F1C0, 0x300, 0x334);
            }
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[42], 0x300, 0x334);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[43], 0x300, 0x334);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[0], 0x200, 0x11);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[1], 0x200, 0x11);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[2], 0x200, 0x11);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[3], 0x200, 0x11);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[4], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[5], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[6], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[7], 0x200, 0x122);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[8], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[9], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[10], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[11], 0x200, 0x233);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[12], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[13], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[14], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[15], 0x200, 0x344);
            break;
        case 7:
            if (GameFlag_GetNibble(0x148) != 0) {
                if (Gp_State1C->eventState == 0 && GameFlag_GetNibble(0xDE) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 7) == 0) {
                        Gp_SpawnEff(0x600E0, NULL, 0x400, &D_neo_ark_power_plant_1_8017F1C0);
                    }
                }
            } else {
                func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F1C0, 0x300, 0x334);
            }
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[0], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[1], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[2], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[3], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[4], 0x200, 0x344);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[5], 0x200, 0x344);
            break;
        case 8:
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[42], 0x300, 0x223);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[43], 0x300, 0x223);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[44], 0x300, 0x223);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[45], 0x300, 0x334);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[46], 0x300, 0x334);
            func_neo_ark_power_plant_1_8017E184(&D_neo_ark_power_plant_1_8017F020[47], 0x300, 0x334);
            break;
    }
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, unless
/// the GTE flags the projection, queues four gouraud `POLY_G4` wedges filling a
/// disc around it, tinted at the centre and black at the rim. `arg1` is the
/// radius in world units, scaled by depth; `arg2` is the tint as three 4-bit
/// channels (red at bit 8, green at bit 4, blue at bit 0), with 8 added to each
/// on odd display frames so the glow flickers. The room draws its steam and
/// spark glows with it.
void func_neo_ark_power_plant_1_8017E184(SVECTOR* arg0, s32 arg1, s32 arg2)
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

/// Sprite-suppression switch for the room's two per-view objects: 0 draws the
/// sprites of both views (both skip-OT-link bytes cleared), 1 leaves both out
/// of the ordering table, and any other value is ignored. The two views hang
/// off the room's sprite-table record at 0x40 and 0x4C.
void func_neo_ark_power_plant_1_8017E524(s32 arg0)
{
    GpAreaKey*                 sess;
    NeoArkPowerPlant1SprtRec*  rec;
    NeoArkPowerPlant1SprtView* view;
    s32                        v;

    sess = &gGameSession->at4.loc;
    rec  = (NeoArkPowerPlant1SprtRec*)Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    v    = arg0 & 0xFF;

    if (v == 0) {
        view          = rec->field_40;
        view->field_C = 0;
        view          = rec->field_4C;
        view->field_C = 0;
        return;
    }
    if (v == 1) {
        view          = rec->field_40;
        view->field_C = v;
        view          = rec->field_4C;
        view->field_C = v;
    }
}
