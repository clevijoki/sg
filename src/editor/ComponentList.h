#pragma once

#include <QWidget>

namespace sg {
	class ComponentList : public QWidget {
		Q_OBJECT

	public:
		ComponentList(class Controller& controller, QWidget* parent);
		~ComponentList();

	signals:
		void opened(const QVariant& component_id);
	};
}