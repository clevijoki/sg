#pragma once
#include <QString>
#include <QMessageBox>
#include <QDialog>
#include "FormatString.h"

namespace sg {

	class SilenceMessageBoxScope {
	public:
		SilenceMessageBoxScope();
		~SilenceMessageBoxScope();
	};
	
	class MessageBox : public QDialog
	{
		Q_OBJECT

	public:
		MessageBox(QString title, QString message, QString info, QWidget *parent=nullptr);
	};

	/*
	Wrapper around QMessageBox::critical to be able to assert in unit tests
	*/
	void MessageBoxCritical(StringBuilder title, StringBuilder message, StringBuilder info=QString());
	QMessageBox::StandardButton MessageBoxQuestion(StringBuilder title, StringBuilder message, QMessageBox::StandardButton default_buttons=QMessageBox::No, QMessageBox::StandardButtons buttons=QMessageBox::Yes|QMessageBox::No);
}
