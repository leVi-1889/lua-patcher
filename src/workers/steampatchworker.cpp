#include "steampatchworker.h"
#include "../config.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>

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

        // 3. Locate the bundled xinput1_4.dll
        //    First check Qt resource (embedded), then check next to exe
        QString bundledDll;
        if (QFile::exists(":/payload/xinput1_4.dll")) {
            bundledDll = ":/payload/xinput1_4.dll";
            emit log("Using embedded payload from Qt resources", "INFO");
        } else {
            // Fallback: look next to the application executable
            QString appDir = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
            QString localDll = QDir(appDir).filePath("xinput1_4.dll");
            if (QFile::exists(localDll)) {
                bundledDll = localDll;
                emit log(QString("Using local payload: %1").arg(localDll), "INFO");
            } else {
                emit log("xinput1_4.dll payload not found (neither embedded nor next to exe)", "ERROR");
                throw std::runtime_error("Patch payload (xinput1_4.dll) not found. Please reinstall the application.");
            }
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
        if (!QFile::copy(bundledDll, destPath)) {
            emit log("Failed to copy xinput1_4.dll to Steam directory", "ERROR");
            throw std::runtime_error("Failed to copy patch file. Make sure Steam is closed and you have write permissions.");
        }

        // Make the destination writable (QFile::copy from resources makes it read-only)
        QFile::setPermissions(destPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);

        emit log("xinput1_4.dll successfully installed to Steam directory!", "SUCCESS");
        emit log("Please restart Steam for the patch to take effect.", "INFO");
        emit finished("Steam patched successfully! Restart Steam to apply.");

    } catch (const std::exception& e) {
        emit log(QString("Steam patch failed: %1").arg(e.what()), "ERROR");
        emit error(QString::fromStdString(e.what()));
    }
}
