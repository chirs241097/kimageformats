/*
    SPDX-FileCopyrightText: 2014 Alex Merry <alex.merry@kdemail.net>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <stdio.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QTextStream>

#include "../tests/format-enum.h"

static void writeImageData(const char *name, const QString &filename, const QImage &image)
{
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        qint64 written = file.write(reinterpret_cast<const char *>(image.bits()), image.sizeInBytes());
        if (written == image.sizeInBytes()) {
            QTextStream(stdout) << "       " << name
                                << " written to " << filename << "\n";
        } else {
            QTextStream(stdout) << "       could not write " << name
                                << " to " << filename << ":"
                                << file.errorString() << "\n";
        }
    } else {
        QTextStream(stdout) << "       could not open "
                            << filename << ":"
                            << file.errorString() << "\n";
    }
}

// allow each byte to be different by up to 1, to allow for rounding errors
static bool fuzzyeq(const QImage &im1, const QImage &im2, uchar fuzziness)
{
    const int height = im1.height();
    const int width = im1.width();
    for (int i = 0; i < height; ++i) {
        const uchar *line1 = im1.scanLine(i);
        const uchar *line2 = im2.scanLine(i);
        for (int j = 0; j < width; ++j) {
            if (line1[j] > line2[j]) {
                if (line1[j] - line2[j] > fuzziness)
                    return false;
            } else {
                if (line2[j] - line1[j] > fuzziness)
                    return false;
            }
        }
    }
    return true;
}

int main(int argc, char ** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::removeLibraryPath(QStringLiteral(PLUGIN_DIR));
    QCoreApplication::addLibraryPath(QStringLiteral(PLUGIN_DIR));
    QCoreApplication::setApplicationName(QStringLiteral("readtest"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Performs basic image conversion checking."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("format"), QStringLiteral("format to test"));
    QCommandLineOption fuzz(
        QStringList() << QStringLiteral("f") << QStringLiteral("fuzz"),
        QStringLiteral("Allow for some deviation in ARGB data."),
        QStringLiteral("max"));
    parser.addOption(fuzz);

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.count() < 1) {
        QTextStream(stderr) << "Must provide a format\n";
        parser.showHelp(1);
    } else if (args.count() > 1) {
        QTextStream(stderr) << "Too many arguments\n";
        parser.showHelp(1);
    }

    uchar fuzziness = 0;
    if (parser.isSet(fuzz)) {
        bool ok;
        uint fuzzarg = parser.value(fuzz).toUInt(&ok);
        if (!ok || fuzzarg > 255) {
            QTextStream(stderr) << "Error: max fuzz argument must be a number between 0 and 255\n";
            parser.showHelp(1);
        }
        fuzziness = uchar(fuzzarg);
    }

    QString suffix = args.at(0);
    QByteArray format = suffix.toLatin1();

    QDir imgdir(QLatin1String(IMAGEDIR "/") + suffix);
    imgdir.setNameFilters(QStringList(QLatin1String("*.") + suffix));
    imgdir.setFilter(QDir::Files);

    int passed = 0;
    int failed = 0;

    QTextStream(stdout) << "********* "
        << "Starting basic read tests for "
        << suffix << " images *********\n";

    const QList<QByteArray> formats = QImageReader::supportedImageFormats();
    QStringList formatStrings;
    formatStrings.reserve(formats.size());
    std::transform(formats.begin(), formats.end(), std::back_inserter(formatStrings), [](const QByteArray &format) { return QString(format); });
    QTextStream(stdout) << "QImageReader::supportedImageFormats: " << formatStrings.join(", ") << "\n";

    const QFileInfoList lstImgDir = imgdir.entryInfoList();
    for (const QFileInfo &fi : lstImgDir) {
        int suffixPos = fi.filePath().count() - suffix.count();
        QString inputfile = fi.filePath();
        QString expfile = fi.filePath().replace(suffixPos, suffix.count(), QStringLiteral("png"));
        QString expfilename = QFileInfo(expfile).fileName();

        QImageReader inputReader(inputfile, format);
        QImageReader expReader(expfile, "png");

        QImage inputImage;
        QImage expImage;

        if (!expReader.read(&expImage)) {
            QTextStream(stdout) << "ERROR: " << fi.fileName()
                << ": could not load " << expfilename
                << ": " << expReader.errorString()
                << "\n";
            ++failed;
            continue;
        }
        if (!inputReader.canRead()) {
            QTextStream(stdout) << "FAIL : " << fi.fileName()
                << ": failed can read: "
                << inputReader.errorString()
                << "\n";
            ++failed;
            continue;
        }
        if (!inputReader.read(&inputImage)) {
            QTextStream(stdout) << "FAIL : " << fi.fileName()
                << ": failed to load: "
                << inputReader.errorString()
                << "\n";
            ++failed;
            continue;
        }
        if (expImage.width() != inputImage.width()) {
            QTextStream(stdout) << "FAIL : " << fi.fileName()
                << ": width was " << inputImage.width()
                << " but " << expfilename << " width was "
                << expImage.width() << "\n";
            ++failed;
        } else if (expImage.height() != inputImage.height()) {
            QTextStream(stdout) << "FAIL : " << fi.fileName()
                << ": height was " << inputImage.height()
                << " but " << expfilename << " height was "
                << expImage.height() << "\n";
            ++failed;
        } else {
            if (inputImage.format() != QImage::Format_ARGB32) {
                QTextStream(stdout) << "INFO : " << fi.fileName()
                    << ": converting " << fi.fileName()
                    << " from " << formatToString(inputImage.format())
                    << " to ARGB32\n";
                inputImage = inputImage.convertToFormat(QImage::Format_ARGB32);
            }
            if (expImage.format() != QImage::Format_ARGB32) {
                QTextStream(stdout) << "INFO : " << fi.fileName()
                    << ": converting " << expfilename
                    << " from " << formatToString(expImage.format())
                    << " to ARGB32\n";
                expImage = expImage.convertToFormat(QImage::Format_ARGB32);
            }
            if (fuzzyeq(inputImage, expImage, fuzziness)) {
                QTextStream(stdout) << "PASS : " << fi.fileName() << "\n";
                ++passed;
            } else {
                QTextStream(stdout) << "FAIL : " << fi.fileName()
                    << ": differs from " << expfilename << "\n";
                writeImageData("expected data",
                               fi.fileName() + QLatin1String("-expected.data"),
                               expImage);
                writeImageData("actual data",
                               fi.fileName() + QLatin1String("-actual.data"),
                               inputImage);
                ++failed;
            }
        }
    }

    QTextStream(stdout) << "Totals: "
        << passed << " passed, "
        << failed << " failed\n";
    QTextStream(stdout) << "********* "
        << "Finished basic read tests for "
        << suffix << " images *********\n";

    return failed == 0 ? 0 : 1;
}
