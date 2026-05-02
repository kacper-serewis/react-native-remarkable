import ReactReconciler from "react-reconciler";
import { DefaultEventPriority } from "react-reconciler/constants";

const W = N.screenWidth;
const H = N.screenHeight;
export const W_SCREEN = W;
export const H_SCREEN = H;

// ── Style → Yoga node props ───────────────────────────────────────
function styleToProps(style = {}) {
  const p = {};
  const keys = [
    "width",
    "height",
    "flex",
    "flexDirection",
    "justifyContent",
    "alignItems",
    "padding",
    "paddingTop",
    "paddingBottom",
    "paddingLeft",
    "paddingRight",
    "margin",
    "marginTop",
    "marginBottom",
    "backgroundColor",
    "borderRadius",
    "borderWidth",
    "borderColor",
    "color",
    "fontSize",
    "position",
    "top",
    "bottom",
    "left",
    "right",
  ];
  keys.forEach((k) => {
    if (style[k] !== undefined) p[k] = style[k];
  });
  if (style.paddingHorizontal !== undefined) {
    p.paddingLeft = style.paddingHorizontal;
    p.paddingRight = style.paddingHorizontal;
  }
  if (style.paddingVertical !== undefined) {
    p.paddingTop = style.paddingVertical;
    p.paddingBottom = style.paddingVertical;
  }
  return p;
}

function childrenToText(children) {
  if (children == null) return "";
  if (typeof children === "string" || typeof children === "number")
    return String(children);
  if (Array.isArray(children)) return children.map(childrenToText).join("");
  return "";
}

// id → instance, used by hit testing to walk up from the touched node
const _byId = new Map();

// Drop a node and all its descendants from the id map. Called when a
// subtree is removed so we don't keep dead instances live for hit
// testing.
function detachSubtree(node) {
  if (!node) return;
  for (const c of node.children || []) detachSubtree(c);
  _byId.delete(node.id);
}

// ── Instance ──────────────────────────────────────────────────────
function createInstance(type, props) {
  const style = props.style || {};
  const isText = type === "text";
  const nProps = styleToProps(style);

  if (isText) {
    nProps.text = childrenToText(props.children);
    nProps.color = nProps.color || "#000000";
    nProps.fontSize = nProps.fontSize || 24;
  }

  const id = N.createNode(isText ? "text" : "view", nProps);
  const instance = { id, type, props, children: [], parent: null };
  _byId.set(id, instance);
  return instance;
}

// ── Reconciler host config ────────────────────────────────────────
let _currentUpdatePriority = DefaultEventPriority;

const hostConfig = {
  supportsMutation: true,
  supportsPersistence: false,
  supportsHydration: false,
  isPrimaryRenderer: true,
  noTimeout: -1,

  now: () => Date.now(),
  scheduleTimeout: setTimeout,
  cancelTimeout: clearTimeout,

  getCurrentEventPriority: () => DefaultEventPriority,
  resolveUpdatePriority: () =>
    _currentUpdatePriority || DefaultEventPriority,
  getCurrentUpdatePriority: () => _currentUpdatePriority,
  setCurrentUpdatePriority: (p) => {
    _currentUpdatePriority = p;
  },
  getInstanceFromNode: () => null,
  beforeActiveInstanceBlur: () => {},
  afterActiveInstanceBlur: () => {},
  prepareScopeUpdate: () => {},
  getInstanceFromScope: () => null,
  detachDeletedInstance: () => {},

  createInstance,

  createTextInstance(text) {
    const id = N.createNode("text", {
      text: String(text),
      fontSize: 24,
      color: "#000000",
    });
    const instance = { id, type: "text", props: {}, children: [], parent: null };
    _byId.set(id, instance);
    return instance;
  },

  appendInitialChild(parent, child) {
    N.appendChild(parent.id, child.id);
    parent.children.push(child);
    child.parent = parent;
  },

  appendChild(parent, child) {
    N.appendChild(parent.id, child.id);
    parent.children.push(child);
    child.parent = parent;
  },

  appendChildToContainer(container, child) {
    container.children.push(child);
    container.rootId = child.id;
    child.parent = null;
  },

  insertBefore(parent, child) {
    N.appendChild(parent.id, child.id);
    parent.children.push(child);
    child.parent = parent;
  },

  insertInContainerBefore(container, child) {
    container.children.push(child);
    container.rootId = child.id;
    child.parent = null;
  },

  removeChild(parent, child) {
    parent.children = parent.children.filter((c) => c !== child);
    detachSubtree(child);
    N.removeChild(parent.id, child.id);
  },

  removeChildFromContainer(container, child) {
    container.children = container.children.filter((c) => c !== child);
    detachSubtree(child);
    N.destroyNode(child.id);
  },

  // reconciler 0.33: (instance, type, oldProps, newProps, finishedWork)
  commitUpdate(instance, type, _oldProps, newProps) {
    const style = newProps.style || {};
    const isText = type === "text";
    const nProps = styleToProps(style);
    if (isText) {
      nProps.text = childrenToText(newProps.children);
      nProps.color = nProps.color || "#000000";
      nProps.fontSize = nProps.fontSize || 24;
    }
    instance.props = newProps;
    N.updateNode(instance.id, nProps);
  },

  commitTextUpdate(instance, _old, newText) {
    instance.props = { ...instance.props, children: newText };
    N.updateNode(instance.id, { text: String(newText) });
  },

  finalizeInitialChildren() {
    return false;
  },
  prepareForCommit() {
    return null;
  },

  resetAfterCommit(container) {
    if (container.rootId !== undefined) {
      N.commit(container.rootId);
    }
  },

  getPublicInstance(i) {
    return i;
  },
  getRootHostContext() {
    return {};
  },
  getChildHostContext() {
    return {};
  },

  shouldSetTextContent(type, props) {
    return (
      type === "text" ||
      typeof props.children === "string" ||
      typeof props.children === "number"
    );
  },

  clearContainer(container) {
    for (const child of container.children) {
      detachSubtree(child);
      N.destroyNode(child.id);
    }
    container.children = [];
    container.rootId = undefined;
  },
  hideInstance() {},
  unhideInstance() {},
  hideTextInstance() {},
  unhideTextInstance() {},
  maybeSuspendLane() {},

  // ── React 19 / reconciler 0.33 additions (no-op safe defaults) ──
  supportsMicrotasks: false,
  supportsResources: false,
  supportsSingletons: false,
  supportsTestSelectors: false,

  NotPendingTransition: null,
  HostTransitionContext: {
    $$typeof: Symbol.for("react.context"),
    Provider: null,
    Consumer: null,
    _currentValue: null,
    _currentValue2: null,
    _threadCount: 0,
  },

  maySuspendCommit: () => false,
  maySuspendCommitInSyncRender: () => false,
  maySuspendCommitOnUpdate: () => false,
  mayResourceSuspendCommit: () => false,
  preloadInstance: () => true,
  startSuspendingCommit: () => {},
  suspendInstance: () => {},
  suspendResource: () => {},
  waitForCommitToBeReady: () => null,

  requestPostPaintCallback: () => {},
  resetFormInstance: () => {},
  shouldAttemptEagerTransition: () => false,
  trackSchedulerEvent: () => {},
  resolveEventType: () => null,
  resolveEventTimeStamp: () => -1.1,

  setFocusIfFocusable: () => false,
  setupIntersectionObserver: () => {},
  bindToConsole: (_methodName, args) => args,

  getBoundingRect: () => null,
  getTextContent: () => "",
  hasInstanceAffectedParent: () => false,
  hasInstanceChanged: () => false,
  matchAccessibilityRole: () => false,
};

const reconciler = ReactReconciler(hostConfig);

// ── Touch hit test ────────────────────────────────────────────────
// Native side returns the deepest-matching node id. We then bubble up
// firing the first node with onPress (event bubbling, like the DOM).
function fireAt(id) {
  let node = _byId.get(id);
  while (node) {
    const press = node.props && (node.props.onPress || node.props.onClick);
    if (press) {
      try { press(); } catch (e) { console.error("onPress error:", e); }
      return true;
    }
    node = node.parent;
  }
  return false;
}

// ── Public API ────────────────────────────────────────────────────
let _container = null;
let _root = null;

export function render(element) {
  if (!_container) {
    _container = { children: [], rootId: undefined };
    const logError = (label) => (err, info) => {
      const stack = err && err.stack ? err.stack : "";
      const compStack = info && info.componentStack ? info.componentStack : "";
      console.error(
        "[" + label + "] " + (err && err.message ? err.message : err) +
        (stack ? "\n" + stack : "") +
        (compStack ? "\nComponent: " + compStack : ""),
      );
    };
    _root = reconciler.createContainer(
      _container,                     // containerInfo
      1,                              // tag (ConcurrentRoot)
      null,                           // hydrationCallbacks
      false,                          // isStrictMode
      null,                           // concurrentUpdatesByDefaultOverride
      "",                             // identifierPrefix
      logError("onUncaughtError"),    // onUncaughtError
      logError("onCaughtError"),      // onCaughtError
      logError("onRecoverableError"), // onRecoverableError
      null,                           // onDefaultTransitionIndicator
    );
  }
  reconciler.updateContainer(element, _root, null, null);
}

// Wire touch events
global.__rmTouchDown = function (x, y) {
  if (!_container || _container.rootId === undefined) return;
  const id = N.hitTest(_container.rootId, x, y);
  if (id !== -1) fireAt(id);
};
global.__rmTouchUp = function () {};

// ── Keyboard / focus ──────────────────────────────────────────────
// Single focused key handler at a time. TextInput (or any other
// focusable component) registers itself via setKeyHandler. The same
// path serves both physical key events (from the C++ host via
// __rmKeyDown) and synthetic key events from the on-screen keyboard.
let _keyHandler = null;
export function setKeyHandler(fn) {
  _keyHandler = fn;
}
export function clearKeyHandler(fn) {
  if (_keyHandler === fn) _keyHandler = null;
}
export function dispatchKey(keyName, text) {
  if (_keyHandler) {
    try { _keyHandler(keyName, text); }
    catch (e) { console.error("key handler error:", e); }
  }
}

global.__rmKeyDown = function (keyName, text) {
  dispatchKey(keyName, text);
};
