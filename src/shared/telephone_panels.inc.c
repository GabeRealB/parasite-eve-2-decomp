/* Continue telephone.inc.c after the preceding overlay wrappers. */

/// Draws the telephone help panel registered as the current prompt holder.
///
/// The task owns its UI object in `spawnArg2.pointer`; its first payload is the
/// borrowed text or packed P.E. item passed to the menu prompt renderer. First
/// update installs the holder and its clearing exit callback. The task and any
/// borrowed text must remain live until prompt drawing and teardown finish.
static void _telephonePromptTask(Task* task)
{
    UiObject* object;

    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == TELEPHONE_PANEL_INITIALIZE) {
        Wip_UiHolder       = object;
        task->exitCallback = _telephonePromptTaskExit;
        task->state       += 1;
    }
    itemMenuDrawTaskPrompt(object, task);
}

/// Inserts a decimal point before the requested trailing digits.
///
/// `digits` is a writable NUL-terminated digit string with one extra byte of
/// capacity, including the terminator. Positive `fractionalDigits` is clamped
/// to the string length; a shorter string receives a leading point, not zeros.
/// Nonpositive counts leave the string unchanged. The digit cursor walks back
/// one byte before the string when all digits move, without dereferencing it.
static void _telephoneInsertDecimalPoint(u8* digits, s32 fractionalDigits)
{
    s32 digitCount = 0;
    s32 shiftIndex;

    if (fractionalDigits > 0) {
        TELEPHONE_SHIFT_DECIMAL_DIGITS(digits, digitCount, fractionalDigits, shiftIndex);
    }
}

/// Formats a decimal-scaled percentage and returns the caller's buffer.
///
/// `scaledPercent` is nonnegative and measured in 10^fractionalDigits units per
/// percent. `fractionalDigits` is 0..8, keeping the decimal threshold in s32.
/// Small values are zero-padded to keep a whole digit before the point; a percent
/// suffix follows. The buffer must hold the formatted digits, optional point,
/// percent sign and NUL (12 bytes suffice for the stated domain). Unsigned
/// decimal conversion saturates at 999999999. No storage is allocated or retained.
static u8* _telephoneFormatPercentage(u8* buffer, s32 scaledPercent, s32 fractionalDigits)
{
    s32 wholePercentThreshold;
    s32 powerIndex;
    s32 digitCount;
    s32 shiftBytes;
    u8* digitEnd;

    wholePercentThreshold = 1;
    for (powerIndex = fractionalDigits; powerIndex > 0; powerIndex--) {
        wholePercentThreshold *= 10;
    }

    if (scaledPercent < wholePercentThreshold) {
        textItoaPadded(buffer, scaledPercent, fractionalDigits + 1);
    } else {
        textItoaUnsigned(buffer, scaledPercent);
    }

    shiftBytes = fractionalDigits;
    digitEnd   = buffer;
    digitCount = 0;
    if (shiftBytes > 0) {
        TELEPHONE_SHIFT_DECIMAL_DIGITS(digitEnd, digitCount, shiftBytes, digitCount);
    }

    textAppendString(buffer, Telephone_Data_80181A78);
    return buffer;
}

#undef TELEPHONE_SHIFT_DECIMAL_DIGITS

/// Updates the nine-row play-data panel and closes it on active cancel.
///
/// The task owns its UI object in `spawnArg2.pointer`. Its first frame opens an
/// inactive help panel, fits the shared list and reserves five extra pixels for
/// the extermination separator. Cancel publishes Confirm for the telephone
/// controller to reopen its menu; ordinary UI teardown owns object release.
static void _telephonePlayDataTask(Task* task)
{
    UiObject* object;
    UiList*   list;

    list           = &Telephone_Data_80181C44;
    object         = task->spawnArg2.pointer;
    object->result = USER_INTERFACE_RESULT_NONE;
    uiDrawPanelLabel(&(object)->panel, Telephone_Data_8017D610);
    if (task->state == TELEPHONE_PANEL_INITIALIZE) {
        uiSpawnObject(&Telephone_Data_80181C90, 0, USER_INTERFACE_PANEL_INACTIVE, 1, object);
        uiFitPanelToList(list, &(object)->panel);
        object->panel.bounds.unsignedRect.h += 5;
        list->flags                          = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        uiSetListSystemCursorSound(list, 1);
        task->state += 1;
    }
    uiUpdateList(list, &object->panel);
    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
        object->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}

/// Queues the opaque horizontal gradient inside a telephone gauge.
///
/// Pixel offsets are relative to the borrowed panel's content origin. Vertices
/// span (offsetX+1, offsetY+1) to (offsetX+width, offsetY+height), narrowed to s16;
/// coordinate arithmetic must fit s32 and height is unchecked. Zero `leftRgb`
/// or width below two skips drawing. Colors
/// are RGB in bits 0..23; the left top byte is replaced by the POLY_G4 command,
/// and the other top bytes are retained. Requires one aligned POLY_G4 of writable
/// primitive space and a writable panel OT index+1; retain it until GPU completion.
/// The caller draws the surrounding bevel separately.
static void _telephoneDrawGaugeFill(const UiPanel* panel, s32 offsetX, s32 offsetY, s32 width, s32 height, u32 leftRgb, u32 rightRgb)
{
    enum { TELEPHONE_GAUGE_PACKET_CODE = 0x38 };
    POLY_G4* primitive;
    s16      edgeX;
    s32      topY;
    s16      bottomY;

    if ((leftRgb != 0) && (width >= 2)) {
        primitive      = gGpuPrimCursor;
        edgeX          = panel->contentOriginX.unsignedValue + offsetX + 1;
        primitive->x2  = edgeX;
        primitive->x0  = edgeX;
        topY           = panel->contentOriginY.unsignedValue;
        gGpuPrimCursor = primitive + 1;
        setlen(primitive, sizeof(*primitive) / sizeof(u32) - 1);
        GPU_PRIMITIVE_COLOR_WORD(primitive, 0) = leftRgb;
        setcode(primitive, TELEPHONE_GAUGE_PACKET_CODE);
        GPU_PRIMITIVE_COLOR_WORD(primitive, 2) = leftRgb;
        GPU_PRIMITIVE_COLOR_WORD(primitive, 3) = rightRgb;
        GPU_PRIMITIVE_COLOR_WORD(primitive, 1) = rightRgb;
        topY                                  += offsetY;
        topY++;
        edgeX         = primitive->x0 + width - 1;
        primitive->y1 = topY;
        primitive->y0 = topY;
        primitive->x3 = edgeX;
        primitive->x1 = edgeX;
        bottomY       = topY + height - 1;
        primitive->y3 = bottomY;
        primitive->y2 = bottomY;
        addPrim(gGpuCurrentOt + panel->otIndex.signedValue + 1, primitive);
    }
}

/// Draws the Save menu row and opens the save dialog on active confirmation.
///
/// Requires a live menu list/object and an idle CD command queue to accept input.
/// Confirmation selects modal display, opens the save child, disables menu input
/// and publishes Confirm to hide the menu. The owner's phase becomes wait-save;
/// the child answer later selects the saved/cancelled notice.
static void _telephoneSaveMenuRow(UiList* list, UiObject* object)
{
    s32 rowInputEnabled;

    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, Telephone_Data_801819F8, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    rowInputEnabled = list->rowInputEnabled;
    if (rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0 && cdCmdIsIdle() != 0) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        gDisplayState.gameMode = DISPLAY_GAME_MODAL;
        uiSpawnObject(&D_800611E4, 1, USER_INTERFACE_PANEL_INACTIVE, 0, object);
        object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        object->result             = USER_INTERFACE_RESULT_CONFIRM;
        object->owner->state       = rowInputEnabled;
    }
}

/// Draws the Play Data menu row and opens its statistics panel on confirmation.
///
/// Requires a live list/object and the selected active row. Confirmation opens
/// the child panel, disables the menu and publishes Confirm to hide it. The
/// owner waits for statistics dismissal, which reopens the telephone menu.
static void _telephonePlayDataMenuRow(UiList* list, UiObject* object)
{
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, Telephone_Data_80181A00, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        uiSpawnObject(&Telephone_Data_80181CAC, 0, USER_INTERFACE_PANEL_ACTIVE, 1, object);
        object->result             = USER_INTERFACE_RESULT_CONFIRM;
        object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        object->owner->state       = TELEPHONE_MENU_WAIT_STATISTICS;
    }
}

/// Draws the Weapon Data menu row and opens its statistics panel on confirmation.
///
/// Requires a live list/object and the selected active row. Confirmation opens
/// the child panel, disables the menu and publishes Confirm to hide it. The
/// owner waits for statistics dismissal, which reopens the telephone menu.
static void _telephoneWeaponDataMenuRow(UiList* list, UiObject* object)
{
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, Telephone_Data_80181A0C, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        uiSpawnObject(&Telephone_Data_80181CC8, TELEPHONE_USAGE_WEAPONS, USER_INTERFACE_PANEL_ACTIVE, 1, object);
        object->result             = USER_INTERFACE_RESULT_CONFIRM;
        object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        object->owner->state       = TELEPHONE_MENU_WAIT_STATISTICS;
    }
}

/// Draws the PE Data menu row and opens its statistics panel on confirmation.
///
/// Requires a live list/object and the selected active row. Confirmation opens
/// the child panel, disables the menu and publishes Confirm to hide it. The
/// owner waits for statistics dismissal, which reopens the telephone menu.
static void _telephonePeDataMenuRow(UiList* list, UiObject* object)
{
    textDrawUiLine(object, list->rowTextX.signedValue, list->rowTextY.signedValue, Telephone_Data_80181A18, list->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (list->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
        sndEvtRequestScriptStart(SOUND_SYSTEM_CONFIRM, 0, 0);
        uiSpawnObject(&Telephone_Data_80181CC8, TELEPHONE_USAGE_PARASITE_ENERGY, USER_INTERFACE_PANEL_ACTIVE, 1, object);
        object->result             = USER_INTERFACE_RESULT_CONFIRM;
        object->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        object->owner->state       = TELEPHONE_MENU_WAIT_STATISTICS;
    }
}

/// Releases a telephone help panel and clears its prompt-holder ownership.
///
/// The task owns the UI object in its second spawn argument. Only a holder that
/// still points at this object is cleared; a newer prompt remains registered.
/// Normal UI teardown releases the object and task, invalidating both pointers.
static void _telephonePromptTaskExit(Task* task)
{
    UiObject* object;

    object = task->spawnArg2.pointer;
    if (Wip_UiHolder == object) {
        Wip_UiHolder = NULL;
    }
    uiObjectTaskExit(task);
}
