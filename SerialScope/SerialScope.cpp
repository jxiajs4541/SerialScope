#include "SerialScope.h"
#include "SerialWorker.h"
#include "QueryDialog.h"
#include <QThread>
#include <QSerialPortInfo>
#include <QSerialPort>
#include <QByteArray>
#include <QString>
#include <QFile>
#include <QDebug> 
#include <QTextStream>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QFileDialog>
#include <QDateTime>
#include <QLabel>

static int findPeerRow(QListWidget* list, const QString& peerInfo)
{
    // 按 UserRole 找，不按显示文本找
    for (int row = 0; row < list->count(); ++row) {
        if (list->item(row)->data(Qt::UserRole).toString() == peerInfo)
            return row;
    }
    return -1;
}

SerialScope::SerialScope(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    
    initSerial();

    initTcp();
    
}

SerialScope::~SerialScope()
{
   
    m_serialThread->quit();
    m_serialThread->wait();
    m_tcpThread->quit();
    m_tcpThread->wait();
}


// ===== private slots =====
// 按钮：刷新串口
void SerialScope::on_btnSerialRefresh_clicked() {
    ui.cmbPortName->clear();
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        ui.cmbPortName->addItem(info.portName() + " - " + info.description(), info.portName());
    }
}

//按钮：串口打开/关闭请求
void SerialScope::on_btnSerialOpenClose_clicked() {
    if (ui.cmbPortName->currentText().isEmpty()) {
        ui.statusBar->showMessage("请选择串口!", 5000);
        return;
    }
    ui.btnSerialOpenClose->setEnabled(false);
    if (m_isOpen) {
        emit closeSerialRequested();
    }
    else {
        SerialConfig cfg;
        cfg.portName = ui.cmbPortName->currentData().toString();
        cfg.baud = ui.cmbBaud->currentData().toInt();
        cfg.dataBits = ui.cmbDataBits->currentData().value<QSerialPort::DataBits>();
        cfg.parity = ui.cmbParity->currentData().value<QSerialPort::Parity>();
        cfg.stopBits = ui.cmbStopBits->currentData().value<QSerialPort::StopBits>();
        emit openSerialRequested(cfg);
    }
}

// 按钮：清除接收
void SerialScope::on_btnSerialClearReceive_clicked() {
    ui.pteReceive->clear();
}

// 按钮：点击发送
void SerialScope::on_btnSerialSend_clicked() {
    if (!m_isOpen) {
        ui.statusBar->showMessage("未打开串口!", 5000);
        return;
    }

    QByteArray data;

    if (ui.chkSendHex->isChecked()) {
        // ===== HEX模式 =====
        QString text = ui.pteSend->toPlainText().trimmed();     //去除首尾空白

        static const QRegularExpression hexPattern("^[0-9A-Fa-f\\s]*$");    //只允许 0-9 A-F a-f 和空白字符
        if (!hexPattern.match(text).hasMatch()) {
            ui.statusBar->showMessage("HEX 格式错误：只能包含 0-9 / A-F 和空格", 5000);
            return;                                          // 不发，直接退出
        }

        QString clean = text;
        clean.remove(QRegularExpression("\\s"));
        if (clean.isEmpty()) return;

        if (clean.size() % 2 != 0) {
            ui.statusBar->showMessage("HEX 长度必须为偶数（如 01 A2 FF）", 5000);
            return;
        }

        data = QByteArray::fromHex(clean.toLatin1());
    }
    else {
        // ===== 文本模式 =====
        QString text = ui.pteSend->toPlainText();
        if (text.isEmpty()) return;
        data = text.toLocal8Bit() + "\r\n";
    }

    emit sendSerialDataRequested(data);
}

// 复选框：日志记录
void SerialScope::on_chkLogEnable_toggled(bool checked)   
{
    if (checked) {
        QString defaultName = QCoreApplication::applicationDirPath() + "/log_"
            + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".txt";
        QString path = QFileDialog::getSaveFileName(this, "保存日志", defaultName, "文本文件 (*.txt)");
        if (path.isEmpty()) {     
            ui.chkLogEnable->setChecked(false);
            return;
        }
        emit setLogFileRequested(path);
        ui.statusBar->showMessage("日志已开始记录：" + path, 5000);
    }
    else {
        emit setLogFileRequested(QString());
    }
}

// 复选框：自动换行
void SerialScope::on_chkAutoWrap_toggled(bool checked)
{
    ui.pteReceive->setLineWrapMode(checked ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
}

// 按钮：数据查询
void SerialScope::on_btnSerialQueryDb_clicked()
{
    QueryDialog dlg(this);
    dlg.exec();
}

// 串口已打开
void SerialScope::onSerialOpened() {    
    ui.widget->setEnabled(false);
    ui.btnSerialOpenClose->setEnabled(true);
    ui.statusBar->showMessage("串口已打开", 5000);
    ui.btnSerialOpenClose->setText("关闭串口");
    m_isOpen = true;
}

// 串口已关闭
void SerialScope::onSerialClosed() {   
    ui.widget->setEnabled(true);
    ui.btnSerialOpenClose->setEnabled(true);
    ui.statusBar->showMessage("串口已关闭", 5000);
    ui.btnSerialOpenClose->setText("打开串口");
    m_isOpen = false;
    
}

//接收数据
void SerialScope::onSerialDataReceived(const QByteArray& data) {        
    if (ui.chkHexDisplay->isChecked()) {
        // HEX 显示：先转十六进制文字，每字节间加空格
        QString hex = QString::fromLatin1(data.toHex(' ').toUpper());
        ui.pteReceive->appendPlainText(hex);
    }
    else {
        // 文本显示：按 GBK 解码（原有逻辑）
        ui.pteReceive->appendPlainText(QString::fromLocal8Bit(data));
    }
    
    if (ui.chkScrollBottom->isChecked()) {
        ui.pteReceive->moveCursor(QTextCursor::End);  
    }
}


// 收发计数变化
void SerialScope::onCountsChanged(qint64 rxCount, qint64 txCount)
{
    m_lblCounts->setText(QString("收: %1  发: %2").arg(rxCount).arg(txCount));
}

// ===== protected ===== 
// 重写：关闭事件
void SerialScope::closeEvent(QCloseEvent* event) {        
    saveSerialConfig();
    QMainWindow::closeEvent(event);
}

void SerialScope::initSerial()
{
    m_lblCounts = new QLabel("收: 0  发: 0", this);
    ui.statusBar->addPermanentWidget(m_lblCounts);
    on_btnSerialRefresh_clicked();
    on_chkAutoWrap_toggled(ui.chkAutoWrap->isChecked());
    setupSerialThread();
    ui.pteReceive->setMaximumBlockCount(5000);
    
    for (int i : {9600, 19200, 38400, 57600, 115200}) {
        ui.cmbBaud->addItem(QString::number(i), i);
    }

    ui.cmbDataBits->addItem("8", QSerialPort::Data8);
    ui.cmbDataBits->addItem("7", QSerialPort::Data7);
    ui.cmbDataBits->addItem("6", QSerialPort::Data6);
    ui.cmbDataBits->addItem("5", QSerialPort::Data5);

    ui.cmbParity->addItem("None", QSerialPort::NoParity);
    ui.cmbParity->addItem("Even", QSerialPort::EvenParity);
    ui.cmbParity->addItem("Odd", QSerialPort::OddParity);

    ui.cmbStopBits->addItem("1", QSerialPort::OneStop);
    ui.cmbStopBits->addItem("2", QSerialPort::TwoStop);

    loadSerialConfig();
}

//将任务对象分配到子线程并connect
void SerialScope::setupSerialThread()
{
    m_serialWorker = new SerialWorker;
    m_serialThread = new QThread(this);
    m_serialWorker->moveToThread(m_serialThread);
    m_serialThread->start();
    connect(m_serialThread, &QThread::finished, m_serialWorker, &QObject::deleteLater);

    // UI -> m_serialWorker
    connect(this, &SerialScope::openSerialRequested, m_serialWorker, &SerialWorker::openSerialPort);
    connect(this, &SerialScope::closeSerialRequested, m_serialWorker, &SerialWorker::closeSerialPort);
    connect(this, &SerialScope::sendSerialDataRequested, m_serialWorker, &SerialWorker::sendData);
    connect(this, &SerialScope::setLogFileRequested, m_serialWorker, &SerialWorker::setLogFile);


    // m_serialWorker -> UI
    connect(m_serialWorker, &SerialWorker::serialOpened, this, &SerialScope::onSerialOpened);
    connect(m_serialWorker, &SerialWorker::serialOpenFailed, this, [this](const QString& error) {     //串口打开失败
        ui.btnSerialOpenClose->setEnabled(true);
        ui.statusBar->showMessage("串口打开失败："+error, 5000);
        });
    connect(m_serialWorker, &SerialWorker::serialClosed, this, &SerialScope::onSerialClosed);
    connect(m_serialWorker, &SerialWorker::dataSendFailed, this, [this](const QString&error) {     //数据发送失败
        ui.statusBar->showMessage("数据发送失败：" + error, 5000);
        });
    connect(m_serialWorker, &SerialWorker::serialLost, this, [this](const QString&error) {     // 串口断开
        onSerialClosed();
        ui.statusBar->showMessage("串口连接断开：" + error, 5000);
        });
    connect(m_serialWorker, &SerialWorker::logWriteFailed, this, [this](const QString& error) {     // 日志写入失败
        ui.chkLogEnable->setChecked(false);
        ui.statusBar->showMessage("日志写入失败：" + error, 5000);
        });
    connect(m_serialWorker, &SerialWorker::dbError, this, [this](const QString& error) {     // 日志写入失败
        ui.statusBar->showMessage(error, 5000);
        });
    connect(m_serialWorker, &SerialWorker::dataReceived, this, &SerialScope::onSerialDataReceived);
    connect(m_serialWorker, &SerialWorker::countsChanged, this, &SerialScope::onCountsChanged);
}

// ===== private =====
// 加载配置
void SerialScope::loadSerialConfig()
{
    QString path = QCoreApplication::applicationDirPath()+ "/config.ini";
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { // 首次运行文件不存在
        return;
    }

    QTextStream in(&file);
    QString portName = in.readLine();
    int baudRate = in.readLine().toInt();   
    int dataBits = in.readLine().toInt();
    int parity = in.readLine().toInt();
    int stopBits = in.readLine().toInt();
    file.close();

    // 恢复端口名
    int portNameIndex = ui.cmbPortName->findData(portName);
    if (portNameIndex >= 0) {
        ui.cmbPortName->setCurrentIndex(portNameIndex);
    }

    // 恢复波特率
    int portBaudIndex = ui.cmbBaud->findData(baudRate);
    if (portBaudIndex >= 0) {
        ui.cmbBaud->setCurrentIndex(portBaudIndex);
    }

    // 恢复数据位
    int portDataBitsIndex = ui.cmbDataBits->findData(dataBits);
    if (portDataBitsIndex >= 0) {
        ui.cmbDataBits->setCurrentIndex(portDataBitsIndex);
    }

    // 恢复校验位
    int portParityIndex= ui.cmbParity->findData(parity);
    if (portParityIndex >= 0) {
        ui.cmbParity->setCurrentIndex(portParityIndex);
    }

    // 恢复停止位
    int portStopBitsIndex = ui.cmbStopBits->findData(stopBits);
    if (portStopBitsIndex >= 0) {
        ui.cmbStopBits->setCurrentIndex(portStopBitsIndex);
    }
}

// 保存配置 
void SerialScope::saveSerialConfig()
{
    QString path = QCoreApplication::applicationDirPath() + "/config.ini";
    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }

    QTextStream out(&file);
   
    out << ui.cmbPortName->currentData().toString() << '\n';
    out << ui.cmbBaud->currentData().toInt() << '\n';
    out << ui.cmbDataBits->currentData().toInt() << '\n';
    out << ui.cmbParity->currentData().toInt() << '\n';
    out << ui.cmbStopBits->currentData().toInt() << '\n';
    file.close();
}

void SerialScope::on_btnTcpStart_clicked() {
    

    bool ok = false;
    const quint16 port = ui.leTcpPort->text().toUShort(&ok);
    if (!ok || port == 0) {
        ui.statusBar->showMessage("端口必须是 1~65535 的数字", 5000);
        return;
    }

    TcpConfig cfg;
    cfg.port = port;
    cfg.role = static_cast<TcpConfig::Role>(ui.cmbTcpRole->currentData().toInt());
    if (cfg.role == TcpConfig::Client) {
        cfg.host = ui.leTcpHost->text().trimmed();
        if (cfg.host.isEmpty()) {
            ui.statusBar->showMessage("客户端模式必须填写主机地址", 5000);
            return;
        }
    }

    ui.btnTcpStart->setEnabled(false);
    emit startTcpRequested(cfg);
}

void SerialScope::on_btnTcpStop_clicked() {
    ui.btnTcpStop->setEnabled(false);
    emit stopTcpRequested();
}

void SerialScope::on_btnTcpSend_clicked()
{
    QByteArray data = ui.leTcpSend->text().toLocal8Bit();
    emit sendTcpDataRequested(data);
}

void SerialScope::on_btnTcpClear_clicked()
{
    ui.pteTcpReceive->clear();
}

void SerialScope::initTcp()
{
    ui.btnTcpStop->setEnabled(false);
    ui.pteTcpReceive->setMaximumBlockCount(5000);
    ui.cmbTcpRole->addItem("服务端", TcpConfig::Server);
    ui.cmbTcpRole->addItem("客户端", TcpConfig::Client);
    ui.leTcpHost->setText("127.0.0.1");
    ui.leTcpPort->setText("12345");
    setupTcpThread();
    connect(ui.cmbTcpRole, &QComboBox::currentIndexChanged, this, [this](int) {
        updateTcpRoleUi();
        });
    updateTcpRoleUi();
}

void SerialScope::updateTcpRoleUi()
{
    const bool isServer = (ui.cmbTcpRole->currentData().toInt() == TcpConfig::Server);
    ui.leTcpHost->setEnabled(!isServer);
    ui.leTcpHost->setToolTip(isServer ? "服务端监听所有网卡，不需要填主机" : "要连接的对端 IP 或域名");
}

void SerialScope::setupTcpThread()
{
    qRegisterMetaType<TcpConfig>("TcpConfig");
    m_tcpWorker = new TcpWorker();
    m_tcpThread = new QThread(this);
    m_tcpWorker->moveToThread(m_tcpThread);
    m_tcpThread->start();
    connect(m_tcpThread, &QThread::finished, m_tcpWorker, &QObject::deleteLater);

    // Ui -> Worker
    connect(this, &SerialScope::startTcpRequested, m_tcpWorker, &TcpWorker::startTcp);
    connect(this, &SerialScope::stopTcpRequested, m_tcpWorker, &TcpWorker::stopTcp);
    connect(this, &SerialScope::sendTcpDataRequested, m_tcpWorker, &TcpWorker::sendTcpData);

    // Worker -> Ui
    connect(m_tcpWorker, &TcpWorker::tcpStarted, this, &SerialScope::onTcpStarted);
    connect(m_tcpWorker, &TcpWorker::tcpStopped, this, &SerialScope::onTcpStopped);
    connect(m_tcpWorker, &TcpWorker::tcpStartFailed, this, &SerialScope::onTcpStartFailed);
    connect(m_tcpWorker, &TcpWorker::tcpDataReceived, this, &SerialScope::onTcpDataReceived);
    connect(m_tcpWorker, &TcpWorker::tcpSendFailed, this, &SerialScope::onTcpDataSendFailed);
    connect(m_tcpWorker, &TcpWorker::peerConnected, this, &SerialScope::onPeerConnected);
    connect(m_tcpWorker, &TcpWorker::peerDisconnected, this, &SerialScope::onPeerDisconnected);
}

void SerialScope::onTcpStarted(const QString& info)
{
    ui.btnTcpStart->setEnabled(false);
    ui.btnTcpStop->setEnabled(true);
    ui.statusBar->showMessage(info, 5000);
}

void SerialScope::onTcpStopped()
{
    ui.btnTcpStart->setEnabled(true);
    ui.btnTcpStop->setEnabled(false);
    ui.statusBar->showMessage("连接已断开", 5000);
}

void SerialScope::onTcpStartFailed(const QString& error) {
    ui.btnTcpStart->setEnabled(true);
    ui.btnTcpStop->setEnabled(false);
    ui.statusBar->showMessage(error, 5000);
}

void SerialScope::onTcpDataReceived(const QByteArray& data)
{
    ui.pteTcpReceive->appendPlainText(QString::fromLocal8Bit(data));
}

void SerialScope::onTcpDataSendFailed(const QString& error)
{
    ui.statusBar->showMessage(error, 5000);
}

void SerialScope::onPeerConnected(const QString& peerInfo)
{
    // 同一对端重复上报不加第二行（现在不会发生，但将来加"自动重连"后一定会）
    if (findPeerRow(ui.lstPeers, peerInfo) >= 0) return;

    auto* item = new QListWidgetItem(peerInfo, ui.lstPeers);  // 第二个参数 = 父控件，构造即插入
    item->setData(Qt::UserRole, peerInfo);                    // 身份和显示文本分开存
    ui.lstPeers->scrollToBottom();                            // 新来的对端滚到可见

    ui.statusBar->showMessage(
        QString("%1 已连接（当前 %2 个）").arg(peerInfo).arg(ui.lstPeers->count()), 5000);
}

void SerialScope::onPeerDisconnected(const QString& peerInfo)
{
    const int row = findPeerRow(ui.lstPeers, peerInfo);
    if (row >= 0) {
        delete ui.lstPeers->takeItem(row);   // takeItem 只摘下来、不释放；不 delete 就是内存泄漏
    }
    // 找不到也不报错：断开可能重复到达，或列表已被清空。这叫幂等

    ui.statusBar->showMessage(
        QString("%1 已断开（剩余 %2 个）").arg(peerInfo).arg(ui.lstPeers->count()), 5000);
}
