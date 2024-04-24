// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include <QApplication>
#include <QTimer>

#include "compositor.h"
#include "protocol.h"

int main(int argc, char **argv)
{
    if (argc <= 1 || QByteArray(argv[1]) != "--debug")
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

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
