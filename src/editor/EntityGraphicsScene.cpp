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
#include <QGraphicsSceneMouseEvent>
#include <QLineEdit>
#include <QTransform>

#include "Controller.h"

namespace sg {


	const QFont PROP_FONT("Fixed", 12);
	const QFont TITLE_FONT("Fixed", 12, QFont::Bold);

	const float PIN_SIZE = 13;
	const QMargins PIN_BORDER(7,7,7,7);

	class ConnectionItem : public QGraphicsItem {
		class PinItem &mSource;
		class PinItem &mTarget;
	public:
		ConnectionItem(Controller &controller, PinItem &source, PinItem &target) 
		: mSource(source)
		, mTarget(target){
		}

		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) override;
	};

	class PreviewConnectionItem : public QGraphicsItem {
		class PinItem &mSource;
		QPointF mTarget;
	public:
		PreviewConnectionItem(Controller &controller, PinItem &source, QPointF target) 
		: mSource(source)
		, mTarget(target){
		}

		void setTarget(const QPointF target) {
			mTarget = target;
		}

		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) override;
	};

	class PinItem : public QGraphicsItem {

		Controller &mController;

		QRectF mBounds;
		QRectF mDrawBounds;
		QBrush mBrush;

		QVariant mId;
		QString mType;

	public:
		PinItem(Controller& controller, float size, QString type, QVariant id, QGraphicsItem *parent = nullptr)
		: QGraphicsItem(parent)
		, mController(controller)
		, mType(std::move(type))
		, mId(std::move(id)) {
			setSize(size);
			setAcceptHoverEvents(true);
			setPanelModality(QGraphicsItem::PanelModal);
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
			mBrush = Qt::red;
			update();
		}

		void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override {
			mBrush = Qt::darkGray;
			update();
		}

		void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
			event->accept();
		}

		void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
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
		enum class EState {
			Default,
			Dragging,
		};

		Controller& mController;

		QString mName, mComponentName;
		QVariant mComponentId, mId;

		QVector<ItemProperty> mProperties;
		QString mComponentNameTitle;

		QRectF mBounds;
		QRectF mComponentNameBounds;
		QRectF mNameBounds;
		PinItem *mRefPin;

		QLineEdit *mTitleEdit;
		QGraphicsProxyWidget *mTitleProxy;

		QPointF mAnchorPoint;
		EState mState = EState::Default;

		void setAnchorPoint(const QPointF& pt) {
			mAnchorPoint = pt;
		}

		void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override {
			mState = EState::Dragging;
			QGraphicsItem::mouseMoveEvent(event);			
		}

		void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override {
			if (mState == EState::Dragging) {
				qDebug() << "Apply drag to " << pos();
			} 

			mState = EState::Default;

			QGraphicsItem::mouseReleaseEvent(event);
		}

	public:
		ComponentEntityItem(Controller& controller, QString name, QString component_name, QVariant component_id, QVariant id, QGraphicsItem *parent = nullptr) 
		: QGraphicsItem(parent)
		, mController(controller)
		, mName(std::move(name)) 
		, mComponentName(std::move(component_name))
		, mComponentId(std::move(component_id))
		, mId(id)
		, mRefPin(new PinItem(controller, PIN_SIZE, "object_ref", std::move(id), this))
		, mTitleProxy(new QGraphicsProxyWidget(this))
		, mTitleEdit(new QLineEdit(mName)) {

			setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);

			mTitleProxy->setWidget(mTitleEdit);
			mTitleEdit->setFont(TITLE_FONT);

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

			mComponentNameBounds = title_fm.boundingRect(mComponentNameTitle).marginsAdded(QMargins(1,1,1,10));
			mNameBounds = title_fm.boundingRect(mName);

			mComponentNameBounds.moveTo(0,0);
			mNameBounds.moveTo(mComponentNameBounds.right(), 0);

			mTitleProxy->setGeometry(mNameBounds);

			float height = std::max(mComponentNameBounds.bottom(), mNameBounds.bottom());

			mRefPin->setSize(prop_fm.height());
			mRefPin->setPos(mNameBounds.right(), (height-prop_fm.height())/2);

			float width = mNameBounds.right() + prop_fm.height();

			const int row_count = m.rowCount();
			float pin_label_width = 0;

			for (int row = 0; row < row_count; ++row) {
				ItemProperty ip;
				ip.name = m.data(m.index(row, 0)).toString();
				ip.type = m.data(m.index(row, 1)).toString();
		
				ip.bounds = prop_fm.boundingRect(ip.name);
				ip.bounds.moveTo(prop_fm.height(), height);
				ip.pin = new PinItem(mController, prop_fm.height(), ip.type, m.data(m.index(row, 3)), this);
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
			return mBounds.marginsAdded(QMargins(3,3,3,3));
		}

		void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) override {
			painter->setBrush(Qt::NoBrush);
			painter->setPen(QPen(Qt::black, 0));
			painter->drawRect(mBounds.marginsAdded(QMargins(2,2,2,2)));

			painter->setPen(QPen(Qt::black, 0));
			painter->setBrush(Qt::white);
			painter->drawRect(mBounds);

			painter->setFont(TITLE_FONT);
			painter->drawText(mComponentNameBounds, Qt::AlignVCenter | Qt::AlignLeft, mComponentNameTitle);

			painter->setFont(PROP_FONT);

			for (const ItemProperty& ip : mProperties) {
				painter->drawText(ip.bounds, Qt::AlignLeft | Qt::AlignVCenter, ip.name);
			}
		}

		const QVariant& id() const {
			return mId;
		}

		const QVariant& componentId() const {
			return mComponentId;
		}


	};

	void ConnectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) {
		painter->setBrush(Qt::NoBrush);
		painter->setPen(QPen(Qt::blue, 3));

		painter->drawLine(mSource.pos(), mTarget.pos());
	}

	void PreviewConnectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget* widget) {
		painter->setBrush(Qt::NoBrush);
		painter->setPen(QPen(Qt::blue, 3));

		painter->drawLine(mSource.pos(), mTarget);
	}

	void PinItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
		event->accept();

		ComponentEntityItem *source_component = dynamic_cast<ComponentEntityItem*>(parentItem());

		if (QGraphicsItem* item = scene()->itemAt(event->scenePos(), QTransform())) {
			if (PinItem* target_pin = dynamic_cast<PinItem*>(item)) {
				if (ComponentEntityItem* target_component = dynamic_cast<ComponentEntityItem*>(target_pin->parentItem())) {
					if (target_component != source_component) {
						qDebug() << QString("Create new connection from id:%1 component_id:%2 property_id:%3 to id:%4 component_id:%5 property_id:%6")
							.arg(source_component->id().toString())
							.arg(source_component->componentId().toString())
							.arg(mId.toString())
							.arg(target_component->id().toString())
							.arg(target_component->componentId().toString())
							.arg(target_pin->mId.toString());
					}
				}
			}
		}
	}

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
		// all_nodes->setGraphicsEffect(dse);

		for (int row = 0; row < m.rowCount(); ++row) {
			ComponentEntityItem *ei = new ComponentEntityItem(
				controller,
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
