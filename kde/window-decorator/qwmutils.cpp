#include "qwmutils.h"

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
