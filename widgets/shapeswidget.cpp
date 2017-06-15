﻿#include "shapeswidget.h"

#include "utils/calculaterect.h"
#include "utils/configsettings.h"

#include <cmath>

#include <QApplication>
#include <QPainter>
#include <QDebug>

#define LINEWIDTH(index) (index*2+3)

const int DRAG_BOUND_RADIUS = 8;
const int SPACING = 12;

using namespace utils;

ShapesWidget::ShapesWidget(QWidget *parent)
    : QFrame(parent),
      m_isMoving(false),
      m_isSelected(false),
      m_isShiftPressed(false),
      m_editing(false),
      m_shapesIndex(-1),
      m_selectedIndex(-1),
      m_menuController(new MenuController)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);

    m_penColor = colorIndexOf(ConfigSettings::instance()->value(
                                  "common", "color_index").toInt());

    connect(m_menuController, &MenuController::shapePressed,
                   this, &ShapesWidget::shapePressed);
    connect(m_menuController, &MenuController::saveBtnPressed,
            this, &ShapesWidget::saveBtnPressed);
    connect(ConfigSettings::instance(), &ConfigSettings::shapeConfigChanged,
            this, &ShapesWidget::updateSelectedShape);
}

ShapesWidget::~ShapesWidget() {}

void ShapesWidget::updateSelectedShape(const QString &group,
                                       const QString &key, int index) {
    qDebug() << "updateSelectedShapes" << m_selectedIndex << m_shapes.length();

    if (m_selectedIndex != -1 && m_selectedIndex < m_shapes.length()) {
        if (m_selectedShape.type == "arrow" && key != "color_index") {
            if (key == "arrow_linewidth_index" && !m_selectedShape.isStraight) {
                m_selectedShape.lineWidth = LINEWIDTH(index);
            } else if (key == "straightline_linewidth_index" && m_selectedShape.isStraight) {
                m_selectedShape.lineWidth = LINEWIDTH(index);
            }
        } else if (m_selectedShape.type == group && key == "linewidth_index") {
            m_selectedShape.lineWidth = LINEWIDTH(index);
        } else if (group == "text" && m_selectedShape.type == group && key == "color_index") {
            int tmpIndex = m_shapes[m_selectedIndex].index;
            if (m_editMap.contains(tmpIndex)) {
                m_editMap.value(tmpIndex)->setColor(colorIndexOf(index));
                m_editMap.value(tmpIndex)->update();
            }
        } else if (group == "text" && m_selectedShape.type == group && key == "fontsize")  {
            int tmpIndex = m_shapes[m_selectedIndex].index;
            if (m_editMap.contains(tmpIndex)) {
            m_editMap.value(tmpIndex)->setFontSize(index);
            m_editMap.value(tmpIndex)->update();
            }
        } else if (group != "text" && m_selectedShape.type == group && key == "color_index") {
            m_selectedShape.colorIndex = index;
        }

        if (m_selectedIndex < m_shapes.length())
        {
            m_shapes[m_selectedIndex] = m_selectedShape;
        }
        update();
    }
}

void ShapesWidget::updatePenColor() {
    setPenColor(colorIndexOf(ConfigSettings::instance()->value(
                                 "common", "color_index").toInt()));
}

void ShapesWidget::setCurrentShape(QString shapeType) {
    if (shapeType != "saveList")
        m_currentType = shapeType;
}

void ShapesWidget::setPenColor(QColor color) {
    int colorNum = colorIndex(color);
    m_penColor = color;
    if ( !m_currentType.isEmpty()) {
        qDebug() << "ShapesWidget setPenColor:" << m_currentType;
        ConfigSettings::instance()->setValue(m_currentType, "color_index", colorNum);
    }

    if (m_currentType != "line") {
        qApp->setOverrideCursor(setCursorShape(m_currentType));
    } else {
        qApp->setOverrideCursor(setCursorShape("line",  colorNum));
    }

    update();
}

void ShapesWidget::clearSelected() {
    for(int j = 0; j < m_selectedShape.mainPoints.length(); j++) {
        m_selectedShape.mainPoints[j] = QPointF(0, 0);
        m_hoveredShape.mainPoints[j] = QPointF(0, 0);
    }

    qDebug() << "clear selected!!!";
    m_isSelected = false;
    m_selectedShape.points.clear();
    m_hoveredShape.points.clear();
}

void ShapesWidget::setAllTextEditReadOnly() {
    QMap<int, TextEdit*>::iterator i = m_editMap.begin();
    while (i != m_editMap.end()) {
        i.value()->setReadOnly(true);
        i.value()->releaseKeyboard();
        ++i;
    }

    update();
}

void ShapesWidget::saveActionTriggered()
{
    qDebug() << "ShapesWidget saveActionTriggered!";
    setAllTextEditReadOnly();
    clearSelected();
    m_clearAllTextBorder = true;
}

bool ShapesWidget::clickedOnShapes(QPointF pos) {
    bool onShapes = false;
    m_selectedIndex = -1;

    qDebug() << "ClickedOnShapes !!!!!!!" << m_shapes.length();
    for (int i = 0; i < m_shapes.length(); i++) {
        bool currentOnShape = false;
        if (m_shapes[i].type == "rectangle") {
            if (clickedOnRect(m_shapes[i].mainPoints, pos,
                              m_shapes[i].isBlur || m_shapes[i].isMosaic)) {
                currentOnShape = true;
            }
        }
        if (m_shapes[i].type == "oval") {
            if (clickedOnEllipse(m_shapes[i].mainPoints, pos,
                                 m_shapes[i].isBlur || m_shapes[i].isMosaic)) {
                currentOnShape = true;
            }
        }
        if (m_shapes[i].type == "arrow") {
            if (clickedOnArrow(m_shapes[i].points, pos)) {
                currentOnShape = true;
            }
        }
        if (m_shapes[i].type == "line") {
            if (clickedOnLine(m_shapes[i].mainPoints, m_shapes[i].points, pos)) {
                currentOnShape = true;
            }
        }

        if (m_shapes[i].type == "text") {
            if (clickedOnText(m_shapes[i].mainPoints, pos)) {
                currentOnShape = true;
            }
        }

        if (currentOnShape) {
            m_selectedShape = m_shapes[i];
            m_selectedIndex = i;
            qDebug() << "currentOnShape" << i << m_selectedIndex;
            onShapes = true;
            break;
        } else {
            update();
            continue;
        }
    }
        return onShapes;
}

    //TODO: selectUnique
bool ShapesWidget::clickedOnRect(FourPoints rectPoints, QPointF pos, bool isBlurMosaic) {
    m_isSelected = false;
    m_isResize = false;
    m_isRotated = false;

    QPointF point1 = rectPoints[0];
    QPointF point2 = rectPoints[1];
    QPointF point3 = rectPoints[2];
    QPointF point4 = rectPoints[3];

    FourPoints otherFPoints = getAnotherFPoints(rectPoints);
    if (pointClickIn(point1, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = First;
        m_resizeDirection = TopLeft;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(point2, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Second;
        m_resizeDirection = BottomLeft;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(point3, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Third;
        m_resizeDirection = TopRight;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(point4, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fourth;
        m_resizeDirection = BottomRight;
        m_pressedPoint = pos;
        return true;
    }  else if (pointClickIn(otherFPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fifth;
        m_resizeDirection = Left;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[1], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Sixth;
        m_resizeDirection = Top;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[2], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Seventh;
        m_resizeDirection = Right;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[3], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Eighth;
        m_resizeDirection = Bottom;
        m_pressedPoint = pos;
        return true;
    } else if (rotateOnPoint(rectPoints, pos)) {
        m_isSelected = true;
        m_isRotated = true;
        m_isResize = false;
        m_resizeDirection = Rotate;
        m_pressedPoint = pos;
        return true;
    } else if (pointOnLine(rectPoints[0], rectPoints[1], pos) || pointOnLine(rectPoints[1],
        rectPoints[3], pos) || pointOnLine(rectPoints[3], rectPoints[2], pos) ||
        pointOnLine(rectPoints[2], rectPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = false;
        m_resizeDirection = Moving;
        m_pressedPoint = pos;
        return true;
    } else if(isBlurMosaic && pointInRect(rectPoints, pos)) {
        m_isSelected = true;
        m_isResize = false;
        m_resizeDirection = Moving;
        m_pressedPoint = pos;
        return true;
    }  else {
        m_isSelected = false;
        m_isResize = false;
        m_isRotated = false;
    }

    return false;
}

bool ShapesWidget::clickedOnEllipse(FourPoints mainPoints, QPointF pos, bool isBlurMosaic) {
    m_isSelected = false;
    m_isResize = false;
    m_isRotated = false;

    m_pressedPoint = pos;
    FourPoints otherFPoints = getAnotherFPoints(mainPoints);
    if (pointClickIn(mainPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = First;
        m_resizeDirection = TopLeft;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(mainPoints[1], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Second;
        m_resizeDirection = BottomLeft;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(mainPoints[2], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Third;
        m_resizeDirection = TopRight;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(mainPoints[3], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fourth;
        m_resizeDirection = BottomRight;
        m_pressedPoint = pos;
        return true;
    }  else if (pointClickIn(otherFPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fifth;
        m_resizeDirection = Left;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[1], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Sixth;
        m_resizeDirection = Top;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[2], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Seventh;
        m_resizeDirection = Right;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[3], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Eighth;
        m_resizeDirection = Bottom;
        m_pressedPoint = pos;
        return true;
    } else if (rotateOnPoint(mainPoints, pos)) {
        m_isSelected = true;
        m_isRotated = true;
        m_isResize = false;
        m_resizeDirection = Rotate;
        m_pressedPoint = pos;
        return true;
    }  else if (pointOnEllipse(mainPoints, pos)) {
            m_isSelected = true;
            m_isResize = false;

            m_resizeDirection = Moving;
            m_pressedPoint = pos;
            return true;
    } else if(isBlurMosaic && pointInRect(mainPoints, pos)) {
        m_isSelected = true;
        m_isResize = false;
        m_resizeDirection = Moving;
        m_pressedPoint = pos;
        return true;
    } else {
        m_isSelected = false;
        m_isResize = false;
        m_isRotated = false;
    }

    return false;
}

bool ShapesWidget::clickedOnArrow(QList<QPointF> points, QPointF pos) {
    if (points.length() != 2)
        return false;

    m_isSelected = false;
    m_isResize = false;
    m_isRotated = false;

    if (pointClickIn(points[0], pos)) {
        m_isSelected = true;
        m_isRotated = true;
        m_resizeDirection = Rotate;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(points[1], pos)) {
        m_isSelected = true;
        m_isRotated = true;
        m_resizeDirection = Rotate;
        m_pressedPoint = pos;
        return true;
    } else if (pointOnLine(points[0], points[1], pos)) {
        m_isSelected = true;
        m_isRotated = false;
        m_resizeDirection = Moving;
        m_pressedPoint = pos;
        return true;
    } else {
        m_isSelected = false;
        m_isRotated = false;
        m_isResize = false;
        m_resizeDirection = Outting;
        m_pressedPoint = pos;
        return false;
    }
}

bool ShapesWidget::clickedOnLine(FourPoints mainPoints,
                                      QList<QPointF> points, QPointF pos) {
    m_isSelected = false;
    m_isResize = false;
    m_isRotated = false;

    m_pressedPoint = QPoint(0, 0);
    FourPoints otherFPoints = getAnotherFPoints(mainPoints);
    if (pointClickIn(mainPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = First;
        m_resizeDirection = TopLeft;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(mainPoints[1], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Second;
        m_resizeDirection = BottomLeft;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(mainPoints[2], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Third;
        m_resizeDirection = TopRight;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(mainPoints[3], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fourth;
        m_resizeDirection = BottomRight;
        m_pressedPoint = pos;
        return true;
    }  else if (pointClickIn(otherFPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fifth;
        m_resizeDirection = Left;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[1], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Sixth;
        m_resizeDirection = Top;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[2], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Seventh;
        m_resizeDirection = Right;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[3], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Eighth;
        m_resizeDirection = Bottom;
        m_pressedPoint = pos;
        return true;
    } else if (rotateOnPoint(mainPoints, pos)) {
        m_isSelected = true;
        m_isRotated = true;
        m_isResize = false;
        m_resizeDirection = Rotate;
        m_pressedPoint = pos;
        return true;
    }  else if (pointOnArLine(points, pos)) {
            m_isSelected = true;
            m_isResize = false;

            m_resizeDirection = Moving;
            m_pressedPoint = pos;
            return true;
    } else {
        m_isSelected = false;
        m_isResize = false;
        m_isRotated = false;
    }

    return false;
}

bool ShapesWidget::clickedOnText(FourPoints mainPoints, QPointF pos) {
    if (pointInRect(mainPoints, pos)) {
        m_isSelected = true;
        m_isResize = false;
        m_resizeDirection = Moving;

        return true;
    } else {
        m_isSelected = false;
        m_isResize = false;

        return false;
    }
}

bool ShapesWidget::hoverOnRect(FourPoints rectPoints, QPointF pos) {
    FourPoints tmpFPoints = getAnotherFPoints(rectPoints);
    if (pointClickIn(rectPoints[0], pos)) {
        m_resizeDirection = TopLeft;
        return true;
    } else if (pointClickIn(rectPoints[1], pos)) {
        m_resizeDirection = BottomLeft;
        return true;
    } else if (pointClickIn(rectPoints[2], pos)) {
        m_resizeDirection = TopRight;
        return true;
    } else if (pointClickIn(rectPoints[3], pos)) {
        m_resizeDirection = BottomRight;
        return true;
    } else if (rotateOnPoint(rectPoints, pos)) {
        m_resizeDirection = Rotate;
        return true;
    }  else if (pointClickIn(tmpFPoints[0], pos)) {
        m_resizeDirection = Left;
        return true;
    } else if (pointClickIn(tmpFPoints[1], pos)) {
        m_resizeDirection = Top;
        return true;
    }  else if (pointClickIn(tmpFPoints[2], pos)) {
        m_resizeDirection = Right;
        return true;
    } else if (pointClickIn(tmpFPoints[3], pos)) {
        m_resizeDirection = Bottom;
        return true;
    } else if (pointOnLine(rectPoints[0],  rectPoints[1], pos) || pointOnLine(rectPoints[1],
        rectPoints[3], pos) || pointOnLine(rectPoints[3], rectPoints[2], pos) ||
               pointOnLine(rectPoints[2], rectPoints[0], pos)) {
        m_resizeDirection = Moving;
        return true;
    } else {
        m_resizeDirection = Outting;
    }
    return false;
}

bool ShapesWidget::hoverOnEllipse(FourPoints mainPoints, QPointF pos) {
    FourPoints tmpFPoints = getAnotherFPoints(mainPoints);

    if (pointClickIn(mainPoints[0], pos)) {
        m_resizeDirection = TopLeft;
        return true;
    } else if (pointClickIn(mainPoints[1], pos)) {
        m_resizeDirection = BottomLeft;
        return true;
    } else if (pointClickIn(mainPoints[2], pos)) {
        m_resizeDirection = TopRight;
        return true;
    } else if (pointClickIn(mainPoints[3], pos)) {
        m_resizeDirection = BottomRight;
        return true;
    } else if (rotateOnPoint(mainPoints, pos)) {
        m_resizeDirection = Rotate;
        return true;
    }  else if (pointClickIn(tmpFPoints[0], pos)) {
        m_resizeDirection = Left;
        return true;
    } else if (pointClickIn(tmpFPoints[1], pos)) {
        m_resizeDirection = Top;
        return true;
    }  else if (pointClickIn(tmpFPoints[2], pos)) {
        m_resizeDirection = Right;
        return true;
    } else if (pointClickIn(tmpFPoints[3], pos)) {
        m_resizeDirection = Bottom;
        return true;
    }  else if (pointOnEllipse(mainPoints, pos)) {
        m_isSelected = true;
        m_isResize = false;

        m_resizeDirection = Moving;
        m_pressedPoint = pos;
        return true;
   } else {
        m_resizeDirection = Outting;
    }
    return false;
}

bool ShapesWidget::hoverOnArrow(QList<QPointF> points, QPointF pos) {
    if (points.length() !=2)
        return false;

    if (pointClickIn(points[0], pos)) {
        m_clickedKey = First;
        m_resizeDirection = Rotate;
        return true;
    } else if (pointClickIn(points[1], pos)) {
        m_clickedKey =   Second;
        m_resizeDirection = Rotate;
        return true;
    } else if(pointOnLine(points[0], points[1], pos)) {
        m_resizeDirection = Moving;
        return true;
    } else {
        m_resizeDirection = Outting;
        return false;
    }
}

bool ShapesWidget::hoverOnLine(FourPoints mainPoints, QList<QPointF> points,
                               QPointF pos) {
    FourPoints tmpFPoints = getAnotherFPoints(mainPoints);

    if (pointClickIn(mainPoints[0], pos)) {
        m_resizeDirection = TopLeft;
        return true;
    } else if (pointClickIn(mainPoints[1], pos)) {
        m_resizeDirection = BottomLeft;
        return true;
    } else if (pointClickIn(mainPoints[2], pos)) {
        m_resizeDirection = TopRight;
        return true;
    } else if (pointClickIn(mainPoints[3], pos)) {
        m_resizeDirection = BottomRight;
        return true;
    } else if (rotateOnPoint(mainPoints, pos)) {
        m_resizeDirection = Rotate;
        return true;
    }  else if (pointClickIn(tmpFPoints[0], pos)) {
        m_resizeDirection = Left;
        return true;
    } else if (pointClickIn(tmpFPoints[1], pos)) {
        m_resizeDirection = Top;
        return true;
    }  else if (pointClickIn(tmpFPoints[2], pos)) {
        m_resizeDirection = Right;
        return true;
    } else if (pointClickIn(tmpFPoints[3], pos)) {
        m_resizeDirection = Bottom;
        return true;
    }  else if (pointOnArLine(points, pos)) {
        m_isSelected = true;
        m_isResize = false;

        m_resizeDirection = Moving;
        m_pressedPoint = pos;
        return true;
   } else {
        m_resizeDirection = Outting;
    }
    return false;
}
bool ShapesWidget::hoverOnShapes(Toolshape toolShape, QPointF pos) {
    if (toolShape.type == "rectangle") {
        return hoverOnRect(toolShape.mainPoints, pos);
    } else if (toolShape.type == "oval") {
        return hoverOnEllipse(toolShape.mainPoints, pos);
    }  else if (toolShape.type == "arrow") {
        return hoverOnArrow(toolShape.points, pos);
    } else if (toolShape.type == "line") {
        return hoverOnLine(toolShape.mainPoints, toolShape.points, pos);
    }

    m_hoveredShape.type = "";
    return false;
}

bool ShapesWidget::rotateOnPoint(FourPoints mainPoints,
                                 QPointF pos) {
    bool result = hoverOnRotatePoint(mainPoints, pos);
    return result;
}

bool ShapesWidget::hoverOnRotatePoint(FourPoints mainPoints,
                                      QPointF pos) {
    QPointF rotatePoint = getRotatePoint(mainPoints[0], mainPoints[1],
                                                                        mainPoints[2], mainPoints[3]);
    rotatePoint = QPointF(rotatePoint.x() - 5, rotatePoint.y() - 5);
    bool result = false;
    if (pos.x() >= rotatePoint.x() - SPACING && pos.x() <= rotatePoint.x() + SPACING
            && pos.y() >= rotatePoint.y() - SPACING && pos.y() <= rotatePoint.y() + SPACING) {
        result = true;
    } else {
        result = false;
    }

    m_pressedPoint = rotatePoint;
    return result;
}

void ShapesWidget::handleDrag(QPointF oldPoint, QPointF newPoint)  {
    qDebug() << "handleDrag:" << m_selectedIndex << m_shapes.length();

    if (m_selectedIndex == -1 || m_selectedIndex > m_shapes.length()) {
        return;
    }

    if (m_shapes[m_selectedIndex].type == "arrow") {
        for(int i = 0; i < m_shapes[m_selectedIndex].points.length(); i++) {
            m_shapes[m_selectedIndex].points[i] = QPointF(
                        m_shapes[m_selectedIndex].points[i].x() + (newPoint.x() - oldPoint.x()),
                        m_shapes[m_selectedIndex].points[i].y() + (newPoint.y() - oldPoint.y())
                        );
        }
        return;
    }

    if (m_shapes[m_selectedIndex].mainPoints.length() == 4) {
        for(int i = 0; i < m_shapes[m_selectedIndex].mainPoints.length(); i++) {
            m_shapes[m_selectedIndex].mainPoints[i] = QPointF(
                        m_shapes[m_selectedIndex].mainPoints[i].x() + (newPoint.x() - oldPoint.x()),
                        m_shapes[m_selectedIndex].mainPoints[i].y() + (newPoint.y() - oldPoint.y())
                        );
        }
    }
    for(int i = 0; i < m_shapes[m_selectedIndex].points.length(); i++) {
        m_shapes[m_selectedIndex].points[i] = QPointF(
                    m_shapes[m_selectedIndex].points[i].x() + (newPoint.x() - oldPoint.x()),
                    m_shapes[m_selectedIndex].points[i].y() + (newPoint.y() - oldPoint.y())
                    );
    }
}

////////////////////TODO: perfect handleRotate..
void ShapesWidget::handleRotate(QPointF pos) {
    qDebug() << "handleRotate:" << m_selectedIndex << m_shapes.length();

    if (m_selectedIndex == -1 || m_selectedIndex > m_shapes.length()
            || m_selectedShape.type == "text") {
        return;
    }

    if (m_selectedShape.type == "arrow") {
        if (m_shapes[m_selectedIndex].isShiftPressed) {
            if (m_shapes[m_selectedIndex].points[0].x() == m_shapes[m_selectedIndex].points[1].x()) {
                if (m_clickedKey == First) {
                    m_shapes[m_selectedIndex].points[0] = QPointF(m_shapes[m_selectedIndex].points[1].x(),
                            pos.y());
                } else if (m_clickedKey == Second) {
                    m_shapes[m_selectedIndex].points[1] = QPointF(m_shapes[m_selectedIndex].points[0].x(),
                            pos.y());
                }
            } else {
                if (m_clickedKey == First) {
                    m_shapes[m_selectedIndex].points[0] = QPointF(pos.x(), m_shapes[m_selectedIndex].points[1].y());
                } else if (m_clickedKey == Second) {
                    m_shapes[m_selectedIndex].points[1] = QPointF(pos.x(), m_shapes[m_selectedIndex].points[0].y());
                }
            }
        } else {
            if (m_clickedKey == First) {
                m_shapes[m_selectedIndex].points[0] = m_pressedPoint;
            } else if (m_clickedKey == Second) {
                m_shapes[m_selectedIndex].points[1] = m_pressedPoint;
            }
        }

        m_selectedShape.points  =  m_shapes[m_selectedIndex].points;
        m_hoveredShape.points = m_shapes[m_selectedIndex].points;
        m_pressedPoint = pos;
        return;
    }

    QPointF centerInPoint = QPointF((m_selectedShape.mainPoints[0].x() +
                                                                 m_selectedShape.mainPoints[3].x())/2,
                                                                 (m_selectedShape.mainPoints[0].y()+
                                                                 m_selectedShape.mainPoints[3].y())/2);
    qreal angle = calculateAngle(m_pressedPoint, pos, centerInPoint)/35;

    for (int i = 0; i < 4; i++) {
        m_shapes[m_selectedIndex].mainPoints[i] = pointRotate(centerInPoint,
                                                              m_shapes[m_selectedIndex].mainPoints[i], angle);
    }

    for(int k = 0; k < m_shapes[m_selectedIndex].points.length(); k++) {
        m_shapes[m_selectedIndex].points[k] = pointRotate(centerInPoint,
                                                              m_shapes[m_selectedIndex].points[k], angle);
    }

    m_selectedShape.mainPoints = m_shapes[m_selectedIndex].mainPoints;
    m_hoveredShape.mainPoints =  m_shapes[m_selectedIndex].mainPoints;
    m_pressedPoint = pos;
}

void ShapesWidget::handleResize(QPointF pos, int key) {
    qDebug() << "handleResize:" << m_selectedIndex << m_shapes.length();

    if (m_isResize && m_selectedIndex != -1 && m_selectedIndex < m_shapes.length()) {
        if (m_shapes[m_selectedIndex].portion.isEmpty()) {
            for(int k = 0; k < m_shapes[m_selectedIndex].points.length(); k++) {
                m_shapes[m_selectedIndex].portion.append(relativePosition(
                m_selectedShape.mainPoints, m_selectedShape.points[k]));
            }
        }

        FourPoints newResizeFPoints = resizePointPosition(
            m_shapes[m_selectedIndex].mainPoints[0],
            m_shapes[m_selectedIndex].mainPoints[1],
            m_shapes[m_selectedIndex].mainPoints[2],
            m_shapes[m_selectedIndex].mainPoints[3], pos, key,
            m_isShiftPressed);

       qDebug() << "handleResize:" << m_selectedIndex <<  m_isShiftPressed;
        m_shapes[m_selectedIndex].mainPoints = newResizeFPoints;
        m_selectedShape.mainPoints = newResizeFPoints;
        m_hoveredShape.mainPoints = newResizeFPoints;

        for (int j = 0; j <  m_shapes[m_selectedIndex].portion.length(); j++) {
              m_shapes[m_selectedIndex].points[j] =
                      getNewPosition(m_shapes[m_selectedIndex].mainPoints,
                                     m_shapes[m_selectedIndex].portion[j]);
        }

        m_selectedShape.points = m_shapes[m_selectedIndex].points;
        m_hoveredShape.points = m_shapes[m_selectedIndex].points;
    }
    m_pressedPoint = pos;
}

void ShapesWidget::mousePressEvent(QMouseEvent *e) {
    if (m_selectedIndex != -1 && m_selectedIndex < m_shapes.length()) {
        if (!(clickedOnShapes(e->pos()) && m_isRotated)) {
            clearSelected();
            setAllTextEditReadOnly();
            m_editing = false;
            m_selectedIndex = -1;
            m_selectedShape.type = "";
            update();
            QFrame::mousePressEvent(e);
            return;
        }
    }

    if (e->button() == Qt::RightButton) {
        qDebug() << "RightButton clicked!";
        m_menuController->showMenu(QPoint(mapToParent(e->pos())));
        QFrame::mousePressEvent(e);
        return;
    }

    m_pressedPoint = e->pos();
    m_isPressed = true;
    qDebug() << "mouse pressed!" << m_pressedPoint;

    if (!clickedOnShapes(m_pressedPoint)) {
        m_isRecording = true;
        qDebug() << "no one shape be clicked!" << m_selectedIndex << m_shapes.length();

        m_currentShape.type = m_currentType;
        m_currentShape.colorIndex = ConfigSettings::instance()->value(
                    m_currentType, "color_index").toInt();
        m_currentShape.lineWidth = LINEWIDTH(ConfigSettings::instance()->value(
                   m_currentType, "linewidth_index").toInt());

        m_selectedIndex = -1;
        m_shapesIndex += 1;
        m_currentIndex = m_shapesIndex;

        if (m_pos1 == QPointF(0, 0)) {
            m_pos1 = e->pos();
            if (m_currentType == "line") {
                m_currentShape.points.append(m_pos1);
            } else if (m_currentType == "arrow") {
                m_currentShape.isShiftPressed = m_isShiftPressed;
                m_currentShape.points.append(m_pos1);
                m_currentShape.isStraight = ConfigSettings::instance()->value(
                                                                    "arrow", "is_straight").toBool();
                if (m_currentShape.isStraight) {
                    m_currentShape.lineWidth = LINEWIDTH(ConfigSettings::instance()->value(
                                                             "arrow", "straightline_linewidth_index").toInt());
                } else {
                    m_currentShape.lineWidth = LINEWIDTH(ConfigSettings::instance()->value(
                                                             "arrow", "arrow_linewidth_index").toInt());
                }
            } else if (m_currentType == "rectangle" || m_currentType == "oval") {
                m_currentShape.isBlur = ConfigSettings::instance()->value(
                                                              "effect", "is_blur").toBool();
                m_currentShape.isMosaic = ConfigSettings::instance()->value(
                                                                  "effect", "is_mosaic").toBool();
                m_currentShape.isShiftPressed = m_isShiftPressed;
                if (m_currentShape.isBlur && !m_blurEffectExist) {
                    emit reloadEffectImg("blur");
                    m_blurEffectExist = true;
                } else if (m_currentShape.isMosaic &&  !m_mosaicEffectExist){
                    emit reloadEffectImg("mosaic");
                    m_mosaicEffectExist = true;
                }
            } else if (m_currentType == "text") {
                if (!m_editing) {
                    setAllTextEditReadOnly();
                    m_currentShape.mainPoints[0] = m_pos1;
                    m_currentShape.index = m_currentIndex;
                    qDebug() << "new textedit:" << m_currentIndex;
                    TextEdit* edit = new TextEdit(m_currentIndex, this);
                    m_editing = true;
                    int defaultFontSize = ConfigSettings::instance()->value("text", "fontsize").toInt();
                    m_currentShape.fontSize = defaultFontSize;
                    edit->setFocus();
                    edit->move(m_pos1.x(), m_pos1.y());
                    edit->show();
                    m_currentShape.mainPoints[0] = m_pos1;
                    m_currentShape.mainPoints[1] = QPointF(m_pos1.x(), m_pos1.y() + edit->height());
                    m_currentShape.mainPoints[2] = QPointF(m_pos1.x() + edit->width(), m_pos1.y());
                    m_currentShape.mainPoints[3] = QPointF(m_pos1.x() + edit->width(),
                                                           m_pos1.y() + edit->height());
                    m_editMap.insert(m_currentIndex, edit);
                    m_selectedShape = m_currentShape;
                    connect(edit, &TextEdit::repaintTextRect, this, &ShapesWidget::updateTextRect);
                    connect(edit, &TextEdit::backToEditing, this, [=]{
                        m_editing = true;
                    });
                    connect(edit, &TextEdit::textEditSelected, this, [=](int index){
                        for (int k = 0; k < m_shapes.length(); k++) {
                            if (m_shapes[k].type == "text" && m_shapes[k].index == index) {
                                m_selectedIndex = k;
                                m_selectedShape = m_shapes[k];
                                break;
                            }
                        }
                    });
                    m_shapes.append(m_currentShape);
                    qDebug() << "Insert text shape:" << m_currentShape.index;
                } else {
                    m_editing = false;
                    setAllTextEditReadOnly();
                }
            }
            update();
        }
    } else {
        m_isRecording = false;
        qDebug() << "some on shape be clicked!";
        if (m_editing && m_editMap.contains(m_shapes[m_selectedIndex].index)) {
            m_editMap.value(m_shapes[m_selectedIndex].index)->setReadOnly(true);
            m_editMap.value(m_shapes[m_selectedIndex].index)->setCursorVisible(false);
            m_editMap.value(m_shapes[m_selectedIndex].index)->setFocusPolicy(Qt::NoFocus);
        }
        update();
    }

    QFrame::mousePressEvent(e);
}

void ShapesWidget::mouseReleaseEvent(QMouseEvent *e) {
    m_isPressed = false;
    m_isMoving = false;

    qDebug() << m_isRecording << m_isSelected << m_pos2;

    if (m_isRecording && !m_isSelected && m_pos2 != QPointF(0, 0)) {
        if (m_currentType == "arrow") {
            if (m_currentShape.points.length() == 2) {
                if (m_isShiftPressed) {
                    if (std::atan2(std::abs(m_pos2.y() - m_pos1.y()), std::abs(m_pos2.x() - m_pos1.x()))
                            *180/M_PI < 45) {
                        m_pos2 = QPointF(m_pos2.x(), m_pos1.y());
                    } else {
                        m_pos2 = QPointF(m_pos1.x(), m_pos2.y());
                    }
                }

                m_currentShape.points[1] = m_pos2;
                m_currentShape.mainPoints = getMainPoints(m_currentShape.points[0], m_currentShape.points[1]);
                m_shapes.append(m_currentShape);
            }
        } else if (m_currentType == "line") {
            FourPoints lineFPoints = fourPointsOfLine(m_currentShape.points);
            m_currentShape.mainPoints = lineFPoints;
            m_shapes.append(m_currentShape);
        } else if (m_currentType != "text"){
            FourPoints rectFPoints = getMainPoints(m_pos1, m_pos2, m_isShiftPressed);
            m_currentShape.mainPoints = rectFPoints;
            m_shapes.append(m_currentShape);
        }

        qDebug() << "ShapesWidget num:" << m_shapes.length();
        clearSelected();
    }

    m_isRecording = false;
    if (m_currentShape.type != "text") {
        for(int i = 0; i < m_currentShape.mainPoints.length(); i++) {
            m_currentShape.mainPoints[i] = QPointF(0, 0);
        }
    }

    m_currentShape.points.clear();
    m_pos1 = QPointF(0, 0);
    m_pos2 = QPointF(0, 0);

    update();
    QFrame::mouseReleaseEvent(e);
}

void ShapesWidget::mouseMoveEvent(QMouseEvent *e) {
    m_isMoving = true;
    m_movingPoint = e->pos();

    if (m_isRecording && m_isPressed) {
        m_pos2 = e->pos();

        if (m_currentShape.type == "arrow") {
            if (m_currentShape.points.length() <= 1) {
                if (m_isShiftPressed) {
                        if (std::atan2(std::abs(m_pos2.y() - m_pos1.y()),
                                       std::abs(m_pos2.x() - m_pos1.x()))*180/M_PI < 45) {
                            m_currentShape.points.append(QPointF(m_pos2.x(), m_pos1.y()));
                        } else {
                            m_currentShape.points.append(QPointF(m_pos1.x(), m_pos2.y()));
                        }
                } else {
                    m_currentShape.points.append(m_pos2);
                }
            } else {
                if (m_isShiftPressed) {
                    if (std::atan2(std::abs(m_pos2.y() - m_pos1.y()),
                                   std::abs(m_pos2.x() - m_pos1.x()))*180/M_PI < 45) {
                        m_currentShape.points[1] = QPointF(m_pos2.x(), m_pos1.y());
                    } else {
                        m_currentShape.points[1] = QPointF(m_pos1.x(), m_pos2.y());
                    }
                } else {
                    m_currentShape.points[1] = m_pos2;
                }
            }
        }
        if (m_currentShape.type == "line") {
             m_currentShape.points.append(m_pos2);
        }
        update();
    } else if (!m_isRecording && m_isPressed) {
        if (m_isRotated && m_isPressed) {
            handleRotate(e->pos());
            update();
        }

        if (m_isResize && m_isPressed) {
            // resize function
            handleResize(QPointF(e->pos()), m_clickedKey);
            update();
            QFrame::mouseMoveEvent(e);
            return;
        }

        if (m_isSelected && m_isPressed && m_selectedIndex != -1) {
            handleDrag(m_pressedPoint, m_movingPoint);
            m_selectedShape = m_shapes[m_selectedIndex];
            m_hoveredShape = m_shapes[m_selectedIndex];

            m_pressedPoint = m_movingPoint;
            update();
        }
    } else {
        if (!m_isRecording) {
            m_isHovered = false;
            for (int i = 0; i < m_shapes.length(); i++) {
                 if (hoverOnShapes(m_shapes[i],  e->pos())) {
                     m_isHovered = true;
                     m_hoveredShape = m_shapes[i];
                     if (m_resizeDirection == Left) {
                         if (m_isSelected || m_isRotated) {
                            qApp->setOverrideCursor(Qt::SizeHorCursor);
                         } else {
                            qApp->setOverrideCursor(Qt::ClosedHandCursor);
                         }
                     } else if (m_resizeDirection == Top) {
                         if (m_isSelected || m_isRotated) {
                            qApp->setOverrideCursor(Qt::SizeVerCursor);
                         } else {
                            qApp->setOverrideCursor(Qt::ClosedHandCursor);
                         }
                     } else if (m_resizeDirection == Right) {
                         if (m_isSelected || m_isRotated) {
                            qApp->setOverrideCursor(Qt::SizeHorCursor);
                         } else {
                            qApp->setOverrideCursor(Qt::ClosedHandCursor);
                         }
                     } else if (m_resizeDirection == Bottom) {
                         if (m_isSelected || m_isRotated) {
                            qApp->setOverrideCursor(Qt::SizeVerCursor);
                         } else {
                            qApp->setOverrideCursor(Qt::ClosedHandCursor);
                         }
                     }  else if (m_resizeDirection == TopLeft) {
                         if (m_isSelected || m_isRotated) {
                            qApp->setOverrideCursor(Qt::SizeFDiagCursor);
                         } else {
                            qApp->setOverrideCursor(Qt::ClosedHandCursor);
                         }
                     } else if (m_resizeDirection == BottomLeft) {
                         if (m_isSelected || m_isRotated) {
                            qApp->setOverrideCursor(Qt::SizeBDiagCursor);
                         } else {
                            qApp->setOverrideCursor(Qt::ClosedHandCursor);
                         }
                     } else if (m_resizeDirection == TopRight) {
                         if (m_isSelected || m_isRotated) {
                            qApp->setOverrideCursor(Qt::SizeBDiagCursor);
                         } else {
                            qApp->setOverrideCursor(Qt::ClosedHandCursor);
                         }
                     } else if (m_resizeDirection == BottomRight) {
                         if (m_isSelected || m_isRotated) {
                            qApp->setOverrideCursor(Qt::SizeFDiagCursor);
                         } else {
                            qApp->setOverrideCursor(Qt::ClosedHandCursor);
                         }
                     } else if (m_resizeDirection == Rotate) {
                         qApp->setOverrideCursor(setCursorShape("rotate"));
                     } else if (m_resizeDirection == Moving) {
                         qApp->setOverrideCursor(Qt::ClosedHandCursor);
                     } else {
                         if (m_currentType == "line") {
                             qApp->setOverrideCursor(setCursorShape(m_currentType, colorIndex(m_penColor)));
                         } else {
                             qApp->setOverrideCursor(setCursorShape(m_currentType));
                         }
                     }
                     update();
                     break;
                 } else {
                     if (m_currentType == "line") {
                         qApp->setOverrideCursor(setCursorShape(m_currentType, colorIndex(m_penColor)));
                     } else {
                         qApp->setOverrideCursor(setCursorShape(m_currentType));
                     }
                     update();
                 }
            }
            if (!m_isHovered) {
                for(int j = 0; j < m_hoveredShape.mainPoints.length(); j++) {
                    m_hoveredShape.mainPoints[j] = QPointF(0, 0);
                }
                m_hoveredShape.type = "";
                update();
            }
            if (m_shapes.length() == 0) {
                if (m_currentType == "line") {
                    qApp->setOverrideCursor(setCursorShape(m_currentType, colorIndex(m_penColor)));
                } else {
                    qApp->setOverrideCursor(setCursorShape(m_currentType));
                }
            }
        } else {
            //TODO text
        }
    }

    QFrame::mouseMoveEvent(e);
}

void ShapesWidget::updateTextRect(TextEdit* edit, QRectF newRect) {
    int index = edit->getIndex();
    qDebug() << "updateTextRect:" << newRect << index;
    for (int j = 0; j < m_shapes.length(); j++) {
        qDebug() << "updateTextRect bbbbbbbb:" << j << m_shapes[j].index << index;
        if (m_shapes[j].type == "text" && m_shapes[j].index == index) {
            m_shapes[j].mainPoints[0] = QPointF(newRect.x(), newRect.y());
            m_shapes[j].mainPoints[1] = QPointF(newRect.x() , newRect.y() + newRect.height());
            m_shapes[j].mainPoints[2] = QPointF(newRect.x() + newRect.width(), newRect.y());
            m_shapes[j].mainPoints[3] = QPointF(newRect.x() + newRect.width(),
                                                                                         newRect.y() + newRect.height());
            m_currentShape = m_shapes[j];
            m_selectedShape = m_shapes[j];
            m_selectedIndex = j;
        }
    }
    update();
}

void ShapesWidget::paintImgPoint(QPainter &painter, QPointF pos, QPixmap img, bool isResize) {
        if (isResize) {
                painter.drawPixmap(QPoint(pos.x() - DRAG_BOUND_RADIUS,
                                  pos.y() - DRAG_BOUND_RADIUS), img);
        } else {
            painter.drawPixmap(QPoint(pos.x() - 12,
                              pos.y() - 12), img);
        }
}

void ShapesWidget::paintRect(QPainter &painter, FourPoints rectFPoints, int index,
                                                       bool isBlur, bool isMosaic) {
    QPainterPath rectPath;
    if ((isBlur | isMosaic) && index != m_selectedIndex) {
        painter.setPen(Qt::transparent);
    }
    rectPath.moveTo(rectFPoints[0].x(), rectFPoints[0].y());
    rectPath.lineTo(rectFPoints[1].x(),rectFPoints[1].y());
    rectPath.lineTo(rectFPoints[3].x(),rectFPoints[3].y());
    rectPath.lineTo(rectFPoints[2].x(),rectFPoints[2].y());
    rectPath.lineTo(rectFPoints[0].x(),rectFPoints[0].y());
    painter.drawPath(rectPath);

    using namespace utils;
    if (isBlur) {
        painter.setClipPath(rectPath);
        painter.drawPixmap(0, 0,  width(), height(),  TMP_BLUR_FILE);
        painter.drawPath(rectPath);
    }
    if (isMosaic) {
        painter.setClipPath(rectPath);
        painter.drawPixmap(0, 0,  width(), height(),  TMP_MOSA_FILE);
        painter.drawPath(rectPath);
    }
    painter.setClipping(false);
}

void ShapesWidget::paintEllipse(QPainter &painter, FourPoints ellipseFPoints, int index,
                                                           bool isBlur, bool isMosaic) {
    if ((isBlur | isMosaic)  && index != m_selectedIndex) {
        painter.setPen(Qt::transparent);
    }

    FourPoints minorPoints = getAnotherFPoints(ellipseFPoints);
    QList<QPointF> eightControlPoints = getEightControlPoint(ellipseFPoints);
    QPainterPath ellipsePath;
    ellipsePath.moveTo(minorPoints[0].x(), minorPoints[0].y());
    ellipsePath.cubicTo(eightControlPoints[0], eightControlPoints[1], minorPoints[1]);
    ellipsePath.cubicTo(eightControlPoints[4], eightControlPoints[5], minorPoints[2]);
    ellipsePath.cubicTo(eightControlPoints[6], eightControlPoints[7], minorPoints[3]);
    ellipsePath.cubicTo(eightControlPoints[3], eightControlPoints[2], minorPoints[0]);
    painter.drawPath(ellipsePath);

    using namespace utils;
    if (isBlur) {
        painter.setClipPath(ellipsePath);
        painter.drawPixmap(0, 0,  width(), height(),  TMP_BLUR_FILE);
        painter.drawPath(ellipsePath);
    }
    if (isMosaic) {
        painter.setClipPath(ellipsePath);
        painter.drawPixmap(0, 0,  width(), height(),  TMP_MOSA_FILE);
        painter.drawPath(ellipsePath);
    }
    painter.setClipping(false);
}

void ShapesWidget::paintArrow(QPainter &painter, QList<QPointF> lineFPoints,
                                                          int lineWidth, bool isStraight) {
    if (lineFPoints.length() == 2) {
        if (!isStraight) {
            QList<QPointF> arrowPoints = pointOfArrow(lineFPoints[0],
                                                         lineFPoints[1], 8+(lineWidth - 1)*2);
            QPainterPath path;
            const QPen oldPen = painter.pen();
            if (arrowPoints.length() >=3) {
                painter.drawLine(lineFPoints[0], lineFPoints[1]);
                path.moveTo(arrowPoints[2].x(), arrowPoints[2].y());
                path.lineTo(arrowPoints[0].x(), arrowPoints[0].y());
                path.lineTo(arrowPoints[1].x(), arrowPoints[1].y());
                path.lineTo(arrowPoints[2].x(), arrowPoints[2].y());
            }
            painter.setPen (Qt :: NoPen);
            painter.fillPath(path, QBrush(oldPen.color()));
        } else {
            painter.drawLine(lineFPoints[0], lineFPoints[1]);
        }
    }
}

void ShapesWidget::paintLine(QPainter &painter, QList<QPointF> lineFPoints) {
    for (int k = 0; k < lineFPoints.length() - 2; k++) {
        painter.drawLine(lineFPoints[k], lineFPoints[k+1]);
    }
}

void ShapesWidget::paintText(QPainter &painter, FourPoints rectFPoints) {
    QPen textPen;
    textPen.setStyle(Qt::DashLine);
    textPen.setColor("#01bdff");
    painter.setPen(textPen);

    if (rectFPoints.length() >= 4) {
        painter.drawLine(rectFPoints[0], rectFPoints[1]);
        painter.drawLine(rectFPoints[1], rectFPoints[3]);
        painter.drawLine(rectFPoints[3], rectFPoints[2]);
        painter.drawLine(rectFPoints[2], rectFPoints[0]);
    }
}

void ShapesWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing);
    QPen pen;

    for(int i = 0; i < m_shapes.length(); i++) {
        pen.setColor(colorIndexOf(m_shapes[i].colorIndex));
        pen.setWidth(m_shapes[i].lineWidth);
        painter.setPen(pen);
        if (m_shapes[i].type == "rectangle") {
            paintRect(painter, m_shapes[i].mainPoints, i, m_shapes[i].isBlur, m_shapes[i].isMosaic);
        } else if (m_shapes[i].type == "oval") {
            paintEllipse(painter, m_shapes[i].mainPoints, i, m_shapes[i].isBlur, m_shapes[i].isMosaic);
        } else if (m_shapes[i].type == "arrow") {
            paintArrow(painter, m_shapes[i].points, pen.width(), m_shapes[i].isStraight);
        } else if (m_shapes[i].type == "line") {
            paintLine(painter, m_shapes[i].points);
        } else if (m_shapes[i].type == "text" && !m_clearAllTextBorder) {
            qDebug() << "*&^" << m_shapes[i].type << m_shapes[i].index << m_selectedIndex << i;
            QMap<int, TextEdit*>::iterator m = m_editMap.begin();
            while (m != m_editMap.end()) {
                if (m.key() == m_shapes[i].index) {
                    if (!(m.value()->isReadOnly() && m_selectedIndex != i)) {
                        paintText(painter, m_shapes[i].mainPoints);
                    }
                    break;
                }
                m++;
            }
        }
    }

    if ((m_pos1 != QPointF(0, 0) && m_pos2 != QPointF(0, 0)) || m_currentShape.type == "text") {
        FourPoints currentFPoint =  getMainPoints(m_pos1, m_pos2, m_isShiftPressed);
        pen.setColor(colorIndexOf(m_currentShape.colorIndex));
        pen.setWidth(m_currentShape.lineWidth);
        painter.setPen(pen);
        if (m_currentType == "rectangle") {
            paintRect(painter, currentFPoint, m_shapes.length(), m_currentShape.isBlur, m_currentShape.isMosaic);
        } else if (m_currentType == "oval") {
            paintEllipse(painter, currentFPoint, m_shapes.length(), m_currentShape.isBlur, m_currentShape.isMosaic);
        } else if (m_currentType == "arrow") {
            paintArrow(painter, m_currentShape.points, pen.width(), m_currentShape.isStraight);
        } else if (m_currentType == "line") {
            paintLine(painter, m_currentShape.points);
        } else if (m_currentType == "text" && !m_clearAllTextBorder) {
            if (m_editing) {
                paintText(painter, m_currentShape.mainPoints);
            }
        }
    }

    if (m_hoveredShape.mainPoints[0] != QPointF(0, 0) ||
            m_hoveredShape.points.length()!=0) {
        pen.setWidth(1);
        pen.setColor("#01bdff");
        painter.setPen(pen);
        if (m_hoveredShape.type == "rectangle") {
            paintRect(painter, m_hoveredShape.mainPoints,
                              false, false);
        } else if (m_hoveredShape.type == "oval") {
            paintEllipse(painter, m_hoveredShape.mainPoints, -1);
        } else if (m_hoveredShape.type == "arrow") {
            paintArrow(painter, m_hoveredShape.points, pen.width(), true);
        } else if (m_hoveredShape.type == "line") {
            paintLine(painter, m_hoveredShape.points);
        }
    } else {
        qDebug() << "hoveredShape.type:" << m_hoveredShape.type;
    }

    QPixmap resizePointImg(":/resources/images/size/resize_handle_big.png");
    if (m_selectedShape.type == "arrow" && m_selectedShape.points.length() == 2) {
        paintImgPoint(painter, m_selectedShape.points[0], resizePointImg);
        paintImgPoint(painter, m_selectedShape.points[1], resizePointImg);
    } else if (m_selectedShape.type != "text") {
        if (m_selectedShape.mainPoints[0] != QPointF(0, 0) || m_selectedShape.type == "arrow") {

            QPointF rotatePoint = getRotatePoint(m_selectedShape.mainPoints[0],
                    m_selectedShape.mainPoints[1], m_selectedShape.mainPoints[2],
                    m_selectedShape.mainPoints[3]);
            QPointF middlePoint((m_selectedShape.mainPoints[0].x() +
                    m_selectedShape.mainPoints[2].x())/2,
                    (m_selectedShape.mainPoints[0].y() +
                    m_selectedShape.mainPoints[2].y())/2);

            painter.setPen(QColor("#01bdff"));
            painter.drawLine(rotatePoint, middlePoint);
            QPixmap rotatePointImg(":/resources/images/size/rotate.png");
            paintImgPoint(painter, rotatePoint, rotatePointImg, false);

            for ( int i = 0; i < m_selectedShape.mainPoints.length(); i ++) {
                paintImgPoint(painter, m_selectedShape.mainPoints[i], resizePointImg);
            }

            FourPoints anotherFPoints = getAnotherFPoints(m_selectedShape.mainPoints);
            for (int j = 0; j < anotherFPoints.length(); j++) {
                paintImgPoint(painter, anotherFPoints[j], resizePointImg);
            }

            if (m_selectedShape.type == "oval" || m_selectedShape.type == "line") {
                paintRect(painter,  m_selectedShape.mainPoints, -1);
            }
        }
    }
}

void ShapesWidget::enterEvent(QEvent *e) {
    Q_UNUSED(e);
    if (m_currentType != "line") {
        qApp->setOverrideCursor(setCursorShape(m_currentType));
    } else {
        qApp->setOverrideCursor(setCursorShape("line",  colorIndex(m_penColor)));
    }
}

//bool ShapesWidget::eventFilter(QObject *watched, QEvent *event) {
//    Q_UNUSED(watched);
//    if (event->type() == QEvent::Enter) {
//        if (m_currentType != "line") {
//            qApp->setOverrideCursor(setCursorShape(m_currentType));
//        } else {
//            qApp->setOverrideCursor(setCursorShape("line",  colorIndex(m_penColor)));
//        }
//    }

//    if (event->type() == QEvent::MouseButtonDblClick) {
//        emit requestScreenshot();
//    }

//    if (event->type() == QKeyEvent::KeyPress ) {
//        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
//        if (keyEvent->key() == Qt::Key_Escape && !keyEvent->isAutoRepeat()) {
//            if (m_editing) {
//                m_editing = false;
//                setAllTextEditReadOnly();
//                return true;
//            } else {
//                emit requestExit();
//            }
//        }
//        QFrame::keyPressEvent(keyEvent);
//    }

//    if (event->type() == QEvent::KeyRelease) {
//        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

//        //NOTE: must be use 'isAutoRepeat' to filter KeyRelease event
//        // send by Qt.
//        if (!keyEvent->isAutoRepeat()) {
//            if (keyEvent->modifiers() ==  (Qt::ShiftModifier | Qt::ControlModifier)) {
//                QProcess::startDetached("killall deepin-shortcut-viewer");
//            } else if (keyEvent->key() == Qt::Key_Question) {
//                QProcess::startDetached("killall deepin-shortcut-viewer");
//            }
//        }

//        QFrame::keyReleaseEvent(keyEvent);
//    }


//    return false;
//}

void ShapesWidget::deleteCurrentShape() {
    if (m_selectedIndex < m_shapes.length()) {
        m_shapes.removeAt(m_selectedIndex);
    } else {
        qWarning() << "Invalid index";
    }


    if (m_selectedShape.type == "text" && m_editMap.contains(m_selectedShape.index)) {
        m_editMap.value(m_selectedShape.index)->clear();
        delete m_editMap.value(m_selectedShape.index);
        m_editMap.remove(m_selectedShape.index);
    }

    clearSelected();
    m_selectedShape.type = "";
    m_currentShape.type = "";
    for(int i = 0; i < m_currentShape.mainPoints.length(); i++) {
        m_currentShape.mainPoints[i] = QPointF(0, 0);
    }
    update();
    m_selectedIndex = -1;
}

void ShapesWidget::undoDrawShapes()
{
    qDebug() << "m_selectedIndex:" << m_selectedIndex << m_shapes.length();
    if (m_selectedIndex < m_shapes.length() && m_selectedIndex != -1)
    {
            deleteCurrentShape();
    } else if (m_shapes.length() > 0) {
        int tmpIndex = m_shapes[m_shapes.length() - 1].index;
        if (m_shapes[m_shapes.length() - 1].type == "text" && m_editMap.contains(tmpIndex) ) {
            m_editMap.value(tmpIndex)->clear();
            delete m_editMap.value(tmpIndex);
            m_editMap.remove(tmpIndex);
        }

        m_shapes.removeLast();
    }
    update();
}

QString ShapesWidget::getCurrentType()
{
    return m_currentShape.type;
}

void ShapesWidget::microAdjust(QString direction) {
    if (m_selectedIndex != -1 && m_selectedIndex < m_shapes.length()) {
        if (direction == "Left" || direction == "Right" || direction == "Up" || direction == "Down") {
            m_shapes[m_selectedIndex].mainPoints = pointMoveMicro(m_shapes[m_selectedIndex].mainPoints, direction);
        } else if (direction == "Ctrl+Shift+Left" || direction == "Ctrl+Shift+Right" || direction == "Ctrl+Shift+Up"
                   || direction == "Ctrl+Shift+Down") {
            m_shapes[m_selectedIndex].mainPoints = pointResizeMicro(m_shapes[m_selectedIndex].mainPoints, direction, false);
        } else {
            m_shapes[m_selectedIndex].mainPoints = pointResizeMicro(m_shapes[m_selectedIndex].mainPoints, direction, true);
        }
        if (m_shapes[m_selectedIndex].type  == "text") {
            return;
        } else if (m_shapes[m_selectedIndex].type == "line" || m_shapes[m_selectedIndex].type == "arrow") {
            if (m_shapes[m_selectedIndex].portion.length() == 0) {
                for(int k = 0; k < m_shapes[m_selectedIndex].points.length(); k++) {
                    m_shapes[m_selectedIndex].portion.append(relativePosition(m_shapes[m_selectedIndex].mainPoints,
                                                                              m_shapes[m_selectedIndex].points[k]));
                }
            }
            for(int j = 0; j < m_shapes[m_selectedIndex].points.length(); j++) {
                m_shapes[m_selectedIndex].points[j] = getNewPosition(
                m_shapes[m_selectedIndex].mainPoints, m_shapes[m_selectedIndex].portion[j]);
            }
        }

        m_selectedShape.mainPoints = m_shapes[m_selectedIndex].mainPoints;
        m_selectedShape.points = m_shapes[m_selectedIndex].points;
        update();
    }
}

void ShapesWidget::setShiftKeyPressed(bool isShift) {
    m_isShiftPressed = isShift;
}
