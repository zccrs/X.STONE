// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#pragma once

#include "rep_kernel_replica.h"
#include <qpa/qplatformbackingstore.h>
#include <QSharedMemory>

class PlatformWindow;
class BackingStore : public QPlatformBackingStore
{
public:
    explicit BackingStore(QWindow *window);

    QPaintDevice *paintDevice() override;
    QImage toImage() const override;

    void resize(const QSize &size, const QRegion &staticContents) override;
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;

    void beginPaint(const QRegion&) override;
    void endPaint() override;

    PlatformWindow *platformWindow() const;
    SurfaceReplica *surface() const;

private:
    void updateBuffer();

    std::unique_ptr<QSharedMemory> m_shm;
    QImage m_image;
};
