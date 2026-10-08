/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Stops the body for a heavy hit recoil, then resumes player engagement.
///
/// The last hit's side chooses the clip: front recovers after 80 frames and behind
/// after 59. The actor must have live GOLEM work; movement and animation are
/// requests executed by the enclosing frame handler.
static void _golemPawnRookRecoilState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_RECOIL_START         = 0,
        GOLEM_PAWN_ROOK_RECOIL_FRONT         = 1,
        GOLEM_PAWN_ROOK_RECOIL_BEHIND        = 2,
        GOLEM_PAWN_ROOK_RECOIL_ANIM_FRONT    = 0x12,
        GOLEM_PAWN_ROOK_RECOIL_ANIM_BEHIND   = 0x13,
        GOLEM_PAWN_ROOK_RECOIL_FRONT_FRAMES  = 80,
        GOLEM_PAWN_ROOK_RECOIL_BEHIND_FRAMES = 59,
    };
    GolemPawnRookWork* work;
    s32                step;
    s32                hitFromFront;

    work = actor->work;
    step = work->step;
    switch (step) {
        case GOLEM_PAWN_ROOK_RECOIL_START:
            hitFromFront = work->hitFromFront;
            if (hitFromFront == 1) {
                work->anim = GOLEM_PAWN_ROOK_RECOIL_ANIM_FRONT;
                work->step = hitFromFront;
            } else {
                work->anim = GOLEM_PAWN_ROOK_RECOIL_ANIM_BEHIND;
                work->step = GOLEM_PAWN_ROOK_RECOIL_BEHIND;
            }
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            break;
        case GOLEM_PAWN_ROOK_RECOIL_FRONT:
            if (work->animFrame >= GOLEM_PAWN_ROOK_RECOIL_FRONT_FRAMES) {
                work->anim     = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
            }
            break;
        case GOLEM_PAWN_ROOK_RECOIL_BEHIND:
            if (work->animFrame >= GOLEM_PAWN_ROOK_RECOIL_BEHIND_FRAMES) {
                work->anim     = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
            }
            break;
    }
}
