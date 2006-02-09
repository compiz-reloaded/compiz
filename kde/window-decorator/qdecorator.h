#ifndef QDECORATOR_H
#define QDECORATOR_H

#include <QApplication>
#include <qhash.h>

class QDecorator : public QApplication
{
    Q_OBJECT
public:
    QDecorator(int &argc, char **argv);
    ~QDecorator();

    bool x11EventFilter(XEvent *);

private:
    QHash<long, long> frameTable;

};

#endif
