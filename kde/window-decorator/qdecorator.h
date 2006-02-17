#ifndef QDECORATOR_H
#define QDECORATOR_H

#include <QApplication>
#include <QHash>

#include <X11/Xdefs.h>

class QWMScreen;

class QDecorator : public QApplication
{
    Q_OBJECT
public:
    QDecorator(int &argc, char **argv);
    ~QDecorator();

    bool x11EventFilter(XEvent *);

private:
    QHash<long, long> frameTable;
    QWMScreen *m_screen;
};

#endif
