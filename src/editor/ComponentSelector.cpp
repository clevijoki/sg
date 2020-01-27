#include "ComponentSelector.h"
#include <QListView>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QSortFilterProxyModel>
#include <QSqlQueryModel>

#include "Controller.h"

namespace sg {

	ComponentSelector::ComponentSelector(class Transaction& t, QWidget* parent) 
	: QDialog(parent)
	, mTransaction(t) {

		setWindowTitle(tr("Select Component"));
		
		auto layout = new QVBoxLayout();

		auto filter = new QLineEdit();
		filter->setPlaceholderText(tr("Search filter..."));
		layout->addWidget(filter);

		auto list_view = new QListView();
		layout->addWidget(list_view);

		auto model = new QSqlQueryModel(this);
		model->setQuery("SELECT name, id FROM component", *t.connection());
		auto proxy_model = new QSortFilterProxyModel(this);
		proxy_model->setSourceModel(model);
		proxy_model->setDynamicSortFilter(true);
		proxy_model->sort(0);
		proxy_model->setFilterKeyColumn(0);
		proxy_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
		list_view->setModel(proxy_model);

		setLayout(layout);

		connect(filter, &QLineEdit::textChanged, this, [proxy_model](const QString& value){
			proxy_model->setFilterFixedString(value);
		});

		connect(list_view, &QListView::doubleClicked, this, [this, proxy_model](const QModelIndex& index){
			this->mSelectedId = proxy_model->data(proxy_model->index(index.row(), 1));
			this->done(QDialog::Accepted);
		});
	}

	ComponentSelector::~ComponentSelector() {
	}

}