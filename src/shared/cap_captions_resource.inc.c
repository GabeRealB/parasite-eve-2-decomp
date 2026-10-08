/* Continue cap_captions.inc.c after the preceding overlay wrappers. */

/// Selects a keyed caption at baseline 208 and queues a cancellable mode task.
///
/// Requires a loaded relocated CAP file with a valid command and key 0..255;
/// its storage and textures remain live through the queued mode. The signed
/// duration counts ticks after the mode task's initial arming tick. Cancel or
/// expiry requests mode exit with reload entry behavior. Selection is shared
/// per carrier and remains selected if queueing cannot start the mode.
static void _capCaptionShowModal(s16 commandIndex, s16 key, s16 durationTicks)
{
    CAP_CAPTION_SELECT_RECORD(commandIndex, key, CAP_CAPTION_DEFAULT_BOTTOM_BASELINE_Y);
    displayQueueModeTask(&CapCaption_Data_80154508, durationTicks, 0, STAGE_ENTRY_RELOAD);
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
