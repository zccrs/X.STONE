// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QColor>
#include <QRect>
#include <QImage>
#include <QPointer>

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
    Q_PROPERTY(int z READ z WRITE setZ NOTIFY zChanged FINAL)

public:
    explicit Node(Node *parent = nullptr);
    ~Node();

    QRect rect() const;
    QRect geometry() const;
    void setGeometry(const QRect &newGeometry);

    QRegion wholeGeometry() const;
    QRegion wholeRect() const;

    bool isVisible() const;
    void setVisible(bool newVisible);

    int z() const;
    void setZ(int newZ);

    void draw(QPainter *pa);

    Node *parentNode() const;
    Node *childAt(const QPoint &position) const;
    QPoint mapFromGlobal(const QPoint &position) const;
    QPoint mapToGlobal(const QPoint &position) const;

signals:
    void geometryChanged(QRect oldGeometry, QRect newGeometry);
    void visibleChanged(bool newVisible);
    void zChanged();

    void mousePressed(QPoint pos);

protected:
    virtual void paint(QPainter *pa);
    virtual void update(QRegion region, bool force = false);
    bool event(QEvent *event) override;

    void addChild(Node *child);
    void removeChild(Node *child);
    void sortChild(Node *child);

private:
    QRect m_geometry = QRect(0, 0, 100, 100);
    QPointer<Node> m_parent;
    QList<Node*> m_orderedChildren;
    bool m_visible = true;
    int m_z = 0;
};

class WindowTitleBar;
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
    void updateTitleBarGeometry();

    State m_state;
    WindowTitleBar *m_titlebar;
};

class Rectangle : public Node
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged FINAL)

public:
    explicit Rectangle(Node *parent = nullptr);

    QColor color() const;
    void setColor(const QColor &newColor);

signals:
    void colorChanged();

private:
    void paint(QPainter *pa) override;

private:
    QColor m_color;
};

class WindowTitleBar: public Node
{
    Q_OBJECT

public:
    explicit WindowTitleBar(Window *window);

    inline Window *window() {
        return static_cast<Window*>(parent());
    }

signals:
    void requestClose();
    void requestToggleMaximize();
    void requestMinimize();

private:
    void updateButtonGeometry();
    void paint(QPainter *pa) override;

    Rectangle *m_maximizeButton;
    Rectangle *m_minimizeButton;
    Rectangle *m_closeButton;
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

    QImage m_buffer;
    bool m_painting = false;
    QColor m_background;
    QImage m_wallpaper;
    QImage m_wallpaperWithPrimaryOutput;

    class RootNode : public Node
    {
    public:
        explicit RootNode(Compositor *compositor);

        inline Compositor *parent() {
            return static_cast<Compositor*>(Node::parent());
        }

        void update(QRegion region, bool) override {
            parent()->markDirty(region);
        }
    };

    Node *m_rootNode = nullptr;
    Cursor *m_cursorNode = nullptr;
    QPointer<Window> m_focusWindow;
};
