// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include <QCoreApplication>
#include <QRemoteObjectNode>
#include <QDebug>

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

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

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
    surface->setGeometry(QRect(100, 100, 600, 400));
    surface->setVisible(true);

    QObject::connect(surface.get(), &SurfaceReplica::geometryChanged, [&] {
        paintButton(surface.get());
    });
    paintButton(surface.get());

    return app.exec();
}
