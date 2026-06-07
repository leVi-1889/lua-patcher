#include <QApplication>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include "overlaywindow.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Static plugin imports for static Qt builds
#ifdef QT_STATIC_BUILD
#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
#endif

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("leVi Studios");
    app.setApplicationName("PulseUI");

    // Read the greeting message and recently added games from QSettings (Registry)
    QSettings settings("leVi Studios", "LuaPatcher");
    QString lastAdded = settings.value("PulseFramework/LastAdded", "").toString();

    // Build the greeting body
    QString greeting = "Steam Library Manager is ready to help you organize your "
                       "collection, discover your installed games, and keep everything "
                       "exactly where you want it.\n\nHappy gaming!";

    // Parse recently added games
    QStringList games;
    if (!lastAdded.isEmpty()) {
        games = lastAdded.split(", ", Qt::SkipEmptyParts);
        // Clear the registry entry so we only show it once
        settings.remove("PulseFramework/LastAdded");
    }

    // Wait a moment for Steam's window to fully initialize
    QTimer::singleShot(3000, [&]() {
        auto* overlay = new OverlayWindow(greeting, games);
        overlay->show();
        overlay->activateWindow();
        overlay->raise();
    });

    return app.exec();
}
