import React, {
  useState,
  useEffect,
  useRef,
  useCallback,
  useMemo,
  useTransition,
  useDeferredValue,
} from "react";
import { render, W_SCREEN, H_SCREEN } from "./RemarkableRenderer";
import { View, Text, TouchableOpacity, StyleSheet } from "./components";

const W = W_SCREEN;
const H = H_SCREEN;

const styles = StyleSheet.create({
  root: {
    width: W,
    height: H,
    flexDirection: "column",
    backgroundColor: "#ffffff",
  },
  header: {
    width: W,
    height: 80,
    backgroundColor: "#000000",
    justifyContent: "center",
    paddingHorizontal: 24,
  },
  headerText: { fontSize: 32, color: "#ffffff", width: W - 48, height: 80 },
  body: {
    flex: 1,
    flexDirection: "column",
    alignItems: "center",
    justifyContent: "center",
    padding: 40,
  },
  button: {
    width: W - 80,
    height: 110,
    backgroundColor: "#000000",
    borderRadius: 16,
    justifyContent: "center",
    alignItems: "center",
    marginBottom: 24,
  },
  btnGreen: {
    width: W - 80,
    height: 90,
    backgroundColor: "#007700",
    borderRadius: 16,
    justifyContent: "center",
    alignItems: "center",
    marginBottom: 24,
  },
  btnRed: {
    width: W - 80,
    height: 90,
    backgroundColor: "#cc0000",
    borderRadius: 16,
    justifyContent: "center",
    alignItems: "center",
    marginBottom: 24,
  },
  btnGrey: {
    width: W - 80,
    height: 90,
    backgroundColor: "#444444",
    borderRadius: 16,
    justifyContent: "center",
    alignItems: "center",
    marginBottom: 24,
  },
  buttonText: { fontSize: 36, color: "#ffffff", width: W - 80, height: 110 },
  buttonTextSm: { fontSize: 28, color: "#ffffff", width: W - 80, height: 90 },
  row: { width: W - 80, flexDirection: "row", justifyContent: "space-between" },
  card: {
    flex: 1,
    height: 130,
    backgroundColor: "#f0f0f0",
    borderRadius: 12,
    margin: 8,
    justifyContent: "center",
    alignItems: "center",
  },
  cardLabel: { fontSize: 18, color: "#888888", width: 160, height: 36 },
  cardValue: { fontSize: 28, color: "#000000", width: 160, height: 48 },
  concurrent: {
    width: W - 80,
    marginTop: 24,
    padding: 20,
    backgroundColor: "#f0f0f0",
    borderRadius: 12,
  },
  concurrentTitle: {
    fontSize: 22,
    color: "#000000",
    width: W - 120,
    height: 32,
  },
  concurrentRow: {
    width: W - 120,
    flexDirection: "row",
    justifyContent: "space-between",
    height: 40,
    marginTop: 8,
  },
  concurrentLabel: { fontSize: 18, color: "#444444", width: 240, height: 40 },
  concurrentValue: {
    fontSize: 18,
    color: "#000000",
    width: 240,
    height: 40,
  },
  concurrentValueStale: {
    fontSize: 18,
    color: "#cc0000",
    width: 240,
    height: 40,
  },
  btnAmber: {
    width: W - 80,
    height: 90,
    backgroundColor: "#aa5500",
    borderRadius: 16,
    justifyContent: "center",
    alignItems: "center",
    marginTop: 16,
  },
  footer: {
    width: W,
    height: 70,
    backgroundColor: "#f8f8f8",
    justifyContent: "center",
    paddingHorizontal: 20,
  },
  footerText: { fontSize: 18, color: "#888888", width: W - 40, height: 70 },
});

// Artificial heavy work — gives concurrent rendering something visible
// to slice. Returns a number derived from `seed` after burning ~80–120ms.
function heavyCompute(seed) {
  let acc = seed;
  for (let i = 0; i < 6_000_000; i++) acc = (acc * 31 + i) | 0;
  return acc;
}

function App() {
  // Real React hooks
  const [count, setCount] = useState(0);
  const [running, setRunning] = useState(false);

  // Concurrent React: useTransition + useDeferredValue
  const [load, setLoad] = useState(0);
  const [isPending, startTransition] = useTransition();
  const deferredCount = useDeferredValue(count);
  const heavyResult = useMemo(
    () => heavyCompute(deferredCount + load),
    [deferredCount, load],
  );
  const isStale = count !== deferredCount;

  // useRef — persists across renders, no re-render on change
  const timerRef = useRef(null);
  const renderCount = useRef(0);
  renderCount.current += 1;

  // useEffect — start/stop timer based on `running`
  useEffect(() => {
    if (running) {
      timerRef.current = setInterval(() => setCount((c) => c + 1), 1000);
    }
    return () => {
      if (timerRef.current) {
        clearInterval(timerRef.current);
        timerRef.current = null;
      }
    };
  }, [running]);

  // useCallback — stable function, not recreated on every render
  const handleReset = useCallback(() => {
    setRunning(false);
    setCount(0);
  }, []);

  // useMemo — only recalculates when count changes
  const parity = useMemo(() => (count % 2 === 0 ? "Even" : "Odd"), [count]);

  return (
    <View style={styles.root}>
      <View style={styles.header}>
        <Text style={styles.headerText}>React Hooks</Text>
      </View>

      <View style={styles.body}>
        <TouchableOpacity
          style={styles.button}
          onPress={() => setCount((c) => c + 1)}
        >
          <Text style={styles.buttonText}>
            Count: {count} ({parity})
          </Text>
        </TouchableOpacity>

        <TouchableOpacity
          style={running ? styles.btnRed : styles.btnGreen}
          onPress={() => setRunning((r) => !r)}
        >
          <Text style={styles.buttonTextSm}>
            {running ? "Stop Timer" : "Start Timer"}
          </Text>
        </TouchableOpacity>

        <TouchableOpacity style={styles.btnGrey} onPress={handleReset}>
          <Text style={styles.buttonTextSm}>Reset</Text>
        </TouchableOpacity>

        <View style={styles.row}>
          <View style={styles.card}>
            <Text style={styles.cardLabel}>useState</Text>
            <Text style={styles.cardValue}>{count}</Text>
          </View>
          <View style={styles.card}>
            <Text style={styles.cardLabel}>useRef</Text>
            <Text style={styles.cardValue}>#{renderCount.current}</Text>
          </View>
          <View style={styles.card}>
            <Text style={styles.cardLabel}>useMemo</Text>
            <Text style={styles.cardValue}>{parity}</Text>
          </View>
        </View>

        <View style={styles.concurrent}>
          <Text style={styles.concurrentTitle}>Concurrent React</Text>

          <View style={styles.concurrentRow}>
            <Text style={styles.concurrentLabel}>count (urgent)</Text>
            <Text style={styles.concurrentValue}>{count}</Text>
          </View>

          <View style={styles.concurrentRow}>
            <Text style={styles.concurrentLabel}>deferred count</Text>
            <Text
              style={
                isStale ? styles.concurrentValueStale : styles.concurrentValue
              }
            >
              {deferredCount}
              {isStale ? "  (catching up...)" : ""}
            </Text>
          </View>

          <View style={styles.concurrentRow}>
            <Text style={styles.concurrentLabel}>heavy result</Text>
            <Text style={styles.concurrentValue}>
              {String(heavyResult).slice(-6)}
              {isPending ? "  (pending...)" : ""}
            </Text>
          </View>

          <TouchableOpacity
            style={styles.btnAmber}
            onPress={() =>
              startTransition(() => setLoad((l) => l + 1))
            }
          >
            <Text style={styles.buttonTextSm}>
              Heavy update via startTransition (load: {load})
            </Text>
          </TouchableOpacity>
        </View>
      </View>

      <View style={styles.footer}>
        <Text style={styles.footerText}>
          Real React hooks — no reimplementation
        </Text>
      </View>
    </View>
  );
}

render(<App />);
