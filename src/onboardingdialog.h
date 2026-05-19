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
#include "workers/steampatchworker.h"

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

  // Loading slots
  void onLoadingUpdate();
  void onAuthFinished(QNetworkReply* reply);
  void onPatchLog(const QString& msg, const QString& level);
  void onPatchFinished(const QString& msg);
  void onPatchError(const QString& err);

private:
  enum Mode { LOGIN, REGISTER };
  int m_currentMode;

  // UI
  QWidget *m_leftPanel;
  QWidget *m_rightPanel;
  QLabel *m_tabSignIn;
  QLabel *m_tabSignUp;

  QLineEdit *m_usernameInput;
  QLineEdit *m_passwordInput;
  QLabel *m_statusLabel;
  QPushButton *m_continueBtn;
  QPushButton *m_guestBtn;

  // Image & Logo
  QPixmap m_bgImage;
  QPixmap m_logoPixmap;

  // Network
  QTimer *m_debounceTimer;
  QNetworkAccessManager *m_networkManager;
  QString m_username;
  QJsonObject m_userData;
  bool m_isAvailable;
  bool m_isChecking;
  bool m_isGuest;

  // Integrated Loading State
  bool m_isLoading;
  float m_loadingProgress;
  float m_loadingTime;
  int m_loadingStage; // 0: verifying, 1: patching
  float m_textOpacity;
  QString m_statusText;

  QTimer* m_loadingTimer;
  QElapsedTimer m_loadingElapsed;

  // Async Tasks
  QNetworkReply* m_authReply;
  bool m_authCompleted;
  bool m_authSuccess;

  SteamPatchWorker* m_patchWorker;
  bool m_patchCompleted;
  bool m_patchSuccess;
  QString m_loadingError;

  void startLoadingFlow(const QString& user, const QString& pass);
  void showLoadingView();
  void hideLoadingView();
  void startPatchingProcess();
};

#endif // ONBOARDINGDIALOG_H
