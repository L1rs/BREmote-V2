#ifndef PAGE_TERM_H
#define PAGE_TERM_H

/*
** Web terminal, forked from Luddi96's sertest.html
** (https://lbre.de/BREmote/sertest.html). Markup and styling are unchanged,
** only the transport was swapped from the Web Serial API to fetch(), see the
** comment in the script block. Served straight from flash by Server.ino, so
** no SPIFFS upload tooling is needed.
*/

const char PAGE_TERM[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>BREmote Terminal</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            max-width: 1200px;
            margin: 0 auto;
        }
        
        .serial-terminal {
            border: 1px solid #ccc;
            border-radius: 4px;
            padding: 10px;
            margin-bottom: 20px;
            background-color: #f9f9f9;
        }
        
        .terminal-header {
            display: flex;
            align-items: center;
            margin-bottom: 10px;
        }
        
        .terminal-header button {
            margin-left: 10px;
            padding: 5px 10px;
            background-color: #4CAF50;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
        }
        
        .terminal-header button:hover {
            background-color: #45a049;
        }
        
        .terminal-header button:disabled {
            background-color: #cccccc;
            cursor: not-allowed;
        }
        
        .input-area {
            display: flex;
            margin-bottom: 10px;
        }
        
        .input-area input {
            flex: 1;
            padding: 8px;
            border: 1px solid #ccc;
            border-radius: 4px;
        }
        
        .input-area button {
            margin-left: 10px;
            padding: 8px 15px;
            background-color: #2196F3;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
        }
        
        .input-area button:hover {
            background-color: #0b7dda;
        }
        
        .input-area button:disabled {
            background-color: #cccccc;
            cursor: not-allowed;
        }
        
        .command-buttons {
            display: flex;
            gap: 10px;
            margin-bottom: 10px;
        }
        
        .command-buttons button {
            padding: 8px 15px;
            background-color: #FF9800;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-family: monospace;
        }
        
        .command-buttons button:hover {
            background-color: #F57C00;
        }
        
        .command-buttons button:disabled {
            background-color: #cccccc;
            cursor: not-allowed;
        }
        
        .output-area {
            height: 300px;
            overflow-y: auto;
            border: 1px solid #ccc;
            border-radius: 4px;
            padding: 10px;
            background-color: #000;
            color: #0f0;
            font-family: monospace;
            white-space: pre-wrap;
            word-wrap: break-word;
        }
        
        .connection-status {
            margin-left: 10px;
            font-size: 14px;
        }
        
        .status-connected {
            color: green;
        }
        
        .status-disconnected {
            color: red;
        }
    </style>
</head>
<body>
    <div class="serial-terminal">
        <div class="terminal-header">
            <button id="connectButton">Connect</button>
            <span id="connectionStatus" class="connection-status status-disconnected">Disconnected</span>
        </div>
        <div class="input-area">
            <input type="text" id="inputField" placeholder="Enter data to send..." disabled>
            <button id="sendButton" disabled>Send</button>
        </div>
        <div class="command-buttons">
            <button id="exitChgButton" disabled>?exitChg</button>
            <button id="confButton" disabled>?conf</button>
            <button id="rebootButton" disabled>?reboot</button>
        </div>
        <div class="output-area" id="outputArea"></div>
        <p style="font-size:14px"><a href="/update">Firmware update</a></p>
    </div>

    <script>
        /*
         * Forked from Luddi96's sertest.html (https://lbre.de/BREmote/sertest.html).
         * Markup and styling are unchanged, only the transport was swapped: the
         * Web Serial API cannot address a network device, and the original page is
         * served over HTTPS while the device answers over HTTP. This copy is served
         * by the device itself, so it can talk to it with plain fetch().
         *
         * GET  /out?since=N  new output bytes, next cursor in the X-Next header
         * POST /cmd          one command line, same parser as the USB console
         */
        let connected = false;
        let cursor = 0;
        // Bumped on every connect and disconnect. A readLoop that is still
        // waiting on a fetch when the user reconnects would otherwise keep
        // polling next to the new one.
        let generation = 0;

        const connectButton = document.getElementById('connectButton');
        const sendButton = document.getElementById('sendButton');
        const inputField = document.getElementById('inputField');
        const outputArea = document.getElementById('outputArea');
        const connectionStatus = document.getElementById('connectionStatus');
        const exitChgButton = document.getElementById('exitChgButton');
        const confButton = document.getElementById('confButton');
        const rebootButton = document.getElementById('rebootButton');

        async function connect() {
            try {
                // No "since" returns whatever is still in the device buffer, so the
                // boot log is there right after opening the page
                const response = await fetch('/out');
                if (!response.ok) throw new Error("HTTP " + response.status);

                cursor = parseInt(response.headers.get('X-Next'), 10) || 0;
                // The device hands over its whole buffer here, so replace
                // instead of append, otherwise reconnecting shows the history
                // a second time
                outputArea.textContent = await response.text();
                outputArea.scrollTop = outputArea.scrollHeight;

                connected = true;
                const myGeneration = ++generation;

                // Update UI
                connectButton.textContent = "Disconnect";
                connectionStatus.textContent = "Connected";
                connectionStatus.classList.remove("status-disconnected");
                connectionStatus.classList.add("status-connected");
                inputField.disabled = false;
                sendButton.disabled = false;
                exitChgButton.disabled = false;
                confButton.disabled = false;
                rebootButton.disabled = false;

                readLoop(myGeneration);
            } catch (error) {
                console.error("Error connecting to device:", error);
                outputArea.textContent += `\nConnection error: ${error.message}`;
                disconnect();
            }
        }

        function disconnect() {
            connected = false;
            generation++;

            // Update UI
            connectButton.textContent = "Connect";
            connectionStatus.textContent = "Disconnected";
            connectionStatus.classList.remove("status-connected");
            connectionStatus.classList.add("status-disconnected");
            inputField.disabled = true;
            sendButton.disabled = true;
            exitChgButton.disabled = true;
            confButton.disabled = true;
            rebootButton.disabled = true;
        }

        async function readLoop(myGeneration) {
            while (connected && myGeneration === generation) {
                try {
                    const response = await fetch('/out?since=' + cursor);
                    if (!response.ok) throw new Error("HTTP " + response.status);

                    cursor = parseInt(response.headers.get('X-Next'), 10) || cursor;
                    const text = await response.text();

                    if (text) {
                        outputArea.textContent += text;
                        outputArea.scrollTop = outputArea.scrollHeight; // Auto-scroll to bottom
                    }
                } catch (error) {
                    // A loop that has already been replaced stays quiet
                    if (myGeneration !== generation) return;

                    console.error("Error reading data:", error);
                    outputArea.textContent += `\nRead error: ${error.message}`;
                    disconnect();
                    return;
                }
                await new Promise(done => setTimeout(done, 200));
            }
        }

        async function sendData(text = null) {
            if (!connected) {
                return;
            }

            const dataToSend = text || inputField.value;
            if (dataToSend) {
                try {
                    const response = await fetch('/cmd', { method: 'POST', body: dataToSend + "\n" });
                    if (!response.ok) throw new Error("HTTP " + response.status);

                    // Clear input field after sending (only if sending from input field)
                    if (!text) {
                        inputField.value = "";
                    }
                } catch (error) {
                    console.error("Error sending data:", error);
                    outputArea.textContent += `\nSend error: ${error.message}`;
                }
            }
        }

        // Event listeners
        connectButton.addEventListener('click', () => {
            if (connected) {
                disconnect();
            } else {
                connect();
            }
        });

        sendButton.addEventListener('click', () => sendData());

        exitChgButton.addEventListener('click', () => sendData('?exitChg'));
        confButton.addEventListener('click', () => sendData('?conf'));
        rebootButton.addEventListener('click', () => sendData('?reboot'));

        inputField.addEventListener('keydown', event => {
            if (event.key === 'Enter') {
                sendData();
            }
        });

        connect();
    </script>
</body>
</html>
)rawliteral";

#endif
