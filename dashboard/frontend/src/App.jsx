import { useState, useEffect } from 'react'

function App() {
  const [nodes, setNodes] = useState([])
  const [status, setStatus] = useState('Connecting...')
  const [packetCount, setPacketCount] = useState(0)
  const [lastReceived, setLastReceived] = useState(null)

  useEffect(() => {
    const ws = new WebSocket('ws://localhost:8000/ws')

    ws.onopen = () => setStatus('Connected')

    ws.onmessage = (event) => {
      const packet = JSON.parse(event.data)
      setNodes(packet.nodes)
      setPacketCount((c) => c + 1)
      setLastReceived(new Date().toLocaleTimeString())
    }

    ws.onerror = () => setStatus('Error — is the backend running?')
    ws.onclose = () => setStatus('Disconnected')

    return () => ws.close()
  }, [])

  return (
    <div>
      <h1>Dashboard</h1>

      {/* Status bar */}
      <div>
        <span>Status: {status}</span>
        &nbsp;|&nbsp;
        <span>Packets received: {packetCount}</span>
        &nbsp;|&nbsp;
        <span>Last packet: {lastReceived ?? 'none'}</span>
      </div>

      {/* Table */}
      {nodes.length === 0 ? (
        <p>Waiting for data...</p>
      ) : (
        <table>
          <thead>
            <tr>
              <th>Node</th>
              <th>RSSI</th>
              <th>Distance (m)</th>
            </tr>
          </thead>
          <tbody>
            {nodes.map((node) => (
              <tr key={node.name}>
                <td>{node.name}</td>
                <td>{node.rssi}</td>
                <td>{node.distance}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </div>
  )
}

export default App