#include <QGuiApplication>
#include <QScreen>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QPainter>
#include <QImage>
#include <QObject>
#include <QDebug>
#include <QTimer>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <hermes/hermes.h>
#include "layout.h"
#include "input.h"

using namespace facebook;
using namespace facebook::jsi;

// ── Image provider ────────────────────────────────────────────────
class FrameProvider : public QQuickImageProvider {
public:
  FrameProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
  void setImage(const QImage& img) { m_image = img; }
  QImage requestImage(const QString&, QSize*, const QSize&) override {
    return m_image;
  }
private:
  QImage m_image;
};

// ── Screen ────────────────────────────────────────────────────────
class Screen : public QObject {
  Q_OBJECT
public:
  Screen(int w, int h, FrameProvider* fp, QObject* parent=nullptr)
    : QObject(parent), m_image(w,h,QImage::Format_ARGB32), m_fp(fp) {
    m_image.fill(Qt::white);
    m_fp->setImage(m_image);
  }

  Q_INVOKABLE void touchDown(double x, double y) {
    qDebug() << "[touch] down x=" << x << "y=" << y;
    emit touched(x, y);
  }
  Q_INVOKABLE void touchUp() {
    emit released();
  }
  Q_INVOKABLE void keyDown(int key, const QString& text) {
    emit keyPressed(key, text);
  }

  void render(RNNode* root) {
    m_image.fill(Qt::white);
    QPainter p(&m_image);
    p.setRenderHint(QPainter::Antialiasing);
    paintNode(p, root, 0, 0);
    p.end();
    m_fp->setImage(m_image);
    emit frameReady();
  }

  int W() const { return m_image.width(); }
  int H() const { return m_image.height(); }

signals:
  void frameReady();
  void touched(double x, double y);
  void released();
  void keyPressed(int key, QString text);

private:
  QImage m_image;
  FrameProvider* m_fp;
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

#include "main.moc"

int main(int argc, char* argv[]) {
  if (argc < 2) { qWarning("Usage: rn-layout <bundle.js>"); return 1; }

  QGuiApplication app(argc, argv);
  auto* fp  = new FrameProvider();
  QSize screenSize = QGuiApplication::primaryScreen()->size();
  int sw = screenSize.width()  > 0 ? screenSize.width()  : 1404;
  int sh = screenSize.height() > 0 ? screenSize.height() : 1872;
  qDebug() << "[host] screen" << sw << "x" << sh;
  auto* scr = new Screen(sw, sh, fp, &app);

  std::ifstream file(argv[1]);
  std::ostringstream ss; ss << file.rdbuf();
  std::string source = ss.str();

  auto runtime = facebook::hermes::makeHermesRuntime();
  NodeMap nodes;
  TouchCB tcb;

  // Input via Qt signals from QML MouseArea
  QObject::connect(scr, &Screen::touched,
    [&](double x, double y) {
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
    });
  QObject::connect(scr, &Screen::released,
    [&]() {
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
    });

  // Map Qt key codes to JS-friendly names. Printable characters arrive
  // via the `text` argument; we only need names for the special ones.
  auto qtKeyName = [](int key) -> std::string {
    switch (key) {
      case Qt::Key_Backspace: return "Backspace";
      case Qt::Key_Return:
      case Qt::Key_Enter:     return "Enter";
      case Qt::Key_Escape:    return "Escape";
      case Qt::Key_Tab:       return "Tab";
      case Qt::Key_Left:      return "ArrowLeft";
      case Qt::Key_Right:     return "ArrowRight";
      case Qt::Key_Up:        return "ArrowUp";
      case Qt::Key_Down:      return "ArrowDown";
      case Qt::Key_Home:      return "Home";
      case Qt::Key_End:       return "End";
      case Qt::Key_Delete:    return "Delete";
      default:                return "";
    }
  };

  QObject::connect(scr, &Screen::keyPressed,
    [&, qtKeyName](int key, QString text) {
      try {
        auto global = runtime->global();
        if (!global.hasProperty(*runtime, "__rmKeyDown")) return;
        auto fn = global.getPropertyAsFunction(*runtime, "__rmKeyDown");
        fn.call(*runtime,
          jsi::String::createFromUtf8(*runtime, qtKeyName(key)),
          jsi::String::createFromUtf8(*runtime, text.toStdString()));
      } catch (const jsi::JSError& e) {
        qWarning() << "[key] JS error:" << e.getMessage().c_str();
        qWarning() << "Stack:" << e.getStack().c_str();
      } catch (...) {}
    });

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

  try {
    runtime->evaluateJavaScript(
      std::make_unique<jsi::StringBuffer>(source), argv[1]);
    qDebug() << "[host] Bundle executed";
  } catch(const jsi::JSError& e) {
    qWarning() << "JS Error:" << e.getMessage().c_str();
    qWarning() << "Stack:" << e.getStack().c_str();
    return 1;
  }


  QQmlApplicationEngine engine;
  engine.addImageProvider("rnframe", fp);
  engine.rootContext()->setContextProperty("rnScreen", scr);
  engine.loadData(R"(
    import QtQuick
    import QtQuick.Window
    Window {
      width: Screen.width
      height: Screen.height
      visible: true
      Item {
        id: root
        anchors.fill: parent
        focus: true
        Keys.onPressed: (event) => {
          rnScreen.keyDown(event.key, event.text)
          event.accepted = true
        }
        Image {
          id: frame
          anchors.fill: parent
          cache: false
          source: "image://rnframe/frame"
        }
        MouseArea {
          anchors.fill: parent
          onPressed: {
            root.forceActiveFocus()
            rnScreen.touchDown(mouseX, mouseY)
          }
          onReleased: rnScreen.touchUp()
        }
      }
      Connections {
        target: rnScreen
        function onFrameReady() {
          frame.source = ""
          frame.source = "image://rnframe/frame"
        }
      }
    }
  )");

  return app.exec();
}
