#ifndef QWMSCREEN_H
#define QWMSCREEN_H

#include <QObject>
#include <QList>

class QWMWindow;

class QWMScreen : public QObject
{
    Q_OBJECT
public:
    QWMScreen();

    QWMWindow *previouslyActiveWindow() const;
    QWMWindow *activeWindow() const;
    QList<QWMWindow*> windows() const;
};

#endif
