#pragma once

#include <QWidget>

namespace sg {
	class EntityPropertyEditor : public QWidget {
		Q_OBJECT

	public:
		EntityPropertyEditor(class Controller& controller, QVariant component_id, QWidget* parent);
		~EntityPropertyEditor();
	};
}