#include "common.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/scene_runtime.h"

#include "main/fs.h"
#include "main/mc.h"
#include "main/task_types.h"

/// 2-byte table at `D_8010CAD0`. `Gp_PollAreaCdLoads` reads `field_0` at
/// `GpCdRec0C.field_4` (stride 2) as the CdCmd 0x21 param1[2] base.
typedef struct _GpTbl2 {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
} GpTbl2;
STATIC_ASSERT_SIZEOF(GpTbl2, 2);

/// The loader reads the same entries and layouts as the spawner.
typedef GpAreaTmdRec GpCdRec0C;

typedef GpAreaVariant GpCdAreaRec;

/* Define BSS before API headers to preserve first-declaration order. */
s16 Gp_AreaCdPhase;

GpCdAreaRec* D_80114C64;

GpCdRec0C* D_80114C68;

GpAreaPlace* Gp_CdRecCur;

u16 D_80114C70;

u16 D_80114C72;

u16 D_80114C74;

#include "loading.h"

extern GpTbl2 D_8010CAD0[];

/// No identified reader for either word; preserve the unresolved storage boundary.
extern u32 D_8010CAC8[2];

/// No identified reader for either word; preserve the unresolved storage boundary.
u32    D_8010CAC8[2] = { 0, 0xE1EFCD00 };
GpTbl2 D_8010CAD0[9] = { { 10, 0 }, { 20, 0 }, { 30, 0 }, { 40, 0 }, { 50, 0 }, { 60, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 } };

u16 Gp_PollAreaCdLoads(void)
{
    u8           param1[8];
    u8           param2[8];
    GpCdAreaRec* rec;
    GpCdRec0C*   rec12;
    s32          val;

    switch (Gp_AreaCdPhase) {
        case 0:
            rec         = Gp_GetNestedAreaRec(&Mc_SaveData[0].state.at4.loc);
            D_80114C64  = rec;
            Gp_CdRecCur = rec->field_0;
            if (rec == NULL) {
                return 1;
            }
            if (D_80114C68 == NULL) {
                return 1;
            }
            if (Gp_CdRecCur == NULL) {
                return 1;
            }
            Gp_AreaCdPhase++;
        case 1:
            while (Gp_CdRecCur->entryId != AREA_TABLE_END_ID) {
                if (Gp_CdRecCur->entryId == 0) {
                    Gp_CdRecCur++;
                    continue;
                }
                for (D_80114C68 = D_80114C64->field_4; D_80114C68->field_0 != AREA_TABLE_END_ID; D_80114C68++) {
                    if (Gp_CdRecCur->entryId == D_80114C68->field_0) {
                        break;
                    }
                }
                if (Gp_CdRecCur->pad_C == 0) {
                    Gp_CdRecCur++;
                    continue;
                }
                param1[3] = 0;
                param1[0] = Gp_CdRecCur->pad_C;
                rec12     = D_80114C68;
                val       = (s16)rec12->field_2;
                if (val >= 0x64) {
                    param2[0] = val % 100;
                    param1[2] = D_8010CAD0[rec12->field_4].field_0 + ((s16)rec12->field_2 / 100);
                } else {
                    param2[0] = rec12->field_2;
                    param1[2] = D_8010CAD0[rec12->field_4].field_0;
                }
                param2[1] = 0;
                param2[2] = Gp_CdRecCur->tpage;
                param2[3] = Gp_CdRecCur->clut;
                CdCmd_Enqueue(0x21, param1, param2);
                Gp_AreaCdPhase++;
                break;
            }
            if (Gp_CdRecCur->entryId == AREA_TABLE_END_ID) {
                return 1;
            }
            break;
        case 2:
            if (CdCmd_IsIdle() & 0xFFFF) {
                Gp_CdRecCur++;
                Gp_AreaCdPhase--;
            }
            break;
    }
    return 0;
}

u16 func_800AA120(void)
{
    u8           param1[8];
    u8           param2[8];
    GpCdAreaRec* rec;
    GpCdRec0C*   rec12;
    u16          key;
    s32          val;
    s16          d;
    s16          e;

    switch (D_80114C70) {
        case 0:
            rec        = Gp_GetNestedAreaRec(&Mc_SaveData[0].state.at4.loc);
            D_80114C64 = rec;
            D_80114C68 = rec->field_4;
            if (rec == NULL) {
                goto finished;
            }
            if (rec->field_4 == NULL) {
                return 1;
            }
            D_80114C70++;
        case 1:
            if (D_80114C68->field_0 == AREA_TABLE_END_ID) {
                return 1;
            }
            do {
                Gp_CdRecCur = D_80114C64->field_0;
                D_80114C72  = 0;
                if (Gp_CdRecCur->entryId != AREA_TABLE_END_ID) {
                    key = D_80114C68->field_0;
                    while (Gp_CdRecCur->entryId != AREA_TABLE_END_ID) {
                        if (Gp_CdRecCur->entryId == key && Gp_CdRecCur->pad_C == 0) {
                            D_80114C72 = 1;
                            break;
                        }
                        Gp_CdRecCur++;
                    }
                }
                rec12 = D_80114C68;
                if (rec12->field_4 != 5) {
                    if (D_80114C72 != 0) {
                        d         = (s8)Gp_CdRecCur->tpage;
                        e         = (s8)Gp_CdRecCur->clut;
                        param1[3] = 0;
                        param1[0] = 0;
                        val       = (s16)rec12->field_2;
                        if (val >= 0x64) {
                            param2[0] = val % 100;
                            param1[2] = D_8010CAD0[rec12->field_4].field_0 + ((s16)rec12->field_2 / 100);
                        } else {
                            param2[0] = rec12->field_2;
                            param1[2] = D_8010CAD0[rec12->field_4].field_0;
                        }
                        param2[1] = 0;
                        param2[2] = d;
                        param2[3] = e;
                        CdCmd_Enqueue(0x21, param1, param2);
                        goto queued;
                    } else {
                        d         = 0;
                        e         = 0;
                        param1[3] = 0;
                        param1[0] = 0;
                        val       = (s16)rec12->field_2;
                        if (val >= 0x64) {
                            param2[0] = val % 100;
                            param1[2] = D_8010CAD0[rec12->field_4].field_0 + ((s16)rec12->field_2 / 100);
                        } else {
                            param2[0] = rec12->field_2;
                            param1[2] = D_8010CAD0[rec12->field_4].field_0;
                        }
                        param2[1] = 0;
                        param2[2] = d;
                        param2[3] = e;
                        CdCmd_Enqueue(0x21, param1, param2);
                        goto queued;
                    }
                } else if (D_80114C72 != 0) {
                    d         = (s8)Gp_CdRecCur->tpage;
                    e         = (s8)Gp_CdRecCur->clut;
                    param1[3] = 0;
                    param1[0] = 0;
                    val       = (s16)rec12->field_2;
                    if (val >= 0x64) {
                        param2[0] = val % 100;
                        param1[2] = D_8010CAD0[rec12->field_4].field_0 + ((s16)rec12->field_2 / 100);
                    } else {
                        param2[0] = rec12->field_2;
                        param1[2] = D_8010CAD0[rec12->field_4].field_0;
                    }
                    param2[1] = 0;
                    param2[2] = d;
                    param2[3] = e;
                    CdCmd_Enqueue(0x21, param1, param2);
                queued:
                    D_80114C70++;
                    break;
                } else {
                    D_80114C68 = rec12 + 1;
                }
            } while (rec12[1].field_0 != AREA_TABLE_END_ID);
            if (D_80114C68->field_0 == AREA_TABLE_END_ID) {
            finished:
                return 1;
            }
            break;
        case 2:
            if (CdCmd_IsIdle() & 0xFFFF) {
                D_80114C68++;
                D_80114C70--;
            }
            break;
    }
    return 0;
}
