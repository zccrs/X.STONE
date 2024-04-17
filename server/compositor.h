// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QColor>

QT_BEGIN_NAMESPACE
class QFbVtHandler;
QT_END_NAMESPACE

class Input;
class Output;
class Compositor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor background READ background WRITE setBackground NOTIFY backgroundChanged FINAL)

public:
    explicit Compositor(QObject *parent = nullptr);
    ~Compositor();

    void start();

    QColor background() const;
    void setBackground(const QColor &newBackground);

signals:
    void backgroundChanged();

private:
    void paint();

    QFbVtHandler *m_vtHandler = nullptr;
    Input *m_input = nullptr;
    QList<Output*> m_outputs;
    QColor m_background;
};
