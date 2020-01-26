#pragma once
#include "Result.h"

#include <QSqlDatabase>
#include <QString>
#include <QSet>
#include <QMap>
#include <QVariant>

#include <memory>
#include <vector>

namespace sg {

	// will convert "This isn't cool" to E'This isn\'t cool'
	QString ToSqlStringLiteral(QString str);
	QString ToSqlLiteral(QVariant value);

	class ICommand {
	public:
		virtual ~ICommand() {}
		virtual void markTablesAffected(QSet<QString>& tables) const=0;
		virtual Result<> perform(QSqlDatabase& db)=0;
		virtual Result<> undo(QSqlDatabase& db)=0;
	};

	class Transaction {
		class Controller& mController;
		QSqlDatabase* mConnection;
		Result<> mResult;
		std::vector<std::unique_ptr<ICommand>> mCommands;
		QString mDescription;
		QSet<QString> mTablesAffected;

		Transaction(Controller& controller, QString description);
		friend class Controller;
		
		Result<> performInternal(ICommand& cmd, bool undo);
		Result<> commitInternal();

		// this takes ownership of the command
		Result<> perform(ICommand* cmd);

	public:
		~Transaction();

		bool failed() const { return mResult.failed(); }

		ResultError error() { return mResult.error(); }

		Result<> commit();
		bool hasCommands() const;

		// returns the new primary key
		Result<QVariant> insert(const QString& table_name, const QMap<QString, QVariant>& values, const QString& primary_key);
		Result<> deleteRow(const QString& table_name, const QString& primary_key, const QVariant& value);

		Result<> createTable(const QString& table_name, const QStringList& types);
		Result<> dropTable(QString table_name, QStringList columns);
		Result<> lockTable(const QString& table_name);

		Result<> update(QString table_name, QMap<QString, QVariant> values, 
			QMap<QString, QVariant> prev_values, QString primary_key, QVariant primary_key_value);
		Result<> renameTable(const QString& old_table_name, const QString& new_table_name);

		QSqlDatabase* connection() const { return mConnection; }
	};

	class Controller : public QObject {
		Q_OBJECT

		QSqlDatabase mConnection;

		friend class Transaction;

		struct CommandGroup {
			QString mDescription;
			std::vector<std::unique_ptr<ICommand>> mCommands;
		};

		std::vector<CommandGroup> mUndoStack;
		size_t mUndoStackIndex = 0;

	public:

		Controller(QSqlDatabase connection)
		: mConnection(std::move(connection))
		{}
		
		~Controller() {}

		Result<> undo();
		Result<> redo();

		Transaction createTransaction(QString name);

	signals:
		void dataChanged(const QSet<QString>& tables_affected);
	};
}