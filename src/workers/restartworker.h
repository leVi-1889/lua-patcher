#ifndef RESTARTWORKER_H
#define RESTARTWORKER_H

#include <QThread>
#include <QString>

class RestartWorker : public QThread {
    Q_OBJECT

public:
    explicit RestartWorker(QObject* parent = nullptr);

signals:
    void finished(QString message);
    void error(QString errorMessage);
    void log(QString message, QString level);

protected:
    void run() override;
};

#endif // RESTARTWORKER_H
