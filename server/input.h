// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QRect>
#include <xkbcommon/xkbcommon.h>

struct udev;
struct libinput;
struct libinput_event;
struct libinput_event_pointer;
struct libinput_event_keyboard;

QT_BEGIN_NAMESPACE
class QSocketNotifier;
QT_END_NAMESPACE

class Input : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QRect cursorBoundsRect READ cursorBoundsRect WRITE setCursorBoundsRect NOTIFY cursorBoundsRectChanged FINAL)

public:
    explicit Input(QObject *parent = nullptr);
    ~Input();

    QRect cursorBoundsRect() const;
    void setCursorBoundsRect(const QRect &newCursorBoundsRect);
    void setCursorPosition(const QPoint &pos);

signals:
    void cursorBoundsRectChanged();
    void pointerDeviceChanged();
    void keyboardDeviceChanged();

private:
    void onReadyRead();
    void processEvent(libinput_event *ev);
    void processButton(libinput_event_pointer *e);
    void processMotion(libinput_event_pointer *e);
    void processAbsMotion(libinput_event_pointer *e);
    void processAxis(libinput_event_pointer *e);
    void processKey(libinput_event_keyboard *e);

    udev *m_udev;
    libinput *m_li;
    int m_liFd;
    QScopedPointer<QSocketNotifier> m_notifier;

    int m_pointerDeviceCount = 0;
    int m_keyboardDeviceCount = 0;

    Qt::MouseButtons m_buttons;
    Qt::KeyboardModifiers m_keyModifiers = Qt::NoModifier;
    QPoint m_cursorPos;
    QRect m_cursorBoundsRect;

    int keysymToQtKey(xkb_keysym_t key) const;
    int keysymToQtKey(xkb_keysym_t keysym, Qt::KeyboardModifiers *modifiers, const QString &text) const;

    xkb_context *m_ctx = nullptr;
    xkb_keymap *m_keymap = nullptr;
    xkb_state *m_state = nullptr;
};
