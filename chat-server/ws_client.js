const WebSocket = require('ws');
const readline = require('readline');

const username = process.argv[2];
if (!username) {
    console.error("Missing username argument");
    process.exit(1);
}

// Use the live Render deployment for WebSockets
const ws = new WebSocket('wss://chat-server-d9k2.onrender.com');

ws.on('open', () => {
    // Authenticate immediately
    ws.send(JSON.stringify({ type: 'authenticate', username: username }));
});

ws.on('message', (data) => {
    // Print received messages to stdout so C++ can read them
    // We append a custom delimiter or just let console.log add a newline.
    // The C++ app will read line by line.
    console.log(data.toString());
});

ws.on('close', () => {
    process.exit(0);
});

ws.on('error', (err) => {
    console.error(err);
    process.exit(1);
});

// Read from stdin (from C++) and forward to WebSocket
const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
    terminal: false
});

rl.on('line', (line) => {
    if (ws.readyState === WebSocket.OPEN) {
        ws.send(line);
    }
});
