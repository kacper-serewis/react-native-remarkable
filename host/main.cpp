#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QPainter>
#include <QImage>
#include <QObject>
#include <QDebug>
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
  }

  Q_INVOKABLE void touchDown(double x, double y) {
    qDebug() << "[touch] down x=" << x << "y=" << y;
    emit touched(x, y);
  }
  Q_INVOKABLE void touchUp() {
    emit released();
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
        YGConfigRef cfg = YGConfigNew();

        if (n > 1 && a[1].isObject()) {
          auto props = a[1].getObject(rt);

          // Dimensions
          double w = getNum(rt,props,"width",-1);
          double h = getNum(rt,props,"height",-1);
          double flex = getNum(rt,props,"flex",-1);
          if (w >= 0)   YGNodeStyleSetWidth(node->yoga, w);
          if (h >= 0)   YGNodeStyleSetHeight(node->yoga, h);
          if (flex >= 0) YGNodeStyleSetFlex(node->yoga, flex);

          // Flex layout
          std::string fd = getProp(rt,props,"flexDirection","column");
          YGNodeStyleSetFlexDirection(node->yoga,
            fd == "row" ? YGFlexDirectionRow : YGFlexDirectionColumn);

          std::string jc = getProp(rt,props,"justifyContent","");
          if (jc == "center")        YGNodeStyleSetJustifyContent(node->yoga, YGJustifyCenter);
          if (jc == "space-between") YGNodeStyleSetJustifyContent(node->yoga, YGJustifySpaceBetween);
          if (jc == "space-around")  YGNodeStyleSetJustifyContent(node->yoga, YGJustifySpaceAround);
          if (jc == "flex-end")      YGNodeStyleSetJustifyContent(node->yoga, YGJustifyFlexEnd);

          std::string ai = getProp(rt,props,"alignItems","");
          if (ai == "center")    YGNodeStyleSetAlignItems(node->yoga, YGAlignCenter);
          if (ai == "flex-end")  YGNodeStyleSetAlignItems(node->yoga, YGAlignFlexEnd);
          if (ai == "stretch")   YGNodeStyleSetAlignItems(node->yoga, YGAlignStretch);

          // Padding / margin
          double p  = getNum(rt,props,"padding",   -1);
          double ph = getNum(rt,props,"paddingH",  -1);
          double pv = getNum(rt,props,"paddingV",  -1);
          double pt = getNum(rt,props,"paddingTop",-1);
          double pb = getNum(rt,props,"paddingBottom",-1);
          double pl = getNum(rt,props,"paddingLeft",-1);
          double pr = getNum(rt,props,"paddingRight",-1);
          if (p  >= 0) YGNodeStyleSetPadding(node->yoga, YGEdgeAll,    p);
          if (ph >= 0) { YGNodeStyleSetPadding(node->yoga,YGEdgeLeft,ph);
                         YGNodeStyleSetPadding(node->yoga,YGEdgeRight,ph); }
          if (pv >= 0) { YGNodeStyleSetPadding(node->yoga,YGEdgeTop,pv);
                         YGNodeStyleSetPadding(node->yoga,YGEdgeBottom,pv); }
          if (pt >= 0) YGNodeStyleSetPadding(node->yoga, YGEdgeTop,    pt);
          if (pb >= 0) YGNodeStyleSetPadding(node->yoga, YGEdgeBottom, pb);
          if (pl >= 0) YGNodeStyleSetPadding(node->yoga, YGEdgeLeft,   pl);
          if (pr >= 0) YGNodeStyleSetPadding(node->yoga, YGEdgeRight,  pr);

          double mg = getNum(rt,props,"margin",-1);
          double mt = getNum(rt,props,"marginTop",-1);
          double mb = getNum(rt,props,"marginBottom",-1);
          if (mg >= 0) YGNodeStyleSetMargin(node->yoga, YGEdgeAll,    mg);
          if (mt >= 0) YGNodeStyleSetMargin(node->yoga, YGEdgeTop,    mt);
          if (mb >= 0) YGNodeStyleSetMargin(node->yoga, YGEdgeBottom, mb);

          // Visual
          std::string bg = getProp(rt,props,"backgroundColor","");
          if (!bg.empty()) node->backgroundColor = QColor(QString::fromStdString(bg));

          std::string col = getProp(rt,props,"color","#000000");
          node->color = QColor(QString::fromStdString(col));

          node->text      = getProp(rt,props,"text","");
          node->fontSize  = (int)getNum(rt,props,"fontSize",24);
          node->borderRadius = (int)getNum(rt,props,"borderRadius",0);
          node->borderWidth  = (float)getNum(rt,props,"borderWidth",0);
          std::string bc  = getProp(rt,props,"borderColor","");
          if (!bc.empty()) node->borderColor = QColor(QString::fromStdString(bc));
        }

        int id = nextId++;
        nodes[id] = node;
        return jsi::Value(id);
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
  auto* scr = new Screen(1404, 1872, fp, &app);

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
        }
      } catch(...) {}
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

  wire(*runtime, scr, nodes, tcb);

  try {
    runtime->evaluateJavaScript(
      std::make_unique<jsi::StringBuffer>(source), argv[1]);
    qDebug() << "[host] Bundle executed";
  } catch(const jsi::JSError& e) {
    qWarning() << "JS Error:" << e.getMessage().c_str(); return 1;
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
      Image {
        id: frame
        anchors.fill: parent
        cache: false
        source: "image://rnframe/frame"
      }
      MouseArea {
        anchors.fill: parent
        onPressed: rnScreen.touchDown(mouseX, mouseY)
        onReleased: rnScreen.touchUp()
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
