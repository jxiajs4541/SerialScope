#include "SerialWorker.h"
#include <DbConfig.h>
#include <QSerialPort>
#include <QIODevice>
#include <QString>
#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>

SerialWorker::SerialWorker(QObject *parent)
	: QObject(parent)
{
}

SerialWorker::~SerialWorker()
{
}

//打开串口
void SerialWorker::openSerialPort(const SerialConfig& config) {
	if (m_serialPort == nullptr) {
		m_serialPort = new QSerialPort(this);  
		// 接收消息
		connect(m_serialPort, &QSerialPort::readyRead, this, [this]() {
			QByteArray data = m_serialPort->readAll();
			m_rxCount += data.size(); //累加
			logData("RX", QString::fromLatin1(data.toHex(' ').toUpper()),data.size());
			emit countsChanged(m_rxCount, m_txCount);
			emit dataReceived(data);
			});
		// 设备被移除
		connect(m_serialPort, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
			if (error == QSerialPort::ResourceError) {
				logEvent("ERR", "设备连接已断开：" + m_serialPort->errorString());
				emit serialLost(m_serialPort->errorString());
				m_serialPort->close();
			}
		});
	}

	m_serialPort->setPortName(config.portName);
	m_serialPort->setBaudRate(config.baud);
	m_serialPort->setDataBits(config.dataBits);
	m_serialPort->setParity(config.parity);
	m_serialPort->setStopBits(config.stopBits);

	QChar parityChar = (config.parity == QSerialPort::NoParity) ? 'N'
		: (config.parity == QSerialPort::EvenParity) ? 'E' : 'O';

	if (m_serialPort->open(QIODevice::ReadWrite)) {
		QString msg = QString("打开串口：%1 %2 %3%4%5")
			.arg(config.portName)
			.arg(config.baud)
			.arg(static_cast<int>(config.dataBits))
			.arg(parityChar)
			.arg(config.stopBits == QSerialPort::OneStop ? 1 : 2);
		logEvent("SYS", msg);
		emit serialOpened();
	}
	else {
		logEvent("ERR", "打开串口失败：" + m_serialPort->errorString());
		emit serialOpenFailed(m_serialPort->errorString());
	}
}

//关闭串口
void SerialWorker::closeSerialPort() {
	if (!m_serialPort) return;
	m_serialPort->close();
	QString msg = "关闭串口";
	logEvent("SYS", msg);
	emit serialClosed();
}

// 发送数据
void SerialWorker::sendData(const QByteArray&data){
	if (!m_serialPort) return;
	qint64 written = m_serialPort->write(data);
	if (written == -1) {
		logEvent("ERR", "发送失败：" + m_serialPort->errorString());
		emit dataSendFailed(m_serialPort->errorString());
		return;
	}
	m_txCount += written;
	logData("TX", QString::fromLatin1(data.toHex(' ').toUpper()), written);
	emit countsChanged(m_rxCount, m_txCount);
}

// 接路径，空=停
void SerialWorker::setLogFile(const QString& path)
{
	m_logPath = path;
}

// 打开数据库
bool SerialWorker::openDatabase()
{
	if (m_dbOk) return true;
	m_db = QSqlDatabase::addDatabase("QODBC", "serial_conn");
	m_db.setDatabaseName(dbDatabaseName());
		
	if (!m_db.open()) {
		emit dbError("数据库打开失败："+ m_db.lastError().text());
		return false;
	}
	m_dbOk = true;
	return true;
}

// 数据入库
void SerialWorker::insertRecord(const QString& direction, const QString& text, int byteCount)
{
	if (!openDatabase()) return;
	QSqlQuery query(m_db);
	query.prepare("INSERT INTO serial_log (time, direction, hex_data, byte_count) VALUES (?, ?, ?, ?)");
	query.addBindValue(QDateTime::currentDateTime());
	query.addBindValue(direction);
	query.addBindValue(text);
	query.addBindValue(byteCount);
	if (!query.exec()) {
		emit dbError("入库失败：" + query.lastError().text());
	}
}

// 写日志
void SerialWorker::appendLog(const QString& direction, const QString& text)
{
	if (m_logPath.isEmpty()) return;

	QFile file(m_logPath);

	if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
		emit logWriteFailed(file.errorString());
		return;
	}

	QTextStream out(&file);
	out << '[' << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz") << "] "
		<< direction<<' '
		<< text << "\n";
	file.close();
}

// 事件行
void SerialWorker::logEvent(const QString& direction, const QString& text)
{
	appendLog(direction, text);
	insertRecord(direction, text, 0);
}

// 数据行
void SerialWorker::logData(const QString& direction, const QString& text, int size)
{
	appendLog(direction, text);
	insertRecord(direction, text, size);
}

