#include "common.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_400500.h"

/* `D_800678F0` selects the model stream a following `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`.
 *
 * Storing to a bare `extern` pointer next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the `TmdObject` loads that
 * follow. Declared as a scalar, `func_actor_400500_80134B88` scores 87.27%
 * (12 register and 8 reorder penalties); as a one-element array it is exact,
 * the same remedy `actor_400600` needed for the same global. */
extern void* D_800678F0[1];

/* Model streams in the overlay's own `.data`, selected through `D_800678F0`. */
extern u8 D_actor_400500_8014393C[];
extern u8 D_actor_400500_80143F40[];
extern u8 D_actor_400500_80144624[];

void func_8009EA50(s32 arg0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132000);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013226C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132438);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132628);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132AB0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132C54);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132D74);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80132E94);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80133160);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80133358);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80133460);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801335E8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80133B14);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013403C);

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013456C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801348D8);

void func_actor_400500_80134B88(Task* arg0)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    GpEffWork* eff3;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* dst3;
    TmdObject* src;
    TmdObject* src2;
    TmdObject* src3;

    D_800678F0[0] = D_actor_400500_8014393C;
    eff           = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[3], 0x200, NULL);
    if (eff != NULL) {
        src           = (TmdObject*)arg0->extra;
        dst           = (TmdObject*)eff->field_0->extra;
        dst->field_24 = src->field_24;
        dst->field_25 = src->field_25;
        if (dst->field_18 != NULL) {
            Tmd_ProcessStream(dst);
            Tmd_ProcessStream(dst);
        }
    }
    D_800678F0[0] = D_actor_400500_80143F40;
    eff2          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[1], 0x200, NULL);
    if (eff2 != NULL) {
        src2           = (TmdObject*)arg0->extra;
        dst2           = (TmdObject*)eff2->field_0->extra;
        dst2->field_24 = src2->field_24;
        dst2->field_25 = src2->field_25;
        if (dst2->field_18 != NULL) {
            Tmd_ProcessStream(dst2);
            Tmd_ProcessStream(dst2);
        }
    }
    D_800678F0[0] = D_actor_400500_80144624;
    eff3          = Gp_SpawnEff(0x20010, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[1], 0x200, NULL);
    if (eff3 != NULL) {
        src3           = (TmdObject*)arg0->extra;
        dst3           = (TmdObject*)eff3->field_0->extra;
        dst3->field_24 = src3->field_24;
        dst3->field_25 = src3->field_25;
        if (dst3->field_18 != NULL) {
            Tmd_ProcessStream(dst3);
            Tmd_ProcessStream(dst3);
        }
    }
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[2], 0x200, NULL);
    Gp_SpawnEff(0x60030, &((GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8)[3], 0x200, NULL);
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80134D6C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80135414);

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131E4C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80135770);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80135EBC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801361EC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013662C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80136864);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801369A4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80136B94);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80136D00);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80136EB8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80137034);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801371A0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80137338);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80137478);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801375B8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013771C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138088);

INCLUDE_RODATA("actors/nonmatchings/actor_400500/actor_400500", D_actor_400500_80131EE4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801385D0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801387E8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013899C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138B78);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138CE8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138DC4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80138EA0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013905C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801391B0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801392D8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139448);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_801395D0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013973C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139AC4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139C1C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139D70);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_80139F6C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A0B8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A484);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A5D8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A700);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013A8E4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013AA98);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013ABE4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013AD60);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013AF44);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B228);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B374);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B4A4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B5E0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B720);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013B920);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BA24);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BAA4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BB18);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BBB0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BC9C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BCCC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BD64);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BE50);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BEC4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013BFB0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C018);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C108);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C174);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C218);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C348);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C3C4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C474);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C508);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C578);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C61C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C750);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C7A4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C818);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C908);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013C9D4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013CA38);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013CB0C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013CBD8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013CCDC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013CDA8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013CE9C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013CF68);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D078);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D144);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D210);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D274);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D2D8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D3B8);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D420);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D4F0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D59C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D630);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D6A0);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D744);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D878);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D8CC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D958);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D9DC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013D9F4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DA24);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DA68);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DACC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DAE4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DB64);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DB78);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DBCC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DC4C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DCBC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DCD4);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DD8C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DDEC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DE2C);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DE98);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DEFC);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DF50);

void func_actor_400500_8013DF64(void)
{
}

void func_actor_400500_8013DF6C(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DF74);

INCLUDE_ASM("actors/nonmatchings/actor_400500/actor_400500", func_actor_400500_8013DFE4);
