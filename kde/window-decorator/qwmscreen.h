#ifndef QWMSCREEN_H
#define QWMSCREEN_H

#include <QtGui>
#include <QHash>
#include <QList>

#include <QtDebug>

#include <X11/Xlib.h>

class QWMWindow;

class QWMScreen
{
public:
    QWMScreen(int screen);

    long rootWindow() const;

    QList<QWMWindow*> windows() const;

public:
    void mapNotify(XMapEvent *ev);
    void destroyNotify(XDestroyWindowEvent *ev);

private:
    void initWindows();
private:
    int  m_number;
    long m_root;
    QHash<long, QWMWindow*> m_windows;
    QWMWindow *m_activeWindow;
};

#endif
