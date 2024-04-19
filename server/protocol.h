// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QRemoteObjectRegistryHost>

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

class Surface : public SurfaceSource
{
    friend class Protocol;
    friend class Manager;
    friend class Client;
public:
    explicit Surface(Window *window, Protocol *parent);

    QRect geometry() const override;
    void setGeometry(QRect geometry) override;

    bool visible() const override;
    void setVisible(bool visible) override;

private:
    Window *m_window;
};

class Client : public ClientSource
{
    friend class Protocol;
    friend class Manager;
public:
    explicit Client(Protocol *parent);

    Protocol *parent();
    QString createSurface() override;

private:
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
