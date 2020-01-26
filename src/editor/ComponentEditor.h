#pragma once

#include <QWidget>

namespace sg {
	class ComponentEditor : public QWidget {
		Q_OBJECT

	public:
		ComponentEditor(class Controller& controller, QVariant component_id, QWidget* parent);
		~ComponentEditor();
	};
}