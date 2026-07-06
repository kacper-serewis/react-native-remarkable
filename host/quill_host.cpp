// quill_host: C ABI over the vendor e-ink engine (libqsgepaper's
// EPFramebuffer, accessed via asivery's epfb-re shim in riddle/quill).
//
// Adapted from riddle/quill/src/quill_c.cpp with one difference: rn-layout
// already runs a QGuiApplication (QPainter needs a font database), so this
// version reuses the existing app instead of creating a QCoreApplication.
// Runs with xochitl STOPPED — this process becomes the display driver.

#include "epframebuffer.h"
#include <QCoreApplication>
#include <QImage>
#include <cstdio>

static EPFramebuffer *g_fb = nullptr;
static QImage *g_aux = nullptr;

extern "C" {

// Returns 0 on success. The host must have created its Q(Gui)Application
// before calling this. After this, quill_buffer()/quill_swap() are usable.
int quill_init() {
    if (g_fb) return 0;
    if (!QCoreApplication::instance()) {
        fprintf(stderr, "quill: no Qt application instance\n");
        return 3;
    }
    g_fb = EPFramebuffer::createControlledInstance();
    if (!g_fb) return 1;
    g_aux = g_fb->getAuxFramebuffer();
    if (!g_aux) return 2;
    fprintf(stderr, "quill: aux framebuffer %dx%d format=%d bpl=%lld\n",
            g_aux->width(), g_aux->height(), (int)g_aux->format(),
            (long long)g_aux->bytesPerLine());
    return 0;
}

// Geometry of the drawing buffer.
int quill_width()  { return g_aux ? g_aux->width() : 0; }
int quill_height() { return g_aux ? g_aux->height() : 0; }
int quill_stride() { return g_aux ? (int)g_aux->bytesPerLine() : 0; }
int quill_format() { return g_aux ? (int)g_aux->format() : -1; }

// Direct pointer into the aux framebuffer pixels.
unsigned char *quill_buffer() {
    return g_aux ? g_aux->bits() : nullptr;
}

// Push a region to glass. mode: 0=fastest(DU-ish) 1=fast 3=medium 4=full-quality.
// full_refresh != 0 forces a flashing clear of the region.
unsigned long quill_swap(int x, int y, int w, int h, int mode, int full_refresh) {
    if (!g_fb) return 0;
    QFlags<EPFramebuffer::UpdateFlag> flags = full_refresh
        ? EPFramebuffer::UpdateFlag::CompleteRefresh
        : EPFramebuffer::UpdateFlag::NoRefresh;
    return g_fb->swapBuffers(QRect(x, y, w, h), EPContentType::Mono,
                             (EPScreenMode)mode, flags);
}

void quill_process_events() {
    if (QCoreApplication::instance()) QCoreApplication::processEvents();
}

} // extern "C"
