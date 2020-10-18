
const BT_NUS_SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e'
const BT_RX_CHARACTERISTIC_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'
const BT_TX_CHARACTERISTIC_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'

const ON_DATA_EVENT_TYPE = 'ondata'

export class TennisSensor {

  constructor(){
    this.rx = null
    this.device = null
    this.data = []
    this.groupedDatasets = {}
    this.listeners = []
    this.connected = false
    this.currentDataset = 'random'
  }

  onDisconnect = () => {
    console.log('On Disconnect')
    this.connected = false
  }

  onData(cb){
    this.listeners.push(cb)
  }

  clearLastReading(){
    delete this.groupedDatasets[this.currentDataset]
    const nData = []
    Object.keys(this.groupedDatasets)
      .sort()
      .forEach( datasetKey => {
        const dataset = this.groupedDatasets[datasetKey]
        nData = nData.concat(dataset)
      } )
    this.data = nData
  }

  clear(){
    this.data = []
    this.groupedDatasets = {}
  }

  async connect(){
    const res = await this.searchDevice()
    this.device = res.device
    this.rx = res.rx

    let currentPos = Number.MAX_SAFE_INTEGER
    this.currentDataset = 'random'
    this.rx.addEventListener('characteristicvaluechanged', evt => {
      /** @type DataView */
      const value = evt.target.value
      console.log('Value arrived', Date.now(), value)
      //const data = td.decode(value)
      //const [x,y,z] = data.split(',')
      /*const x1 = value.getInt8(0, true)
      const y1 = value.getInt8(1, true)
      const z1 = value.getInt8(2, true)
      const x2 = value.getInt32(4, true)
      const y2 = value.getInt32(8, true)
      const z2 = value.getInt32(12, true)
      const x = x1 + x2/1000000
      const y = y1 + y2/1000000
      const z = z1 + z2/1000000*/
      const ax = value.getFloat64(0, true);
      const ay = value.getFloat64(8, true);
      const az = value.getFloat64(16, true);
      const gx = value.getFloat64(24, true);
      const gy = value.getFloat64(32, true);
      const gz = value.getFloat64(40, true);
      const pos = value.getInt32(48, true);
      const out = { ax ,ay ,az, gx, gy, gz, pos }
      if(pos < currentPos){
        currentDataset = String(Date.now())
        this.groupedDatasets[currentDataset] = []
      }
      currentPos = pos
      console.log( out )
      this.data.push(out)
      this.groupedDatasets[currentDataset].push(out)
      this.listeners.forEach( cb => cb(out) )
      //this.dispatchEvent(new Event(ON_DATA_EVENT_TYPE, out))
    })

    this.rx.startNotifications()

    this.connected = true
  }

  async searchDevice(){
    const device = await navigator.bluetooth.requestDevice({ filters: [{ services : [BT_NUS_SERVICE_UUID]}]})
    const server = await device.gatt.connect()
    const service = await server.getPrimaryService(BT_NUS_SERVICE_UUID)

    const rx = await service.getCharacteristic(BT_TX_CHARACTERISTIC_UUID)

    device.ongattserverdisconnected = this.onDisconnect

    return { rx, device }
  }
}