/* Part of the screen fade library; see screen_fade.h. */

/// `screenFadeInTask` with the overlay drawn in place: the same subtractive
/// ramp from white, but each frame links its own semi-transparent full-screen
/// `TILE` (-0xA0,-0x78, 0x140 by 0xF0) and the `0xE1000240` `DR_TPAGE` into
/// `gGpuCurrentOt[-16]`, tinted `r`/`g`/`r`, instead of calling
/// `fadeDrawOverlay`. State 0 allocates the `ScreenFadeWork` at `Task::work`
/// with all three channels at 0xFF (a failed allocation kills the task); state 1
/// steps them down by `Task::spawnArg1` and kills the task once `r` is below 0.
void screenFadeInTileTask(Task* arg0)
{
    ScreenFadeWork* fade;
    ScreenFadeWork* alloc;
    u8              r;
    u8              g;
    TILE*           tile;
    DR_TPAGE*       dr;

    fade = arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = memMalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                goto kill;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            r              = fade->r;
            g              = fade->g;
            tile           = gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            setlen(tile, 3);
            setcode(tile, 0x62);
            tile->r0 = r;
            tile->g0 = g;
            tile->b0 = r;
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            addPrim(gGpuCurrentOt - 16, tile);

            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
            addPrim(gGpuCurrentOt - 16, dr);

            fade->r -= (u16)arg0->spawnArg1.value;
            fade->g -= (u16)arg0->spawnArg1.value;
            fade->b -= (u16)arg0->spawnArg1.value;
            if (fade->r < 0) {
            kill:
                taskKill(arg0);
            }
            break;
    }
}
