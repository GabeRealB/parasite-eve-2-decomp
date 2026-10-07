/* Private per-instance storage. Include at the original data position.
 * The configuration contract is documented in cap_captions.h. */

static void CapCaption_CancelableTask(Task* task);

static void CapCaption_TimedTask(Task* task);

static TaskDesc CapCaption_Data_801544FC = { { { TASK_BODY_NONE, 32 } }, CapCaption_TimedTask, { .value = 0 } };

static TaskDesc CapCaption_Data_80154508 = { { { TASK_BODY_NONE, 32 } }, CapCaption_CancelableTask, { .value = 0 } };

static CapCaptionScheduleWindow CapCaption_Data_80154514[13] = {
    { 300, 295, 16, 5 },
    { 240, 235, 16, 4 },
    { 180, 175, 16, 3 },
    { 120, 115, 16, 2 },
    { 60, 55, 16, 1 },
    { 30, 25, 17, 30 },
    { 5, 4, 17, 5 },
    { 4, 3, 17, 4 },
    { 3, 2, 17, 3 },
    { 2, 1, 17, 2 },
    { 1, 0, 17, 1 },
    { 0, -3, 17, 0 },
    { CAP_CAPTION_SCHEDULE_END, 0, 0, 0 },
};

// Initial pulse state retained across caption selections.
enum { CAP_CAPTION_CARET_INITIAL_LEVEL     = 8,
       CAP_CAPTION_CARET_INITIAL_DIRECTION = 0 };

/// Current continuation-caret brightness scale, inclusive integer range 8..15.
///
/// Drawing converts level / 15 to vertex intensities, then steps the pulse.
/// Caption suppression and the initial caret countdown leave this level frozen.
static s32 _gCapCaptionCaretPulseLevel = CAP_CAPTION_CARET_INITIAL_LEVEL;

/// Continuation-caret pulse direction (0 rising, 1 falling).
///
/// Reverses after stepping to either brightness endpoint and persists across
/// caption selections. Kept as a word because the pulse helper updates its address.
static s32 _gCapCaptionCaretPulseFalling = CAP_CAPTION_CARET_INITIAL_DIRECTION;
