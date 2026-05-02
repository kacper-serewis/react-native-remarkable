# react-native-remarkable

> React Native for the reMarkable Paper Pro

Write React apps in JSX and run them on your reMarkable Paper Pro.

## Stack

| Layer | Technology |
|---|---|
| JS Engine | Hermes |
| Layout | Yoga (Flexbox) |
| Renderer | Qt 6 + epaper backend |
| Bridge | JSI (C++) |
| Display | DRM/KMS + e-ink waveform |

## Requirements

- reMarkable Paper Pro with developer mode enabled
- Docker (for building the C++ host)
- Node.js 18+
- macOS or Linux host

## Quick Start

```bash
git clone https://github.com/yourname/react-native-remarkable
cd react-native-remarkable

# Install JS deps
cd template && npm install && cd ..

# Build everything (C++ host + JS bundle)
./scripts/build.sh

# Deploy to device (default: 192.168.1.196)
./scripts/deploy.sh 192.168.1.196
```

## Development workflow

Edit `template/index.js`, then:

```bash
# Fast JS-only rebuild + deploy
./scripts/dev.sh 192.168.1.196
```

## Writing apps

```jsx
import React from 'react';
import { render, useState } from '../renderer/RemarkableRenderer';
import { View, Text, TouchableOpacity, StyleSheet } from '../renderer/components';

const styles = StyleSheet.create({
  container: { flex: 1, justifyContent: 'center', alignItems: 'center' },
  button: { backgroundColor: '#000', padding: 20, borderRadius: 12 },
  text: { color: '#fff', fontSize: 32 },
});

function App() {
  const [count, setCount] = useState(0);
  return (
    <View style={styles.container}>
      <TouchableOpacity style={styles.button} onPress={() => setCount(c => c + 1)}>
        <Text style={styles.text}>Tapped: {count}</Text>
      </TouchableOpacity>
    </View>
  );
}

render(App);
```

## Supported components

- `View` — Flexbox container
- `Text` — Text rendering
- `TouchableOpacity` — Tappable container
- `StyleSheet` — Style definitions

## Supported style props

`width`, `height`, `flex`, `flexDirection`, `justifyContent`, `alignItems`,
`padding`, `paddingHorizontal`, `paddingVertical`, `margin`, `marginTop`,
`marginBottom`, `backgroundColor`, `borderRadius`, `borderWidth`,
`borderColor`, `color`, `fontSize`

## ArchitectureJS (JSX/React)
↓ RemarkableRenderer.js
N.createNode() / N.appendChild() / N.commit()
↓ JSI bridge (C++)
Yoga layout pass
↓
QPainter draw calls
↓
Qt epaper platform plugin
↓
E-ink display (reMarkable Paper Pro)## Device setup

1. Enable developer mode: Settings → Security → Developer mode
2. Find your device IP: Settings → Wi-Fi → tap network
3. Connect: `ssh root@<ip>`

## Roadmap / TODO

### Renderer hardening
- [ ] **Prop removal on update.** `applyProps` only sets values; if a re-render drops a prop (e.g. `backgroundColor`) the native node keeps the old value. Diff `oldProps` vs `newProps` in `commitUpdate` and reset cleared fields.
- [ ] **Memory leaks.** `removeChild` filters from JS but the native `nodes` map and Yoga tree grow forever. Add `N.removeChild(parentId, childId)` and `N.destroyNode(id)` and call them from JS `removeChild` / `removeChildFromContainer`.
- [ ] **`clearContainer`.** Currently a no-op. Will silently break when an app calls `render(null)` or fully unmounts.
- [ ] **More style props.** `flexWrap`, `position: 'absolute'` + `top`/`left`/`right`/`bottom`, `alignSelf`, `gap`, `aspectRatio`, `opacity`. Mostly one line each in `applyProps`.
- [ ] **Touch up + long-press.** `__rmTouchUp` is a no-op. Wire `onPressOut` and add long-press detection in JS via `setTimeout`.

### E-paper specific
- [x] ~~Partial refresh.~~ Already happening — the reMarkable epaper QPA backend diffs framebuffers and only drives waveform updates for changed regions. We repaint the full `QImage` each commit; the driver slices it for us.
- [ ] **Refresh-mode hints per node.** `refreshMode="fast" | "quality"` prop to choose A2 (fast, mono) vs GC16 (quality) waveforms. Critical for animations vs. static text.
- [ ] **Pen / stylus input.** Paper Pro has a Wacom digitizer; we currently only handle finger touch via `MouseArea`. Read pen evdev events and expose a JS gesture stream.

### Networking
- [x] ~~`fetch`~~ — implemented via `QNetworkAccessManager`. C++ host registers `N.fetch(url, opts)` returning a JS Promise; the inline polyfill exposes a browser-style `globalThis.fetch` with `.text()` / `.json()` / `.headers.get(name)`. Supports `GET`/`POST`/`PUT`/`DELETE`/`HEAD` plus arbitrary methods, request body (string), request headers, and a 30s default transfer timeout. Reply finishes on the Qt main thread, which is also the JSI thread, so promise resolution is thread-safe. No streaming, no `FormData`, no abort signal yet.

### Component library
- [ ] **`Pressable`** with proper press/release visual states (current `TouchableOpacity` doesn't even change opacity).
- [ ] **`ScrollView`** — Yoga `overflow: scroll`, scroll state in JS, clip rect in `paintNode`.
- [x] ~~`TextInput`~~ — physical keys (BT/folio): QML root `Item` captures `Keys.onPressed`, the C++ host maps Qt key codes to JS-friendly names (`"Backspace"`, `"Enter"`, `"ArrowLeft"`, …) and forwards `(keyName, text)` to a global `__rmKeyDown`. JS-side `RemarkableRenderer` exposes `setKeyHandler` / `clearKeyHandler` / `dispatchKey`; `TextInput` registers a handler when focused (tap-to-focus) and renders value + cursor (`|`).
- [x] ~~On-screen keyboard~~ — **Custom**, rendered with our own `<View>`/`<Text>` primitives. There is no system OSK to call: reMarkable's keyboard is QML embedded inside the closed-source `xochitl` app, not a system input-method service, and the QPA `epaperkeyboardhandler` is just an evdev forwarder for physical keys (its name is misleading). `qtvirtualkeyboard` isn't shipped on the device firmware. So `OnScreenKeyboard` is our own QWERTY/symbols layout that auto-shows whenever any `TextInput` is focused (via `useHasFocusedInput`); each key calls `dispatchKey()` which feeds the same handler physical keys use, so `TextInput` is agnostic to input source. Overlays the bottom of the screen via `position: 'absolute'` (added to `applyProps` and `styleToProps` to support this). Embedding `qtvirtualkeyboard` is a possible future option but means cross-compiling the module and shipping its plugin alongside the binary.
- [ ] **`Image`** — load PNG/JPEG via Qt and paint into a node's bounds.

### Developer experience
- [ ] **Hot reload.** Add a `__rmReload(source)` JSI host function and a tiny client in `dev.sh` that watches `dist/remarkable.bundle.js` and pushes it over the existing SSH session — kill `scp` + restart loop.
- [x] ~~Source maps.~~ Metro now emits `dist/remarkable.bundle.js.map`. `scripts/translate-stack.js` reads stdin and rewrites any `bundle.js:LINE:COL` reference to `original-source:LINE:COL` using `@jridgewell/trace-mapping`. `dev.sh` and `deploy.sh` pipe device output through it. Translation runs on the dev machine — nothing extra ships to the device.
- [ ] **Public package.** Promote `template/components.js` and `template/RemarkableRenderer.js` into a published `react-remarkable` package so apps can `npm install` instead of copying the template.
- [ ] **TypeScript types.** Ship `.d.ts` for the host components and the `N.*` JSI surface.

### Build / deploy polish
- [ ] **Strip `rn-layout`.** Binary is ~330 KB; `strip` cuts that significantly.
- [ ] **`.watchmanconfig`.** Suppresses the harmless Metro warning each bundle.
- [ ] Document the JS-only fast path (`dev.sh`) more prominently.

### Bigger swings
- [ ] ~~Drop QML.~~ **Not viable on Paper Pro.** Tried it: replacing the QML `Image` with `QRasterWindow` produces a window that never draws anything (not even a clear). The reMarkable epaper QPA plugin appears to expose only screen geometry + input — display flushing to the e-ink panel is done by the dedicated **QtQuick scenegraph backend** (`QT_QUICK_BACKEND=epaper`), not by `QPlatformBackingStore::flush()`. So removing QtQuick removes the only working display path. Doing this for real would require writing a custom QPA backing store implementation that drives the e-ink waveform driver — much bigger than the binary-size win is worth.
- [x] ~~Concurrent React.~~ `createContainer` now passes `tag: 1` (ConcurrentRoot) — Suspense, transitions, and `useDeferredValue` are available.
- [ ] **`requestAnimationFrame`** driven by a 10–15 Hz `QTimer` for the cases where animation makes sense on e-paper (drag handles, sliders).

## Acknowledgements

Built on [Hermes](https://hermesengine.dev),
[Yoga](https://yogalayout.dev),
[Qt](https://qt.io), and the
[reMarkable developer portal](https://developer.remarkable.com).
