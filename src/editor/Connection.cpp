#include "Connection.h"
#include "ConnectDialog.h"

#include <QSettings>
#include <QSqlDatabase>
#include <QUrl>

namespace sg {
	Result<QSqlDatabase> CreateConnection() {

		QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");

		QSettings settings;

		ConnectDialog cd(
			settings.value("login/address").value<QString>().toStdString(),
			settings.value("login/database_name").value<QString>().toStdString(),
			settings.value("login/user_name").value<QString>().toStdString(),
			settings.value("login/password").value<QString>().toStdString(),
			settings.value("login/auto_connect").value<bool>(),
			db
		);

		bool first = true;
		while(true) {

			if (!first || cd.autoConnect()) {

				QUrl url(cd.address().c_str());
				db.setHostName(url.host());
				db.setPort(url.port());
				db.setDatabaseName(cd.databaseName().c_str());
				db.setUserName(cd.userName().c_str());
				db.setPassword(cd.password().c_str());

				if (db.open()) {
					return Ok(std::move(db));
				}
			} 

			first = false;

			if (cd.exec() != QDialog::Accepted) {
				return Ok(db);
			} else {
				// save settings

				settings.setValue("login/address", cd.address().c_str());
				settings.setValue("login/database_name", cd.databaseName().c_str());
				settings.setValue("login/user_name", cd.userName().c_str());
				settings.setValue("login/password", cd.password().c_str());
				settings.setValue("login/auto_connect", cd.autoConnect());
			}
		}
	}
}