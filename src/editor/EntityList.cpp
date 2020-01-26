#include "EntityList.h"
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

	class EntityModel : public QSqlQueryModel {
		Controller& mController;
	public:

		static const int NAME_COL = 0;
		static const int ID_COL = 1;

		EntityModel(Controller& controller, QObject* parent)
		: QSqlQueryModel(parent)
		, mController(controller)
		{}

		void refresh() {
			static const QString select_query = QString("SELECT name, id FROM entity");
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
					return Error(EntityList::tr("Entity named '%1' already exists").arg(new_name));
				}

				// want to rename the table and model table
				auto t = mController.createTransaction("Rename Entity");

				{
					auto res = t.update(
						"entity", 
						{{"name", new_name}},
						{{"name", current_name}},
						"id",
						data(this->index(index.row(), ID_COL))
					);

					if (res.failed())
						return res.error();
				}
				return t.commit();
			};

			auto res = perform();
			if (res.failed()) {
				MessageBoxCritical(EntityList::tr("Unable to rename entity"), res.errorMessage(), res.errorInfo());
				return false;
			}

			return true;
		}
	};

	EntityList::EntityList(Controller& controller, QWidget* parent) 
	: QWidget(parent) {

		auto layout = new QVBoxLayout(this);
		auto input_layout = new QHBoxLayout();

		layout->addLayout(input_layout);

		auto filter = new QLineEdit(this);
		filter->setPlaceholderText(tr("Search filter..."));
		input_layout->addWidget(filter);

		auto view = new QListView(this);
		view->setSelectionMode(QAbstractItemView::ExtendedSelection);
		view->setEditTriggers(QAbstractItemView::EditKeyPressed);
		layout->addWidget(view);

		auto model = new EntityModel(controller, this);
		auto proxy_model = new QSortFilterProxyModel(this);
		proxy_model->setSourceModel(model);
		proxy_model->setDynamicSortFilter(true);
		proxy_model->sort(EntityModel::NAME_COL);
		proxy_model->setFilterKeyColumn(EntityModel::NAME_COL);
		proxy_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
		view->setModel(proxy_model);
		model->refresh();
		// view->setColumnHidden(EntityModel::ID_COL);

		connect(filter, &QLineEdit::textChanged, this, [proxy_model](const QString& value){
			proxy_model->setFilterFixedString(value);
		});

		auto save_selection = [view]() {
			auto m = view->model();
			view->setProperty("save_selection", 
				m->data(m->index(view->selectionModel()->currentIndex().row(), EntityModel::ID_COL))
			);
		};

		auto restore_selection = [view]() {

			auto m = view->model();
			QVariant selected_id = view->property("save_selection");

			for (int row = 0; row < m->rowCount(); ++row) {
				if (m->data(m->index(row, EntityModel::ID_COL)) == selected_id) {
					view->selectionModel()->setCurrentIndex(m->index(row, EntityModel::NAME_COL), QItemSelectionModel::SelectCurrent);
					return;
				}
			}
		};

		connect(model, &QAbstractItemModel::modelAboutToBeReset, this, save_selection);
		connect(model, &QAbstractItemModel::modelReset, this, restore_selection);		

		connect(&controller, &Controller::dataChanged, [model](const QSet<QString>& tables){
			if (tables.contains("entity")) {
				model->refresh();
			}
		});

		auto new_button = new QPushButton(tr("New"), this);
		connect(new_button, &QPushButton::clicked, [view, model, &controller](){

			auto perform = [view, model, &controller]() -> Result<> {

				auto t = controller.createTransaction("New Entity");
				auto lock_res = t.lockTable("entity");
				if (lock_res.failed())
					return lock_res.error();

				QString table_name;

				{
					int index = 0;
					while(1) {

						table_name = QString("New Entity %1").arg(index);

						if (model->containsName(table_name)) {
							++index;
							continue;
						} else {
							break;
						}
					}
				}

				auto create_node_res = t.insert(
					"entity",
					{
						{"name", table_name}
					},
					"id"
				);

				if (create_node_res.failed()) {
					return create_node_res.error();
				}

				return t.commit();
			};

			auto res = perform();
			if (res.failed()) {
				MessageBoxCritical(EntityList::tr("Error creating entity"), res.errorMessage(), res.errorInfo());
			}
		});

		input_layout->addWidget(new_button);

		setLayout(layout);

		auto edit_row = new QAction(tr("Edit"), this);
		addAction(edit_row);
		edit_row->setShortcut(Qt::Key_Enter);
		edit_row->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(edit_row, &QAction::triggered, this, [this, view, proxy_model](bool){
			for (auto index : view->selectionModel()->selectedIndexes()) {
				emit this->opened(proxy_model->data(index).toString());
			}
		});
		connect(view, &QListView::doubleClicked, this, [this, proxy_model, view](const QModelIndex& index){
			view->selectionModel()->clearSelection();

			emit this->opened(proxy_model->data(proxy_model->index(index.row(), EntityModel::ID_COL)).toString());

		});

		auto rename_row = new QAction(tr("Rename"), this);
		addAction(rename_row);
		rename_row->setShortcut(Qt::Key_F2);
		rename_row->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(rename_row, &QAction::triggered, this, [view, proxy_model](bool){

			if (!view->currentIndex().isValid())
				return;

			view->edit(view->currentIndex());
		});

		auto delete_row = new QAction(tr("Delete"), this);
		addAction(delete_row);
		delete_row->setShortcut(Qt::Key_Delete);
		delete_row->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(delete_row, &QAction::triggered, this, [view, model, proxy_model, &controller](bool) {

			auto perform = [view, proxy_model, &controller]() -> Result<> {

				if (view->selectionModel()->selectedIndexes().isEmpty())
					return Ok();

				auto t = controller.createTransaction("Delete Entity");

				QString where;
				for (auto index : view->selectionModel()->selectedIndexes()) {
					qDebug() << "deleting row";
					QVariant entity_id = proxy_model->data(proxy_model->index(index.row(), EntityModel::ID_COL));
					auto res = t.deleteRow("entity", "id", entity_id);
					if (res.failed())
						return res.error();
				}

				return t.commit();
			};

			auto res = perform();
			if (res.failed()) {
				MessageBoxCritical(tr("Error deleting entity(s)"), res.errorMessage(), res.errorInfo());
			}
		});

		setContextMenuPolicy(Qt::ActionsContextMenu);
	}

	EntityList::~EntityList() {
	}
}