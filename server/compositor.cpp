// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include "compositor.h"
#include "output.h"
#include "input.h"

#include <QCoreApplication>
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
        // close(tty_fd);
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

    for (auto fbFile : fbList)
        m_outputs << new Output(fbFile);

    paint();
}

void Compositor::paint()
{
    for (auto output : m_outputs) {
        if (output->isNull())
            continue;

        QPainter pa(output);
        pa.setBrush(QBrush(m_background));
        pa.drawRect(output->rect());
    }
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
