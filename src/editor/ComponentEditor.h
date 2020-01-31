#pragma once
#include "Types.h"

#include <QWidget>

namespace sg {
	class ComponentEditor : public QWidget {
		Q_OBJECT

	public:
		ComponentEditor(class Controller& controller, id_t component_id, QWidget* parent);
		~ComponentEditor();
	};
}