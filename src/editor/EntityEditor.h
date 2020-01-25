#pragma once

#include <QWidget>

namespace sg {
	class EntityEditor : public QWidget {
		Q_OBJECT

	public:
		EntityEditor(class Controller& controller, QWidget* parent);
		~EntityEditor();

	signals:
		void opened(const QVariant& component_id);
	};
}
