#include "TcpWorker.h"
#include <QDebug>
#include <QThread>

TcpWorker::TcpWorker(QObject *parent)
	: QObject(parent)
{}

TcpWorker::~TcpWorker()
{}

void TcpWorker::startTcp(const TcpConfig& config) { // 开：服务端=listen / 客户端=connectToHost
	const bool serverRunning = (m_server && m_server->isListening());
	const bool clientRunning = (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState);

	m_role = config.role;

	if (config.role == TcpConfig::Server) {
		if (serverRunning) {
			emit tcpStartFailed("服务端已在监听中");
			return;
		}

		if (!m_server) {
			m_server = new QTcpServer(this);

			connect(m_server, &QTcpServer::newConnection, this, &TcpWorker::onNewConnection);
			connect(m_server, &QTcpServer::acceptError, this, [this](QAbstractSocket::SocketError) {
				qWarning() << "接受连接失败：" << m_server->errorString() << "（监听仍在继续）";
				});
		}

		if (!m_server->listen(QHostAddress::Any, config.port))
		{
			emit tcpStartFailed("监听失败：" + m_server->errorString());
			return;
		}

		emit tcpStarted(QString("端口%1监听成功").arg(m_server->serverPort()));
	}
	else {
		if (clientRunning) {
			emit tcpStartFailed("客户端已连接");
			return;
		}

		if (!m_socket) {
			m_socket = new QTcpSocket(this);

			connect(m_socket, &QTcpSocket::connected, this, &TcpWorker::onConnected);
			connect(m_socket, &QTcpSocket::disconnected, this, &TcpWorker::onDisconnected);
			connect(m_socket, &QTcpSocket::errorOccurred, this, &TcpWorker::onSocketError);

			connect(m_socket, &QTcpSocket::readyRead, this, [this](){
				const QByteArray data = m_socket->readAll();
				if (!data.isEmpty()) emit tcpDataReceived(data);
			});
		}

		m_socket->connectToHost(config.host, config.port);
	}
} 

void TcpWorker::stopTcp() // 关
{
	// 服务端
	if (m_server && m_server->isListening()) {
		const QList<QTcpSocket*> peers = m_peers;
		for (QTcpSocket* peer : peers) {
			peer->disconnectFromHost();
		}
		m_server->close();
		emit tcpStopped();
	}

	// 还在连：disconnectFromHost() 在这里是空操作（实测），必须 abort。
	// abort 不发 disconnected，所以自己补一次通知
	if (!m_socket) return;
	if (m_socket->state() == QAbstractSocket::ConnectingState || m_socket->state() == QAbstractSocket::HostLookupState) {
		m_socket->abort();
		m_clientConnected = false;
		emit tcpStopped();
		return;
	}

	// 客户端
	if (m_socket->state() == QAbstractSocket::ConnectedState) {
		m_socket->disconnectFromHost();
	}
}

void TcpWorker::sendTcpData(const QByteArray& data)  // 发
{
	// 服务端
	if (m_role == TcpConfig::Server)
	{
		if (m_peers.isEmpty())
		{
			emit tcpSendFailed("还没有客户端连接");
			return;
		}
		for (QTcpSocket* peer : m_peers)
		{
			// 别往已经废掉的连接里写
			if (peer->state() != QAbstractSocket::ConnectedState) continue;
			if (peer->write(data) == -1)
				emit tcpSendFailed(peer->errorString());
		}
	}
	else //客户端
	{
		if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
		{
			// ConnectingState 是常态！没连上就点发送，这里必须挡住
			emit tcpSendFailed("尚未连接");
			return;
		}
		if (m_socket->write(data) == -1)
			emit tcpSendFailed(m_socket->errorString());
	}
}

void TcpWorker::onNewConnection() // 服务端：有人连进来
{
	while (m_server->hasPendingConnections())
	{
		QTcpSocket* peer = m_server->nextPendingConnection();

		m_peers.append(peer);
		bindPeer(peer);

		emit peerConnected(QString("%1:%2")
			.arg(peer->peerAddress().toString())
			.arg(peer->peerPort()));
	}
}

void TcpWorker::bindPeer(QTcpSocket* peer)
{
	// 为什么要传 peer 进来、还要捕获它？
	// 因为服务端有 N 个对端，信号槽里必须知道"这条消息是谁发的"。
	// 如果像单客户端那样只捕获 this 去读成员 m_socket，在多客户端下你
	// 根本分不清是谁 —— 这正是你 10/11 在 TcpServer 练习里统一成
	// [this, client] 的原因，只是这里把它提成了独立函数。

	const QString info = QString("%1:%2")
		.arg(peer->peerAddress().toString())
		.arg(peer->peerPort());

	connect(peer, &QTcpSocket::readyRead, this, [this, peer]() {
		const QByteArray data = peer->readAll();
		if (!data.isEmpty()) emit tcpDataReceived(data);
	});

	// info 在连接建立时就取好，不要在断开后再取
	//（那时 socket 已关，peerAddress() 拿不到东西了）
	connect(peer, &QTcpSocket::disconnected, this, [this, peer, info]() {
		m_peers.removeAll(peer);
		peer->deleteLater(); // 不能 delete：此刻信号正在发射，销毁自己会崩
		emit peerDisconnected(info);
	});
}

void TcpWorker::onConnected() // 客户端：连上了
{
	m_clientConnected = true;
	emit tcpStarted("已连接");        // 到这里才敢说"开了"
}

void TcpWorker::onDisconnected() // 客户端：连接结束
{
	m_clientConnected = false;
	emit tcpStopped();        // 自己关的、对端断的，都从这条路出去，统一宣布一次
}

void TcpWorker::onSocketError(QAbstractSocket::SocketError e) // 客户端：出错了
{
	// ★ 同一个 errorOccurred，有两种完全不同的含义，靠 m_clientConnected 区分：
	//   · 还没连上就出错 → 「连接失败」（没人接 / 号码不对）
	//   · 连上之后才出错 → 「掉线」
	if (m_clientConnected) return;
	
	emit tcpStartFailed(m_socket->errorString());
}
