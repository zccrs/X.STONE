// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#pragma once

#include <QImage>

class Output : public QImage
{
public:
    explicit Output(const QString &fbFile);
    ~Output();

    static QStringList allFrmaebufferFiles();

private:
    void init(const QString &fbFile);

    int metric(PaintDeviceMetric metric) const override;

    quint32 m_widthMM, m_heightMM;
};
