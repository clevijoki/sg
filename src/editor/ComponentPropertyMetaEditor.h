#pragma once

#include <QWidget>

namespace sg {
	class ComponentPropertyMetaEditor : public QWidget {
		Q_OBJECT

	public:
		ComponentPropertyMetaEditor(class Controller& controller, QVariant component_id, QWidget* parent);
		~ComponentPropertyMetaEditor();
	};
}