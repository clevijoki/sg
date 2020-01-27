#pragma once

#include <QDialog>
#include <QVariant>

namespace sg {
	class EntitySelector : public QDialog {
		Q_OBJECT

		class Transaction& mTransaction;
		QVariant mSelectedId;

	public:
		EntitySelector(class Transaction& t, QWidget* parent=nullptr);
		~EntitySelector();

		const QVariant& selectedId() const {
			return mSelectedId;
		}
	};
}
