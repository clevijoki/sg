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
#include "InitialSetup.h"



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

	{
		auto setup_res = PerformInitialSetup(controller);
		if (setup_res.failed()) {
			MessageBoxCritical(MainWindow::tr("Unable to perform initial setup"), setup_res.errorMessage(), setup_res.errorInfo());
			return -1;
		} 
	}

	if (parser.isSet(codegen_header) || parser.isSet(codegen_cpp)) {
		auto gen_res = sg::GenerateComponentFiles(
			controller.createTransaction("Generate Headers"),
			parser.value(codegen_header),
			parser.value(codegen_cpp)
		);

		if (gen_res.failed()) {
			qCritical() << gen_res.errorMessage();
			if (!gen_res.errorInfo().isEmpty()) {
				qCritical() << gen_res.errorInfo();
			}

			return -1;
		}

		return 0;
	}

	MainWindow main_window(controller);
	main_window.show();

	return app.exec();
}

