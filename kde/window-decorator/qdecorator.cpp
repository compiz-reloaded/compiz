#include "qdecorator.h"

#include "qwmutils.h"
#include "qwmscreen.h"

#include <QtDebug>
#include <QX11Info>

#include <X11/Xlib.h>
#include <X11/X.h>

Atom net_frame_window;
Atom net_window_decor;
Atom net_window_decor_sync;
Atom net_wm_moveresize;
Atom net_active_window;

static void initAtoms()
{

    Display *xdisplay     = QX11Info::display();

    net_frame_window     = XInternAtom(xdisplay, "_NET_FRAME_WINDOW", false);
    net_window_decor	  = XInternAtom(xdisplay, "_NET_WINDOW_DECOR", false);
    net_window_decor_sync   = XInternAtom(xdisplay, "_NET_WINDOW_DECOR_SYNC", false);
    net_wm_moveresize   = XInternAtom(xdisplay, "_NET_WM_MOVERESIZE", false);
    net_active_window = XInternAtom(xdisplay, "_NET_ACTIVE_WINDOW", false);

}

QDecorator::QDecorator(int &argc, char **argv)
    : QApplication(argc, argv)
{
    Display *xdisplay   = QX11Info::display();

    initAtoms();

    m_screen = new QWMScreen(DefaultScreen(xdisplay));
    XSelectInput(xdisplay, m_screen->rootWindow(),
                 SubstructureNotifyMask | ExposureMask |
                 StructureNotifyMask    | PropertyChangeMask);
}

QDecorator::~QDecorator()
{

}

bool QDecorator::x11EventFilter(XEvent *xevent)
{
    long        xid = 0;
    //qDebug()<<"event filter in decorator";
    switch (xevent->type) {
    case ButtonPress:
    case ButtonRelease:
        qDebug()<<"button";
	xid = frameTable[xevent->xbutton.window];
	break;
    case EnterNotify:
    case LeaveNotify:
        qDebug()<<"enter/leave";
	xid = frameTable[xevent->xcrossing.window];
	break;
    case MotionNotify:
        qDebug()<<"motion";
	xid = frameTable[xevent->xmotion.window];
	break;
    case PropertyNotify:
        qDebug()<<"property";
#if 0
	if (xevent->xproperty.atom == frame_window_atom)
	{
	    WnckWindow *win;

	    xid = xevent->xproperty.window;

	    win = wnck_window_get (xid);
	    if (win)
	    {
		Window frame;

		if (get_window_prop (xid, frame_window_atom, &frame))
		    add_frame_window (win, frame);
		else
		    remove_frame_window (win);
	    }
	}
#endif
	break;
    case ConfigureNotify:
    {
        qDebug()<<"configure notify";
        break;
    }
    break;
    case DestroyNotify:
        qDebug()<<"destroy";
        m_screen->destroyNotify(reinterpret_cast<XDestroyWindowEvent*>(xevent));
        break;
    case Expose:
        qDebug()<<"expose";
        break;
    case ClientMessage:
#ifdef HAVE_STARTUP_NOTIFICATION && 0
        sn_display_process_event(_wnck_screen_get_sn_display (s),
                                 xevent);
#endif /* HAVE_STARTUP_NOTIFICATION */
        break;
    case MapNotify:
        m_screen->mapNotify(reinterpret_cast<XMapEvent*>(xevent));
        break;
    case CreateNotify:
        qDebug()<<"create notify";
        break;
    default:
        break;
    }

#if 0
    if (xid)
    {
	QWnWindow *win = screen->window(xid);
	if (win)
	{
	    static event_callback callback[3][3] = {
		{ top_left_event,    top_event,    top_right_event    },
		{ left_event,	     title_event,  right_event	      },
		{ bottom_left_event, bottom_event, bottom_right_event }
	    };
	    static event_callback button_callback[3] = {
		close_button_event,
		max_button_event,
		min_button_event
	    };
	    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

	    if (d->decorated)
	    {
		gint i, j;

		for (i = 0; i < 3; i++)
		    for (j = 0; j < 3; j++)
			if (d->event_windows[i][j] == xevent->xany.window)
			    (*callback[i][j]) (win, xevent);

		for (i = 0; i < 3; i++)
		    if (d->button_windows[i] == xevent->xany.window)
			(*button_callback[i]) (win, xevent);
	    }
	}
    }
#endif
    return QApplication::x11EventFilter(xevent);
}

#include "qdecorator.moc"
