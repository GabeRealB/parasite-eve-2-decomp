#include "common.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/scene_runtime.h"

#include "main/areas.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/task_types.h"

enum { LOADING_FILE_ID_RADIX = 100 };

/// Zero entry IDs skip additional loads; a zero file index selects the base resource.
enum { LOADING_AREA_UNUSED_PLACEMENT_ENTRY = 0,
       LOADING_AREA_BASE_FILE_INDEX        = 0 };

/* Define BSS before API headers to preserve first-declaration order. */
s16 gLoadingAreaPlacementFilePhase;

/// Borrowed placement/resource layout shared by the serialized area-load passes.
///
/// Each pass latches the live saved destination's layout at initialization.
/// Its tables and the saved destination must stay valid through both passes;
/// loading neither owns nor releases this storage.
static AreaVariant* _gLoadingAreaVariant;

AreaResource* D_80114C68;

AreaPlacement* Gp_CdRecCur;

u16 gLoadingAreaBaseResourcePhase;

/// Whether the current base resource has a placement with file index zero (0 no, 1 yes).
///
/// Refreshed for each resource. A match supplies texture offsets and permits
/// optional base-60 loads; it is not a completion flag for the whole pass.
static u16 _gLoadingAreaHasBasePlacement;

u16 gLoadingAreaResourcePass;

#include "loading.h"

extern u16 D_8010CAD0[];

/// Retained two-word data with no reader; its purpose and internal format are unproven.
u32 D_8010CAC8[2] = { 0, 0xE1EFCD00 };

/// File-group base selected by each `AreaResource.fileGroupIndex` value.
u16 D_8010CAD0[9] = { 10, 20, 30, 40, 50, 60, 0, 1, 2 };

/// Queues the current placement's global-library file and signed texture relocation.
///
/// Borrows matching live placement/resource cursors; fileIdLow is the nonzero
/// low base-100 component. The resource's file-group selector is 0..8 and its
/// nonnegative file number must resolve to a catalogued global file.
/// The group/hundreds components narrow to bytes. X offsets count 64-word
/// VRAM columns; Y offsets count CLUT rows. Enqueue copies key bytes 3/2/0
/// and all four argument bytes synchronously, retaining no stack addresses;
/// key byte 1 is unused. Queue capacity is the caller's responsibility.
static inline void _loadingQueueCurrentPlacementFile(void)
{
    _LoadingFileKey     fileKey;
    _LoadingFileArgs    fileParams;
    const AreaResource* resource;
    s32                 fileNumber;

    fileKey.stage     = GAME_STAGE_NONE;
    fileKey.fileIndex = Gp_CdRecCur->fileIdLow;
    resource          = D_80114C68;
    fileNumber        = resource->fileNumber;
    if (fileNumber >= LOADING_FILE_ID_RADIX) {
        fileParams.fileIdHundreds = fileNumber % LOADING_FILE_ID_RADIX;
        fileKey.fileGroup         = D_8010CAD0[resource->fileGroupIndex] + (resource->fileNumber / LOADING_FILE_ID_RADIX);
    } else {
        fileParams.fileIdHundreds = resource->fileNumber;
        fileKey.fileGroup         = D_8010CAD0[resource->fileGroupIndex];
    }
    fileParams.loadMode         = CD_COMMAND_LOAD_DEFAULT;
    fileParams.imageXPageOffset = Gp_CdRecCur->texturePageOffset;
    fileParams.imageYOffset     = Gp_CdRecCur->clutRowOffset;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, &fileKey, &fileParams);
}

u16 loadingPollAreaPlacementFiles(void)
{
    AreaVariant* layout;

    switch (gLoadingAreaPlacementFilePhase) {
        case LOADING_AREA_INIT:
            // The loaded destination must resolve before either table is read.
            layout               = areaGetVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
            _gLoadingAreaVariant = layout;
            Gp_CdRecCur          = layout->placements;
            if (layout == NULL) {
                return 1;
            }
            if (D_80114C68 == NULL) {
                return 1;
            }
            if (Gp_CdRecCur == NULL) {
                return 1;
            }
            gLoadingAreaPlacementFilePhase++;
        case LOADING_AREA_QUEUE:
            while (Gp_CdRecCur->entryId != AREA_PLACEMENT_END) {
                if (Gp_CdRecCur->entryId == LOADING_AREA_UNUSED_PLACEMENT_ENTRY) {
                    Gp_CdRecCur++;
                    continue;
                }
                for (D_80114C68 = _gLoadingAreaVariant->resources; D_80114C68->entryId != AREA_PLACEMENT_END; D_80114C68++) {
                    if (Gp_CdRecCur->entryId == D_80114C68->entryId) {
                        break;
                    }
                }
                if (Gp_CdRecCur->fileIdLow == LOADING_AREA_BASE_FILE_INDEX) {
                    Gp_CdRecCur++;
                    continue;
                }
                // Load this placement's additional file with its texture relocation.
                _loadingQueueCurrentPlacementFile();
                gLoadingAreaPlacementFilePhase++;
                break;
            }
            if (Gp_CdRecCur->entryId == AREA_PLACEMENT_END) {
                return 1;
            }
            break;
        case LOADING_AREA_WAIT:
            if (cdCmdIsIdle()) {
                Gp_CdRecCur++;
                gLoadingAreaPlacementFilePhase--;
            }
            break;
    }
    return 0;
}

/// Queues an area's global-library base file with its image relocation offsets.
///
/// `resource` is borrowed for this call; its file-group selector must be 0..8
/// and its nonnegative file number must name a catalogued file. The base file's
/// low index is zero. Offsets retain their low signed byte: X counts 64-word
/// VRAM pages and CLUT Y counts rows. `cdCmdEnqueue` copies the request before
/// returning; its ring-capacity contract applies and its slot result is ignored.
static inline void _loadingQueueAreaResource(const AreaResource* resource, s16 texturePageOffset, s16 clutRowOffset)
{
    u8  fileKey[4];
    u8  commandArgs[4];
    s32 fileNumber;

    // Split the decimal file number between the group and hundreds byte.
    fileKey[3] = 0;
    fileKey[0] = 0;
    fileNumber = resource->fileNumber;
    if (fileNumber >= LOADING_FILE_ID_RADIX) {
        commandArgs[0] = fileNumber % LOADING_FILE_ID_RADIX;
        fileKey[2]     = D_8010CAD0[resource->fileGroupIndex] + (resource->fileNumber / LOADING_FILE_ID_RADIX);
    } else {
        commandArgs[0] = resource->fileNumber;
        fileKey[2]     = D_8010CAD0[resource->fileGroupIndex];
    }
    commandArgs[1] = CD_COMMAND_LOAD_DEFAULT;
    commandArgs[2] = texturePageOffset;
    commandArgs[3] = clutRowOffset;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, commandArgs);
}

u16 loadingPollAreaBaseResources(void)
{
    AreaVariant*  layout;
    AreaResource* resource;
    u16           entryId;

    switch (gLoadingAreaBaseResourcePhase) {
        case LOADING_AREA_INIT:
            // The loaded destination must resolve before either table is read.
            layout               = areaGetVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
            _gLoadingAreaVariant = layout;
            D_80114C68           = layout->resources;
            if (layout == NULL || layout->resources == NULL) {
                return 1;
            }
            gLoadingAreaBaseResourcePhase++;
        case LOADING_AREA_QUEUE:
            if (D_80114C68->entryId == AREA_PLACEMENT_END) {
                return 1;
            }
            do {
                Gp_CdRecCur                   = _gLoadingAreaVariant->placements;
                _gLoadingAreaHasBasePlacement = false;
                if (Gp_CdRecCur->entryId != AREA_PLACEMENT_END) {
                    entryId = D_80114C68->entryId;
                    while (Gp_CdRecCur->entryId != AREA_PLACEMENT_END) {
                        if (Gp_CdRecCur->entryId == entryId && Gp_CdRecCur->fileIdLow == LOADING_AREA_BASE_FILE_INDEX) {
                            _gLoadingAreaHasBasePlacement = true;
                            break;
                        }
                        Gp_CdRecCur++;
                    }
                }
                // Base-60 files are optional unless a base placement names them.
                resource = D_80114C68;
                if (resource->fileGroupIndex != AREA_RESOURCE_FILE_GROUP_BASE_60) {
                    if (_gLoadingAreaHasBasePlacement != 0) {
                        _loadingQueueAreaResource(resource, Gp_CdRecCur->texturePageOffset, Gp_CdRecCur->clutRowOffset);
                    } else {
                        _loadingQueueAreaResource(resource, 0, 0);
                    }
                    gLoadingAreaBaseResourcePhase++;
                    break;
                } else if (_gLoadingAreaHasBasePlacement != 0) {
                    _loadingQueueAreaResource(resource, Gp_CdRecCur->texturePageOffset, Gp_CdRecCur->clutRowOffset);
                    gLoadingAreaBaseResourcePhase++;
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
            if (cdCmdIsIdle()) {
                D_80114C68++;
                gLoadingAreaBaseResourcePhase--;
            }
            break;
    }
    return 0;
}
