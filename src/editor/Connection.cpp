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
			settings.value("login/address").value<QString>(),
			settings.value("login/database_name").value<QString>(),
			settings.value("login/user_name").value<QString>(),
			settings.value("login/password").value<QString>(),
			settings.value("login/auto_connect").value<bool>(),
			db
		);

		bool first = true;
		while(true) {

			if (!first || cd.autoConnect()) {

				QUrl url(cd.address());
				db.setHostName(url.host());
				db.setPort(url.port());
				db.setDatabaseName(cd.databaseName());
				db.setUserName(cd.userName());
				db.setPassword(cd.password());

				if (db.open()) {
					return Ok(std::move(db));
				}
			} 

			first = false;

			if (cd.exec() != QDialog::Accepted) {
				return Ok(db);
			} else {
				// save settings

				settings.setValue("login/address", cd.address());
				settings.setValue("login/database_name", cd.databaseName());
				settings.setValue("login/user_name", cd.userName());
				settings.setValue("login/password", cd.password());
				settings.setValue("login/auto_connect", cd.autoConnect());
			}
		}
	}
}