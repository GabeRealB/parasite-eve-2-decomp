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
/// `gpuUploadImages` requires a non-NULL list with an accessible terminator:
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

/// Queues every raw texture or palette copy in a terminated upload list.
///
/// `uploadList` must be non-NULL and contain an accessible nonzero-operation
/// terminator. Only `GPU_IMAGE_UPLOAD_COPY` entries transfer data; any other
/// operation ends the walk. Each rectangle must fit VRAM and its pixel buffer
/// must meet `GpuImageUpload`'s alignment, extent and transfer-lifetime contract.
/// The records are read only and can be released after this call. Pixel data
/// stays borrowed until GPU transfer completes. SDK return values are ignored;
/// this does not wait for completion. Reserves one `RECT` on the scratch stack
/// and restores the cursor before returning.
void gpuUploadImages(const GpuImageUpload* uploadList);

#endif // GAMEPLAY_GPU_IMAGE_UPLOAD_H
