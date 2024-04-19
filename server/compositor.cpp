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
        // close(tty_fd);
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
    m_rootNode = new Node();
    m_rootNode->setParent(this);

    m_cursorNode = new Cursor(m_rootNode);

    connect(m_input, &Input::cursorPositionChanged, m_cursorNode, [this] {
        m_cursorNode->move(m_input->cursorPosition());
    });

    m_input->setCursorPosition(m_outputs.first()->rect().center());

    connect(m_rootNode, &Node::updateRequest, this, &Compositor::markDirty);

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
    QRegion dirtyRegion;
    dirtyRegion += m_geometry;
    dirtyRegion += newGeometry;
    m_geometry = newGeometry;
    emit geometryChanged(newGeometry);
    emit updateRequest(dirtyRegion);
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

    updateRequest(geometry());
}

void Node::paint(QPainter *pa)
{
    for (auto child : m_orderedChildren) {
        if (!child->isVisible())
            continue;

        pa->save();
        QTransform tf;
        pa->setWorldTransform(tf.translate(child->geometry().x(), child->geometry().y()));
        pa->setClipRect(child->rect(), Qt::IntersectClip);
        child->paint(pa);
        pa->restore();
    }
}

void Node::addChild(Node *child)
{
    Q_ASSERT(!m_orderedChildren.contains(child));
    m_orderedChildren.append(child);

    connect(child, &Node::updateRequest, this, [this, child] (const QRegion &region) {
        if (child->isVisible())
            emit updateRequest(region.translated(geometry().topLeft()));
    });

    connect(child, &Node::destroyed, this, [this, child] {
        removeChild(child);
    });

    if (child->isVisible())
        emit updateRequest(child->geometry());
}

void Node::removeChild(Node *child)
{
    m_orderedChildren.removeOne(child);
    if (child->isVisible())
        emit updateRequest(child->geometry());
}

Window::Window(Node *parent)
    : Node(parent)
{

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
    emit updateRequest({});
}

void Window::paint(QPainter *pa)
{
    pa->fillRect(rect(), Qt::blue);
}

Cursor::Cursor(Node *parent)
    : Node(parent)
{
    if (m_image.load(":/images/cursor.png")) {
        m_image = m_image.scaledToWidth(48, Qt::SmoothTransformation);
        setGeometry(m_image.rect());
        setVisible(true);
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
