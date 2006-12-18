// taken from KWin

#ifndef KWIN_INTERFACE_H
#define KWIN_INTERFACE_H

#include <dcopobject.h>

class KWinInterface:virtual public DCOPObject
{
    K_DCOP
    
    k_dcop:
    
    virtual ASYNC reconfigure () = 0;
};

#endif
