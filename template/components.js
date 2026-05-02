// Host element identifiers for the reMarkable renderer.
// JSX (<View />) becomes React.createElement(View, ...). We want it to
// become createElement('view', ...) so the reconciler's createInstance
// receives the right type string.

import React, { useEffect, useReducer, useRef, useState } from "react";
import {
  setKeyHandler,
  clearKeyHandler,
  dispatchKey,
  W_SCREEN,
} from "./RemarkableRenderer";

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

// Component that re-renders whenever focus changes anywhere. Used by
// the on-screen keyboard to auto-show / hide.
export function useHasFocusedInput() {
  const [, force] = useReducer((x) => x + 1, 0);
  useEffect(() => {
    _focusListeners.add(force);
    return () => _focusListeners.delete(force);
  }, []);
  return _focusedKey !== null;
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

// ── On-screen keyboard ────────────────────────────────────────────
// Auto-shows when any TextInput is focused; calls into the same
// dispatchKey path the physical-keyboard handler uses, so TextInput's
// onChangeText logic doesn't care which keyboard the keystroke came
// from.

const ROW_LETTERS = [
  ["q", "w", "e", "r", "t", "y", "u", "i", "o", "p"],
  ["a", "s", "d", "f", "g", "h", "j", "k", "l"],
  ["z", "x", "c", "v", "b", "n", "m"],
];
const ROW_SYMBOLS = [
  ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"],
  ["-", "/", ":", ";", "(", ")", "$", "&", "@", "\""],
  [".", ",", "?", "!", "'"],
];

function Key({ label, width, onPress, dark }) {
  return React.createElement(
    "view",
    {
      style: {
        width,
        height: 80,
        margin: 4,
        borderRadius: 8,
        borderWidth: 1,
        borderColor: "#000000",
        backgroundColor: dark ? "#000000" : "#ffffff",
        justifyContent: "center",
        alignItems: "center",
      },
      onPress,
    },
    React.createElement(
      "text",
      {
        style: {
          fontSize: 24,
          color: dark ? "#ffffff" : "#000000",
          width: width - 8,
          height: 32,
        },
      },
      label,
    ),
  );
}

export function OnScreenKeyboard({ width = W_SCREEN }) {
  const visible = useHasFocusedInput();
  const [shift, setShift] = useState(false);
  const [symbols, setSymbols] = useState(false);

  if (!visible) return null;

  const rows = symbols ? ROW_SYMBOLS : ROW_LETTERS;
  const keyW = Math.floor((width - 8 * 11) / 10);

  const tap = (label) => {
    const ch = !symbols && shift ? label.toUpperCase() : label;
    dispatchKey("", ch);
    if (shift) setShift(false);
  };

  return React.createElement(
    "view",
    {
      style: {
        position: "absolute",
        bottom: 0,
        left: 0,
        right: 0,
        backgroundColor: "#f0f0f0",
        padding: 8,
        flexDirection: "column",
        alignItems: "center",
      },
    },
    // Rows of letter/symbol keys
    ...rows.map((row, i) =>
      React.createElement(
        "view",
        {
          key: "row" + i,
          style: {
            flexDirection: "row",
            justifyContent: "center",
          },
        },
        ...row.map((ch) =>
          React.createElement(Key, {
            key: ch,
            label: !symbols && shift ? ch.toUpperCase() : ch,
            width: keyW,
            onPress: () => tap(ch),
          }),
        ),
      ),
    ),
    // Bottom row: shift / symbols / space / backspace / enter
    React.createElement(
      "view",
      {
        style: { flexDirection: "row", justifyContent: "center" },
      },
      React.createElement(Key, {
        label: symbols ? "ABC" : shift ? "SHIFT*" : "shift",
        width: keyW * 1.5,
        dark: shift,
        onPress: () => (symbols ? setSymbols(false) : setShift((s) => !s)),
      }),
      React.createElement(Key, {
        label: "123",
        width: keyW * 1.2,
        dark: symbols,
        onPress: () => setSymbols((s) => !s),
      }),
      React.createElement(Key, {
        label: "space",
        width: keyW * 4,
        onPress: () => dispatchKey("", " "),
      }),
      React.createElement(Key, {
        label: "⌫",
        width: keyW * 1.2,
        onPress: () => dispatchKey("Backspace", ""),
      }),
      React.createElement(Key, {
        label: "↵",
        width: keyW * 1.5,
        dark: true,
        onPress: () => dispatchKey("Enter", ""),
      }),
    ),
  );
}
