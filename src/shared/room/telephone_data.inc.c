/* Private per-instance storage. Include at the original data position.
 * The configuration contract is documented in telephone.h. */

static void _telephoneDrawPlayDataRow(UiList* list, UiObject* object);
static void _telephoneDrawUsageRow(UiList* list, UiObject* object);
static void _telephoneUsageTask(Task* task);
static void _telephonePromptTask(Task* task);
static void _telephonePlayDataTask(Task* task);
static void _telephoneSaveMenuRow(UiList* list, UiObject* object);
static void _telephonePlayDataMenuRow(UiList* list, UiObject* object);
static void _telephoneWeaponDataMenuRow(UiList* list, UiObject* object);
static void _telephonePeDataMenuRow(UiList* list, UiObject* object);

/// NUL-terminated caption of the telephone menu choice that opens the save dialog.
static const u8 _gTelephoneSaveMenuLabel[8] SECTION(".data") = "Save";

/// NUL-terminated caption of the telephone menu choice that opens play statistics.
static const u8 _gTelephonePlayDataMenuLabel[12] SECTION(".data") = "Play Data";

/// NUL-terminated caption of the telephone menu choice that opens weapon usage.
static const u8 _gTelephoneWeaponDataMenuLabel[12] SECTION(".data") = "Weapon Data";

/// NUL-terminated caption of the telephone menu choice that opens Parasite Energy usage.
static const u8 _gTelephonePeDataMenuLabel[8] SECTION(".data") = "PE Data";

/// NUL-terminated caption for elapsed play time, formatted from saved minutes.
static const u8 _gTelephonePlayTimeLabel[8] SECTION(".data") = "Time";

/// NUL-terminated caption for the saved count of battles won.
static const u8 _gTelephoneBattlesWonLabel[4] SECTION(".data") = "Won";

/// NUL-terminated caption for the saved count of battles escaped.
static const u8 _gTelephoneBattlesEscapedLabel[8] SECTION(".data") = "Escaped";

/// NUL-terminated caption for wins as a percentage of won and escaped battles.
static const u8 _gTelephoneWinPercentageLabel[12] SECTION(".data") = "Battles won";

/// NUL-terminated caption for wins plus Neo Ark roamer kills as a percentage of 326.
static const u8 _gTelephoneExterminationPercentageLabel[16] SECTION(".data") = "Exterminated";

/// NUL-terminated caption for the saved number of saves.
static const u8 _gTelephoneSaveCountLabel[8] SECTION(".data") = "Saved";

/// NUL-terminated caption for the number of completed runs.
static const u8 _gTelephoneClearCountLabel[8] SECTION(".data") = "Cleared";

/// NUL-terminated caption for the greatest EXP total retained from completed runs.
static const u8 _gTelephoneMaxExpLabel[8] SECTION(".data") = "Max EXP";

/// NUL-terminated caption for the greatest BP total retained from completed runs.
static const u8 _gTelephoneMaxBpLabel[8] SECTION(".data") = "Max BP";

/// NUL-terminated count suffix appended to saves, wins, escapes and completed runs.
static const u8 _gTelephoneTimesSuffix[8] SECTION(".data") = " times";

/// NUL-terminated percent sign appended to telephone statistics.
///
/// The four-byte allocation retains two more zero bytes after the terminator.
/// Formatters borrow the suffix without modifying it.
static const u8 _gTelephonePercentSuffix[4] SECTION(".data") = "%";

/// NUL-terminated two-line help for the elapsed play time row.
///
/// Borrowed by the help panel until its prompt is replaced or the panel closes.
static const u8 _gTelephonePlayTimeHelpText[44] SECTION(".data") = "Total amount of\ntime spent for this game.";

/// NUL-terminated two-line help for the save count row.
///
/// Borrowed by the help panel until its prompt is replaced or the panel closes.
static const u8 _gTelephoneSaveCountHelpText[36] SECTION(".data") = "Number of saves\nused in this game.";

/// NUL-terminated two-line help for the battles won row.
///
/// Borrowed by the help panel until its prompt is replaced or the panel closes.
static const u8 _gTelephoneBattlesWonHelpText[48] SECTION(".data") = "Total number of enemies\ndefeated in this game.";

/// NUL-terminated two-line help for the battles escaped row.
///
/// Borrowed by the help panel until its prompt is replaced or the panel closes.
static const u8 _gTelephoneBattlesEscapedHelpText[52] SECTION(".data") = "Total number of escapes\nfrom battle in this game.";

/// NUL-terminated two-line help for the battle-win percentage row.
///
/// Borrowed by the help panel until its prompt is replaced or the panel closes.
static const u8 _gTelephoneWinPercentageHelpText[52] SECTION(".data") = "Current percent of total\nbattles won in this game.";

/// NUL-terminated two-line help for the extermination percentage row.
///
/// Borrowed by the help panel until its prompt is replaced or the panel closes.
static const u8 _gTelephoneExterminationPercentageHelpText[56] SECTION(".data") = "Current percent of total\nenemies defeated in this game.";

/// NUL-terminated two-line help for the completed-run count row.
///
/// Borrowed by the help panel until its prompt is replaced or the panel closes.
static const u8 _gTelephoneClearCountHelpText[52] SECTION(".data") = "Number of times you have\ncleared the game so far.";

/// NUL-terminated two-line help for the maximum completed-run EXP row.
///
/// Borrowed by the help panel until its prompt is replaced or the panel closes.
static const u8 _gTelephoneMaxExpHelpText[56] SECTION(".data") = "Greatest amount of EXP gathered\nby the end of the game.";

/// NUL-terminated two-line help for the maximum completed-run BP row.
///
/// Borrowed by the help panel until its prompt is replaced or the panel closes.
static const u8 _gTelephoneMaxBpHelpText[56] SECTION(".data") = "Greatest amount of BP gathered\nby the end of the game.";

/// Shared row callback for all nine telephone play-statistic rows.
///
/// The list must enable `USER_INTERFACE_LIST_SHARED_ROW_CALLBACK`; entry 0
/// handles every item. This borrowed table lives for the overlay's lifetime.
static UiListRowCallback _gTelephonePlayDataRowCallbacks[] = {
    _telephoneDrawPlayDataRow,
};

enum { TELEPHONE_PLAY_DATA_LIST_ROW_COUNT = 9 };

/// Mutable list of Time, Saved, Won, Escaped, Battles won, Exterminated, Cleared, Max EXP and Max BP.
///
/// Row indices 0..8 share callback entry zero. Opening fits 15-pixel rows,
/// reserves five extra pixels for the extermination separator, then enables
/// shared dispatch and system cursor sound. Layout resets scrolling and input;
/// selection persists unless the saved cursor mode requests a reset. This
/// overlay-lifetime singleton serves one live play-data panel at a time.
static UiList _gTelephonePlayDataList = {
    .rowCallbacks    = _gTelephonePlayDataRowCallbacks,
    .itemCount       = TELEPHONE_PLAY_DATA_LIST_ROW_COUNT,
    .visibleRowCount = { .unsignedValue = TELEPHONE_PLAY_DATA_LIST_ROW_COUNT },
    .rowHeight       = 15,
};

/// Shared row callback for the populated weapon or Parasite Energy usage list.
///
/// The list must enable `USER_INTERFACE_LIST_SHARED_ROW_CALLBACK`; entry 0
/// handles every item. This borrowed table lives for the overlay's lifetime.
static UiListRowCallback _gTelephoneUsageRowCallbacks[] = {
    _telephoneDrawUsageRow,
};

/// Mutable list of recorded weapon or Parasite Energy usage, most used first.
///
/// Opening replaces the seeded count with 0..32 weapon rows or 0..12 spell
/// rows and resets selection and the top index. Layout fits 15-pixel rows,
/// then enables shared row dispatch and system cursor sound. The panel task
/// owns the matching usage work; its populated rows remain live through list
/// drawing and are released at teardown. This overlay-lifetime singleton
/// serves one usage panel at a time.
static UiList _gTelephoneUsageList = {
    .rowCallbacks    = _gTelephoneUsageRowCallbacks,
    .itemCount       = 1,
    .visibleRowCount = { .unsignedValue = 1 },
    .rowHeight       = 15,
};

enum { TELEPHONE_HELP_PANEL_STYLE = 3 };

/// Bottom help-panel recipe shared by the play-data and weapon/PE usage panels.
///
/// Bounds seed a 288-by-40-pixel panel below the statistics. The bodyless task
/// registers its UI object as the prompt holder and clears that registration
/// on exit. Spawning reads this recipe synchronously; each parent owns its
/// help child, whose callback must stay loaded until teardown.
static const UiObjectDesc _gTelephoneHelpPanelDescriptor SECTION(".data") = {
    .style           = TELEPHONE_HELP_PANEL_STYLE,
    .bounds          = { -144, 64, 288, 40 },
    .otIndexSeed     = 56,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _telephonePromptTask,
};

/// Titled play-statistics panel recipe opened by the telephone menu.
///
/// The bodyless task fits the seeded bounds to its nine-row singleton list,
/// opens a help child and returns Confirm on active cancel. Spawning reads the
/// recipe synchronously; the menu owns the child and its callback must stay
/// loaded until teardown. Only one play-data panel may use the list at a time.
static const UiObjectDesc _gTelephonePlayDataPanelDescriptor SECTION(".data") = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -144, -104, 288, 120 },
    .otIndexSeed     = 40,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _telephonePlayDataTask,
};

/// Titled weapon or Parasite Energy usage-panel recipe opened by the telephone menu.
///
/// The first spawn payload selects weapons (TELEPHONE_USAGE_WEAPONS, 0) or
/// spells (TELEPHONE_USAGE_PARASITE_ENERGY, 1). The bodyless task populates the
/// singleton usage list and opens a help child. Spawning reads the recipe
/// synchronously; the menu owns the panel and its callback code must remain
/// loaded until teardown. Only one usage panel may use the list at a time.
static const UiObjectDesc _gTelephoneUsagePanelDescriptor SECTION(".data") = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -144, -104, 288, 168 },
    .otIndexSeed     = 40,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _telephoneUsageTask,
};

/// Per-item telephone menu callbacks: Save, Play Data, Weapon Data, then PE Data.
///
/// Dispatch uses row indices 0..3 without the shared-row flag. This borrowed
/// table lives for the overlay's lifetime.
static UiListRowCallback _gTelephoneMenuRowCallbacks[] = {
    _telephoneSaveMenuRow,
    _telephonePlayDataMenuRow,
    _telephoneWeaponDataMenuRow,
    _telephonePeDataMenuRow,
};

enum { TELEPHONE_MENU_WRAP_NAVIGATION = 1 };

/// Mutable four-choice telephone menu: Save, Play Data, Weapon Data, then PE Data.
///
/// Indices 0..3 dispatch their corresponding callbacks, with wrapping
/// navigation and 15-pixel rows. Opening fits the list, resets scroll/input
/// state and enables system cursor sound without shared dispatch. Selection
/// persists unless the saved cursor mode requests a reset. This overlay-lifetime
/// singleton serves one telephone menu at a time; save-only entry bypasses it.
static UiList _gTelephoneMenuList = {
    .rowCallbacks    = _gTelephoneMenuRowCallbacks,
    .itemCount       = ARRAY_SIZE(_gTelephoneMenuRowCallbacks),
    .visibleRowCount = { .unsignedValue = ARRAY_SIZE(_gTelephoneMenuRowCallbacks) },
    .wrapNavigation  = TELEPHONE_MENU_WRAP_NAVIGATION,
    .rowHeight       = 15,
};
