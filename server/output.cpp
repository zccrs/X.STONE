// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include "output.h"

#include <QDir>
#include <QFile>
#include <QPixelFormat>
#include <QDebug>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

Output::Output(const QString &fbFile)
{
    init(fbFile);
}

Output::~Output()
{

}

QStringList Output::allFrmaebufferFiles()
{
    QStringList files;
    int i = 0;

    Q_FOREVER {
        QString fbFilePath = QString("/dev/fb%1").arg(i++);

        if (QFile::exists(fbFilePath)) {
            files.append(fbFilePath);
        } else {
            break;
        }
    }

    return files;
}

void Output::init(const QString &fbFile)
{
    qDebug() << "Init framebuffer" << fbFile;
    QFile file(fbFile);

    if (!file.open(QIODevice::ReadWrite)) {
        qWarning() << "Can't open file:" << file.errorString();
        return;
    }

    auto fb_fd = file.handle();

    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        qWarning() << "Error reading variable information";
        return;
    }

    size_t screensize = vinfo.xres_virtual * vinfo.yres_virtual * vinfo.bits_per_pixel / 8;
    unsigned char *fb_ptr = (unsigned char *)mmap(NULL, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_ptr == MAP_FAILED) {
        qWarning() << "Error mapping framebuffer device to memory";
        return;
    }

    m_widthMM = vinfo.width;
    m_heightMM = vinfo.height;

    QImage image(fb_ptr, vinfo.xres_virtual, vinfo.yres_virtual, QImage::Format_RGB32,
        [] (void *image) {
            auto &i = *reinterpret_cast<Output*>(image);
            if (!i.isNull())
                munmap(i.bits(), i.sizeInBytes());
        }, this);

    QImage::swap(image);

    qDebug() << "Init finished:" << *this;
}

int Output::metric(PaintDeviceMetric metric) const
{
    if (metric == PdmWidthMM)
        return m_widthMM;
    if (metric == PdmHeightMM)
        return m_heightMM;

    return QImage::metric(metric);
}
