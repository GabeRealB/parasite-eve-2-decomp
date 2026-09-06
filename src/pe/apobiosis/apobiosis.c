#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/apobiosis.h"

extern s8  D_80114C0B;
extern s32 Gp_LcgState;

void func_apobiosis_8013017C(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_apobiosis_80130630(GsCOORDINATE2* arg0, s16* arg1, s16 arg2, s16 arg3);

INCLUDE_RODATA("pe/nonmatchings/apobiosis/apobiosis", D_apobiosis_8012EF30);

INCLUDE_ASM("pe/nonmatchings/apobiosis/apobiosis", func_apobiosis_8012EF4C);

/// Flashes a screen-filling `POLY_F4` over the whole 320x240 frame, offset by
/// `Display_State.vramYOffset` so it tracks the active draw buffer. `bright`
/// is the flash level: normally the quad is blue-tinted (red and green
/// halved), but on stage `Gp_StateC08.field_0 % 10 == 3` one draw in four
/// comes out yellow instead (blue halved). The prim is linked at a fixed
/// `otz` of 0x30, in front of the scene.
void func_apobiosis_8012F808(u32 bright)
{
    POLY_F4* prim;
    s32      rng;
    s32      kind;
    u16      level;

    prim           = (POLY_F4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setPolyF4(prim);
    kind  = (u16)(Gp_StateC08.field_0 % 10U) - 1;
    level = bright;
    if (kind == 2) {
        SOFT_TOUCH_REG(level);
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        if ((((u32)rng >> 16) & 3) == 0) {
            setRGB0(prim, level, level, bright >> 1);
        } else {
            setRGB0(prim, (s16)level >> 1, (s16)level >> 1, level);
        }
    } else {
        SOFT_TOUCH_REG(level);
        setRGB0(prim, (s16)level >> 1, (s16)level >> 1, level);
    }
    setXY4(prim, -0xA0, -0x78 - Display_State.vramYOffset, 0xA0,
           -0x78 - Display_State.vramYOffset, -0xA0, 0x78 - Display_State.vramYOffset,
           0xA0, 0x78 - Display_State.vramYOffset);
    addPrim((u_long*)((((u32)(0x30 << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
            prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, 0x30);
}

INCLUDE_ASM("pe/nonmatchings/apobiosis/apobiosis", func_apobiosis_8012F9D0);

/// One shard of the apobiosis burst. Every frame it ticks the shard's life
/// counter `GpEffWork.field_22` and bails out - handing the work block back -
/// once the player is dying (`D_80114C0B`), the room is fading (`Gp_State1C`)
/// or the shard has outlived its state. State 0 reparents the shard onto the
/// cast task and splits on `spawnArg1`: a non-zero arg pins the shard to the
/// cast's coordinate at the origin (state 1), a zero arg gives it a random
/// drift `field_10`..`field_14` and lets it fly (state 2). Either way the tail
/// seeds the shard's `field_18`..`field_1C` offset, its radius `field_26` and
/// the intensity `field_2A` that picks a `D_apobiosis_80130B5C` row. Both live
/// states redraw the shard every other frame, at twice the row's radius while
/// pinned and at the plain radius once free.
void func_apobiosis_8012FE10(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((D_80114C0B != -2) && (Gp_State1C->field_E < 4)) {
        mem->field_22 = (u16)mem->field_22 + 1;
        switch (arg0->state) {
            case 0:
                Task_Reparent(D_apobiosis_80130BA0, arg0);
                if (arg0->spawnArg1 != 0) {
                    coord->sub        = mem->field_8;
                    coord->coord.t[0] = 0;
                    coord->coord.t[1] = 0;
                    coord->coord.t[2] = 0;
                    coord->flg        = 0;
                    Gp_UpdateCoord(coord);
                    arg0->state = 1;
                } else {
                    mem->field_12 = 0;
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_10 = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                    Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                    mem->field_14 = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                    arg0->state   = 2;
                }
                mem->field_1A = -0x1000;
                mem->field_24 = 0x80;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_18 = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_1C = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
                mem->field_2A = Gp_StateC08.field_0 % 10 - 1;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                if (mem->field_22 & 1) {
                    mem->field_20 = (u16)mem->field_20 + 1;
                    func_apobiosis_8013017C(coord, mem->field_20,
                                            D_apobiosis_80130B5C[mem->field_2A].field_6 * 2,
                                            mem->field_26);
                    func_apobiosis_80130630(coord, &mem->field_18, mem->field_20,
                                            D_apobiosis_80130B5C[mem->field_2A].field_6 * 2);
                }
                if (mem->field_22 < 0x19) {
                    return;
                }
                break;
            case 2:
                coord->coord.t[0] += mem->field_10;
                coord->coord.t[1] += mem->field_12;
                coord->coord.t[2] += mem->field_14;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (mem->field_22 & 1) {
                    mem->field_20 = (u16)mem->field_20 + 1;
                    func_apobiosis_8013017C(coord, mem->field_20,
                                            D_apobiosis_80130B5C[mem->field_2A].field_6,
                                            mem->field_26);
                    func_apobiosis_80130630(coord, &mem->field_18, mem->field_20,
                                            D_apobiosis_80130B5C[mem->field_2A].field_6);
                }
                if (mem->field_22 < 0x11) {
                    return;
                }
                break;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("pe/nonmatchings/apobiosis/apobiosis", func_apobiosis_8013017C);

INCLUDE_ASM("pe/nonmatchings/apobiosis/apobiosis", func_apobiosis_80130630);
