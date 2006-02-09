#include "qwmutils.h"

#include <QtDebug>

#include <X11/Xlib.h>
#include <X11/X.h>

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

bool QWM::eventFilter(void *message, long *result)
{
    XEvent *xevent = reinterpret_cast<XEvent*>(message);

    switch (xevent->type)
    {
    case PropertyNotify:
    {
        qDebug()<<"property notify";
        return true;
    }
    break;

    case ConfigureNotify:
    {
        qDebug()<<"configure notify";
        return true;
    }
    break;

    case SelectionClear:
    {
        qDebug()<<"selection clear";
        return true;
    }
    break;

    case ClientMessage:
#ifdef HAVE_STARTUP_NOTIFICATION && 0
        sn_display_process_event (_wnck_screen_get_sn_display (s),
                                  xevent);
#endif /* HAVE_STARTUP_NOTIFICATION */
        return true;
    }

    return false;
}
