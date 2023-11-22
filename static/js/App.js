import { PacketDecoder } from "./PacketDecoder.js";

class App {
    constructor() {
        this.ws = null;
        this.ws_url = null;
        this.ws_heartbeat_id = null;
        this.ws_is_updating = false;
        this.on_connection_change = new Set();

        this.packet_decoder = new PacketDecoder();
    }

    send_ws_data = (data) => {
        if (this.ws === null) return;
        if (this.ws.readyState !== WebSocket.OPEN) return;
        this.ws.send(data);
    }

    notify_ws_state = (state) => {
        for (let callback of this.on_connection_change) {
            callback(state);
        }
    }

    // websocket methods
    close_ws_heartbeat = () => {
        if (this.ws_heartbeat_id === null) return;
        clearInterval(this.ws_heartbeat_id);
        this.ws_heartbeat_id = null;
    }

    open_ws_heartbeat = () => {
        this.close_ws_heartbeat();
        this.ws_heartbeat_id = setInterval(() => {
            // this.send_ws_data(this.packet_encoder.acquire_device(this.device_id));
        }, 1000);
    }

    open_websocket = (ws_url) => {
        if (this.ws !== null) return;
        if (this.ws_is_updating) return;
        this.ws_is_updating = true;
        this.notify_ws_state(WebSocket.CONNECTING);
    
        this.ws = new WebSocket(ws_url);
        this.ws.binaryType = "arraybuffer";
        this.ws_url = ws_url;
        this.ws.onopen = () => {
            // this.send_ws_data(this.packet_encoder.acquire_device(this.device_id));
            // this.send_ws_data(this.packet_encoder.reset_device());
            this.open_ws_heartbeat();
            this.notify_ws_state(WebSocket.OPEN);
            this.ws_is_updating = false;
        };

        this.ws.onmessage = (ev) => {
            let packet = ev.data;
            if (packet instanceof ArrayBuffer) {
                this.packet_decoder.on_packet(new Uint8Array(packet));
            } else {
                // TODO:
            }
        };

        this.ws.onclose = () => {
            this.ws = null;
            this.ws_url = null;
            this.close_ws_heartbeat();
            this.notify_ws_state(WebSocket.CLOSED);
            this.ws_is_updating = false;
        };
    }

    close_websocket = () => {
        if (this.ws === null) return;
        if (this.ws_is_updating) return;
        this.ws.close();
    }
}

export { App };
