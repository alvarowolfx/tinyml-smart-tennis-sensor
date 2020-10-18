import "babel-polyfill"
import crypto from 'crypto'
import React, { useEffect, useState } from 'react'
import ReactDOM from 'react-dom'
import {Line} from 'react-chartjs-2'
import { TennisSensor } from "./device"

const sensor = new TennisSensor()

const Input = ({ label, value, onChange }) => {
  return (
    <div className="flex items-center">
      <div className="w-1/3">
        <label className="block text-gray-500 font-bold text-right mb-1 mb-0 pr-4" htmlFor="inline-full-name">
          {label}
        </label>
      </div>
      <div className="w-2/3">
        <input className="bg-gray-200 appearance-none border-2 border-gray-200 rounded w-full py-2 px-4 text-gray-700 leading-tight focus:outline-none focus:bg-white focus:border-blue-500"
        value={value}
        onChange={onChange} />
      </div>
    </div>
  )
}

const Button = (props) => {
  const { color } = props
  return (
    <button className={`flex text-white font-bold p-2 m-2 rounded bg-${color}-500 hover:bg-${color}-700`}
      {...props}>{props.children}</button>
  )
}

const PrimaryButton = (props) => {
  return <Button color="blue" {...props}/>
}

const App = () => {
  const [dataPoints, setDataPoints] = useState([])
  const [count, setCount] = useState(1)
  const [alias, setAlias] = useState("")
  const [apiKey, setApiKey] = useState("")
  const [hmacKey, setHmacKey] = useState("")
  useEffect( () => {
    const interval = setInterval( () => {
      setDataPoints(sensor.data)
      setCount( prev => prev + 1)
      //console.log('Data points updated', sensor.data)
    }, 100)

    return () => {
      clearInterval(interval)
    }
  },[dataPoints])


  const onConnect = async () => {
    await sensor.connect()
  }

  const onReset = async () => {
    sensor.clear()
  }

  const onExport = () => {
    const filename = `dataset-${alias}.json`
    const data = sensor.data
    const dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(data))
    const anchorEl = document.createElement('a')
    anchorEl.setAttribute("href",dataStr)
    anchorEl.setAttribute("download", filename)
    document.body.appendChild(anchorEl)
    anchorEl.click()
    anchorEl.remove()
    sensor.clear()
  }

  const onRemoveLast = () => {
    sensor.clearLastReading()
  }

  const onEdgeImpulseExport = async () => {
    const values = sensor.data.map( d => [d.ax, d.ay, d.az, d.gx, d.gy, d.gz])
    const emptySignature = Array(64).fill('0').join('')
    const data = {
        protected: {
            ver: "v1",
            alg: "HS256",
            iat: Math.floor(Date.now() / 1000)
        },
        signature: emptySignature,
        payload: {
            device_name: "nrf52840-dongle-001",
            device_type: "tinyml-forwarder",
            interval_ms: 16,
            sensors: [
                { name: "aX", units: "m/s2" },
                { name: "aY", units: "m/s2" },
                { name: "aZ", units: "m/s2" },
                { name: "gX", units: "m/s2" },
                { name: "gY", units: "m/s2" },
                { name: "gZ", units: "m/s2" }
            ],
            values
        }
    }
    let encoded = JSON.stringify(data)
    const hmac = crypto.createHmac('sha256', hmacKey)
    hmac.update(encoded)
    const signature = hmac.digest().toString('hex')
    data.signature = signature
    encoded = JSON.stringify(data)
    const url = 'https://ingestion.edgeimpulse.com/api/training/data'
    const headers = {
      'x-api-key': apiKey,
      'x-file-name': alias,
      'Content-Type': 'application/json'
    }
    const res = await fetch(url, {
      method : 'POST',
      headers,
      body : encoded,
    })
    console.log("response:", res)
  }

  const colors = ['red','blue','green', 'pink', 'purple', 'gold']
  //console.log('Render', dataPoints)
  return <div className="my-4">
    <h1>TinyML Tennis Capture</h1>
    <div className="flex flex-row py-8">
      <div className="flex-grow-0 flex-column">
        <Button color={sensor.connected ? 'green' : 'blue'}
          disabled={sensor.connected}
          onClick={onConnect}>{sensor.connected ? 'Connected' : 'Connect'}
        </Button>
        {sensor.connected && <PrimaryButton onClick={onReset}>Clear</PrimaryButton>}
        {sensor.connected && <PrimaryButton onClick={onRemoveLast}>Remove last reading</PrimaryButton>}
        {sensor.connected && <Input label="Dataset Label"
          value={alias}
          onChange={e => setAlias(e.target.value)} />}
        <Input label="Edge Impulse API Key"
          value={apiKey}
          onChange={e => setApiKey(e.target.value)} />
        <Input label="Edge Impulse HMAC Key"
          value={hmacKey}
          onChange={e => setHmacKey(e.target.value)} />
        {sensor.connected && <PrimaryButton onClick={onExport}>Export</PrimaryButton>}
        {sensor.connected && <PrimaryButton onClick={onEdgeImpulseExport}>Send to Edge Impulse</PrimaryButton>}
        {sensor.connected && <div className="flex-column m-8">
          <p className="flex">Points : {sensor.data.length}</p>
          <p className="flex">Datasets : {Object.keys(sensor.groupedDatasets).length}</p>
        </div>}
      </div>
      <div className="flex flex-grow">
        <Line data={{
          labels : sensor.data.map( d => String(d.pos) ),
          datasets : ['ax','ay','az', 'gx','gy','gz'].map( (attr,i) => ({
            label: attr,
            fill: false,
            lineTension: 0.1,
            backgroundColor: 'rgba(75,192,192,0.4)',
            borderColor: colors[i],
            borderCapStyle: 'butt',
            borderDash: [],
            borderDashOffset: 0.0,
            borderJoinStyle: 'miter',
            pointBorderColor: colors[i],
            pointBackgroundColor: '#fff',
            pointBorderWidth: 1,
            pointHoverRadius: 5,
            pointHoverBackgroundColor: colors[i],
            pointHoverBorderColor: 'rgba(220,220,220,1)',
            pointHoverBorderWidth: 2,
            pointRadius: 1,
            pointHitRadius: 10,
            data: sensor.data.map( dp => dp[attr])
          }))
        }} />
      </div>
    </div>
  </div>
}


ReactDOM.render(<App/>, document.getElementById('root'))