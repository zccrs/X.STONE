// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QColor>
#include <QRect>
#include <QImage>

QT_BEGIN_NAMESPACE
class QFbVtHandler;
class QPainter;
QT_END_NAMESPACE

class Node : public QObject
{
    friend class Compositor;
    Q_OBJECT
    Q_PROPERTY(QRect geometry READ geometry WRITE setGeometry NOTIFY geometryChanged FINAL)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)

public:
    explicit Node(Node *parent = nullptr);

    QRect rect() const;
    QRect geometry() const;
    void setGeometry(const QRect &newGeometry);

    bool isVisible() const;
    void setVisible(bool newVisible);

signals:
    void geometryChanged(QRect newGeometry);
    void updateRequest(QRegion region);
    void visibleChanged(bool newVisible);

protected:
    virtual void paint(QPainter *pa);
    void addChild(Node *child);
    void removeChild(Node *child);

private:
    QRect m_geometry = QRect(0, 0, 100, 100);
    QList<Node*> m_orderedChildren;
    bool m_visible = false;
};

class Window : public Node
{
    Q_OBJECT
    Q_PROPERTY(State state READ state WRITE setState NOTIFY stateChanged FINAL)

public:
    enum State {
        Normal,
        Maximized
    };
    Q_ENUM(State)

    explicit Window(Node *parent = nullptr);
    Window::State state() const;
    void setState(State newState);

signals:
    void stateChanged();

private:
    void paint(QPainter *pa) override;

    State m_state;
};

class Cursor : public Node
{
    Q_OBJECT
public:
    inline static int zOrder = 999;

    explicit Cursor(Node *parent = nullptr);

    void move(const QPoint &pos);

private:
    void paint(QPainter *pa);

    QImage m_image;
};

class Input;
class Output;
class Compositor : public QObject
{
    friend class InputEventManager;
    Q_OBJECT
    Q_PROPERTY(QColor background READ background WRITE setBackground NOTIFY backgroundChanged FINAL)

public:
    explicit Compositor(QObject *parent = nullptr);
    ~Compositor();

    void start();

    QColor background() const;
    void setBackground(const QColor &newBackground);

    void setWallpaper(const QImage &image);

    void markDirty(const QRegion &region);

    void addWindow(Window *window);
    void removeWindow(Window *window);

signals:
    void backgroundChanged();

private:
    void paint(const QRegion &region);
    void paint();

    QFbVtHandler *m_vtHandler = nullptr;
    Input *m_input = nullptr;
    QList<Output*> m_outputs;

    bool m_painting = false;
    QColor m_background;
    QImage m_wallpaper;
    QImage m_wallpaperWithPrimaryOutput;

    Node *m_rootNode = nullptr;
    Cursor *m_cursorNode = nullptr;
};
