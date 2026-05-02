// Lightweight component stubs for reMarkable renderer
// These work with JSX via React.createElement

export const View = (props) => ({
  type: 'view',
  props,
});

export const Text = (props) => ({
  type: 'text',
  props,
});

export const TouchableOpacity = (props) => ({
  type: 'view',
  props,
});

export const StyleSheet = {
  create: (styles) => styles,
};
