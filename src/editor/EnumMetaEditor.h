#pragma once

#include <QWidget>

namespace sg {
	class EnumMetaEditor : public QWidget {
		Q_OBJECT

	public:
		EnumMetaEditor(class Controller& controller, QWidget* parent);
		~EnumMetaEditor();

	signals:
		void opened(const QString& component);
	};
}