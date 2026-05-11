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

        // ── Step 1: Kill all Steam processes ──
        emit log("Terminating Steam processes...", "INFO");

        QStringList steamProcesses = {
            "steam.exe",
            "steamwebhelper.exe",
            "steamservice.exe",
            "GameOverlayUI.exe"
        };

        for (const QString& proc : steamProcesses) {
            QProcess kill;
            kill.start("taskkill", QStringList() << "/F" << "/IM" << proc);
            kill.waitForFinished(5000);
        }

        // Wait for processes to fully exit
        emit log("Waiting for processes to exit...", "INFO");
        QThread::sleep(4);

        // Verify steam.exe is actually gone
        QProcess checkProc;
        checkProc.start("tasklist", QStringList() << "/FI" << "IMAGENAME eq steam.exe" << "/NH");
        checkProc.waitForFinished(5000);
        QString taskOutput = QString::fromLocal8Bit(checkProc.readAllStandardOutput());
        if (taskOutput.contains("steam.exe", Qt::CaseInsensitive)) {
            emit log("Steam still running, waiting longer...", "WARNING");
            QThread::sleep(3);
        }

        // ── Step 2: Verify DLL payload is still in place ──
        emit log("Verifying DLL payload...", "INFO");

        if (!steamDir.isEmpty()) {
            QString dllPath = QDir(steamDir).filePath("xinput1_4.dll");
            if (!QFile::exists(dllPath)) {
                emit log("WARNING: xinput1_4.dll missing from Steam directory!", "WARNING");
                emit log("The payload may need to be reinstalled via Steam Tools.", "WARNING");
            } else {
                QFileInfo fi(dllPath);
                // Original xinput1_4.dll from Windows is ~50KB; the payload is larger
                if (fi.size() < 100000) {
                    emit log("WARNING: xinput1_4.dll appears to be the original system DLL (too small).", "WARNING");
                    emit log("Steam may have overwritten the payload. Reinstall via Steam Tools.", "WARNING");
                } else {
                    emit log("DLL payload verified (" + QString::number(fi.size() / 1024) + " KB).", "SUCCESS");
                }
            }
        }

        // ── Step 3: Restart Steam with correct working directory ──
        emit log("Launching Steam...", "INFO");

        if (QFile::exists(steamExe)) {
            // Use the 3-argument overload that sets the working directory
            // This ensures xinput1_4.dll can resolve relative paths to config/stplug-in/*.lua
            qint64 pid = 0;
            bool launched = QProcess::startDetached(
                steamExe,
                QStringList() << "-silent",
                steamDir,        // Set CWD to Steam's root directory
                &pid
            );

            if (launched) {
                emit log(QString("Steam launched (PID: %1) with CWD: %2").arg(pid).arg(steamDir), "SUCCESS");
                emit finished("Steam restarted successfully!");
            } else {
                emit log("QProcess::startDetached failed.", "ERROR");
                emit error("Failed to launch Steam executable.");
            }
        } else {
            emit log("Steam executable not found at: " + steamExe, "ERROR");
            // Fallback: try steam:// protocol
            QProcess::startDetached("cmd", QStringList() << "/c" << "start" << "steam://open/main");
            emit log("Attempted fallback via steam:// protocol.", "WARNING");
            emit finished("Restart command sent (fallback).");
        }

    } catch (const std::exception& e) {
        emit error(QString::fromStdString(e.what()));
    } catch (...) {
        emit error("Unknown error occurred");
    }
}
