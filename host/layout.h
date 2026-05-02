#pragma once
#include <yoga/Yoga.h>
#include <QObject>
#include <QPainter>
#include <QImage>
#include <QString>
#include <QColor>
#include <QFont>
#include <vector>
#include <string>
#include <memory>

struct RNNode {
  int id = 0;
  YGNodeRef yoga = nullptr;
  std::string type;       // "view" or "text"

  // Style
  QColor backgroundColor = QColor(0,0,0,0);
  QColor color            = QColor(0,0,0,255);
  std::string text;
  int fontSize = 24;
  int borderRadius = 0;
  QColor borderColor = QColor(0,0,0,0);
  float borderWidth = 0;

  std::vector<std::shared_ptr<RNNode>> children;

  RNNode(const std::string& t) : type(t) {
    yoga = YGNodeNew();
  }
  ~RNNode() {
    YGNodeFree(yoga);
  }
};

// Paint a node and its children recursively
static void paintNode(QPainter& p, RNNode* node,
                      float offsetX, float offsetY) {
  float x = offsetX + YGNodeLayoutGetLeft(node->yoga);
  float y = offsetY + YGNodeLayoutGetTop(node->yoga);
  float w = YGNodeLayoutGetWidth(node->yoga);
  float h = YGNodeLayoutGetHeight(node->yoga);

  // Background
  if (node->backgroundColor.alpha() > 0) {
    if (node->borderRadius > 0) {
      p.setBrush(node->backgroundColor);
      p.setPen(Qt::NoPen);
      p.drawRoundedRect(QRectF(x,y,w,h),
                        node->borderRadius, node->borderRadius);
    } else {
      p.fillRect(QRectF(x,y,w,h), node->backgroundColor);
    }
  }

  // Border
  if (node->borderWidth > 0 && node->borderColor.alpha() > 0) {
    p.setPen(QPen(node->borderColor, node->borderWidth));
    p.setBrush(Qt::NoBrush);
    if (node->borderRadius > 0)
      p.drawRoundedRect(QRectF(x,y,w,h),
                        node->borderRadius, node->borderRadius);
    else
      p.drawRect(QRectF(x,y,w,h));
  }

  // Text
  if (node->type == "text" && !node->text.empty()) {
    p.setPen(node->color);
    QFont f; f.setPixelSize(node->fontSize);
    p.setFont(f);
    p.drawText(QRectF(x,y,w,h),
               Qt::AlignVCenter | Qt::AlignLeft,
               QString::fromStdString(node->text));
  }

  // Children
  for (auto& child : node->children)
    paintNode(p, child.get(), x, y);
}
