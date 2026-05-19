#include "restartworker.h"
#include "../config.h"
#include "../utils/paths.h"
#include <QProcess>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

RestartWorker::RestartWorker(QObject* parent)
    : QThread(parent)
{
}

void RestartWorker::run() {
    try {
        QString steamDir = Config::getSteamDir();
        QString steamExe = Config::getSteamExePath();
        
        // 1. Kill Steam
        emit log("Shutting down Steam...", "INFO");
        QProcess killProcess;
        killProcess.start("taskkill", QStringList() << "/F" << "/IM" << "steam.exe");
        killProcess.waitForFinished();
        
        // Wait for processes to fully exit
        QThread::sleep(2);
        
        // 2. Clear stale appcache so DLL can inject fresh license data
        emit log("Clearing Steam cache...", "INFO");
        QString appCacheDir = QDir(steamDir).filePath("appcache");
        QFile::remove(QDir(appCacheDir).filePath("appinfo.vdf"));
        QFile::remove(QDir(appCacheDir).filePath("packageinfo.vdf"));
        
        // 3. Clear stale registry session
        QSettings reg("HKEY_CURRENT_USER\\Software\\Valve\\Steam\\ActiveProcess", QSettings::NativeFormat);
        reg.remove("pid");
        reg.remove("ActiveUser");
        reg.remove("StartupFinished");
        reg.sync();
        
        // 4. Restart Steam with correct working directory
        emit log("Restarting Steam...", "INFO");
        if (QFile::exists(steamExe)) {
            QProcess::startDetached(steamExe, QStringList(), steamDir);
            emit finished("Steam launched!");
        } else {
            QProcess::startDetached("cmd", QStringList() << "/c" << "start" << "steam://open/main");
            emit finished("Restart command sent.");
        }
        
    } catch (const std::exception& e) {
        emit error(QString::fromStdString(e.what()));
    } catch (...) {
        emit error("Unknown error occurred");
    }
}
