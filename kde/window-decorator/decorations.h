#ifndef DECORATIONS_H
#define DECORATIONS_H

#include "qwmwindow.h"

#include <QPixmap>
#include <QString>
#include <QPainter>
#include <QtDebug>

#include <X11/Xlib.h>

class Decorations
{
public:
    Decorations();

    void bind(QWMWindow *win, long frame);
    void update();

    void draw(QPainter *p);
private:
    Window	       event_windows[3][3];
    Window	       button_windows[3];
    uint	       button_states[3];
    QPixmap	       pixmap;
    QPixmap	       buffer_pixmap;
    int	               width;
    int	               height;
    bool	       decorated;
    bool	       active;
    QString	       name;
    QBrush             icon;
    QPixmap	       icon_pixmap;
    //WnckWindowState    state;
    //WnckWindowActions  actions;
    long	       prop_xid;
};

#endif
