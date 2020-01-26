#pragma once

#include <QWidget>

namespace sg {
	class EntityList : public QWidget {
		Q_OBJECT

	public:
		EntityList(class Controller& controller, QWidget* parent);
		~EntityList();

	signals:
		void opened(const QVariant& component_id);
	};
}
