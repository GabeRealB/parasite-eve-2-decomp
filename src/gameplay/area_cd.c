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
AreaVariant* _gLoadingAreaVariant;

/// Borrowed resource cursor shared by the serialized base and placement-file passes.
///
/// The base pass advances through the resource list; the placement pass searches
/// it anew for each live entry ID. The list ends at AREA_PLACEMENT_END and must
/// remain loaded with the selected variant until both passes finish.
AreaResource* _gLoadingAreaResourceCursor;

/// Borrowed placement cursor shared by the serialized area-load passes.
///
/// The base pass searches for a matching zero-file-index placement; the additional
/// file pass advances through nonzero file indices. AREA_PLACEMENT_END ends the
/// list, which must remain loaded with the selected variant through both passes.
AreaPlacement* _gLoadingAreaPlacementCursor;

u16 gLoadingAreaBaseResourcePhase;

/// Whether the current base resource has a placement with file index zero (0 no, 1 yes).
///
/// Refreshed for each resource. A match supplies texture offsets and permits
/// optional base-60 loads; it is not a completion flag for the whole pass.
u16 _gLoadingAreaHasBasePlacement;

u16 gLoadingAreaResourcePass;

#include "loading.h"

/// Retained two-word data with no reader; its purpose and internal format are unproven.
u32 D_8010CAC8[2] = { 0, 0xE1EFCD00 };

/// Global-library file-group bases indexed by `AreaResource.fileGroupIndex` (0..8).
///
/// Adding fileNumber / 100 produces the request's group byte. Entries retain
/// their two-byte stride; the group byte consumes the low byte of the sum.
static const u16 _gLoadingAreaFileGroupBases[AREA_RESOURCE_FILE_GROUP_BASE_2 + 1] __attribute__((section(".data"))) = { 10, 20, 30, 40, 50, 60, 0, 1, 2 };

/// Queues the current placement's global-library file and signed texture relocation.
///
/// Borrows matching live placement/resource cursors; fileIdLow is the nonzero
/// low base-100 component. The resource's file-group selector is 0..8 and its
/// nonnegative file number must resolve to a cataloged global file.
/// The group/hundreds components narrow to bytes. X offsets count 64-word
/// VRAM columns; Y offsets count CLUT rows. Enqueue copies key bytes 3/2/0
/// and all four argument bytes synchronously, retaining no stack addresses;
/// key byte 1 is unused. Queue capacity is the caller's responsibility.
static inline void _loadingQueueCurrentPlacementFile(void)
{
    CdCmdFileKey        fileKey;
    CdCmdFileLoadArgs   fileParams;
    const AreaResource* resource;
    s32                 fileNumber;

    fileKey.stage     = GAME_STAGE_NONE;
    fileKey.fileIndex = _gLoadingAreaPlacementCursor->fileIdLow;
    resource          = _gLoadingAreaResourceCursor;
    fileNumber        = resource->fileNumber;
    if (fileNumber >= LOADING_FILE_ID_RADIX) {
        fileParams.fileIdHundreds = fileNumber % LOADING_FILE_ID_RADIX;
        fileKey.fileGroup         = _gLoadingAreaFileGroupBases[resource->fileGroupIndex] + (resource->fileNumber / LOADING_FILE_ID_RADIX);
    } else {
        fileParams.fileIdHundreds = resource->fileNumber;
        fileKey.fileGroup         = _gLoadingAreaFileGroupBases[resource->fileGroupIndex];
    }
    fileParams.loadMode         = CD_COMMAND_LOAD_DEFAULT;
    fileParams.imageXPageOffset = _gLoadingAreaPlacementCursor->texturePageOffset;
    fileParams.imageYOffset     = _gLoadingAreaPlacementCursor->clutRowOffset;
    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, &fileKey, &fileParams);
}

u16 loadingPollAreaPlacementFiles(void)
{
    AreaVariant* layout;

    switch (gLoadingAreaPlacementFilePhase) {
        case LOADING_AREA_INIT:
            // The loaded destination must resolve before either table is read.
            layout                       = areaGetVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location);
            _gLoadingAreaVariant         = layout;
            _gLoadingAreaPlacementCursor = layout->placements;
            if (layout == NULL) {
                return 1;
            }
            if (_gLoadingAreaResourceCursor == NULL) {
                return 1;
            }
            if (_gLoadingAreaPlacementCursor == NULL) {
                return 1;
            }
            gLoadingAreaPlacementFilePhase++;
        case LOADING_AREA_QUEUE:
            while (_gLoadingAreaPlacementCursor->entryId != AREA_PLACEMENT_END) {
                if (_gLoadingAreaPlacementCursor->entryId == LOADING_AREA_UNUSED_PLACEMENT_ENTRY) {
                    _gLoadingAreaPlacementCursor++;
                    continue;
                }
                for (_gLoadingAreaResourceCursor = _gLoadingAreaVariant->resources; _gLoadingAreaResourceCursor->entryId != AREA_PLACEMENT_END; _gLoadingAreaResourceCursor++) {
                    if (_gLoadingAreaPlacementCursor->entryId == _gLoadingAreaResourceCursor->entryId) {
                        break;
                    }
                }
                if (_gLoadingAreaPlacementCursor->fileIdLow == LOADING_AREA_BASE_FILE_INDEX) {
                    _gLoadingAreaPlacementCursor++;
                    continue;
                }
                // Load this placement's additional file with its texture relocation.
                _loadingQueueCurrentPlacementFile();
                gLoadingAreaPlacementFilePhase++;
                break;
            }
            if (_gLoadingAreaPlacementCursor->entryId == AREA_PLACEMENT_END) {
                return 1;
            }
            break;
        case LOADING_AREA_WAIT:
            if (cdCmdIsIdle()) {
                _gLoadingAreaPlacementCursor++;
                gLoadingAreaPlacementFilePhase--;
            }
            break;
    }
    return 0;
}

/// Queues an area's global-library base file with its image relocation offsets.
///
/// `resource` is borrowed for this call; its file-group selector must be 0..8
/// and its nonnegative file number must name a cataloged file. The base file's
/// low index is zero. Offsets retain their low signed byte: X counts 64-word
/// VRAM pages and CLUT Y counts rows. `cdCmdEnqueue` copies the request before
/// returning; its ring-capacity contract applies and its slot result is ignored.
///
/// A resource may name a file the disc does not have: a package holding several
/// spawn kinds lists each kind as a resource, the later ones with file numbers
/// of their own (Akropolis's forked road encounters 47, 71 and 73, the landing
/// pad departure's Rupert and helicopter). The load finds no sector and reads
/// nothing; the kind's task table is in the package an earlier resource loads.
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
        fileKey[2]     = _gLoadingAreaFileGroupBases[resource->fileGroupIndex] + (resource->fileNumber / LOADING_FILE_ID_RADIX);
    } else {
        commandArgs[0] = resource->fileNumber;
        fileKey[2]     = _gLoadingAreaFileGroupBases[resource->fileGroupIndex];
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
            layout                      = areaGetVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location);
            _gLoadingAreaVariant        = layout;
            _gLoadingAreaResourceCursor = layout->resources;
            if (layout == NULL || layout->resources == NULL) {
                return 1;
            }
            gLoadingAreaBaseResourcePhase++;
        case LOADING_AREA_QUEUE:
            if (_gLoadingAreaResourceCursor->entryId == AREA_PLACEMENT_END) {
                return 1;
            }
            do {
                _gLoadingAreaPlacementCursor  = _gLoadingAreaVariant->placements;
                _gLoadingAreaHasBasePlacement = false;
                if (_gLoadingAreaPlacementCursor->entryId != AREA_PLACEMENT_END) {
                    entryId = _gLoadingAreaResourceCursor->entryId;
                    while (_gLoadingAreaPlacementCursor->entryId != AREA_PLACEMENT_END) {
                        if (_gLoadingAreaPlacementCursor->entryId == entryId && _gLoadingAreaPlacementCursor->fileIdLow == LOADING_AREA_BASE_FILE_INDEX) {
                            _gLoadingAreaHasBasePlacement = true;
                            break;
                        }
                        _gLoadingAreaPlacementCursor++;
                    }
                }
                // Base-60 files are optional unless a base placement names them.
                resource = _gLoadingAreaResourceCursor;
                if (resource->fileGroupIndex != AREA_RESOURCE_FILE_GROUP_BASE_60) {
                    if (_gLoadingAreaHasBasePlacement != 0) {
                        _loadingQueueAreaResource(resource, _gLoadingAreaPlacementCursor->texturePageOffset, _gLoadingAreaPlacementCursor->clutRowOffset);
                    } else {
                        _loadingQueueAreaResource(resource, 0, 0);
                    }
                    gLoadingAreaBaseResourcePhase++;
                    break;
                } else if (_gLoadingAreaHasBasePlacement != 0) {
                    _loadingQueueAreaResource(resource, _gLoadingAreaPlacementCursor->texturePageOffset, _gLoadingAreaPlacementCursor->clutRowOffset);
                    gLoadingAreaBaseResourcePhase++;
                    break;
                } else {
                    _gLoadingAreaResourceCursor = resource + 1;
                }
            } while (resource[1].entryId != AREA_PLACEMENT_END);
            if (_gLoadingAreaResourceCursor->entryId == AREA_PLACEMENT_END) {
                return 1;
            }
            break;
        case LOADING_AREA_WAIT:
            if (cdCmdIsIdle()) {
                _gLoadingAreaResourceCursor++;
                gLoadingAreaBaseResourcePhase--;
            }
            break;
    }
    return 0;
}
