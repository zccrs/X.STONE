// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: MIT

#include "integration.h"
#include <qpa/qplatformintegrationplugin.h>

#include <QDebug>

class PlatformIntegrationPlugin : public QPlatformIntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformIntegrationFactoryInterface_iid FILE "plugin.json")

public:
    QPlatformIntegration *create(const QString&, const QStringList&, int &, char **) Q_DECL_OVERRIDE;
};

QPlatformIntegration *PlatformIntegrationPlugin::create(const QString& system, const QStringList&, int &, char **)
{
    return new Integration();
}

#include "main.moc"
