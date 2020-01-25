#include "ComponentMetaEditor.h"
#include "Controller.h"
#include "MessageBox.h"

#include <QListView>
#include <QSqlQueryModel>
#include <QSqlQuery>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QAction>
#include <QSortFilterProxyModel>
#include <QDebug>

namespace sg {

	class ComponentMetaModel : public QSqlQueryModel {
		Controller& mController;
	public:

		static const int NAME_COL = 0;
		static const int ID_COL = 1;

		ComponentMetaModel(Controller& controller, QObject* parent)
		: QSqlQueryModel(parent)
		, mController(controller)
		{}

		void refresh() {
			static const QString select_query = QString("SELECT name, id FROM component");
			setQuery(select_query);
		}

		Qt::ItemFlags flags(const QModelIndex& index) const override {
			return QSqlQueryModel::flags(index) | Qt::ItemIsEditable;
		};

		bool containsName(const QString& name) {

			// try and find a table that doesn't exist
			for (int row = 0; row < rowCount(); ++row) {

				if (data(index(row, 0)).toString() == name) {
					return true;
				}
			}

			return false;
		};

		bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) {
			if (role != Qt::EditRole) {
				return QSqlQueryModel::setData(index, value, role);
			}

			auto perform = [&]() -> Result<> {

				QString current_name = data(index).toString();
				QString new_name = value.toString();

				if (current_name == new_name)
					return Ok(); // ignore rename

				if (containsName(new_name)) {
					return Error(ComponentMetaEditor::tr("Component named '%1' already exists").arg(new_name));
				}

				// want to rename the table and model table
				auto t = mController.createTransaction("Rename Component");

				{
					auto res = t.update(
						"component", 
						{{"name", new_name}},
						{{"name", current_name}},
						"id",
						this->data(this->index(index.row(), ID_COL, index.parent()))
					);

					if (res.failed())
						return res.error();
				}
				return t.commit();
			};

			auto res = perform();
			if (res.failed()) {
				MessageBoxCritical(ComponentMetaEditor::tr("Unable to rename component"), res.errorMessage(), res.errorInfo());
				return false;
			}

			return true;
		}
	};

	ComponentMetaEditor::ComponentMetaEditor(Controller& controller, QWidget* parent) 
	: QWidget(parent) {

		auto layout = new QVBoxLayout(this);
		auto input_layout = new QHBoxLayout();

		layout->addLayout(input_layout);

		auto filter = new QLineEdit(this);
		filter->setPlaceholderText(tr("Search filter..."));
		input_layout->addWidget(filter);

		auto list_view = new QListView(this);
		list_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
		list_view->setEditTriggers(QAbstractItemView::EditKeyPressed);
		layout->addWidget(list_view);

		auto model = new ComponentMetaModel(controller, this);
		auto proxy_model = new QSortFilterProxyModel(this);
		proxy_model->setSourceModel(model);
		proxy_model->setDynamicSortFilter(true);
		proxy_model->sort(ComponentMetaModel::NAME_COL);
		proxy_model->setFilterKeyColumn(ComponentMetaModel::NAME_COL);
		proxy_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
		list_view->setModel(proxy_model);
		model->refresh();
		// list_view->setColumnHidden(ComponentMetaModel::ID_COL);

		connect(filter, &QLineEdit::textChanged, this, [proxy_model](const QString& value){
			proxy_model->setFilterFixedString(value);
		});

		auto save_selection = [list_view, proxy_model]() {

			QStringList selected_ids;

			for (const auto idx : list_view->selectionModel()->selectedIndexes()) {
				selected_ids.push_back(proxy_model->data(proxy_model->index(idx.row(), ComponentMetaModel::ID_COL)).toString());
			}

			list_view->setProperty("save_selection", selected_ids.join(":"));
		};

		auto restore_selection = [list_view, proxy_model]() {

			QStringList selected_ids = list_view->property("save_selection").toString().split(":");

			list_view->clearSelection();

			for (int row = 0; row < proxy_model->rowCount(); ++row) {
				QModelIndex idx = proxy_model->index(row, ComponentMetaModel::ID_COL);
				if (selected_ids.contains(proxy_model->data(idx).toString())) {
					list_view->setCurrentIndex(proxy_model->index(row, ComponentMetaModel::NAME_COL));
					return;
				}
			}
		};

		connect(model, &QAbstractItemModel::modelAboutToBeReset, this, save_selection);
		connect(model, &QAbstractItemModel::modelReset, this, restore_selection);

		connect(&controller, &Controller::dataChanged, this, [model](const QSet<QString>& tables){
			if (tables.contains("component")) {
				model->refresh();
			}
		});

		auto new_button = new QPushButton(tr("New"), this);
		connect(new_button, &QPushButton::clicked, this, [list_view, model, &controller](){

			auto perform = [list_view, model, &controller]() -> Result<> {

				auto t = controller.createTransaction("New Component");
				auto lock_res = t.lockTable("component");
				if (lock_res.failed())
					return lock_res.error();

				QString table_name;

				{
					int index = 0;
					while(1) {

						table_name = QString("New Component %1").arg(index);

						if (model->containsName(table_name)) {
							++index;
							continue;
						} else {
							break;
						}
					}
				}

				auto create_node_res = t.insert(
					"component",
					{
						{ "name", table_name }
					},
					"name"
				);

				if (create_node_res.failed()) {
					return create_node_res.error();
				}

				return t.commit();
			};

			auto res = perform();
			if (res.failed()) {
				MessageBoxCritical(ComponentMetaEditor::tr("Error creating component"), res.errorMessage(), res.errorInfo());
			}
		});

		input_layout->addWidget(new_button);

		setLayout(layout);

		auto edit_row = new QAction(tr("Edit"), this);
		addAction(edit_row);
		edit_row->setShortcut(Qt::Key_Enter);
		edit_row->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(edit_row, &QAction::triggered, this, [this, list_view, proxy_model](bool){
			for (auto index : list_view->selectionModel()->selectedIndexes()) {
				emit this->opened(proxy_model->data(index).toString());
			}
		});
		connect(list_view, &QListView::doubleClicked, this, [this, proxy_model, list_view](const QModelIndex& index){
			list_view->selectionModel()->clearSelection();

			emit this->opened(proxy_model->data(proxy_model->index(index.row(), ComponentMetaModel::ID_COL)).toString());

		});

		auto rename_row = new QAction(tr("Rename"), this);
		addAction(rename_row);
		rename_row->setShortcut(Qt::Key_F2);
		rename_row->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(rename_row, &QAction::triggered, this, [list_view, proxy_model](bool){

			// if (list_view->selectionModel()->selectedIndexes().isEmpty())
			// 	return;

			qDebug() << list_view->currentIndex();

			list_view->edit(list_view->currentIndex());
		});

		auto delete_row = new QAction(tr("Delete"), this);
		addAction(delete_row);
		delete_row->setShortcut(Qt::Key_Delete);
		delete_row->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(delete_row, &QAction::triggered, this, [list_view, model, proxy_model, &controller](bool) {

			auto perform = [list_view, proxy_model, &controller]() -> Result<> {

				if (list_view->selectionModel()->selectedIndexes().isEmpty())
					return Ok();

				auto t = controller.createTransaction("Delete Component");

				QString where;
				for (auto index : list_view->selectionModel()->selectedIndexes()) {
					QVariant component_id = proxy_model->data(proxy_model->index(index.row(), ComponentMetaModel::ID_COL));
					auto res = t.deleteRow("component", "id", component_id);
					if (res.failed())
						return res.error();
				}

				return t.commit();
			};

			auto res = perform();
			if (res.failed()) {
				MessageBoxCritical(tr("Error deleting component(s)"), res.errorMessage(), res.errorInfo());
			}
		});

		setContextMenuPolicy(Qt::ActionsContextMenu);
	}

	ComponentMetaEditor::~ComponentMetaEditor() {
	}
}