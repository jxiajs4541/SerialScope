#pragma once

#include <QObject>
#include <QSerialPort>
#include <QSqlDatabase>

struct SerialConfig {
	QString portName;
	int baud = 9600;
	QSerialPort::DataBits dataBits = QSerialPort::Data8;
	QSerialPort::Parity   parity = QSerialPort::NoParity;
	QSerialPort::StopBits stopBits = QSerialPort::OneStop;
}; Q_DECLARE_METATYPE(SerialConfig)

class SerialWorker : public QObject
{
	Q_OBJECT

public:
	SerialWorker(QObject *parent=nullptr);
	~SerialWorker();

signals:
	void serialOpened();
	void serialClosed();
	void serialOpenFailed(const QString& error);
	void dataSendFailed(const QString &error);
	void dataReceived(const QByteArray &data);
	void serialLost(const QString& error);
	void logWriteFailed(const QString &error);
	void countsChanged(qint64 rxCount, qint64 txCount);
	void dbError(const QString& msg);

public slots:
	//打开串口
	void openSerialPort(const SerialConfig& config);
	//关闭串口
	void closeSerialPort();
	//发送数据
	void sendData(const QByteArray&data);
	// 接路径，空=停
	void setLogFile(const QString& path);

private:
	QSerialPort* m_serialPort = nullptr;
	QString m_logPath;	//日志保存地址
	qint64 m_rxCount = 0;	// 已收字节                   
	qint64 m_txCount = 0;	// 已发字节
	QSqlDatabase m_db;
	bool m_dbOk = false;
	bool openDatabase();
	// 写日志
	void appendLog(const QString& direction, const QString& text);
	// 入库
	void insertRecord(const QString& direction, const QString& text, int byteCount);
	void logEvent(const QString& direction, const QString& text);
	void logData(const QString& direction, const QString& text, int size);
};

