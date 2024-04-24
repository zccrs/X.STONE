// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QRemoteObjectRegistryHost>
#include <QPointer>

#include "rep_kernel_source.h"

class Window;
class Protocol;
class Manager : public ManagerSource
{
    friend class Protocol;
public:
    explicit Manager(Protocol *parent);

    Protocol *parent();

    QString createClient() override;
    void destroyClient(const QString &id) override;
};

class Client;
class Surface : public SurfaceSource
{
    friend class Protocol;
    friend class Manager;
    friend class Client;
public:
    explicit Surface(Window *window, Client *client, Protocol *parent);
    ~Surface();

    QRect geometry() const override;
    void setGeometry(QRect geometry) override;

    bool visible() const override;
    void setVisible(bool visible) override;

private:
    void destroy() override;

    // for render
    bool begin() override;
    void fillRect(QRect rect, QColor color) override;
    void drawText(QPoint pos, QString text, QColor color) override;
    void end() override;

    Window *m_window;
    QPointer<Client> m_client;
};

class Client : public ClientSource
{
    friend class Protocol;
    friend class Manager;
    friend class Surface;
    Q_OBJECT
public:
    explicit Client(Protocol *parent);

    Protocol *parent();
    QString createSurface() override;

signals:
    void disconnected();

private:
    void doPing();
    void pong() override;
    void destroySurface(Surface *surface);
    void timerEvent(QTimerEvent *event);

    int pingTimer = 0;
    QList<Surface*> surfaces;
};

class Protocol : public QObject
{
    friend class Manager;
    friend class Client;
    friend class Surface;
    Q_OBJECT
public:
    explicit Protocol(QObject *parent = nullptr);
    ~Protocol();

    void start();
    void stop();

signals:
    void windowAdded(Window *window);
    void windowRemoved(Window *window);

private:
    QRemoteObjectHost m_node;
    QList<Client*> m_clients;
};
