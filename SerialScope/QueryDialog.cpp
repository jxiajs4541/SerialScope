#include "QueryDialog.h"
#include "DbConfig.h"
#include <QDateTime>
#include <QTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QElapsedTimer>
#include <QSqlQueryModel>

QueryDialog::QueryDialog(QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);
	m_model = new QSqlQueryModel(this);
	ui.tblResult->setModel(m_model);

	ui.tblResult->setModel(m_model);
	ui.tblResult->setEditTriggers(QAbstractItemView::NoEditTriggers);  
	ui.tblResult->setSelectionBehavior(QAbstractItemView::SelectRows); 
	ui.tblResult->setAlternatingRowColors(true);

	ui.cmbDirection->addItem("全部", "ALL");
	ui.cmbDirection->addItem("TX", "TX");
	ui.cmbDirection->addItem("RX", "RX");
	ui.cmbDirection->addItem("事件 SYS/ERR", "EVENT");

	ui.cmbLimit->addItem("显示最新的100条", 100);
	ui.cmbLimit->addItem("显示最新的500条", 500);
	ui.cmbLimit->addItem("显示最新的1000条", 1000);
	ui.cmbLimit->addItem("显示最新的5000", 5000);
	ui.cmbLimit->setCurrentIndex(2);

	QDateTime now = QDateTime::currentDateTime();
	ui.dtStart->setDisplayFormat("yyyy-MM-dd HH:mm:ss.zzz");
	ui.dtEnd->setDisplayFormat("yyyy-MM-dd HH:mm:ss.zzz");
	ui.dtStart->setDateTime(QDateTime(now.date(), QTime(0, 0, 0)));
	ui.dtEnd->setDateTime(now);
}

QueryDialog::~QueryDialog()
{}

bool QueryDialog::openDatabase()
{
	if (m_db.isOpen()) return true;

	if (QSqlDatabase::contains("query_conn")) {
		m_db = QSqlDatabase::database("query_conn");
	}
	else {
		m_db = QSqlDatabase::addDatabase("QODBC", "query_conn");
	}
	
	m_db.setDatabaseName(dbDatabaseName());
	if (!m_db.open()) {
		ui.lblStatus->setText("数据库打开失败：" + m_db.lastError().text());
		return false;
	}
	return true;
}

void QueryDialog::on_btnQuery_clicked() {
	if (!openDatabase()) return;

	QDateTime t1 = ui.dtStart->dateTime();
	QDateTime t2 = ui.dtEnd->dateTime();

	if (t1 > t2) {
		ui.lblStatus->setText("起始时间大于结束时间,请重新设置");
		return;
	}

	QString sql="SELECT id, `time`, direction, hex_data, byte_count "
				"FROM serial_log "
				"WHERE `time` BETWEEN ? AND ? ";

	QString dir = ui.cmbDirection->currentData().toString();
	if(dir=="TX" || dir=="RX") sql += "AND direction = ? ";
	if (dir == "EVENT") sql += "AND direction IN (?,?) ";
	sql += QString("ORDER BY `time` DESC LIMIT %1;").arg(ui.cmbLimit->currentData().toInt());

	QSqlQuery query(m_db);
	query.prepare(sql);
	query.addBindValue(t1.toString("yyyy-MM-dd HH:mm:ss.zzz"));
	query.addBindValue(t2.toString("yyyy-MM-dd HH:mm:ss.zzz"));
	if (dir == "TX" || dir == "RX") query.addBindValue(dir);
	if (dir == "EVENT") query.addBindValue("SYS"), query.addBindValue("ERR");

	
	QElapsedTimer timer;
	timer.start();

	if (!query.exec()) {
		ui.lblStatus->setText("SQL执行失败：" + query.lastError().text());
		return;
	}

	qint64 ms = timer.elapsed();
	m_model->setQuery(std::move(query));
	m_model->setHeaderData(0, Qt::Horizontal, "ID");
	m_model->setHeaderData(1, Qt::Horizontal, "时间");
	m_model->setHeaderData(2, Qt::Horizontal, "方向");
	m_model->setHeaderData(3, Qt::Horizontal, "数据 HEX");
	m_model->setHeaderData(4, Qt::Horizontal, "字节数");

	if (m_model->lastError().isValid()) {
		ui.lblStatus->setText("查询失败：" + m_model->lastError().text());
		return;
	}
	ui.tblResult->resizeColumnToContents(0);
    ui.tblResult->resizeColumnToContents(1);
	ui.tblResult->resizeColumnToContents(2);
    ui.tblResult->resizeColumnToContents(3);
    ui.tblResult->resizeColumnToContents(4);

    ui.lblStatus->setText(QString("共 %1 条 · 耗时 %2 ms")
        .arg(m_model->rowCount()).arg(ms));
}

