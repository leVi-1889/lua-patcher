#ifndef ONBOARDINGDIALOG_H
#define ONBOARDINGDIALOG_H

#include <QDialog>
#include <QEvent>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QElapsedTimer>
#include <QStackedWidget>
#include "workers/steampatchworker.h"

// ============================================================================
// SLEEK MODULAR LOADING WIDGET
// ============================================================================
class LoadingWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoadingWidget(QWidget* parent = nullptr);
    ~LoadingWidget();

    void start(const QString& username, const QString& password, bool isRegister, bool isGuest);
    void stop();

    QJsonObject userData() const { return m_userData; }
    QString username() const { return m_verifiedUsername; }
    bool isGuest() const { return m_isGuest; }

signals:
    void success();
    void failed(const QString& errMsg);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onUpdate();
    void onAuthFinished();
    void onPatchLog(const QString& msg, const QString& level);
    void onPatchFinished(const QString& msg);
    void onPatchError(const QString& err);

private:
    void startAuth();
    void startPatching();

    // Inputs
    QString m_inputUsername;
    QString m_inputPassword;
    bool m_isRegister;
    bool m_isGuest;

    // Results
    QJsonObject m_userData;
    QString m_verifiedUsername;

    // Animation & State
    float m_progress;
    float m_time;
    int m_stage; // 0: verifying, 1: patching
    float m_textOpacity;
    QString m_statusText;

    QTimer* m_animationTimer;
    QElapsedTimer m_elapsedTimer;
    QPixmap m_logoPixmap;

    // Async Operations
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_authReply;
    bool m_authCompleted;
    bool m_authSuccess;

    SteamPatchWorker* m_patchWorker;
    bool m_patchCompleted;
    bool m_patchSuccess;
    QString m_errorMsg;
};

// ============================================================================
// MAIN ONBOARDING DIALOG WITH STACKED PAGES
// ============================================================================
class OnboardingDialog : public QDialog {
    Q_OBJECT
public:
    explicit OnboardingDialog(QWidget *parent = nullptr);
    QString username() const;
    QJsonObject userData() const;
    bool isGuest() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onUsernameChanged(const QString &text);
    void checkAvailability();
    void onCheckFinished(QNetworkReply *reply);
    void onPrimaryClicked();
    void switchToLogin();
    void switchToRegister();
    void onGuestClicked();

private:
    enum Mode { LOGIN, REGISTER };
    int m_currentMode;

    // Stacked container to switch views robustly
    QStackedWidget* m_stackedWidget;
    QWidget* m_loginPageWidget;
    LoadingWidget* m_loadingWidget;

    // UI elements inside m_loginPageWidget
    QWidget *m_leftPanel;
    QWidget *m_rightPanel;
    QLabel *m_tabSignIn;
    QLabel *m_tabSignUp;

    QLineEdit *m_usernameInput;
    QLineEdit *m_passwordInput;
    QLabel *m_statusLabel;
    QPushButton *m_continueBtn;
    QPushButton *m_guestBtn;

    // Image
    QPixmap m_bgImage;

    // Network
    QTimer *m_debounceTimer;
    QNetworkAccessManager *m_networkManager;
    QString m_username;
    QJsonObject m_userData;
    bool m_isAvailable;
    bool m_isChecking;
    bool m_isGuest;
};

#endif // ONBOARDINGDIALOG_H
