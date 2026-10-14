/* Part of the view figure library; see view_figure.h. */

/// Applies a pending figure animation request or ticks its playing body tracks.
///
/// Uses the live published `_gViewFigureWork`, independently of the unread task
/// argument. BLEND captures and blends slots 1..19 over eight normal-rate
/// frames; RESET restarts them. Both select TICK and return before an ordinary
/// slot tick. TICK advances those tracks; other request states do nothing.
/// The twenty-part rig, loaded clips and pose storage must remain live, with
/// the scratch stack initialized for the animation helpers. Overwrites GTE state.
static void _viewFigureStepAnim(Task* task)
{
    if (_gViewFigureWork->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        _viewFigureReseedAnim();
        _gViewFigureWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (_gViewFigureWork->st.state == ACTOR_ENEMY_ANIM_RESET) {
        _viewFigureResetAnim();
        _gViewFigureWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (_gViewFigureWork->st.state == ACTOR_ENEMY_ANIM_TICK) {
        _viewFigureTickAnim();
    }
}

/// Advances the published figure's nineteen animated parts and writes their poses.
///
/// Requires live `_gViewFigureWork` with its context bound to the twenty-part
/// model and initialized slots 1 through 19. Slot 0 is the root controlled by
/// view placement. Keep the model, clip data and pose storage live; scratch
/// stack and GTE requirements are those of `animationTickSlot`.
static void _viewFigureTickAnim(void)
{
    enum { VIEW_FIGURE_FIRST_ANIM_SLOT = 1 };

    s32 slotIndex;

    for (slotIndex = VIEW_FIGURE_FIRST_ANIM_SLOT; slotIndex < ARRAY_SIZE(_gViewFigureWork->rig.slots); slotIndex++) {
        animationTickSlot(&_gViewFigureWork->rig.anim, slotIndex);
    }
}

/// Restarts the published figure's nineteen body tracks at normal playback rate.
///
/// Requires live `_gViewFigureWork`, a context bound to its twenty-part rig and
/// a loaded `st.animId` set supporting slots 1..19. Slot 0 remains under view
/// placement. Resets each driven slot's track, encoding and playback flags,
/// then records `appliedAnimId`; no pose is ticked or written by the reset.
/// The model, set data, slots and pose buffers remain borrowed for playback.
static void _viewFigureResetAnim(void)
{
    enum { VIEW_FIGURE_RESET_PRESET_RATE = 1,
           VIEW_FIGURE_RESET_FIRST_SLOT  = 1 };
    s32 slotIndex;

    for (slotIndex = VIEW_FIGURE_RESET_FIRST_SLOT; slotIndex < ARRAY_SIZE(_gViewFigureWork->rig.slots); slotIndex++) {
        // The reset replaces this preliminary rate with ANIMATION_RATE_ONE.
        _gViewFigureWork->rig.slots[slotIndex].rate = VIEW_FIGURE_RESET_PRESET_RATE;
        animationResetSlot(&_gViewFigureWork->rig.anim, slotIndex, _gViewFigureWork->st.animId);
    }
    _gViewFigureWork->st.appliedAnimId = _gViewFigureWork->st.animId;
}

/// Captures the figure's body poses and blends each track to the requested clip start.
///
/// Requires live `_gViewFigureWork` with initialized slots 1..19 of its twenty-
/// part rig and a loaded `st.animId` set supporting their current encodings.
/// Slot 0 stays under view placement. Each driven slot first ticks and captures
/// its pose, then blends to track-relative record zero over eight normal-rate
/// frames while retaining its rate and capture status. Records `appliedAnimId`
/// after all slots are sought. Borrowed clip and pose-buffer storage must remain
/// live throughout playback; scratch and GTE requirements are those of
/// `animationSeekSlotWithBlend`.
static void _viewFigureReseedAnim(void)
{
    enum { VIEW_FIGURE_RESEED_FIRST_SLOT   = 1,
           VIEW_FIGURE_RESEED_BLEND_FRAMES = 8 };
    s32 slotIndex;

    for (slotIndex = VIEW_FIGURE_RESEED_FIRST_SLOT; slotIndex < ARRAY_SIZE(_gViewFigureWork->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&_gViewFigureWork->rig.anim, slotIndex, _gViewFigureWork->st.animId, 0, VIEW_FIGURE_RESEED_BLEND_FRAMES);
    }
    _gViewFigureWork->st.appliedAnimId = _gViewFigureWork->st.animId;
}

/// Restarts the singleton view figure's body animation from a message request.
///
/// Requires live published figure work, its initialized rig and a request
/// whose `animationId` selects a loaded body set (1..5 in both carriers).
/// Reads only `animationId`; bank, blend and collision options are ignored.
/// The signed test rejects IDs >= 6 with -1, while negative IDs and zero still
/// enter the restart path. Accepted requests return zero after resetting slots
/// 1..19. Borrows the request only during dispatch; the receiver is unused.
static s32 _viewFigurePlayMessage(Task* unusedTask, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg)
{
    enum { VIEW_FIGURE_ANIMATION_ID_LIMIT = 6 };
    Task* figureTask;

    if (request->animationId < VIEW_FIGURE_ANIMATION_ID_LIMIT) {
        _gViewFigureWork->st.animId  = request->animationId;
        figureTask                   = ACTOR_MESSAGE_BODY_TASK;
        _gViewFigureWork->st.state   = ACTOR_ENEMY_ANIM_RESET;
        _gViewFigureWork->st.field_6 = 0;
        _viewFigureStepAnim(figureTask);
        return 0;
    }
    return -1;
}
