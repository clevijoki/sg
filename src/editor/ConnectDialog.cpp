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
		QString address, QString database_name, QString user_name, QString password, bool auto_connect,
		QSqlDatabase& db, QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f)
	, mAddress(new QLineEdit(address, this))
	, mDatabaseName(new QLineEdit(database_name, this))
	, mUserName(new QLineEdit(user_name, this))
	, mPassword(new QLineEdit(password, this))
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

			db.setDatabaseName(this->databaseName());
			db.setUserName(this->userName());
			db.setPassword(this->password());
			QUrl url(this->address());
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

	QString ConnectDialog::address() const {
		return mAddress->text();
	}

	QString ConnectDialog::databaseName() const {
		return mDatabaseName->text();
	}

	QString ConnectDialog::userName() const {
		return mUserName->text();
	}

	QString ConnectDialog::password() const {
		return mPassword->text();
	}

	bool ConnectDialog::autoConnect() const {
		return mAutoConnect->checkState() == Qt::Checked;
	}
}