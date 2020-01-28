#include "Controller.h"
#include <gtest/gtest.h>

#include <QSqlError>
#include <QSqlDriver>
#include <QString>
#include <QWidget>
#include <QSqlField>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QList>
#include <QSqlQueryModel>
#include <QDebug>

namespace sg {

	static QString ColumnStr(const QList<QString>& columns) {
		QString result;

		for (const QString& c : columns) {
			if (!result.isEmpty()) {
				result += ", ";
			}

			result += '"';
			result += c;
			result += '"';
		}

		return result;
	}

	QString ToSqlStringLiteral(QString str) {
		bool needs_escape = false;
		auto escape = [&](QChar c, QString replacement) {
			if (str.indexOf(c) == -1)
				return;

			str.replace(c, replacement);
			needs_escape = true;
		};

		escape('\n', "\\n");
		escape('\t', "\\t");
		escape('\'', "\\'");

		QString result;
		if (needs_escape)
			result += 'E';

		result += '\'';
		result += str;
		result += '\'';

		return result;
	}

	QString ToSqlLiteral(QVariant value) {
		QMetaType::Type t = static_cast<QMetaType::Type>(value.type());

		switch (t) {
			case QMetaType::Bool:
			case QMetaType::Int:
			case QMetaType::UInt:
			case QMetaType::Double:
			case QMetaType::Long:
			case QMetaType::LongLong:
			case QMetaType::Short:
			case QMetaType::ULong:
			case QMetaType::ULongLong:
			case QMetaType::UShort:
			case QMetaType::Float:
				return value.toString();

			case QMetaType::QPoint: {
				auto p = value.toPoint();
				return QString("'(%1, %2)'").arg(p.x()).arg(p.y());
			}
			case QMetaType::QPointF: {
				auto p = value.toPointF();
				return QString("'(%1, %2)'").arg(p.x()).arg(p.y());
			}

			default:
				return ToSqlStringLiteral(value.toString());
		}
	}

	static QString ValueStr(const QList<QVariant>& values) {

		QString result;

		for (const auto& value : values) {
			if (!result.isEmpty()) {
				result += ", ";
			}

			result += ToSqlLiteral(value);
		}

		return result;
	}

	static QString MapKeyValueStr(const QString& op, const QString& join_str, const QMap<QString, QVariant>& values) {

		QString result;
		for (const auto& key : values.keys()) {
			if (!result.isEmpty())
				result += join_str;

			result += QString("\"%1\" %2 %3")
				.arg(key)
				.arg(op)
				.arg(ToSqlLiteral(values[key]));
		}

		return result;
	}

	static Result<> PerformQuery(QSqlDatabase& db, const QString& statement) {

		qDebug() << statement;
		QSqlQuery q(db);
		if (!q.exec(statement)) {
			return Error(q.lastError().text(), statement);
		}

		return Ok();
	}

	Transaction::Transaction(Controller& controller, QString description)
	: mController(controller)
	, mConnection(&controller.mConnection)
	, mResult(Ok())
	, mDescription(std::move(description))
	{
		if (!mConnection->transaction()) {
			mResult = Error(QWidget::tr("Unable to start transaction"), mConnection->lastError().text());
			mConnection = nullptr;
		}
	}

	Transaction::~Transaction() {
		if (mConnection) {
			Q_ASSERT(mConnection->rollback());

			emit mController.dataChanged(mTablesAffected);
		}
	}

	Result<> Transaction::commitInternal() {
		if (mConnection) {
			if (mConnection->commit()) {
				mConnection = nullptr;

				emit mController.dataChanged(mTablesAffected);

				return Ok();
			}

			mResult = Error(QWidget::tr("Error comitting"), mConnection->lastError().text());
		}

		if (mResult.failed())
			return mResult.error();

		return Error("Internal error, transaction using invalid connection");
	}

	Result<> Transaction::commit() {

		if (failed())
			return error();

		auto res = commitInternal();
		if (res.failed())
			return res.error();

		Controller::CommandGroup cg;
		cg.mDescription = std::move(mDescription);
		cg.mCommands = std::move(mCommands);

		// succeeded, add this to our 'undo' stack at the end
		mController.mUndoStack.resize(mController.mUndoStackIndex);
		mController.mUndoStack.emplace_back(std::move(cg));
		mController.mUndoStackIndex++;

		return Ok();
	}

	bool Transaction::hasCommands() const {
		return !mCommands.empty();
	}

	Result<> Transaction::performInternal(ICommand& cmd, bool undo) {

		auto res = undo ? cmd.undo(*mConnection) : cmd.perform(*mConnection);
		if (res.failed()) {

			mConnection->rollback();
			mConnection = nullptr;

			emit mController.dataChanged(mTablesAffected);

			mResult = res;

			return res.error();
		} else {
			cmd.markTablesAffected(mTablesAffected);
		}

		return Ok();
	}


	Result<> Transaction::perform(ICommand* in_cmd) {

		if (failed())
			return error();

		std::unique_ptr<ICommand> cmd(in_cmd);

		auto res = performInternal(*cmd, false);
		if (res.failed())
			return res.error();

		mCommands.emplace_back(std::move(cmd));
		return Ok();
	}

	class CmdInsert : public ICommand {
		QString mTableName;
		QMap<QString, QVariant> mValues;
		QString mPrimaryKey;
		QVariant mInsertedKey;

	public:

		CmdInsert(QString table_name, QMap<QString, QVariant> values, QString primary_key)
		: mTableName(std::move(table_name))
		, mValues(std::move(values))
		, mPrimaryKey(std::move(primary_key)) {
		}

		void markTablesAffected(QSet<QString>& tables) const override {
			tables |= mTableName;
		}

		// this is performed the first time, and returns/sets the primary key
		Result<QVariant> performInternal(QSqlDatabase& db) {

			QString statement = QString("INSERT INTO \"%1\"(%2) VALUES (%3) RETURNING \"%4\"")
				.arg(mTableName)
				.arg(ColumnStr(mValues.keys()))
				.arg(ValueStr(mValues.values()))
				.arg(mPrimaryKey);

			QSqlQuery q(db);

			if (!q.exec(statement)) {
				return Error(q.lastError().text(), statement);
			}

			while (q.next()) {
				mInsertedKey = q.value(0);
			}

			if (!mValues.contains(mPrimaryKey)) {
				// ensure this is inserted on redo
				mValues[mPrimaryKey] = mInsertedKey;
			}

			return Ok(mInsertedKey);
		}

		Result<> perform(QSqlDatabase& db) override {

			QString statement = QString("INSERT INTO \"%1\"(%2) VALUES (%3)")
				.arg(mTableName)
				.arg(ColumnStr(mValues.keys()))
				.arg(ValueStr(mValues.values()));

			return PerformQuery(db, statement);
		}

		Result<> undo(QSqlDatabase& db) override {

			return PerformQuery(db, QString("DELETE FROM \"%1\" WHERE \"%2\" = %3")
				.arg(mTableName)
				.arg(mPrimaryKey)
				.arg(ToSqlLiteral(mInsertedKey)));
		}
	};

	Result<QVariant> Transaction::insert(const QString& table_name, const QMap<QString, QVariant>& values, const QString& primary_key) {

		if (failed())
			return error();

		auto cmd = std::make_unique<CmdInsert>(table_name, values, primary_key);
		auto res = cmd->performInternal(*mConnection);

		if (res.failed()) {

			mConnection->rollback();
			mConnection = nullptr;

			emit mController.dataChanged(mTablesAffected);

			mResult = res.errorCopy();
			return res.error();

		}
		cmd->markTablesAffected(mTablesAffected);
		mCommands.emplace_back(cmd.release());
		return res;
	}

	class CmdDeleteRow : public ICommand {
		QString mTableName;
		QString mPrimaryKey;
		QVariant mValue;
		QMap<QString, QVariant> mPrevValues;

	public:
		CmdDeleteRow(QString table_name, QString primary_key, QVariant value)
		: mTableName(std::move(table_name))
		, mPrimaryKey(primary_key)
		, mValue(value) 
		{}

		void markTablesAffected(QSet<QString>& tables) const override {
			tables |= mTableName;
		}

		Result<> perform(QSqlDatabase& db) override {

			if (mPrevValues.isEmpty())
			{
				QString current_data_statement = QString("SELECT * from \"%1\" WHERE \"%2\" = %3")
					.arg(mTableName)
					.arg(mPrimaryKey)
					.arg(ToSqlLiteral(mValue));

				QSqlQuery q(db);
				if (!q.exec(current_data_statement)) {
					return Error(q.lastError().text(), current_data_statement);
				}

				while (q.next()) {

					for (int n = 0; n < q.record().count(); ++n) {
						mPrevValues[q.record().fieldName(n)] = q.value(n);
					}
				}

				if (mPrevValues.isEmpty())
					return Error("No rows found to delete", current_data_statement);;
			}

			return PerformQuery(db, QString("DELETE FROM \"%1\" WHERE \"%2\" = %3")
					.arg(mTableName)
					.arg(mPrimaryKey)
					.arg(ToSqlLiteral(mValue))
			);
		}

		Result<> undo(QSqlDatabase& db) {

			if (mPrevValues.isEmpty())
				return Ok(); // deleted nothing during perform

			return PerformQuery(db, QString("INSERT INTO \"%1\" (%2) VALUES (%3)")
				.arg(mTableName)
				.arg(ColumnStr(mPrevValues.keys()))
				.arg(ValueStr(mPrevValues.values()))
			);
		}
	};

	Result<> Transaction::deleteRow(const QString& table_name, const QString& primary_key, const QVariant& value) {
		return perform(new CmdDeleteRow(table_name, primary_key, value));
	}

	class CmdCreateTable : public ICommand {

		QString mTableName;
		QStringList mColumns;

	public:
		CmdCreateTable(QString table_name, QStringList columns)
		: mTableName(std::move(table_name))
		, mColumns(std::move(columns))
		{}

		void markTablesAffected(QSet<QString>& tables) const override {
			tables |= mTableName;
		}

		Result<> perform(QSqlDatabase& db) override {
			return PerformQuery(db, QString("CREATE TABLE \"%1\" (%2)")
				.arg(mTableName)
				.arg(mColumns.join(", "))
			);
		}

		Result<> undo(QSqlDatabase& db) override {
			return PerformQuery(db, QString("DROP TABLE \"%1\"")
				.arg(mTableName)
			);
		}
	};

	Result<> Transaction::createTable(const QString& table_name, const QStringList& columns) {
		return perform(new CmdCreateTable(table_name, columns));
	}

	class CmdDropTable : public ICommand {
		QString mTableName;
		QVector<QMap<QString, QVariant>> mTableRows;
		QStringList mReCreateColumns;

	public:

		CmdDropTable(QString table_name, QStringList re_create_columns)
		: mTableName(std::move(table_name))
		, mReCreateColumns(std::move(re_create_columns))
		{}

		void markTablesAffected(QSet<QString>& tables) const override {
			tables |= mTableName;
		}

		Result<> perform(QSqlDatabase& db) override {

			if (mTableRows.isEmpty()) {
				// query all of the data to restore
				QSqlQuery q(db);
				QString statement = QString("SELECT * FROM \"%1\"").arg(mTableName);
				if (!q.exec(statement)) {
					return Error(q.lastError().text(), statement);
				}

				while (q.next()) {
					QMap<QString, QVariant> row;

					for (int n = 0; n < q.record().count(); ++n) {
						row[q.record().fieldName(n)] = q.value(n);
					}

					mTableRows.push_back(std::move(row));
				}
			}

			return PerformQuery(db, QString("DROP TABLE \"%1\"").arg(mTableName));
		}

		Result<> undo(QSqlDatabase& db) override {

			QString create = QString("CREATE TABLE \"%1\" (%2)")
				.arg(mTableName)
				.arg(mReCreateColumns.join(", "));

			auto res = PerformQuery(db, create);
			if (res.failed())
				return res.error();

			if (mTableRows.isEmpty())
				return Ok();

			QString values_str;

			for (const auto& kv : mTableRows) {
				if (!values_str.isEmpty()) {
					values_str += ", ";
				}

				values_str += QString("(%1)").arg(ValueStr(kv.values()));
			}

			QString insert = QString("INSERT INTO \"%1\" (%2) VALUES %3")
				.arg(mTableName)
				.arg(ColumnStr(mTableRows[0].keys()))
				.arg(values_str);

			return PerformQuery(db, insert);
		}
	};

	Result<> Transaction::dropTable(QString table_name, QStringList columns) {
		return perform(new CmdDropTable(std::move(table_name), std::move(columns)));
	}

	Result<> Transaction::lockTable(const QString& table_name) {

		Q_ASSERT(mConnection);

		if (failed())
			return error();

		QString statement = QString("LOCK TABLE \"%1\" IN EXCLUSIVE MODE")
			.arg(table_name);

		QSqlQuery q(*mConnection);
		if (!q.exec(statement)) {
			return Error(q.lastError().text(), statement);
		}

		return Ok();
	}

	class CmdUpdate : public ICommand {

		QString mTableName;
		QMap<QString, QVariant> mTableValues;
		QString mPrimaryKey;
		QVariant mKeyValue;
		QVariant mNewPrimaryKey;
		QMap<QString, QVariant> mPrevValues;

	public:
		CmdUpdate(QString table_name, QMap<QString, QVariant> values, QMap<QString, QVariant> prev_values, QString primary_key, QVariant key_value, QVariant new_primary_key)
		: mTableName(std::move(table_name))
		, mTableValues(std::move(values))
		, mPrevValues(std::move(prev_values))
		, mPrimaryKey(std::move(primary_key))
		, mKeyValue(std::move(key_value))
		, mNewPrimaryKey(std::move(new_primary_key))
		{}

		void markTablesAffected(QSet<QString>& tables) const override {
			tables |= mTableName;
		}

		Result<> perform(QSqlDatabase& db) override {

			return PerformQuery(db, QString("UPDATE \"%1\" SET %2 WHERE \"%3\" = %4")
				.arg(mTableName)
				.arg(MapKeyValueStr("=", ", ", mTableValues))
				.arg(mPrimaryKey)
				.arg(ToSqlLiteral(mKeyValue))
			);

		}

		Result<> undo(QSqlDatabase& db) override {
			return PerformQuery(db, QString("UPDATE \"%1\" SET %2 WHERE \"%3\" = %4")
				.arg(mTableName)
				.arg(MapKeyValueStr("=", ", ", mPrevValues))
				.arg(mPrimaryKey)
				.arg(ToSqlLiteral(mNewPrimaryKey))
			);
		}
	};

	Result<> Transaction::update(QString table_name, QMap<QString, QVariant> values, 
		QMap<QString, QVariant> prev_values, QString primary_key, QVariant primary_key_value) {

		QVariant new_primary_key;

		if (values.contains(primary_key)) {
			new_primary_key = values[primary_key];
		} else {
			new_primary_key = primary_key_value;
		}

		return perform(
			new CmdUpdate(
				std::move(table_name),
				std::move(values),
				std::move(prev_values),
				std::move(primary_key),
				std::move(primary_key_value),
				std::move(new_primary_key)
			)
		);
	}

	class CmdRenameTable : public ICommand {
		QString mOldName;
		QString mNewName;

	public:

		CmdRenameTable(QString old_name, QString new_name) 
		: mOldName(std::move(old_name))
		, mNewName(std::move(new_name)) {
		}

		void markTablesAffected(QSet<QString>& tables) const override {
			tables |= mOldName;
			tables |= mNewName;
		}

		Result<> perform(QSqlDatabase& db) {
			return PerformQuery(db, QString("ALTER TABLE \"%1\" RENAME TO \"%2\"")
				.arg(mOldName)
				.arg(mNewName)
			);
		}

		Result<> undo(QSqlDatabase& db) {
			return PerformQuery(db, QString("ALTER TABLE \"%1\" RENAME TO \"%2\"")
				.arg(mNewName)
				.arg(mOldName)
			);
		}
	};

	Result<> Transaction::renameTable(const QString& old_table_name, const QString& new_table_name){
		return perform(new CmdRenameTable(old_table_name, new_table_name));
	}

	Transaction Controller::createTransaction(QString description) {
		return Transaction(*this, std::move(description));
	}

	Result<> Controller::undo() {

		if (mUndoStackIndex != 0) {
			mUndoStackIndex--;

			// perform the undo 
			Transaction t(*this, "Undo");

			const CommandGroup& cg = mUndoStack[mUndoStackIndex];

			for (auto itr = cg.mCommands.rbegin(); itr != cg.mCommands.rend(); ++itr) {

				auto res = t.performInternal(**itr, true);
				if (res.failed())
					return res.error();
			}

			return t.commitInternal();
		}

		return Ok();
	}

	Result<> Controller::redo() {

		if (mUndoStackIndex < mUndoStack.size()) {

			Transaction t(*this, "Redo");

			const CommandGroup& cg = mUndoStack[mUndoStackIndex];

			for (auto itr = cg.mCommands.begin(); itr != cg.mCommands.end(); ++itr) {

				auto res = t.performInternal(**itr, false);
				if (res.failed())
					return res.error();
			}

			auto res = t.commitInternal();
			if (res.failed())
				return res.error();

			++mUndoStackIndex;

			return Ok();
		}

		return Ok();
	}

	QSqlDatabase CreateTestDB() {

		static QSqlDatabase result;

		if (!result.isOpen()) {
			result = QSqlDatabase::addDatabase("QPSQL");
			result.setDatabaseName("sg_unittest");

#if WIN32
			result.setUserName("sg_unittest");
			result.setPassword("sg_unittest_pw");
#endif

			EXPECT_TRUE(result.open());
		}

		return result;			
	} 
}

TEST(Controller, ToSqlStringLiteral) {
	EXPECT_STREQ("E'This isn\\'t cool'", sg::ToSqlStringLiteral("This isn't cool").toStdString().c_str());
	EXPECT_STREQ("'This does not need escaping'", sg::ToSqlStringLiteral("This does not need escaping").toStdString().c_str());
}

TEST(Controller, ValueStr) {

	EXPECT_STREQ("'Normal String', 10, 100, 3.14", sg::ValueStr({"Normal String", 10, 100.0f, 3.14}).toStdString().c_str());
}

TEST(Controller, CmdCreateTable) {

	QSqlDatabase db = sg::CreateTestDB();

	QSqlQuery q(db);

	if (db.tables().contains("cmd_create_table")) {
		EXPECT_TRUE(q.exec("DROP TABLE cmd_create_table")) << q.lastError().text().toStdString().c_str();
	}

	sg::Controller c(db);

	EXPECT_FALSE(db.tables().contains("cmd_create_table"));

	{
		auto t = c.createTransaction("CmdCreateTable");
		t.createTable("cmd_create_table", {"id SERIAL PRIMARY KEY", "name VARCHAR(32) NOT NULL"}).verify();
		t.commit().verify();
	}

	EXPECT_TRUE(db.tables().contains("cmd_create_table"));
	EXPECT_EQ(2, db.record("cmd_create_table").count());

	c.undo().verify();

	EXPECT_FALSE(db.tables().contains("cmd_create_table"));

	c.redo().verify();

	EXPECT_TRUE(db.tables().contains("cmd_create_table"));
	EXPECT_EQ(2, db.record("cmd_create_table").count());
}

TEST(Controller, CmdInsert) {

	QSqlDatabase db = sg::CreateTestDB();

	QSqlQuery q(db);

	if (db.tables().contains("cmd_insert")) {
		EXPECT_TRUE(q.exec("DROP TABLE cmd_insert")) << q.lastError().text().toStdString().c_str();
	}

	sg::Controller c(db);

	{
		auto t = c.createTransaction("CmdInsert Setup");
		t.createTable("cmd_insert", {"id SERIAL PRIMARY KEY", "name VARCHAR(128) NOT NULL"}).verify();
		t.commit().verify();
	}

	{
		QSqlQueryModel m;
		m.setQuery("SELECT * FROM cmd_insert", db);
		EXPECT_EQ(0, m.rowCount());
	}

	{
		auto t = c.createTransaction("CmdInsert Insert");
		t.insert("cmd_insert", {{"name", "Test 1"}}, "id").verify();
		t.insert("cmd_insert", {{"name", "Test 2"}}, "id").verify();
		t.commit().verify();
	}

	{
		QSqlQueryModel m;
		m.setQuery("SELECT * FROM cmd_insert", db);
		EXPECT_EQ(2, m.rowCount());
	}

	c.undo().verify();

	{
		QSqlQueryModel m;
		m.setQuery("SELECT * FROM cmd_insert", db);
		EXPECT_EQ(0, m.rowCount());
	}

	c.redo().verify();

	{
		QSqlQueryModel m;
		m.setQuery("SELECT id, name FROM cmd_insert", db);
		EXPECT_EQ(2, m.rowCount());

		EXPECT_STREQ("Test 1", m.data(m.index(0,1)).toString().toStdString().c_str());
		EXPECT_STREQ("Test 2", m.data(m.index(1,1)).toString().toStdString().c_str());
	}
}

TEST(Controller, CmdDeleteRow) {

	QSqlDatabase db = sg::CreateTestDB();

	QSqlQuery q(db);
	QSqlQueryModel m;

	if (db.tables().contains("cmd_delete_row")) {
		EXPECT_TRUE(q.exec("DROP TABLE cmd_delete_row")) << q.lastError().text().toStdString().c_str();
	}

	sg::Controller c(db);

	QVariant delete_id;

	{
		auto t = c.createTransaction("CmdDeleteRow Setup");
		t.createTable("cmd_delete_row", {"id SERIAL PRIMARY KEY", "name VARCHAR(128) NOT NULL"}).verify();
		t.insert("cmd_delete_row", {{"name", "neato"}}, "id").verify();
		delete_id = *t.insert("cmd_delete_row", {{"name", "burrito"}}, "id");
		t.commit().verify();
	}

	m.setQuery("SELECT id, name FROM cmd_delete_row", db);
	EXPECT_EQ(2, m.rowCount());

	{
		auto t = c.createTransaction("CmdDeleteRow Delete");
		t.deleteRow("cmd_delete_row", "id", delete_id).verify();
		t.commit().verify();
	}

	m.setQuery("SELECT id, name FROM cmd_delete_row", db);
	EXPECT_EQ(1, m.rowCount());
	EXPECT_STREQ("neato", m.data(m.index(0,1)).toString().toStdString().c_str());

	c.undo().verify();

	m.setQuery("SELECT id, name FROM cmd_delete_row", db);
	EXPECT_EQ(2, m.rowCount());

	EXPECT_STREQ(delete_id.toString().toStdString().c_str(),
		m.data(m.index(1,0)).toString().toStdString().c_str());

	EXPECT_STREQ("burrito", m.data(m.index(1,1)).toString().toStdString().c_str());

	c.redo().verify();

	m.setQuery("SELECT id, name FROM cmd_delete_row", db);
	EXPECT_EQ(1, m.rowCount());
	EXPECT_STREQ("neato", m.data(m.index(0,1)).toString().toStdString().c_str());
}

TEST(Controller, CmdDropTable) {

	QSqlDatabase db = sg::CreateTestDB();

	QSqlQuery q(db);
	QSqlQueryModel m;

	if (db.tables().contains("cmd_drop_table")) {
		EXPECT_TRUE(q.exec("DROP TABLE cmd_drop_table")) << q.lastError().text().toStdString().c_str();
	}

	sg::Controller c(db);

	QVector<QVariant> row_ids;

	{
		auto t = c.createTransaction("CmdDropTable setup");
		t.createTable("cmd_drop_table", {"id SERIAL PRIMARY KEY", "name VARCHAR(64) NOT NULL"}).verify();
		row_ids.push_back(*t.insert("cmd_drop_table", {{"name", "neato"}}, "id"));
		row_ids.push_back(*t.insert("cmd_drop_table", {{"name", "burrito"}}, "id"));
		t.commit().verify();
	}

	m.setQuery("SELECT name from cmd_drop_table");
	EXPECT_EQ(2, m.rowCount());

	{
		auto t = c.createTransaction("CmdDropTable");
		t.dropTable("cmd_drop_table", {"id SERIAL PRIMARY KEY", "name VARCHAR(64) NOT NULL"}).verify();
		t.commit().verify();
	}

	EXPECT_FALSE(db.tables().contains("cmd_drop_table"));

	c.undo();

	EXPECT_TRUE(db.tables().contains("cmd_drop_table"));

	m.setQuery("SELECT id, name from cmd_drop_table");
	EXPECT_EQ(2, m.rowCount());

	auto find_data = [&](QVariant key, QString name) {

		for (int n = 0; n < m.rowCount(); ++n) {

			if (m.data(m.index(n, 0)) == key &&
				m.data(m.index(n, 1)).toString() == name)
				return true;
		}

		return false;
	};

	EXPECT_TRUE(find_data(row_ids[0], "neato"));
	EXPECT_TRUE(find_data(row_ids[1], "burrito"));

	c.redo();

	EXPECT_FALSE(db.tables().contains("cmd_drop_table"));	

	// now create an empty one
	{
		auto t = c.createTransaction("CmdDropTable empty table");
		t.createTable("cmd_drop_table", {"id SERIAL PRIMARY KEY", "name VARCHAR(64) NOT NULL"}).verify();
		t.commit().verify();
	}

	EXPECT_TRUE(db.tables().contains("cmd_drop_table"));	

	{
		auto t = c.createTransaction("CmdDropTable");
		t.dropTable("cmd_drop_table", {"id SERIAL PRIMARY KEY", "name VARCHAR(64) NOT NULL"}).verify();
		t.commit().verify();
	}

	EXPECT_FALSE(db.tables().contains("cmd_drop_table"));	

	c.undo();

	EXPECT_TRUE(db.tables().contains("cmd_drop_table"));	

	c.redo();

	EXPECT_FALSE(db.tables().contains("cmd_drop_table"));				
}

TEST(Controller, CmdUpdate) {

	QSqlDatabase db = sg::CreateTestDB();

	QSqlQuery q(db);
	QSqlQueryModel m;

	if (db.tables().contains("cmd_update_table")) {
		EXPECT_TRUE(q.exec("DROP TABLE cmd_update_table")) << q.lastError().text().toStdString().c_str();
	}

	sg::Controller c(db);
	QVariant update_id;

	{
		auto t = c.createTransaction("CmdUpdate setup");
		t.createTable("cmd_update_table", {"id SERIAL PRIMARY KEY", "name VARCHAR(64) NOT NULL"}).verify();
		update_id = *t.insert("cmd_update_table", {{"name", "neato"}}, "id");
		t.commit().verify();
	}

	m.setQuery("SELECT name from cmd_update_table");
	EXPECT_EQ(1, m.rowCount());
	EXPECT_STREQ("neato", m.data(m.index(0,0)).toString().toStdString().c_str());

	{
		auto t = c.createTransaction("CmdUpdate");
		// it's fine to mess with previous values because its what we want in our update history, not necessarily 
		// the current value
		t.update("cmd_update_table", {{"name", "burrito"}}, {{"name", "neato_before"}}, "id", update_id).verify();
		t.commit().verify();
	}

	m.setQuery("SELECT name from cmd_update_table");
	EXPECT_EQ(1, m.rowCount());
	EXPECT_STREQ("burrito", m.data(m.index(0,0)).toString().toStdString().c_str());

	c.undo().verify();

	m.setQuery("SELECT name from cmd_update_table");
	EXPECT_EQ(1, m.rowCount());
	EXPECT_STREQ("neato_before", m.data(m.index(0,0)).toString().toStdString().c_str());

	c.redo().verify();

	m.setQuery("SELECT name from cmd_update_table");
	EXPECT_EQ(1, m.rowCount());
	EXPECT_STREQ("burrito", m.data(m.index(0,0)).toString().toStdString().c_str());
}

TEST(Controller, CmdRenameTable) {

	QSqlDatabase db = sg::CreateTestDB();

	QSqlQuery q(db);
	QSqlQueryModel m;

	if (db.tables().contains("cmd_rename_table_1")) {
		EXPECT_TRUE(q.exec("DROP TABLE cmd_rename_table_1")) << q.lastError().text().toStdString().c_str();
	}

	if (db.tables().contains("cmd_rename_table_2")) {
		EXPECT_TRUE(q.exec("DROP TABLE cmd_rename_table_2")) << q.lastError().text().toStdString().c_str();
	}
	
	sg::Controller c(db);

	EXPECT_FALSE(db.tables().contains("cmd_rename_table_1"));
	EXPECT_FALSE(db.tables().contains("cmd_rename_table_2"));

	{
		auto t = c.createTransaction("CmdRenameTable setup"); 
		t.createTable("cmd_rename_table_1", {"id SERIAL PRIMARY KEY"}).verify();
		t.commit().verify();
	}

	EXPECT_TRUE(db.tables().contains("cmd_rename_table_1"));

	{
		auto t = c.createTransaction("CmdRenameTable");
		t.renameTable("cmd_rename_table_1", "cmd_rename_table_2").verify();
		t.commit().verify();
	}

	EXPECT_FALSE(db.tables().contains("cmd_rename_table_1"));
	EXPECT_TRUE(db.tables().contains("cmd_rename_table_2"));

	c.undo().verify();

	EXPECT_TRUE(db.tables().contains("cmd_rename_table_1"));
	EXPECT_FALSE(db.tables().contains("cmd_rename_table_2"));

	c.redo().verify();

	EXPECT_FALSE(db.tables().contains("cmd_rename_table_1"));
	EXPECT_TRUE(db.tables().contains("cmd_rename_table_2"));
}
