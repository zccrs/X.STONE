// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#pragma once

#include "rep_kernel_replica.h"
#include <qpa/qplatformwindow.h>

class PlatformWindow : public QPlatformWindow
{
    friend class BackingStore;
public:
    explicit PlatformWindow(const QRemoteObjectPendingReply<QString> &surfaceID,
                            QRemoteObjectNode *node, QWindow *window);

    void initialize() override;

    QSurfaceFormat format() const override;

    void setGeometry(const QRect &rect) override;
    QRect geometry() const override;
    QRect normalGeometry() const override;

    void setVisible(bool visible) override;

    WId winId() const override;

    void setWindowTitle(const QString &title) override;
    bool close() override;

    bool isExposed() const override;
    bool isActive() const override;

    void setSurfaceWatcher(std::function<void()> watcher);

private:
    void initForSurface();

    QRect m_geometry;
    bool m_visible = false;

    std::function<void()> m_surfaceWatcher;
    std::unique_ptr<SurfaceReplica> m_surface;
    std::unique_ptr<QRemoteObjectPendingCallWatcher> m_watcher;
};
