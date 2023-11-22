let DEFAULT_HOST_URL = document.location.host;
if (DEFAULT_HOST_URL.startsWith("localhost")) {
    DEFAULT_HOST_URL = "192.168.1.114:80";
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

let bind_dht11 = (app) => {
    let refresh_elem = document.getElementById("dht11_refresh"); 
    let temp_elem = document.getElementById("dht11_temperature");
    let humidity_elem = document.getElementById("dht11_humidity");
    let status_elem = document.getElementById("dht11_status");

    const DHT11_ID = 3;
    let refresh_dht11_data = () => {
        app.send_ws_data(new Uint8Array([DHT11_ID]));
    };

    refresh_elem.addEventListener("click", ev => {
        ev.preventDefault();
        refresh_dht11_data();
    });

    app.packet_decoder.on_dht11_reading.add((humidity, temperature) => {
        temp_elem.innerText = `${temperature} 'C`;
        humidity_elem.innerText = `${humidity} %`;
        status_elem.innerText = "Good";
    });

    app.packet_decoder.on_dht11_error.add((error_code) => {
        status_elem.innerText = `Error(${error_code})`;
    });

    app.on_connection_change.add((state) => {
        if (state == WebSocket.OPEN) {
            refresh_dht11_data();
        }
    });

    setInterval(() => {
        refresh_dht11_data();
    }, 1000);
}

let bind_led_controls = (app) => {
    let led_controls_elem = document.getElementById("led_controls");
    let led_get_all_elem = document.getElementById("led_get_all");
    let led_set_all_elem = document.getElementById("led_set_all");

    const LED_ID = 1;
    const LED_SET_CMD = 1;
    const LED_GET_CMD = 2;

    let slider_elems = [];

    let set_led = (index, value) => {
        let data = new Uint8Array([LED_ID, LED_SET_CMD, index, value]);
        app.send_ws_data(data);
    };

    let set_all_leds = () => {
        let data = [LED_ID, LED_SET_CMD];
        let index = 0;
        for (let slider of slider_elems) {
            let value = slider.value;
            data.push(index);
            data.push(Number(value));
            index++;
        }
        let buffer = new Uint8Array(data);
        app.send_ws_data(buffer);
    };

    let get_all_leds = () => {
        let data = new Uint8Array([LED_ID, LED_GET_CMD]);
        app.send_ws_data(data);
    };

    let create_led_control = (index, value) => {
        let row = document.createElement("tr");
        let col_0 = document.createElement("td");
        col_0.innerText = `${index}`;

        let col_1 = document.createElement("td");
        let slider = document.createElement("input");
        slider.type = "range";
        slider.min = "0";
        slider.max = "127";
        slider.step = "1";
        slider.value = String(value);

        col_1.appendChild(slider);
        row.appendChild(col_0);
        row.appendChild(col_1);
        led_controls_elem.appendChild(row);
        slider_elems.push(slider);

        slider.addEventListener("input", (ev) => {
            let value = Number(slider.value);
            set_led(index, value); 
        });
    };

    app.packet_decoder.on_led_reading.add((values) => {
        let total_leds = values.length;
        if (total_leds > slider_elems.length) {
            let old_length = slider_elems.length;
            for (let i = old_length; i < total_leds; i++) {
                create_led_control(i, values[i]);
            }
        }
        for (let i = 0; i < total_leds; i++) {
            let slider_elem = slider_elems[i];
            slider_elem.value = values[i];
        }
    });

    led_set_all_elem.addEventListener("click", (ev) => {
        ev.preventDefault();
        set_all_leds();
    });

    led_get_all_elem.addEventListener("click", (ev) => {
        ev.preventDefault();
        get_all_leds();
    });

    app.on_connection_change.add((state) => {
        if (state == WebSocket.OPEN) {
            get_all_leds();
        }
    });
}

let bind_pc_controls = (app) => {
    let button_power_off_elem = document.getElementById("pc_power_off");
    let button_power_on_elem = document.getElementById("pc_power_on");
    let button_reset_elem = document.getElementById("pc_reset");
    let button_get_status_elem = document.getElementById("pc_get_status");
    let text_status_elem = document.getElementById("pc_status");

    const PC_ID = 2;
    const PC_POWER_OFF_CMD = 1;
    const PC_POWER_ON_CMD = 2;
    const PC_RESET_CMD = 3;
    const PC_GET_STATUS_CMD = 4;
    
    let send_power_off = () => app.send_ws_data(new Uint8Array([PC_ID, PC_POWER_OFF_CMD]));
    let send_power_on = () => app.send_ws_data(new Uint8Array([PC_ID, PC_POWER_ON_CMD]));
    let send_reset = () => app.send_ws_data(new Uint8Array([PC_ID, PC_RESET_CMD]));
    let send_get_status = () => app.send_ws_data(new Uint8Array([PC_ID, PC_GET_STATUS_CMD]));

    button_power_off_elem.addEventListener("click", (ev) => {
        ev.preventDefault();
        send_power_off();
    });

    button_power_on_elem.addEventListener("click", (ev) => {
        ev.preventDefault();
        send_power_on();
    });

    button_reset_elem.addEventListener("click", (ev) => {
        ev.preventDefault();
        send_reset();
    });

    button_get_status_elem.addEventListener("click", (ev) => {
        ev.preventDefault();
        send_get_status();
    });

    app.packet_decoder.on_pc_status.add((is_on) => {
        let message = is_on ? "On" : "Off";
        text_status_elem.innerText = `Status: ${message}`;
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
    bind_dht11(app);
    bind_led_controls(app);
    bind_pc_controls(app);
    app.notify_ws_state(WebSocket.CLOSED);
    app.open_websocket(DEFAULT_WS_URL);
}

export { bind_app }
