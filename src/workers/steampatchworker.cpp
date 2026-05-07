#include "steampatchworker.h"
#include "../config.h"
#include "../utils/paths.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QCryptographicHash>

SteamPatchWorker::SteamPatchWorker(QObject* parent)
    : QThread(parent)
{
}

void SteamPatchWorker::run() {
    try {
        emit log("Starting Steam patch process...", "INFO");

        // 1. Find the Steam root directory
        QString steamDir = Config::getSteamDir();
        if (steamDir.isEmpty()) {
            emit log("Could not find Steam installation directory", "ERROR");
            throw std::runtime_error("Steam installation not found. Please make sure Steam is installed.");
        }
        emit log(QString("Found Steam directory: %1").arg(steamDir), "INFO");

        // 2. Verify steam.exe exists in that directory
        QString steamExe = QDir(steamDir).filePath("steam.exe");
        if (!QFile::exists(steamExe)) {
            emit log(QString("steam.exe not found at: %1").arg(steamExe), "ERROR");
            throw std::runtime_error("steam.exe not found in the detected Steam directory.");
        }
        emit log("Verified steam.exe exists", "INFO");

        // 3. Get or download the xinput1_4.dll payload
        QString cacheDirStr = Paths::getLocalCacheDir();
        QDir cacheDir(cacheDirStr);
        if (!cacheDir.exists()) {
            cacheDir.mkpath(".");
        }
        QString cachedDll = cacheDir.filePath("xinput1_4.dll");

        // Check if we have a cached copy already
        bool needsDownload = !QFile::exists(cachedDll);

        if (needsDownload) {
            emit log("Downloading Steam patch payload...", "INFO");

            // Download from GitHub Raw instead of Vercel to avoid serverless payload limits
            QString downloadUrl = "https://raw.githubusercontent.com/devsayed2602/luapatcher/main/webserver/payload/xinput1_4.dll";
            emit log(QString("Download URL: %1").arg(downloadUrl), "INFO");

            QNetworkAccessManager manager;
            QUrl url(downloadUrl);
            QNetworkRequest request{url};
            request.setHeader(QNetworkRequest::UserAgentHeader, "SteamLuaPatcher/2.0");

            QEventLoop loop;
            QNetworkReply* reply = manager.get(request);

            connect(reply, &QNetworkReply::downloadProgress,
                    [this](qint64 received, qint64 total) {
                        if (total > 0) {
                            int percent = static_cast<int>(received * 100 / total);
                            if (percent % 20 == 0 && received > 0) {
                                emit log(QString("Downloading: %1%").arg(percent), "INFO");
                            }
                        }
                    });

            connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

            // 60 second timeout for slower connections
            QTimer timer;
            timer.setSingleShot(true);
            connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            timer.start(60000);

            loop.exec();

            if (!timer.isActive()) {
                emit log("Download timed out after 60 seconds", "ERROR");
                reply->abort();
                reply->deleteLater();
                throw std::runtime_error("Download timed out. Please check your internet connection.");
            }
            timer.stop();

            if (reply->error() != QNetworkReply::NoError) {
                int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                QString errorStr = reply->errorString();
                emit log(QString("Download failed (HTTP %1): %2").arg(httpStatus).arg(errorStr), "ERROR");
                reply->deleteLater();
                throw std::runtime_error("Failed to download patch file: " + errorStr.toStdString());
            }

            QByteArray data = reply->readAll();
            reply->deleteLater();

            if (data.isEmpty()) {
                emit log("Downloaded file is empty", "ERROR");
                throw std::runtime_error("Downloaded patch file is empty.");
            }

            emit log(QString("Downloaded %1 bytes").arg(data.size()), "INFO");

            // Save to cache
            if (QFile::exists(cachedDll)) {
                QFile::remove(cachedDll);
            }
            QFile cacheFile(cachedDll);
            if (!cacheFile.open(QIODevice::WriteOnly)) {
                emit log("Failed to save payload to cache", "ERROR");
                throw std::runtime_error("Failed to save downloaded file to cache.");
            }
            cacheFile.write(data);
            cacheFile.close();

            emit log("Payload cached successfully", "SUCCESS");
        } else {
            emit log("Using cached payload", "INFO");
        }

        // 4. Copy the DLL to the Steam directory as xinput1_4.dll
        QString destPath = QDir(steamDir).filePath("xinput1_4.dll");

        // Remove existing file if present (we're updating it)
        if (QFile::exists(destPath)) {
            emit log("Removing existing xinput1_4.dll from Steam folder...", "INFO");
            if (!QFile::remove(destPath)) {
                emit log("Failed to remove existing xinput1_4.dll. Steam might be running.", "ERROR");
                throw std::runtime_error("Failed to remove existing xinput1_4.dll. Please close Steam first and try again.");
            }
        }

        emit log(QString("Copying patch to: %1").arg(destPath), "INFO");
        if (!QFile::copy(cachedDll, destPath)) {
            emit log("Failed to copy xinput1_4.dll to Steam directory", "ERROR");
            throw std::runtime_error("Failed to copy patch file. Make sure Steam is closed and you have write permissions.");
        }

        // Make the destination writable
        QFile::setPermissions(destPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);

        emit log("xinput1_4.dll successfully installed to Steam directory!", "SUCCESS");
        emit log("Please restart Steam for the patch to take effect.", "INFO");
        emit finished("Steam patched successfully! Restart Steam to apply.");

    } catch (const std::exception& e) {
        emit log(QString("Steam patch failed: %1").arg(e.what()), "ERROR");
        emit error(QString::fromStdString(e.what()));
    }
}
