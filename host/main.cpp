#include <QGuiApplication>
#include <QPainter>
#include <QImage>
#include <QDebug>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <hermes/hermes.h>
#include "layout.h"
#include "input.h"
#include "quill.h"

using namespace facebook;
using namespace facebook::jsi;

// ── Screen ────────────────────────────────────────────────────────
// Paints the RN tree straight into quill's aux framebuffer — the vendor
// e-ink engine's drawing buffer (libqsgepaper, taken over via the epfb-re
// shim in libquill.so) — then swaps only the dirty region to glass.
class Screen {
public:
  bool init() {
    if (quill_init() != 0) return false;
    int w = quill_width(), h = quill_height();
    unsigned char* buf = quill_buffer();
    if (!buf || w <= 0 || h <= 0) return false;
    auto fmt = (QImage::Format)quill_format();
    m_front = QImage(buf, w, h, quill_stride(), fmt);
    m_prev  = QImage(w, h, fmt);
    m_front.fill(Qt::white);
    m_prev.fill(Qt::white);
    quill_swap(0, 0, w, h, /*QualityFull*/4, /*flashing*/1);
    return true;
  }

  void render(RNNode* root) {
    m_front.fill(Qt::white);
    QPainter p(&m_front);
    p.setRenderHint(QPainter::Antialiasing);
    paintNode(p, root, 0, 0);
    p.end();

    // E-ink updates are expensive: diff against the previous frame and
    // swap only the changed bounding box.
    const int w = m_front.width(), h = m_front.height();
    const int bpp = m_front.depth() / 8;
    const int rowBytes = w * bpp;
    int y0 = -1, y1 = -1;
    for (int y = 0; y < h; y++)
      if (memcmp(m_front.constScanLine(y), m_prev.constScanLine(y), rowBytes))
        { y0 = y; break; }
    if (y0 < 0) return;  // nothing changed
    for (int y = h - 1; y >= y0; y--)
      if (memcmp(m_front.constScanLine(y), m_prev.constScanLine(y), rowBytes))
        { y1 = y; break; }
    int x0 = w - 1, x1 = 0;
    for (int y = y0; y <= y1; y++) {
      const uchar* a = m_front.constScanLine(y);
      const uchar* b = m_prev.constScanLine(y);
      int lo = 0;
      while (lo < rowBytes && a[lo] == b[lo]) lo++;
      if (lo == rowBytes) continue;
      int hi = rowBytes - 1;
      while (hi > lo && a[hi] == b[hi]) hi--;
      x0 = std::min(x0, lo / bpp);
      x1 = std::max(x1, hi / bpp);
    }
    for (int y = y0; y <= y1; y++)
      memcpy(m_prev.scanLine(y), m_front.constScanLine(y), rowBytes);
    quill_swap(x0, y0, x1 - x0 + 1, y1 - y0 + 1, /*Quality3*/3, 0);
  }

  // Flashing clear of the whole panel (ghost removal).
  void fullRefresh() {
    quill_swap(0, 0, m_front.width(), m_front.height(), 4, 1);
  }

  int W() const { return m_front.width(); }
  int H() const { return m_front.height(); }

private:
  QImage m_front;  // wraps the aux framebuffer bits — do not reassign
  QImage m_prev;
};

// ── JSI helpers ───────────────────────────────────────────────────
static std::string str(jsi::Runtime& rt, const jsi::Value& v) {
  return v.toString(rt).utf8(rt);
}
static std::string getProp(jsi::Runtime& rt, const jsi::Object& o,
                            const char* key, const char* def="") {
  if (o.hasProperty(rt, key)) return str(rt, o.getProperty(rt, key));
  return def;
}
static double getNum(jsi::Runtime& rt, const jsi::Object& o,
                     const char* key, double def=0) {
  if (o.hasProperty(rt, key))
    return o.getProperty(rt, key).asNumber();
  return def;
}

// ── Node registry ─────────────────────────────────────────────────
using NodeMap = std::unordered_map<int, std::shared_ptr<RNNode>>;
static int nextId = 1;

// Apply a JS props object to an RNNode. Used by both createNode (initial)
// and updateNode (commit-time mutation). Only props present on the JS
// object overwrite existing values, so updates don't silently reset
// fields the caller didn't provide.
static void applyProps(jsi::Runtime& rt, RNNode* node, const jsi::Object& props) {
  // Dimensions
  if (props.hasProperty(rt, "width"))
    YGNodeStyleSetWidth(node->yoga, props.getProperty(rt, "width").asNumber());
  if (props.hasProperty(rt, "height"))
    YGNodeStyleSetHeight(node->yoga, props.getProperty(rt, "height").asNumber());
  if (props.hasProperty(rt, "flex"))
    YGNodeStyleSetFlex(node->yoga, props.getProperty(rt, "flex").asNumber());

  if (props.hasProperty(rt, "flexDirection")) {
    std::string fd = str(rt, props.getProperty(rt, "flexDirection"));
    YGNodeStyleSetFlexDirection(node->yoga,
      fd == "row" ? YGFlexDirectionRow : YGFlexDirectionColumn);
  }
  if (props.hasProperty(rt, "justifyContent")) {
    std::string jc = str(rt, props.getProperty(rt, "justifyContent"));
    if (jc == "center")        YGNodeStyleSetJustifyContent(node->yoga, YGJustifyCenter);
    else if (jc == "space-between") YGNodeStyleSetJustifyContent(node->yoga, YGJustifySpaceBetween);
    else if (jc == "space-around")  YGNodeStyleSetJustifyContent(node->yoga, YGJustifySpaceAround);
    else if (jc == "flex-end")      YGNodeStyleSetJustifyContent(node->yoga, YGJustifyFlexEnd);
    else if (jc == "flex-start")    YGNodeStyleSetJustifyContent(node->yoga, YGJustifyFlexStart);
  }
  if (props.hasProperty(rt, "alignItems")) {
    std::string ai = str(rt, props.getProperty(rt, "alignItems"));
    if (ai == "center")    YGNodeStyleSetAlignItems(node->yoga, YGAlignCenter);
    else if (ai == "flex-end")  YGNodeStyleSetAlignItems(node->yoga, YGAlignFlexEnd);
    else if (ai == "stretch")   YGNodeStyleSetAlignItems(node->yoga, YGAlignStretch);
    else if (ai == "flex-start") YGNodeStyleSetAlignItems(node->yoga, YGAlignFlexStart);
  }

  // Absolute / relative positioning
  if (props.hasProperty(rt, "position")) {
    std::string pos = str(rt, props.getProperty(rt, "position"));
    YGNodeStyleSetPositionType(node->yoga,
      pos == "absolute" ? YGPositionTypeAbsolute : YGPositionTypeRelative);
  }
  if (props.hasProperty(rt, "top"))
    YGNodeStyleSetPosition(node->yoga, YGEdgeTop,
      props.getProperty(rt, "top").asNumber());
  if (props.hasProperty(rt, "bottom"))
    YGNodeStyleSetPosition(node->yoga, YGEdgeBottom,
      props.getProperty(rt, "bottom").asNumber());
  if (props.hasProperty(rt, "left"))
    YGNodeStyleSetPosition(node->yoga, YGEdgeLeft,
      props.getProperty(rt, "left").asNumber());
  if (props.hasProperty(rt, "right"))
    YGNodeStyleSetPosition(node->yoga, YGEdgeRight,
      props.getProperty(rt, "right").asNumber());

  // Padding
  if (props.hasProperty(rt, "padding"))
    YGNodeStyleSetPadding(node->yoga, YGEdgeAll, props.getProperty(rt, "padding").asNumber());
  if (props.hasProperty(rt, "paddingTop"))
    YGNodeStyleSetPadding(node->yoga, YGEdgeTop, props.getProperty(rt, "paddingTop").asNumber());
  if (props.hasProperty(rt, "paddingBottom"))
    YGNodeStyleSetPadding(node->yoga, YGEdgeBottom, props.getProperty(rt, "paddingBottom").asNumber());
  if (props.hasProperty(rt, "paddingLeft"))
    YGNodeStyleSetPadding(node->yoga, YGEdgeLeft, props.getProperty(rt, "paddingLeft").asNumber());
  if (props.hasProperty(rt, "paddingRight"))
    YGNodeStyleSetPadding(node->yoga, YGEdgeRight, props.getProperty(rt, "paddingRight").asNumber());

  // Margin
  if (props.hasProperty(rt, "margin"))
    YGNodeStyleSetMargin(node->yoga, YGEdgeAll, props.getProperty(rt, "margin").asNumber());
  if (props.hasProperty(rt, "marginTop"))
    YGNodeStyleSetMargin(node->yoga, YGEdgeTop, props.getProperty(rt, "marginTop").asNumber());
  if (props.hasProperty(rt, "marginBottom"))
    YGNodeStyleSetMargin(node->yoga, YGEdgeBottom, props.getProperty(rt, "marginBottom").asNumber());

  // Visual
  if (props.hasProperty(rt, "backgroundColor"))
    node->backgroundColor = QColor(QString::fromStdString(
      str(rt, props.getProperty(rt, "backgroundColor"))));
  if (props.hasProperty(rt, "color"))
    node->color = QColor(QString::fromStdString(
      str(rt, props.getProperty(rt, "color"))));
  if (props.hasProperty(rt, "text"))
    node->text = str(rt, props.getProperty(rt, "text"));
  if (props.hasProperty(rt, "fontSize"))
    node->fontSize = (int)props.getProperty(rt, "fontSize").asNumber();
  if (props.hasProperty(rt, "borderRadius"))
    node->borderRadius = (int)props.getProperty(rt, "borderRadius").asNumber();
  if (props.hasProperty(rt, "borderWidth"))
    node->borderWidth = (float)props.getProperty(rt, "borderWidth").asNumber();
  if (props.hasProperty(rt, "borderColor"))
    node->borderColor = QColor(QString::fromStdString(
      str(rt, props.getProperty(rt, "borderColor"))));
}

// ── Wire JSI ─────────────────────────────────────────────────────
struct TouchCB {
  std::shared_ptr<jsi::Function> onTouchDown;
  std::shared_ptr<jsi::Function> onTouchUp;
};

static void wire(jsi::Runtime& rt, Screen* scr,
                 NodeMap& nodes, TouchCB& tcb) {
  auto obj = jsi::Object(rt);

  obj.setProperty(rt, "screenWidth",  jsi::Value((double)scr->W()));
  obj.setProperty(rt, "screenHeight", jsi::Value((double)scr->H()));

  // N.createNode(type, props) → id
  obj.setProperty(rt, "createNode",
    jsi::Function::createFromHostFunction(rt,
      jsi::PropNameID::forAscii(rt,"createNode"),2,
      [&nodes](jsi::Runtime& rt,const jsi::Value&,
               const jsi::Value* a,size_t n)->jsi::Value{
        std::string type = a[0].toString(rt).utf8(rt);
        auto node = std::make_shared<RNNode>(type);
        if (n > 1 && a[1].isObject()) {
          auto props = a[1].getObject(rt);
          applyProps(rt, node.get(), props);
        }
        int id = nextId++;
        node->id = id;
        nodes[id] = node;
        return jsi::Value(id);
      }));

  // N.updateNode(id, props) — apply prop diffs to an existing node
  obj.setProperty(rt, "updateNode",
    jsi::Function::createFromHostFunction(rt,
      jsi::PropNameID::forAscii(rt,"updateNode"),2,
      [&nodes](jsi::Runtime& rt,const jsi::Value&,
               const jsi::Value* a,size_t n)->jsi::Value{
        if (n < 2 || !a[0].isNumber() || !a[1].isObject())
          return jsi::Value::undefined();
        int id = (int)a[0].asNumber();
        auto it = nodes.find(id);
        if (it == nodes.end()) return jsi::Value::undefined();
        applyProps(rt, it->second.get(), a[1].getObject(rt));
        return jsi::Value::undefined();
      }));

  // N.appendChild(parentId, childId)
  obj.setProperty(rt, "appendChild",
    jsi::Function::createFromHostFunction(rt,
      jsi::PropNameID::forAscii(rt,"appendChild"),2,
      [&nodes](jsi::Runtime&,const jsi::Value&,
               const jsi::Value* a,size_t)->jsi::Value{
        int pid = (int)a[0].asNumber();
        int cid = (int)a[1].asNumber();
        auto& parent = nodes[pid];
        auto& child  = nodes[cid];
        YGNodeInsertChild(parent->yoga, child->yoga,
                          parent->children.size());
        parent->children.push_back(child);
        return jsi::Value::undefined();
      }));

  // N.hitTest(rootId, x, y) → deepest node id containing (x,y), or -1
  obj.setProperty(rt, "hitTest",
    jsi::Function::createFromHostFunction(rt,
      jsi::PropNameID::forAscii(rt,"hitTest"),3,
      [&nodes](jsi::Runtime& rt,const jsi::Value&,
               const jsi::Value* a,size_t n)->jsi::Value{
        if (n < 3) return jsi::Value(-1);
        int rid  = (int)a[0].asNumber();
        double tx = a[1].asNumber();
        double ty = a[2].asNumber();
        auto it = nodes.find(rid);
        if (it == nodes.end()) return jsi::Value(-1);

        int found = -1;
        std::function<void(RNNode*, float, float)> walk =
          [&](RNNode* node, float ox, float oy) {
            float x = ox + YGNodeLayoutGetLeft(node->yoga);
            float y = oy + YGNodeLayoutGetTop(node->yoga);
            float w = YGNodeLayoutGetWidth(node->yoga);
            float h = YGNodeLayoutGetHeight(node->yoga);
            if (tx < x || tx >= x + w || ty < y || ty >= y + h) return;
            found = node->id;
            for (auto& c : node->children) walk(c.get(), x, y);
          };
        walk(it->second.get(), 0, 0);
        return jsi::Value(found);
      }));

  // N.removeChild(parentId, childId) — detach child from parent's Yoga
  // tree and from the parent's children vector, then drop the subtree
  // from the nodes registry. shared_ptr destruction cascades into
  // RNNode::~RNNode which calls YGNodeFree.
  obj.setProperty(rt, "removeChild",
    jsi::Function::createFromHostFunction(rt,
      jsi::PropNameID::forAscii(rt,"removeChild"),2,
      [&nodes](jsi::Runtime&,const jsi::Value&,
               const jsi::Value* a,size_t n)->jsi::Value{
        if (n < 2) return jsi::Value::undefined();
        int pid = (int)a[0].asNumber();
        int cid = (int)a[1].asNumber();
        auto cIt = nodes.find(cid);
        if (cIt == nodes.end()) return jsi::Value::undefined();
        auto child = cIt->second;
        auto pIt = nodes.find(pid);
        if (pIt != nodes.end()) {
          auto& parent = pIt->second;
          YGNodeRemoveChild(parent->yoga, child->yoga);
          for (auto it = parent->children.begin();
               it != parent->children.end(); ) {
            if (*it == child) it = parent->children.erase(it);
            else ++it;
          }
        }
        std::function<void(RNNode*)> eraseSubtree = [&](RNNode* node) {
          for (auto& c : node->children) eraseSubtree(c.get());
          nodes.erase(node->id);
        };
        eraseSubtree(child.get());
        return jsi::Value::undefined();
      }));

  // N.destroyNode(id) — drop a root-level node (one not attached to a
  // parent in our registry, e.g. removed from the container).
  obj.setProperty(rt, "destroyNode",
    jsi::Function::createFromHostFunction(rt,
      jsi::PropNameID::forAscii(rt,"destroyNode"),1,
      [&nodes](jsi::Runtime&,const jsi::Value&,
               const jsi::Value* a,size_t n)->jsi::Value{
        if (n < 1) return jsi::Value::undefined();
        int id = (int)a[0].asNumber();
        auto it = nodes.find(id);
        if (it == nodes.end()) return jsi::Value::undefined();
        auto node = it->second;
        YGNodeRef yp = YGNodeGetParent(node->yoga);
        if (yp) YGNodeRemoveChild(yp, node->yoga);
        std::function<void(RNNode*)> eraseSubtree = [&](RNNode* n2) {
          for (auto& c : n2->children) eraseSubtree(c.get());
          nodes.erase(n2->id);
        };
        eraseSubtree(node.get());
        return jsi::Value::undefined();
      }));

  // N.commit(rootId) — layout + render
  obj.setProperty(rt, "commit",
    jsi::Function::createFromHostFunction(rt,
      jsi::PropNameID::forAscii(rt,"commit"),1,
      [&nodes,scr](jsi::Runtime&,const jsi::Value&,
                   const jsi::Value* a,size_t)->jsi::Value{
        int rid = (int)a[0].asNumber();
        auto& root = nodes[rid];
        YGNodeCalculateLayout(root->yoga,
          YGUndefined, YGUndefined, YGDirectionLTR);
        scr->render(root.get());
        return jsi::Value::undefined();
      }));

  // N.fullRefresh() — flashing clear of the whole panel (ghost removal)
  obj.setProperty(rt, "fullRefresh",
    jsi::Function::createFromHostFunction(rt,
      jsi::PropNameID::forAscii(rt,"fullRefresh"),0,
      [scr](jsi::Runtime&,const jsi::Value&,
            const jsi::Value*,size_t)->jsi::Value{
        scr->fullRefresh();
        return jsi::Value::undefined();
      }));

  // N.onTouchDown / onTouchUp
  obj.setProperty(rt, "onTouchDown",
    jsi::Function::createFromHostFunction(rt,
      jsi::PropNameID::forAscii(rt,"onTouchDown"),1,
      [&tcb](jsi::Runtime& rt,const jsi::Value&,
             const jsi::Value* a,size_t)->jsi::Value{
        tcb.onTouchDown = std::make_shared<jsi::Function>(
          a[0].getObject(rt).asFunction(rt));
        return jsi::Value::undefined();
      }));
  obj.setProperty(rt, "onTouchUp",
    jsi::Function::createFromHostFunction(rt,
      jsi::PropNameID::forAscii(rt,"onTouchUp"),1,
      [&tcb](jsi::Runtime& rt,const jsi::Value&,
             const jsi::Value* a,size_t)->jsi::Value{
        tcb.onTouchUp = std::make_shared<jsi::Function>(
          a[0].getObject(rt).asFunction(rt));
        return jsi::Value::undefined();
      }));

  rt.global().setProperty(rt, "N", obj);
}

int main(int argc, char* argv[]) {
  if (argc < 2) { qWarning("Usage: rn-layout <bundle.js>"); return 1; }

  // Takeover mode has no window system: default to the offscreen QPA so
  // QGuiApplication (font database for QPainter text) comes up without a
  // display server. The panel is driven by quill, not by Qt.
  if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication app(argc, argv);

  Screen screen;
  if (!screen.init()) {
    qWarning("quill: failed to take over the e-ink engine "
             "(is xochitl stopped? is libqsgepaper.so reachable?)");
    return 1;
  }
  Screen* scr = &screen;
  qDebug() << "[host] screen" << scr->W() << "x" << scr->H();

  std::ifstream file(argv[1]);
  std::ostringstream ss; ss << file.rdbuf();
  std::string source = ss.str();

  auto runtime = facebook::hermes::makeHermesRuntime();
  NodeMap nodes;
  TouchCB tcb;

  // ── Input: raw evdev (no window system in takeover mode) ────────
  // Readers emit normalized 0..1 coordinates; scale to screen pixels here.
  // If the touch panel turns out to be rotated relative to the display,
  // fix it with RN_TOUCH_SWAP_XY / RN_TOUCH_INVERT_X / RN_TOUCH_INVERT_Y.
  const bool swapXY = qEnvironmentVariableIntValue("RN_TOUCH_SWAP_XY") != 0;
  const bool invX   = qEnvironmentVariableIntValue("RN_TOUCH_INVERT_X") != 0;
  const bool invY   = qEnvironmentVariableIntValue("RN_TOUCH_INVERT_Y") != 0;
  auto mapPoint = [scr, swapXY, invX, invY](double nx, double ny,
                                            double& x, double& y) {
    if (swapXY) std::swap(nx, ny);
    if (invX) nx = 1.0 - nx;
    if (invY) ny = 1.0 - ny;
    x = nx * (scr->W() - 1);
    y = ny * (scr->H() - 1);
  };

  auto touchDown = [&, mapPoint](double nx, double ny) {
    double x, y;
    mapPoint(nx, ny, x, y);
    qDebug() << "[touch] down x=" << x << "y=" << y;
    // Fire both the legacy callback and N.onTouchDown
    if (tcb.onTouchDown) {
      try { tcb.onTouchDown->call(*runtime,
              jsi::Value(x), jsi::Value(y)); }
      catch(...) {}
    }
    // Also call global __rmTouchDown if set by renderer
    try {
      auto global = runtime->global();
      if (global.hasProperty(*runtime, "__rmTouchDown")) {
        auto fn = global.getPropertyAsFunction(*runtime, "__rmTouchDown");
        fn.call(*runtime, jsi::Value(x), jsi::Value(y));
      } else {
        qWarning() << "[touch] __rmTouchDown not set on global";
      }
    } catch (const jsi::JSError& e) {
      qWarning() << "[touch] JS error:" << e.getMessage().c_str();
      qWarning() << "Stack:" << e.getStack().c_str();
    } catch (const std::exception& e) {
      qWarning() << "[touch] C++ error:" << e.what();
    } catch (...) {
      qWarning() << "[touch] unknown error";
    }
  };

  auto touchMove = [&, mapPoint](double nx, double ny) {
    double x, y;
    mapPoint(nx, ny, x, y);
    try {
      auto global = runtime->global();
      if (global.hasProperty(*runtime, "__rmTouchMove")) {
        auto fn = global.getPropertyAsFunction(*runtime, "__rmTouchMove");
        fn.call(*runtime, jsi::Value(x), jsi::Value(y));
      }
    } catch(...) {}
  };

  auto touchUp = [&]() {
    if (tcb.onTouchUp) {
      try { tcb.onTouchUp->call(*runtime); }
      catch(...) {}
    }
    try {
      auto global = runtime->global();
      if (global.hasProperty(*runtime, "__rmTouchUp")) {
        auto fn = global.getPropertyAsFunction(*runtime, "__rmTouchUp");
        fn.call(*runtime);
      }
    } catch(...) {}
  };

  auto keyDown = [&](const QString& name, const QString& text) {
    try {
      auto global = runtime->global();
      if (!global.hasProperty(*runtime, "__rmKeyDown")) return;
      auto fn = global.getPropertyAsFunction(*runtime, "__rmKeyDown");
      fn.call(*runtime,
        jsi::String::createFromUtf8(*runtime, name.toStdString()),
        jsi::String::createFromUtf8(*runtime, text.toStdString()));
    } catch (const jsi::JSError& e) {
      qWarning() << "[key] JS error:" << e.getMessage().c_str();
      qWarning() << "Stack:" << e.getStack().c_str();
    } catch (...) {}
  };

  PointerReader touch, pen;
  KeyboardReader keyboard, power;

  touch.onDown = touchDown;
  touch.onMove = touchMove;
  touch.onUp   = touchUp;
  // Takeover mode owns the screen; a 5-finger tap is the escape hatch
  // (same gesture as riddle).
  touch.onFiveFinger = [&app] {
    qDebug() << "[input] five-finger tap — exiting";
    app.quit();
  };
  pen.onDown = touchDown;
  pen.onMove = touchMove;
  pen.onUp   = touchUp;

  keyboard.onKey = keyDown;
  power.onKey = [&app](const QString& name, const QString&) {
    if (name == "Power") {
      qDebug() << "[input] power button — exiting";
      app.quit();
    }
  };

  QString touchDev = findInputDevice({"touch"});
  QString penDev   = findInputDevice({"marker", "wacom", "stylus"});
  QString kbdDev   = findInputDevice({"keyboard", "folio"});
  QString pwrDev   = findInputDevice({"powerkey"});
  if (!touchDev.isEmpty()) touch.start(touchDev);
  else qWarning() << "[input] no touchscreen found";
  if (!penDev.isEmpty())   pen.start(penDev);
  if (!kbdDev.isEmpty())   keyboard.start(kbdDev);
  if (!pwrDev.isEmpty())   power.start(pwrDev);

  // Console
  auto makeLog = [&](std::string lv) {
    return jsi::Function::createFromHostFunction(*runtime,
      jsi::PropNameID::forAscii(*runtime,lv),1,
      [lv](jsi::Runtime& rt,const jsi::Value&,
           const jsi::Value* a,size_t n)->jsi::Value{
        QString msg;
        for(size_t i=0;i<n;i++)
          msg+=QString::fromStdString(a[i].toString(rt).utf8(rt))+" ";
        qDebug()<<"["<<lv.c_str()<<"]"<<msg;
        return jsi::Value::undefined();
      });
  };
  auto console = jsi::Object(*runtime);
  console.setProperty(*runtime,"log",  makeLog("log"));
  console.setProperty(*runtime,"warn", makeLog("warn"));
  console.setProperty(*runtime,"error",makeLog("error"));
  runtime->global().setProperty(*runtime,"console",console);

  runtime->evaluateJavaScript(std::make_unique<jsi::StringBuffer>(R"(
    var __DEV__ = false;
    var __BUNDLE_START_TIME__ = Date.now();
    var process = { env: { NODE_ENV: 'production' } };
    var performance = { now: function() { return Date.now(); } };
    var global = globalThis;
    __fbBatchedBridgeConfig = { remoteModuleConfig:[], localModulesConfig:[] };
    var TurboModuleRegistry = {
      get: function() { return null; },
      getEnforcing: function(name) {
        if (name === 'Timing') return {
          createTimer: function() {},
          deleteTimer: function() {},
          setSendIdleEvents: function() {},
        };
        if (name === 'ExceptionsManager') return {
          reportFatalException: function() {},
          reportSoftException: function() {},
          updateExceptionMessage: function() {},
          dismissRedbox: function() {},
        };
        return {};
      }
    };
    var nativeModuleProxy = TurboModuleRegistry;
  )"), "globals");

  // ── Timers (setTimeout / setInterval / clearTimeout / clearInterval) ──
  struct TimerEntry {
    QTimer* timer;
    std::shared_ptr<jsi::Function> callback;
    bool repeat;
  };
  auto timers = std::make_shared<std::unordered_map<int, TimerEntry>>();
  auto nextTimerId = std::make_shared<int>(1);

  auto installTimer = [&](const char* name, bool repeat) {
    runtime->global().setProperty(*runtime, name,
      jsi::Function::createFromHostFunction(*runtime,
        jsi::PropNameID::forAscii(*runtime, name), 2,
        [&runtime, timers, nextTimerId, repeat]
        (jsi::Runtime& rt, const jsi::Value&,
         const jsi::Value* a, size_t n) -> jsi::Value {
          if (n < 1 || !a[0].isObject() || !a[0].getObject(rt).isFunction(rt))
            return jsi::Value::undefined();
          auto cb = std::make_shared<jsi::Function>(
            a[0].getObject(rt).asFunction(rt));
          int delay = (n > 1 && a[1].isNumber()) ? (int)a[1].asNumber() : 0;
          if (delay < 0) delay = 0;
          int id = (*nextTimerId)++;
          QTimer* t = new QTimer();
          t->setSingleShot(!repeat);
          t->setInterval(delay);
          (*timers)[id] = TimerEntry{t, cb, repeat};
          QObject::connect(t, &QTimer::timeout,
            [&runtime, timers, id]() {
              auto it = timers->find(id);
              if (it == timers->end()) return;
              auto cb = it->second.callback;
              bool repeat = it->second.repeat;
              try { cb->call(*runtime); }
              catch (const jsi::JSError& e) {
                qWarning() << "Timer JS error:" << e.getMessage().c_str();
                qWarning() << "Stack:" << e.getStack().c_str();
              } catch (const std::exception& e) {
                qWarning() << "Timer C++ error:" << e.what();
              } catch (...) {
                qWarning() << "Timer unknown error";
              }
              if (!repeat) {
                auto it2 = timers->find(id);
                if (it2 != timers->end()) {
                  it2->second.timer->deleteLater();
                  timers->erase(it2);
                }
              }
            });
          t->start();
          return jsi::Value(id);
        }));
  };

  auto installClear = [&](const char* name) {
    runtime->global().setProperty(*runtime, name,
      jsi::Function::createFromHostFunction(*runtime,
        jsi::PropNameID::forAscii(*runtime, name), 1,
        [timers](jsi::Runtime&, const jsi::Value&,
                 const jsi::Value* a, size_t n) -> jsi::Value {
          if (n < 1 || !a[0].isNumber()) return jsi::Value::undefined();
          int id = (int)a[0].asNumber();
          auto it = timers->find(id);
          if (it != timers->end()) {
            it->second.timer->stop();
            it->second.timer->deleteLater();
            timers->erase(it);
          }
          return jsi::Value::undefined();
        }));
  };

  installTimer("setTimeout",  false);
  installTimer("setInterval", true);
  installClear("clearTimeout");
  installClear("clearInterval");

  // setImmediate / clearImmediate + minimal MessageChannel polyfill
  // (React's scheduler uses MessageChannel to yield between work units.)
  runtime->evaluateJavaScript(std::make_unique<jsi::StringBuffer>(R"(
    globalThis.setImmediate = function(fn) {
      var args = Array.prototype.slice.call(arguments, 1);
      return setTimeout(function() { fn.apply(null, args); }, 0);
    };
    globalThis.clearImmediate = function(id) { clearTimeout(id); };
    globalThis.queueMicrotask = globalThis.queueMicrotask || function(fn) {
      Promise.resolve().then(fn).catch(function(e){
        setTimeout(function(){ throw e; }, 0);
      });
    };
    globalThis.MessageChannel = function MessageChannel() {
      var port1 = { onmessage: null };
      var port2 = {
        postMessage: function() {
          setTimeout(function() {
            if (typeof port1.onmessage === 'function') {
              try { port1.onmessage({ data: null }); } catch (e) {}
            }
          }, 0);
        }
      };
      this.port1 = port1;
      this.port2 = port2;
    };
  )"), "polyfills");

  wire(*runtime, scr, nodes, tcb);

  // ── N.fetch(url, opts) → Promise<Response>
  // Backed by QNetworkAccessManager. Reply finishes on the Qt main
  // thread, which is also the JSI thread, so resolve/reject is safe.
  auto nam = std::make_shared<QNetworkAccessManager>();
  {
    auto N = runtime->global()
      .getProperty(*runtime, "N").asObject(*runtime);

    N.setProperty(*runtime, "fetch",
      jsi::Function::createFromHostFunction(*runtime,
        jsi::PropNameID::forAscii(*runtime, "fetch"), 2,
        [&runtime, nam](jsi::Runtime& rt, const jsi::Value&,
                        const jsi::Value* a, size_t n) -> jsi::Value {
          if (n < 1) {
            throw jsi::JSError(rt, "fetch: url is required");
          }
          QString url = QString::fromStdString(str(rt, a[0]));
          QString method = "GET";
          QByteArray body;
          QList<QPair<QByteArray, QByteArray>> headers;

          if (n > 1 && a[1].isObject()) {
            auto opts = a[1].getObject(rt);
            if (opts.hasProperty(rt, "method"))
              method = QString::fromStdString(
                str(rt, opts.getProperty(rt, "method"))).toUpper();
            if (opts.hasProperty(rt, "body") &&
                opts.getProperty(rt, "body").isString())
              body = QByteArray::fromStdString(
                opts.getProperty(rt, "body").toString(rt).utf8(rt));
            if (opts.hasProperty(rt, "headers") &&
                opts.getProperty(rt, "headers").isObject()) {
              auto hdrs = opts.getProperty(rt, "headers").getObject(rt);
              auto names = hdrs.getPropertyNames(rt);
              size_t ln = names.size(rt);
              for (size_t i = 0; i < ln; i++) {
                std::string k = names.getValueAtIndex(rt, i)
                  .toString(rt).utf8(rt);
                std::string v = str(rt, hdrs.getProperty(rt, k.c_str()));
                headers.append({QByteArray::fromStdString(k),
                                QByteArray::fromStdString(v)});
              }
            }
          }

          QNetworkRequest req((QUrl(url)));
          req.setTransferTimeout(30000);
          for (const auto& h : headers)
            req.setRawHeader(h.first, h.second);

          QNetworkReply* reply = nullptr;
          if (method == "GET")          reply = nam->get(req);
          else if (method == "POST")    reply = nam->post(req, body);
          else if (method == "PUT")     reply = nam->put(req, body);
          else if (method == "DELETE")  reply = nam->deleteResource(req);
          else if (method == "HEAD")    reply = nam->head(req);
          else reply = nam->sendCustomRequest(req, method.toUtf8(), body);

          // Build the JS-side Promise(executor) that captures
          // resolve/reject and connects them to reply->finished.
          auto Promise = rt.global()
            .getPropertyAsFunction(rt, "Promise");

          auto executor = jsi::Function::createFromHostFunction(rt,
            jsi::PropNameID::forAscii(rt, "fetchExec"), 2,
            [&runtime, reply, urlCopy = url](
              jsi::Runtime& rt2, const jsi::Value&,
              const jsi::Value* args, size_t na) -> jsi::Value {
              if (na < 2) return jsi::Value::undefined();
              auto resolve = std::make_shared<jsi::Function>(
                args[0].getObject(rt2).asFunction(rt2));
              auto reject = std::make_shared<jsi::Function>(
                args[1].getObject(rt2).asFunction(rt2));

              QObject::connect(reply, &QNetworkReply::finished,
                [&runtime, reply, resolve, reject, urlCopy]() {
                  jsi::Runtime& r = *runtime;
                  try {
                    if (reply->error() != QNetworkReply::NoError) {
                      auto err = jsi::Object(r);
                      err.setProperty(r, "message",
                        jsi::String::createFromUtf8(r,
                          reply->errorString().toStdString()));
                      reject->call(r, std::move(err));
                    } else {
                      auto res = jsi::Object(r);
                      int status = reply->attribute(
                        QNetworkRequest::HttpStatusCodeAttribute).toInt();
                      QString reason = reply->attribute(
                        QNetworkRequest::HttpReasonPhraseAttribute)
                        .toString();
                      res.setProperty(r, "status", jsi::Value(status));
                      res.setProperty(r, "statusText",
                        jsi::String::createFromUtf8(r,
                          reason.toStdString()));
                      res.setProperty(r, "url",
                        jsi::String::createFromUtf8(r,
                          urlCopy.toStdString()));
                      QByteArray bytes = reply->readAll();
                      res.setProperty(r, "body",
                        jsi::String::createFromUtf8(r,
                          std::string(bytes.constData(), bytes.size())));
                      auto hdrs = jsi::Object(r);
                      for (const auto& p : reply->rawHeaderPairs()) {
                        hdrs.setProperty(r,
                          jsi::PropNameID::forUtf8(r,
                            QString::fromUtf8(p.first).toLower().toStdString()),
                          jsi::String::createFromUtf8(r,
                            QString::fromUtf8(p.second).toStdString()));
                      }
                      res.setProperty(r, "headers", hdrs);
                      resolve->call(r, std::move(res));
                    }
                  } catch (const jsi::JSError& e) {
                    qWarning() << "[fetch] resolve error:"
                               << e.getMessage().c_str();
                  } catch (...) {}
                  reply->deleteLater();
                });
              return jsi::Value::undefined();
            });

          return Promise.callAsConstructor(rt, std::move(executor));
        }));
  }

  // Browser-style fetch() polyfill — wraps N.fetch in a Response-like
  // object. Body is delivered as a string (no streaming for now).
  runtime->evaluateJavaScript(std::make_unique<jsi::StringBuffer>(R"(
    globalThis.fetch = function(url, opts) {
      return N.fetch(String(url), opts || {}).then(function(res) {
        return {
          ok: res.status >= 200 && res.status < 300,
          status: res.status,
          statusText: res.statusText || "",
          url: res.url,
          headers: {
            get: function(name) {
              return res.headers[String(name).toLowerCase()] || null;
            }
          },
          text: function() { return Promise.resolve(res.body); },
          json: function() {
            try { return Promise.resolve(JSON.parse(res.body)); }
            catch (e) { return Promise.reject(e); }
          }
        };
      });
    };
  )"), "fetch-polyfill");

  try {
    runtime->evaluateJavaScript(
      std::make_unique<jsi::StringBuffer>(source), argv[1]);
    qDebug() << "[host] Bundle executed";
  } catch(const jsi::JSError& e) {
    qWarning() << "JS Error:" << e.getMessage().c_str();
    qWarning() << "Stack:" << e.getStack().c_str();
    return 1;
  }

  return app.exec();
}
