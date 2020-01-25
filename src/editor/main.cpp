#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>

#include <gtest/gtest.h>

#include "MainWindow.h"
#include "MessageBox.h"
#include "Controller.h"
#include "Connection.h"
#include "CodeGenerator.h"


namespace sg {
	class MessageBoxDisabler : public ::testing::EmptyTestEventListener {

		union { SilenceMessageBoxScope mScope; };
		bool mIsScopeValid = false;

		void OnTestStart(const ::testing::TestInfo&) override {

			new (&mScope) SilenceMessageBoxScope();
			mIsScopeValid = true;
		}

		void OnTestEnd(const ::testing::TestInfo&) override {
			ASSERT_TRUE(mIsScopeValid);
			mScope.~SilenceMessageBoxScope();
			mIsScopeValid = false;
		}

	public:
		MessageBoxDisabler() {}
		~MessageBoxDisabler() {
			if (mIsScopeValid) {
				mScope.~SilenceMessageBoxScope();
				mIsScopeValid = false;
			}

		}
	};
}

int main(int argc, char** argv)
{
	using namespace sg;

	QCoreApplication::setOrganizationName("SG");
	QCoreApplication::setOrganizationName("SG Editor");
	QCoreApplication::setApplicationVersion("1");

	QCommandLineParser parser;
	parser.setApplicationDescription("SG Editor");
	parser.addHelpOption();
	parser.addVersionOption();

	QCommandLineOption test("test", "Run unit tests (google test argments are valid)");
	parser.addOption(test);

	QCommandLineOption codegen_header("codegen_header", "Output C++ header (specify path)", "output.h");
	QCommandLineOption codegen_cpp("codegen_cpp", "Output C++ header (specify path)", "output.cpp");

	parser.addOption(codegen_header);
	parser.addOption(codegen_cpp);


	QApplication app(argc, argv);

	parser.process(app);

	if (parser.isSet(test)) {
		::testing::InitGoogleTest(&argc, argv);

		::testing::UnitTest::GetInstance()->listeners().Append(
			new MessageBoxDisabler
		);

		return RUN_ALL_TESTS();
	}

	auto res = CreateConnection();
	if (res.failed()) {
		MessageBoxCritical(MainWindow::tr("Unable to start SGEdit"), res.errorMessage(), res.errorInfo());
		return -1;
	}

	Controller controller(*res);

	typedef Result<> (*create_fn)(Transaction&);

	QMap<QString, create_fn> required_tables;

	required_tables["prop_editor"] = [](Transaction &t) -> Result<> {
		auto res = t.createTable("prop_editor", {"name VARCHAR(32) PRIMARY KEY NOT NULL, type VARCHAR(32)"});
		if (res.failed())
			return res.error();

		auto res2 = t.insert("prop_editor", {{"name", "Default"}, {"type", "int"}}, "name");
		if (res2.failed())
			return res2.error();

		return Ok();
	};

	required_tables["component"] = [](Transaction &t) -> Result<> {
		return t.createTable("component", {"id SERIAL PRIMARY KEY", "name VARCHAR(64) NOT NULL"});
	};

	required_tables["component_prop"] = [](Transaction &t) -> Result<> {
		return t.createTable("component_prop", {
			"id SERIAL PRIMARY KEY",
			"component_id INTEGER REFERENCES component(id)",
			"name VARCHAR(128) NOT NULL CONSTRAINT name_cannot_contain_whitespace CHECK (strpos(name, ' ') = 0)",
			"type VARCHAR(32) NOT NULL",
			"default_value TEXT",
			"UNIQUE(component_id, name)",
		});
	};

	required_tables["entity"] = [](Transaction &t) -> Result<> {
		return t.createTable(
			"entity",
			{
				"id SERIAL PRIMARY KEY",
				"name VARCHAR(64) NOT NULL"
			}
		);
	};

	required_tables["entity_component"] = [](Transaction &t) -> Result<> {
		return t.createTable(
			"entity_component",
			{
				"id SERIAL PRIMARY KEY",
				"name VARCHAR(64) NOT NULL",
				"component_id INTEGER REFERENCES component(id)",
			}
		);
	};

	required_tables["entity_component_prop"] = [](Transaction &t) -> Result<> {
		return t.createTable(
			"entity_component_prop",
			{
				"id SERIAL PRIMARY KEY",
				"entity_component_id INTEGER REFERENCES entity_component(id)",
				"component_prop_id INTEGER REFERENCES component_prop(id)",
				"value TEXT",
			}
		);
	};

	required_tables["entity_prop"] = [](Transaction &t) -> Result<> {

		return t.createTable(
			"entity_prop",
			{
				"id SERIAL PRIMARY KEY",
				"entity_id INTEGER REFERENCES entity(id)",
				"name VARCHAR(64) NOT NULL",
				"type VARCHAR(32) NOT NULL",
				"default_value TEXT",
				"UNIQUE(name, entity_id)",
			}
		);
	};

	required_tables["entity_prop_link_ops"] = [](Transaction& t) -> Result<> {
		auto res = t.createTable(
			"entity_prop_link_ops",
			{
				"name VARCHAR(32) PRIMARY KEY"
			}
		);

		if (res.failed())
			return res.error();

		auto insert_res = t.insert(
			"entity_prop_link_ops",
			{
				{"name", "set"},
				{"name", "add"},
				{"name", "subtract"},
				{"name", "preset"},
			},
			"name"
		);

		if (insert_res.failed())
			return insert_res.error();

		return Ok();
	};

	// this sets up entity "scripting" where specific entity interface values
	// do specific actions to the target values
	required_tables["entity_prop_link"] = [](Transaction &t) -> Result<> {
		return t.createTable(
			"entity_prop_link",
			{
				"id SERIAL PRIMARY KEY",
				"entity_prop_id INTEGER REFERENCES entity(id)",
				"entity_component_id INTEGER REFERENCES entity_component(id)",
				"component_prop_id INTEGER REFERENCES component_prop(id)",
				"operation VARCHAR(32) NOT NULL REFERENCES entity_prop_link_ops(name)", 
				"UNIQUE(entity_prop_id, entity_component_id, component_prop_id)"
			}
		);
	};

	// check to see if we have our required tables, and if not, prompt to create them
	const auto tables = res->tables();

	bool needs_tables = false;
	QStringList missing_tables;

	for (const QString& table : tables) {

		if (!required_tables.contains(table)) {
			missing_tables.push_back(table);
		}
	}

	if (!missing_tables.isEmpty()) {

		if (MessageBoxQuestion(MainWindow::tr("Perform initial setup?"), MainWindow::tr("Missing tables '%1'").arg(missing_tables.join(", "))) == QMessageBox::No) {
			return -1;
		}

		auto t = controller.createTransaction(MainWindow::tr("Initial setup"));

		for (const QString& table : missing_tables) {
			required_tables[table](t);
		}

		auto res = t.commit();
		if (res.failed()) {
			MessageBoxCritical(MainWindow::tr("Unable to perform initial setup"), res.errorMessage(), res.errorInfo());
			return -1;
		}
	}

	if (parser.isSet(codegen_header) || parser.isSet(codegen_cpp)) {
		auto res = sg::GenerateComponentFiles(
			controller.createTransaction("Generate Headers"),
			parser.value(codegen_header),
			parser.value(codegen_cpp)
		);

		if (res.failed()) {
			qCritical() << res.errorMessage();
			if (!res.errorInfo().isEmpty()) {
				qCritical() << res.errorInfo();
			}

			return -1;
		}

		return 0;
	}

	MainWindow main_window(controller);
	main_window.show();

	return app.exec();
}
