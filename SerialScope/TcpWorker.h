#pragma once

#include <QObject>
#include <QMetaType> 
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QList>

struct TcpConfig
{
    enum Role { Server, Client };
    Role role = Server;
    QString host = "0.0.0.0";   
    quint16 port = 12345;       
};
Q_DECLARE_METATYPE(TcpConfig)


// ===================== worker =====================
class TcpWorker : public QObject
{
    Q_OBJECT

public:
    TcpWorker(QObject* parent = nullptr);
    ~TcpWorker();

signals:
    void tcpStarted(const QString& info); // 通道可用：服务端=监听成功 / 客户端=真的连上了
    void tcpStopped(); // 通道已关闭
    void tcpStartFailed(const QString& error); // 通道建立失败
    
    void peerConnected(const QString& peerInfo); 
    void peerDisconnected(const QString& peerInfo);

    // ---- 数据 ----
    void tcpDataReceived(const QByteArray& data);
    void tcpSendFailed(const QString& error);
    
public slots:
    void startTcp(const TcpConfig& config); // 开：服务端=listen / 客户端=connectToHost
    void stopTcp(); // 关
    void sendTcpData(const QByteArray& data); // 发

private:
    TcpConfig::Role    m_role = TcpConfig::Server; // 角色

    QTcpServer* m_server = nullptr; // 服务端用
    QList<QTcpSocket*> m_peers;     // 服务端用

    QTcpSocket* m_socket = nullptr; // 客户端用
    bool m_clientConnected = false; // 客户端：连上过没有

    void onNewConnection(); // 服务端：有人连进来
    void bindPeer(QTcpSocket* peer); // 服务端：给某个对端挂信号
    void onConnected(); // 客户端：连上了
    void onDisconnected(); // 客户端：连接结束
    void onSocketError(QAbstractSocket::SocketError e); // 客户端：出错了
};