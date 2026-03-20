// Plugin state management
import * as bridge from './bridge.js';

const changeListeners = [];

export const state = {
    plugins: [],
    mcpServers: []
};

export function onChange(cb) { changeListeners.push(cb); }

function notify() { changeListeners.forEach(cb => cb(state)); }

// Listen for plugin/MCP list updates from C++ host
bridge.on('PLUGINS_LIST', data => {
    state.plugins = data || [];
    notify();
});

bridge.on('MCP_SERVERS_LIST', data => {
    state.mcpServers = data || [];
    notify();
});

export function listPlugins() {
    bridge.send('LIST_PLUGINS');
}

export function enablePlugin(pluginId, enabled) {
    bridge.send('TOGGLE_PLUGIN', { pluginId, enabled });
}

export function listMcpServers() {
    bridge.send('LIST_MCP_SERVERS');
}

export function addMcpServer(config) {
    bridge.send('ADD_MCP_SERVER', config);
}

export function removeMcpServer(serverId) {
    bridge.send('REMOVE_MCP_SERVER', { serverId });
}

export function refresh() {
    listPlugins();
    listMcpServers();
}
