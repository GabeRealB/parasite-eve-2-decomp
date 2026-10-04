/* Part of the jukebox library; see jukebox.h. */

/// Row callback of the jukebox list: draws the row's track name, and on
/// confirm, when the row is not the one already chosen, plays the select
/// sound and, when the track differs from the one playing, fades the music
/// out and hands the sequence id to the menu task to load.
void jukeboxDrawRow(UiList* prompt, UiObject* obj)
{
    JukeboxTrackLists work;
    JukeboxTrack*     track;
    s32               row;
    s32               list;
    s32               mode;

    row  = prompt->currentItemIndex;
    work = _gJukeboxTrackLists;

    list = 4;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount != 0) {
        list = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode;
    }
    if (Gp_IsDebugAttachRoom() == 0) {
        list += 5;
    }

    track               = &work.lists[list][row];
    work.req.x          = obj->panel.contentOriginX.unsignedValue + prompt->rowTextX.unsignedValue;
    work.req.y          = (prompt->rowTextY.signedValue - 3) + obj->panel.contentOriginY.unsignedValue;
    work.req.otIndex    = obj->panel.otIndex.signedValue + 1;
    work.req.colorRgb   = prompt->colorRgb;
    work.req.glyphTable = TEXT_GLYPH_TABLE_LARGE;
    work.req.drawMode   = TEXT_DRAW_OUTLINED;
    work.req.alignment  = TEXT_ALIGNMENT_LEFT;
    Text_DrawString(&work.req, track->name);

    mode = prompt->rowInputEnabled;
    if (mode == 1) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
            if (obj->owner->spawnArg1.value != prompt->currentItemIndex) {
                sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
                if (obj->owner->status != track->sequenceId) {
                    SndEvt_EnqueueType2(0, 0x3C);
                    obj->owner->state  = mode;
                    obj->owner->status = track->sequenceId;
                    CdCmd_DropPending();
                }
                obj->owner->spawnArg1.value = prompt->currentItemIndex;
            }
        }
    }
}
