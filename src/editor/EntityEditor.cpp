#include "EntityEditor.h"
#include "FormatString.h"

#include <QTableView>
#include <QSqlQueryModel>
#include <QSqlError>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QPushButton>
#include <QAction>
#include <QItemDelegate>
#include <QComboBox>
#include <QLabel>
#include <QDebug>
#include <QTabWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QCursor>
#include <QtMath>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QTreeView>

#include "EntityGraphicsScene.h"
#include "ComponentSelector.h"
#include "EntitySelector.h"
#include "MessageBox.h"
#include "Controller.h"
#include "ViewEventFilters.h"


namespace sg {

	class EntityComponentModel : public QSqlQueryModel {

		QString mEntityId;

	public:
		static const int NAME_COL = 0;
		static const int TYPE_NAME_COL = 1;
		static const int TYPE_ID_COL = 2;
		static const int ID_COL = 3;
		static const int GRAPH_POS_COL = 4;

		EntityComponentModel(const QVariant& entity_id, QObject *parent)
		: QSqlQueryModel(parent)
		, mEntityId(entity_id.toString()) {
			refresh();
		}

		void refresh() {
			QString statement = QString("SELECT entity_component.name, component.name, entity_component.component_id, entity_component.id, entity_component.graph_pos FROM entity_component INNER JOIN component ON component_id = component.id WHERE entity_component.entity_id = %1 ").arg(mEntityId);

			setQuery(statement);
			if (lastError().isValid()) {
				MessageBoxCritical("Model Query Error", lastError().text(), statement);
			}
		}

		bool contains(const QString& name) {

			for (int row = 0; row < rowCount(); ++row) {
				if (data(index(row, 0)).toString() == name)
					return true;
			}

			return false;
		}

		Result<> addNew(Transaction& t, const QVariant& component_type_id, const QVariant& graph_pos) {
			QString new_comp_name;

			{
				int index = 0;

				while (index < 100) {
					new_comp_name = QString("Component %1").arg(++index);

					if (!contains(new_comp_name))
						break;
				}

				if (index == 100)
					return Error("Too many unnamed components");
			}

			auto insert_res = t.insert(
				"entity_component",
				{
					{"name", new_comp_name},
					{"entity_id", mEntityId},
					{"component_id", component_type_id},
					{"graph_pos", graph_pos},
				},
				"id"
			);

			if (insert_res.failed())
				return insert_res.error();

			beginInsertRows(QModelIndex(), rowCount(), rowCount());
			refresh();
			endInsertRows();

			return Ok();
		}


	};

	class EntityChildModel : public QAbstractItemModel {

		Controller& mController;

		QSqlQueryModel *mComponentModel;
		QSqlQueryModel *mEntityModel;
		QSqlQueryModel *mPropertyModel;

		QString mEntityId;

	public:
		EntityChildModel(Controller& controller, QObject* parent, QVariant entity_id)
		 : QAbstractItemModel(parent)
		 , mController(controller)
		 , mEntityId(entity_id.toString())
		 , mComponentModel(new QSqlQueryModel(this))
		 , mEntityModel(new QSqlQueryModel(this))
		 , mPropertyModel(new QSqlQueryModel(this))
		{}

		static const int NAME_COL = 0;
		static const int TYPE_COL = 1;
		static const int VALUE_COL = 2;
		static const int COL_COUNT = 3;

		static const int COMPONENT_CHILD_ROW = 0;
		static const int ENTITY_CHILD_ROW = 1;
		static const int PROP_CHILD_ROW = 2;
		static const int ROOT_ROW_COUNT = 3;

		static const int COMPONENT_NAME_COL = 0;
		static const int COMPONENT_TYPE_NAME_COL = 1;
		static const int COMPONENT_TYPE_ID_COL = 2;
		static const int COMPONENT_ID_COL = 3;

		void refreshComponents() {
			QString statement = QString("SELECT entity_component.name, component.name, entity_component.component_id, entity_component.id FROM entity_component INNER JOIN component ON component_id = component.id WHERE entity_component.entity_id = %1 ").arg(mEntityId);

			mComponentModel->setQuery(statement);
			if (mComponentModel->lastError().isValid()) {
				MessageBoxCritical("Model Query Error", mComponentModel->lastError().text(), statement);
			}
		}

		static const int ENTITY_NAME_COL = 0;
		static const int ENTITY_TYPE_NAME_COL = 1;
		static const int ENTITY_TYPE_ID_COL = 2;
		static const int ENTITY_ID_COL = 3;

		void refreshEntities() {
			QString statement = QString("SELECT entity_child.name, entity.name, entity_child.child_id, entity_child.id FROM entity_child INNER JOIN entity ON entity_child.child_id = entity.id WHERE entity_id = %1").arg(mEntityId);
			mEntityModel->setQuery(statement);
			if (mEntityModel->lastError().isValid()) {
				MessageBoxCritical("Model Query Error", mEntityModel->lastError().text(), statement);
			}
		}

		static const int PROP_NAME_COL = 0;
		static const int PROP_TYPE_NAME_COL = 1;
		static const int PROP_DEFAULT_TYPE_COL = 2;
		static const int PROP_ID_COL = 3;

		void refreshProperties() {
			QString statement = QString("SELECT name, type, default_value, id FROM entity_prop WHERE entity_id = %1").arg(mEntityId);
			mPropertyModel->setQuery(statement);
			if (mPropertyModel->lastError().isValid()) {
				MessageBoxCritical("Model Query Error", mPropertyModel->lastError().text(), statement);
			}
		}

		void refresh() {
			refreshComponents();
			refreshEntities();
			refreshProperties();
		}

		bool containsComponent(const QString& name) {

			for (int row = 0; row < mComponentModel->rowCount(); ++row) {
				if (mComponentModel->data(mComponentModel->index(row, 0)).toString() == name)
					return true;
			}

			return false;
		}

		bool containsEntity(const QString& name) {

			for (int row = 0; row < mEntityModel->rowCount(); ++row) {
				if (mEntityModel->data(mEntityModel->index(row, 0)).toString() == name)
					return true;
			}

			return false;
		}

		bool containsProperty(const QString& name) {

			for (int row = 0; row < mPropertyModel->rowCount(); ++row) {
				if (mPropertyModel->data(mPropertyModel->index(row, 0)).toString() == name)
					return true;
			}

			return false;
		}

		int columnCount(const QModelIndex& parent=QModelIndex()) const override {
			return COL_COUNT;
		}

		Qt::ItemFlags flags(const QModelIndex& index) const override {
			if (!index.parent().isValid())
				return QAbstractItemModel::flags(index);

			return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
		};

		int rowCount(const QModelIndex& parent=QModelIndex()) const override {

			if (!parent.isValid())
				return ROOT_ROW_COUNT;

			QSqlQueryModel* model = reinterpret_cast<QSqlQueryModel*>(parent.internalPointer());
			if (!model) {
				switch (parent.row()) {
					case COMPONENT_CHILD_ROW: return mComponentModel->rowCount();
					case ENTITY_CHILD_ROW: return mEntityModel->rowCount();
					case PROP_CHILD_ROW: return mPropertyModel->rowCount();
				}
				qDebug() << "Invalid parent";
				return 0;
			}

			return 0;
			//return model->rowCount();
		}

		QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {

			if (role != Qt::DisplayRole && role != Qt::EditRole)
				return QVariant();

			QModelIndex parent = index.parent();

			if (!parent.isValid()) {
				if (index.column() != NAME_COL)
					return QVariant();

				switch (index.row()) {
					case COMPONENT_CHILD_ROW: return "Components";
					case ENTITY_CHILD_ROW: return "Entities";
					case PROP_CHILD_ROW: return "Properties";
					default: qDebug() << "Invalid top level index"; return QVariant("Bad Top Level Data");
				}

			} else {

				QModelIndex parent2 = parent.parent();
				if (parent2.isValid()) {
					// TODO: these our are our top level children
					return QVariant("TODO");
				}

				switch (parent.row()) {
					case COMPONENT_CHILD_ROW: {
						switch (index.column()) {
							case COMPONENT_NAME_COL: return mComponentModel->data(mComponentModel->index(index.row(), COMPONENT_NAME_COL));
							case COMPONENT_TYPE_NAME_COL: return mComponentModel->data(mComponentModel->index(index.row(), role == Qt::EditRole ? COMPONENT_TYPE_ID_COL : COMPONENT_TYPE_NAME_COL));
							default: return QVariant();
						}
					} break;

					case ENTITY_CHILD_ROW: {
						switch (index.column()) {
							case ENTITY_NAME_COL: return mEntityModel->data(mEntityModel->index(index.row(), ENTITY_NAME_COL));
							case ENTITY_TYPE_NAME_COL: return mEntityModel->data(mEntityModel->index(index.row(), role == Qt::EditRole ? ENTITY_TYPE_ID_COL : ENTITY_TYPE_NAME_COL));
							default: return QVariant();
						}
					} break;

					case PROP_CHILD_ROW: {
						return mPropertyModel->data(mPropertyModel->index(index.row(), index.column()));
					} break;

					default: return QVariant("Unknown root row");
				}
			}

			return "Bad Child Data";
		}

		QModelIndex index(int row, int column, const QModelIndex& parent=QModelIndex()) const {

			// if parent is invalid, this is a top level node
			if (!parent.isValid()) {
				return createIndex(row, column);
			}

			// otherwise if parent is valid this is a 'Component', 'Entity' or 'Property' child

			if (parent.internalPointer() == nullptr) {
				switch (parent.row()) {
					case COMPONENT_CHILD_ROW: return createIndex(row, column, mComponentModel);
					case ENTITY_CHILD_ROW: return createIndex(row, column, mEntityModel);
					case PROP_CHILD_ROW: return createIndex(row, column, mPropertyModel);
				}
				qDebug() << "bad index parent internalPointer assumption";
				return QModelIndex();
			} else {
				// this is the model children
				qDebug() << "should not have level 3 yet";
				return QModelIndex();
			}
		}

		QModelIndex parent(const QModelIndex& index) const {

			QSqlQueryModel* model = reinterpret_cast<QSqlQueryModel*>(index.internalPointer());
			if (!model)
				return QModelIndex(); // root node

			if (model == mComponentModel)
				return createIndex(COMPONENT_CHILD_ROW, NAME_COL);

			if (model == mEntityModel)
				return createIndex(ENTITY_CHILD_ROW, NAME_COL);

			if (model == mPropertyModel)
				return createIndex(PROP_CHILD_ROW, NAME_COL);

			qDebug() << "Unknown parent index for " << index;
			return QModelIndex(); // error;
		}

		QVariant headerData(int section, Qt::Orientation orientation, int role=Qt::DisplayRole) const override {
			if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
				return QVariant();

			switch (section) {
				case NAME_COL: return "Name";
				case TYPE_COL: return "Type";
				case VALUE_COL: return "Value";
				default: return QVariant();
			}
		}

		Result<> newComponent() {
			QString new_comp_name;

			{
				int index = 0;

				while (index < 100) {
					new_comp_name = QString("Component %1").arg(++index);

					if (!containsComponent(new_comp_name))
						break;
				}

				if (index == 100)
					return Error("Too many unnamed components");
			}

			QVariant first_component_id;

			auto t = mController.createTransaction(QString("New entity component"));

			{
				QSqlQueryModel m;
				m.setQuery("SELECT id FROM component", *t.connection());
				if (m.rowCount() < 1)
					return Error("There are no components to add");

				first_component_id = m.data(m.index(0,0)).toString();
			}


			auto res = t.insert(
				"entity_component",
				{
					{"name", new_comp_name},
					{"entity_id", mEntityId},
					{"component_id", first_component_id}
				},
				"id"
			);

			if (res.failed())
				return res.error();

			beginInsertRows(this->index(COMPONENT_CHILD_ROW, NAME_COL), mComponentModel->rowCount(), mComponentModel->rowCount());
			auto result = t.commit();
			endInsertRows();

			return result;
		}

		Result<> deleteComponent(Transaction& t, int row) {

			QVariant id_to_delete = mComponentModel->data(mComponentModel->index(row, COMPONENT_ID_COL));

			if (!id_to_delete.isValid())
				return Error("Invalid id");

			beginRemoveRows(this->index(COMPONENT_CHILD_ROW,0), row, row);
			auto result = t.deleteRow("entity_component", "id", id_to_delete);
			endRemoveRows();

			return result;
		}

		Result<> newChildEntity() {
			QString child_name;

			{
				int index = 0;

				while (index < 100) {
					child_name = QString("Entity %1").arg(++index);

					if (!containsEntity(child_name))
						break;
				}

				if (index == 100)
					return Error("Too many unnamed components");
			}

			QVariant first_entity_id;

			auto t = mController.createTransaction(QString("New entity child"));

			{
				QSqlQueryModel m;
				m.setQuery("SELECT id FROM entity", *t.connection());
				if (m.rowCount() < 2)
					return Error("There are no entities to add (cannot add self)");

				for (int row = 0; row < m.rowCount(); ++row) {
					QVariant entity_id = m.data(m.index(0,0)).toString();

					if (entity_id != mEntityId) {
						first_entity_id = std::move(entity_id);
						break;
					}
				}
			}

			if (!first_entity_id.isValid())
				return Error("Unable to find valid entity child");

			auto res = t.insert(
				"entity_child",
				{
					{"name", child_name},
					{"entity_id", mEntityId},
					{"child_id", first_entity_id}
				},
				"id"
			);

			if (res.failed())
				return res.error();

			beginInsertRows(this->index(ENTITY_CHILD_ROW, NAME_COL), mEntityModel->rowCount(), mEntityModel->rowCount());
			auto result = t.commit();
			endInsertRows();

			return result;
		}

		Result<> deleteEntity(Transaction& t, int row) {
			QVariant id_to_delete = mEntityModel->data(mEntityModel->index(row, ENTITY_ID_COL));

			if (!id_to_delete.isValid())
				return Error("Invalid id");

			beginRemoveRows(this->index(ENTITY_CHILD_ROW, NAME_COL), row, row);
			auto result = t.deleteRow("entity_child", "id", id_to_delete);
			endRemoveRows();

			return result;
		}

		Result<> newChildProperty() {
			return Error("TODO");
		}

		Result<> deleteProperty(Transaction& t, int row) {
			QVariant id_to_delete = mPropertyModel->data(mPropertyModel->index(row, PROP_ID_COL));

			if (!id_to_delete.isValid())
				return Error("Invalid id");

			beginRemoveRows(this->index(PROP_CHILD_ROW, NAME_COL), row, row);
			auto result = t.deleteRow("entity_prop", "id", id_to_delete);
			endRemoveRows();

			return result;
		}

		bool setData(const QModelIndex& index, const QVariant& new_value, int role = Qt::EditRole) {
			if (role != Qt::EditRole) {
				return false;
			}

			auto perform = [&]() -> Result<> {

				// identify what property is being set

				QModelIndex parent1 = index.parent();
				if (!parent1.isValid())
					return Error("Cannot edit root elements");

				QModelIndex parent2 = parent1.parent();
				if (parent2.isValid()) {

					if (index.row() != TYPE_COL)
						return Error("Can only edit value properties");

					return Error("TODO: allow editing properties");
				}

				// this editing a child property
				switch (parent1.row()) {
					case COMPONENT_CHILD_ROW: {

						// editing a component child
						switch (index.column()) {
							case COMPONENT_NAME_COL: {

								const QVariant old_value = mComponentModel->data(mComponentModel->index(index.row(), index.column()), role);
								if (old_value == new_value)
									return Ok();

								if (containsComponent(new_value.toString()))
									return Error("Already contains child component named '"_sb + new_value.toString() + "'");

								auto t = mController.createTransaction("Rename entity child component");
								auto update_res = t.update(
									"entity_component",
									{{"name", new_value}},
									{{"name", old_value}},
									"id",
									mComponentModel->data(mComponentModel->index(index.row(), COMPONENT_ID_COL))
								);

								if (update_res.failed())
									return update_res.error();

								auto commit_res = t.commit();
								if (!commit_res.failed())
									refreshComponents();

								return commit_res;
							} break;
							case COMPONENT_TYPE_NAME_COL: {

								const QVariant old_value = mComponentModel->data(mComponentModel->index(index.row(), COMPONENT_TYPE_ID_COL));
								if (old_value == new_value)
									return Ok();

								auto t = mController.createTransaction("Change entity child name");
								auto update_res = t.update(
									"entity_component",
									{{"component_id", new_value}},
									{{"component_id", old_value}},
									"id",
									mComponentModel->data(mComponentModel->index(index.row(), COMPONENT_ID_COL))
								);

								if (update_res.failed())
									return update_res.error();

								auto commit_res = t.commit();
								if (!commit_res.failed())
									refreshComponents();

								return commit_res;
							} break;
							default: return Error(QString("Unable to modify child component column %1").arg(index.column()));
						}

					} break;
					case ENTITY_CHILD_ROW: {
						// editing an entity child
						return Error("TODO: entity child editing");
					} break;
					case PROP_CHILD_ROW: {
						// editing a property child
						return Error("TODO: prop child editing");
					} break;

					default: return Error("Unknown property");
				}

				return Error("Bad code path");
			};

			auto res = perform();
			if (res.failed()) {
				MessageBoxCritical(EntityEditor::tr("Unable to update data"), res.errorMessage(), res.errorInfo());
				return false;
			}

			dataChanged(index, index, {role});
			return true;
		}

	};

	class EntityValueTypeDelegate : public QItemDelegate {
	public:
		using QItemDelegate::QItemDelegate;

		QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex& index) const override {

			QModelIndex parent1 = index.parent();

			if (!parent1.isValid())
				return nullptr;

			if (parent1.parent().isValid())
				return nullptr; // too deep

			auto cb = new QComboBox(parent);
			auto model = new QSqlQueryModel(cb);

			switch (parent1.row()) {
				case EntityChildModel::COMPONENT_CHILD_ROW: {
					model->setQuery("SELECT name, id FROM component");
				} break;

				case EntityChildModel::ENTITY_CHILD_ROW: {
					model->setQuery("SELECT name, id FROM entity");
				} break;

				case EntityChildModel::PROP_CHILD_ROW: {
					model->setQuery("SELECT name, name FROM prop_type");
				} break;

				default: return nullptr;
			}

			cb->setModel(model);

			return cb;
		}

		void setEditorData(QWidget *editor, const QModelIndex& index) const {

			QComboBox *cb = static_cast<QComboBox*>(editor);

			QString text = index.model()->data(index).toString();
			int idx = cb->findText(text);
			if (idx != -1) {
				cb->setCurrentIndex(idx);
			} else {
				qDebug() << " unable to find index for " << text;
			}
		}

		void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex& index) const {

			QComboBox *cb = static_cast<QComboBox*>(editor);

			model->setData(index, cb->currentText(), Qt::EditRole);
		}

		void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem& option, const QModelIndex&) const {

			editor->setGeometry(option.rect);
		}
	};

	EntityEditor::EntityEditor(Controller& controller, QVariant entity_id, QWidget* parent)
	: QWidget(parent)
	, mGraphicsView(new QGraphicsView(this)) {

		auto layout = new QVBoxLayout();
		this->setLayout(layout);
		auto tab_widget = new QTabWidget();
		layout->addWidget(tab_widget);

		// auto game_viewport = new QWidget();
		// game_viewport->setStyleSheet("background-color:green;");
		// tab_widget->addTab(game_viewport, "Game");

		auto graph_scene = new EntityGraphicsScene(controller, entity_id.toLongLong(), this);

		mGraphicsView->setDragMode(QGraphicsView::RubberBandDrag);
		mGraphicsView->setScene(graph_scene);
		mGraphicsView->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
		mGraphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
		mGraphicsView->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
		// mGraphicsView->setOptimizationFlags(QGraphicsView::DontSavePainterState);

		tab_widget->addTab(mGraphicsView, "Setup");

		auto ecm = new EntityComponentModel(entity_id, this);

		auto add_component_action = new QAction("Add Component");
		mGraphicsView->addAction(add_component_action);
		add_component_action->setShortcut(Qt::CTRL + Qt::Key_N);
		add_component_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(add_component_action, &QAction::triggered, this, [this, &controller, ecm](bool){

			auto res = [&]() -> Result<> {
				auto t = controller.createTransaction("Query Component");
				ComponentSelector cs(t, this);

				QPointF new_point = mGraphicsView->mapFromGlobal(QCursor::pos());

				if (cs.exec() == QDialog::Accepted) {
					auto res = ecm->addNew(t, cs.selectedId(), new_point);
					if (res.failed()) {
						return res;
					}

					return t.commit();
				}

				return Ok();
			}();

			if (res.failed()) {
				MessageBoxCritical(tr("Error adding new component"), res.errorMessage(), res.errorInfo());
			}
		});

		auto add_entity_action = new QAction("Add Entity Instance");
		mGraphicsView->addAction(add_entity_action);
		add_entity_action->setShortcut(Qt::CTRL + Qt::Key_A);
		add_entity_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(add_entity_action, &QAction::triggered, this, [&](bool){

			auto t = controller.createTransaction("Query Entity");
			EntitySelector es(t, this);

			if (es.exec() == QDialog::Accepted) {
				qDebug() << "TODO new entity";
			}
		});

		mGraphicsView->setContextMenuPolicy(Qt::ActionsContextMenu);


		/*
		auto property_widget = new QWidget();
		splitter->addWidget(property_widget);
		auto property_widget_layout = new QVBoxLayout();
		property_widget->setLayout(property_widget_layout);

		auto filter_line = new QLineEdit();
		filter_line->setPlaceholderText(tr("Search filter..."));

		property_widget_layout->addWidget(filter_line);
		auto model = new EntityChildModel(controller, this, entity_id);

		auto child_view = new QTreeView();
		child_view->setItemDelegateForColumn(1, new EntityValueTypeDelegate());
		property_widget_layout->addWidget(child_view);

		// TODO: probably need custom filter model because it's a tree view
		auto proxy_model = new QSortFilterProxyModel(this);
		proxy_model->setSourceModel(model);
		proxy_model->setDynamicSortFilter(true);
		proxy_model->sort(0);
		proxy_model->setFilterKeyColumn(0);
		proxy_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
		child_view->setModel(proxy_model);

		model->refresh();
		child_view->setFirstColumnSpanned(0, QModelIndex(), true);

		auto filter_layout = new QHBoxLayout();
		auto add_component_button = new QPushButton(tr("Add Component"));
		auto add_entity_button = new QPushButton(tr("Add Entity"));
		auto add_property_button = new QPushButton(tr("Add Property"));
		filter_layout->addWidget(add_component_button);
		filter_layout->addWidget(add_entity_button);
		filter_layout->addWidget(add_property_button);
		property_widget_layout->addLayout(filter_layout);

		connect(filter_line, &QLineEdit::textChanged, this, [child_view, proxy_model](const QString& value){
			proxy_model->setFilterFixedString(value);
			child_view->setFirstColumnSpanned(0, QModelIndex(), true);
		});

		connect(add_component_button, &QPushButton::clicked, [model](){

			auto res = model->newComponent();
			if (res.failed())
				MessageBoxCritical(EntityEditor::tr("Error adding component"), res.errorMessage(), res.errorInfo());

			model->refreshComponents();
		});

		connect(add_entity_button, &QPushButton::clicked, [model](){
			auto res = model->newChildEntity();
			if (res.failed())
				MessageBoxCritical(EntityEditor::tr("Error adding child entity"), res.errorMessage(), res.errorInfo());

			model->refreshEntities();
		});

		// auto save_selection = [child_view, proxy_model]() {

		// 	QStringList selected_ids;

		// 	for (const auto idx : child_view->selectionModel()->selectedIndexes()) {
		// 		selected_ids.push_back(proxy_model->data(proxy_model->index(idx.row(), EntityChildModel::ID_COL)).toString());
		// 	}

		// 	child_view->setProperty("save_selection", selected_ids.join(":"));
		// };

		// auto restore_selection = [child_view, proxy_model]() {

		// 	QStringList selected_ids = child_view->property("save_selection").toString().split(":");

		// 	child_view->clearSelection();

		// 	for (int row = 0; row < proxy_model->rowCount(); ++row) {
		// 		QModelIndex idx = proxy_model->index(row, EntityChildModel::ID_COL);
		// 		if (selected_ids.contains(proxy_model->data(idx).toString())) {
		// 			child_view->setCurrentIndex(proxy_model->index(row, 0));
		// 			return;
		// 		}
		// 	}
		// };

		// connect(model, &QAbstractItemModel::modelAboutToBeReset, this, save_selection);
		// connect(model, &QAbstractItemModel::modelReset, this, restore_selection);

		auto delete_something = new QAction(tr("Delete"), this);
		addAction(delete_something);
		delete_something->setShortcut(Qt::Key_Delete);
		delete_something->setShortcutContext(Qt::WidgetWithChildrenShortcut);

		connect(delete_something, &QAction::triggered, [child_view, model, proxy_model, &controller](bool){

			auto perform = [&]() -> Result<> {
				auto t = controller.createTransaction("Delete Entity Children");

				for (auto index : child_view->selectionModel()->selectedRows()) {

					QModelIndex parent1 = index.parent();
					if (!parent1.isValid())
						continue;

					QModelIndex parent2 = parent1.parent();

					if (parent2.isValid())
						continue;

					QModelIndex root_model_index = proxy_model->mapToSource(parent1);
					QModelIndex model_index = proxy_model->mapToSource(index);

					Result<> delete_res = Ok();

					switch (root_model_index.row()) {
						case EntityChildModel::COMPONENT_CHILD_ROW: delete_res = model->deleteComponent(t, index.row()); break;
						case EntityChildModel::ENTITY_CHILD_ROW: delete_res = model->deleteEntity(t, index.row()); break;
						case EntityChildModel::PROP_CHILD_ROW: delete_res = model->deleteProperty(t, index.row()); break;
						default: delete_res = Error(QString("Unknown root row %1").arg(root_model_index.row())); break;
					}

					if (delete_res.failed())
						return delete_res.error();
				}

				if (t.hasCommands())
					return t.commit();

				return Ok();
			};

			auto res = perform();
			if (res.failed()) {
				MessageBoxCritical(tr("Error deleting property"), res.errorMessage(), res.errorInfo());
			} else {
				model->refresh();
			}
		});
		*/

		setContextMenuPolicy(Qt::ActionsContextMenu);
	}

	EntityEditor::~EntityEditor() {
	}

	// void EntityEditor::wheelEvent(QWheelEvent *e) {
	// 	if (e->angleDelta().y() > 0) {
	// 		setZoomLevel(6);
	// 	} else {
	// 		setZoomLevel(-6);
	// 	}

	// 	e->accept();
	// }

	void EntityEditor::setZoomLevel(int adjust) {
		mZoomLevel = std::min<int>(500, std::max<int>(0, mZoomLevel + adjust));
		setupMatrix();
	}

	void EntityEditor::setupMatrix() {
		qreal scale = qPow(qreal(2), (mZoomLevel - 250) / qreal(50));

		QMatrix matrix;
		matrix.scale(scale, scale);

		mGraphicsView->setMatrix(matrix);
	}

}