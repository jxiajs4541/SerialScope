#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_SerialScope.h"
#include "SerialWorker.h"  

class QThread;
class QLabel;

class SerialScope : public QMainWindow
{
    Q_OBJECT

public:
    SerialScope(QWidget* parent = nullptr);
    ~SerialScope();

signals:
    // 打开串口请求
    void openPortRequested(const SerialConfig& config);
    // 关闭串口请求
    void closePortRequested();
    // 发送数据请求
    void sendDataRequested(const QByteArray& data);
    // 记录日志请求
    void setLogFileRequested(const QString &path);

private slots:
    // 刷新串口
    void on_btnRefresh_clicked();
    // 打开/关闭串口
    void on_btnOpenClose_clicked();
    // 清除接收
    void on_btnClearReceive_clicked();
    // 点击发送
    void on_btnSend_clicked();
    // 日志记录
    void on_chkLogEnable_toggled(bool checked);
    // 自动换行
    void on_chkAutoWrap_toggled(bool checked);
    // 数据查询
    void on_btnQueryDb_clicked();

    // 串口已打开
    void onSerialOpened();
    // 串口已关闭
    void onSerialClosed();
    // 接收消息
    void onSerialDataReceived(const QByteArray& data);
    // 收发计数变化
    void onCountsChanged(qint64 rxCount, qint64 txCount);

protected:
    void closeEvent(QCloseEvent* event) override;

private:

    Ui::SerialScopeClass ui;

    // 初始化
    void initComboBoxes();        // 填充波特率/数据位/校验位/停止位下拉框
    void setupSerialThread();     // 创建 worker、线程、建立全部连接

    // 加载配置
    void loadConfig();

    // 保存配置
    void saveConfig();

    SerialWorker* m_serialWorker = nullptr;
    QThread* m_serialThread = nullptr;
    QLabel* m_lblCounts = nullptr;
    bool m_isOpen = false;

};
