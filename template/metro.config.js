const { getDefaultConfig, mergeConfig } = require('@react-native/metro-config');

const config = {
  transformer: {
    hermesParser: true,
  },
  serializer: {
    // No platform-specific extensions needed
  },
};

module.exports = mergeConfig(getDefaultConfig(__dirname), config);
