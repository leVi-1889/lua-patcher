#include "mainwindow.h"
#include "utils/colors.h"
#include "onboardingdialog.h"
#include "workers/restartworker.h"
#include <QApplication>
#include <QThread>
#include <QEventLoop>
#include <QFont>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

QString getStyleSheet() {
    return QString(R"(
QMainWindow {
    background: transparent;
}

QWidget {
    font-family: 'Oswald', sans-serif;
    color: %2;
}

/* Scrollbar */
QScrollBar:vertical {
    background-color: transparent;
    width: 6px;
    margin: 0px;
}
QScrollBar::handle:vertical {
    background-color: %3;
    border-radius: 3px;
    min-height: 20px;
}
QScrollBar::handle:vertical:hover {
    background-color: %4;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: none;
}

/* List Widget */
QListWidget {
    background-color: transparent;
    border: none;
    outline: none;
}
QListWidget::item {
    background-color: %5;
    border: 1px solid %3;
    border-radius: 16px;
    padding: 12px;
    margin-bottom: 8px;
    color: %2;
    word-wrap: break-word;
}
QListWidget::item:hover {
    background-color: %6;
    border: 1px solid %4;
}
QListWidget::item:selected {
    background-color: %7;
    border: 1px solid %8;
}

/* Inputs - Material Outlined Text Field */
QLineEdit {
    background-color: %5;
    border: 1px solid %3;
    border-radius: 28px;
    padding: 12px 20px;
    font-size: 14px;
    selection-background-color: %8;
    color: %2;
}
QLineEdit:focus {
    border: 2px solid %8;
    background-color: %6;
}
QLineEdit::placeholder {
    color: %4;
}

/* MessageBox - Material Dialog */
QMessageBox {
    background-color: %9;
    border-radius: 28px;
}
QMessageBox QLabel {
    color: %2;
    font-family: 'Oswald', sans-serif;
}
QMessageBox QPushButton {
    background-color: %8;
    color: %10;
    border-radius: 20px;
    padding: 8px 24px;
    border: none;
    font-weight: bold;
    font-family: 'Oswald', sans-serif;
}
QMessageBox QPushButton:hover {
    background-color: %11;
}

/* ToolTip */
QToolTip {
    background-color: %9;
    color: %2;
    border: 1px solid %3;
    border-radius: 8px;
    padding: 6px 12px;
    font-family: 'Oswald', sans-serif;
}
)")
    .arg(Colors::SURFACE)                // %1  background
    .arg(Colors::ON_SURFACE)             // %2  text
    .arg(Colors::OUTLINE_VARIANT)        // %3  border/scrollbar
    .arg(Colors::OUTLINE)                // %4  hover border/scrollbar
    .arg(Colors::SURFACE_CONTAINER)      // %5  input/list bg
    .arg(Colors::SURFACE_CONTAINER_HIGH) // %6  hover bg
    .arg(Colors::SURFACE_CONTAINER_HIGHEST) // %7  selected bg
    .arg(Colors::PRIMARY)                // %8  primary accent
    .arg(Colors::SURFACE_CONTAINER_HIGH) // %9  dialog bg
    .arg(Colors::ON_PRIMARY)             // %10 on-primary text
    .arg(Colors::PRIMARY_CONTAINER);     // %11 primary hover
}

int main(int argc, char *argv[]) {
    qDebug() << "Step 1: Starting main";
    QCoreApplication::setOrganizationName("leVi Studios");
    QCoreApplication::setApplicationName("LuaPatcher");

    qDebug() << "Step 2: Creating QApplication";
    QApplication app(argc, argv);
    qDebug() << "Step 3: Setting icon and style";
    app.setWindowIcon(QIcon("icon.png"));
    app.setStyle("Fusion");
    app.setStyleSheet(getStyleSheet());
    
    // Auto-fix mode for Windows Startup
    if (argc > 1 && QString(argv[1]) == "--auto-fix") {
        qDebug() << "Auto-fix mode: Waiting for Steam to start...";
        
        // Step 1: Remove Steam's own broken registry Run key (it keeps re-adding it)
        QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        reg.remove("Steam");
        
        // Step 2: Wait for Steam to actually be running (up to 90 seconds)
        bool steamFound = false;
        for (int i = 0; i < 90; ++i) {
            QProcess checkProc;
            checkProc.start("tasklist", QStringList() << "/FI" << "IMAGENAME eq steam.exe" << "/NH");
            checkProc.waitForFinished(3000);
            QString output = QString::fromLocal8Bit(checkProc.readAllStandardOutput());
            if (output.contains("steam.exe", Qt::CaseInsensitive)) {
                steamFound = true;
                break;
            }
            QThread::sleep(1);
        }
        
        if (!steamFound) {
            qDebug() << "Auto-fix: Steam never started. Exiting.";
            return 0;
        }
        
        // Step 3: Give Steam time to fully initialize (load UI, sync library)
        qDebug() << "Auto-fix: Steam detected. Waiting for initialization...";
        QThread::sleep(15);
        
        // Step 4: Restart Steam with correct working directory
        qDebug() << "Auto-fix: Restarting Steam with correct environment...";
        RestartWorker* worker = new RestartWorker();
        QEventLoop loop;
        QObject::connect(worker, &RestartWorker::finished, &loop, &QEventLoop::quit);
        QObject::connect(worker, &RestartWorker::error, &loop, &QEventLoop::quit);
        worker->start();
        loop.exec();
        
        qDebug() << "Auto-fix: Done. Exiting.";
        return 0;
    }

    qDebug() << "Step 4: Setting up fonts";
    QFont font("Oswald");
    if (!QFontInfo(font).exactMatch()) {
#ifdef Q_OS_WIN
        font.setFamily("Segoe UI");
#else
        font.setFamily("sans-serif");
#endif
    }
    font.setPointSize(10);
    font.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(font);
    
    qDebug() << "Step 5: Setting up settings";
    // Use AppData folder for settings (Fixes permission issues in Program Files)
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(appDataPath);
    QString settingsPath = appDataPath + "/settings.ini";
    QSettings settings(settingsPath, QSettings::IniFormat);
    QString username = settings.value("username", "").toString();
    
    QJsonObject initialUserData;
    bool isGuest = false;

    if (username.isEmpty()) {
        qDebug() << "Step 6: Showing onboarding dialog";
        OnboardingDialog dialog;
        if (dialog.exec() == QDialog::Accepted) {
            isGuest = dialog.isGuest();
            if (isGuest) {
                username = "Guest";
            } else {
                username = dialog.username();
                initialUserData = dialog.userData();
                settings.setValue("userData", QJsonDocument(initialUserData).toJson());
            }
            settings.setValue("username", username);
            settings.setValue("isGuest", isGuest);
        } else {
            qDebug() << "Step 6.1: Onboarding cancelled";
            return 0; // User closed onboarding, exit app
        }
    } else {
        qDebug() << "Step 6.2: Loading existing user data";
        isGuest = settings.value("isGuest", false).toBool();
        QString dataStr = settings.value("userData", "").toString();
        if (!dataStr.isEmpty()) {
            initialUserData = QJsonDocument::fromJson(dataStr.toUtf8()).object();
        }
    }
    
    qDebug() << "Step 7: Creating MainWindow";
    MainWindow window;
    qDebug() << "Step 8: Setting initial user";
    window.setInitialUser(username, initialUserData, isGuest);
    qDebug() << "Step 9: Showing window maximized";
    window.showMaximized();
    qDebug() << "Step 10: Starting event loop";

    return app.exec();
}
