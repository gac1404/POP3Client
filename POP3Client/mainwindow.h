#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QTcpSocket>
#include <QTimer>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void authenticationOK();
    void authenticationFailed();

private slots:
    void authenticate();
    void startStopSession();

    void onAuthenticationOK();
    void onAuthenticationFailed();

    void refreshMailList();

private:
    struct Config
    {
        QString hostname;
        QString username;
        QString password;
        int port;
        int refreshRateInSec;
    };
    enum class ResponseType
    {
        Ok,
        Error,
        Timeout
    };

    ResponseType getServerResponse();
    QString getNormalServerMessage();
    QString getMailServerMessage();
    void writeServerCommand(QString command);
    QString getLastMailIdFromServer();
    int getServerMessagesCount();
    bool loadConfigFromFile();

    Config _config;

    Ui::MainWindow *ui;
    QTcpSocket _socket;
    QTimer _timer;

    bool _firstTime = true;
    int _receivedMails = 0;

    QString _currentLastMailID;
};

#endif // MAINWINDOW_H
