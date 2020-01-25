#pragma once

#include <QWidget>

namespace sg {
	class ComponentMetaEditor : public QWidget {
		Q_OBJECT

	public:
		ComponentMetaEditor(class Controller& controller, QWidget* parent);
		~ComponentMetaEditor();

	signals:
		void opened(const QVariant& component_id);
	};
}