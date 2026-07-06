# react-native-remarkable

> React Native for the reMarkable Paper Pro

Write React apps in JSX and run them on your reMarkable Paper Pro.

## Stack

| Layer | Technology |
|---|---|
| JS Engine | Hermes |
| Layout | Yoga (Flexbox) |
| Renderer | QPainter → quill (vendor e-ink engine takeover) |
| Bridge | JSI (C++) |
| Display | libqsgepaper waveform engine via the epfb-re shim ([riddle/quill](https://github.com/MaximeRivest/riddle)) |
| Input | raw evdev (touchscreen, pen, Type Folio, power button) |

## Requirements

- reMarkable Paper Pro with developer mode enabled
- Docker (for building the C++ host)
- Node.js 18+
- macOS or Linux host

## Install a release (no build required)

Each tagged release publishes a pre-built `react-native-remarkable-vX.Y.Z.tar.gz` containing the ARM64 binary, the Hermes runtime, the JS bundle, and an installer.

**One-liner from the device** (the Paper Pro needs SSH and Wi-Fi):

```sh
ssh root@<device-ip> '
  curl -sSL https://github.com/kacper-serewis/react-native-remarkable/releases/latest/download/react-native-remarkable-vLATEST.tar.gz \
    | tar xz -C /tmp \
  && /tmp/react-native-remarkable-vLATEST/install.sh
'
```

**Or manually:**

1. Download the release tarball from the [Releases page](https://github.com/kacper-serewis/react-native-remarkable/releases).
2. `scp react-native-remarkable-*.tar.gz root@<ip>:/tmp/` and `ssh` into the device.
3. `cd /tmp && tar xzf react-native-remarkable-*.tar.gz && cd react-native-remarkable-* && ./install.sh`
4. Launch the app: `~/rn-app/start.sh`

`start.sh` stops `xochitl` (the default reMarkable UI) — the app takes over the e-ink engine directly. **Exit the app with a 5-finger tap or the power button.** To restore xochitl: reboot, or `systemctl start xochitl`.

If touch input appears mirrored or rotated, set `RN_TOUCH_SWAP_XY=1`, `RN_TOUCH_INVERT_X=1`, or `RN_TOUCH_INVERT_Y=1` when launching.

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
QPainter draw calls into the aux framebuffer
↓
quill (libquill.so — epfb-re shim over vendor libqsgepaper.so)
↓ dirty-rect swapBuffers
E-ink display (reMarkable Paper Pro)

Input flows the other way: raw evdev (`/dev/input/event*`) → touch/pen/keyboard readers in the host → `__rmTouchDown` / `__rmKeyDown` in JS. There is no window system: `xochitl` is stopped and this process drives the panel.## Device setup

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
- [x] ~~Partial refresh.~~ The host diffs each committed frame against the previous one and swaps only the dirty bounding box to glass (`Quality3` waveform). `N.fullRefresh()` is exposed to JS for a flashing ghost-removal clear.
- [ ] **Refresh-mode hints per node.** `refreshMode="fast" | "quality"` prop to choose the fastest (DU-ish, mode 0) vs full-quality (mode 4) waveforms per update. The quill C ABI already takes the mode per swap — plumb it through `N.commit`.
- [x] ~~Pen / stylus input.~~ The pen (evdev `marker` device) now drives the same pointer path as finger touch — taps work everywhere. A pressure/gesture stream for drawing apps is still TODO (quill's mode-0 swaps make low-latency ink feasible — see `riddle/quill/src/scribble.c`).

### Networking
- [x] ~~`fetch`~~ — implemented via `QNetworkAccessManager`. C++ host registers `N.fetch(url, opts)` returning a JS Promise; the inline polyfill exposes a browser-style `globalThis.fetch` with `.text()` / `.json()` / `.headers.get(name)`. Supports `GET`/`POST`/`PUT`/`DELETE`/`HEAD` plus arbitrary methods, request body (string), request headers, and a 30s default transfer timeout. Reply finishes on the Qt main thread, which is also the JSI thread, so promise resolution is thread-safe. No streaming, no `FormData`, no abort signal yet.

### Component library
- [ ] **`Pressable`** with proper press/release visual states (current `TouchableOpacity` doesn't even change opacity).
- [ ] **`ScrollView`** — Yoga `overflow: scroll`, scroll state in JS, clip rect in `paintNode`.
- [x] ~~`TextInput`~~ — physical keys (Type Folio): the host reads the keyboard evdev device directly, maps Linux keycodes to JS-friendly names (`"Backspace"`, `"Enter"`, `"ArrowLeft"`, …) and forwards `(keyName, text)` to a global `__rmKeyDown`. JS-side `RemarkableRenderer` exposes `setKeyHandler` / `clearKeyHandler` / `dispatchKey`; `TextInput` registers a handler when focused (tap-to-focus) and renders value + cursor (`|`).
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
- [x] ~~Drop QML.~~ **Done, via quill.** The earlier attempt failed because the epaper QPA only exposes geometry + input and display flushing lived in the QtQuick scenegraph backend. quill (from the [riddle](https://github.com/MaximeRivest/riddle) submodule) sidesteps Qt's display stack entirely: an epfb-re-style QImage-constructor interposition shim over the vendor `libqsgepaper.so` waveform engine hands us the aux framebuffer and `swapBuffers` directly. The host paints with QPainter into that buffer and swaps dirty rects itself; QtQuick, QML, and the QPA display path are gone (`QGuiApplication` runs on the offscreen platform, kept only for the font database). Input moved to raw evdev.
- [x] ~~Concurrent React.~~ `createContainer` now passes `tag: 1` (ConcurrentRoot) — Suspense, transitions, and `useDeferredValue` are available.
- [ ] **`requestAnimationFrame`** driven by a 10–15 Hz `QTimer` for the cases where animation makes sense on e-paper (drag handles, sliders).

## Acknowledgements

Built on [Hermes](https://hermesengine.dev),
[Yoga](https://yogalayout.dev),
[Qt](https://qt.io),
[quill from riddle](https://github.com/MaximeRivest/riddle) (Maxime Rivest) with
[asivery's epfb-re](https://github.com/asivery) interposition technique, and the
[reMarkable developer portal](https://developer.remarkable.com).
