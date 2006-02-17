#include "qwmutils.h"

#include <QX11Info>
#include <QtDebug>

#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xatom.h>

static int trapped_error_code = 0;
static int (*old_error_handler) (Display *d, XErrorEvent *e);
static int xErrorHandler(Display  *, XErrorEvent *error)
{
    trapped_error_code = error->error_code;
    return 0;
}

namespace QWM
{

void trapXError(void)
{
    trapped_error_code = 0;
    old_error_handler = XSetErrorHandler(xErrorHandler);
}
int popXError(void)
{
    XSetErrorHandler(old_error_handler);
    return trapped_error_code;
}

}

void *readXProperty(Window window, Atom property, Atom type, int *items)
{
    long offset = 0, length = 2048l;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_remaining;
    unsigned char *data = 0l;

    int result = XGetWindowProperty(QX11Info::display(), window, property, offset, length,
                                    false, type,
                                     &actual_type, &actual_format, &nitems,
                                     &bytes_remaining, &data);

    if (result == Success && actual_type == type
         && actual_format == 32 && nitems > 0) {
        if (items)
            *items = nitems;

        return reinterpret_cast< void* >(data);
    }

    if (data)
        XFree(data);

    if (items)
        *items = 0;

    return NULL;
}

bool QWM::readWindowProperty(long window, long property, long *value)
{
    void *data = readXProperty(window, property, XA_WINDOW, NULL);

    if (data) {
        if (value)
            *value = *reinterpret_cast<int *>(data);
        XFree(data);

        return true;
    }

    return false;
}
