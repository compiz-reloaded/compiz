#include "qwmscreen.h"

#include "qwmwindow.h"
#include "qwmutils.h"

#include <QtGui>
#include <QX11Info>
#include <QtDebug>

#include <X11/Xlib.h>

extern Atom net_frame_window;
extern Atom net_window_decor;
extern Atom net_window_decor_sync;
extern Atom net_wm_moveresize;
extern Atom net_active_window;

QWMScreen::QWMScreen(int screen)
    : m_number(screen)
{
    initWindows();
}


QList<QWMWindow*> QWMScreen::windows() const
{
    return m_windows.values();
}

long QWMScreen::rootWindow() const
{
    return RootWindow(QX11Info::display(), m_number);
}

void QWMScreen::mapNotify(XMapEvent *ev)
{
    qDebug()<<"map notify"<<ev->event<<" and "<<ev->window
            <<", override = "<<ev->override_redirect;

    if (ev->override_redirect) {
        return;
    }

    if (m_windows.contains((long)ev->window)) {
        qDebug()<<"Window "<<ev->window<<" already cought";
    } else {
        qDebug()<<"New window"<<ev->window;
        QWMWindow *win = new QWMWindow(ev->window);
        m_windows.insert(ev->window, win);
    }
}

void QWMScreen::destroyNotify(XDestroyWindowEvent *ev)
{
    m_windows.remove(ev->window);
}

void QWMScreen::initWindows()
{
    // Get the active window ID
    Window activeWindow = None;
    QWM::readWindowProperty(rootWindow(), net_active_window, (long*)&activeWindow);

    // Get a list of all toplevel windows from the X server, sorted bottom to top
    uint nwindows;
    Window root, parent, *windows;
    XQueryTree(QX11Info::display(), rootWindow(), &root, &parent, &windows, &nwindows);

    // Create a client object for each window and insert it into the client
    // list, which is sorted top to bottom. (the opposite of the order returned
    // by XQueryTree()).
    for (uint i = 0; i < nwindows; i++) {
        XWindowAttributes attr;
        if (!XGetWindowAttributes(QX11Info::display(), windows[i], &attr))
            continue;

        if (attr.override_redirect)
            continue;
        // Create the client object for the window
        QWMWindow *client = new QWMWindow(windows[i]);

        if (client->xid() == activeWindow)
            m_activeWindow = client;

        m_windows.insert(client->xid(), client);
    }

    XFree(windows);
}
