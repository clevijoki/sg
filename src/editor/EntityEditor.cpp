#include "EntityEditor.h"

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
#include <QGroupBox>
#include <QSplitter>

#include "MessageBox.h"
#include "Controller.h"
#include <QTreeView>

namespace sg {

	class EntityPropertyComponentTypeDelegate : public QItemDelegate {

	public:
		using QItemDelegate::QItemDelegate;

		QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex& index) const override {

			auto cb = new QComboBox(parent);

			auto model = new QSqlQueryModel(cb);
			model->setQuery("SELECT name, id FROM component");

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

	class EntityPropMetaModel : public QSqlQueryModel {

		Controller& mController;
		QVariant mComponentId;
	public:

		static const int NAME_COL = 0;
		static const int TYPE_COL = 1;
		static const int DEFAULT_VALUE_COL = 2;
		static const int ID_COL = 3;

		EntityPropMetaModel(Controller& controller, QObject* parent, QVariant entity_id)
		: QSqlQueryModel(parent)
		, mController(controller)
		, mComponentId(std::move(entity_id))
		{}

		~EntityPropMetaModel() {
		}

		void refresh() {
			setQuery(QString("SELECT name, type, default_value, id FROM entity_prop WHERE entity_id = %1").arg(ToSqlLiteral(mComponentId)));
		}

		bool containsName(const QString& name) const {

			for (int row = 0; row < rowCount(); ++row) {
				if (data(index(row, NAME_COL)).toString() == name)
					return true;
			}

			return false;
		}

		Qt::ItemFlags flags(const QModelIndex& index) const override {
			return QSqlQueryModel::flags(index) | Qt::ItemIsEditable;
		};

		bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) {
			if (role != Qt::EditRole) {
				return QSqlQueryModel::setData(index, value, role);
			}

			auto perform = [&](const QString& column_name, bool unique_check) -> Result<> {

				const QString current_value = data(index).toString();
				const QString new_value = value.toString();

				if (current_value == new_value)
					return Ok(); // ignore value change

				if (unique_check && containsName(new_value)) {
					return Error(EntityEditor::tr("Property '%1' '%2' already exists").arg(column_name).arg(new_value));
				}

				// want to rename the table and model table
				auto t = mController.createTransaction("Rename Component");

				{
					QModelIndex id_idx = this->index(index.row(), int(ID_COL));

					auto res = t.update(
						"entity_prop", 
						{{column_name, new_value}},
						{{column_name, current_value}},
						"id",
						data(id_idx)
					);

					if (res.failed())
						return res.error();
				}

				return t.commit();
			};

			switch (index.column()) {

				case NAME_COL: {
					auto res = perform("name", true);
					if (res.failed()) {
						MessageBoxCritical(EntityEditor::tr("Unable to rename property"), res.errorMessage(), res.errorInfo());
						return false;
					}
				}
				break;

				case TYPE_COL: {
					auto res = perform("type", false);

					if (res.failed()) {
						MessageBoxCritical(EntityEditor::tr("Unable to change type"), res.errorMessage(), res.errorInfo());
						return false;
					}
				}
				break;

				case DEFAULT_VALUE_COL: {
					auto res = perform("default_value", false);

					if (res.failed()) {
						MessageBoxCritical(EntityEditor::tr("Unable to change default value"), res.errorMessage(), res.errorInfo());
						return false;
					}
				}
				break;

				default:
					return false;
			}

			refresh();

			return true;
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

		void refreshComponents() {
			QString statement = QString("SELECT entity_component.name, component.name, entity_component.component_id, entity_component.id FROM entity_component INNER JOIN component ON component_id = component.id WHERE entity_component.entity_id = %1 ").arg(mEntityId);

			mComponentModel->setQuery(statement);			
			if (mComponentModel->lastError().isValid()) {
				MessageBoxCritical("Model Query Error", mComponentModel->lastError().text(), statement);
			}
		}

		void refreshEntities() {
			mEntityModel->setQuery(QString("SELECT name, child_entity_id, id FROM entity_child WHERE entity_id = %1").arg(mEntityId));
		}

		void refreshProperties() {
			mPropertyModel->setQuery(QString("SELECT name, entity_property_id, id FROM entity_property WHERE entity_id = %1").arg(mEntityId));
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
			return 2;
		}

		int rowCount(const QModelIndex& parent=QModelIndex()) const override {

			if (!parent.isValid())
				return 3;

			QSqlQueryModel* model = reinterpret_cast<QSqlQueryModel*>(parent.internalPointer());
			if (!model) {
				switch (parent.row()) {
					case 0: return mComponentModel->rowCount();
					case 1: return mEntityModel->rowCount();
					case 2: return mPropertyModel->rowCount();
				}
				qDebug() << "Invalid parent";
				return 0;
			}

			return 0;
			//return model->rowCount();
		}

		QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {

			if (role != Qt::DisplayRole)
				return QVariant();

			QSqlQueryModel* model = reinterpret_cast<QSqlQueryModel*>(index.internalPointer());
			if (model)
				return model->data(model->index(index.row(), index.column()));

			QModelIndex parent = index.parent();
			if (!parent.isValid()) {
				if (index.column() != 0)
					return QVariant();

				switch (index.row()) {
					case 0: return "Components";
					case 1: return "Entities";
					case 2: return "Properties";
					default: qDebug() << "Invalid top level index"; return QVariant("Bad Top Level Data");
				}				
			}

			return "Bad Child Data";
		}

		QModelIndex index(int row, int column, const QModelIndex& parent) const {

			// if parent is invalid, this is a top level node
			if (!parent.isValid()) {
				return createIndex(row, column);
			}

			// otherwise if parent is valid this is a 'Component', 'Entity' or 'Property' child

			if (parent.internalPointer() == nullptr) {
				switch (parent.row()) {
					case 0: return createIndex(row, column, mComponentModel);
					case 1: return createIndex(row, column, mEntityModel);
					case 2: return createIndex(row, column, mPropertyModel);
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
				return createIndex(0, 0);

			if (model == mEntityModel)
				return createIndex(1, 0);

			if (model == mPropertyModel)
				return createIndex(2, 0);

			qDebug() << "Unknown parent index for " << index;
			return QModelIndex(); // error;
		}

		QVariant headerData(int section, Qt::Orientation orientation, int role=Qt::DisplayRole) const override {
			if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
				return QVariant();

			switch (section) {
				case 0: return "Name";
				case 1: return "Value";
				default: return QVariant();
			}
		}
	};
	
	EntityEditor::EntityEditor(Controller& controller, QVariant entity_id, QWidget* parent)
	: QWidget(parent) {

		auto splitter = new QSplitter(Qt::Horizontal);
		auto layout = new QVBoxLayout();
		this->setLayout(layout);
		layout->addWidget(splitter);

		auto viewport = new QWidget();
		viewport->setStyleSheet("background-color:green;");
		splitter->addWidget(viewport);

		auto property_widget = new QWidget();
		splitter->addWidget(property_widget);
		auto property_widget_layout = new QVBoxLayout();
		property_widget->setLayout(property_widget_layout);

		auto filter_line = new QLineEdit();
		filter_line->setPlaceholderText(tr("Search filter..."));

		property_widget_layout->addWidget(filter_line);
		auto model = new EntityChildModel(controller, this, entity_id);

		auto child_view = new QTreeView();
		// child_view->setItemDelegateForColumn(1, new EntityPropertyComponentTypeDelegate());
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

		connect(add_component_button, &QPushButton::clicked, [entity_id, &controller, model](){

			auto perform = [&]() -> Result<> {
				auto t = controller.createTransaction(QString("New entity component"));

				QString new_comp_name;

				{
					int index = 0;

					while (index < 10) {
						new_comp_name = QString("Component %1").arg(++index);

						if (!model->containsComponent(new_comp_name))
							break;
					}
				}

				QVariant first_component_id;

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
						{"entity_id", entity_id},
						{"component_id", first_component_id}
					},
					"id"
				);

				if (res.failed())
					return res.error();

				return t.commit();
			};

			auto res = perform();
			if (res.failed())
				MessageBoxCritical(EntityEditor::tr("Error adding component"), res.errorMessage(), res.errorInfo());

			model->refreshComponents();
		});

#if 0
		connect(add_button, &QPushButton::clicked, [entity_id, &controller, model](){

			auto perform = [&]() -> Result<> {
				auto t = controller.createTransaction(QString("New component property"));

				QString new_prop_name;

				{
					int index = 0;

					while (index < 10) {
						new_prop_name = QString("Property_%1").arg(++index);

						if (!model->containsName(new_prop_name))
							break;
					}
				}

				// add a new row to the selected component
				auto res = t.insert(
					"entity_prop",
					{
						{"entity_id", entity_id},
						{"name", new_prop_name},
						{"type", "i32"},
						{"default_value", "0"}
					},
					"id"
				);

				if (res.failed())
					return res.error();

				model->refresh();

				return t.commit();
			};

			auto res = perform();
			if (res.failed()) {
				MessageBoxCritical(EntityEditor::tr("Error creating component property"), res.errorMessage(), res.errorInfo());
			}
		});

		auto save_selection = [child_view, proxy_model]() {

			QStringList selected_ids;

			for (const auto idx : child_view->selectionModel()->selectedIndexes()) {
				selected_ids.push_back(proxy_model->data(proxy_model->index(idx.row(), EntityPropMetaModel::ID_COL)).toString());
			}

			child_view->setProperty("save_selection", selected_ids.join(":"));
		};

		auto restore_selection = [child_view, proxy_model]() {

			QStringList selected_ids = child_view->property("save_selection").toString().split(":");

			child_view->clearSelection();

			for (int row = 0; row < proxy_model->rowCount(); ++row) {
				QModelIndex idx = proxy_model->index(row, EntityPropMetaModel::ID_COL);
				if (selected_ids.contains(proxy_model->data(idx).toString())) {
					child_view->setCurrentIndex(proxy_model->index(row, EntityPropMetaModel::NAME_COL));
					return;
				}
			}
		};

		connect(model, &QAbstractItemModel::modelAboutToBeReset, this, save_selection);
		connect(model, &QAbstractItemModel::modelReset, this, restore_selection);		

		auto delete_row = new QAction(tr("Delete"), this);
		addAction(delete_row);
		delete_row->setShortcut(Qt::Key_Delete);
		delete_row->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(delete_row, &QAction::triggered, [child_view, model, proxy_model, &controller](bool){

			auto perform = [&]() -> Result<> {
				auto t = controller.createTransaction("Delete property");

				for (auto index : child_view->selectionModel()->selectedRows()) {
					QString id = proxy_model->data(proxy_model->index(index.row(), EntityPropMetaModel::ID_COL)).toString();

					auto res = t.deleteRow("entity_prop", "id", id);
					if (res.failed())
						return res.error();
				}

				return t.commit();
			};

			auto res = perform();
			if (res.failed()) {
				MessageBoxCritical(tr("Error deleting property"), res.errorMessage(), res.errorInfo());
			} else {
				model->refresh();
			}
		});
#endif		
		setContextMenuPolicy(Qt::ActionsContextMenu);
	}

	EntityEditor::~EntityEditor() {
	}
}