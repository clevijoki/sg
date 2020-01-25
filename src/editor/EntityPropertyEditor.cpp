#include "EntityPropertyEditor.h"

#include <QTableView>
#include <QSqlQueryModel>
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
#include <QScrollArea>

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
					return Error(EntityPropertyEditor::tr("Property '%1' '%2' already exists").arg(column_name).arg(new_value));
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
						MessageBoxCritical(EntityPropertyEditor::tr("Unable to rename property"), res.errorMessage(), res.errorInfo());
						return false;
					}
				}
				break;

				case TYPE_COL: {
					auto res = perform("type", false);

					if (res.failed()) {
						MessageBoxCritical(EntityPropertyEditor::tr("Unable to change type"), res.errorMessage(), res.errorInfo());
						return false;
					}
				}
				break;

				case DEFAULT_VALUE_COL: {
					auto res = perform("default_value", false);

					if (res.failed()) {
						MessageBoxCritical(EntityPropertyEditor::tr("Unable to change default value"), res.errorMessage(), res.errorInfo());
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
	
	EntityPropertyEditor::EntityPropertyEditor(Controller& controller, QVariant entity_id, QWidget* parent)
	: QWidget(parent) {

		auto scroll_area = new QScrollArea();
		auto layout = new QVBoxLayout();
		this->setLayout(layout);
		layout->addWidget(scroll_area);

		auto scroll_area_widget = new QWidget(scroll_area);
		auto scroll_area_layout = new QVBoxLayout();
		scroll_area_widget->setLayout(scroll_area_layout);

		auto component_group = new QGroupBox(tr("Components"));
		auto component_group_layout = new QVBoxLayout();

		scroll_area_layout->addWidget(component_group);

		auto filter_line = new QLineEdit(scroll_area_widget);

		auto filter_layout = new QHBoxLayout();
		filter_layout->addWidget(filter_line);

		auto add_button = new QPushButton(tr("Add Component"), scroll_area_widget);
		auto model = new EntityPropMetaModel(controller, this, entity_id);

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
				MessageBoxCritical(EntityPropertyEditor::tr("Error creating component property"), res.errorMessage(), res.errorInfo());
			}
		});

		filter_layout->addWidget(add_button);
		component_group_layout->addLayout(filter_layout);

		auto component_view = new QTableView(scroll_area_widget);
		component_view->setItemDelegateForColumn(1, new EntityPropertyComponentTypeDelegate());
		component_group_layout->addWidget(component_view);

		auto component_proxy_model = new QSortFilterProxyModel(this);
		component_proxy_model->setSourceModel(model);
		component_view->setModel(component_proxy_model);

		model->refresh();
		component_view->hideColumn(EntityPropMetaModel::ID_COL);

		auto save_selection = [component_view, component_proxy_model]() {

			QStringList selected_ids;

			for (const auto idx : component_view->selectionModel()->selectedIndexes()) {
				selected_ids.push_back(component_proxy_model->data(component_proxy_model->index(idx.row(), EntityPropMetaModel::ID_COL)).toString());
			}

			component_view->setProperty("save_selection", selected_ids.join(":"));
		};

		auto restore_selection = [component_view, component_proxy_model]() {

			QStringList selected_ids = component_view->property("save_selection").toString().split(":");

			component_view->clearSelection();

			for (int row = 0; row < component_proxy_model->rowCount(); ++row) {
				QModelIndex idx = component_proxy_model->index(row, EntityPropMetaModel::ID_COL);
				if (selected_ids.contains(component_proxy_model->data(idx).toString())) {
					component_view->setCurrentIndex(component_proxy_model->index(row, EntityPropMetaModel::NAME_COL));
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
		connect(delete_row, &QAction::triggered, [component_view, model, component_proxy_model, &controller](bool){

			auto perform = [&]() -> Result<> {
				auto t = controller.createTransaction("Delete property");

				for (auto index : component_view->selectionModel()->selectedRows()) {
					QString id = component_proxy_model->data(component_proxy_model->index(index.row(), EntityPropMetaModel::ID_COL)).toString();

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
		component_group->setLayout(component_group_layout);
		component_group->show();

		scroll_area->setWidget(scroll_area_widget);
		scroll_area_widget->show();

		setContextMenuPolicy(Qt::ActionsContextMenu);
	}

	EntityPropertyEditor::~EntityPropertyEditor() {
	}
}