#pragma once

#include <QDialog>
#include <ui_QueryDialog.h>
#include <QSqlDatabase>
#include <QSqlQueryModel>

class QueryDialog  : public QDialog
{
	Q_OBJECT

public:
	QueryDialog(QWidget *parent = nullptr);
	~QueryDialog();

signals:
	void dbError(const QString &err);

private slots:
	void on_btnQuery_clicked();

private:
	Ui::QueryDialog ui;
	bool openDatabase();
	bool m_dbOk=false;
	QSqlDatabase m_db;
	QSqlQueryModel* m_model = nullptr;
};

