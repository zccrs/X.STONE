// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include "protocol.h"
#include "compositor.h"

#include <QLocalServer>
#include <QLocalSocket>

Protocol::Protocol(QObject *parent)
    : QObject{parent}
    , m_node(QUrl(QStringLiteral("local:X.STONE")))
{

}

Protocol::~Protocol()
{
    for (auto client : m_clients) {
        for (auto s : client->surfaces) {
            emit windowRemoved(s->m_window);
            s->deleteLater();
        }
    }
}

void Protocol::start()
{
    new Manager(this);
}

void Protocol::stop()
{
    m_node.disableRemoting(this);
}

Manager::Manager(Protocol *parent)
    : ManagerSource(parent)
{
    qDebug() << parent->m_node.enableRemoting(this);
}

Protocol *Manager::parent()
{
    return static_cast<Protocol*>(QObject::parent());
}

QString Manager::createClient()
{
    auto client = new Client(parent());
    parent()->m_clients << client;

    return client->objectName();
}

void Manager::destroyClient(const QString &id)
{
    auto client = parent()->findChild<Client*>(id);
    parent()->m_clients.removeOne(client);

    for (auto s : client->surfaces) {
        emit parent()->windowRemoved(s->m_window);
        s->deleteLater();
    }

    if (client)
        client->deleteLater();
}

inline static QString getID(void *ptr) {
    return "0x" + QString::number(reinterpret_cast<quintptr>(ptr), 16);
}

Client::Client(Protocol *parent)
    : ClientSource(parent)
{
    setObjectName(getID(this));
    parent->m_node.enableRemoting(this, objectName());
}

Protocol *Client::parent()
{
    return static_cast<Protocol*>(QObject::parent());
}

QString Client::createSurface()
{
    auto surface = new Surface(new Window(), parent());
    surfaces << surface;

    emit parent()->windowAdded(surface->m_window);

    return surface->objectName();
}

Surface::Surface(Window *window, Protocol *parent)
    : SurfaceSource(parent)
    , m_window(window)
{
    connect(window, &Window::geometryChanged, this, &Surface::geometryChanged);
    connect(window, &Window::visibleChanged, this, &Surface::visibleChanged);

    setObjectName(getID(this));
    parent->m_node.enableRemoting(this, objectName());
}

QRect Surface::geometry() const
{
    return m_window->geometry();
}

void Surface::setGeometry(QRect geometry)
{
    m_window->setGeometry(geometry);
}

bool Surface::visible() const
{
    return m_window->isVisible();
}

void Surface::setVisible(bool visible)
{
    m_window->setVisible(visible);
}
