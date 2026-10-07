/* Part of the jukebox library; see jukebox.h. */

/// Draws a jukebox track row and publishes a newly confirmed selection to its menu task.
///
/// Borrows a live list, panel object and owner task. The current row must fit
/// the selected track list: mode 0..3 after a clear, otherwise list 4, plus
/// five outside training mode. Draws outlined large text in panel-relative
/// pixels. Only an active row handles confirm; a changed row sounds confirm
/// and records its index. If its sequence differs from the pending sequence,
/// fades MIDI over 60 ticks, arms menu state 1 and drops the queued CD tail.
/// Text and primitive storage must satisfy `textDrawString`'s frame lifetime.
static void _jukeboxDrawRow(UiList* list, UiObject* object)
{
    enum {
        JUKEBOX_UNCLEARED_TRACK_LIST      = 4,
        JUKEBOX_REGULAR_TRACK_LIST_OFFSET = 5,
        JUKEBOX_MIDI_FADE_TICKS           = 60
    };
    JukeboxTrackLists   rowScratch;
    const JukeboxTrack* track;
    s32                 rowIndex;
    s32                 listIndex;
    s32                 rowInputEnabled;

    rowIndex   = list->currentItemIndex;
    rowScratch = _gJukeboxTrackLists;

    listIndex = JUKEBOX_UNCLEARED_TRACK_LIST;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount != 0) {
        listIndex = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode;
    }
    if (attachmentIsTrainingMode() == 0) {
        listIndex += JUKEBOX_REGULAR_TRACK_LIST_OFFSET;
    }

    // Resolve the rowIndex before its stack table storage becomes a text request.
    track                     = &rowScratch.lists[listIndex][rowIndex];
    rowScratch.req.x          = object->panel.contentOriginX.unsignedValue + list->rowTextX.unsignedValue;
    rowScratch.req.y          = (list->rowTextY.signedValue - 3) + object->panel.contentOriginY.unsignedValue;
    rowScratch.req.otIndex    = object->panel.otIndex.signedValue + 1;
    rowScratch.req.colorRgb   = list->colorRgb;
    rowScratch.req.glyphTable = TEXT_GLYPH_TABLE_LARGE;
    rowScratch.req.drawMode   = TEXT_DRAW_OUTLINED;
    rowScratch.req.alignment  = TEXT_ALIGNMENT_LEFT;
    textDrawString(&rowScratch.req, (const u8*)track->name);

    rowInputEnabled = list->rowInputEnabled;
    if (rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            if (object->owner->spawnArg1.value != list->currentItemIndex) {
                sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
                if (object->owner->status != track->sequenceId) {
                    sndEvtRequestMidiStop(0, JUKEBOX_MIDI_FADE_TICKS);
                    object->owner->state  = rowInputEnabled;
                    object->owner->status = track->sequenceId;
                    cdCmdDropQueuedTail();
                }
                object->owner->spawnArg1.value = list->currentItemIndex;
            }
        }
    }
}
