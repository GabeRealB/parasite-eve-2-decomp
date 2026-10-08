/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Plays a fatal hit from the saved downed pose and enters the corpse state.
///
/// `task` owns a live GOLEM model/work block. The reaction lasts 16 frames
/// after a hit from behind, 22 after one from the front. It requests an
/// appearance fade with nominal lengths of ten translucency frames and five
/// colour-blend frames, then hands the task to its dead-state handler.
static void _golemKnightBishopDownedDeathSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_START            = 0,
        GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_BEHIND           = 1,
        GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_FRONT            = 2,
        GOLEM_KNIGHT_BISHOP_TASK_DEAD                        = 2,
        GOLEM_KNIGHT_BISHOP_DOWNED_DEATH_TRANSLUCENCY_FRAMES = 10,
        GOLEM_KNIGHT_BISHOP_DOWNED_DEATH_COLOR_FRAMES        = 5,
    };
    GolemKnightBishopWork* work;
    s16                    step;
    s32                    downedPose;

    work = task->work;
    step = work->step;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_START:
            // Keep the widened pose snapshot that also selects the reaction step.
            downedPose = work->downedPose;
            if (downedPose == GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND) {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_BEHIND;
                work->step = downedPose;
            } else {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_FRONT;
                work->step = GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_FRONT;
            }
            work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
            work->translucencyFadeFrames = GOLEM_KNIGHT_BISHOP_DOWNED_DEATH_TRANSLUCENCY_FRAMES;
            work->colorBlendFadeFrames   = GOLEM_KNIGHT_BISHOP_DOWNED_DEATH_COLOR_FRAMES;
            break;
        case GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_BEHIND:
            if (work->animFrame >= GOLEM_KNIGHT_BISHOP_DOWNED_HIT_BEHIND_FRAMES) {
                task->state = GOLEM_KNIGHT_BISHOP_TASK_DEAD;
                work->step  = 0;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_FRONT:
            if (work->animFrame >= GOLEM_KNIGHT_BISHOP_DOWNED_HIT_FRONT_FRAMES) {
                task->state = step;
                work->step  = 0;
            }
            break;
    }
}
