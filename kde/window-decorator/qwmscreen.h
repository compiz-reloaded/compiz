#ifndef QWMSCREEN_H
#define QWMSCREEN_H

class QWMScreen
{
public:
    QWMScreen();

    QWMWindow previouslyActiveWindow() const;
    QWMWindow activeWindow() const;
    QList<QWMWindow> windows() const;
};

#endif
