#ifndef GAMEPLAY_GPU_IMAGE_UPLOAD_H
#define GAMEPLAY_GPU_IMAGE_UPLOAD_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

/// Copy the record's packed RAM words to its VRAM destination.
enum { GPU_IMAGE_UPLOAD_COPY = 0 };

/// One entry in a terminated list of raw texture or palette uploads.
///
/// `Gp_LoadImages` requires a non-NULL list with an accessible terminator:
/// every nonzero `operation` ends the walk, and assets use `GP_IMG_REC_END`
/// (255). The terminator's remaining fields are ignored.
///
/// `destination.x` and `destination.w` count 16-bit VRAM words;
/// `destination.y` and `destination.h` count rows. `pixels` borrows a
/// word-aligned buffer holding two VRAM words per `u_long`, with an odd
/// rectangle area rounded up to a whole `u_long`. Keep that storage valid and
/// unchanged until GPU transfer completes. The SDK copies the rectangle when
/// queuing the transfer, but retains the payload pointer.
///
/// Actor texture uploads rewrite the first entry's destination before loading
/// the list, so those records must be writable.
typedef struct {
    u16     operation;   // 0 copy, 255 end; every other nonzero value also ends the list
    u16     unknown_2;   // Zero in the supplied lists; role unproven
    RECT    destination; // VRAM destination: X/width in 16-bit words, Y/height in rows
    u_long* pixels;      // Borrowed packed texture or palette words, read by the GPU upload
} GpuImageUpload;
STATIC_ASSERT_SIZEOF(GpuImageUpload, 0x10);

#endif // GAMEPLAY_GPU_IMAGE_UPLOAD_H
