/* Continue telephone.inc.c after the preceding overlay wrappers. */

/// Task body of a prompt window: on its first frame it becomes the UI holder and
/// installs `Telephone_ClosePrompt` as its exit callback; every
/// frame it draws the prompt lines.
static void Telephone_PromptTask(Task* task)
{
    UiObject* obj;

    obj         = task->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    if (task->state == 0) {
        Wip_UiHolder       = obj;
        task->exitCallback = Telephone_ClosePrompt;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
static void Telephone_InsertDecimalPoint(u8* str, s32 decimals)
{
    s32 len = 0;
    s32 i;

    if (decimals > 0) {
        while (*str != 0) {
            str++;
            len++;
        }
        if (len < decimals) {
            decimals = len;
        }
        decimals++;
        for (i = 0; i < decimals; i++) {
            str[1] = str[0];
            str--;
        }
        str[1] = '.';
    }
}

/// Format `value` as a percentage with `decimals` fractional digits into `buf`:
/// print the integer with at least `decimals + 1` digits when it is small enough
/// (so "5" with two decimals becomes "0.05"), otherwise print it unpadded, then
/// shift the last `decimals` digits right by one and drop a '.' in front of
/// them. Appends "%" and returns `buf`.
static u8* Telephone_FormatPercentage(u8* buf, s32 value, s32 decimals)
{
    s32 limit;
    s32 i;
    s32 len;
    s32 n;
    u8* p;

    limit = 1;
    for (i = decimals; i > 0; i--) {
        limit *= 10;
    }

    if (value < limit) {
        Text_ItoaPadded(buf, value, decimals + 1);
    } else {
        Text_ItoaUnsigned(buf, value);
    }

    n   = decimals;
    p   = buf;
    len = 0;
    if (n > 0) {
        while (*p != 0) {
            p++;
            len++;
        }
        if (len < n) {
            n = len;
        }
        n++;
        for (len = 0; len < n; len++) {
            p[1] = p[0];
            p--;
        }
        p[1] = '.';
    }

    Text_Strcat(buf, Telephone_Data_80181A78);
    return buf;
}

/// Task body of the play-data panel: on its first frame it spawns
/// `Telephone_Data_80181C90` and lays out the list; every frame it
/// draws the title, updates the list and closes on cancel.
static void Telephone_PlayDataTask(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list        = &Telephone_Data_80181C44;
    obj         = task->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawText(&(obj)->panel, Telephone_Data_8017D610);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&Telephone_Data_80181C90, 0, 0, 1, obj);
        Ui_LayoutListPanel(list, &(obj)->panel);
        obj->panel.bounds.unsignedRect.h += 5;
        list->flags                       = USER_INTERFACE_LIST_SHARED_ROW_CALLBACK;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskCancel) != 0) {
        obj->result = USER_INTERFACE_RESULT_CONFIRM;
    }
}

/// Queues a gouraud-shaded rectangle into the current OT one slot past the
/// panel's draw order. Origin is `field_20`/`field_22` plus (`arg1`, `arg2`);
/// `arg3`/`arg4` are width and height. Left vertices take `arg5`, right vertices
/// take `arg6`. A zero color or width < 2 draws nothing.
static void Telephone_DrawGauge(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
{
    POLY_G4* prim;
    s16      x;
    s32      y;
    s16      bottom;

    if ((arg5 != 0) && (arg3 >= 2)) {
        prim           = gGpuPrimCursor;
        x              = arg0->contentOriginX.unsignedValue + arg1 + 1;
        prim->x2       = x;
        prim->x0       = x;
        y              = arg0->contentOriginY.unsignedValue;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 8);
        GPU_PRIMITIVE_COLOR_WORD(prim, 0) = arg5;
        setcode(prim, 0x38);
        GPU_PRIMITIVE_COLOR_WORD(prim, 2) = arg5;
        GPU_PRIMITIVE_COLOR_WORD(prim, 3) = arg6;
        GPU_PRIMITIVE_COLOR_WORD(prim, 1) = arg6;
        y                                += arg2;
        y++;
        x        = prim->x0 + arg3 - 1;
        prim->y1 = y;
        prim->y0 = y;
        prim->x3 = x;
        prim->x1 = x;
        bottom   = y + arg4 - 1;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(gGpuCurrentOt + arg0->otIndex.signedValue + 1, prim);
    }
}

/// The telephone menu's "Save" row: confirmed while the CD is idle, it spawns
/// `D_800611E4` and moves the owning task to state 1.
static void Telephone_SaveRow(UiList* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, Telephone_Data_801819F8, prompt->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    sel = prompt->rowInputEnabled;
    if (sel == 1 && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0 && CdCmd_IsIdle() != 0) {
        SndEvt_EnqueueType6(SOUND_SYSTEM_CONFIRM, 0, 0);
        gDisplayState.gameMode = DISPLAY_GAME_MODAL;
        Ui_SpawnFromDesc(&D_800611E4, 1, 0, 0, obj);
        obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        obj->result             = USER_INTERFACE_RESULT_CONFIRM;
        obj->owner->state       = sel;
    }
}

/// The telephone menu's "Play Data" row: confirmed, it opens
/// `Telephone_Data_80181CAC` and moves the owning task to state 2.
static void Telephone_PlayDataRow(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, Telephone_Data_80181A00, prompt->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(SOUND_SYSTEM_CONFIRM, 0, 0);
        Ui_SpawnFromDesc(&Telephone_Data_80181CAC, 0, 1, 1, obj);
        obj->result             = USER_INTERFACE_RESULT_CONFIRM;
        obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        obj->owner->state       = 2;
    }
}

/// The telephone menu's "Weapon Data" row: confirmed, it opens the usage panel
/// `Telephone_Data_80181CC8` for weapons and moves the owning task to
/// state 2.
static void Telephone_WeaponDataRow(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, Telephone_Data_80181A0C, prompt->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(SOUND_SYSTEM_CONFIRM, 0, 0);
        Ui_SpawnFromDesc(&Telephone_Data_80181CC8, 0, 1, 1, obj);
        obj->result             = USER_INTERFACE_RESULT_CONFIRM;
        obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        obj->owner->state       = 2;
    }
}

/// The telephone menu's "PE Data" row: confirmed, it opens the usage panel
/// `Telephone_Data_80181CC8` for Parasite Energy and moves the owning
/// task to state 2.
static void Telephone_PeDataRow(UiList* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->rowTextX.signedValue, prompt->rowTextY.signedValue, Telephone_Data_80181A18, prompt->colorRgb, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    if (prompt->rowInputEnabled == USER_INTERFACE_LIST_ROW_ACTIVE && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(SOUND_SYSTEM_CONFIRM, 0, 0);
        Ui_SpawnFromDesc(&Telephone_Data_80181CC8, 1, 1, 1, obj);
        obj->result             = USER_INTERFACE_RESULT_CONFIRM;
        obj->panel.control.word = USER_INTERFACE_PANEL_INACTIVE;
        obj->owner->state       = 2;
    }
}

/// Task exit callback for the save-prompt UI: if this task still owns
/// `Wip_UiHolder`, clear it, then free the spawned UI object and kill the task.
static void Telephone_ClosePrompt(Task* task)
{
    UiObject* holder;

    holder = task->spawnArg2.pointer;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    uiObjectTaskExit(task);
}
