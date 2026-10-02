/* Part of the glow drawing library; see glow_draw.h. */

/// Draws a glowing disc at `worldPoint`: the point is projected and, when it lies
/// past the near limit, four gouraud wedges are queued around it with a radius
/// of `radiusScale * 64` over its depth. The centre colour takes one bit per channel
/// from `packedColor` (red from its high byte), scaled by 0x20 or 0x28 as the frame
/// counter alternates.
///
/// `worldPoint` uses world coordinates; `radiusScale` is narrowed to signed 16 bits
/// before division by camera depth/4. Angles use 4096 units per turn and the
/// trigonometric coordinates use a 12-bit fractional scale. Colour bytes wrap.
void glowDrawBitDisc(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor)
{
    enum {
        ROOM_VISUAL_EFFECTS_GLOW_MIN_DEPTH       = 17,
        ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_BASE = 0x20,
        ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_STEP = 8,
        ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT      = 12,
        ROOM_VISUAL_EFFECTS_GLOW_FULL_TURN       = 0x1000,
    };

    u8*                      head;
    GlowCentreRadiusScratch* block;
    POLY_G4*                 prim;
    DisplayState*            displayBase;
    DisplayState*            ds;
    s32                      radius;
    s32                      angle;
    s32                      halfStepAngle;
    s32                      nextAngle;
    s32                      shiftedColor;
    u8                       brightness;
    u8                       r;
    u8                       g;
    u8                       b;

    {
        void** scratch;
        u8*    tmp;

        scratch = SCRATCH_STACK_CURSOR_SLOT;
        head    = *scratch;
        tmp     = (*scratch = head - sizeof(*block));
        block   = (GlowCentreRadiusScratch*)tmp;
    }

    // Project the world point before allocating its glow packets.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&((GlowCentreRadiusScratch*)(head - sizeof(*block)))->sx);
    gte_stszotz(&block->otz);
    if (((GlowCentreRadiusScratch*)(head - sizeof(*block)))->otz >= ROOM_VISUAL_EFFECTS_GLOW_MIN_DEPTH) {
        radius        = ((s16)radiusScale * 64) / ((GlowCentreRadiusScratch*)(head - sizeof(*block)))->otz;
        displayBase   = &gDisplayState;
        shiftedColor  = packedColor << 16;
        brightness    = (((u8)displayBase->animFrame & 1) * ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_STEP) | ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_BASE;
        r             = brightness * (shiftedColor >> 24);
        g             = brightness * ((shiftedColor >> 20) & 1);
        b             = brightness * (packedColor & 1);
        angle         = 0;
        ds            = displayBase;
        block->radius = radius;
        // Build four glow wedges and quantize their shared camera depth.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0      = block->sx + ((block->radius * rsin(angle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            halfStepAngle = angle + 0x200;
            prim->y0      = block->sy + ((block->radius * rcos(angle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->x1      = block->sx + ((block->radius * rsin(halfStepAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->y1      = block->sy + ((block->radius * rcos(halfStepAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            nextAngle     = angle + 0x400;
            prim->x2      = block->sx;
            prim->y2      = block->sy;
            prim->x3      = block->sx + ((block->radius * rsin(nextAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->y3      = block->sy + ((block->radius * rcos(nextAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            angle         = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (angle < ROOM_VISUAL_EFFECTS_GLOW_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(*block));
}
