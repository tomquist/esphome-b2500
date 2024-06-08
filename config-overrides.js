// config-overrides.js
const { override, addWebpackModuleRule } = require('customize-cra');
const webpack = require('webpack');

module.exports = override(
    addWebpackModuleRule({
        test: /\.jinja2$/,
        use: 'raw-loader'
    }),
    (config) => {
        config.resolve.fallback = {
            ...config.resolve.fallback,
            "crypto": require.resolve("crypto-browserify"),
            "buffer": require.resolve("buffer/"),
            "process": require.resolve("process/browser"),
            "vm": false,
        };
        config.plugins.push(
            new webpack.ProvidePlugin({
                Buffer: ['buffer', 'Buffer'],
                process: 'process/browser',
            }),
        );
        return config;
    }
);
