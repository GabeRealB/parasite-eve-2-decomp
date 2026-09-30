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

enum { LOADING_AREA_FILE_COMMAND = 0x21,
       LOADING_FILE_ID_RADIX     = 100 };

/* Define BSS before API headers to preserve first-declaration order. */
s16 Gp_AreaCdPhase;

GpCdAreaRec* D_80114C64;

GpCdRec0C* D_80114C68;

AreaPlacement* Gp_CdRecCur;

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
    u8           fileKey[8];
    u8           fileParams[8];
    GpCdAreaRec* layout;
    GpCdRec0C*   resource;
    s32          fileNumber;

    switch (Gp_AreaCdPhase) {
        case LOADING_AREA_INIT:
            layout      = Gp_GetNestedAreaRec(&Mc_SaveData[0].state.location.loc);
            D_80114C64  = layout;
            Gp_CdRecCur = layout->field_0;
            if (layout == NULL) {
                return 1;
            }
            if (D_80114C68 == NULL) {
                return 1;
            }
            if (Gp_CdRecCur == NULL) {
                return 1;
            }
            Gp_AreaCdPhase++;
        case LOADING_AREA_QUEUE:
            while (Gp_CdRecCur->entryId != AREA_PLACEMENT_END) {
                if (Gp_CdRecCur->entryId == 0) {
                    Gp_CdRecCur++;
                    continue;
                }
                for (D_80114C68 = D_80114C64->field_4; D_80114C68->field_0 != AREA_PLACEMENT_END; D_80114C68++) {
                    if (Gp_CdRecCur->entryId == D_80114C68->field_0) {
                        break;
                    }
                }
                if (Gp_CdRecCur->fileIdLow == 0) {
                    Gp_CdRecCur++;
                    continue;
                }
                // Load this placement's additional file with its texture relocation.
                fileKey[3] = 0;
                fileKey[0] = Gp_CdRecCur->fileIdLow;
                resource   = D_80114C68;
                fileNumber = (s16)resource->field_2;
                if (fileNumber >= LOADING_FILE_ID_RADIX) {
                    fileParams[0] = fileNumber % LOADING_FILE_ID_RADIX;
                    fileKey[2]    = D_8010CAD0[resource->field_4].field_0 + ((s16)resource->field_2 / LOADING_FILE_ID_RADIX);
                } else {
                    fileParams[0] = resource->field_2;
                    fileKey[2]    = D_8010CAD0[resource->field_4].field_0;
                }
                fileParams[1] = 0;
                fileParams[2] = Gp_CdRecCur->texturePageOffset;
                fileParams[3] = Gp_CdRecCur->clutRowOffset;
                CdCmd_Enqueue(LOADING_AREA_FILE_COMMAND, fileKey, fileParams);
                Gp_AreaCdPhase++;
                break;
            }
            if (Gp_CdRecCur->entryId == AREA_PLACEMENT_END) {
                return 1;
            }
            break;
        case LOADING_AREA_WAIT:
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
    u8           fileKey[8];
    u8           fileParams[8];
    GpCdAreaRec* layout;
    GpCdRec0C*   resource;
    u16          entryId;
    s32          fileNumber;
    s16          texturePageOffset;
    s16          clutRowOffset;

    switch (D_80114C70) {
        case LOADING_AREA_INIT:
            layout     = Gp_GetNestedAreaRec(&Mc_SaveData[0].state.location.loc);
            D_80114C64 = layout;
            D_80114C68 = layout->field_4;
            if (layout == NULL) {
                goto finished;
            }
            if (layout->field_4 == NULL) {
                return 1;
            }
            D_80114C70++;
        case LOADING_AREA_QUEUE:
            if (D_80114C68->field_0 == AREA_PLACEMENT_END) {
                return 1;
            }
            do {
                Gp_CdRecCur = D_80114C64->field_0;
                D_80114C72  = 0;
                if (Gp_CdRecCur->entryId != AREA_PLACEMENT_END) {
                    entryId = D_80114C68->field_0;
                    while (Gp_CdRecCur->entryId != AREA_PLACEMENT_END) {
                        if (Gp_CdRecCur->entryId == entryId && Gp_CdRecCur->fileIdLow == 0) {
                            D_80114C72 = 1;
                            break;
                        }
                        Gp_CdRecCur++;
                    }
                }
                resource = D_80114C68;
                if (resource->field_4 != 5) {
                    if (D_80114C72 != 0) {
                        texturePageOffset = Gp_CdRecCur->texturePageOffset;
                        clutRowOffset     = Gp_CdRecCur->clutRowOffset;
                        fileKey[3]        = 0;
                        fileKey[0]        = 0;
                        fileNumber        = (s16)resource->field_2;
                        if (fileNumber >= LOADING_FILE_ID_RADIX) {
                            fileParams[0] = fileNumber % LOADING_FILE_ID_RADIX;
                            fileKey[2]    = D_8010CAD0[resource->field_4].field_0 + ((s16)resource->field_2 / LOADING_FILE_ID_RADIX);
                        } else {
                            fileParams[0] = resource->field_2;
                            fileKey[2]    = D_8010CAD0[resource->field_4].field_0;
                        }
                        fileParams[1] = 0;
                        fileParams[2] = texturePageOffset;
                        fileParams[3] = clutRowOffset;
                        CdCmd_Enqueue(LOADING_AREA_FILE_COMMAND, fileKey, fileParams);
                        goto queued;
                    } else {
                        texturePageOffset = 0;
                        clutRowOffset     = 0;
                        fileKey[3]        = 0;
                        fileKey[0]        = 0;
                        fileNumber        = (s16)resource->field_2;
                        if (fileNumber >= LOADING_FILE_ID_RADIX) {
                            fileParams[0] = fileNumber % LOADING_FILE_ID_RADIX;
                            fileKey[2]    = D_8010CAD0[resource->field_4].field_0 + ((s16)resource->field_2 / LOADING_FILE_ID_RADIX);
                        } else {
                            fileParams[0] = resource->field_2;
                            fileKey[2]    = D_8010CAD0[resource->field_4].field_0;
                        }
                        fileParams[1] = 0;
                        fileParams[2] = texturePageOffset;
                        fileParams[3] = clutRowOffset;
                        CdCmd_Enqueue(LOADING_AREA_FILE_COMMAND, fileKey, fileParams);
                        goto queued;
                    }
                } else if (D_80114C72 != 0) {
                    texturePageOffset = Gp_CdRecCur->texturePageOffset;
                    clutRowOffset     = Gp_CdRecCur->clutRowOffset;
                    fileKey[3]        = 0;
                    fileKey[0]        = 0;
                    fileNumber        = (s16)resource->field_2;
                    if (fileNumber >= LOADING_FILE_ID_RADIX) {
                        fileParams[0] = fileNumber % LOADING_FILE_ID_RADIX;
                        fileKey[2]    = D_8010CAD0[resource->field_4].field_0 + ((s16)resource->field_2 / LOADING_FILE_ID_RADIX);
                    } else {
                        fileParams[0] = resource->field_2;
                        fileKey[2]    = D_8010CAD0[resource->field_4].field_0;
                    }
                    fileParams[1] = 0;
                    fileParams[2] = texturePageOffset;
                    fileParams[3] = clutRowOffset;
                    CdCmd_Enqueue(LOADING_AREA_FILE_COMMAND, fileKey, fileParams);
                queued:
                    D_80114C70++;
                    break;
                } else {
                    D_80114C68 = resource + 1;
                }
            } while (resource[1].field_0 != AREA_PLACEMENT_END);
            if (D_80114C68->field_0 == AREA_PLACEMENT_END) {
            finished:
                return 1;
            }
            break;
        case LOADING_AREA_WAIT:
            if (CdCmd_IsIdle() & 0xFFFF) {
                D_80114C68++;
                D_80114C70--;
            }
            break;
    }
    return 0;
}
