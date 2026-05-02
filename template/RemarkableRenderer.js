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
  return { id, type, props, children: [] };
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
    return { id, type: "text", props: {}, children: [] };
  },

  appendInitialChild(parent, child) {
    N.appendChild(parent.id, child.id);
    parent.children.push(child);
  },

  appendChild(parent, child) {
    N.appendChild(parent.id, child.id);
    parent.children.push(child);
  },

  appendChildToContainer(container, child) {
    container.children.push(child);
    container.rootId = child.id;
  },

  insertBefore(parent, child) {
    N.appendChild(parent.id, child.id);
    parent.children.push(child);
  },

  insertInContainerBefore(container, child) {
    container.children.push(child);
    container.rootId = child.id;
  },

  removeChild(parent, child) {
    parent.children = parent.children.filter((c) => c !== child);
  },

  removeChildFromContainer(container, child) {
    container.children = container.children.filter((c) => c !== child);
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

  clearContainer() {},
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
function hitTest(node) {
  if (!node) return false;
  const press = node.props && (node.props.onPress || node.props.onClick);
  if (press) {
    press();
    return true;
  }
  for (const child of node.children || []) {
    if (hitTest(child)) return true;
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
      0,                              // tag (LegacyRoot)
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
global.__rmTouchDown = function () {
  if (!_container) return;
  for (const child of _container.children) hitTest(child);
};
global.__rmTouchUp = function () {};
