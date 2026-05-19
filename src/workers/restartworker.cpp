#include "restartworker.h"
#include "../config.h"
#include "../utils/paths.h"
#include <QProcess>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QDateTime>
#include <QCoreApplication>

RestartWorker::RestartWorker(QObject* parent)
    : QThread(parent)
{
}

// Helper: get list of running Steam-related processes with details
static QString getRunningProcesses(const QStringList& names) {
    QString result;
    for (const QString& name : names) {
        QProcess proc;
        proc.start("tasklist", QStringList() << "/FI" << QString("IMAGENAME eq %1").arg(name) << "/V" << "/NH");
        proc.waitForFinished(3000);
        QString output = QString::fromLocal8Bit(proc.readAllStandardOutput()).trimmed();
        if (output.contains(name, Qt::CaseInsensitive)) {
            result += "  [RUNNING] " + output + "\n";
        } else {
            result += "  [NOT RUNNING] " + name + "\n";
        }
    }
    return result;
}

void RestartWorker::run() {
    // Setup log file in the app's own directory
    QString logPath = QCoreApplication::applicationDirPath() + "/restart_debug.log";
    QFile logFile(logPath);
    
    // Append to existing log so we can compare multiple restarts
    logFile.open(QIODevice::Append | QIODevice::Text);
    
    auto writeLog = [&](const QString& msg) {
        QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") + " | " + msg + "\n";
        logFile.write(line.toUtf8());
        logFile.flush();
    };

    QStringList steamProcesses = {"steam.exe", "steamwebhelper.exe", "steamservice.exe", "GameOverlayUI.exe"};
    QString steamDir = Config::getSteamDir();
    QString steamExe = Config::getSteamExePath();

    try {
        writeLog("========== RESTART BEGIN ==========");
        writeLog("Steam Dir: " + steamDir);
        writeLog("Steam Exe: " + steamExe);
        writeLog("Steam Exe Exists: " + QString(QFile::exists(steamExe) ? "YES" : "NO"));
        
        // 1. LOG: xinput1_4.dll state BEFORE restart
        QString dllPath = QDir(steamDir).filePath("xinput1_4.dll");
        QFileInfo dllInfo(dllPath);
        writeLog("--- DLL STATE (BEFORE KILL) ---");
        writeLog("  Path: " + dllPath);
        writeLog("  Exists: " + QString(dllInfo.exists() ? "YES" : "NO"));
        if (dllInfo.exists()) {
            writeLog("  Size: " + QString::number(dllInfo.size()) + " bytes");
            writeLog("  Modified: " + dllInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss"));
        }

        // 2. LOG: stplug-in directory state
        QString pluginDir = QDir(steamDir).filePath("config/stplug-in");
        writeLog("--- STPLUG-IN STATE ---");
        writeLog("  Path: " + pluginDir);
        writeLog("  Exists: " + QString(QDir(pluginDir).exists() ? "YES" : "NO"));
        if (QDir(pluginDir).exists()) {
            QDir dir(pluginDir);
            QStringList luaFiles = dir.entryList(QStringList() << "*.lua", QDir::Files);
            writeLog("  Lua file count: " + QString::number(luaFiles.size()));
            for (const QString& f : luaFiles) {
                QFileInfo fi(dir.filePath(f));
                writeLog("    " + f + " (" + QString::number(fi.size()) + " bytes, modified " + fi.lastModified().toString("yyyy-MM-dd hh:mm:ss") + ")");
            }
        }
        
        // 3. LOG: appcache state
        QString appCacheDir = QDir(steamDir).filePath("appcache");
        writeLog("--- APPCACHE STATE (BEFORE KILL) ---");
        QString appInfoPath = QDir(appCacheDir).filePath("appinfo.vdf");
        QString pkgInfoPath = QDir(appCacheDir).filePath("packageinfo.vdf");
        QFileInfo appInfoFile(appInfoPath);
        QFileInfo pkgInfoFile(pkgInfoPath);
        writeLog("  appinfo.vdf: " + QString(appInfoFile.exists() ? "EXISTS (" + QString::number(appInfoFile.size()) + " bytes, " + appInfoFile.lastModified().toString("yyyy-MM-dd hh:mm:ss") + ")" : "MISSING"));
        writeLog("  packageinfo.vdf: " + QString(pkgInfoFile.exists() ? "EXISTS (" + QString::number(pkgInfoFile.size()) + " bytes, " + pkgInfoFile.lastModified().toString("yyyy-MM-dd hh:mm:ss") + ")" : "MISSING"));

        // 4. LOG: Registry ActiveProcess state
        writeLog("--- REGISTRY STATE (BEFORE KILL) ---");
        QSettings reg("HKEY_CURRENT_USER\\Software\\Valve\\Steam\\ActiveProcess", QSettings::NativeFormat);
        writeLog("  pid: " + reg.value("pid", "N/A").toString());
        writeLog("  ActiveUser: " + reg.value("ActiveUser", "N/A").toString());
        writeLog("  SteamClientDll: " + reg.value("SteamClientDll", "N/A").toString());
        writeLog("  SteamClientDll64: " + reg.value("SteamClientDll64", "N/A").toString());

        // 5. LOG: Running processes BEFORE kill
        writeLog("--- PROCESSES (BEFORE KILL) ---");
        writeLog(getRunningProcesses(steamProcesses));

        // 6. KILL Steam
        writeLog("--- KILLING STEAM ---");
        QProcess killProcess;
        killProcess.start("taskkill", QStringList() << "/F" << "/IM" << "steam.exe");
        killProcess.waitForFinished();
        QString killOutput = QString::fromLocal8Bit(killProcess.readAllStandardOutput()).trimmed();
        QString killError = QString::fromLocal8Bit(killProcess.readAllStandardError()).trimmed();
        writeLog("  taskkill exit code: " + QString::number(killProcess.exitCode()));
        writeLog("  taskkill stdout: " + killOutput);
        writeLog("  taskkill stderr: " + killError);
        emit log("Shutting down Steam...", "INFO");

        // Wait 2 seconds
        writeLog("  Waiting 2 seconds...");
        QThread::sleep(2);

        // 7. LOG: Running processes AFTER kill
        writeLog("--- PROCESSES (AFTER KILL, 2s wait) ---");
        writeLog(getRunningProcesses(steamProcesses));

        // Check if any survivors
        bool hasSurvivors = false;
        for (const QString& proc : steamProcesses) {
            QProcess checkProc;
            checkProc.start("tasklist", QStringList() << "/FI" << QString("IMAGENAME eq %1").arg(proc) << "/NH");
            checkProc.waitForFinished(2000);
            if (QString::fromLocal8Bit(checkProc.readAllStandardOutput()).contains(proc, Qt::CaseInsensitive)) {
                hasSurvivors = true;
                writeLog("  WARNING: " + proc + " is STILL ALIVE after taskkill!");
            }
        }
        writeLog("  Survivors present: " + QString(hasSurvivors ? "YES" : "NO"));

        // 8. LOG: Registry state AFTER kill
        writeLog("--- REGISTRY STATE (AFTER KILL) ---");
        QSettings reg2("HKEY_CURRENT_USER\\Software\\Valve\\Steam\\ActiveProcess", QSettings::NativeFormat);
        reg2.sync(); // Force re-read
        writeLog("  pid: " + reg2.value("pid", "N/A").toString());
        writeLog("  ActiveUser: " + reg2.value("ActiveUser", "N/A").toString());

        // 9. LAUNCH Steam
        writeLog("--- LAUNCHING STEAM ---");
        if (QFile::exists(steamExe)) {
            writeLog("  Method: QProcess::startDetached");
            writeLog("  Exe: " + steamExe);
            writeLog("  Args: (none)");
            QProcess::startDetached(steamExe, QStringList());
            writeLog("  Launch: OK");
            emit log("Steam launched!", "INFO");

            // Wait 5 seconds then log post-launch state
            QThread::sleep(5);
            writeLog("--- PROCESSES (5s AFTER LAUNCH) ---");
            writeLog(getRunningProcesses(steamProcesses));

            // Check DLL state after launch
            writeLog("--- DLL STATE (AFTER LAUNCH) ---");
            QFileInfo dllAfter(dllPath);
            writeLog("  Exists: " + QString(dllAfter.exists() ? "YES" : "NO"));
            if (dllAfter.exists()) {
                writeLog("  Size: " + QString::number(dllAfter.size()) + " bytes");
                writeLog("  Modified: " + dllAfter.lastModified().toString("yyyy-MM-dd hh:mm:ss"));
            }

            emit finished("Steam launched!");
        } else {
            writeLog("  Method: steam:// URI fallback");
            QProcess::startDetached("cmd", QStringList() << "/c" << "start" << "steam://open/main");
            emit finished("Restart command sent.");
        }

        writeLog("========== RESTART END ==========\n");

    } catch (const std::exception& e) {
        writeLog("EXCEPTION: " + QString::fromStdString(e.what()));
        writeLog("========== RESTART END (ERROR) ==========\n");
        emit error(QString::fromStdString(e.what()));
    } catch (...) {
        writeLog("EXCEPTION: Unknown error");
        writeLog("========== RESTART END (ERROR) ==========\n");
        emit error("Unknown error occurred");
    }

    logFile.close();
}
