#pragma once

#include <QMainWindow>

class QMdiSubWindow;
class QMdiArea;

class QCloseEvent;
namespace sg {

	class ResourceWindowTitleManager : public QObject {
		Q_OBJECT

		QString mStatement;
		QString mTitleTable;
		QMdiSubWindow *mMdiSubWindow;
		class Controller& mController;

		void updateTitle();

	public:
		ResourceWindowTitleManager(
			class Controller& controller,
			QString title_table,
			QString title_table_name_key,
			QString title_table_id,
			QVariant id_value,
			QMdiSubWindow *parent
		);

	private slots:
		void onDataChanged(const QSet<QString>& tables);
	};
	
	class MainWindow : public QMainWindow {
		Q_OBJECT

		class ComponentList *mComponentList;
		class EntityList *mEntityList;

		class Controller& mController;

		QHash<QString, QMdiSubWindow*> mOpenedResources; 
		QMdiArea* mMdiArea;

		template<typename T> void openResource(QString title_table, QString title_table_name_key, QString title_table_id, QVariant id_value);

	public:
		MainWindow(Controller& controller);

		void saveSettings() const;
		void loadSettings();

		void closeEvent(QCloseEvent* event) override;
	};
}
