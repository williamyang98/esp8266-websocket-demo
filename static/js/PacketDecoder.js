class PacketDecoder {
    constructor() {
        this.on_dht11_reading = new Set(); // (humidity, temperature) => {}
        this.on_dht11_error = new Set();  // (error_code) => {}
    }

    on_packet = (packet) => {
        const DHT11_ID = 3;
        if (packet.length < 1) {
            console.error(`Unknown packet: ${packet}`);
            return;
        }

        let id = packet[0];
        let data = packet.slice(1);
        switch (id) {
        case DHT11_ID: 
            this._on_dht11(data);
            break;
        default:
            console.error(`Unknown packet id=${id}, data=${data}`);
            break;
        }
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
