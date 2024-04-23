// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include "compositor.h"
#include "output.h"
#include "input.h"

#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QDebug>

#include <private/qfbvthandler_p.h>
#include <private/qcore_unix_p.h>

#include <unistd.h>
#include <fcntl.h>
#include <linux/kd.h>
#include <sys/ioctl.h>

class InputEventManager : public Input
{
public:
    explicit InputEventManager(Compositor *parent)
        : Input(parent) {}

    inline Compositor *compositor() {
        return static_cast<Compositor*>(parent());
    }

private:
    bool event(QEvent *event) override {
        if (event->type() == QEvent::KeyPress) {
            auto key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Escape)
                qApp->quit();
        }

        return Input::event(event);
    }
};

static bool setConsoleMode(int mode)
{
    bool ok = false;

    int console_fd = open("/dev/console", O_RDWR);
    if (console_fd == -1) {
        qWarning("Error opening console device");
    } else {
        if (ioctl(console_fd, KDSETMODE, mode) == -1) {
            qWarning("Error setting console mode to %d", mode);
        } else {
            ok = true;
        }
        close(console_fd);
    }

    return ok;
}

Compositor::Compositor(QObject *parent)
    : QObject{parent}
{}

Compositor::~Compositor()
{
    qDeleteAll(m_outputs);
    setConsoleMode(KD_TEXT);
}

void Compositor::start()
{
    if (m_input)
        return;

    // 切换到图形模式，避免被tty的文字输出影响
    setConsoleMode(KD_GRAPHICS);
    m_vtHandler = new QFbVtHandler(this);
    m_input = new InputEventManager(this);

    auto fbList = Output::allFrmaebufferFiles();
    qDebug() << "Found framebuffer:" << fbList;

    if (fbList.isEmpty()) {
        qFatal("Not found framebuffer.");
    }

    for (auto fbFile : fbList) {
        auto o = new Output(fbFile);
        if (o->isNull()) {
            delete o;
            continue;
        }

        m_outputs << o;
    }

    if (m_outputs.isEmpty())
        qFatal("No valid framebuffer.");

    m_input->setCursorBoundsRect(m_outputs.first()->rect());

    Q_ASSERT(!m_rootNode);
    m_rootNode = new RootNode(this);

    m_cursorNode = new Cursor(m_rootNode);
    m_cursorNode->setZ(9999);

    connect(m_input, &Input::cursorPositionChanged, m_cursorNode, [this] {
        m_cursorNode->move(m_input->cursorPosition());
    });

    m_input->setCursorPosition(m_outputs.first()->rect().center());

    paint();
}

void Compositor::paint(const QRegion &region)
{
    Q_ASSERT(!m_painting);
    if (m_outputs.isEmpty())
        return;

    // 多屏采用复制模式，先绘制到第一个屏幕，再复制到其它屏幕

    auto primaryOutput = m_outputs.first();
    QPainter pa(primaryOutput);

    if (!pa.isActive())
        return;

    m_painting = true;
    pa.setBackground(m_background);
    pa.setBackgroundMode(Qt::OpaqueMode);

    if (!region.isEmpty())
        pa.setClipRegion(region);

    // 绘制壁纸
    if (!m_wallpaper.isNull()) {
        if (m_wallpaperWithPrimaryOutput.isNull()
            || m_wallpaperWithPrimaryOutput.width() != primaryOutput->width()) {
            const auto tmpRect = QRect(QPoint(0, 0), primaryOutput->size().scaled(m_wallpaper.size(),
                                                                                  Qt::KeepAspectRatio));
            m_wallpaperWithPrimaryOutput = m_wallpaper.copy(tmpRect);
            m_wallpaperWithPrimaryOutput = m_wallpaperWithPrimaryOutput.scaled(primaryOutput->size(),
                                                                               Qt::IgnoreAspectRatio,
                                                                               Qt::SmoothTransformation);
        }

        pa.drawImage(0, 0, m_wallpaperWithPrimaryOutput);
    }

    pa.setBackgroundMode(Qt::TransparentMode);
    // 绘制窗口
    m_rootNode->setGeometry(primaryOutput->rect());
    m_rootNode->paint(&pa);
    pa.end();

    for (int i = 1; i < m_outputs.count(); ++i) {
        auto o = m_outputs.at(i);

        pa.begin(o);
        pa.setBackground(m_background);
        pa.setBackgroundMode(Qt::OpaqueMode);
        pa.setCompositionMode(QPainter::CompositionMode_Source);
        pa.setRenderHint(QPainter::SmoothPixmapTransform);

        QRect targetRect = primaryOutput->rect();
        // 等比缩放到目标屏幕
        targetRect.setSize(targetRect.size().scaled(o->size(), Qt::KeepAspectRatio));
        //  居中显示
        targetRect.moveCenter(o->rect().center());
        pa.drawImage(targetRect, *primaryOutput, primaryOutput->rect());
    }

    m_painting = false;
}

void Compositor::paint()
{
    paint({});
}

QColor Compositor::background() const
{
    return m_background;
}

void Compositor::setBackground(const QColor &newBackground)
{
    if (m_background == newBackground)
        return;
    m_background = newBackground;
    emit backgroundChanged();

    paint();
}

void Compositor::setWallpaper(const QImage &image)
{
    m_wallpaper = image;
    paint();
}

void Compositor::markDirty(const QRegion &region)
{
    qDebug() << "Dirty" << region;

    if (m_painting)
        return;
    paint(region);
}

void Compositor::addWindow(Window *window)
{
    m_rootNode->addChild(window);
}

void Compositor::removeWindow(Window *window)
{
    m_rootNode->removeChild(window);
}

Node::Node(Node *parent)
    : QObject(parent)
{
    if (parent)
        parent->addChild(this);
}

Node::~Node()
{

}

QRect Node::rect() const
{
    return QRect(QPoint(0, 0), m_geometry.size());
}

QRect Node::geometry() const
{
    return m_geometry;
}

void Node::setGeometry(const QRect &newGeometry)
{
    if (m_geometry == newGeometry)
        return;
    auto oldGeometry = m_geometry;
    m_geometry = newGeometry;
    emit geometryChanged(oldGeometry, newGeometry);
}

// 包括 child node 的 geometry
QRegion Node::wholeGeometry() const
{
    QRegion region;
    region += geometry();

    for (auto child : m_orderedChildren)
        region += child->wholeGeometry().translated(geometry().topLeft());

    return region;
}

QRegion Node::wholeRect() const
{
    QRegion region;
    region += rect();

    for (auto child : m_orderedChildren)
        region += child->wholeGeometry();

    return region;
}

bool Node::isVisible() const
{
    return m_visible;
}

void Node::setVisible(bool newVisible)
{
    if (m_visible == newVisible)
        return;
    m_visible = newVisible;
    emit visibleChanged(newVisible);

    update(wholeRect());
}

int Node::z() const
{
    return m_z;
}

void Node::setZ(int newZ)
{
    if (m_z == newZ)
        return;
    m_z = newZ;
    emit zChanged();
}

void Node::paint(QPainter *pa)
{
    for (auto child : m_orderedChildren) {
        if (!child->isVisible())
            continue;

        pa->save();
        QTransform tf;
        pa->setWorldTransform(tf.translate(child->geometry().x(), child->geometry().y()), true);
        pa->setClipRect(child->rect(), Qt::ReplaceClip);
        child->paint(pa);
        pa->restore();
    }
}

void Node::update(QRegion region)
{
    if (!isVisible())
        return;

    qDebug() << this << "request update" << region;

    if (auto parentNode = qobject_cast<Node*>(parent()))
        parentNode->update(region.translated(geometry().topLeft()));
}

void Node::addChild(Node *child)
{
    qDebug() << "Add child" << child << "to" << this;

    Q_ASSERT(!m_orderedChildren.contains(child));
    m_orderedChildren.append(child);
    sortChild(child);

    connect(child, &Node::destroyed, this, [this, child] {
        removeChild(child);
    });

    connect(child, &Node::geometryChanged, this, [this, child] (QRect oldGeo, QRect newGeo) {
        if (!child->isVisible())
            return;

        QPoint positionDiff = oldGeo.topLeft() - newGeo.topLeft();
        QRegion dirtyRegion = child->wholeGeometry();
        dirtyRegion += dirtyRegion.translated(positionDiff);
        update(dirtyRegion);
    });

    connect(child, &Node::zChanged, this, [this, child] {
        sortChild(child);
    });

    if (child->isVisible())
        update(child->wholeGeometry());
}

void Node::removeChild(Node *child)
{
    qDebug() << "Remove child" << child << "from" << this;

    child->disconnect(this);
    m_orderedChildren.removeOne(child);
    if (child->isVisible())
        update(child->wholeGeometry());
}

void Node::sortChild(Node *child)
{
    if (m_orderedChildren.count() == 1)
        return;

    int index = m_orderedChildren.indexOf(child);
    Q_ASSERT(index >= 0);

    int newIndex = index;
    for (int i = index + 1; i < m_orderedChildren.size(); ++i) {
        if (child->z() > m_orderedChildren.at(i)->z())
            newIndex = i + 1;
        else
            break;
    }

    if (newIndex != index) {
        // index < newIndex，因此不用担心插入对象导致 index 失效
        m_orderedChildren.insert(newIndex, child);
        // 必须在insert之后调用
        m_orderedChildren.removeAt(index);
        return;
    }

    for (int i = index - 1; i >= 0; --i) {
        if (child->z() < m_orderedChildren.at(i)->z())
            newIndex = i;
        else
            break;
    }

    if (newIndex != index) {
        // index > newIndex，因此为避免index失效，先移除对象
        m_orderedChildren.removeAt(index);
        // 必须在removeAt之后调用
        m_orderedChildren.insert(newIndex, child);
        return;
    }
}


Window::Window(Node *parent)
    : Node(parent)
    , m_titlebar(new WindowTitleBar(this))
{
    connect(this, &Window::geometryChanged, this, &Window::updateTitleBarGeometry);
    updateTitleBarGeometry();
}

Window::State Window::state() const
{
    return m_state;
}

void Window::setState(State newState)
{
    if (m_state == newState)
        return;
    m_state = newState;
    emit stateChanged();
    update(rect());
}

void Window::paint(QPainter *pa)
{
    { // draw titlebar
        QRect titlebarGeometry(0, 0, geometry().width(), 35);
        titlebarGeometry.moveBottomLeft(QPoint(0, 0));

        pa->fillRect(titlebarGeometry, Qt::white);
    }

    pa->fillRect(rect(), Qt::blue);
    Node::paint(pa);
}

void Window::updateTitleBarGeometry()
{
    QRect rect = this->rect();

    rect.setHeight(35);
    // 标题栏显示在窗口上方
    rect.moveBottomLeft(QPoint(0, 0));

    m_titlebar->setGeometry(rect);
}

Rectangle::Rectangle(Node *parent)
    : Node(parent)
{

}

QColor Rectangle::color() const
{
    return m_color;
}

void Rectangle::setColor(const QColor &newColor)
{
    if (m_color == newColor)
        return;
    m_color = newColor;
    emit colorChanged();
    update(rect());
}

void Rectangle::paint(QPainter *pa)
{
    pa->fillRect(rect(), m_color);
    Node::paint(pa);
}

WindowTitleBar::WindowTitleBar(Window *window)
    : Node(window)
    , m_maximizeButton(new Rectangle(this))
    , m_minimizeButton(new Rectangle(this))
    , m_closeButton(new Rectangle(this))
{
    m_maximizeButton->setColor(Qt::green);
    m_minimizeButton->setColor(Qt::yellow);
    m_closeButton->setColor(Qt::red);
    connect(this, &WindowTitleBar::geometryChanged, this, &WindowTitleBar::updateButtonGeometry);
    updateButtonGeometry();
}

void WindowTitleBar::updateButtonGeometry()
{
    const auto rect = this->rect();

    QRect buttonGeometry = QRect(0, 0, rect.height(), rect.height());
    buttonGeometry.moveTopRight(rect.topRight());
    m_closeButton->setGeometry(buttonGeometry);

    buttonGeometry.moveTopRight(buttonGeometry.topLeft());
    m_maximizeButton->setGeometry(buttonGeometry);

    buttonGeometry.moveTopRight(buttonGeometry.topLeft());
    m_minimizeButton->setGeometry(buttonGeometry);
}

void WindowTitleBar::paint(QPainter *pa)
{
    pa->fillRect(rect(), Qt::white);
    Node::paint(pa);
}

Cursor::Cursor(Node *parent)
    : Node(parent)
{
    if (m_image.load(":/images/cursor.png")) {
        m_image = m_image.scaledToWidth(48, Qt::SmoothTransformation);
        setGeometry(m_image.rect());
    }
}

void Cursor::move(const QPoint &pos)
{
    setGeometry(QRect(pos, geometry().size()));
}

void Cursor::paint(QPainter *pa)
{
    pa->drawImage(0, 0, m_image);
}

Compositor::RootNode::RootNode(Compositor *compositor)
    : Node(nullptr)
{
    setParent(compositor);
}
