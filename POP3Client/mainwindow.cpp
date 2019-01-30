#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>

#include <QFile>
#include <QTextStream>

//haslo, nazwa, host itd w App.config

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(&_socket, QTcpSocket::connected, this, MainWindow::authenticate);

    connect(ui->pb_start_end, QPushButton::clicked, this, MainWindow::startStopSession);

    connect(this, MainWindow::authenticationOK, this, MainWindow::onAuthenticationOK);
    connect(this, MainWindow::authenticationFailed, this, MainWindow::onAuthenticationFailed);

    connect(&_timer, QTimer::timeout, this, MainWindow::refreshMailList);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::authenticate()
{
    qDebug() << "Connected!";

    if (getServerResponse() != ResponseType::Ok)
    {
        writeServerCommand("QUIT");
        return;
    }

    writeServerCommand("USER " + _config.username);

    if (getServerResponse() != ResponseType::Ok)
    {
        writeServerCommand("QUIT");
        return;
    }

    writeServerCommand("PASS " + _config.password);

    ResponseType response = getServerResponse();
    if (response == ResponseType::Ok)
    {
        emit authenticationOK();
        return;
    }
    else if (response == ResponseType::Error)
    {
        ui->lbl_info->setText("Invalid login or password.");
    }

    writeServerCommand("QUIT");
}

void MainWindow::startStopSession()
{
    if (!loadConfigFromFile())
        return;

    if (ui->pb_start_end->text() == "Start")
    {
        ui->listWidget->clear();
        refreshMailList();
        _timer.start(_config.refreshRateInSec * 1000); // sec to msec
    }
    else
    {
        _firstTime = true;
        ui->lbl_info->setText("Received messages during this session: " + QString::number(_receivedMails));
        _receivedMails = 0;
        ui->pb_start_end->setText("Start");
        _timer.stop();
    }
}

void MainWindow::onAuthenticationOK()
{
    qDebug() << "Authentication OK";
    ui->lbl_info->setText("Logged as " + _config.hostname);
    ui->pb_start_end->setText("End");

    if (_firstTime)
    {
        _currentLastMailID = getLastMailIdFromServer();
        _firstTime = false;
    }
    // numer wiadomosci liczony od 1, wiec ostatnia wiadomosc ma numer równy liczbie wiadomości
    int lastMessageNumber = getServerMessagesCount();
    if (lastMessageNumber <= 0)
    {
        writeServerCommand("QUIT");
        return;
    }

    QString lastMailID = getLastMailIdFromServer();

    if (_currentLastMailID == lastMailID || lastMailID.isEmpty())
    {
        writeServerCommand("QUIT");
        return;
    }

    _currentLastMailID = lastMailID;
    writeServerCommand("RETR " + QString::number(lastMessageNumber));
    getServerResponse();
    QString message = getMailServerMessage();
    qDebug() << "RETR RESPONSE:" << message;

    int titleStartPos = message.indexOf("Subject:") + 8;
    int titleEndPos = message.indexOf("\r\n", titleStartPos);

    QString title = message.mid(titleStartPos, titleEndPos - titleStartPos);

    qDebug() << "TITLE:" << title;

    ui->listWidget->addItem(title);
    _receivedMails++;
    writeServerCommand("QUIT");

}

void MainWindow::onAuthenticationFailed()
{
    ui->lbl_info->setText("Invalid login or password.");

    writeServerCommand("QUIT");
}

void MainWindow::refreshMailList()
{
    qDebug() << "refreshMailList()";

    _socket.connectToHost(_config.hostname, _config.port);
}

MainWindow::ResponseType MainWindow::getServerResponse()
{
    qDebug() << "getServerResponse()";

    while (_socket.canReadLine())
    {
        char responseChar = _socket.readLine().front();
        if (responseChar == '+')
        {
            return ResponseType::Ok;
        }
        else if (responseChar == '-')
        {
            return ResponseType::Error;
        }
    }

    if (!_socket.waitForReadyRead(700))
    {
        qDebug() << "waitForReadyRead returned false";
        return ResponseType::Timeout;
    }

    QString message = _socket.readLine();
    qDebug() << message;

    if (!message.isEmpty())
    {
        if (message[0] == '+')
        {
            return ResponseType::Ok;
        }
    }

    // clear buffer
    while (_socket.canReadLine())
    {
        _socket.readLine();
    }

    return ResponseType::Timeout;
}

QString MainWindow::getNormalServerMessage()
{
    qDebug() << "getNormalServerMessage()";
    if (!_socket.waitForReadyRead(700))
    {
        qDebug() << "waitForReadyRead returned false";
        return {};
    }

    QString message;
    if (_socket.canReadLine())
        message = _socket.readAll();

//    message += getNormalServerMessage();

//    if (!message.endsWith("\r\n"))
//    {
//        message += getServerMessage();
//    }
    qDebug() << message << "\n\n";

    return message;
}

QString MainWindow::getMailServerMessage()
{
    qDebug() << "getMailServerMessage()";
    if (!_socket.waitForReadyRead(700))
    {
        qDebug() << "waitForReadyRead returned false";
        return {};
    }

    QString message = _socket.readAll();
    message += getMailServerMessage();

//    if (!message.endsWith("+OK\r\n"))
//    {
//        message += getMailServerMessage();
//    }

//    qDebug() << message << "\n\n";

    return message;
}

void MainWindow::writeServerCommand(QString command)
{
    qDebug() << "writeServerCommand()";
    // serwer oczekuje takiej końcówki wiadomości, więc trzeba to dokleić
    command += "\r\n";
    qDebug() << command;
    _socket.write(command.toStdString().c_str());
}

QString MainWindow::getLastMailIdFromServer()
{
    qDebug() << "getLastMailIdFromServer()";
    int messagesCount = getServerMessagesCount();

    if (messagesCount <= 0)
        return {};

    writeServerCommand("UIDL " + QString::number(messagesCount));

    // przykładowy message "+OK 2 4265154341.1737783590.3465945125\r\n"
    // wyciagam tylko numer + \r\n, ale on jest niewazny to go zostawiam"
    QStringList responseList = getNormalServerMessage().split(' ');

    QString result;
    if (responseList.size() >= 3)
    {
        result = responseList[2];
    }

    return result;
}

int MainWindow::getServerMessagesCount()
{
    qDebug() << "getServerMessagesCount()";
    writeServerCommand("STAT");

    QStringList msgList = getNormalServerMessage().split(' ');
    int messagesCount = -1;

    qDebug() << msgList;

    if (msgList.size() >= 2)
    {
        messagesCount = msgList.at(1).toInt();
    }

    qDebug() << "Messages count:" << messagesCount;

    return messagesCount;
}

bool MainWindow::loadConfigFromFile()
{
    QFile file("App.config");
    if (file.open(QFile::ReadOnly))
    {
        QTextStream stream(&file);

        stream >> _config.hostname >> _config.username >> _config.password >> _config.port >> _config.refreshRateInSec;
        qDebug() << _config.hostname << _config.username << _config.password << _config.port << _config.refreshRateInSec;
        return true;
    }
    ui->lbl_info->setText("Error during parsing configration file.");
    return false;
}
