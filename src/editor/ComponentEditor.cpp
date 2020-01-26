#include "ComponentEditor.h"

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
#include <QDebug>

#include "MessageBox.h"
#include "Controller.h"
#include <QTreeView>

namespace sg {

	class ComponentPropTypeDelegate : public QItemDelegate {

	public:
		using QItemDelegate::QItemDelegate;

		QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex& index) const override {

			auto cb = new QComboBox(parent);
			cb->addItems({
				"i8",
				"u8",
				"i16",
				"u16",
				"i32",
				"u32",
				"f32",
				"f64",
				"text",
				"component",
				"component_ref",
			});

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

	class ComponentPropModel : public QSqlQueryModel {

		Controller& mController;
		QVariant mComponentId;
	public:

		static const int NAME_COL = 0;
		static const int TYPE_COL = 1;
		static const int DEFAULT_VALUE_COL = 2;
		static const int ID_COL = 3;

		ComponentPropModel(Controller& controller, QObject* parent, QVariant component_id)
		: QSqlQueryModel(parent)
		, mController(controller)
		, mComponentId(std::move(component_id))
		{}

		~ComponentPropModel() {
		}

		void refresh() {
			setQuery(QString("SELECT name AS \"Name\", type AS \"Type\", default_value AS \"Default Value\", id FROM component_prop WHERE component_id = %1").arg(ToSqlLiteral(mComponentId)));
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
					return Error(ComponentEditor::tr("Property '%1' '%2' already exists").arg(column_name).arg(new_value));
				}

				// want to rename the table and model table
				auto t = mController.createTransaction("Rename Component");

				{
					QModelIndex id_idx = this->index(index.row(), int(ID_COL));

					auto res = t.update(
						"component_prop", 
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
						MessageBoxCritical(ComponentEditor::tr("Unable to rename property"), res.errorMessage(), res.errorInfo());
						return false;
					}
				}
				break;

				case TYPE_COL: {
					auto res = perform("type", false);

					if (res.failed()) {
						MessageBoxCritical(ComponentEditor::tr("Unable to change type"), res.errorMessage(), res.errorInfo());
						return false;
					}
				}
				break;

				case DEFAULT_VALUE_COL: {
					auto res = perform("default_value", false);

					if (res.failed()) {
						MessageBoxCritical(ComponentEditor::tr("Unable to change default value"), res.errorMessage(), res.errorInfo());
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
	
	ComponentEditor::ComponentEditor(Controller& controller, QVariant component_id, QWidget* parent)
	: QWidget(parent) {

		auto layout = new QVBoxLayout(this);
		setLayout(layout);

		auto filter_line = new QLineEdit(this);
		filter_line->setPlaceholderText(tr("Search filter..."));

		auto filter_layout = new QHBoxLayout();
		layout->addLayout(filter_layout);

		filter_layout->addWidget(filter_line);

		auto add_button = new QPushButton(tr("New Property"), this);
		auto model = new ComponentPropModel(controller, this, component_id);

		connect(add_button, &QPushButton::clicked, [component_id, &controller, model](){

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
					"component_prop",
					{
						{"component_id", component_id},
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
				MessageBoxCritical(ComponentEditor::tr("Error creating component property"), res.errorMessage(), res.errorInfo());
			}
		});

		filter_layout->addWidget(add_button);

		auto view = new QTreeView(this);
		view->setItemDelegateForColumn(ComponentPropModel::TYPE_COL, new ComponentPropTypeDelegate());
		layout->addWidget(view);

		auto proxy_model = new QSortFilterProxyModel(this);
		proxy_model->setSourceModel(model);
		proxy_model->setDynamicSortFilter(true);
		proxy_model->sort(ComponentPropModel::NAME_COL);
		proxy_model->setFilterKeyColumn(ComponentPropModel::NAME_COL);
		proxy_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
		view->setModel(proxy_model);

		model->refresh();
		view->hideColumn(ComponentPropModel::ID_COL);

		connect(filter_line, &QLineEdit::textChanged, this, [proxy_model](const QString& value){
			proxy_model->setFilterFixedString(value);
		});

		auto delete_row = new QAction(tr("Delete"), this);
		addAction(delete_row);
		delete_row->setShortcut(Qt::Key_Delete);
		delete_row->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(delete_row, &QAction::triggered, [view, model, proxy_model, &controller](bool){

			auto perform = [&]() -> Result<> {
				auto t = controller.createTransaction("Delete property");

				for (auto index : view->selectionModel()->selectedRows()) {
					QString id = proxy_model->data(proxy_model->index(index.row(), ComponentPropModel::ID_COL)).toString();

					auto res = t.deleteRow("component_prop", "id", id);
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

		setContextMenuPolicy(Qt::ActionsContextMenu);
	}

	ComponentEditor::~ComponentEditor() {
	}
}