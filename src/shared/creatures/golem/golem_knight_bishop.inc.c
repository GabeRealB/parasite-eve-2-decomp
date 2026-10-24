/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Falls dead from standing and hands the task to its corpse handler.
///
/// The last hit's side selects the fall clip, saved downed pose and impact frame.
/// Grid participation moves from the root's ground sphere to the shifted hurt
/// sphere. The signed frame countdown includes the first update after setup.
static void _golemKnightBishopCollapseDeathSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_COLLAPSE_START = 0,
        GOLEM_KNIGHT_BISHOP_COLLAPSE_FALL  = 1,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    s32                    step;
    s32                    sound;
    s32                    audioPan;
    s32                    impactFrame;
    s16                    framesLeft;

    work = task->work;
    step = work->step;
    root = task->extra.tmd->coords;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_COLLAPSE_START:
            if (work->hitFromFront == 0) {
                work->anim            = GOLEM_KNIGHT_BISHOP_ANIM_FALL_BEHIND;
                work->step            = GOLEM_KNIGHT_BISHOP_COLLAPSE_FALL;
                work->downedPose      = GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND;
                work->timer           = GOLEM_KNIGHT_BISHOP_FALL_BEHIND_FRAMES;
                work->hurtBody.pos.vz = GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND_OFFSET_Z;
            } else {
                work->anim            = GOLEM_KNIGHT_BISHOP_ANIM_FALL_FRONT;
                work->step            = GOLEM_KNIGHT_BISHOP_COLLAPSE_FALL;
                work->downedPose      = GOLEM_KNIGHT_BISHOP_DOWNED_FRONT;
                work->timer           = GOLEM_KNIGHT_BISHOP_FALL_FRONT_FRAMES;
                work->hurtBody.pos.vz = GOLEM_KNIGHT_BISHOP_DOWNED_FRONT_OFFSET_Z;
            }
            work->hurtBody.radius        = GOLEM_KNIGHT_BISHOP_HURT_RADIUS;
            work->knockdownStage         = GOLEM_KNIGHT_BISHOP_FALL_STARTED;
            work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
            work->translucencyFadeFrames = GOLEM_KNIGHT_BISHOP_STANDARD_TRANSLUCENCY_FRAMES;
            work->colorBlendFadeFrames   = GOLEM_KNIGHT_BISHOP_STANDARD_COLOR_BLEND_FRAMES;
            work->reactionLock           = GOLEM_KNIGHT_BISHOP_REACTION_FALLING;
            work->forwardSpeed           = 0;
            work->hurtBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->groundBody.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
        case GOLEM_KNIGHT_BISHOP_COLLAPSE_FALL:
            if (work->knockdownStage == step) {
                work->knockdownStage = GOLEM_KNIGHT_BISHOP_FALL_SETTLED;
            }
            impactFrame = GOLEM_KNIGHT_BISHOP_FALL_FRONT_IMPACT_FRAME;
            if (work->downedPose == step) {
                impactFrame = GOLEM_KNIGHT_BISHOP_FALL_BEHIND_IMPACT_FRAME;
            }
            if (work->animFrame == impactFrame) {
                sound    = _gGolemKnightBishopAnimSounds[work->soundSet + 8] | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                audioPan = (s8)worldCoordGetOriginAudioPan(root);
                sndEvtRequestScriptStart(sound, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
            }
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                task->state        = GOLEM_KNIGHT_BISHOP_TASK_DEAD;
                work->step         = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                work->reactionLock = GOLEM_KNIGHT_BISHOP_REACTION_UNLOCKED;
            }
            break;
    }
}

/// Queues a GOLEM cue with placement-instance selection and spatial audio.
///
/// `root` is a `GfxCoord*` with a composed view matrix; `placeKey` is the Enemy's
/// unsigned halfword placement key, promoted before extracting its index.
/// `soundId` is a base script ID. Invoke as a standalone statement inside a
/// braced block. Root is evaluated twice, the integers once each; arguments
/// must have no side effects. Captures the enclosing function's writable s32
/// `instanceSoundId`, which must not alias an argument; pan is local to the block.
#define GOLEM_KNIGHT_BISHOP_PLAY_ANIM_CUE(root, soundId, placeKey)                                                         \
    {                                                                                                                      \
        enum { GOLEM_KNIGHT_BISHOP_CUE_INSTANCE_SHIFT = 8 };                                                               \
        s32 audioPan;                                                                                                      \
        instanceSoundId = (soundId) | (((placeKey) >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_KNIGHT_BISHOP_CUE_INSTANCE_SHIFT); \
        audioPan        = (s8)worldCoordGetOriginAudioPan((root));                                                         \
        sndEvtRequestScriptStart(instanceSoundId, audioPan, (s8)worldCoordGetOriginAudioDepth((root)));                    \
    }

/// Plays spatial sound scripts on falling edges of the GOLEM's animation cues.
///
/// `task` owns a live rig/model and borrows its Enemy through `spawnArg2`.
/// Room sound sets 1..4 select paired script IDs; set 0 disables cues.
/// Slot 1 supplies cue bits 0x20 and 0x10, played in that order if both fall.
/// The Enemy placement index occupies script bits 8..11. Pan and depth narrow
/// to signed bytes. Disabled cues and buffered poses retain the previous bits.
static void _golemKnightBishopPlayAnimCues(Task* task)
{
    s32                    instanceSoundId;
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    const AnimationRecord* record;

    work = task->work;
    root = task->extra.tmd->coords;
    if (work->soundSet != 0) {
        record = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
        if (record != NULL) {
            if (!(record->flags & ANIMATION_RECORD_CUE_2) && (work->prevCueFlags & ANIMATION_RECORD_CUE_2)) {
                GOLEM_KNIGHT_BISHOP_PLAY_ANIM_CUE(root, _gGolemKnightBishopAnimSounds[work->soundSet * 2 - 1], ((Enemy*)task->spawnArg2.pointer)->placeKey);
            }
            if (!(record->flags & ANIMATION_RECORD_CUE_1) && (work->prevCueFlags & ANIMATION_RECORD_CUE_1)) {
                GOLEM_KNIGHT_BISHOP_PLAY_ANIM_CUE(root, _gGolemKnightBishopAnimSounds[work->soundSet * 2], ((Enemy*)task->spawnArg2.pointer)->placeKey);
            }
            work->prevCueFlags = record->flags & ANIMATION_RECORD_CUE_MASK;
        }
    }
}

#undef GOLEM_KNIGHT_BISHOP_PLAY_ANIM_CUE

/// Scratch-stack block the aim beam is set up in.
///
/// The point the beam leaves is a fixed offset from part 4. It is carried
/// into the root's frame for the beam's collision capsule, and into world
/// space, with the beam's far end, for the projection through `GsWSMATRIX`.
/// Reserved for the length of the call; nothing in it outlives that.
typedef struct {
    MATRIX  partInRoot; // part 4's transform expressed in the root's frame
    VECTOR  offset;     // result of a GTE rotation: a point's offset along the target frame's axes, before that frame's translation is added; also the contact point less the point on part 4, measured for the beam's length
    SVECTOR ends[2];    // the beam's ends, each first in its own frame and then in world space: [0] the far end, placed from the root; [1] the point on part 4
    s32     screenXY;   // projected screen position of the end last transformed: x in the low half, y in the high
    s32     depth;      // its ordering depth, a quarter of the screen z
} _GolemKnightBishopAimScratch;
STATIC_ASSERT_SIZEOF(_GolemKnightBishopAimScratch, 0x48);

/// Updates the collision probe and projected red beam from model part 4.
///
/// `task` owns live GOLEM work and composed model coordinates. A positive aim
/// countdown enables the capsule; drawing starts after its first countdown tick
/// and also runs on the final zero tick. Contact distance is measured in world
/// units and narrowed to s16; a player-body contact extends it by 300 units.
/// The scratch block remains reserved while the beam drawer borrows the results.
static void _golemKnightBishopAimFromPart(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_AIM_PART_X           = -40,
        GOLEM_KNIGHT_BISHOP_AIM_PART_Y           = -120,
        GOLEM_KNIGHT_BISHOP_AIM_PART_Z           = 220,
        GOLEM_KNIGHT_BISHOP_AIM_PLAYER_EXTENSION = 300,
    };
    _GolemKnightBishopAimScratch* scratch;
    GolemKnightBishopWork*        work;
    GfxCoord*                     root;
    GfxCoord*                     beamPart;
    s32                           endpointIndex;
    s16                           contactDistance;

    root                   = task->extra.tmd->coords;
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(_GolemKnightBishopAimScratch);
    work                   = task->work;
    beamPart               = &root[4];
    root->composeStamp     = GRAPHICS_COORD_DIRTY;
    beamPart->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(beamPart);
    // Express the part offset in root space for the live collision capsule.
    if (work->auxTimer > 0) {
        gfxMakeRelativeTransform(&root->workm, &beamPart->workm, &scratch->partInRoot);
        scratch->ends[1].vx = GOLEM_KNIGHT_BISHOP_AIM_PART_X;
        scratch->ends[1].vy = GOLEM_KNIGHT_BISHOP_AIM_PART_Y;
        scratch->ends[1].vz = GOLEM_KNIGHT_BISHOP_AIM_PART_Z;
        gte_SetRotMatrix(&scratch->partInRoot);
        gte_ldv0(&scratch->ends[1]);
        gte_rtv0();
        gte_stlvnl(&scratch->offset);
        work->aimBeamCapsule.ends[1].vx = scratch->partInRoot.t[0] + scratch->offset.vx;
        work->aimBeamCapsule.ends[1].vy = scratch->partInRoot.t[1] + scratch->offset.vy;
        work->aimBeamCapsule.ends[1].vz = scratch->partInRoot.t[2] + scratch->offset.vz;
        work->aimBeamBody.flags        |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->aimBeamBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    }
    // Consume the previous collision pass, then project both world-space ends.
    if (work->auxTimer < GOLEM_KNIGHT_BISHOP_AIM_FRAMES - 1) {
        scratch->ends[1].vx = GOLEM_KNIGHT_BISHOP_AIM_PART_X;
        scratch->ends[1].vy = GOLEM_KNIGHT_BISHOP_AIM_PART_Y;
        scratch->ends[1].vz = GOLEM_KNIGHT_BISHOP_AIM_PART_Z;
        gte_SetRotMatrix(&beamPart->workm);
        gte_ldv0(&scratch->ends[1]);
        gte_rtv0();
        gte_stlvnl(&scratch->offset);
        scratch->ends[1].vx = beamPart->workm.t[0] + scratch->offset.vx;
        scratch->ends[1].vy = beamPart->workm.t[1] + scratch->offset.vy;
        scratch->ends[1].vz = beamPart->workm.t[2] + scratch->offset.vz;
        scratch->ends[0].vx = 0;
        scratch->ends[0].vy = GOLEM_KNIGHT_BISHOP_AIM_ROOT_Y;
        if (worldCollisionFindContactIndex(work->aimBeamContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
            scratch->offset.vx  = work->aimBeamContacts[0].point.vx - scratch->ends[1].vx;
            scratch->offset.vy  = work->aimBeamContacts[0].point.vy - scratch->ends[1].vy;
            scratch->offset.vz  = work->aimBeamContacts[0].point.vz - scratch->ends[1].vz;
            contactDistance     = SquareRoot0(scratch->offset.vx * scratch->offset.vx + scratch->offset.vy * scratch->offset.vy + scratch->offset.vz * scratch->offset.vz);
            scratch->ends[0].vz = contactDistance;
            if ((work->aimBeamContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
                scratch->ends[0].vz = contactDistance + GOLEM_KNIGHT_BISHOP_AIM_PLAYER_EXTENSION;
            }
            worldCollisionClearContacts(work->aimBeamContacts);
        } else {
            scratch->ends[0].vz = GOLEM_KNIGHT_BISHOP_AIM_RANGE;
        }
        gte_SetRotMatrix(&root->workm);
        gte_ldv0(&scratch->ends[0]);
        gte_rtv0();
        gte_stlvnl(&scratch->offset);
        scratch->ends[0].vx = root->workm.t[0] + scratch->offset.vx;
        scratch->ends[0].vy = root->workm.t[1] + scratch->offset.vy;
        scratch->ends[0].vz = root->workm.t[2] + scratch->offset.vz;
        for (endpointIndex = 0; endpointIndex < ARRAY_SIZE(scratch->ends); endpointIndex++) {
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_ldv0(&scratch->ends[endpointIndex]);
            gte_rtps();
            gte_stsxy(&scratch->screenXY);
            gte_stszotz(&scratch->depth);
            work->beamScreenX[endpointIndex] = scratch->screenXY;
            work->beamScreenY[endpointIndex] = scratch->screenXY >> 16;
            work->beamDepth[endpointIndex]   = scratch->depth;
        }
        _golemKnightBishopDrawAimBeam(task);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemKnightBishopAimScratch);
}

/// Scratch-stack block the aim beam is drawn from.
///
/// The beam is cut into eight segments between its two projected ends. Each
/// segment has three pairs of vertices: its start and end on the centre line,
/// and the same two points set off to either side by a width that shrinks
/// with the segment's depth. Reserved for the length of the call.
typedef struct {
    VECTOR  span;       // screen vector from the beam's far end to the point on part 4; z is zero
    SVECTOR direction;  // `span` normalised (0x1000 for 1), with y negated; its components, swapped, give the offsets across the beam
    s32     depth;      // ordering depth at the end of the segment being drawn; the segment is skipped below 0x1E
    s32     depthStep;  // an eighth of the depth difference between the beam's ends
    u16     vertexX[6]; // screen x of the segment's vertices: [0], [1] its start and end on the centre line; [2], [3] and [4], [5] the same points on either edge
    u16     vertexY[6]; // screen y of the same vertices
    s16     stepX;      // an eighth of the beam's extent in screen x: one segment's length
    s16     stepY;      // the same in screen y
} _GolemKnightBishopAimBeamScratch;
STATIC_ASSERT_SIZEOF(_GolemKnightBishopAimBeamScratch, 0x3C);

/// Queues the red core and two ribbons that fade to black across one beam segment.
///
/// `beam` supplies six screen-pixel vertex pairs and a positive quarter-Z depth.
/// The carrier's two corner rows select vertices 0..5. The current GPU arena
/// must have room for two `POLY_G4`, one `LINE_F2` and one `DR_TPAGE`, and remain
/// live through submission; the scratch input is only borrowed during this call.
static inline void _golemKnightBishopEmitAimBeamSegment(const _GolemKnightBishopAimBeamScratch* beam)
{
    enum {
        GOLEM_KNIGHT_BISHOP_BEAM_DRAW_MODE   = 0xE1000620, // Additive blending, dithering and drawing in the display area.
        GOLEM_KNIGHT_BISHOP_BEAM_CORE_CODE   = 0x42,
        GOLEM_KNIGHT_BISHOP_BEAM_RIBBON_CODE = 0x3A,
    };
    POLY_G4*  ribbon;
    LINE_F2*  line;
    DR_TPAGE* drawMode;
    s32       ribbonIndex;
    for (ribbonIndex = 0; ribbonIndex < ARRAY_SIZE(_gGolemKnightBishopBeamQuadCorners); ribbonIndex++) {
        ribbon         = gGpuPrimCursor;
        gGpuPrimCursor = ribbon + 1;
        setlen(ribbon, sizeof(*ribbon) / sizeof(u32) - 1);
        ribbon->code = GOLEM_KNIGHT_BISHOP_BEAM_RIBBON_CODE;
        ribbon->x0   = beam->vertexX[_gGolemKnightBishopBeamQuadCorners[ribbonIndex][0]];
        ribbon->y0   = beam->vertexY[_gGolemKnightBishopBeamQuadCorners[ribbonIndex][0]];
        ribbon->x1   = beam->vertexX[_gGolemKnightBishopBeamQuadCorners[ribbonIndex][1]];
        ribbon->y1   = beam->vertexY[_gGolemKnightBishopBeamQuadCorners[ribbonIndex][1]];
        ribbon->x2   = beam->vertexX[_gGolemKnightBishopBeamQuadCorners[ribbonIndex][2]];
        ribbon->y2   = beam->vertexY[_gGolemKnightBishopBeamQuadCorners[ribbonIndex][2]];
        ribbon->x3   = beam->vertexX[_gGolemKnightBishopBeamQuadCorners[ribbonIndex][3]];
        ribbon->y3   = beam->vertexY[_gGolemKnightBishopBeamQuadCorners[ribbonIndex][3]];
        setRGB0(ribbon, 0xFF, 0, 0);
        setRGB1(ribbon, 0xFF, 0, 0);
        setRGB2(ribbon, 0, 0, 0);
        setRGB3(ribbon, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(beam->depth), ribbon);
    }
    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setlen(line, sizeof(*line) / sizeof(u32) - 1);
    line->code = GOLEM_KNIGHT_BISHOP_BEAM_CORE_CODE;
    line->x0   = beam->vertexX[0];
    line->y0   = beam->vertexY[0];
    line->x1   = beam->vertexX[1];
    line->y1   = beam->vertexY[1];
    setRGB0(line, 0xFF, 0, 0);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(beam->depth), line);
    // Prepending the draw mode last makes it execute before the additive packets.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setlen(drawMode, sizeof(*drawMode) / sizeof(u32) - 1);
    drawMode->code[0] = GOLEM_KNIGHT_BISHOP_BEAM_DRAW_MODE;
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(beam->depth), drawMode);
}

/// Draws the red aim beam from the work block's projected endpoints.
///
/// Requires the screen X/Y and quarter-Z depths prepared by
/// `_golemKnightBishopAimFromPart`. Eight screen-space segments use their ending
/// depth for both width and ordering; depths below 30 are skipped. Each segment
/// queues two fading additive ribbons and a red core into the current GPU arena.
/// The arena must have room for the packets and stay live through GPU submission.
static void _golemKnightBishopDrawAimBeam(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_BEAM_SEGMENTS  = 8,
        GOLEM_KNIGHT_BISHOP_BEAM_MIN_DEPTH = 30,
    };
    _GolemKnightBishopAimBeamScratch* beam;
    GolemKnightBishopWork*            work;
    s32                               segmentIndex;

    beam          = SCRATCH_STACK_RESERVE_BLOCK(_GolemKnightBishopAimBeamScratch);
    work          = task->work;
    beam->span.vx = work->beamScreenX[1] - work->beamScreenX[0];
    beam->span.vy = work->beamScreenY[1] - work->beamScreenY[0];
    beam->span.vz = 0;
    VectorNormalS(&beam->span, &beam->direction);
    beam->direction.vy *= -1;
    beam->stepX         = (work->beamScreenX[1] - work->beamScreenX[0]) / GOLEM_KNIGHT_BISHOP_BEAM_SEGMENTS;
    beam->stepY         = (work->beamScreenY[1] - work->beamScreenY[0]) / GOLEM_KNIGHT_BISHOP_BEAM_SEGMENTS;
    beam->depthStep     = (work->beamDepth[1] - work->beamDepth[0]) / GOLEM_KNIGHT_BISHOP_BEAM_SEGMENTS;
    // Interpolate in screen space; width and ordering use the segment end depth.
    for (segmentIndex = 0; segmentIndex < GOLEM_KNIGHT_BISHOP_BEAM_SEGMENTS; segmentIndex++) {
        beam->depth = beam->depthStep * (segmentIndex + 1) + work->beamDepth[0];
        if (beam->depth < GOLEM_KNIGHT_BISHOP_BEAM_MIN_DEPTH) {
            continue;
        }
        beam->vertexX[0] = work->beamScreenX[0] + beam->stepX * segmentIndex;
        beam->vertexX[1] = work->beamScreenX[0] + beam->stepX * (segmentIndex + 1);
        beam->vertexX[2] = beam->vertexX[0] + ((-(beam->direction.vy * 0x600) >> 12) / beam->depth);
        beam->vertexX[3] = beam->vertexX[1] + ((-(beam->direction.vy * 0x600) >> 12) / beam->depth);
        beam->vertexX[4] = beam->vertexX[0] + (((beam->direction.vy * 3) >> 3) / beam->depth);
        beam->vertexX[5] = beam->vertexX[1] + (((beam->direction.vy * 3) >> 3) / beam->depth);
        beam->vertexY[0] = work->beamScreenY[0] + beam->stepY * segmentIndex;
        beam->vertexY[1] = work->beamScreenY[0] + beam->stepY * (segmentIndex + 1);
        beam->vertexY[2] = beam->vertexY[0] + ((-(beam->direction.vx * 0x600) >> 12) / beam->depth);
        beam->vertexY[3] = beam->vertexY[1] + ((-(beam->direction.vx * 0x600) >> 12) / beam->depth);
        beam->vertexY[4] = beam->vertexY[0] + (((beam->direction.vx * 3) >> 3) / beam->depth);
        beam->vertexY[5] = beam->vertexY[1] + (((beam->direction.vx * 3) >> 3) / beam->depth);
        _golemKnightBishopEmitAimBeamSegment(beam);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemKnightBishopAimBeamScratch);
}

/// Restarts changed animation requests with a blend, or ticks the active clip.
///
/// `task` owns the initialized nineteen-part rig and its retained animation state.
/// Clip IDs index the carrier's 22-entry blend table; slots 1 through 18 are
/// the animated parts and slot 0 is the root. A restart resets `animFrame`;
/// otherwise it increments once before ticking the slots.
static inline void _golemKnightBishopTickAnimInline(Task* task)
{
    GolemKnightBishopWork* work;
    s32                    slotIndex;
    s32                    blendFrames;

    work = task->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        blendFrames       = _gGolemKnightBishopAnimBlendFrames[work->anim];
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->anim, 0, blendFrames);
        }
    } else {
        work->animFrame++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

/// Draws a ground shadow below part 3 at the root's composed world height.
///
/// `task` owns a live GOLEM model/work block with current composed coordinates.
/// The half-side is 768 world units. Zero shade is changed to the negative
/// no-shadow sentinel before drawing; this also updates the work block.
static inline void _golemKnightBishopDrawShadowInline(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_SHADOW_HALF_SIZE = 768,
        GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN    = -1,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    GfxCoord*              shadowPart;
    VECTOR3                shadowCentre;

    work       = task->work;
    root       = &task->extra.tmd->coords[0];
    shadowPart = &task->extra.tmd->coords[3];
    if (work->shadowShade == 0) {
        work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
    }
    shadowCentre.vx = shadowPart->workm.t[0];
    shadowCentre.vy = root->workm.t[1];
    shadowCentre.vz = shadowPart->workm.t[2];
    effectDrawGroundShadow(&shadowCentre, GOLEM_KNIGHT_BISHOP_SHADOW_HALF_SIZE, work->shadowShade);
}

/// Samples room lighting at the root and applies a pending ambient-colour tint.
///
/// Requires a live GOLEM model/work block, Enemy spawn argument and current
/// composed root position. Dim blue sets ambient RGB to (0, 0, 1024); white
/// sets it to (4095, 4095, 4095), in Q12 channel units. Recognized requests are
/// cleared after application; other values leave the room lighting in effect.
static inline void _golemKnightBishopUpdateTintInline(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_TINT_NONE        = 0,
        GOLEM_KNIGHT_BISHOP_TINT_DIM_BLUE    = 1,
        GOLEM_KNIGHT_BISHOP_TINT_BLUE_LEVEL  = 1024,
        GOLEM_KNIGHT_BISHOP_TINT_WHITE_LEVEL = 4095,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    VECTOR                 worldPosition;
    s16                    red;
    s16                    green;
    s16                    blue;

    root             = task->extra.tmd->coords;
    work             = task->work;
    worldPosition.vx = root->workm.t[0];
    worldPosition.vy = root->workm.t[1];
    worldPosition.vz = root->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &worldPosition, 0, 0);
    switch (work->tintRequest) {
        case GOLEM_KNIGHT_BISHOP_TINT_DIM_BLUE:
            red   = 0;
            green = 0;
            blue  = GOLEM_KNIGHT_BISHOP_TINT_BLUE_LEVEL;
            worldCoordSetModelAmbientColor(task->extra.tmd, red, green, blue);
            work->tintRequest = GOLEM_KNIGHT_BISHOP_TINT_NONE;
            break;
        case GOLEM_KNIGHT_BISHOP_TINT_WHITE:
            red   = GOLEM_KNIGHT_BISHOP_TINT_WHITE_LEVEL;
            green = GOLEM_KNIGHT_BISHOP_TINT_WHITE_LEVEL;
            blue  = GOLEM_KNIGHT_BISHOP_TINT_WHITE_LEVEL;
            worldCoordSetModelAmbientColor(task->extra.tmd, red, green, blue);
            work->tintRequest = GOLEM_KNIGHT_BISHOP_TINT_NONE;
            break;
        case GOLEM_KNIGHT_BISHOP_TINT_NONE:
        default:
            return;
    }
}

#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Refreshes a corpse's root composition, room lighting/tint and ground shadow.
///
/// `root` is coordinate 0 of `task`'s live model; its work, enemy spawn argument,
/// ancestors and lighting/scratch/GTE state must remain initialized. Marks root
/// and part 3 dirty, composes only the root, then updates tint and draws from
/// part 3's cached X/Z with the root's cached Y. A pending tint may be consumed
/// and zero shadow shade becomes the no-shadow sentinel. Advances no animation,
/// allocates no retained resource and borrows both pointers only for the call.
static inline void _golemKnightBishopRefreshCorpsePresentation(Task* task, GfxCoord* root)
{
    root->composeStamp                      = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(root);
    _golemKnightBishopUpdateTintInline(task);
    _golemKnightBishopDrawShadowInline(task);
}

/// Detaches a newly dead GOLEM or presents a corpse restored from its saved pose.
///
/// `enemy` belongs to `task`, whose GOLEM rig/model remain live. Entry step 0
/// unlinks targeting and the ground, hurt and strike bodies, releases battle
/// rewards and saves the downed pose. Step 1 emits a flash every fourth
/// animation frame at local Y offsets -511..0. Restored corpses enter step 2,
/// capture/fade once and remain at step 3 without flashes. Grab release and
/// animation continue in running mode, followed by composition, tint and shadow.
///
/// Paused mode refreshes presentation only; hidden mode suppresses drawing.
/// Both early exits retain one `SVECTOR` scratch reservation until the next
/// main-loop scratch reset, as in the original; the running path releases it.
static void _golemKnightBishopDeadState(Enemy* enemy, Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_CORPSE_DETACH            = 0,
        GOLEM_KNIGHT_BISHOP_CORPSE_FLASHES           = 1,
        GOLEM_KNIGHT_BISHOP_CORPSE_RESTORE           = 2,
        GOLEM_KNIGHT_BISHOP_CORPSE_RESTORED          = 3,
        GOLEM_KNIGHT_BISHOP_CORPSE_FLASH_FRAME_MASK  = 3,
        GOLEM_KNIGHT_BISHOP_CORPSE_FLASH_OFFSET_MASK = 511,
        GOLEM_KNIGHT_BISHOP_CORPSE_FLASH_SIZE        = 1024,
    };
    SVECTOR*               flashOffset;
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    u32                    randomDraw;
    s16                    lyingAnimation;

    work        = task->work;
    root        = &task->extra.tmd->coords[0];
    flashOffset = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            task->extra.tmd->flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _golemKnightBishopRefreshCorpsePresentation(task, root);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
    switch (work->step) {
        case GOLEM_KNIGHT_BISHOP_CORPSE_DETACH:
            // Retire combat participation before saving the persistent corpse pose.
            enemy->recs = NULL;
            worldTargetUnlinkNode(&enemy->node);
            worldCollisionUnlinkBody(&work->groundBody);
            worldCollisionUnlinkBody(&work->hurtBody);
            worldCollisionUnlinkBody(&work->strikeBody);
            sceneReleaseBattleRefWithRewards(task, work->actorId);
            lyingAnimation = GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT;
            if (work->downedPose == GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND) {
                lyingAnimation = GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND;
            }
            work->anim        = lyingAnimation;
            work->step        = GOLEM_KNIGHT_BISHOP_CORPSE_FLASHES;
            enemy->spawnState = work->downedPose;
            areaSaveEnemyPose(enemy);
            break;
        case GOLEM_KNIGHT_BISHOP_CORPSE_FLASHES:
            if (!(work->animFrame & GOLEM_KNIGHT_BISHOP_CORPSE_FLASH_FRAME_MASK)) {
                flashOffset->vx = 0;
                flashOffset->vz = 0;
                randomDraw      = RANDOM_LCG_NEXT(gRandomLcgState);
                flashOffset->vy = -((randomDraw >> 16) & GOLEM_KNIGHT_BISHOP_CORPSE_FLASH_OFFSET_MASK);
                gRandomLcgState = randomDraw;
                effectSpawn(EFFECT_FLASH_BURST, &task->extra.tmd->coords[3], GOLEM_KNIGHT_BISHOP_CORPSE_FLASH_SIZE, flashOffset);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_CORPSE_RESTORE:
            _golemKnightBishopQueueFrameCapture(&task->extra.tmd->coords[3], GOLEM_KNIGHT_BISHOP_FRAME_CAPTURE_BIAS);
            _golemKnightBishopUpdateAppearance(task);
            modelLightingSetLayerMaterials(work->translucency);
            work->step = GOLEM_KNIGHT_BISHOP_CORPSE_RESTORED;
            break;
    }
    _golemKnightBishopTickGrabRelease(task);
    _golemKnightBishopTickAnimInline(task);
    _golemKnightBishopRefreshCorpsePresentation(task, root);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
