#ifndef QWMUTILS_H
#define QWMUTILS_H

namespace QWM
{
    void trapXError(void);
    int  popXError(void);
    bool eventFilter(void *message, long *result);
}

#endif
