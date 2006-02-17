#ifndef QWMUTILS_H
#define QWMUTILS_H

namespace QWM
{
    void trapXError(void);
    int  popXError(void);
    bool eventFilter(void *message, long *result);
    bool readWindowProperty(long wId, long property, long *value);
}

#endif
