// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include "protocol.h"
#include "compositor.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QTimerEvent>
#include <QDebug>

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

    connect(client, &Client::disconnected, this, [client, this] {
        Q_ASSERT(client->parent() == parent());
        destroyClient(client->objectName());
    });

    return client->objectName();
}

void Manager::destroyClient(const QString &id)
{
    qDebug() << "Destroy client:" << id;

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
    doPing();
}

Protocol *Client::parent()
{
    return static_cast<Protocol*>(QObject::parent());
}

QString Client::createSurface()
{
    auto surface = new Surface(new Window(), this, parent());
    surfaces << surface;

    emit parent()->windowAdded(surface->m_window);

    return surface->objectName();
}

void Client::doPing()
{
    pingTimer = startTimer(std::chrono::seconds(1));
    emit ClientSource::ping();
}

void Client::pong()
{
    if (pingTimer > 0)
        killTimer(pingTimer);
    QTimer::singleShot(std::chrono::seconds(2), this, &Client::doPing);
}

void Client::destroySurface(Surface *surface)
{
    Q_ASSERT(surface);
    surfaces.removeOne(surface);
    emit parent()->windowRemoved(surface->m_window);
    surface->m_client = nullptr;
    surface->deleteLater();
}

void Client::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == pingTimer) {
        killTimer(pingTimer);
        pingTimer = 0;
        emit disconnected();
        return;
    }

    ClientSource::timerEvent(event);
}

Surface::Surface(Window *window, Client *client, Protocol *parent)
    : SurfaceSource(parent)
    , m_window(window)
    , m_client(client)
{
    connect(window, &Window::geometryChanged, this, &Surface::geometryChanged);
    connect(window, &Window::visibleChanged, this, &Surface::visibleChanged);

    setObjectName(getID(this));
    parent->m_node.enableRemoting(this, objectName());
}

Surface::~Surface()
{
    destroy();
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

void Surface::destroy()
{
    if (m_client)
        m_client->destroySurface(this);
    if (m_window) {
        m_window->deleteLater();
        m_window = nullptr;
    }
}

bool Surface::begin()
{
    bool ok = m_window->begin();
    return ok;
}

void Surface::fillRect(QRect rect, QColor color)
{
    m_window->fillRect(rect, color);
}

void Surface::drawText(QPoint pos, QString text, QColor color)
{
    m_window->drawText(pos, text, color);
}

void Surface::end()
{
    m_window->end();
}

QPair<QString, QSize> Surface::getShm()
{
    return m_window->getShm();
}

void Surface::releaseShm(QString key)
{
    m_window->releaseShm(key);
}

bool Surface::putImage(QString key, QRegion region)
{
    return m_window->putImage(key, region);
}
