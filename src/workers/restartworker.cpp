#include "restartworker.h"
#include "../config.h"
#include <QProcess>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QFileInfo>

RestartWorker::RestartWorker(QObject* parent)
    : QThread(parent)
{
}

void RestartWorker::run() {
    try {
        QString steamDir = Config::getSteamDir();
        QString steamExe = Config::getSteamExePath();
        QStringList steamProcesses = {"steam.exe", "steamwebhelper.exe", "steamservice.exe", "GameOverlayUI.exe"};
        
        // 1. Gracefully shutdown Steam
        emit log("Shutting down Steam gracefully...", "INFO");
        if (QFile::exists(steamExe)) {
            QProcess::execute(steamExe, QStringList() << "-shutdown");
        }
        
        // Wait up to 10 seconds for ALL Steam processes to close
        emit log("Waiting for all Steam processes to terminate...", "INFO");
        bool allClosed = false;
        for (int i = 0; i < 15; ++i) {
            bool foundAny = false;
            for (const QString& proc : steamProcesses) {
                QProcess checkProc;
                checkProc.start("tasklist", QStringList() << "/FI" << QString("IMAGENAME eq %1").arg(proc) << "/NH");
                checkProc.waitForFinished(2000);
                if (QString::fromLocal8Bit(checkProc.readAllStandardOutput()).contains(proc, Qt::CaseInsensitive)) {
                    foundAny = true;
                    break;
                }
            }
            
            if (!foundAny) {
                allClosed = true;
                break;
            }
            QThread::sleep(1);
        }
        
        // 2. CRITICAL: Clear the ActiveProcess and session state
        // This prevents the new Steam instance from thinking an old one is still running.
        emit log("Clearing Steam session state and cache...", "INFO");
        QSettings reg("HKEY_CURRENT_USER\\Software\\Valve\\Steam\\ActiveProcess", QSettings::NativeFormat);
        reg.remove("pid");
        reg.remove("ActiveUser");
        reg.remove("StartupFinished");

        // 3. Optional: Clear Steam AppCache
        // Many library synchronization issues are solved by clearing the appcache.
        QString appCacheDir = QDir(steamDir).filePath("appcache");
        if (QDir(appCacheDir).exists()) {
            emit log("Clearing Steam appcache for fresh sync...", "INFO");
            // We only delete specific cache files to avoid full re-downloads
            QFile::remove(QDir(appCacheDir).filePath("appinfo.vdf"));
            QFile::remove(QDir(appCacheDir).filePath("packageinfo.vdf"));
        }

        // 4. Verify DLL integrity before launch
        QString payloadPath = QDir(steamDir).filePath("xinput1_4.dll");
        if (!QFile::exists(payloadPath)) {
            emit log("Warning: xinput1_4.dll was missing. Re-applying from cache...", "WARNING");
            QString cachedDll = QDir(Paths::getLocalCacheDir()).filePath("xinput1_4.dll");
            if (QFile::exists(cachedDll)) {
                QFile::copy(cachedDll, payloadPath);
            }
        }

        emit log("Steam environment cleaned.", "INFO");
        
        // 4. Restart Steam
        emit log("Restarting Steam...", "INFO");
        
        if (QFile::exists(steamExe)) {
            qint64 pid = 0;
            // Use -silent and set the CWD to steamDir
            bool launched = QProcess::startDetached(
                steamExe, 
                QStringList() << "-silent", 
                steamDir, 
                &pid
            );
            
            if (launched) {
                emit log("Steam launched successfully!", "SUCCESS");
                emit finished("Steam restarted successfully!");
            } else {
                emit log("Failed to launch Steam process.", "ERROR");
                emit error("Could not restart Steam process.");
            }
        } else {
            QProcess::startDetached("cmd", QStringList() << "/c" << "start" << "steam://open/main");
            emit finished("Restart command sent via URI.");
        }
        
    } catch (const std::exception& e) {
        emit error(QString::fromStdString(e.what()));
    } catch (...) {
        emit error("Unknown error occurred");
    }
}
