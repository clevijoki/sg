	#include "ConnectDialog.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QUrl>

namespace sg {

	ConnectDialog::ConnectDialog(
		std::string_view address, std::string_view database_name, std::string_view user_name, std::string_view password, bool auto_connect,
		QSqlDatabase& db, QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f)
	, mAddress(new QLineEdit(QString((const QChar*)address.data(), address.length()), this))
	, mDatabaseName(new QLineEdit(QString((const QChar*)database_name.data(), database_name.length()), this))
	, mUserName(new QLineEdit(QString((const QChar*)user_name.data(), user_name.length()), this))
	, mPassword(new QLineEdit(QString((const QChar*)password.data(), password.length()), this))
	, mConnectButton(new QPushButton(tr("Connect"), this))
	, mAutoConnect(new QCheckBox(tr("Auto Connect"), this)) {

		setWindowTitle(tr("Database Connect"));

		auto layout = new QGridLayout(this);

		layout->addWidget(new QLabel(tr("URL"), this), 0, 0);
		layout->addWidget(mAddress, 0, 1);

		layout->addWidget(new QLabel(tr("Database Name"), this), 1, 0);
		layout->addWidget(mDatabaseName, 1, 1);

		layout->addWidget(new QLabel(tr("User Name"), this), 2, 0);
		layout->addWidget(mUserName, 2, 1);

		layout->addWidget(new QLabel(tr("Password"), this), 3, 0);
		layout->addWidget(mPassword, 3, 1);

		mAutoConnect->setCheckState(auto_connect ? Qt::Checked : Qt::Unchecked);
		layout->addWidget(mAutoConnect, 4, 0, 1, 2, Qt::AlignRight);

		layout->addWidget(mConnectButton, 5, 0, 1, 2, Qt::AlignHCenter);

		QLabel* status_label = new QLabel("", this);
		status_label->setVisible(false);
		layout->addWidget(status_label, 6, 0, 1, 2);

		QObject::connect(mConnectButton, &QPushButton::clicked, [this, status_label, &db]() {

			db.setDatabaseName(this->databaseName().c_str());
			db.setUserName(this->userName().c_str());
			db.setPassword(this->password().c_str());
			QUrl url(this->address().c_str());
			db.setHostName(url.host());
			db.setPort(url.port());

			if (db.open()) {
				this->done(QDialog::Accepted);
			} else {
				status_label->setText(db.lastError().text());
				status_label->setVisible(true);
			}
		});
		this->setLayout(layout);
	}

	ConnectDialog::~ConnectDialog() {

	}

	std::string ConnectDialog::address() const {
		return mAddress->text().toStdString();
	}

	std::string ConnectDialog::databaseName() const {
		return mDatabaseName->text().toStdString();
	}

	std::string ConnectDialog::userName() const {
		return mUserName->text().toStdString();
	}

	std::string ConnectDialog::password() const {
		return mPassword->text().toStdString();
	}

	bool ConnectDialog::autoConnect() const {
		return mAutoConnect->checkState() == Qt::Checked;
	}
}