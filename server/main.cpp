// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include <QGuiApplication>
#include <QTimer>

#include "compositor.h"
#include "protocol.h"

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    Compositor compositor;
    compositor.start();

    compositor.setBackground(Qt::black);
    compositor.setWallpaper(QImage("/usr/share/wallpapers/deepin/desktop.jpg"));

    Protocol protocol;

    QObject::connect(&protocol, &Protocol::windowAdded, &compositor, &Compositor::addWindow);
    QObject::connect(&protocol, &Protocol::windowRemoved, &compositor, &Compositor::removeWindow);

    protocol.start();

    return app.exec();
}
