// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include <QCoreApplication>
#include <QRemoteObjectNode>
#include <QDebug>

#include "rep_kernel_replica.h"

int main(int argc, char **argv)
{
    QLoggingCategory::setFilterRules("qt.remoteobjects.*=true");

    QCoreApplication app(argc, argv);

    QRemoteObjectNode node;
    qDebug() << node.connectToNode(QUrl(QStringLiteral("local:X.STONE")));

    std::unique_ptr<ManagerReplica> manager(node.acquire<ManagerReplica>());
    manager->waitForSource();
    auto clientID = manager->createClient();
    clientID.waitForFinished();

    qDebug() << "New Client:" << clientID.returnValue();

    std::unique_ptr<ClientReplica> client(node.acquire<ClientReplica>(clientID.returnValue()));
    client->waitForSource();

    auto surfaceID = client->createSurface();
    surfaceID.waitForFinished();

    qDebug() << "New Surface:" << surfaceID.returnValue();
    std::unique_ptr<SurfaceReplica> surface(node.acquire<SurfaceReplica>(surfaceID.returnValue()));
    surface->waitForSource();
    surface->setGeometry(QRect(100, 100, 600, 400));
    surface->setVisible(true);

    return app.exec();
}
