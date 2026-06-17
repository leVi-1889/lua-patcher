#include "generatorworker.h"
#include "../utils/paths.h"
#include "../config.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <QStandardPaths>
#include <QDateTime>
#include <QTextStream>

// Debug file logger - writes every step to Desktop/luapatcher_debug.txt
static void debugLog(const QString& msg) {
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString logPath = QDir(desktopPath).filePath("luapatcher_debug.txt");
    QFile f(logPath);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " | " << msg << "\n";
        f.close();
    }
}
GeneratorWorker::GeneratorWorker(const QString& appId, QObject* parent)
    : QThread(parent)
    , m_appId(appId)
{
}

void GeneratorWorker::run() {
    try {
        debugLog("========== NEW INSTALL SESSION ==========");
        debugLog(QString("App ID: %1").arg(m_appId));
        emit log("Starting generation process...", "INFO");
        emit status("Fetching game data...");
        
        // Build URL
        QString url = QString("%1/api/free-download?appid=%2&user=luamanifest")
            .arg(Config::WEBSERVER_BASE_URL)
            .arg(m_appId);
        QString cacheDirStr = Paths::getLocalCacheDir();
        QString archivePath = QDir(cacheDirStr).filePath(m_appId + "_gen.zip");
        QString extractDir = QDir(cacheDirStr).filePath(m_appId + "_gen");
        
        debugLog(QString("Request URL: %1").arg(url));
        debugLog(QString("Cache dir: %1").arg(cacheDirStr));
        debugLog(QString("Archive path: %1").arg(archivePath));
        debugLog(QString("Extract dir: %1").arg(extractDir));
        
        emit log(QString("Target App ID: %1").arg(m_appId), "INFO");
        emit log(QString("Request URL: %1").arg(url), "INFO");
        emit log(QString("Cache directory: %1").arg(cacheDirStr), "INFO");
        
        // Ensure cache directory exists
        QDir cacheDir(cacheDirStr);
        if (!cacheDir.exists()) {
            if (!cacheDir.mkpath(".")) {
                emit log("Failed to create cache directory", "ERROR");
                throw std::runtime_error("Failed to create cache directory");
            }
        }
        emit log("Cache directory ready", "INFO");
        
        // Clean up previous runs
        if (QFile::exists(archivePath)) {
            emit log("Removing previous archive...", "INFO");
            QFile::remove(archivePath);
        }
        if (QDir(extractDir).exists()) {
            emit log("Removing previous extraction directory...", "INFO");
            QDir(extractDir).removeRecursively();
        }
        
        emit log("Sending HTTP request...", "INFO");
        QNetworkAccessManager manager;
        QNetworkRequest request;
        request.setUrl(QUrl(url));
        request.setHeader(QNetworkRequest::UserAgentHeader, "genshinreya");
        request.setRawHeader("Accept", "*/*");
        
        QEventLoop loop;
        QNetworkReply* reply = manager.get(request);
        
        connect(reply, &QNetworkReply::downloadProgress, 
                [this](qint64 received, qint64 total) {
                    emit progress(received, total);
                    if (total > 0) {
                        emit log(QString("Downloading: %1 / %2 bytes").arg(received).arg(total), "INFO");
                    }
                });
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        
        // Timeout - 60 seconds for slower connections
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(60000); 
        
        loop.exec();
        
        if (timer.isActive()) {
            timer.stop();
        } else {
            emit log("Request timed out after 60 seconds", "ERROR");
            reply->abort();
            reply->deleteLater();
            throw std::runtime_error("Connection timed out");
        }
        
        if (reply->error() != QNetworkReply::NoError) {
            QString errorStr = reply->errorString();
            int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            emit log(QString("Network error (HTTP %1): %2").arg(httpStatus).arg(errorStr), "ERROR");
            reply->deleteLater();
            throw std::runtime_error(errorStr.toStdString());
        }
        
        QByteArray data = reply->readAll();
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        emit log(QString("Response received: HTTP %1, %2 bytes").arg(httpStatus).arg(data.size()), "INFO");
        reply->deleteLater();
        
        if (data.isEmpty()) {
            emit log("Response is empty", "ERROR");
            throw std::runtime_error("Empty response from server");
        }
        
        // Check if response is a ZIP (starts with PK)
        if (data.startsWith("PK")) {
            emit log("Received ZIP archive. Saving to disk...", "INFO");
            
            QFile file(archivePath);
            if (!file.open(QIODevice::WriteOnly)) {
                emit log(QString("Failed to open file for writing: %1").arg(archivePath), "ERROR");
                throw std::runtime_error("Failed to save zip file");
            }
            qint64 written = file.write(data);
            file.close();
            
            emit log(QString("Archive saved: %1 bytes written to %2").arg(written).arg(archivePath), "INFO");
            
            // Create extraction directory
            QDir extractDirObj(extractDir);
            if (!extractDirObj.exists()) {
                if (!extractDirObj.mkpath(".")) {
                    emit log("Failed to create extraction directory", "ERROR");
                    throw std::runtime_error("Failed to create extraction directory");
                }
            }
            
            // Extract using PowerShell (Windows)
            emit log("Extracting archive using PowerShell...", "INFO");
            QProcess process;
            process.setProcessChannelMode(QProcess::MergedChannels);
            
            // Use native path separators for Windows
            QString nativeArchivePath = QDir::toNativeSeparators(archivePath);
            QString nativeExtractDir = QDir::toNativeSeparators(extractDir);
            
            QString cmd = QString("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                .arg(nativeArchivePath)
                .arg(nativeExtractDir);
            
            emit log(QString("PowerShell command: %1").arg(cmd), "INFO");
            
            process.start("powershell", QStringList() << "-NoProfile" << "-NonInteractive" << "-Command" << cmd);
            
            if (!process.waitForStarted(5000)) {
                emit log("Failed to start PowerShell process", "ERROR");
                throw std::runtime_error("Failed to start extraction process");
            }
            
            if (!process.waitForFinished(30000)) {
                emit log("Extraction process timed out", "ERROR");
                process.kill();
                throw std::runtime_error("Extraction timed out");
            }
            
            QString processOutput = QString::fromUtf8(process.readAll());
            if (!processOutput.isEmpty()) {
                emit log(QString("PowerShell output: %1").arg(processOutput.trimmed()), "INFO");
            }
            
            if (process.exitCode() != 0) {
                emit log(QString("Extraction failed with exit code: %1").arg(process.exitCode()), "ERROR");
                throw std::runtime_error("Failed to extract archive");
            }
            
            emit log("Archive extracted successfully", "SUCCESS");
            
            // Find Lua file in extraction directory
            QDir dir(extractDir);
            QStringList filters;
            filters << "*.lua";
            dir.setNameFilters(filters);
            
            // Search recursively
            QStringList files = dir.entryList(QDir::Files);
            
            // If no files found at top level, search subdirectories
            if (files.isEmpty()) {
                emit log("No Lua files at top level, searching subdirectories...", "INFO");
                QDirIterator it(extractDir, filters, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    QString foundFile = it.next();
                    files.append(foundFile);
                    emit log(QString("Found: %1").arg(foundFile), "INFO");
                }
            } else {
                // Convert to full paths
                QStringList fullPaths;
                for (const QString& f : files) {
                    fullPaths.append(dir.filePath(f));
                }
                files = fullPaths;
            }
            
            if (files.isEmpty()) {
                emit log("No .lua file found in the archive", "ERROR");
                throw std::runtime_error("No .lua file found in the archive");
            }
            
            QString luaFile = files.first();
            emit log(QString("Found Lua file: %1").arg(luaFile), "SUCCESS");
            debugLog(QString("Found Lua file: %1").arg(luaFile));
            
            // Log Lua file contents for debugging
            {
                QFile luaDbg(luaFile);
                if (luaDbg.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QString luaContent = luaDbg.readAll();
                    luaDbg.close();
                    debugLog(QString("=== LUA FILE CONTENTS (first 2000 chars) ==="));
                    debugLog(luaContent.left(2000));
                    debugLog("=== END LUA FILE ===");
                } else {
                    debugLog("FAILED to open Lua file for debug read!");
                }
            }
            
            // Now copy directly to plugin folder instead of just returning path
            QStringList targetDirs = Config::getAllSteamPluginDirs();
            debugLog(QString("getAllSteamPluginDirs returned %1 dirs").arg(targetDirs.size()));
            for (const QString& d : targetDirs) debugLog(QString("  plugin dir: %1").arg(d));
            
            if (targetDirs.isEmpty()) {
                targetDirs.append(Config::getSteamPluginDir());
                debugLog(QString("Using fallback plugin dir: %1").arg(targetDirs.first()));
                emit log("No plugin paths found, using default path", "WARN");
            }
            
            bool atLeastOneSuccess = false;
            QString destFile;
            
            for (const QString& pluginDir : targetDirs) {
                emit log(QString("Checking plugin folder: %1").arg(pluginDir), "INFO");
                debugLog(QString("Installing to plugin folder: %1").arg(pluginDir));
                
                QDir pDir(pluginDir);
                if (!pDir.exists()) {
                    emit log(QString("Creating plugin folder: %1").arg(pluginDir), "INFO");
                    if (!pDir.mkpath(".")) {
                        emit log(QString("Failed to create folder: %1").arg(pluginDir), "WARN");
                        debugLog(QString("FAILED to create plugin folder: %1").arg(pluginDir));
                        continue;
                    }
                }
                
                destFile = pDir.filePath(m_appId + ".lua");
                debugLog(QString("Destination file: %1").arg(destFile));
                
                // Remove existing file
                if (QFile::exists(destFile)) {
                    emit log(QString("Removing existing: %1").arg(destFile), "INFO");
                    QFile::remove(destFile);
                }
                
                // Copy the lua file
                emit log(QString("Copying to: %1").arg(destFile), "INFO");
                if (QFile::copy(luaFile, destFile)) {
                    emit log(QString("Successfully installed to: %1").arg(destFile), "SUCCESS");
                    debugLog(QString("SUCCESS: Copied lua to %1").arg(destFile));
                    atLeastOneSuccess = true;
                } else {
                    emit log(QString("Failed to copy to: %1").arg(destFile), "WARN");
                    debugLog(QString("FAILED to copy lua to: %1").arg(destFile));
                }
            }
            
            // Cleanup
            emit log("Cleaning up temporary files...", "INFO");
            QFile::remove(archivePath);
            QDir(extractDir).removeRecursively();
            
            if (!atLeastOneSuccess) {
                debugLog("FAILED: Could not install lua to any plugin folder!");
                throw std::runtime_error("Failed to install Lua file to any plugin folder");
            }
            
            emit log("Generation and installation complete!", "SUCCESS");
            debugLog(QString("Lua installed. Now calling downloadManifests with: %1").arg(destFile));
            
            // Download manifests automatically
            downloadManifests(destFile);
            
            debugLog("downloadManifests() returned. Emitting finished.");
            emit finished(destFile);
            
        } else {
            // Not a ZIP - log the response for debugging
            emit log("Response is not a ZIP archive", "ERROR");
            QString preview = QString::fromUtf8(data.left(500));
            emit log(QString("Response preview: %1").arg(preview), "WARN");
            
            // Check for common error indicators
            if (data.contains("error") || data.contains("Error")) {
                emit log("Server returned an error response", "ERROR");
            }
            if (data.contains("<!DOCTYPE") || data.contains("<html")) {
                emit log("Server returned HTML instead of ZIP (possibly a redirect or error page)", "ERROR");
            }
            
            throw std::runtime_error("Unexpected response format (not a ZIP file)");
        }
        
    } catch (const std::exception& e) {
        emit log(QString("Generation failed: %1").arg(e.what()), "ERROR");
        emit error(QString::fromStdString(e.what()));
    }
}

// Helper to validate manifest
static bool isInvalidManifestGen(const QByteArray& data) {
    QString str = QString::fromUtf8(data.left(100)).trimmed().toLower();
    return str.startsWith("<!doctype") || str.startsWith("<html") || str.contains("404: not found") || str.contains("package size exceeded") || data.isEmpty();
}

void GeneratorWorker::downloadManifests(const QString& luaFile) {
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
    debugLog(QString("Lua file first 500 chars: %1").arg(content.left(500)));

    QMap<QString, QString> depotManifestMap;
    // Match setManifestid(depotId, "manifestId")
    QRegularExpression reManifest("setManifestid\\s*\\(\\s*(\\d+)\\s*,\\s*\"(\\d+)\"");
    QRegularExpressionMatchIterator iMan = reManifest.globalMatch(content);
    while (iMan.hasNext()) {
        QRegularExpressionMatch match = iMan.next();
        debugLog(QString("  Regex match: depot=%1 manifest=%2").arg(match.captured(1), match.captured(2)));
        depotManifestMap[match.captured(1)] = match.captured(2);
    }

    // Also try addappid pattern for depot IDs (for reference)
    QRegularExpression reAddApp("addappid\\s*\\(\\s*(\\d+)");
    QRegularExpressionMatchIterator iAdd = reAddApp.globalMatch(content);
    debugLog("--- addappid entries found ---");
    while (iAdd.hasNext()) {
        QRegularExpressionMatch match = iAdd.next();
        debugLog(QString("  addappid: %1").arg(match.captured(1)));
    }

    if (depotManifestMap.isEmpty()) {
        debugLog("WARNING: No setManifestid entries found in Lua file!");
        debugLog("Full Lua content for diagnosis:");
        debugLog(content);
        emit log("No depot/manifest IDs found in Lua file.", "WARN");
        return;
    }
    
    debugLog(QString("Found %1 depot/manifest pairs:").arg(depotManifestMap.size()));
    QMapIterator<QString, QString> dbgIt(depotManifestMap);
    while (dbgIt.hasNext()) {
        dbgIt.next();
        debugLog(QString("  depot %1 -> manifest %2").arg(dbgIt.key(), dbgIt.value()));
    }
    emit log(QString("Found %1 depots with manifests in Lua file.").arg(depotManifestMap.size()), "INFO");

    QString steamDir = Config::getSteamDir();
    debugLog(QString("Steam dir: %1").arg(steamDir));
    QString depotCachePath = steamDir + "/depotcache";
    debugLog(QString("Depot cache path: %1").arg(depotCachePath));
    debugLog(QString("Depot cache exists: %1").arg(QDir(depotCachePath).exists() ? "YES" : "NO"));
    QDir().mkpath(depotCachePath);
    debugLog(QString("Depot cache exists after mkpath: %1").arg(QDir(depotCachePath).exists() ? "YES" : "NO"));

    // List existing manifest files in depotcache
    QDir depotDir(depotCachePath);
    QStringList existingManifests = depotDir.entryList(QStringList() << "*.manifest", QDir::Files);
    debugLog(QString("Existing manifests in depotcache: %1").arg(existingManifests.size()));
    for (const QString& em : existingManifests) {
        debugLog(QString("  existing: %1").arg(em));
    }

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
        debugLog(QString("--- Processing depot %1 manifest %2 ---").arg(depotId, manifestId));
        debugLog(QString("  dest file: %1").arg(destFile));
        
        if (QFile::exists(destFile)) {
            QFile existingFile(destFile);
            if (existingFile.open(QIODevice::ReadOnly)) {
                QByteArray header = existingFile.read(100);
                existingFile.close();
                if (isInvalidManifestGen(header)) {
                    debugLog(QString("  DELETING existing invalid HTML manifest: %1").arg(destFile));
                    QFile::remove(destFile);
                } else {
                    QFileInfo fi(destFile);
                    debugLog(QString("  SKIPPED: already exists (%1 bytes)").arg(fi.size()));
                    successCount++;
                    continue;
                }
            }
        }
        debugLog("  File does NOT exist, will download.");

        emit status(QString("Downloading manifest for depot %1...").arg(depotId));
        
        bool downloaded = false;
        
        for (int urlIdx = 0; urlIdx < baseUrls.size(); urlIdx++) {
            const QString& baseUrl = baseUrls[urlIdx];
            QString manifestUrl = baseUrl + QString("%1_%2.manifest").arg(depotId, manifestId);
            debugLog(QString("  Trying URL [%1]: %2").arg(urlIdx).arg(manifestUrl));
            
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
                debugLog(QString("  TIMEOUT on URL [%1]").arg(urlIdx));
                mReply->abort();
                mReply->deleteLater();
                emit log(QString("Timeout downloading from source %1").arg(urlIdx), "WARN");
                continue;
            }
            
            int httpStatus = mReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            debugLog(QString("  HTTP status: %1, error: %2, errorString: %3")
                .arg(httpStatus)
                .arg(mReply->error())
                .arg(mReply->errorString()));
            
            if (mReply->error() == QNetworkReply::NoError) {
                QByteArray mData = mReply->readAll();
                debugLog(QString("  Response size: %1 bytes").arg(mData.size()));
                if (isInvalidManifestGen(mData)) {
                    debugLog("  ERROR: Downloaded data is an HTML error page, not a manifest!");
                } else if (mData.size() > 0) {
                    QFile mFile(destFile);
                    if (mFile.open(QIODevice::WriteOnly)) {
                        qint64 written = mFile.write(mData);
                        mFile.close();
                        debugLog(QString("  Written %1 bytes to %2").arg(written).arg(destFile));
                        debugLog(QString("  File exists after write: %1").arg(QFile::exists(destFile) ? "YES" : "NO"));
                        successCount++;
                        downloaded = true;
                        emit log(QString("Downloaded manifest %1_%2").arg(depotId, manifestId), "SUCCESS");
                    } else {
                        debugLog(QString("  FAILED to open dest file for writing: %1").arg(mFile.errorString()));
                    }
                } else {
                    debugLog("  Response was empty (0 bytes)");
                }
            } else {
                debugLog(QString("  Network error: %1").arg(mReply->errorString()));
            }
            
            mReply->deleteLater();
            
            if (downloaded) break;
            
            emit log(QString("Proxy failed, trying next fallback..."), "WARN");
        }
        
        if (!downloaded) {
            debugLog(QString("  FAILED to download from ALL sources!"));
            emit log(QString("Failed to download manifest %1_%2 from all sources!").arg(depotId, manifestId), "ERROR");
        }
    }
    
    debugLog(QString("========== MANIFEST DOWNLOAD COMPLETE: %1 success ==========").arg(successCount));
    
    // Final check - list what's in depotcache now
    QStringList finalManifests = depotDir.entryList(QStringList() << "*.manifest", QDir::Files);
    debugLog(QString("Final manifests in depotcache: %1").arg(finalManifests.size()));
    for (const QString& fm : finalManifests) {
        debugLog(QString("  final: %1").arg(fm));
    }
    
    emit log(QString("Finished downloading %1 manifests").arg(successCount), "INFO");
    emit status("Manifest download complete.");
}
