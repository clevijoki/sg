#include "InitialSetup.h"
#include "Controller.h"
#include "MainWindow.h"
#include "MessageBox.h"
#include <QSqlQueryModel>
#include <QSqlQuery>
#include <QSqlError>
#include <iterator>

namespace sg {

	struct RequiredTable {
		QString name;
		QStringList columns;
	};

	static const char* DEFAULT_TYPES[] = {"i8","u8","i16","u16","i32","u32","f32","f64","text","component", "point2d", "point3d", "scale2d", "scale3d", "rotation2d", "rotation3d"};

	const RequiredTable REQURIED_TABLES[] = {
		{
			"sg_properties",
			{
				"name VARCHAR(32) PRIMARY KEY NOT NULL",
				"value TEXT"
			}
		}, {
			"prop_type",
			{
				"name VARCHAR(32) PRIMARY KEY NOT NULL"
			},
		}, {
			"prop_editor",
			{
				"name VARCHAR(32) NOT NULL",
				"type VARCHAR(32) NOT NULL REFERENCES prop_type(name)",
				"UNIQUE(name, type)"
			}
		}, {
			"component",
			{
				"id SERIAL PRIMARY KEY",
				"name VARCHAR(64) NOT NULL"
			}
		}, {
			"component_prop",
			{
				"id SERIAL PRIMARY KEY",
				"component_id INTEGER REFERENCES component(id)",
				"name VARCHAR(128) NOT NULL CONSTRAINT name_cannot_contain_whitespace CHECK (strpos(name, ' ') = 0)",
				"type VARCHAR(32) NOT NULL REFERENCES prop_type(name)",
				"default_value TEXT",
				"UNIQUE(component_id, name)",
			}
		}, {
			"entity",
			{
				"id SERIAL PRIMARY KEY",
				"name VARCHAR(64) NOT NULL"
			}
		}, {
			"entity_component",
			{
				"id SERIAL PRIMARY KEY",
				"name VARCHAR(64) NOT NULL",
				"entity_id INTEGER REFERENCES entity(id)",
				"component_id INTEGER REFERENCES component(id)",
				"graph_pos POINT",
			}
		}, {
			"entity_component_override",
			{
				"id SERIAL PRIMARY KEY",
				"entity_component_id INTEGER REFERENCES entity_component(id)",
				"component_prop_id INTEGER REFERENCES component_prop(id)",
				"value TEXT",
			}
		}, {
			"entity_child",
			{
				"id SERIAL PRIMARY KEY",
				"name VARCHAR(64) NOT NULL",
				"entity_id INTEGER REFERENCES entity(id)",
				"child_id INTEGER REFERENCES entity(id)",
				"graph_pos POINT",
			}
		}, {
			"entity_child_override",
			{
				"id SERIAL PRIMARY KEY",
				"entity_child_id INTEGER REFERENCES entity_child(id)",
				"component_prop_id INTEGER REFERENCES component_prop(id)",
				"value TEXT",
			}
		}, {
			"entity_prop",
			{
				"id SERIAL PRIMARY KEY",
				"entity_id INTEGER REFERENCES entity(id)",
				"name VARCHAR(64) NOT NULL",
				"type VARCHAR(32) NOT NULL",
				"default_value TEXT",
				"UNIQUE(name, entity_id)",
			}
		}, {
			"entity_prop_link_ops",
			{
				"name VARCHAR(32) PRIMARY KEY"
			}
		}, {
			"entity_prop_link",
			{
				"id SERIAL PRIMARY KEY",
				"entity_prop_id INTEGER REFERENCES entity(id)",
				"entity_component_id INTEGER REFERENCES entity_component(id)",
				"component_prop_id INTEGER REFERENCES component_prop(id)",
				"operation VARCHAR(32) NOT NULL REFERENCES entity_prop_link_ops(name)", 
				"UNIQUE(entity_prop_id, entity_component_id, component_prop_id)"
			}
		}
	};

	Result<> PerformInitialSetup(Controller& controller) {

		Transaction t = controller.createTransaction("Initial Setup");

		// check to see if we have our required tables, and if not, prompt to create them
		const auto existing_tables = t.connection()->tables();

		bool version_mismatch = false;
		bool initial_setup_required = false;
		uint required_hash = 0;

		for (const RequiredTable& required_table : REQURIED_TABLES) {
			if (!existing_tables.contains(required_table.name)) {
				initial_setup_required = true;
			}

			required_hash = qHash(required_table.name, required_hash);
			for (const QString& s : required_table.columns) {
				required_hash = qHash(s, required_hash);
			}
		}

		if (!initial_setup_required) {

			// check that thhe 
			QSqlQueryModel m;
			m.setQuery("SELECT value FROM sg_properties WHERE name = 'table_hash'", *t.connection());

			if (m.columnCount() != 1 || m.rowCount() != 1) {
				version_mismatch = true;
			} else {
				const uint db_hash = m.data(m.index(0,0)).toUInt();
				version_mismatch = db_hash != required_hash;
			}			
		}

		if (version_mismatch || initial_setup_required) {

			if (MessageBoxQuestion(
				MainWindow::tr("Perform table setup?"),
				version_mismatch ? MainWindow::tr("Table format has changed, data migration is necessary") : MainWindow::tr("Initial setup required")
			) == QMessageBox::No) {
				return Error("Setup cancelled");
			}

			QMap<QString, QSqlQueryModel*> existing_values;

			// remove all old tables in reverse
			for (auto itr = std::rbegin(REQURIED_TABLES); itr != std::rend(REQURIED_TABLES); ++itr) {

				// also capture id sequence
				QString id_seq = QString("%1_id_seq").arg(itr->name);
				if (existing_tables.contains(id_seq)) {

					QSqlQueryModel *existing_table_values = new QSqlQueryModel();
					existing_values[id_seq] = existing_table_values;
					existing_table_values->setQuery(QString("SELECT * FROM \"%1\"").arg(id_seq), *t.connection());
				}

				if (existing_tables.contains(itr->name)) {

					QSqlQueryModel *existing_table_values = new QSqlQueryModel();
					existing_values[itr->name] = existing_table_values;
					existing_table_values->setQuery(QString("SELECT * FROM \"%1\"").arg(itr->name), *t.connection());

					auto drop_res = t.dropTable(itr->name, itr->columns);
					if (drop_res.failed()) 
						return drop_res.error();
				}
			}

			for (const RequiredTable& rt : REQURIED_TABLES) {

				auto res = t.createTable(rt.name, rt.columns);
				if (res.failed())
					return res.error();

				QString primary_key;
				for (const QString& col : rt.columns) {
					if (col.contains("PRIMARY KEY", Qt::CaseInsensitive)) {
						primary_key = col.split(" ")[0];
						break;
					}
				}

				if (primary_key.isEmpty())
					continue;

				QSqlQueryModel *existing_table_values = existing_values[rt.name];
				if (!existing_table_values)
					continue;

				for (int row = 0; row < existing_table_values->rowCount(); ++row) {
					QMap<QString, QVariant> values;

					for (int col = 0; col < existing_table_values->columnCount(); ++col) {
						values[existing_table_values->headerData(col, Qt::Horizontal).toString()] = existing_table_values->data(existing_table_values->index(row, col));
					}

					auto insert_res = t.insert(
						rt.name,
						values,
						primary_key
					);

					if (insert_res.failed())
						return insert_res.error();
				}

				// restore id_seq
				QString id_seq = QString("%1_id_seq").arg(rt.name);
				QSqlQueryModel *existing_id_seq_values = existing_values[id_seq];
				if (!existing_id_seq_values || existing_id_seq_values->rowCount() != 1)
					continue;

				// update the id_seq
				{
					QMap<QString, QVariant> values;

					for (int col = 0; col < existing_id_seq_values->columnCount(); ++col) {
						values[existing_id_seq_values->headerData(col, Qt::Horizontal).toString()] = existing_id_seq_values->data(existing_id_seq_values->index(0, col));
					}

					QVariant old_value = values["last_value"];
					QString statement = QString("ALTER SEQUENCE %1 RESTART WITH %2").arg(id_seq).arg(ToSqlLiteral(old_value));
					QSqlQuery q(statement, *t.connection());

					if (q.lastError().isValid()) {
						return Error(q.lastError().text(), statement);
					}
				}
			}

			for (QSqlQueryModel* m : existing_values) {
				delete m;
			}
		}

		{
			QSqlQueryModel m;
			m.setQuery("SELECT name FROM prop_type", *t.connection());

			auto m_contains = [&](const char* name) {
				for (int row = 0; row < m.rowCount(); ++row) {
					if (m.data(m.index(row, 0)).toString() == name)
						return true;
				}

				return false;
			};

			for (const char* name : DEFAULT_TYPES) {
				if (m_contains(name))
					continue;

				auto insert_res = t.insert("prop_type", {{"name", name}}, "name");
				if (insert_res.failed())
					return insert_res.error();
			}
		}

		{
			QSqlQueryModel m;
			m.setQuery("SELECT name, type FROM prop_editor", *t.connection());

			auto m_contains = [&](const char *name, const char *type) {
				for (int row = 0; row < m.rowCount(); ++row) {
					if (m.data(m.index(row, 0)).toString() == name &&
						m.data(m.index(row, 1)).toString() == type) {
						return true;
					}
				}

				return false;
			};

			for (const char* type : DEFAULT_TYPES) {
				if (m_contains("Text", type))
					continue;

				auto insert_res = t.insert(
					"prop_editor",
					{
						{"name", "Text"},
						{"type", type}
					},
					"name"
				);

				if (insert_res.failed())
					return insert_res.error();
			}
		}

		{
			QSqlQueryModel m;
			m.setQuery("SELECT name FROM entity_prop_link_ops", *t.connection());

			auto m_contains = [&](const char* name) {
				for (int row = 0; row < m.rowCount(); ++row) {
					if (m.data(m.index(row, 0)).toString() == name)
						return true;
				}

				return false;
			};

			for (const char *op : {"copy", "add", "subtract", "preset"}) {

				if (m_contains(op))
					continue;

				auto insert_res = t.insert(
					"entity_prop_link_ops",
					{
						{"name", op},
					},
					"name"
				);
			}

		}

		{
			QSqlQueryModel m;
			m.setQuery("SELECT name, value FROM sg_properties", *t.connection());

			auto m_update = [&](const char* name, QVariant value) -> Result<> {
				for (int row = 0; row < m.rowCount(); ++row) {
					if (m.data(m.index(row, 0)).toString() == name) {
						QVariant old_value = m.data(m.index(row, 1));
						if (old_value != value) {

							return t.update("sg_properties", {{"value", value}}, {{"value", old_value}}, "name", name);
						} else {
							return Ok(); // no change necessary
						}
					}
				}

				auto insert_res = t.insert(
					"sg_properties",
					{
						{"name", name},
						{"value", value}
					},
					"name"
				);

				if (insert_res.failed())
					return insert_res.error();

				return Ok();
			};

			auto res = m_update("table_hash", QVariant(required_hash));
			if (res.failed())
				return res.error();
		}

		if (t.hasCommands())
			return t.commit();

		return Ok();
	}
}