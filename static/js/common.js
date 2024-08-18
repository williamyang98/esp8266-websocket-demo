let DEFAULT_HOST_URL = document.location.host;
// If we are serving webpage locally use the ESP8266's ip address
const LOCALHOST_URLS = [ "localhost", "127.0.0.1", "0.0.0.0" ];
for (const url of LOCALHOST_URLS) {
    if (DEFAULT_HOST_URL.startsWith(url)) {
        DEFAULT_HOST_URL = "192.168.1.104:80";
        console.log(`Detected demo is being served locally, overriding with ESP8266 ip address`);
        break;
    }
}
let DEFAULT_WS_URL = `ws://${DEFAULT_HOST_URL}/api/v1/websocket`;

let bind_connect_button = (app) => {
    let button_elem = document.getElementById("ws_button");
    let input_elem = document.getElementById("ws_url_textedit");
    input_elem.value = DEFAULT_WS_URL;

    app.on_connection_change.add((state) => {
        switch (state) {
        case WebSocket.OPEN:
            button_elem.innerText = "Disconnect";
            button_elem.disabled = false;
            break;
        case WebSocket.CLOSED:
            button_elem.innerText = "Connect";
            button_elem.disabled = false;
            break;
        case WebSocket.CONNECTING:
            button_elem.disabled = true;
            break;
        }
    });

    button_elem.addEventListener("click", ev => {
        ev.preventDefault();
        if (app.ws === null) {
            let ws_url = input_elem.value;
            app.open_websocket(ws_url);
        } else {
            app.close_websocket();
        }
    });
}

let bind_pc_controls = (app) => {
    // let button_power_off_elem = document.getElementById("pc_power_off");
    let button_power_on_elem = document.getElementById("pc_power_on");

    const PC_ID = 2;
    // const PC_POWER_OFF_CMD = 1;
    const PC_POWER_ON_CMD = 2;
    const PC_GET_STATUS_CMD = 4;
 
    // let send_power_off = () => app.send_ws_data(new Uint8Array([PC_ID, PC_POWER_OFF_CMD]));
    let send_power_on = () => app.send_ws_data(new Uint8Array([PC_ID, PC_POWER_ON_CMD]));
    let send_get_status = () => app.send_ws_data(new Uint8Array([PC_ID, PC_GET_STATUS_CMD]));

    setInterval(() => send_get_status(), 200);

    button_power_on_elem.addEventListener("click", (ev) => {
        ev.preventDefault();
        send_power_on();
    });

    app.packet_decoder.on_pc_status.add((is_on) => {
        let elem_class = is_on ? "active" : "inactive";
        button_power_on_elem.classList = [elem_class];
    });

    app.packet_decoder.on_pc_cmd_result.add((command, code) => {
        console.log(`PC command=${command} got code=${code}`);
    });

    app.on_connection_change.add((state) => {
        if (state == WebSocket.OPEN) {
            send_get_status();
        }
    });
}

let bind_app = (app) => {
    bind_connect_button(app);
    bind_pc_controls(app);
    app.notify_ws_state(WebSocket.CLOSED);
    app.open_websocket(DEFAULT_WS_URL);
}

export { bind_app }
