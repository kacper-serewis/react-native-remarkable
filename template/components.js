// Host element identifiers for the reMarkable renderer.
// JSX (<View />) becomes React.createElement(View, ...). We want it to
// become createElement('view', ...) so the reconciler's createInstance
// receives the right type string.

export const View = 'view';
export const Text = 'text';
export const TouchableOpacity = 'view';

export const StyleSheet = {
  create: (styles) => styles,
};
