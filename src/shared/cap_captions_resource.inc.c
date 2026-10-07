/* Continue cap_captions.inc.c after the preceding overlay wrappers. */

static void CapCaption_ShowModal(s16 arg0, s16 arg1, s16 arg2)
{
    CAP_CAPTION_SELECT_RECORD(arg0, arg1, 0xD0);
    displayQueueModeTask(&CapCaption_Data_80154508, arg2, 0, STAGE_ENTRY_RELOAD);
}

/// Selects a loaded CAP data payload and its font texture-page origin.
///
/// texturePageX is a VRAM word X and texturePageY a VRAM row. dataResourceIndex
/// is zero-based among FILE_SYSTEM_RESOURCE_DATA slots, not all directory slots.
/// The bundle load must have finished and the selected CAP storage must remain
/// alive during caption use. Page coordinates are stored even on failure; a
/// missing data ordinal or invalid CAP magic leaves the previous tables selected.
/// This selects/relocates existing storage and performs no I/O or allocation.
static inline void _capCaptionLoadResource(s16 texturePageX, s16 texturePageY, s16 dataResourceIndex)
{
    s32 dataOrdinal;
    s32 slotIndex;

    dataOrdinal              = 0;
    _gCapCaptionTexturePageX = texturePageX;
    _gCapCaptionTexturePageY = texturePageY;
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(D_8006C338); slotIndex++) {
        if (D_8006C338[slotIndex].kind == FILE_SYSTEM_RESOURCE_DATA) {
            if (dataOrdinal == dataResourceIndex) {
                _capCaptionRelocateFile(D_8006C338[slotIndex].data);
                break;
            }
            dataOrdinal++;
        }
    }
}
