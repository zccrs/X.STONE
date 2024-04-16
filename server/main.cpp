// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include <QCoreApplication>
#include <QDebug>

#include "output.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    qDebug() << "Found framebuffer:" << Output::allFrmaebufferFiles();

    return app.exec();
}
