#include "qdecorator.h"

#include "qwmutils.h"

#include <QtDebug>
#include <QX11Info>

#include <X11/Xlib.h>
#include <X11/X.h>

static Atom frame_window_atom;
static Atom win_decor_atom;
static Atom win_decor_sync_atom;
static Atom wm_move_resize_atom;

QDecorator::QDecorator(int &argc, char **argv)
    : QApplication(argc, argv)
{
    Display *xdisplay   = QX11Info::display();
    frame_window_atom	= XInternAtom(xdisplay, "_NET_FRAME_WINDOW", false);
    win_decor_atom	= XInternAtom(xdisplay, "_NET_WINDOW_DECOR", false);
    //win_decor_sync_atom	= XInternAtom (xdisplay, "_NET_WINDOW_DECOR_SYNC", false);
    wm_move_resize_atom = XInternAtom(xdisplay, "_NET_WM_MOVERESIZE", false);


    XSelectInput(xdisplay,
                 RootWindow(xdisplay, DefaultScreen(xdisplay)),
                 SubstructureNotifyMask | ExposureMask |
                 StructureNotifyMask | PropertyChangeMask);

    setEventFilter(QWM::eventFilter);
}

QDecorator::~QDecorator()
{

}

bool QDecorator::x11EventFilter(XEvent *xevent)
{
    long        xid = 0;
    qDebug()<<"event filter in decorator";
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
    case DestroyNotify:
        qDebug()<<"destroy";
	frameTable.remove(xevent->xproperty.window);
    default:
	break;
    }
#if 0
    if (xid)
    {
	WnckWindow *win;

	win = wnck_window_get (xid);
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
