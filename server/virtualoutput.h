// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#pragma once

#include <QWidget>
#include <QPaintEvent>

class VirtualOutput : public QWidget
{
    Q_OBJECT
public:
    explicit VirtualOutput(QWidget *parent = nullptr);

    void setImage(QImage *image);

private:
    void paintEvent(QPaintEvent *event) override;

    QImage *m_image = nullptr;
};
