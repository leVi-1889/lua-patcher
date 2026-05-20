const { WebSocketServer } = require("ws");
const { createClient } = require("@supabase/supabase-js");
require("dotenv").config();

const SUPABASE_URL = process.env.SUPABASE_URL;
const SUPABASE_KEY = process.env.SUPABASE_KEY;

if (!SUPABASE_URL || !SUPABASE_KEY) {
    console.error("Missing SUPABASE credentials in .env!");
    process.exit(1);
}

const supabase = createClient(SUPABASE_URL, SUPABASE_KEY);
const PORT = process.env.PORT || 3000;
const wss = new WebSocketServer({ port: PORT });

console.log(`Raw WebSocket Chat Server running on port ${PORT}`);

// Store active connections: username -> WebSocket
const clients = new Map();

wss.on("connection", (ws) => {
    console.log("New client connected");
    ws.username = null;

    ws.on("message", async (messageStr) => {
        try {
            const data = JSON.parse(messageStr);
            const type = data.type;
            
            if (type === "authenticate") {
                const username = data.username;
                if (!username) return;
                ws.username = username;
                clients.set(username, ws);
                console.log(`User authenticated: ${username}`);
            }
            else if (type === "send_message") {
                const { sender, receiver, message } = data;
                if (!sender || !receiver || !message) return;
                
                console.log(`Message from ${sender} to ${receiver}: ${message}`);
                
                const payload = JSON.stringify({
                    type: "new_message",
                    sender, receiver, message
                });
                
                // 1. Instantly push to the receiver if they are online
                const receiverWs = clients.get(receiver);
                if (receiverWs && receiverWs.readyState === 1) { // 1 = OPEN
                    receiverWs.send(payload);
                }
                
                // 2. Push confirmation back to sender
                const senderWs = clients.get(sender);
                if (senderWs && senderWs.readyState === 1) {
                    senderWs.send(JSON.stringify({
                        type: "message_sent",
                        sender, receiver, message
                    }));
                }
                
                // 3. Persist the message in the Supabase database
                const { error } = await supabase.from('chat_messages').insert({
                    sender: sender,
                    receiver: receiver,
                    message: message
                });
                
                if (error) {
                    console.error("Error saving message to database:", error);
                }
            }
            else if (type === "fetch_history") {
                const { user1, user2 } = data;
                if (!user1 || !user2) return;
                
                const { data: messages, error } = await supabase
                    .from('chat_messages')
                    .select('*')
                    .or(`and(sender.eq.${user1},receiver.eq.${user2}),and(sender.eq.${user2},receiver.eq.${user1})`)
                    .order('created_at', { ascending: true })
                    .limit(50);
                    
                if (error) {
                    console.error("Error fetching history:", error);
                    return;
                }
                
                ws.send(JSON.stringify({
                    type: "chat_history",
                    user2: user2,
                    messages: messages || []
                }));
            }
        } catch (e) {
            console.error("Invalid message format:", e);
        }
    });

    ws.on("close", () => {
        console.log("Client disconnected");
        if (ws.username) {
            clients.delete(ws.username);
        }
    });
});
