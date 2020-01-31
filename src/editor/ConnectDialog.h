#pragma once

#include <QDialog>

#include <string_view>

class QPushButton;
class QLineEdit;
class QSqlDatabase;
class QCheckBox;

namespace sg {
	class ConnectDialog : public QDialog {

		QLineEdit *const mAddress, *const mDatabaseName, *const mUserName, *const mPassword;
		QPushButton *const mConnectButton;
		QCheckBox *const mAutoConnect;

	public:
		ConnectDialog(std::string_view address, std::string_view database_name, std::string_view user_name, std::string_view password, bool auto_connect,
			QSqlDatabase& db, QWidget *parent=nullptr, Qt::WindowFlags f = Qt::WindowFlags());
		~ConnectDialog();

		std::string address() const;
		std::string databaseName() const;
		std::string userName() const;
		std::string password() const;
		bool autoConnect() const;
	};
}