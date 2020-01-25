#pragma once

#include <QDialog>

class QPushButton;
class QLineEdit;
class QString;
class QSqlDatabase;
class QCheckBox;

namespace sg {
	class ConnectDialog : public QDialog {

		QLineEdit *const mAddress, *const mDatabaseName, *const mUserName, *const mPassword;
		QPushButton *const mConnectButton;
		QCheckBox *const mAutoConnect;

	public:
		ConnectDialog(QString address, QString database_name, QString user_name, QString password, bool auto_connect,
			QSqlDatabase& db, QWidget *parent=nullptr, Qt::WindowFlags f = Qt::WindowFlags());
		~ConnectDialog();

		QString address() const;
		QString databaseName() const;
		QString userName() const;
		QString password() const;
		bool autoConnect() const;
	};
}