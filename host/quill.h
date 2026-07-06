#pragma once
// C ABI over the vendor e-ink engine (libqsgepaper), exposed by libquill.so.
// See riddle/quill/src/quill_c.cpp for the reference implementation; the host
// builds its own variant (quill_host.cpp) that reuses the existing Qt app.

extern "C" {
int quill_init();
int quill_width();
int quill_height();
int quill_stride();
int quill_format();          // QImage::Format of the aux framebuffer
unsigned char* quill_buffer();
// Push a region to glass. mode: 0=fastest(DU-ish) 1=fast 3=medium 4=full-quality.
// full_refresh != 0 forces a flashing clear of the region.
unsigned long quill_swap(int x, int y, int w, int h, int mode, int full_refresh);
void quill_process_events();
}
