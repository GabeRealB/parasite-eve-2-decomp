/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Starts an upright reaction and prevents the interrupted strike from hitting.
static inline void _golemKnightBishopBeginHitReaction(GolemKnightBishopWork* work, s16 sequence)
{
    work->sequence          = sequence;
    work->step              = 0;
    work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Selects the GOLEM's reaction after HP loss or an interrupted grab.
///
/// `task` owns a live GOLEM work block and Enemy with valid parameters;
/// `damage` is HP damage, possibly accumulated across a grab. Zero HP selects
/// death and stops appearance sounds. Below one tenth maximum HP selects a
/// knockdown or downed hit. Other unlocked or flickering hits flinch, with
/// damage below 80 selecting the light reaction. A locked downed pose delays
/// its hit/death sequence; upright reactions restart and disarm the strike.
static void _golemKnightBishopPickHitReaction(Task* task, s32 damage)
{
    enum {
        GOLEM_KNIGHT_BISHOP_REACTION_NONE           = 0,
        GOLEM_KNIGHT_BISHOP_REACTION_LIGHT_FLINCH   = 1,
        GOLEM_KNIGHT_BISHOP_REACTION_HEAVY_FLINCH   = 2,
        GOLEM_KNIGHT_BISHOP_REACTION_KNOCKDOWN      = 3,
        GOLEM_KNIGHT_BISHOP_REACTION_DOWNED_HIT     = 4,
        GOLEM_KNIGHT_BISHOP_REACTION_COLLAPSE_DEATH = 5,
        GOLEM_KNIGHT_BISHOP_REACTION_DOWNED_DEATH   = 6,
        GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_DAMAGE     = 80,
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_HP_DIVISOR    = 10,
    };
    Enemy*                 enemy    = task->spawnArg2.pointer;
    s16                    hp       = enemy->hp;
    GolemKnightBishopWork* work     = task->work;
    u32                    reaction = GOLEM_KNIGHT_BISHOP_REACTION_NONE;
    s32                    maxHp;

    if (hp <= 0) {
        reaction = GOLEM_KNIGHT_BISHOP_REACTION_DOWNED_DEATH;
        if (work->downedPose == 0) {
            reaction = GOLEM_KNIGHT_BISHOP_REACTION_COLLAPSE_DEATH;
        }
        if (work->appearSound != 0) {
            sndEvtRequestScriptStop(work->appearSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            work->appearSound = 0;
        }
        if (work->vanishSound != 0) {
            sndEvtRequestScriptStop(work->vanishSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            work->vanishSound = 0;
        }
    } else if (maxHp = enemy->param->hpMax, hp < maxHp / GOLEM_KNIGHT_BISHOP_KNOCKDOWN_HP_DIVISOR) {
        reaction = GOLEM_KNIGHT_BISHOP_REACTION_DOWNED_HIT;
        if (work->downedPose == 0) {
            reaction = GOLEM_KNIGHT_BISHOP_REACTION_KNOCKDOWN;
        }
    } else if (work->reactionLock == 0 || work->flickerStage != 0) {
        work->reactionLock = 0;
        reaction           = GOLEM_KNIGHT_BISHOP_REACTION_HEAVY_FLINCH;
        if (damage < GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_DAMAGE) {
            reaction = GOLEM_KNIGHT_BISHOP_REACTION_LIGHT_FLINCH;
        }
    }

    // Downed reactions wait for the fall lock; upright reactions cancel the strike.
    switch (reaction) {
        case GOLEM_KNIGHT_BISHOP_REACTION_NONE:
            break;
        case GOLEM_KNIGHT_BISHOP_REACTION_LIGHT_FLINCH:
            _golemKnightBishopBeginHitReaction(work, GOLEM_KNIGHT_BISHOP_SEQUENCE_LIGHT_FLINCH);
            break;
        case GOLEM_KNIGHT_BISHOP_REACTION_HEAVY_FLINCH:
            _golemKnightBishopBeginHitReaction(work, GOLEM_KNIGHT_BISHOP_SEQUENCE_HEAVY_FLINCH);
            break;
        case GOLEM_KNIGHT_BISHOP_REACTION_KNOCKDOWN:
            _golemKnightBishopBeginHitReaction(work, GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL);
            break;
        case GOLEM_KNIGHT_BISHOP_REACTION_DOWNED_HIT:
            if (work->reactionLock == 0) {
                work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_HIT;
                work->step     = 0;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_REACTION_COLLAPSE_DEATH:
            _golemKnightBishopBeginHitReaction(work, GOLEM_KNIGHT_BISHOP_SEQUENCE_COLLAPSE_DEATH);
            break;
        case GOLEM_KNIGHT_BISHOP_REACTION_DOWNED_DEATH:
            if (work->reactionLock == 0) {
                work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_DEATH;
                work->step     = 0;
            }
            break;
    }
}
