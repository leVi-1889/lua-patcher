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
        
        // 1. Gracefully shutdown Steam
        emit log("Shutting down Steam gracefully...", "INFO");
        if (QFile::exists(steamExe)) {
            QProcess::execute(steamExe, QStringList() << "-shutdown");
        }
        
        // Wait up to 10 seconds for Steam to close normally
        bool steamClosed = false;
        for (int i = 0; i < 10; ++i) {
            QProcess checkProc;
            checkProc.start("tasklist", QStringList() << "/FI" << "IMAGENAME eq steam.exe" << "/NH");
            checkProc.waitForFinished(2000);
            QString output = QString::fromLocal8Bit(checkProc.readAllStandardOutput());
            if (!output.contains("steam.exe", Qt::CaseInsensitive)) {
                steamClosed = true;
                break;
            }
            QThread::sleep(1);
        }
        
        // Force kill if it didn't close
        if (!steamClosed) {
            emit log("Steam is taking too long to close. Force terminating...", "WARNING");
            QStringList steamProcesses = {"steam.exe", "steamwebhelper.exe", "steamservice.exe", "GameOverlayUI.exe"};
            for (const QString& proc : steamProcesses) {
                QProcess killProcess;
                killProcess.start("taskkill", QStringList() << "/F" << "/IM" << proc);
                killProcess.waitForFinished(2000);
            }
            QThread::sleep(2); // Wait for processes to die
        }
        
        emit log("Steam closed successfully.", "INFO");
        
        // 2. Restart Steam with the correct working directory
        emit log("Restarting Steam...", "INFO");
        
        if (QFile::exists(steamExe)) {
            // CRITICAL FIX: Pass steamDir as the working directory.
            // If we don't, Steam inherits LuaPatcher's working directory, 
            // and the xinput1_4.dll payload won't be able to find the config/stplug-in/ lua files.
            qint64 pid = 0;
            bool launched = QProcess::startDetached(
                steamExe, 
                QStringList() << "-silent", // Start silently to avoid interrupting the user
                steamDir, // Set Working Directory
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
            // Fallback
            QProcess::startDetached("cmd", QStringList() << "/c" << "start" << "steam://open/main");
            emit log("Restart command sent via fallback.", "INFO");
            emit finished("Restart command sent.");
        }
        
    } catch (const std::exception& e) {
        emit error(QString::fromStdString(e.what()));
    } catch (...) {
        emit error("Unknown error occurred");
    }
}
