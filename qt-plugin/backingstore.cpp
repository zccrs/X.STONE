// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include "backingstore.h"
#include "platformwindow.h"

#include <QSharedMemory>
#include <QGuiApplication>
#include <QTimer>

#include <qpa/qwindowsysteminterface.h>

BackingStore::BackingStore(QWindow *window)
    : QPlatformBackingStore(window)
{
    QTimer::singleShot(0, [this] {
        if (auto pw = platformWindow()) {
            pw->setSurfaceWatcher([this] {
                updateBuffer();
            });

            updateBuffer();
        }
    });
}

QPaintDevice *BackingStore::paintDevice()
{
    return &m_image;
}

QImage BackingStore::toImage() const
{
    return m_image;
}

void BackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    updateBuffer();
}

void BackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    if (auto s = this->surface())
        s->putImage(m_shm->nativeKey(), region);
}

void BackingStore::beginPaint(const QRegion &)
{
    if (!m_shm->lock()) {
        qDebug() << "Can't lock shm.";
        return;
    }
}

void BackingStore::endPaint()
{
    m_shm->unlock();
}

PlatformWindow *BackingStore::platformWindow() const
{
    return dynamic_cast<PlatformWindow*>(window()->handle());
}

SurfaceReplica *BackingStore::surface() const
{
    if (!window()->handle())
        return nullptr;
    return platformWindow()->m_surface.get();
}

void BackingStore::updateBuffer()
{
    auto surface = this->surface();
    if (!surface)
        return;

    auto watcher = new QRemoteObjectPendingCallWatcher(surface->getShm(), surface);
    QObject::connect(watcher, &QRemoteObjectPendingCallWatcher::finished, surface, [this, watcher, surface] {
        auto ret = watcher->returnValue().value<QPair<QString, QSize>>();
        watcher->deleteLater();
        qDebug() << "Get shm:" << ret;

        std::unique_ptr<QSharedMemory> shm(new QSharedMemory(QNativeIpcKey(ret.first)));
        if (!shm->attach()) {
            surface->releaseShm(ret.first);
            qWarning() << "Can't attach to shm:" << shm->errorString();
            return;
        }

        bool oldShmIsNull = !m_shm;
        m_image = QImage(reinterpret_cast<uchar*>(shm->data()), ret.second.width(), ret.second.height(), QImage::Format_RGB888);
        m_shm.reset(shm.release());

        if (oldShmIsNull) {
            const QPoint cursorPos = QCursor::pos();
            if (window()->isVisible()) {
                QRect rect(QPoint(), platformWindow()->geometry().size());
                QWindowSystemInterface::handleExposeEvent(window(), rect);

                if (platformWindow()->geometry().contains(cursorPos))
                    QWindowSystemInterface::handleEnterEvent(window(),
                                                             window()->mapFromGlobal(cursorPos), cursorPos);
            } else {
                QWindowSystemInterface::handleExposeEvent(window(), QRegion());
                if (window()->type() & Qt::Window) {
                    if (QWindow *windowUnderMouse = QGuiApplication::topLevelAt(cursorPos)) {
                        QWindowSystemInterface::handleEnterEvent(windowUnderMouse,
                                                                 windowUnderMouse->mapFromGlobal(cursorPos),
                                                                 cursorPos);
                    }
                }
            }
        }
    });
}
