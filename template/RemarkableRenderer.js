const W = N.screenWidth;
const H = N.screenHeight;
export const W_SCREEN = W;
export const H_SCREEN = H;

function styleToProps(style = {}) {
  const p = {};
  const keys = [
    'width','height','flex','flexDirection','justifyContent','alignItems',
    'padding','paddingTop','paddingBottom','paddingLeft','paddingRight',
    'margin','marginTop','marginBottom',
    'backgroundColor','borderRadius','borderWidth','borderColor',
    'color','fontSize',
  ];
  keys.forEach(k => { if (style[k] !== undefined) p[k] = style[k]; });
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
  if (children === null || children === undefined) return '';
  if (typeof children === 'string' || typeof children === 'number')
    return String(children);
  if (Array.isArray(children))
    return children.map(childrenToText).join('');
  return '';
}

function buildNode(element) {
  if (element === null || element === undefined) return null;
  if (typeof element === 'string' || typeof element === 'number') {
    return {
      id: N.createNode('text', { text: String(element), fontSize: 24, color: '#000000' }),
      onPress: null, children: [],
    };
  }

  const { type, props } = element;
  if (!props) return null;
  if (typeof type === 'function') return buildNode(type(props));

  const style = props.style || {};
  const isText = type === 'text' || type === 'Text' || type === 'RCTText';
  const nodeType = isText ? 'text' : 'view';
  const nodeProps = styleToProps(style);

  if (isText) {
    nodeProps.text = childrenToText(props.children);
    if (!nodeProps.color)    nodeProps.color    = '#000000';
    if (!nodeProps.fontSize) nodeProps.fontSize = 24;
  }

  const id = N.createNode(nodeType, nodeProps);
  const node = { id, onPress: props.onPress || null, children: [] };

  if (!isText && props.children) {
    const arr = Array.isArray(props.children)
      ? props.children : [props.children];
    arr.forEach(child => {
      if (typeof child === 'string' || typeof child === 'number') return;
      const childNode = buildNode(child);
      if (childNode) {
        N.appendChild(id, childNode.id);
        node.children.push(childNode);
      }
    });
  }

  return node;
}

function hitTest(node) {
  if (!node) return false;
  if (node.onPress) { node.onPress(); return true; }
  for (const child of node.children) {
    if (hitTest(child)) return true;
  }
  return false;
}

let currentRoot = null;
let rerenderFn  = null;

export function render(elementOrFn) {
  rerenderFn = () => {
    stateIndex = 0;
    const element = typeof elementOrFn === 'function'
      ? elementOrFn() : elementOrFn;
    currentRoot = buildNode(element);
    if (currentRoot) N.commit(currentRoot.id);
  };
  rerenderFn();
}

global.__rmTouchDown = function(x, y) {
  if (currentRoot) hitTest(currentRoot);
};
global.__rmTouchUp = function() {};

let stateStore = [];
let stateIndex = 0;

export function useState(initial) {
  const idx = stateIndex++;
  if (stateStore[idx] === undefined) stateStore[idx] = initial;
  const setState = (newVal) => {
    stateStore[idx] = typeof newVal === 'function'
      ? newVal(stateStore[idx]) : newVal;
    if (rerenderFn) rerenderFn();
  };
  return [stateStore[idx], setState];
}
