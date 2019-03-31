/* This file is part of the KDE project
   Copyright (C) 2013 Boudewijn Rempt <boud@valdyas.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the Lesser GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This code is based on Thacher Ulrich PSD loading code released
   on public domain. See: http://tulrich.com/geekstuff/
*/

#include "kra.h"

#include <kzip.h>

#include <QImage>
#include <QIODevice>
#include <QFile>

static constexpr char s_magic[] = "application/x-krita";
static constexpr int s_magic_size = strlen(s_magic);

KraHandler::KraHandler()
{
}

bool KraHandler::canRead() const
{
    if (canRead(device())) {
        setFormat("kra");
        return true;
    }
    return false;
}

bool KraHandler::read(QImage *image)
{
    KZip zip(device());
    if (!zip.open(QIODevice::ReadOnly)) return false;

    const KArchiveEntry *entry = zip.directory()->entry(QLatin1String("mergedimage.png"));
    if (!entry || !entry->isFile()) return false;

    const KZipFileEntry* fileZipEntry = static_cast<const KZipFileEntry*>(entry);

    image->loadFromData(fileZipEntry->data(), "PNG");

    return true;
}

bool KraHandler::canRead(QIODevice *device)
{
    if (!device) {
        qWarning("KraHandler::canRead() called with no device");
        return false;
    }

    char buff[57];
    if (device->peek(buff, sizeof(buff)) == sizeof(buff))
        return memcmp(buff + 0x26, s_magic, s_magic_size) == 0;

    return false;
}

QImageIOPlugin::Capabilities KraPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "kra" || format == "KRA") {
        return Capabilities(CanRead);
    }
    if (!format.isEmpty()) {
        return {};
    }
    if (!device->isOpen()) {
        return {};
    }

    Capabilities cap;
    if (device->isReadable() && KraHandler::canRead(device)) {
        cap |= CanRead;
    }
    return cap;
}

QImageIOHandler *KraPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new KraHandler;
    handler->setDevice(device);
    handler->setFormat(format);
    return handler;
}
