// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include "compositor.h"
#include "output.h"
#include "virtualoutput.h"
#include "input.h"

#include <QGuiApplication>
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
        switch (event->type()) {
        case QEvent::KeyPress: {
            auto key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Escape)
                qApp->quit();
            Q_FALLTHROUGH();
        }
        case QEvent::KeyRelease: {
            if (compositor()->m_focusWindow)
                qApp->sendEvent(compositor()->m_focusWindow.get(), event);
            break;
        }
        case QEvent::MouseButtonPress: Q_FALLTHROUGH();
        case QEvent::MouseButtonRelease: Q_FALLTHROUGH();
        case QEvent::MouseMove: {
            auto mouseEvent = static_cast<QMouseEvent*>(event);
            const auto globalPos = mouseEvent->globalPosition().toPoint();
            auto node = compositor()->m_rootNode->childAt(globalPos);

            if (node) {
                QMouseEvent newEvent(event->type(),
                                     node->mapFromGlobal(globalPos),
                                     globalPos,
                                     mouseEvent->button(),
                                     mouseEvent->buttons(),
                                     mouseEvent->modifiers());
                return qApp->sendEvent(node, &newEvent);
            }
            break;
        }
        default: break;
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

    if (m_outputs.isEmpty()) {
        qWarning("No valid framebuffer.");

        if (qGuiApp->platformName() == "offscreen") {
            qGuiApp->exit(-1);
        }

        qDebug() << "Fallback to virtual output.";

        m_virtualOutput.reset(new VirtualOutput());
        m_virtualOutput->resize(1280, 800);
        m_virtualOutput->show();
    }

    auto primaryOutput = m_outputs.isEmpty() ? nullptr : m_outputs.first();
    const QRect bufferRect = m_virtualOutput ? m_virtualOutput->rect() : primaryOutput->rect();
    m_input->setCursorBoundsRect(bufferRect);

    Q_ASSERT(!m_rootNode);
    m_rootNode = new RootNode(this);

    m_cursorNode = new Cursor(m_rootNode);
    m_cursorNode->setZ(9999);

    connect(m_input, &Input::cursorPositionChanged, m_cursorNode, [this] {
        m_cursorNode->move(m_input->cursorPosition());
    });

    m_input->setCursorPosition(bufferRect.center());
    m_buffer = QImage(bufferRect.size(),
                      m_virtualOutput ? QImage::Format_RGB888 : primaryOutput->format());

    paint();
}

void Compositor::paint(const QRegion &region)
{
    Q_ASSERT(!m_painting);
    if (m_outputs.isEmpty() && !m_virtualOutput)
        return;

    if (m_buffer.isNull())
        return;

    QPainter pa(&m_buffer);
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
            || m_wallpaperWithPrimaryOutput.width() != m_buffer.width()) {
            const auto tmpRect = QRect(QPoint(0, 0), m_buffer.size().scaled(m_wallpaper.size(),
                                                                                  Qt::KeepAspectRatio));
            m_wallpaperWithPrimaryOutput = m_wallpaper.copy(tmpRect);
            m_wallpaperWithPrimaryOutput = m_wallpaperWithPrimaryOutput.scaled(m_buffer.size(),
                                                                               Qt::IgnoreAspectRatio,
                                                                               Qt::SmoothTransformation);
        }

    }

    pa.drawImage(0, 0, m_wallpaperWithPrimaryOutput);
    pa.setBackgroundMode(Qt::TransparentMode);

    // 绘制窗口
    m_rootNode->setGeometry(m_buffer.rect());
    m_rootNode->draw(&pa);
    pa.end();

    // for debug
    // int i = 0;
    // m_buffer.save(QString("/tmp/zccrs/%1.png").arg(++i));

    // 送显
    for (int i = 0; i < m_outputs.count(); ++i) {
        auto o = m_outputs.at(i);
        if (!o->waitForVSync())
            continue;

        pa.begin(o);
        pa.setBackground(m_background);
        pa.setBackgroundMode(Qt::OpaqueMode);
        pa.setCompositionMode(QPainter::CompositionMode_Source);
        pa.setRenderHint(QPainter::SmoothPixmapTransform);

        QRect targetRect = m_buffer.rect();
        // 等比缩放到目标屏幕
        targetRect.setSize(targetRect.size().scaled(o->size(), Qt::KeepAspectRatio));
        //  居中显示
        targetRect.moveCenter(o->rect().center());

        if (region.isEmpty()) {
            pa.drawImage(targetRect, m_buffer, m_buffer.rect());
        } else {
            QTransform mapToOutput;
            mapToOutput.scale(qreal(targetRect.width()) / m_buffer.width(),
                              qreal(targetRect.height()) / m_buffer.height());
            mapToOutput.translate(targetRect.x(), targetRect.y());

            for (const QRect &r : region) {
                pa.drawImage(mapToOutput.mapRect(r), m_buffer, r);
            }
        }
    }

    if (m_virtualOutput) {
        m_virtualOutput->setImage(&m_buffer);
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
    // qDebug() << "Dirty" << region;

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

    update(wholeRect(), true);
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

void Node::draw(QPainter *pa)
{
    if (!isVisible())
        return;

    pa->save();
    QTransform tf;
    pa->setWorldTransform(tf.translate(geometry().x(), geometry().y()), true);
    auto worldTransform = pa->worldTransform();

    pa->setClipRect(rect(), Qt::IntersectClip);
    paint(pa);
    pa->restore();

    if (m_orderedChildren.isEmpty())
        return;

    auto oldWorldTransform = pa->worldTransform();
    pa->setWorldTransform(worldTransform, false);

    for (auto child : m_orderedChildren) {
        child->draw(pa);
    }

    pa->setWorldTransform(oldWorldTransform, false);
}

Node *Node::parentNode() const
{
    return m_parent;
}

Node *Node::childAt(const QPoint &position) const
{
    for (int i = m_orderedChildren.count() - 1; i >= 0; --i) {
        Node *child = m_orderedChildren.at(i);
        // 排除光标
        if (qobject_cast<Cursor*>(child))
            continue;

        if (auto node = child->childAt(position - child->geometry().topLeft()))
            return node;

        if (child->geometry().contains(position, true))
            return child;
    }

    return nullptr;
}

QPoint Node::mapFromGlobal(const QPoint &position) const
{
    if (auto parent = parentNode())
        return parent->mapFromGlobal(position) - geometry().topLeft();
    return position - geometry().topLeft();
}

QPoint Node::mapToGlobal(const QPoint &position) const
{
    if (auto parent = parentNode())
        return parent->mapToGlobal(position) + geometry().topLeft();
    return position + geometry().topLeft();
}

void Node::paint(QPainter *pa)
{
    Q_UNUSED(pa);
}

void Node::update(QRegion region, bool force)
{
    if (!isVisible() && !force)
        return;

    if (!qobject_cast<Cursor*>(this))
        qDebug() << this << "request update" << region;

    if (auto parentNode = this->parentNode())
        parentNode->update(region.translated(geometry().topLeft()));
}

bool Node::event(QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        emit mousePressed(static_cast<QMouseEvent*>(event)->position().toPoint());
        return true;
    }

    return QObject::event(event);
}

void Node::addChild(Node *child)
{
    qDebug() << "Add child" << child << "to" << this;

    Q_ASSERT(!m_orderedChildren.contains(child));
    m_orderedChildren.append(child);
    child->m_parent = this;
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
    child->m_parent = nullptr;
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
    setVisible(false);
    connect(this, &Window::geometryChanged, this, &Window::onGeometryChanged);
    connect(m_titlebar, &WindowTitleBar::requestClose, this, [this] {
        setVisible(false);
    });
    onGeometryChanged();
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

// for render
bool Window::begin()
{
    if (m_bgBuffer.isNull())
        return false;

    if (m_painter.isActive())
        return true;

    Q_ASSERT(m_damage.isEmpty());
    bool ok = m_painter.begin(&m_bgBuffer);

    if (ok)
        qDebug() << "Paint request from client" << this;

    return ok;
}

void Window::fillRect(QRect rect, QColor color)
{
    if (!m_painter.isActive())
        return;

    m_damage += rect;
    m_painter.fillRect(rect, color);
}

void Window::drawText(QPoint pos, QString text, QColor color)
{
    if (!m_painter.isActive())
        return;

    QRect textRect = m_painter.boundingRect(pos.x(), pos.y(),
                                            rect().width() - pos.x(),
                                            rect().height() - pos.y(),
                                            0, text);

    m_damage += textRect;
    m_painter.setBrush(Qt::NoBrush);
    m_painter.setPen(color);
    m_painter.drawText(textRect, text);
}

void Window::end()
{
    if (!m_painter.isActive())
        return;

    m_painter.end();

    if (m_damage.isEmpty())
        return;

    m_painter.begin(&m_buffer);
    for (QRect r : m_damage) {
        m_painter.drawImage(r, m_bgBuffer, r);
    }

    QRegion tmp;
    m_damage.swap(tmp);

    qDebug() << "Damage by client" << tmp;

    update(tmp);
}

void Window::paint(QPainter *pa)
{
    pa->drawImage(rect(), m_buffer);
}

void Window::onGeometryChanged()
{
    updateTitleBarGeometry();
    updateBuffers();
}

void Window::updateTitleBarGeometry()
{
    QRect rect = this->rect();

    rect.setHeight(35);
    // 标题栏显示在窗口上方
    rect.moveBottomLeft(QPoint(0, 0));

    m_titlebar->setGeometry(rect);
}

void Window::updateBuffers()
{
    const QSize size = geometry().size();
    if (size.isEmpty()) {
        m_buffer = QImage();
        m_bgBuffer = m_buffer;
        return;
    }

    m_buffer = QImage(size, QImage::Format_RGB888);
    m_buffer.fill(Qt::black);
    m_bgBuffer = m_buffer;
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
    connect(m_closeButton, &Rectangle::mousePressed, this, &WindowTitleBar::requestClose);
    connect(m_maximizeButton, &Rectangle::mousePressed, this, &WindowTitleBar::requestToggleMaximize);
    connect(m_minimizeButton, &Rectangle::mousePressed, this, &WindowTitleBar::requestMinimize);
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
}

Cursor::Cursor(Node *parent)
    : Node(parent)
{
    if (m_image.load(":/images/cursor.png")) {
        m_image = m_image.scaledToWidth(32, Qt::SmoothTransformation);

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
