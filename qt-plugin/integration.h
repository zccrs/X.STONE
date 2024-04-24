// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#pragma once

#include "rep_kernel_replica.h"
#include <QRemoteObjectNode>
#include <qpa/qplatformintegration.h>

class Integration : public QPlatformIntegration
{
public:
    Integration();

    void initialize() override;
    void destroy() override;

    bool hasCapability(QPlatformIntegration::Capability cap) const override;

    QPlatformServices *services() const override;

    QPlatformFontDatabase *fontDatabase() const override;
    QAbstractEventDispatcher *createEventDispatcher() const override;

    QStringList themeNames() const override;
    QPlatformTheme *createPlatformTheme(const QString &name) const override;

    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;

private:
    mutable std::unique_ptr<QPlatformFontDatabase> m_fontDb;
    std::unique_ptr<QPlatformServices> m_services;
    std::unique_ptr<QPlatformPlaceholderScreen> m_placeholderScreen;

    QRemoteObjectNode m_roNode;
    std::unique_ptr<ManagerReplica> m_roManager;
    QString m_clientId;
    std::unique_ptr<ClientReplica> m_roClient;
};
