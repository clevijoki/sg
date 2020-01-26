#pragma once

#include <QWidget>

namespace sg {
	class EntityEditor : public QWidget {
		Q_OBJECT

	public:
		EntityEditor(class Controller& controller, QVariant component_id, QWidget* parent);
		~EntityEditor();
	};
}