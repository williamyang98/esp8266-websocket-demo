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
    
    // TODO: Rate limit this on the esp8266 server
    //       Avoid polling DHT11 excessively
    // setInterval(() => {
    //     refresh_dht11_data();
    // }, 1000);
}

let bind_adc = (app) => {
    let refresh_elem = document.getElementById("adc_refresh"); 
    let value_elem = document.getElementById("adc_value");
    let status_elem = document.getElementById("adc_status");

    const ADC_ID = 4;
    let refresh_adc_data = () => {
        app.send_ws_data(new Uint8Array([ADC_ID]));
    };

    refresh_elem.addEventListener("click", ev => {
        ev.preventDefault();
        refresh_adc_data();
    });

    app.packet_decoder.on_adc_reading.add((value) => {
        value_elem.innerText = `${value} mV`;
        status_elem.innerText = "Good";
    });

    app.packet_decoder.on_adc_error.add((error_code) => {
        status_elem.innerText = `Error(${error_code})`;
    });

    app.on_connection_change.add((state) => {
        if (state == WebSocket.OPEN) {
            refresh_adc_data();
        }
    });

    // TODO: Rate limit this on the esp8266 server
    //       Avoid polling ADC excessively
    // setInterval(() => {
    //     refresh_adc_data();
    // }, 1000);
}

let bind_app = (app) => {
    bind_connect_button(app);
    bind_dht11(app);
    bind_adc(app);
    app.notify_ws_state(WebSocket.CLOSED);
    app.open_websocket(DEFAULT_WS_URL);
}

export { bind_app }
