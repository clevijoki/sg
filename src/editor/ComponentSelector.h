#pragma once

#include <QDialog>
#include <QVariant>

namespace sg {
	class ComponentSelector : public QDialog {
		Q_OBJECT

		class Transaction& mTransaction;
		QVariant mSelectedId;

	public:
		ComponentSelector(class Transaction& t, QWidget* parent=nullptr);
		~ComponentSelector();

		const QVariant& selectedId() const {
			return mSelectedId;
		}
	};
}
