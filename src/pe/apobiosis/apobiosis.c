#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"

extern s32 Gp_LcgState;

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

INCLUDE_ASM("pe/nonmatchings/apobiosis/apobiosis", func_apobiosis_8012FE10);

INCLUDE_ASM("pe/nonmatchings/apobiosis/apobiosis", func_apobiosis_8013017C);

INCLUDE_ASM("pe/nonmatchings/apobiosis/apobiosis", func_apobiosis_80130630);
