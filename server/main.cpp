// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include <QCoreApplication>
#include <QTimer>

#include "compositor.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    Compositor compositor;
    compositor.start();

    QTimer timer;
    timer.connect(&timer, &QTimer::timeout, [&compositor] {
        compositor.setBackground(compositor.background() == Qt::red ? Qt::blue : Qt::red);
    });
    timer.start(1000);

    return app.exec();
}
