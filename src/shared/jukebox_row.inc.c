/* Part of the jukebox library; see jukebox.h. */

/// Row callback of the jukebox list: draws the row's track name, and on
/// confirm, when the row is not the one already chosen, plays the select
/// sound and, when the track differs from the one playing, fades the music
/// out and hands the track id to the menu task to load.
void jukeboxDrawRow(UiList* prompt, UiObject* obj)
{
    RoomsShared8018055cMenu    menu;
    RoomsShared8018055cCourse* course;
    s32                        row;
    s32                        list;
    s32                        mode;

    row  = prompt->field_8;
    menu = _gJukeboxTrackLists;

    list = 4;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.clearCount != 0) {
        list = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode;
    }
    if (Gp_IsDebugAttachRoom() == 0) {
        list += 5;
    }

    course              = &menu.lists[list][row];
    menu.req.x          = obj->panel.contentOriginX.unsignedValue + (u16)prompt->field_18;
    menu.req.y          = (prompt->field_1A - 3) + obj->panel.contentOriginY.unsignedValue;
    menu.req.otIndex    = obj->panel.otIndex.signedValue + 1;
    menu.req.colorRgb   = prompt->field_1C;
    menu.req.glyphTable = TEXT_GLYPH_TABLE_LARGE;
    menu.req.drawMode   = TEXT_DRAW_OUTLINED;
    menu.req.alignment  = TEXT_ALIGNMENT_LEFT;
    Text_DrawString(&menu.req, course->name);

    mode = prompt->field_C;
    if (mode == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
            if (obj->owner->spawnArg1.value != prompt->field_8) {
                SndEvt_EnqueueType6(SOUND_SYSTEM_CONFIRM, 0, 0);
                if (obj->owner->status != course->id) {
                    SndEvt_EnqueueType2(0, 0x3C);
                    obj->owner->state  = mode;
                    obj->owner->status = course->id;
                    CdCmd_DropPending();
                }
                obj->owner->spawnArg1.value = prompt->field_8;
            }
        }
    }
}
