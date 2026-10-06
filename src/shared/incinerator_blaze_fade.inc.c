/* Part of the incinerator blaze library; see incinerator_blaze.h. */

/// Fade-to-white driver of the encounter, six states over the eight-byte
/// channel block it allocates into its own `Task::work` and hands the parent
/// work block through `Task::spawnArg2`.
///
/// State 0 allocates the ramp, zeroes the three channels and parks the
/// message record `gBlazeFadeMessages` in `Task::msgTable`. States 2 and
/// 3 step `r` -- the first by 0xA up to 0x50, the second by 1 up to
/// 0xFF -- and each hands the state machine back to 1 when it clamps, so the
/// two ramps run back to back. State 4 steps `g` / `b` by 8; once
/// `g` passes 0xFF the framebuffer environments are reconfigured, `Fs_ImgBuffers` is
/// filled white, the parent work block's wave ramp is sent to
/// `SCREEN_WAVE_RAMP_FINISHED`, and state 5 draws the full-screen white
/// `TILE` + `DR_TPAGE` packed into `gGpuPrimCursor` before returning without
/// the fade call. Every other state
/// -- 1, 6 and up -- only draws the fade.
void blazeFadeTask(Task* arg0)
{
    ScreenFadeWork*  work;
    ScreenFadeWork*  alloc;
    BlazeParentWork* parent;
    TILE*            tile;
    DR_TPAGE*        dr;

    work = arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = memMalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work           = alloc;
            work->b        = 0;
            work->g        = 0;
            work->r        = 0;
            arg0->msgTable = gBlazeFadeMessages;
            arg0->state   += 1;
            break;
        case 2:
            work->r += 0xA;
            if (work->r >= 0x51) {
                work->r     = 0x50;
                arg0->state = 1;
            }
            break;
        case 3:
            work->r += 1;
            if (work->r >= 0x100) {
                work->r     = 0xFF;
                arg0->state = 1;
            }
            break;
        case 4:
            work->g += 8;
            work->b += 8;
            if (work->g >= 0x100) {
                parent             = (BlazeParentWork*)((Task*)arg0->spawnArg2.pointer)->work;
                parent->wave.state = SCREEN_WAVE_RAMP_FINISHED;
                displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
                memFillBytes(Fs_ImgBuffers, 0xFF, sizeof(*Fs_ImgBuffers));
                work->b     = 0xFF;
                work->g     = 0xFF;
                arg0->state = 5;
            }
            break;
        case 5:
            tile           = gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            setlen(tile, 3);
            setcode(tile, 0x60);
            tile->r0 = 0xFF;
            tile->g0 = 0xFF;
            tile->b0 = 0xFF;
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            addPrim(gGpuCurrentOt - 16, tile);

            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000200;
            addPrim(gGpuCurrentOt - 16, dr);
            return;
    }
    fadeDrawOverlay(work->r, work->g, work->b, GPU_BLEND_ADD);
}
