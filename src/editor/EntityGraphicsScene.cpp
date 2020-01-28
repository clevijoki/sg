#include "EntityGraphicsScene.h"
#include <QPainter>
#include <QSqlQueryModel>
#include <QSqlError>
#include <QDebug>
#include <QFontMetrics>
#include <QFont>
#include <QGraphicsItemGroup>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsProxyWidget>
#include <QLineEdit>

#include "Controller.h"

namespace sg {


	const QFont PROP_FONT("Fixed", 12);
	const QFont TITLE_FONT("Fixed", 12, QFont::Bold);

	const float PIN_SIZE = 13;
	const QMargins PIN_BORDER(7,7,7,7);

	class PinItem : public QGraphicsItem {

		QRectF mBounds;
		QRectF mDrawBounds;
		QBrush mBrush;

	public:
		PinItem(float size, QGraphicsItem *parent)
		: QGraphicsItem(parent) {
			setSize(size);
			setAcceptHoverEvents(true);
			mBrush = Qt::darkGray;
		}

		void setSize(float size) {
			mBounds = QRect(0,0,size,size);
			mDrawBounds = mBounds.marginsRemoved(PIN_BORDER);
		}

		QRectF boundingRect() const override {
			return mBounds;
		}

		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) override {
			painter->setBrush(mBrush);
			painter->drawRect(mDrawBounds);
		}

		void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override {
			mBrush = Qt::gray;
			update();
		}

		void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override {
			mBrush = Qt::darkGray;
			update();
		}
	};

	struct ItemProperty {
		QString name;
		QString type;
		QString default_value;
		QVariant id;
		QRectF bounds;
		PinItem *pin = nullptr;
		QGraphicsProxyWidget *proxy = nullptr;
		QLineEdit *edit_widget = nullptr;
	};

	class ComponentEntityItem : public QGraphicsItem {
		QString mName, mComponentName;
		QVariant mComponentId, mId;

		QVector<ItemProperty> mProperties;
		QString mComponentNameTitle;

		QRectF mBounds;
		QRectF mComponentNameBounds;
		QRectF mNameBounds;
		PinItem *mRefPin;

	public:
		ComponentEntityItem(QString name, QString component_name, QVariant component_id, QVariant id) 
		: mName(std::move(name)) 
		, mComponentName(std::move(component_name))
		, mComponentId(std::move(component_id))
		, mId(std::move(id))
		, mRefPin(new PinItem(PIN_SIZE, this)) {

			// get all of our properties from the component_id
			refresh();
			setHandlesChildEvents(false);
		}

		void refresh() {
			QSqlQueryModel m;
			m.setQuery(QString("SELECT name, type, default_value, id FROM component_prop WHERE component_id = %1").arg(ToSqlLiteral(mComponentId)));


			QFontMetrics title_fm(TITLE_FONT);
			QFontMetrics prop_fm(PROP_FONT);

			mComponentNameTitle = QString("%1:").arg(mComponentName);

			mComponentNameBounds = title_fm.boundingRect(mComponentNameTitle).marginsAdded(QMargins(1,1,1,1));
			mNameBounds = title_fm.boundingRect(mName).marginsAdded(QMargins(1,1,1,1));

			mComponentNameBounds.moveTo(0,0);
			mNameBounds.moveTo(mComponentNameBounds.right(), 0);

			float height = std::max(mComponentNameBounds.bottom(), mNameBounds.bottom());

			mRefPin->setSize(height);
			mRefPin->setPos(mNameBounds.right(), 0);

			float width = mNameBounds.right() + height;

			const int row_count = m.rowCount();
			float pin_label_width = 0;

			for (int row = 0; row < row_count; ++row) {
				ItemProperty ip;
				ip.name = m.data(m.index(row, 0)).toString();
				ip.type = m.data(m.index(row, 1)).toString();
				ip.default_value = m.data(m.index(row, 2)).toString();
				ip.id = m.data(m.index(row, 3));
				ip.bounds = prop_fm.boundingRect(ip.name);
				ip.bounds.moveTo(prop_fm.height(), height);
				ip.pin = new PinItem(prop_fm.height(), this);
				ip.pin->setPos(0, height);

				ip.proxy = new QGraphicsProxyWidget(this);
				ip.edit_widget = new QLineEdit();
				ip.proxy->setWidget(ip.edit_widget);
				ip.edit_widget->setText(ip.default_value);

				width = std::max<float>(width, ip.bounds.right());
				pin_label_width = std::max<float>(pin_label_width, ip.bounds.right());
				height = ip.bounds.bottom();

				mProperties.push_back(std::move(ip));
			}

			pin_label_width += PIN_SIZE;

			width = std::max(width, pin_label_width+100);

			for (int row = 0; row < row_count; ++row) {

				ItemProperty& ip = mProperties[row];
				ip.edit_widget->setGeometry(QRectF(pin_label_width, ip.bounds.top(), width-pin_label_width, ip.bounds.height()).toRect());
			}

			mBounds = QRectF(0, 0, width, height).marginsAdded(QMargins(4,4,4,4));
		}

		QRectF boundingRect() const override {
			const qreal pen_width = 1;
			return mBounds;
		}

		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) override {
			painter->setBrush(Qt::NoBrush);
			painter->setPen(QPen(Qt::black, 0));
			painter->drawRect(mBounds.marginsAdded(QMargins(2,2,2,2)));

			painter->setPen(QPen(Qt::black, 0));
			painter->setBrush(Qt::white);
			painter->drawRect(mBounds);

			painter->setFont(TITLE_FONT);
			painter->drawText(mComponentNameBounds, Qt::AlignTop | Qt::AlignLeft, mComponentNameTitle);
			painter->drawText(mNameBounds, Qt::AlignTop | Qt::AlignLeft, mName);

			painter->setFont(PROP_FONT);
			for (const ItemProperty& ip : mProperties) {
				painter->drawText(ip.bounds, Qt::AlignLeft | Qt::AlignVCenter, ip.name);
			}
		}
	};
	
	static void Refresh(EntityGraphicsScene& egs, class Controller& controller, QVariant entity_id) {
		QSqlQueryModel m;
		m.setQuery(QString("SELECT entity_component.name, component.name, component_id, entity_component.id, entity_component.graph_pos FROM entity_component INNER JOIN component on component.id = component_id WHERE entity_id = %1").arg(ToSqlLiteral(entity_id)));

		if (m.lastError().isValid())
			qDebug() << m.lastError().text();

		QGraphicsItemGroup *all_nodes = egs.createItemGroup({});
		all_nodes->setHandlesChildEvents(false);
		auto dse = new QGraphicsDropShadowEffect();
		dse->setBlurRadius(32);
		dse->setColor(Qt::lightGray);
		all_nodes->setGraphicsEffect(dse);

		for (int row = 0; row < m.rowCount(); ++row) {
			ComponentEntityItem *ei = new ComponentEntityItem(
				m.data(m.index(row, 0)).toString(),
				m.data(m.index(row, 1)).toString(),
				m.data(m.index(row, 2)),
				m.data(m.index(row, 3))
			);
			ei->setGroup(all_nodes);

			QPointF pos = sg::ToQPointF(m.data(m.index(row, 4)));
			ei->setPos(pos);

			QPainterPath path;
			path.moveTo(pos - QPointF(200,200));
			path.cubicTo(pos - QPointF(100, 200), pos - QPointF(100, 0), pos);
			auto item = egs.addPath(path, QPen(Qt::black, 0));
			item->setGroup(all_nodes);

		}
	}

	EntityGraphicsScene::EntityGraphicsScene(class Controller& controller, QVariant entity_id, QObject *parent)
	: QGraphicsScene(parent) {
		Refresh(*this, controller, entity_id);

		connect(&controller, &Controller::dataChanged, this, [&, entity_id](const QSet<QString>& tables_affected){
			if (tables_affected.contains("entity_component")) {
				Refresh(*this, controller, entity_id);
			}
		});
	}
}
