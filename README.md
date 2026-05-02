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

## Acknowledgements

Built on [Hermes](https://hermesengine.dev),
[Yoga](https://yogalayout.dev),
[Qt](https://qt.io), and the
[reMarkable developer portal](https://developer.remarkable.com).
