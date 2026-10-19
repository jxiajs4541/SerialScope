#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_SerialScope.h"
#include "SerialWorker.h"  
#include "TcpWorker.h"

class QThread;
class QLabel;

class SerialScope : public QMainWindow
{
    Q_OBJECT

public:
    SerialScope(QWidget* parent = nullptr);
    ~SerialScope();

signals:
    // Serial
    void openSerialRequested(const SerialConfig& config);
    void closeSerialRequested();
    void sendSerialDataRequested(const QByteArray& data);
    void setLogFileRequested(const QString &path);

    // Tcp
    void startTcpRequested(const TcpConfig& config);
    void stopTcpRequested();
    void sendTcpDataRequested(const QByteArray& data);
private slots:
    // Serial
    void on_btnSerialRefresh_clicked();
    void on_btnSerialOpenClose_clicked();
    void on_btnSerialClearReceive_clicked();
    void on_btnSerialSend_clicked();
    void on_chkLogEnable_toggled(bool checked);
    void on_chkAutoWrap_toggled(bool checked);
    void on_btnSerialQueryDb_clicked();
    void onSerialOpened();
    void onSerialClosed();
    void onSerialDataReceived(const QByteArray& data);
    void onCountsChanged(qint64 rxCount, qint64 txCount);

    // Tcp
    void on_btnTcpStart_clicked();
    void on_btnTcpStop_clicked();
    void on_btnTcpSend_clicked();
    void on_btnTcpClear_clicked();

    void onTcpStarted(const QString& info);
    void onTcpStopped();
    void onTcpStartFailed(const QString& error);
    void onTcpDataReceived(const QByteArray& data);
    void onTcpDataSendFailed(const QString& error);
    void onPeerConnected(const QString& peerInfo);
    void onPeerDisconnected(const QString& peerInfo);

protected:
    void closeEvent(QCloseEvent* event) override;

private:

    Ui::SerialScopeClass ui;
    // Serial
    SerialWorker* m_serialWorker = nullptr;
    QThread* m_serialThread = nullptr;
    QLabel* m_lblCounts = nullptr;
    bool m_isOpen = false;
    void initSerial();
    void setupSerialThread();   
    void loadSerialConfig();
    void saveSerialConfig();

    // Tcp
    TcpWorker* m_tcpWorker = nullptr;
    QThread* m_tcpThread = nullptr;
    void initTcp();
    void updateTcpRoleUi();
    void setupTcpThread();
    

};
