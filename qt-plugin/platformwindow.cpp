// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include "platformwindow.h"

#include <qpa/qwindowsysteminterface.h>
#include <QGuiApplication>
#include <QWindow>

PlatformWindow::PlatformWindow(const QRemoteObjectPendingReply<QString> &surfaceID,
                               QRemoteObjectNode *node, QWindow *window)
    : QPlatformWindow(window)
    , m_watcher(new QRemoteObjectPendingCallWatcher(surfaceID))
{
    m_geometry = normalGeometry();

    QObject::connect(m_watcher.get(), &QRemoteObjectPendingCallWatcher::finished, node, [this, node] {
        qDebug() << "New Surface:" << m_watcher->returnValue();
        auto surface = node->acquire<SurfaceReplica>(m_watcher->returnValue().toString());

        QObject::connect(surface, &SurfaceReplica::initialized, m_watcher.get(), [this, surface] {
            m_surface.reset(surface);
            m_surface->setGeometry(m_geometry);
            m_surface->setVisible(m_visible);

            if (m_surfaceWatcher)
                m_surfaceWatcher();

            initForSurface();
        });
    });
}

void PlatformWindow::initialize()
{
    QPlatformWindow::initialize();
}

QSurfaceFormat PlatformWindow::format() const
{
    QSurfaceFormat format;

    format.setAlphaBufferSize(0);
    format.setRedBufferSize(8);
    format.setBlueBufferSize(8);
    format.setGreenBufferSize(8);

    return format;
}

void PlatformWindow::setGeometry(const QRect &rect)
{
    m_geometry = rect;

    if (m_surface)
        m_surface->setGeometry(rect);
}

QRect PlatformWindow::geometry() const
{
    return m_surface ? m_surface->geometry() : m_geometry;
}

QRect PlatformWindow::normalGeometry() const
{
    return QRect(100, 100, 300, 200);
}

void PlatformWindow::setVisible(bool visible)
{
    m_visible = visible;
    if (m_surface)
        m_surface->setVisible(visible);
}

WId PlatformWindow::winId() const
{
    return reinterpret_cast<quintptr>(this);
}

void PlatformWindow::setWindowTitle(const QString &)
{
    // TODO
}

bool PlatformWindow::close()
{
    setVisible(false);
    return true;
}

bool PlatformWindow::isExposed() const
{
    return true;
}

bool PlatformWindow::isActive() const
{
    // TODO
    return true;
}

void PlatformWindow::setSurfaceWatcher(std::function<void ()> watcher)
{
    m_surfaceWatcher = watcher;
}

void PlatformWindow::initForSurface()
{
    QObject::connect(m_surface.get(), &SurfaceReplica::mouseEvent,
                     [this] (QEvent::Type type, QPoint local, QPoint global,
                             Qt::MouseButton button, Qt::MouseButtons buttons,
                             Qt::KeyboardModifiers modifiers) {
        qDebug() << "Mouse Event" << type << local << global << button << buttons << modifiers;
        QWindowSystemInterface::handleMouseEvent(window(), local, global, buttons, button, type, modifiers);
    });

    QObject::connect(m_surface.get(), &SurfaceReplica::keyEvent,
                     [this] (QEvent::Type type, int qtkey, Qt::KeyboardModifiers modifiers, QString text) {
        qDebug() << "Key Event" << type << qtkey << modifiers << text;
        QWindowSystemInterface::handleKeyEvent(window(), type, qtkey, modifiers, text);
    });
}
