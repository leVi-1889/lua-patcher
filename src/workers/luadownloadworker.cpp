#include "luadownloadworker.h"
#include "../config.h"
#include "../utils/paths.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QUrlQuery>
#include <QDateTime>
#include <QRegularExpression>
#include <QMap>
#include <QStandardPaths>
#include <QTextStream>

// Debug file logger - writes every step to Desktop/luapatcher_debug.txt
static void debugLog(const QString& msg) {
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString logPath = QDir(desktopPath).filePath("luapatcher_debug.txt");
    QFile f(logPath);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " [LUA-WORKER] | " << msg << "\n";
        f.close();
    }
}

LuaDownloadWorker::LuaDownloadWorker(const QString& appId, QObject* parent)
    : QThread(parent)
    , m_appId(appId)
{
}

void LuaDownloadWorker::run() {
    try {
        debugLog("========== NEW PRE-MADE INSTALL SESSION ==========");
        debugLog(QString("App ID: %1").arg(m_appId));
        emit log("Starting patch process...", "INFO");
        emit status("Downloading patch...");
        
        // Build URL with timestamp to prevent caching
        QString urlBase = Config::luaFileUrl() + m_appId + ".lua";
        QUrl qurl{urlBase};
        QUrlQuery query;
        query.addQueryItem("_t", QString::number(QDateTime::currentMSecsSinceEpoch()));
        qurl.setQuery(query);
        
        QString cachePath = QDir(Paths::getLocalCacheDir()).filePath(m_appId + ".lua");
        
        emit log(QString("Target App ID: %1").arg(m_appId), "INFO");
        emit log(QString("Download URL: %1").arg(qurl.toString()), "INFO");
        emit log(QString("Cache path: %1").arg(cachePath), "INFO");
        
        // Ensure cache directory exists
        QDir cacheDir(Paths::getLocalCacheDir());
        if (!cacheDir.exists()) {
            emit log("Creating cache directory...", "INFO");
            cacheDir.mkpath(".");
        }
        
        emit log("Initializing network request...", "INFO");
        QNetworkAccessManager manager;
        QNetworkRequest request{qurl};
        QString token = Config::getAccessToken();
        request.setHeader(QNetworkRequest::UserAgentHeader, "SteamLuaPatcher/2.0");
        request.setRawHeader("X-Access-Token", token.toUtf8());
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
        request.setRawHeader("Cache-Control", "no-cache");
        
        emit log("Connecting to server...", "INFO");
        QEventLoop loop;
        QNetworkReply* reply = manager.get(request);
        
        // Connect progress
        connect(reply, &QNetworkReply::downloadProgress, 
                [this](qint64 received, qint64 total) {
                    emit progress(received, total);
                    if (total > 0) {
                        int percent = static_cast<int>(received * 100 / total);
                        if (percent % 25 == 0 && received > 0) {  // Log at 25%, 50%, 75%, 100%
                            emit log(QString("Download progress: %1%").arg(percent), "INFO");
                        }
                    }
                });
        
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(30000); // 30 second timeout
        
        emit log("Downloading Lua patch file...", "INFO");
        loop.exec();
        
        if (!timer.isActive()) {
            emit log("Download timed out after 30 seconds", "ERROR");
            throw std::runtime_error("Connection timed out");
        }
        
        if (reply->error() != QNetworkReply::NoError) {
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (statusCode == 404 || statusCode == 401) {
                emit log("Remote patch not found or unauthorized (Fix system may be removed).", "WARN");
                emit log("Falling back to local generation is recommended.", "INFO");
            }
            emit log(QString("Network error: %1").arg(reply->errorString()), "ERROR");
            throw std::runtime_error(reply->errorString().toStdString());
        }
        
        emit log("Download completed successfully", "SUCCESS");
        
        // Save to cache
        QByteArray data = reply->readAll();
        emit log(QString("Received %1 bytes").arg(data.size()), "INFO");
        
        emit log(QString("Writing to cache: %1").arg(cachePath), "INFO");
        QFile file(cachePath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit log("Failed to open cache file for writing", "ERROR");
            throw std::runtime_error("Failed to write cache file");
        }
        
        file.write(data);
        file.close();
        reply->deleteLater();
        
        emit log("Cache file written successfully", "SUCCESS");
        
        debugLog("Downloading manifests for pre-made patch...");
        downloadManifests(cachePath);
        
        emit finished(cachePath);
        
    } catch (const std::exception& e) {
        debugLog(QString("ERROR: %1").arg(e.what()));
        emit log(QString("Error: %1").arg(e.what()), "ERROR");
        emit error(QString::fromStdString(e.what()));
    }
}

void LuaDownloadWorker::downloadManifests(const QString& luaFile) {
    debugLog("========== downloadManifests() CALLED ==========");
    debugLog(QString("Lua file path: %1").arg(luaFile));
    emit log("Starting automatic manifest download...", "INFO");

    // 1. Read Lua file and extract depot IDs and manifest IDs
    QFile file(luaFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        debugLog(QString("FAILED to open lua file: %1").arg(luaFile));
        emit log("Failed to open Lua file for manifest parsing", "ERROR");
        return;
    }
    QString content = file.readAll();
    file.close();
    debugLog(QString("Lua file size: %1 bytes").arg(content.size()));

    QMap<QString, QString> depotManifestMap;
    // Match setManifestid(depotId, "manifestId")
    QRegularExpression reManifest("setManifestid\\s*\\(\\s*(\\d+)\\s*,\\s*\"(\\d+)\"");
    QRegularExpressionMatchIterator iMan = reManifest.globalMatch(content);
    while (iMan.hasNext()) {
        QRegularExpressionMatch match = iMan.next();
        depotManifestMap[match.captured(1)] = match.captured(2);
    }

    if (depotManifestMap.isEmpty()) {
        debugLog("WARNING: No setManifestid entries found in Lua file!");
        emit log("No depot/manifest IDs found in Lua file.", "WARN");
        return;
    }
    
    debugLog(QString("Found %1 depot/manifest pairs").arg(depotManifestMap.size()));
    emit log(QString("Found %1 depots with manifests in Lua file.").arg(depotManifestMap.size()), "INFO");

    QString steamDir = Config::getSteamDir();
    QString depotCachePath = steamDir + "/depotcache";
    QDir().mkpath(depotCachePath);

    QNetworkAccessManager manager;
    int successCount = 0;

    QStringList baseUrls = {
        "https://raw.githubusercontent.com/qwe213312/k25FCdfEOoEJ42S6/main/",
        "https://ghproxy.com/https://raw.githubusercontent.com/qwe213312/k25FCdfEOoEJ42S6/main/",
        "https://fastly.jsdelivr.net/gh/qwe213312/k25FCdfEOoEJ42S6@main/"
    };

    // 2. Download manifests
    QMapIterator<QString, QString> iMap(depotManifestMap);
    while (iMap.hasNext()) {
        iMap.next();
        QString depotId = iMap.key();
        QString manifestId = iMap.value();

        QString destFile = QString("%1/%2_%3.manifest").arg(depotCachePath, depotId, manifestId);
        
        if (QFile::exists(destFile)) {
            successCount++;
            continue;
        }

        emit status(QString("Downloading manifest for depot %1...").arg(depotId));
        
        bool downloaded = false;
        
        for (int urlIdx = 0; urlIdx < baseUrls.size(); urlIdx++) {
            const QString& baseUrl = baseUrls[urlIdx];
            QString manifestUrl = baseUrl + QString("%1_%2.manifest").arg(depotId, manifestId);
            
            QNetworkRequest mReq;
            mReq.setUrl(QUrl(manifestUrl));
            mReq.setHeader(QNetworkRequest::UserAgentHeader, "SteamLuaPatcher/2.0");
            mReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
            
            QNetworkReply* mReply = manager.get(mReq);
            QEventLoop mLoop;
            connect(mReply, &QNetworkReply::finished, &mLoop, &QEventLoop::quit);
            
            QTimer mTimer;
            mTimer.setSingleShot(true);
            connect(&mTimer, &QTimer::timeout, &mLoop, &QEventLoop::quit);
            mTimer.start(15000); 
            
            mLoop.exec();
            bool timedOut = !mTimer.isActive();
            if (mTimer.isActive()) mTimer.stop();
            
            if (timedOut) {
                mReply->abort();
                mReply->deleteLater();
                continue;
            }
            
            if (mReply->error() == QNetworkReply::NoError) {
                QByteArray mData = mReply->readAll();
                if (mData.size() > 0) {
                    QFile mFile(destFile);
                    if (mFile.open(QIODevice::WriteOnly)) {
                        mFile.write(mData);
                        mFile.close();
                        successCount++;
                        downloaded = true;
                        emit log(QString("Downloaded manifest %1_%2").arg(depotId, manifestId), "SUCCESS");
                    }
                }
            }
            mReply->deleteLater();
            if (downloaded) break;
            
            emit log(QString("Proxy failed, trying next fallback..."), "WARN");
        }
        
        if (!downloaded) {
            emit log(QString("Failed to download manifest %1_%2 from all sources!").arg(depotId, manifestId), "ERROR");
        }
    }
    
    debugLog(QString("========== MANIFEST DOWNLOAD COMPLETE: %1 success ==========").arg(successCount));
    emit log(QString("Finished downloading %1 manifests").arg(successCount), "INFO");
    emit status("Manifest download complete.");
}