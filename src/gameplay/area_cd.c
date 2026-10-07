#include "common.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/scene_runtime.h"

#include "main/fs.h"
#include "main/mc.h"
#include "main/task_types.h"

enum { LOADING_AREA_FILE_COMMAND = 0x21,
       LOADING_FILE_ID_RADIX     = 100 };

/* Define BSS before API headers to preserve first-declaration order. */
s16 Gp_AreaCdPhase;

AreaVariant* D_80114C64;

AreaResource* D_80114C68;

AreaPlacement* Gp_CdRecCur;

u16 D_80114C70;

u16 D_80114C72;

u16 D_80114C74;

#include "loading.h"

extern u16 D_8010CAD0[];

/// No identified reader for either word; preserve the unresolved storage boundary.
extern u32 D_8010CAC8[2];

/// No identified reader for either word; preserve the unresolved storage boundary.
u32 D_8010CAC8[2] = { 0, 0xE1EFCD00 };

/// File-group base selected by each `AreaResource.fileGroupIndex` value.
u16 D_8010CAD0[9] = { 10, 20, 30, 40, 50, 60, 0, 1, 2 };

u16 Gp_PollAreaCdLoads(void)
{
    u8            fileKey[8];
    u8            fileParams[8];
    AreaVariant*  layout;
    AreaResource* resource;
    s32           fileNumber;

    switch (Gp_AreaCdPhase) {
        case LOADING_AREA_INIT:
            layout      = areaGetVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
            D_80114C64  = layout;
            Gp_CdRecCur = layout->placements;
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
                for (D_80114C68 = D_80114C64->resources; D_80114C68->entryId != AREA_PLACEMENT_END; D_80114C68++) {
                    if (Gp_CdRecCur->entryId == D_80114C68->entryId) {
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
                fileNumber = resource->fileNumber;
                if (fileNumber >= LOADING_FILE_ID_RADIX) {
                    fileParams[0] = fileNumber % LOADING_FILE_ID_RADIX;
                    fileKey[2]    = D_8010CAD0[resource->fileGroupIndex] + (resource->fileNumber / LOADING_FILE_ID_RADIX);
                } else {
                    fileParams[0] = resource->fileNumber;
                    fileKey[2]    = D_8010CAD0[resource->fileGroupIndex];
                }
                fileParams[1] = 0;
                fileParams[2] = Gp_CdRecCur->texturePageOffset;
                fileParams[3] = Gp_CdRecCur->clutRowOffset;
                cdCmdEnqueue(LOADING_AREA_FILE_COMMAND, fileKey, fileParams);
                Gp_AreaCdPhase++;
                break;
            }
            if (Gp_CdRecCur->entryId == AREA_PLACEMENT_END) {
                return 1;
            }
            break;
        case LOADING_AREA_WAIT:
            if (cdCmdIsIdle() & 0xFFFF) {
                Gp_CdRecCur++;
                Gp_AreaCdPhase--;
            }
            break;
    }
    return 0;
}

/// Queues the file of `resource` for loading with the given texture relocation.
static inline void _areaCdQueueResource(AreaResource* resource, s16 texturePageOffset, s16 clutRowOffset)
{
    u8  fileKey[8];
    u8  fileParams[8];
    s32 fileNumber;

    fileKey[3] = 0;
    fileKey[0] = 0;
    fileNumber = resource->fileNumber;
    if (fileNumber >= LOADING_FILE_ID_RADIX) {
        fileParams[0] = fileNumber % LOADING_FILE_ID_RADIX;
        fileKey[2]    = D_8010CAD0[resource->fileGroupIndex] + (resource->fileNumber / LOADING_FILE_ID_RADIX);
    } else {
        fileParams[0] = resource->fileNumber;
        fileKey[2]    = D_8010CAD0[resource->fileGroupIndex];
    }
    fileParams[1] = 0;
    fileParams[2] = texturePageOffset;
    fileParams[3] = clutRowOffset;
    cdCmdEnqueue(LOADING_AREA_FILE_COMMAND, fileKey, fileParams);
}

u16 func_800AA120(void)
{
    AreaVariant*  layout;
    AreaResource* resource;
    u16           entryId;

    switch (D_80114C70) {
        case LOADING_AREA_INIT:
            layout     = areaGetVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
            D_80114C64 = layout;
            D_80114C68 = layout->resources;
            if (layout == NULL || layout->resources == NULL) {
                return 1;
            }
            D_80114C70++;
        case LOADING_AREA_QUEUE:
            if (D_80114C68->entryId == AREA_PLACEMENT_END) {
                return 1;
            }
            do {
                Gp_CdRecCur = D_80114C64->placements;
                D_80114C72  = 0;
                if (Gp_CdRecCur->entryId != AREA_PLACEMENT_END) {
                    entryId = D_80114C68->entryId;
                    while (Gp_CdRecCur->entryId != AREA_PLACEMENT_END) {
                        if (Gp_CdRecCur->entryId == entryId && Gp_CdRecCur->fileIdLow == 0) {
                            D_80114C72 = 1;
                            break;
                        }
                        Gp_CdRecCur++;
                    }
                }
                resource = D_80114C68;
                if (resource->fileGroupIndex != AREA_RESOURCE_FILE_GROUP_BASE_60) {
                    if (D_80114C72 != 0) {
                        _areaCdQueueResource(resource, Gp_CdRecCur->texturePageOffset, Gp_CdRecCur->clutRowOffset);
                    } else {
                        _areaCdQueueResource(resource, 0, 0);
                    }
                    D_80114C70++;
                    break;
                } else if (D_80114C72 != 0) {
                    _areaCdQueueResource(resource, Gp_CdRecCur->texturePageOffset, Gp_CdRecCur->clutRowOffset);
                    D_80114C70++;
                    break;
                } else {
                    D_80114C68 = resource + 1;
                }
            } while (resource[1].entryId != AREA_PLACEMENT_END);
            if (D_80114C68->entryId == AREA_PLACEMENT_END) {
                return 1;
            }
            break;
        case LOADING_AREA_WAIT:
            if (cdCmdIsIdle() & 0xFFFF) {
                D_80114C68++;
                D_80114C70--;
            }
            break;
    }
    return 0;
}
