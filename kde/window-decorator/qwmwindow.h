#ifndef QWMWINDOW_H
#define QWMWINDOW_H

#include <QPixmap>
#include <QString>
#include <QRect>

class QWidget;

enum QWMState
{};
enum QWMActions
{};

class QWMWindow
{
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

protected:
    bool fetchFrame();

private:
    long   m_xid;
    QRect  m_geometry;
    long   m_frame;
};

#endif
