#include "qwmwindow.h"

#include "qdecorator.h"
#include "qwmutils.h"

#include <QtDebug>
#include <QX11Info>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

extern Atom net_frame_window;


QWMWindow::QWMWindow(long xid)
    : m_xid(xid)
{
    qDebug()<<"Window "<<m_xid;
    if (fetchFrame()) {
        qDebug()<<"\tFrame is cool" << m_frame;
    } else {
        qDebug()<<"\tBusted frame "<< m_frame;
    }
}


long QWMWindow::xid() const
{

}


bool QWMWindow::isActive() const
{

}


bool QWMWindow::isMaximized() const
{

}


QString QWMWindow::name() const
{

}


QPixmap QWMWindow::miniIcon() const
{

}


QWMState QWMWindow::state() const
{

}


QWMActions QWMWindow::actions() const
{

}


QRect QWMWindow::geometry() const
{

}


void QWMWindow::close(long time)
{

}


void QWMWindow::maximize()
{

}


void QWMWindow::unmaximize()
{

}


void QWMWindow::minimize()
{

}


QWidget * QWMWindow::createActionMenu()
{

}

bool QWMWindow::fetchFrame()
{
    Atom   type;
    int	   format;
    unsigned long nitems;
    unsigned long bytes_after;
    Window *w;
    int    err, result;

    m_frame = 0;

    QWM::trapXError();

    type = None;
    result = XGetWindowProperty(QX11Info::display(),
                                m_xid,
                                net_frame_window,
                                0, LONG_MAX,
                                False, XA_WINDOW, &type, &format, &nitems,
                                &bytes_after, (unsigned char**) &w);
    err = QWM::popXError();

    if (err || result != Success)
	return false;

    if (type != XA_WINDOW) {
	XFree(w);
	return false;
    }

    m_frame = *w;
    XFree(w);

    return true;
}
