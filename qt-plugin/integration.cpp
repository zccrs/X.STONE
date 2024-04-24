// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include "integration.h"
#include "platformwindow.h"
#include "backingstore.h"

#include <QGuiApplication>
#include <private/qgenericunixfontdatabase_p.h>
#include <private/qgenericunixservices_p.h>
#include <private/qgenericunixeventdispatcher_p.h>
#include <private/qhighdpiscaling_p.h>
#include <private/qinputdevice_p.h>
#include <qpa/qplatformsurface.h>
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformoffscreensurface.h>
#include <private/qgenericunixthemes_p.h>

Integration::Integration() {}

void Integration::initialize()
{
    m_services.reset(new QGenericUnixServices);

    if (!qGuiApp->primaryScreen()) {
        m_placeholderScreen.reset(new QPlatformPlaceholderScreen);
        QWindowSystemInterface::handleScreenAdded(m_placeholderScreen.get(), true);
    }

    m_roNode.connectToNode(QUrl(QStringLiteral("local:X.STONE")));

    m_roManager.reset(m_roNode.acquire<ManagerReplica>());
    m_roManager->waitForSource();
    auto clientID = m_roManager->createClient();
    clientID.waitForFinished();

    qDebug() << "New Client:" << clientID.returnValue();

    m_clientId = clientID.returnValue();
    m_roClient.reset(m_roNode.acquire<ClientReplica>(m_clientId));
    QObject::connect(m_roClient.get(), &ClientReplica::ping, m_roClient.get(), &ClientReplica::pong);

    m_roClient->waitForSource();
    m_roClient->pong();
}

void Integration::destroy()
{
    if (m_placeholderScreen)
        QWindowSystemInterface::handleScreenRemoved(m_placeholderScreen.release());

    if (m_roClient)
        m_roManager->destroyClient(m_clientId);
    m_roManager.reset();
}

bool Integration::hasCapability(Capability cap) const
{
    if (cap == NativeWidgets)
        return true;

    return false;
}

QPlatformServices *Integration::services() const
{
    return m_services.get();
}

QPlatformFontDatabase *Integration::fontDatabase() const
{
    if (!m_fontDb)
        m_fontDb.reset(new QGenericUnixFontDatabase);
    return m_fontDb.get();
}

QAbstractEventDispatcher *Integration::createEventDispatcher() const
{
    return createUnixEventDispatcher();
}

QStringList Integration::themeNames() const
{
    return {"deepin"};
}

QPlatformTheme *Integration::createPlatformTheme(const QString &name) const
{
    return QGenericUnixTheme::createUnixTheme(name);
}

QPlatformWindow *Integration::createPlatformWindow(QWindow *window) const
{
    auto surfaceID = m_roClient->createSurface();
    return new PlatformWindow(surfaceID, const_cast<QRemoteObjectNode*>(&m_roNode), window);
}

QPlatformBackingStore *Integration::createPlatformBackingStore(QWindow *window) const
{
    return new BackingStore(window);
}
