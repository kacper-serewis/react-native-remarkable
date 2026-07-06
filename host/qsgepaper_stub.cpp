// Link-time stub for the vendor e-ink engine. NEVER shipped: it exists only
// so libquill.so can link inside the build container without the device's
// libqsgepaper.so. The mangled symbol names come from the same declarations
// in riddle/quill/src/epframebuffer.h that the real library exports, so at
// runtime libquill's rpath (/usr/lib/plugins/scenegraph) resolves the real
// engine on the device instead of this stub.
#define EPFB_INTERNAL
#include "epframebuffer.h"
#include <cstdlib>

EPFramebuffer *EPFramebuffer::instance() { abort(); }

unsigned long EPFramebuffer::swapBuffers(QRect, EPContentType, EPScreenMode,
                                         QFlags<EPFramebuffer::UpdateFlag>) {
    abort();
}
