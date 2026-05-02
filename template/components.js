// Host element identifiers for the reMarkable renderer.
// JSX (<View />) becomes React.createElement(View, ...). We want it to
// become createElement('view', ...) so the reconciler's createInstance
// receives the right type string.

import React, { useEffect, useReducer, useRef } from "react";
import { setKeyHandler, clearKeyHandler } from "./RemarkableRenderer";

export const View = "view";
export const Text = "text";
export const TouchableOpacity = "view";

export const StyleSheet = {
  create: (styles) => styles,
};

// Single source of truth for the focused input. Tap-to-focus replaces
// it; the previously-focused input re-renders without the cursor.
let _focusedKey = null;
const _focusListeners = new Set();
function setFocus(key) {
  if (_focusedKey === key) return;
  _focusedKey = key;
  for (const l of _focusListeners) l();
}

function useFocusToken() {
  const tokenRef = useRef(null);
  if (tokenRef.current === null) tokenRef.current = {};
  const [, force] = useReducer((x) => x + 1, 0);
  useEffect(() => {
    _focusListeners.add(force);
    return () => {
      _focusListeners.delete(force);
      if (_focusedKey === tokenRef.current) _focusedKey = null;
    };
  }, []);
  return [tokenRef.current, _focusedKey === tokenRef.current];
}

export function TextInput({
  value = "",
  onChangeText,
  placeholder = "",
  style,
  textStyle,
}) {
  const [token, focused] = useFocusToken();

  useEffect(() => {
    if (!focused) return;
    const handler = (keyName, text) => {
      if (keyName === "Backspace") {
        onChangeText && onChangeText(value.slice(0, -1));
      } else if (keyName === "Enter") {
        onChangeText && onChangeText(value + "\n");
      } else if (text) {
        onChangeText && onChangeText(value + text);
      }
    };
    setKeyHandler(handler);
    return () => clearKeyHandler(handler);
  }, [focused, value, onChangeText]);

  const display = (value || (focused ? "" : placeholder)) + (focused ? "|" : "");

  return React.createElement(
    "view",
    {
      style: {
        backgroundColor: "#ffffff",
        borderWidth: focused ? 2 : 1,
        borderColor: focused ? "#000000" : "#888888",
        borderRadius: 8,
        padding: 12,
        height: 60,
        justifyContent: "center",
        ...style,
      },
      onPress: () => setFocus(token),
    },
    React.createElement(
      "text",
      {
        style: {
          fontSize: 24,
          color: value ? "#000000" : "#888888",
          ...textStyle,
        },
      },
      display,
    ),
  );
}
