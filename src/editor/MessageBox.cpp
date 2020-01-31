#include "MessageBox.h"
#include <QMessageBox>
#include <QDebug>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QLabel>
#include <QFont>
#include <QPushButton>

namespace sg {

	int gMessageBoxSilenceScope = 0;
	bool gIsTesting = false;
	bool gTestingHadCriticalError = false;

	SilenceMessageBoxScope::SilenceMessageBoxScope() {
		gMessageBoxSilenceScope++;
	}

	SilenceMessageBoxScope::~SilenceMessageBoxScope() {
		gMessageBoxSilenceScope--;
	}

	MessageBox::MessageBox(QString title, QString message, QString info, QWidget *parent)
	: QDialog(parent) {
		setWindowTitle(std::move(title));

		auto layout = new QVBoxLayout(this);
		auto message_label = new QLabel(std::move(message), this);
		message_label->setFont(QFont("Courier New", 12, QFont::Bold));

		layout->addWidget(message_label);

		if (!info.isEmpty()) {
			auto info_box = new QGroupBox(this);
			auto info_layout = new QVBoxLayout(info_box);
			auto info_edit = new QPlainTextEdit(std::move(info), info_box);
			info_edit->setFont(QFont("Courier New", 12, QFont::Bold));
			info_edit->setReadOnly(true);
			info_layout->addWidget(info_edit);
			info_box->setLayout(info_layout);
			layout->addWidget(info_box);
		}

		auto ok_button = new QPushButton(tr("Ok"), this);
		connect(ok_button, &QPushButton::clicked, [this](){
			this->done(QDialog::Accepted);
		});

		ok_button->setDefault(true);
		layout->addWidget(ok_button, 0, Qt::AlignHCenter);
		setLayout(layout);
	}

	void MessageBoxCritical(StringBuilder title, StringBuilder message, StringBuilder info) {
		if (gMessageBoxSilenceScope == 0) {
			printf("%s: %s %s\n", title.c_str(), message.c_str(), info.c_str());
			MessageBox mb(title.c_str(), message.c_str(), info.c_str());
			mb.exec();

		} else {
			if (gIsTesting) {
				gTestingHadCriticalError = true;
			} else {
				qFatal("%s: %s", title.c_str(), message.c_str());
			}
		}
	}

	QMessageBox::StandardButton MessageBoxQuestion(StringBuilder title, StringBuilder message, QMessageBox::StandardButton default_button, QMessageBox::StandardButtons buttons) {
		if (gMessageBoxSilenceScope == 0) {
			return QMessageBox::question(nullptr, title.c_str(), message.c_str(), buttons, default_button);
		} else {
			QDebug(QtInfoMsg) << message.c_str() << "... selecting" << default_button;
			return default_button;
		}
	}
}

#include <gtest/gtest.h>

TEST(MessageBox, MessageBoxCritical) {

	using namespace sg;

	gIsTesting = true;

	MessageBoxCritical("Title", "Message");
	EXPECT_TRUE(gTestingHadCriticalError);

	gIsTesting = false;
}

TEST(MessageBox, MessageBoxQuestion) {

	using namespace sg;

	EXPECT_EQ(QMessageBox::No, MessageBoxQuestion("Title", "Select QMessageBox::No"));
	EXPECT_EQ(QMessageBox::Yes, MessageBoxQuestion("Title", "Select QMessageBox::Yes", QMessageBox::Yes));
}
