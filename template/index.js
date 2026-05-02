import React from 'react';
import { render, useState, W_SCREEN, H_SCREEN } from './RemarkableRenderer';
import { View, Text, TouchableOpacity, StyleSheet } from './components';

const W = W_SCREEN;
const H = H_SCREEN;

const styles = StyleSheet.create({
  root: {
    width: W, height: H,
    flexDirection: 'column',
    backgroundColor: '#ffffff',
  },
  header: {
    width: W, height: 80,
    backgroundColor: '#000000',
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 24,
  },
  headerText: {
    fontSize: 32, color: '#ffffff', flex: 1,
  },
  body: {
    flex: 1,
    flexDirection: 'column',
    alignItems: 'center',
    justifyContent: 'center',
    padding: 40,
  },
  button: {
    width: W - 80, height: 120,
    backgroundColor: '#000000',
    borderRadius: 16,
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: 40,
  },
  buttonText: {
    fontSize: 40, color: '#ffffff',
    width: W - 80, height: 120,
  },
  row: {
    width: W - 80,
    flexDirection: 'row',
    justifyContent: 'space-between',
  },
  card: {
    flex: 1, height: 140,
    backgroundColor: '#f0f0f0',
    borderRadius: 12,
    margin: 8,
    justifyContent: 'center',
    alignItems: 'center',
  },
  cardLabel: {
    fontSize: 20, color: '#888888',
    width: 160, height: 40,
  },
  cardValue: {
    fontSize: 32, color: '#000000',
    width: 160, height: 50,
  },
  footer: {
    width: W, height: 70,
    backgroundColor: '#f8f8f8',
    justifyContent: 'center',
    paddingHorizontal: 20,
  },
  footerText: {
    fontSize: 20, color: '#888888',
    width: W - 40, height: 70,
  },
});

function App() {
  const [count, setCount] = useState(0);

  return (
    <View style={styles.root}>

      <View style={styles.header}>
        <Text style={styles.headerText}>React Native</Text>
      </View>

      <View style={styles.body}>
        <TouchableOpacity
          style={styles.button}
          onPress={() => setCount(c => c + 1)}>
          <Text style={styles.buttonText}>Tapped: {count}</Text>
        </TouchableOpacity>

        <View style={styles.row}>
          <View style={styles.card}>
            <Text style={styles.cardLabel}>Layout</Text>
            <Text style={styles.cardValue}>Yoga</Text>
          </View>
          <View style={styles.card}>
            <Text style={styles.cardLabel}>Engine</Text>
            <Text style={styles.cardValue}>Hermes</Text>
          </View>
          <View style={styles.card}>
            <Text style={styles.cardLabel}>Count</Text>
            <Text style={styles.cardValue}>{count}</Text>
          </View>
        </View>
      </View>

      <View style={styles.footer}>
        <Text style={styles.footerText}>
          reMarkable Paper Pro  •  Real React Native
        </Text>
      </View>

    </View>
  );
}

render(App);
