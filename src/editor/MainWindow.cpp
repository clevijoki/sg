#include "MainWindow.h"

#include <QDockWidget>
#include <QSettings>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QMdiArea>
#include <QDebug>
#include <QVBoxLayout>
#include <QMdiSubWindow>
#include <QSqlQueryModel>
#include <QSqlError>
#include <QTabBar>

#include "ComponentList.h"
#include "ComponentEditor.h"
#include "EntityList.h"
#include "EntityEditor.h"
#include "Controller.h"
#include "MessageBox.h"

namespace sg {

	ResourceWindowTitleManager::ResourceWindowTitleManager(
		class Controller& controller,
		QString title_table,
		QString title_table_name_key,
		QString title_table_id,
		QVariant id_value,
		QMdiSubWindow* parent)
	: QObject(parent)
	, mStatement(QString("SELECT \"%1\" FROM \"%2\" WHERE \"%3\" = %4").arg(title_table_name_key).arg(title_table).arg(title_table_id).arg(ToSqlLiteral(id_value))) 
	, mTitleTable(std::move(title_table))
	, mMdiSubWindow(parent)
	, mController(controller)
	{
		updateTitle();
		connect(&controller, &Controller::dataChanged, this, &ResourceWindowTitleManager::onDataChanged);
	}

	void ResourceWindowTitleManager::updateTitle() {
		QSqlQueryModel m;

		m.setQuery(mStatement, *mController.createTransaction("Title Query").connection());
		if (m.lastError().isValid()) {
			qCritical() << m.lastError().text() << "\n" << mStatement;
		}

		if (m.rowCount() == 1) {
			mMdiSubWindow->setWindowTitle(QString("%1:%2").arg(mTitleTable).arg(m.data(m.index(0,0)).toString()));
		} else {
			mMdiSubWindow->setWindowTitle(QString("%1:unknown").arg(mTitleTable));
		}
	}

	void ResourceWindowTitleManager::onDataChanged(const QSet<QString>& tables_affected) {
		if (tables_affected.contains(mTitleTable)) {
			updateTitle();
		}
	}

	MainWindow::MainWindow(Controller& controller)
	: mController(controller)
	, mComponentList(new ComponentList(controller, this))
	, mEntityList(new EntityList(controller, this))
	, mMdiArea(new QMdiArea(this))
	{
		setWindowTitle(tr("SG Edit"));

		setCentralWidget(mMdiArea);
		mMdiArea->setViewMode(QMdiArea::TabbedView);
		mMdiArea->setTabsClosable(true);
		mMdiArea->setTabsMovable(true);
		mMdiArea->setDocumentMode(true);
		mMdiArea->setObjectName("MdiArea");

		for (QTabBar *tb : mMdiArea->findChildren<QTabBar*>()) {
			tb->setExpanding(false);
		}

		auto edit_menu = menuBar()->addMenu(tr("&Edit"));
		auto view_menu = menuBar()->addMenu(tr("&View"));

		auto new_dock_widget = [&](const char *title, Qt::DockWidgetArea area, QWidget *widget) {
			auto dw = new QDockWidget(title);
			dw->setObjectName(QString("%1:DockFrame").arg(widget->objectName()));
			dw->setWidget(widget);
			this->addDockWidget(area, dw);
			view_menu->addAction(dw->toggleViewAction());
		};

		new_dock_widget(
			"Component",
			Qt::LeftDockWidgetArea,
			mComponentList
		);

		connect(mComponentList, &ComponentList::opened, [&](const QVariant& component_id){
			this->openResource<ComponentEditor>("component", "name", "id", component_id);
		});

		connect(mEntityList, &EntityList::opened, [&](const QVariant& component_id){
			this->openResource<EntityEditor>("entity", "name", "id", component_id);
		});

		new_dock_widget(
			"Entity",
			Qt::LeftDockWidgetArea,
			mEntityList
		);

		auto undo_act = new QAction(tr("&Undo"), this);
		undo_act->setShortcut(QKeySequence::Undo);
		edit_menu->addAction(undo_act);
		connect(undo_act, &QAction::triggered, [&controller](bool){
			auto res = controller.undo();
			if (res.failed()) {
				MessageBoxCritical(tr("Error undoing"), res.errorMessage(), res.errorInfo());
			}
		});

		auto redo_act = new QAction(tr("&Redo"), this);
		redo_act->setShortcut(QKeySequence::Redo);
		edit_menu->addAction(redo_act);
		connect(redo_act, &QAction::triggered, [&controller](bool){
			auto res = controller.redo();
			if (res.failed()) {
				MessageBoxCritical(tr("Error redoing"), res.errorMessage(), res.errorInfo());
			}
		});

		loadSettings();
	}

	void MainWindow::saveSettings() const {
		QSettings settings("SG", "Editor");

		settings.beginGroup("MainWindow");
		settings.setValue("geometry", saveGeometry());
		settings.setValue("state", saveState());
	}

	void MainWindow::loadSettings() {
		QSettings settings("SG", "Editor");

		settings.beginGroup("MainWindow");
		restoreGeometry(settings.value("geometry").toByteArray());
		restoreState(settings.value("state").toByteArray());
	}

	void MainWindow::closeEvent(QCloseEvent *event) {
		saveSettings();
	}

	template <typename T>	
	void MainWindow::openResource(QString title_table, QString title_table_name_key, QString title_table_id, QVariant id_value) {

		QString prop_name = QString("%1:%2").arg(title_table).arg(id_value.toString());

		for (QMdiSubWindow* msw : mMdiArea->subWindowList()) {
			if (msw->property("resource_id") == prop_name && msw->widget()->metaObject() == &T::staticMetaObject) {
				mMdiArea->setActiveSubWindow(msw);
				return;
			}
		}

		T* sw = new T(mController, id_value, this);
		auto msw = mMdiArea->addSubWindow(sw);
		msw->setProperty("resource_id", prop_name);
		// this should delete with the parent
		new ResourceWindowTitleManager(mController, title_table, title_table_name_key, title_table_id, id_value, msw);
		sw->show();
		msw->setAttribute(Qt::WA_DeleteOnClose);

		auto undock_action = new QAction("Float", this);
		msw->systemMenu()->addAction(undock_action);
		connect(undock_action, &QAction::triggered, [sw, msw](bool){
			auto frame = new QFrame();
			auto new_mdi_area = new QMdiArea(frame);

			auto frame_layout = new QVBoxLayout(frame);
			frame_layout->addWidget(new_mdi_area);
			frame->setLayout(frame_layout);
			frame->show();

			new_mdi_area->addSubWindow(sw);
			sw->show();
		});
	}
}
