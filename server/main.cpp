// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QDebug>

#include "output.h"
#include "input.h"

class InputEventManager : public Input
{
public:
    explicit InputEventManager(QObject *parent = nullptr)
        : Input(parent) {}

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

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    qDebug() << "Found framebuffer:" << Output::allFrmaebufferFiles();

    InputEventManager event;
    Q_UNUSED(event);

    return app.exec();
}
