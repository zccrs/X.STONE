// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include "input.h"

#include <QEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCoreApplication>
#include <QSocketNotifier>
#include <private/qxkbcommon_p.h>

#include <fcntl.h>
#include <unistd.h>
#include <libinput.h>

static int liOpen(const char *path, int flags, void *user_data)
{
    Q_UNUSED(user_data);
    return open(path, flags);
}

static void liClose(int fd, void *user_data)
{
    Q_UNUSED(user_data);
    close(fd);
}

static const struct libinput_interface liInterface = {
    liOpen,
    liClose
};

static void liLogHandler(libinput *libinput, libinput_log_priority priority, const char *format, va_list args)
{
    Q_UNUSED(libinput);
    Q_UNUSED(priority);

    char buf[512];
    int n = vsnprintf(buf, sizeof(buf), format, args);
    if (n > 0) {
        if (buf[n - 1] == '\n')
            buf[n - 1] = '\0';
        qDebug("libinput: %s", buf);
    }
}

// Begin copy from qtbase project
Input::Input(QObject *parent)
    : QObject{parent}
{
    m_udev = udev_new();
    if (Q_UNLIKELY(!m_udev))
        qFatal("Failed to get udev context for libinput");

    m_li = libinput_udev_create_context(&liInterface, nullptr, m_udev);
    if (Q_UNLIKELY(!m_li))
        qFatal("Failed to get libinput context");

    libinput_log_set_handler(m_li, liLogHandler);
#ifndef QT_NO_DEBUG
    libinput_log_set_priority(m_li, LIBINPUT_LOG_PRIORITY_INFO);
#endif

    if (Q_UNLIKELY(libinput_udev_assign_seat(m_li, "seat0")))
        qFatal("Failed to assign seat");

    m_liFd = libinput_get_fd(m_li);
    m_notifier.reset(new QSocketNotifier(m_liFd, QSocketNotifier::Read));

    connect(m_notifier.data(), &QSocketNotifier::activated, this, &Input::onReadyRead);
    // Process the initial burst of DEVICE_ADDED events.
    onReadyRead();

    qDebug() << "Using xkbcommon for key mapping";
    m_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!m_ctx) {
        qWarning("Failed to create xkb context");
        return;
    }
    m_keymap = xkb_keymap_new_from_names(m_ctx, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!m_keymap) {
        qWarning("Failed to compile keymap");
        return;
    }
    m_state = xkb_state_new(m_keymap);
    if (!m_state) {
        qWarning("Failed to create xkb state");
        return;
    }
}

Input::~Input()
{
    if (m_state)
        xkb_state_unref(m_state);
    if (m_keymap)
        xkb_keymap_unref(m_keymap);
    if (m_ctx)
        xkb_context_unref(m_ctx);
}

void Input::onReadyRead()
{
    if (libinput_dispatch(m_li)) {
        qWarning("libinput_dispatch failed");
        return;
    }

    libinput_event *ev;
    while ((ev = libinput_get_event(m_li)) != nullptr) {
        processEvent(ev);
        libinput_event_destroy(ev);
    }
}

void Input::processEvent(libinput_event *ev)
{
    libinput_event_type type = libinput_event_get_type(ev);
    libinput_device *dev = libinput_event_get_device(ev);

    switch (type) {
    case LIBINPUT_EVENT_DEVICE_ADDED:
    {
        if (libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_POINTER)) {
            ++m_pointerDeviceCount;
            Q_EMIT pointerDeviceChanged();
        }
        if (libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_KEYBOARD)) {
            ++m_keyboardDeviceCount;
            Q_EMIT keyboardDeviceChanged();
        }
        break;
    }
    case LIBINPUT_EVENT_DEVICE_REMOVED:
    {
        if (libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_POINTER)) {
            --m_pointerDeviceCount;
            Q_EMIT pointerDeviceChanged();
        }
        if (libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_KEYBOARD)) {
            --m_keyboardDeviceCount;
            Q_EMIT keyboardDeviceChanged();
        }
        break;
    }
    case LIBINPUT_EVENT_POINTER_BUTTON:
        processButton(libinput_event_get_pointer_event(ev));
        break;
    case LIBINPUT_EVENT_POINTER_MOTION:
        processMotion(libinput_event_get_pointer_event(ev));
        break;
    case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
        processAbsMotion(libinput_event_get_pointer_event(ev));
        break;
    case LIBINPUT_EVENT_POINTER_AXIS:
        processAxis(libinput_event_get_pointer_event(ev));
        break;
    case LIBINPUT_EVENT_KEYBOARD_KEY:
        processKey(libinput_event_get_keyboard_event(ev));
        break;
    default:
        break;
    }
}

void Input::processButton(libinput_event_pointer *e)
{
    const uint32_t b = libinput_event_pointer_get_button(e);
    const bool pressed = libinput_event_pointer_get_button_state(e) == LIBINPUT_BUTTON_STATE_PRESSED;

    Qt::MouseButton button = Qt::NoButton;
    switch (b) {
    case 0x110: button = Qt::LeftButton; break;    // BTN_LEFT
    case 0x111: button = Qt::RightButton; break;
    case 0x112: button = Qt::MiddleButton; break;
    case 0x113: button = Qt::ExtraButton1; break;  // AKA Qt::BackButton
    case 0x114: button = Qt::ExtraButton2; break;  // AKA Qt::ForwardButton
    case 0x115: button = Qt::ExtraButton3; break;  // AKA Qt::TaskButton
    case 0x116: button = Qt::ExtraButton4; break;
    case 0x117: button = Qt::ExtraButton5; break;
    case 0x118: button = Qt::ExtraButton6; break;
    case 0x119: button = Qt::ExtraButton7; break;
    case 0x11a: button = Qt::ExtraButton8; break;
    case 0x11b: button = Qt::ExtraButton9; break;
    case 0x11c: button = Qt::ExtraButton10; break;
    case 0x11d: button = Qt::ExtraButton11; break;
    case 0x11e: button = Qt::ExtraButton12; break;
    case 0x11f: button = Qt::ExtraButton13; break;
    }

    m_buttons.setFlag(button, pressed);

    QEvent::Type type = pressed ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease;
    QMouseEvent event(type, m_cursorPos, m_cursorPos, button, m_buttons, m_keyModifiers);
    qApp->sendEvent(this, &event);
}

void Input::processMotion(libinput_event_pointer *e)
{
    const auto g = m_cursorBoundsRect;
    if (g.isEmpty())
        return;

    const double dx = libinput_event_pointer_get_dx(e);
    const double dy = libinput_event_pointer_get_dy(e);

    setCursorPosition({qBound(g.left(), qRound(m_cursorPos.x() + dx), g.right()),
                       qBound(g.top(), qRound(m_cursorPos.y() + dy), g.bottom())});

    QMouseEvent event(QEvent::MouseMove, m_cursorPos, m_cursorPos, Qt::NoButton, m_buttons, m_keyModifiers);
    qApp->sendEvent(this, &event);
}

void Input::processAbsMotion(libinput_event_pointer *e)
{
    const auto g = m_cursorBoundsRect;
    if (g.isEmpty())
        return;

    const double x = libinput_event_pointer_get_absolute_x_transformed(e, g.width());
    const double y = libinput_event_pointer_get_absolute_y_transformed(e, g.height());

    setCursorPosition({qBound(g.left(), qRound(g.left() + x), g.right()),
                       qBound(g.top(), qRound(g.top() + y), g.bottom())});

    QMouseEvent event(QEvent::MouseMove, m_cursorPos, m_cursorPos, Qt::NoButton, m_buttons, m_keyModifiers);
    qApp->sendEvent(this, &event);
}

void Input::processAxis(libinput_event_pointer *e)
{
    double value; // default axis value is 15 degrees per wheel click
    QPoint angleDelta;

    if (libinput_event_pointer_has_axis(e, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
        value = libinput_event_pointer_get_axis_value(e, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
        angleDelta.setY(qRound(value));
    }

    if (libinput_event_pointer_has_axis(e, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
        value = libinput_event_pointer_get_axis_value(e, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
        angleDelta.setX(qRound(value));
    }

    const int factor = -8;
    angleDelta *= factor;

    QWheelEvent event(m_cursorPos, m_cursorPos, QPoint(), angleDelta, m_buttons, m_keyModifiers,
                      Qt::NoScrollPhase, false);
    qApp->sendEvent(this, &event);
}

void Input::processKey(libinput_event_keyboard *e)
{
    if (!m_ctx || !m_keymap || !m_state)
        return;

    const uint32_t keycode = libinput_event_keyboard_get_key(e) + 8;
    const xkb_keysym_t sym = xkb_state_key_get_one_sym(m_state, keycode);
    const bool pressed = libinput_event_keyboard_get_key_state(e) == LIBINPUT_KEY_STATE_PRESSED;

    // Modifiers here is the modifier state before the event, i.e. not
    // including the current key in case it is a modifier. See the XOR
    // logic in QKeyEvent::modifiers(). ### QTBUG-73826
    Qt::KeyboardModifiers modifiers = QXkbCommon::modifiers(m_state);

    const QString text = QXkbCommon::lookupString(m_state, keycode);
    const int qtkey = QXkbCommon::keysymToQtKey(sym, modifiers, m_state, keycode);

    xkb_state_update_key(m_state, keycode, pressed ? XKB_KEY_DOWN : XKB_KEY_UP);

    m_keyModifiers = QXkbCommon::modifiers(m_state);

    QKeyEvent event(pressed ? QEvent::KeyPress : QEvent::KeyRelease, qtkey, m_keyModifiers, text);
    qApp->sendEvent(this, &event);
}

// End copy from qtbase project

QRect Input::cursorBoundsRect() const
{
    return m_cursorBoundsRect;
}

void Input::setCursorBoundsRect(const QRect &newCursorBoundsRect)
{
    if (m_cursorBoundsRect == newCursorBoundsRect)
        return;
    m_cursorBoundsRect = newCursorBoundsRect;
    emit cursorBoundsRectChanged();
}

void Input::setCursorPosition(const QPoint &pos)
{
    const auto g = m_cursorBoundsRect;

    QPoint tmp;

    if (g.isEmpty()) {
        tmp = pos;
    } else {
        tmp.setX(qBound(g.left(), pos.x(), g.right()));
        tmp.setY(qBound(g.top(), pos.y(), g.bottom()));
    }

    if (tmp == m_cursorPos)
        return;

    m_cursorPos = tmp;
    emit cursorPositionChanged();
}

QPoint Input::cursorPosition() const
{
    return m_cursorPos;
}
