// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include <QCoreApplication>
#include <QRemoteObjectNode>
#include <QDebug>

#include "rep_kernel_replica.h"

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

    return app.exec();
}
