/* Part of the Pawn/Rook GOLEM library; see golem_pawn_rook.h. */

/// Faces and approaches the player, then selects a close attack, weapon approach or scream.
///
/// The Beam Sword approaches by charging; the Grenade Launcher starts a burst.
/// Only a Rook with scream charges can choose Silence; successive screams widen
/// the random mask against the generator's upper sixteen bits. Requires live
/// body work and the player's matrix in the root's parent frame. Bearing narrows
/// X/Z offsets to signed halfwords; distance uses
/// full-word offsets whose squared sum must fit a signed word. Headings use
/// 4096 units per turn. Borrows a VECTOR block
/// without using its Y, and leaves movement and animation to the frame handler.
static void _golemPawnRookEngageState(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_ENGAGE_TRACK           = 0,
        GOLEM_PAWN_ROOK_ENGAGE_PICK_ATTACK     = 1,
        GOLEM_PAWN_ROOK_ENGAGE_LISTEN          = GOLEM_PAWN_ROOK_ENGAGE_LISTEN_STEP,
        GOLEM_PAWN_ROOK_ENGAGE_TURN            = 3,
        GOLEM_PAWN_ROOK_ENGAGE_ANIM_TURN       = 3,
        GOLEM_PAWN_ROOK_ENGAGE_ANIM_SCREAM     = 10,
        GOLEM_PAWN_ROOK_ENGAGE_SPEED           = 20,
        GOLEM_PAWN_ROOK_ENGAGE_TURN_THRESHOLD  = 1409,
        GOLEM_PAWN_ROOK_ENGAGE_SIGHT_YAW_LIMIT = 128,
        GOLEM_PAWN_ROOK_ENGAGE_LISTEN_FRAMES   = 96,
        GOLEM_PAWN_ROOK_ENGAGE_TURN_FRAMES     = 35,
        GOLEM_PAWN_ROOK_ENGAGE_TURN_RATE       = 59,
    };
    s16 currentYaw;
    s16 listenYaw;
    s16 step;
    s16 yawDifference;
    s16 listenYawDifference;
    s16 forwardSpeed;
    s32 absoluteDifference;
    s32 absoluteListenDifference;
    s16 wrappedDifference;
    s16 wrappedListenDifference;
    s16 yawError;
    s32 playerOffsetX;
    s32 playerOffsetZ;
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
    u32 screamRandom;
#endif
    u16                sightFlags;
    u16                listenSightFlags;
    VECTOR*            scratchTop;
    VECTOR*            toPlayer;
    GolemPawnRookWork* work;
    GfxCoord*          root;

    /// Updates player/current yaw and measures their shortest 4096-unit gap.
    ///
    /// Arguments must be side-effect-free pointers or lvalues, evaluated repeatedly.
    /// yawValue, differenceValue, wrappedValue and errorValue are signed halfwords;
    /// magnitudeValue is s32. Captures no locals and expands to a compound statement.
    /// Kept local to this handler and undefined below.
#define GOLEM_PAWN_ROOK_MEASURE_PLAYER_YAW_GAP(work, root, toPlayer, yawValue, differenceValue, magnitudeValue, wrappedValue, errorValue) \
    {                                                                                                                                     \
        (work)->targetYaw = (u16)(ratan2((s16)(toPlayer)->vx, (s16)(toPlayer)->vz) & ACTOR_TRANSFORM_ANGLE_MASK);                         \
        (yawValue)        = ratan2((root)->coord.m[0][2], (root)->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;                            \
        (work)->yaw       = (yawValue);                                                                                                   \
        (differenceValue) = (work)->targetYaw - (yawValue);                                                                               \
        (magnitudeValue)  = __builtin_abs((differenceValue));                                                                             \
        if ((magnitudeValue) < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {                                                                         \
            (errorValue) = (magnitudeValue);                                                                                              \
        } else {                                                                                                                          \
            if ((differenceValue) > 0) {                                                                                                  \
                (wrappedValue) = ACTOR_TRANSFORM_ANGLE_TURN - (differenceValue);                                                          \
            } else {                                                                                                                      \
                (wrappedValue) = (differenceValue) + ACTOR_TRANSFORM_ANGLE_TURN;                                                          \
            }                                                                                                                             \
            (errorValue) = (wrappedValue);                                                                                                \
        }                                                                                                                                 \
    }

    scratchTop                   = SCRATCH_STACK_CURSOR(VECTOR);
    toPlayer                     = scratchTop - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = toPlayer;
    work                         = actor->work;
    step                         = work->step;
    root                         = actor->extra.tmd->coords;
    switch (step) {
        // Approach after the blend, opening sight inside the narrow yaw cone.
        case GOLEM_PAWN_ROOK_ENGAGE_TRACK:
            forwardSpeed = 0;
            if (work->animFrame >= gGolemPawnRookAnimBlendFrames[work->anim]) {
                forwardSpeed = GOLEM_PAWN_ROOK_ENGAGE_SPEED;
            }
            work->forwardSpeed = forwardSpeed;
            work->turnRate     = GOLEM_PAWN_ROOK_WIND_UP_TURN;
            toPlayer->vx       = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
            toPlayer->vz       = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
            GOLEM_PAWN_ROOK_MEASURE_PLAYER_YAW_GAP(work, root, toPlayer, currentYaw, yawDifference, absoluteDifference, wrappedDifference, yawError);
            if (yawError >= GOLEM_PAWN_ROOK_ENGAGE_TURN_THRESHOLD) {
                if (work->turnedAround == 0) {
                    work->anim = GOLEM_PAWN_ROOK_ENGAGE_ANIM_TURN;
                    work->step = GOLEM_PAWN_ROOK_ENGAGE_TURN;
                } else {
                    work->anim         = GOLEM_PAWN_ROOK_ANIM_LISTEN;
                    work->step         = GOLEM_PAWN_ROOK_ENGAGE_PICK_ATTACK;
                    work->turnedAround = 0;
                }
            }
            if (yawError < GOLEM_PAWN_ROOK_ENGAGE_SIGHT_YAW_LIMIT) {
                sightFlags            = work->sightBody.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->sightBody.flags = sightFlags;
                if (work->playerSpotted != 0) {
                    work->sightBody.flags = (u16)(sightFlags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
                    work->step            = GOLEM_PAWN_ROOK_ENGAGE_PICK_ATTACK;
                    work->turnedAround    = 0;
                }
            }
            break;
        // Prefer the close strike; otherwise the Rook can scream before its weapon approach.
        case GOLEM_PAWN_ROOK_ENGAGE_PICK_ATTACK:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            toPlayer->vx       = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
            playerOffsetZ      = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
            toPlayer->vz       = playerOffsetZ;
            playerOffsetX      = toPlayer->vx;
            if (SquareRoot0((playerOffsetX * playerOffsetX) + (playerOffsetZ * playerOffsetZ)) < GOLEM_PAWN_ROOK_LUNGE_RANGE) {
                work->behavior = GOLEM_PAWN_ROOK_LUNGE_STATE;
                work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                work->anim     = GOLEM_PAWN_ROOK_LUNGE_ANIM;
            } else {
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
                screamRandom    = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = screamRandom;
                if (!((screamRandom >> 0x10) & ((1 << (work->screamCount + 1)) - 1)) && !(gPlayerStatus.statusFlags & PLAYER_STATUS_SILENCE) &&
                    work->screamCharges != 0) {
                    work->behavior        = GOLEM_PAWN_ROOK_BEHAVIOR_SCREAM;
                    work->step            = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                    work->anim            = GOLEM_PAWN_ROOK_ENGAGE_ANIM_SCREAM;
                    work->timer           = 0;
                    work->screamActive    = 1;
                    work->interruptDamage = 0;
                    work->screamCount++;
                } else {
                    work->behavior = GOLEM_PAWN_ROOK_WALK_STATE;
                    work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                    work->anim     = GOLEM_PAWN_ROOK_WALK_ANIM;
                    work->timer    = 0;
                }
#else
                work->behavior = GOLEM_PAWN_ROOK_WALK_STATE;
                work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                work->anim     = GOLEM_PAWN_ROOK_WALK_ANIM;
                work->timer    = 0;
#endif
            }
            break;
        // Listen after an attack, then retry facing once the hold expires.
        case GOLEM_PAWN_ROOK_ENGAGE_LISTEN:
            work->forwardSpeed    = 0;
            work->turnRate        = 0;
            listenSightFlags      = work->sightBody.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->sightBody.flags = listenSightFlags;
            if (work->playerSpotted != 0) {
                work->sightBody.flags = (u16)(listenSightFlags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
                work->anim            = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->step            = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
            } else if (work->animFrame >= GOLEM_PAWN_ROOK_ENGAGE_LISTEN_FRAMES) {
                toPlayer->vx = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
                toPlayer->vz = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
                GOLEM_PAWN_ROOK_MEASURE_PLAYER_YAW_GAP(work, root, toPlayer, listenYaw, listenYawDifference, absoluteListenDifference, wrappedListenDifference, yawError);
                if (yawError >= GOLEM_PAWN_ROOK_ENGAGE_TURN_THRESHOLD) {
                    work->anim = GOLEM_PAWN_ROOK_ENGAGE_ANIM_TURN;
                    work->step = GOLEM_PAWN_ROOK_ENGAGE_TURN;
                } else {
                    work->anim = GOLEM_PAWN_ROOK_ANIM_WALK;
                    work->step = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                }
            }
            break;
        case GOLEM_PAWN_ROOK_ENGAGE_TURN:
            work->forwardSpeed = 0;
            work->turnRate     = GOLEM_PAWN_ROOK_ENGAGE_TURN_RATE;
            if (work->animFrame >= GOLEM_PAWN_ROOK_ENGAGE_TURN_FRAMES) {
                work->anim         = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->step         = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                work->turnedAround = 1;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);

#undef GOLEM_PAWN_ROOK_MEASURE_PLAYER_YAW_GAP
}
