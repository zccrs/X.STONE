// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include "virtualoutput.h"

#include <QPainter>

VirtualOutput::VirtualOutput(QWidget *parent)
    : QWidget{parent}
{}

void VirtualOutput::setImage(QImage *image)
{
    m_image = image;
    update();
}

void VirtualOutput::paintEvent(QPaintEvent *event)
{
    if (!m_image)
        return;

    Q_UNUSED(event)
    QPainter pa(this);
    pa.drawImage(rect(), *m_image);
}
