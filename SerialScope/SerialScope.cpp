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

SerialScope::SerialScope(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    m_lblCounts=new QLabel("收: 0  发: 0", this);
    ui.statusBar->addPermanentWidget(m_lblCounts);
    // =====初始化=====
    on_btnRefresh_clicked();
    on_chkAutoWrap_toggled(ui.chkAutoWrap->isChecked());
    initComboBoxes();
    setupSerialThread();
    ui.pteReceive->setMaximumBlockCount(5000);
    loadConfig();
}

SerialScope::~SerialScope()
{
   
    m_serialThread->quit();
    m_serialThread->wait();
}


// ===== private slots =====
// 按钮：刷新串口
void SerialScope::on_btnRefresh_clicked() {       
    ui.cmbPortName->clear();
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        ui.cmbPortName->addItem(info.portName() + " - " + info.description(), info.portName());
    }
}

//按钮：串口打开/关闭请求
void SerialScope::on_btnOpenClose_clicked() {     
    if (ui.cmbPortName->currentText().isEmpty()) {
        ui.statusBar->showMessage("请选择串口!", 5000);
        return;
    }
    ui.btnOpenClose->setEnabled(false);
    if (m_isOpen) {
        emit closePortRequested();
    }
    else {
        SerialConfig cfg;
        cfg.portName = ui.cmbPortName->currentData().toString();
        cfg.baud = ui.cmbBaud->currentData().toInt();
        cfg.dataBits = ui.cmbDataBits->currentData().value<QSerialPort::DataBits>();
        cfg.parity = ui.cmbParity->currentData().value<QSerialPort::Parity>();
        cfg.stopBits = ui.cmbStopBits->currentData().value<QSerialPort::StopBits>();
        emit openPortRequested(cfg);
    }
}

// 按钮：清除接收
void SerialScope::on_btnClearReceive_clicked() {      
    ui.pteReceive->clear();
}

// 按钮：点击发送
void SerialScope::on_btnSend_clicked() {      
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

    emit sendDataRequested(data);
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
void SerialScope::on_btnQueryDb_clicked()
{
    QueryDialog dlg(this);
    dlg.exec();
}

// 串口已打开
void SerialScope::onSerialOpened() {    
    ui.widget->setEnabled(false);
    ui.btnOpenClose->setEnabled(true);
    ui.statusBar->showMessage("串口已打开", 5000);
    ui.btnOpenClose->setText("关闭串口");
    m_isOpen = true;
}

// 串口已关闭
void SerialScope::onSerialClosed() {   
    ui.widget->setEnabled(true);
    ui.btnOpenClose->setEnabled(true);
    ui.statusBar->showMessage("串口已关闭", 5000);
    ui.btnOpenClose->setText("打开串口");
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
    saveConfig();
    QMainWindow::closeEvent(event);
}

void SerialScope::initComboBoxes()
{
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
    connect(this, &SerialScope::openPortRequested, m_serialWorker, &SerialWorker::openSerialPort);
    connect(this, &SerialScope::closePortRequested, m_serialWorker, &SerialWorker::closeSerialPort);
    connect(this, &SerialScope::sendDataRequested, m_serialWorker, &SerialWorker::sendData);
    connect(this, &SerialScope::setLogFileRequested, m_serialWorker, &SerialWorker::setLogFile);


    // m_serialWorker -> UI
    connect(m_serialWorker, &SerialWorker::serialOpened, this, &SerialScope::onSerialOpened);
    connect(m_serialWorker, &SerialWorker::serialOpenFailed, this, [this](const QString& error) {     //串口打开失败
        ui.btnOpenClose->setEnabled(true);
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
void SerialScope::loadConfig()        
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
void SerialScope::saveConfig()        
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

