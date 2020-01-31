	#include "ConnectDialog.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QUrl>
#include <QLatin1String>

namespace sg {

	ConnectDialog::ConnectDialog(
		std::string_view address, std::string_view database_name, std::string_view user_name, std::string_view password, bool auto_connect,
		QSqlDatabase& db, QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f)
	, mAddress(new QLineEdit(QLatin1String(address.data(), (int)address.size()), this))
	, mDatabaseName(new QLineEdit(QLatin1String(database_name.data(), (int)database_name.size()), this))
	, mUserName(new QLineEdit(QLatin1String(user_name.data(), (int)user_name.size()), this))
	, mPassword(new QLineEdit(QLatin1String(password.data(), (int)password.size()), this))
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