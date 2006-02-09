#ifndef QWMWINDOW_H
#define QWMWINDOW_H

#include <QObject>
#include <QPixmap>
#include <QString>
#include <QRect>

class QWidget;

enum QWMState
{};
enum QWMActions
{};

class QWMWindow : public QObject
{
    Q_OBJECT
public:
    QWMWindow(long xid);

    long xid() const;

    bool isActive() const;
    bool isMaximized() const;

    QString name() const;
    QPixmap miniIcon() const;

    QWMState state() const;
    QWMActions actions() const;

    QRect geometry() const;

    void close(long time);
    void maximize();
    void unmaximize();
    void minimize();

    QWidget *createActionMenu();
};

#endif
