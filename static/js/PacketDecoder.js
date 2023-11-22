class PacketDecoder {
    constructor() {
        this.on_dht11_reading = new Set(); // (humidity, temperature) => {}
        this.on_dht11_error = new Set();  // (error_code) => {}
        this.on_led_reading = new Set(); // (values: Uint8Array) => {}
        this.on_pc_status = new Set();  // (is_on) => {}
        this.on_pc_cmd_result = new Set(); // (command, code) => {}
    }

    on_packet = (packet) => {
        const LED_ID = 1;
        const PC_ID = 2;
        const DHT11_ID = 3;

        if (packet.length < 1) {
            console.error(`Unknown packet: ${packet}`);
            return;
        }

        let id = packet[0];
        let data = packet.slice(1);
        switch (id) {
        case LED_ID:   this._on_led(data); break;
        case PC_ID:    this._on_pc_controls(data); break;
        case DHT11_ID: this._on_dht11(data); break;
        default:
            console.error(`Unknown packet id=${id}, data=${data}`);
            break;
        }
    }

    _on_pc_controls = (data) => {
        const PC_POWER_OFF_CMD = 1;
        const PC_POWER_ON_CMD = 2;
        const PC_RESET_CMD = 3;
        const PC_GET_STATUS_CMD = 4;

        if (data.length < 2) {
            console.error(`Insufficient PC packet length data=${data}`);
            return;
        }

        let cmd = data[0];
        let value = data[1];

        switch (cmd) {
        case PC_POWER_OFF_CMD:
        case PC_POWER_ON_CMD:
        case PC_RESET_CMD:
            for (let listener of this.on_pc_cmd_result) {
                listener(cmd, value);
            }
            break;
        case PC_GET_STATUS_CMD:
            for (let listener of this.on_pc_status) {
                listener(value === 1);
            }
            break;
        default:
            console.error(`Unknown PC packet cmd=${cmd}, data=${data}`);
            break;
        }
    }

    _on_led = (data) => {
        const LED_SET_ID = 1;
        const LED_GET_ID = 2;
        if (data.length < 1) {
            console.error(`Insufficient LED packet length data=${data}`);
            return;
        }

        let id = data[0]; 
        if (id === LED_SET_ID) {
            return;
        }

        if (id === LED_GET_ID) {
            if (data.length < 2) {
                console.error(`Insufficient LED packet length for LED_GET data=${data}`);
                return;
            }
            let total_pins = data[1];
            let total_packet_length = total_pins + 2;
            if (data.length !== total_packet_length) {
                console.error(`Mismatch between expected (${total_packet_length}) and actual (${data.length}) packet length`);
                return;
            }
            let values = data.slice(2);
            for (let listener of this.on_led_reading) {
                listener(values);
            }
            return;
        }

        console.error(`Unknown LED packet id=${id}, data=${data}`);
    }

    _on_dht11 = (data) => {
        if (data.length === 1) {
            for (let listener of this.on_dht11_error) {
                listener(data[0]);
            }
            return; 
        }
        if (data.length === 2) {
            let humidity = data[0];
            let temperature = data[1];
            for (let listener of this.on_dht11_reading) {
                listener(humidity, temperature);
            }
            return; 
        }
        console.error(`Unknown DHT11 packet, data=${data}`);
    }
}

export { PacketDecoder };
