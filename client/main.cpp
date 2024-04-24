// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include <QGuiApplication>
#include <QRemoteObjectNode>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <QSharedMemory>
#include <QImage>
#include <QPainter>

#include "rep_kernel_replica.h"

#define PAINT_STATE "__paiting"

void paintButton(SurfaceReplica *surface)
{
    if (surface->property(PAINT_STATE).toBool())
        return;

    surface->setProperty(PAINT_STATE, true);
    // paint
    auto ok = surface->begin();
    // TODO: use QRemoteObjectPendingCallWatcher
    if (!ok.waitForFinished()) {
        surface->setProperty(PAINT_STATE, false);
        return;
    }
    if (!ok.returnValue()) {
        surface->setProperty(PAINT_STATE, false);
        return;
    }

    surface->fillRect(QRect(QPoint(0, 0), surface->geometry().size()), Qt::white);
    surface->fillRect(QRect(102, 102, 50, 30), Qt::black);
    surface->fillRect(QRect(100, 100, 50, 30), Qt::gray);
    surface->drawText(QPoint(102, 102), "Button", Qt::red);
    surface->end();
    surface->setProperty(PAINT_STATE, false);
}

void paintWithShm(SurfaceReplica *surface)
{
    auto watcher = new QRemoteObjectPendingCallWatcher(surface->getShm(), surface);
    QObject::connect(watcher, &QRemoteObjectPendingCallWatcher::finished, surface, [watcher, surface] {
        auto ret = watcher->returnValue().value<QPair<QString, QSize>>();
        qDebug() << "Get shm:" << ret;

        QSharedMemory shm(QNativeIpcKey(ret.first));
        if (!shm.attach()) {
            surface->releaseShm(ret.first);
            qWarning() << "Can't attach to shm:" << shm.errorString();
            return;
        }

        if (!shm.lock()) {
            qDebug() << "Can't lock shm.";
            return;
        }

        QImage buffer(reinterpret_cast<uchar*>(shm.data()), ret.second.width(), ret.second.height(), QImage::Format_RGB888);
        QPainter pa(&buffer);

        pa.fillRect(QRect(QPoint(0, 0), ret.second), Qt::white);
        pa.drawImage(buffer.rect(), QImage("/home/zccrs/Downloads/Designer (3).png"));
        pa.end();

        shm.unlock();

        surface->putImage(ret.first, buffer.rect());
    });
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QCommandLineParser cmParser;
    QCommandLineOption useShm("shm", "Use shm for window");
    cmParser.addOption(useShm);
    cmParser.addHelpOption();

    QGuiApplication app(argc, argv);
    cmParser.process(app);

    QRemoteObjectNode node;
    qDebug() << node.connectToNode(QUrl(QStringLiteral("local:X.STONE")));

    std::unique_ptr<ManagerReplica> manager(node.acquire<ManagerReplica>());
    manager->waitForSource();
    auto clientID = manager->createClient();
    clientID.waitForFinished();

    QObject::connect(&app, &QCoreApplication::aboutToQuit, manager.get(), [&] {
        manager->destroyClient(clientID.returnValue());
    });

    qDebug() << "New Client:" << clientID.returnValue();

    std::unique_ptr<ClientReplica> client(node.acquire<ClientReplica>(clientID.returnValue()));
    QObject::connect(client.get(), &ClientReplica::ping, client.get(), &ClientReplica::pong);

    client->waitForSource();
    client->pong();

    auto surfaceID = client->createSurface();
    surfaceID.waitForFinished();

    qDebug() << "New Surface:" << surfaceID.returnValue();
    std::unique_ptr<SurfaceReplica> surface(node.acquire<SurfaceReplica>(surfaceID.returnValue()));
    surface->waitForSource();
    surface->setVisible(true);

    if (cmParser.isSet(useShm)) {
        surface->setGeometry(QRect(100, 100, 600, 400));
        QObject::connect(surface.get(), &SurfaceReplica::geometryChanged, [&] {
            paintWithShm(surface.get());
        });
        paintWithShm(surface.get());
    } else {
        surface->setGeometry(QRect(500, 500, 300, 200));
        QObject::connect(surface.get(), &SurfaceReplica::geometryChanged, [&] {
            paintButton(surface.get());
        });
        paintButton(surface.get());
    }

    return app.exec();
}
