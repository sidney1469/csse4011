import { useState, useEffect, useRef, useCallback } from 'react'
import axios from 'axios'

const NODE_NAMES = [
  '4011-A','4011-B','4011-C','4011-D','4011-E','4011-F','4011-G',
  '4011-H','4011-I','4011-J','4011-K','4011-L','4011-M'
]
const API      = 'http://localhost:8000'
const GRID_W   = 1200
const GRID_H   = 900
const CELL     = 60
const TABS     = ['nodes','position','config','log']

function metersToGrid(x, y) {
  return { gx: GRID_W / 2 + x * CELL, gy: GRID_H / 2 - y * CELL }
}
function gridToMeters(gx, gy) {
  return {
    x: +((gx - GRID_W / 2) / CELL).toFixed(2),
    y: +((GRID_H / 2 - gy) / CELL).toFixed(2),
  }
}
// Strip the "4011-" prefix for the circle label → "A", "B", etc.
const shortLabel = (name) => name.replace(/^4011[-_]/, '')

const EMPTY_FORM = { name:'', mac:'', major:0, minor:0, x:0, y:0, z:0, left:'', right:'' }

export default function App() {
  const [nodes,      setNodes]      = useState({})
  const [liveData,   setLiveData]   = useState({})
  const [livePos,    setLivePos]    = useState(null)
  const [selectedId, setSelectedId] = useState(null)
  const [addMode,    setAddMode]    = useState(false)
  const [showRings,  setShowRings]  = useState(true)
  const [showPos,    setShowPos]    = useState(true)
  const [form,       setForm]       = useState(EMPTY_FORM)
  const [wsStatus,   setWsStatus]   = useState('Disconnected')
  const [pktCount,   setPktCount]   = useState(0)
  const [toast,      setToast]      = useState({ type:'', msg:'' })
  const [sending,    setSending]    = useState(false)
  const [activeTab,  setActiveTab]  = useState('nodes')
  const [config,     setConfig]     = useState({ meas_power:-56, path_loss_exp:2.5 })
  const [configSaving, setConfigSaving] = useState(false)
  const [log,        setLog]        = useState([])

  const canvasRef   = useRef(null)
  const gridWrapRef = useRef(null)
  const wsRef       = useRef(null)
  const logRef      = useRef(null)

  // ── Log ──────────────────────────────────────────────
  const addLog = useCallback((msg, type = 'info') => {
    const ts = new Date().toISOString().slice(11, 23)
    setLog(l => [...l.slice(-299), { ts, msg, type }])
  }, [])

  // ── Grid canvas ──────────────────────────────────────
  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    canvas.width  = GRID_W
    canvas.height = GRID_H
    const ctx = canvas.getContext('2d')
    ctx.clearRect(0, 0, GRID_W, GRID_H)
    ctx.strokeStyle = 'rgba(37,43,53,0.7)'
    ctx.lineWidth = 0.5
    for (let x = 0; x <= GRID_W; x += CELL) {
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, GRID_H); ctx.stroke()
    }
    for (let y = 0; y <= GRID_H; y += CELL) {
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(GRID_W, y); ctx.stroke()
    }
    ctx.strokeStyle = 'rgba(55,65,80,1)'
    ctx.lineWidth = 1
    ctx.beginPath(); ctx.moveTo(GRID_W/2,0); ctx.lineTo(GRID_W/2,GRID_H); ctx.stroke()
    ctx.beginPath(); ctx.moveTo(0,GRID_H/2); ctx.lineTo(GRID_W,GRID_H/2); ctx.stroke()
  }, [])

  // ── Fit view ─────────────────────────────────────────
  useEffect(() => {
    const w = gridWrapRef.current
    if (!w) return
    w.scrollLeft = (GRID_W - w.clientWidth)  / 2
    w.scrollTop  = (GRID_H - w.clientHeight) / 2
  }, [])

  // ── Load config ──────────────────────────────────────
  useEffect(() => {
    axios.get(`${API}/config`).then(r => {
      setConfig({ meas_power: r.data.meas_power, path_loss_exp: r.data.path_loss_exp })
    }).catch(() => {})
  }, [])

  // ── Auto-scroll log ──────────────────────────────────
  useEffect(() => {
    if (logRef.current) logRef.current.scrollTop = logRef.current.scrollHeight
  }, [log])

  // ── WebSocket ────────────────────────────────────────
  useEffect(() => {
    const connect = () => {
      const ws = new WebSocket('ws://localhost:8000/ws')
      wsRef.current = ws

      ws.onopen  = () => { setWsStatus('Connected');    addLog('WebSocket connected', 'ok') }
      ws.onerror = () => { setWsStatus('Error');        addLog('WebSocket error', 'err') }
      ws.onclose = () => {
        setWsStatus('Disconnected')
        addLog('WebSocket disconnected — retrying in 3s', 'warn')
        setTimeout(connect, 3000)
      }

      ws.onmessage = (e) => {
        let pkt
        try { pkt = JSON.parse(e.data) } catch { return }

        // ── Beacon registry from device ──────────────
        if (pkt.type === 'beacon_list') {
          addLog(`Received ${pkt.beacons?.length ?? 0} beacons from device`, 'ok')
          setNodes(prev => {
            const next = { ...prev }
            ;(pkt.beacons || []).forEach(b => {
              const key = b.name   // e.g. "4011-A"
              next[key] = {
                ...(prev[key] || {}),
                name:  key,
                mac:   b.mac   || '',
                major: b.major ?? 0,
                minor: b.minor ?? 0,
                x:     b.x     ?? 0,
                y:     b.y     ?? 0,
                z:     b.z     ?? 0,
                left:  b.left  || '',
                right: b.right || '',
                placed: true,
              }
            })
            return next
          })
          return
        }

        // ── RSSI / position packet ───────────────────
        if (pkt.type === 'rssi') {
          setPktCount(c => c + 1)
          if (pkt.position) setLivePos(pkt.position)

          setLiveData(prev => {
            const next = { ...prev }
            ;(pkt.nodes || []).forEach(n => {
              // Log first packet so we can confirm names match
              next[n.name] = n   // key by exact name from backend e.g. "4011-A"
            })
            return next
          })

          // Debug: log names on first few packets
          if (pktCount < 3) {
            const names = (pkt.nodes || []).map(n => n.name).join(', ')
            addLog(`Packet names: ${names}`, 'info')
          }
        }
      }
    }
    const t = setTimeout(connect, 200)
    return () => { clearTimeout(t); wsRef.current?.close() }
  }, [addLog])   // pktCount intentionally excluded to avoid reconnecting

  // ── Populate form when selection changes ─────────────
  useEffect(() => {
    if (!selectedId || !nodes[selectedId]) return
    const n = nodes[selectedId]
    setForm({
      name:  n.name,
      mac:   n.mac   || '',
      major: n.major ?? 0,
      minor: n.minor ?? 0,
      x:     n.x     ?? 0,
      y:     n.y     ?? 0,
      z:     n.z     ?? 0,
      left:  n.left  || '',
      right: n.right || '',
    })
    setToast({ type:'', msg:'' })
  }, [selectedId, nodes])

  // ── Grid click → place node ──────────────────────────
  const handleGridClick = useCallback((e) => {
    if (!addMode) return
    const wrap = gridWrapRef.current
    const rect = wrap.getBoundingClientRect()
    const gx = e.clientX - rect.left + wrap.scrollLeft
    const gy = e.clientY - rect.top  + wrap.scrollTop
    const { x, y } = gridToMeters(gx, gy)
    const used = new Set(Object.keys(nodes))
    const free = NODE_NAMES.find(n => !used.has(n))
    if (!free) { addLog('All 13 nodes already placed', 'warn'); return }
    setNodes(prev => ({
      ...prev,
      [free]: { name:free, mac:'', major:0, minor:0, x, y, z:0, left:'', right:'', placed:true },
    }))
    addLog(`Placed ${free} at (${x}, ${y})`)
    setAddMode(false)
    setSelectedId(free)
    setActiveTab('nodes')
  }, [addMode, nodes, addLog])

  const handleFormChange = (e) => {
    const { name, value } = e.target
    setForm(f => ({ ...f, [name]: value }))
  }

  const removeNode = async () => {
    if (!selectedId) return
    try {
      await axios.delete(`${API}/remove_node/${selectedId}`)
      addLog(`Removed ${selectedId} from device`, 'ok')
    } catch (err) {
      addLog(`Remove failed: ${err.message}`, 'err')
    }
    setNodes(prev => { const n = { ...prev }; delete n[selectedId]; return n })
    setLiveData(prev => { const n = { ...prev }; delete n[selectedId]; return n })
    setSelectedId(null)
    setForm(EMPTY_FORM)
  }

  const sendNode = async () => {
    if (!form.name) { setToast({ type:'err', msg:'Select a name' }); return }
    const payload = {
      name:  form.name,
      mac:   form.mac,
      major: +form.major,
      minor: +form.minor,
      x:     +form.x,
      y:     +form.y,
      z:     +form.z,
      left:  form.left  || '',
      right: form.right || '',
    }
    setNodes(prev => ({ ...prev, [payload.name]: { ...(prev[payload.name]||{}), ...payload, placed:true } }))
    setSending(true)
    setToast({ type:'', msg:'' })
    try {
      await axios.post(`${API}/add_node`, payload)
      setToast({ type:'ok', msg:`✓ ${payload.name} sent` })
      addLog(`Sent ${payload.name} (${payload.mac}) to device`, 'ok')
    } catch (err) {
      setToast({ type:'err', msg:`✗ ${err.message}` })
      addLog(`Failed to send ${payload.name}: ${err.message}`, 'err')
    } finally {
      setSending(false)
    }
  }

  const viewNode = async (name) => {
    try {
      await axios.get(`${API}/view_node/${name}`)
      addLog(`Queried device for ${name}`, 'info')
    } catch (err) {
      addLog(`View node failed: ${err.message}`, 'err')
    }
  }

  const viewAllNodes = async () => {
    try {
      await axios.get(`${API}/view_nodes`)
      addLog('Queried device for all nodes', 'info')
    } catch (err) {
      addLog(`View all nodes failed: ${err.message}`, 'err')
    }
  }

  const saveConfig = async () => {
    setConfigSaving(true)
    try {
      await axios.post(`${API}/config`, config)
      addLog(`Config saved — Tx: ${config.meas_power} dBm, PLE: ${config.path_loss_exp}`, 'ok')
    } catch (err) {
      addLog(`Config save failed: ${err.message}`, 'err')
    } finally {
      setConfigSaving(false)
    }
  }

  const fitView = () => {
    const w = gridWrapRef.current
    if (!w) return
    w.scrollLeft = (GRID_W - w.clientWidth)  / 2
    w.scrollTop  = (GRID_H - w.clientHeight) / 2
  }

  // ── Derived ──────────────────────────────────────────
  const live        = selectedId ? (liveData[selectedId] || {}) : {}
  const activeCount = Object.values(liveData).filter(n => n.rssi && n.rssi !== 0).length

  return (
    <div style={s.root}>

      {/* ── Topbar ── */}
      <div style={s.topbar}>
        <span style={s.logo}>⬡ BEACON GRID</span>
        <Pill><Dot status={wsStatus}/>{wsStatus}</Pill>
        <Pill>PKT <b style={s.val}>{pktCount}</b></Pill>
        <Pill>NODES <b style={s.val}>{Object.keys(nodes).length}</b></Pill>
        <Pill>ACTIVE <b style={{ ...s.val, color: activeCount > 0 ? '#00e5a0' : '#4a5568' }}>{activeCount}</b></Pill>
        <div style={{ flex:1 }}/>
        <button style={{ ...s.btnSm, ...(addMode ? s.btnSmActive : {}) }}
          onClick={() => setAddMode(v => !v)}>
          {addMode ? '✕ Cancel' : '+ Place'}
        </button>
        <button style={s.btnSm} onClick={fitView}>Fit</button>
        <button style={s.btnSm} onClick={viewAllNodes}>Query All</button>
        <label style={s.checkLabel}>
          <input type="checkbox" checked={showRings} onChange={e => setShowRings(e.target.checked)}/> rings
        </label>
        <label style={s.checkLabel}>
          <input type="checkbox" checked={showPos} onChange={e => setShowPos(e.target.checked)}/> pos
        </label>
      </div>

      <div style={s.workspace}>

        {/* ── Grid ── */}
        <div ref={gridWrapRef}
          style={{ ...s.gridWrap, cursor: addMode ? 'crosshair' : 'default' }}
          onClick={handleGridClick}>
          <div style={{ position:'relative', width:GRID_W, height:GRID_H }}>
            <canvas ref={canvasRef} style={{ position:'absolute', inset:0 }}/>

            {/* RSSI rings */}
            {showRings && Object.values(nodes).map(node => {
              const ld = liveData[node.name]
              if (!ld || ld.distance <= 0) return null
              const { gx, gy } = metersToGrid(node.x, node.y)
              const px = ld.distance * CELL
              return (
                <div key={node.name+'_ring'} style={{
                  position:'absolute', borderRadius:'50%',
                  border:'1px dashed rgba(59,138,255,0.25)',
                  width:px*2, height:px*2,
                  left:gx-px, top:gy-px, pointerEvents:'none',
                }}/>
              )
            })}

            {/* Live position marker */}
            {showPos && livePos && (() => {
              const { gx, gy } = metersToGrid(livePos.x, livePos.y)
              return (
                <div style={{
                  position:'absolute', left:gx, top:gy,
                  transform:'translate(-50%,-50%)', zIndex:20, pointerEvents:'none',
                }}>
                  <div style={{
                    width:16, height:16, borderRadius:'50%',
                    background:'#ff6b35', border:'2px solid rgba(255,107,53,0.4)',
                    boxShadow:'0 0 0 4px rgba(255,107,53,0.15)',
                    animation:'pulse 1.5s ease-in-out infinite',
                  }}/>
                  <div style={{
                    position:'absolute', top:20, left:'50%', transform:'translateX(-50%)',
                    fontFamily:'monospace', fontSize:9, color:'#ff6b35', whiteSpace:'nowrap',
                    background:'#0c0e11', padding:'1px 4px', borderRadius:2,
                  }}>
                    ({livePos.x.toFixed(2)}, {livePos.y.toFixed(2)})
                  </div>
                </div>
              )
            })()}

            {/* Nodes */}
            {Object.values(nodes).map(node => {
              // liveData is keyed by exact name from backend e.g. "4011-A"
              const ld       = liveData[node.name] || {}
              const active   = typeof ld.rssi === 'number' && ld.rssi !== 0
              const dim      = !active
              const selected = node.name === selectedId
              const { gx, gy } = metersToGrid(node.x, node.y)
              return (
                <div key={node.name} onClick={e => {
                  e.stopPropagation()
                  setSelectedId(node.name)
                  setActiveTab('nodes')
                }} style={{
                  position:'absolute', left:gx, top:gy,
                  transform:'translate(-50%,-50%)',
                  display:'flex', flexDirection:'column', alignItems:'center', gap:3,
                  cursor:'pointer', opacity: dim ? 0.35 : 1, transition:'opacity .25s',
                }}>
                  <div style={{
                    width:38, height:38, borderRadius:'50%',
                    border:`2px solid ${selected ? '#3b8aff' : active ? '#00e5a0' : '#252b35'}`,
                    background: selected ? 'rgba(59,138,255,.15)' : active ? 'rgba(0,229,160,.1)' : '#1a1e24',
                    display:'flex', alignItems:'center', justifyContent:'center',
                    fontFamily:'monospace', fontSize:11, fontWeight:700,
                    color: active ? '#00e5a0' : '#c8d4e0',
                    boxShadow: selected ? '0 0 0 2px rgba(59,138,255,.3)'
                              : active  ? '0 0 8px rgba(0,229,160,.2)' : 'none',
                    transition:'all .2s',
                  }}>
                    {shortLabel(node.name)}
                  </div>
                  <span style={{ fontFamily:'monospace', fontSize:8, color:'#4a5568', whiteSpace:'nowrap' }}>
                    {node.name}
                  </span>
                  {active && (
                    <span style={{ fontFamily:'monospace', fontSize:8, color:'#3b8aff' }}>
                      {ld.rssi} dBm · {ld.distance}m
                    </span>
                  )}
                </div>
              )
            })}
          </div>
        </div>

        {/* ── Side panel ── */}
        <div style={s.panel}>
          <div style={s.tabs}>
            {TABS.map(tab => (
              <button key={tab} onClick={() => setActiveTab(tab)}
                style={{ ...s.tab, ...(activeTab === tab ? s.tabActive : {}) }}>
                {tab === 'nodes' ? '◈ Nodes' : tab === 'position' ? '⊕ Pos' : tab === 'config' ? '⚙ Cfg' : '▤ Log'}
              </button>
            ))}
          </div>

          <div style={s.panelBody}>

            {/* ── NODES TAB ── */}
            {activeTab === 'nodes' && (
              !selectedId ? (
                <div style={s.emptyState}>
                  <div style={{ fontSize:28, opacity:.3 }}>◎</div>
                  <div>Click a node on the grid<br/>or use + Place to add one</div>
                </div>
              ) : (
                <>
                  <div style={s.sectionLabel}>{selectedId}</div>

                  {/* Live telemetry inline at top when active */}
                  {(typeof live.rssi === 'number' && live.rssi !== 0) && (
                    <div style={s.liveBanner}>
                      <span style={{ color:'#00e5a0' }}>● LIVE</span>
                      <span style={{ color:'#3b8aff' }}>{live.rssi} dBm</span>
                      <span style={{ color:'#00e5a0' }}>{live.distance} m</span>
                    </div>
                  )}

                  <Field label="Name">
                    <select name="name" value={form.name} onChange={handleFormChange} style={s.input}>
                      <option value="">— select —</option>
                      {NODE_NAMES.map(n => <option key={n}>{n}</option>)}
                    </select>
                  </Field>
                  <Field label="MAC Address">
                    <input name="mac" value={form.mac} onChange={handleFormChange}
                      placeholder="XX:XX:XX:XX:XX:XX" style={s.input}/>
                  </Field>
                  <div style={s.row2}>
                    <Field label="Major">
                      <input name="major" type="number" min={0} max={65535}
                        value={form.major} onChange={handleFormChange} style={s.input}/>
                    </Field>
                    <Field label="Minor">
                      <input name="minor" type="number" min={0} max={65535}
                        value={form.minor} onChange={handleFormChange} style={s.input}/>
                    </Field>
                  </div>
                  <div style={s.row2}>
                    <Field label="X (m)">
                      <input name="x" type="number" step={0.1}
                        value={form.x} onChange={handleFormChange} style={s.input}/>
                    </Field>
                    <Field label="Y (m)">
                      <input name="y" type="number" step={0.1}
                        value={form.y} onChange={handleFormChange} style={s.input}/>
                    </Field>
                  </div>
                  <Field label="Z (m)">
                    <input name="z" type="number" step={0.1}
                      value={form.z} onChange={handleFormChange} style={s.input}/>
                  </Field>
                  <div style={s.row2}>
                    <Field label="Left">
                      <select name="left" value={form.left} onChange={handleFormChange} style={s.input}>
                        <option value="">none</option>
                        {NODE_NAMES.filter(n => n !== form.name).map(n => <option key={n}>{n}</option>)}
                      </select>
                    </Field>
                    <Field label="Right">
                      <select name="right" value={form.right} onChange={handleFormChange} style={s.input}>
                        <option value="">none</option>
                        {NODE_NAMES.filter(n => n !== form.name).map(n => <option key={n}>{n}</option>)}
                      </select>
                    </Field>
                  </div>

                  <div style={{ display:'flex', gap:6, marginTop:4 }}>
                    <button onClick={sendNode} disabled={sending} style={{ ...s.btnPrimary, flex:1 }}>
                      {sending ? 'Sending…' : 'Send to device'}
                    </button>
                    <button onClick={() => viewNode(selectedId)} style={{ ...s.btnSm, fontSize:10 }}>
                      Query
                    </button>
                  </div>
                  <button onClick={removeNode} style={s.btnDanger}>Remove node</button>

                  {toast.msg && (
                    <div style={{ ...s.toast, ...(toast.type==='ok' ? s.toastOk : s.toastErr) }}>
                      {toast.msg}
                    </div>
                  )}
                </>
              )
            )}

            {/* ── POSITION TAB ── */}
            {activeTab === 'position' && (
              <>
                <div style={s.sectionLabel}>Tracked position</div>
                {livePos ? (
                  <>
                    <div style={s.posCard}>
                      <PosVal label="X" value={livePos.x.toFixed(3)} unit="m"/>
                      <PosVal label="Y" value={livePos.y.toFixed(3)} unit="m"/>
                      <PosVal label="Z" value={livePos.z.toFixed(3)} unit="m"/>
                    </div>
                    <hr style={s.divider}/>
                    <div style={s.sectionLabel}>Active nodes</div>
                    {Object.values(liveData)
                      .filter(n => typeof n.rssi === 'number' && n.rssi !== 0)
                      .sort((a, b) => b.rssi - a.rssi)
                      .map(n => (
                        <div key={n.name} style={s.nodeRow}
                          onClick={() => { setSelectedId(n.name); setActiveTab('nodes') }}>
                          <span style={{ fontFamily:'monospace', fontSize:10, color:'#c8d4e0' }}>
                            {n.name}
                          </span>
                          <div style={{ display:'flex', gap:10, alignItems:'center' }}>
                            <RssiBar rssi={n.rssi}/>
                            <span style={{ fontFamily:'monospace', fontSize:10, color:'#3b8aff', minWidth:48, textAlign:'right' }}>
                              {n.rssi} dBm
                            </span>
                            <span style={{ fontFamily:'monospace', fontSize:10, color:'#00e5a0', minWidth:40, textAlign:'right' }}>
                              {n.distance}m
                            </span>
                          </div>
                        </div>
                      ))}
                    {Object.values(liveData).filter(n => n.rssi && n.rssi !== 0).length === 0 && (
                      <div style={{ ...s.emptyState, height:80 }}>No active signals</div>
                    )}
                  </>
                ) : (
                  <div style={s.emptyState}>
                    <div style={{ fontSize:28, opacity:.3 }}>⊕</div>
                    <div>No position data yet.<br/>Waiting for packets…</div>
                  </div>
                )}
              </>
            )}

            {/* ── CONFIG TAB ── */}
            {activeTab === 'config' && (
              <>
                <div style={s.sectionLabel}>Path loss model</div>
                <Field label="Measured power at 1m (dBm)">
                  <input type="number" step={1} value={config.meas_power}
                    onChange={e => setConfig(c => ({ ...c, meas_power: +e.target.value }))}
                    style={s.input}/>
                </Field>
                <Field label="Path loss exponent">
                  <input type="number" step={0.1} min={1} max={6} value={config.path_loss_exp}
                    onChange={e => setConfig(c => ({ ...c, path_loss_exp: +e.target.value }))}
                    style={s.input}/>
                </Field>
                <div style={{ ...s.infoBox, marginBottom:10 }}>
                  <b>Guide:</b> Free space ≈ 2.0 · Light indoor ≈ 2.5 · Heavy obstruction ≈ 3.5–4.0
                </div>
                <button onClick={saveConfig} disabled={configSaving} style={s.btnPrimary}>
                  {configSaving ? 'Saving…' : 'Save config'}
                </button>

                <hr style={s.divider}/>
                <div style={s.sectionLabel}>Connection</div>
                <StatRow label="WS status"   value={wsStatus}               color={wsStatus==='Connected' ? '#00e5a0' : '#ff6b35'}/>
                <StatRow label="Packets rx"  value={pktCount}               color="#c8d4e0"/>
                <StatRow label="Nodes known" value={Object.keys(nodes).length} color="#c8d4e0"/>
                <StatRow label="Active"      value={activeCount}            color={activeCount > 0 ? '#00e5a0' : '#4a5568'}/>

                <hr style={s.divider}/>
                <div style={s.sectionLabel}>Raw liveData keys</div>
                <div style={{ fontFamily:'monospace', fontSize:9, color:'#4a5568', lineHeight:1.8 }}>
                  {Object.keys(liveData).length === 0
                    ? 'No live data yet'
                    : Object.keys(liveData).map(k => (
                        <div key={k} style={{ color: liveData[k].rssi !== 0 ? '#00e5a0' : '#4a5568' }}>
                          {k} → {liveData[k].rssi} dBm
                        </div>
                      ))
                  }
                </div>
              </>
            )}

            {/* ── LOG TAB ── */}
            {activeTab === 'log' && (
              <>
                <div style={{ display:'flex', justifyContent:'space-between', alignItems:'center', marginBottom:8 }}>
                  <div style={s.sectionLabel}>Event log</div>
                  <button onClick={() => setLog([])} style={{ ...s.btnSm, fontSize:9 }}>Clear</button>
                </div>
                <div ref={logRef} style={s.logBox}>
                  {log.length === 0 && (
                    <div style={{ color:'#4a5568', fontFamily:'monospace', fontSize:10, padding:8 }}>
                      No events yet.
                    </div>
                  )}
                  {log.map((entry, i) => (
                    <div key={i} style={{ display:'flex', gap:8, padding:'2px 0', borderBottom:'1px solid #252b35' }}>
                      <span style={{ fontFamily:'monospace', fontSize:9, color:'#4a5568', flexShrink:0 }}>
                        {entry.ts}
                      </span>
                      <span style={{
                        fontFamily:'monospace', fontSize:9,
                        color: entry.type==='ok' ? '#00e5a0' : entry.type==='err' ? '#ff6b35' : entry.type==='warn' ? '#f6c343' : '#8899aa',
                      }}>{entry.msg}</span>
                    </div>
                  ))}
                </div>
              </>
            )}
          </div>

          {/* Legend */}
          <div style={s.legend}>
            {[
              { bg:'rgba(0,229,160,.1)',  border:'#00e5a0', label:'Active' },
              { bg:'rgba(59,138,255,.15)',border:'#3b8aff', label:'Selected' },
              { bg:'rgba(255,107,53,.15)',border:'#ff6b35', label:'Live pos' },
              { bg:'#1a1e24',            border:'#252b35', label:'Inactive', dim:true },
            ].map(({ bg, border, label, dim }) => (
              <div key={label} style={{ display:'flex', alignItems:'center', gap:5, opacity:dim?.4:1 }}>
                <div style={{ width:8, height:8, borderRadius:'50%', background:bg, border:`2px solid ${border}`, flexShrink:0 }}/>
                <span style={{ fontFamily:'monospace', fontSize:9, color:'#4a5568' }}>{label}</span>
              </div>
            ))}
          </div>
        </div>
      </div>

      <style>{`
        @keyframes pulse {
          0%,100% { box-shadow: 0 0 0 4px rgba(255,107,53,0.15); }
          50%      { box-shadow: 0 0 0 8px rgba(255,107,53,0.05); }
        }
      `}</style>
    </div>
  )
}

// ── Sub-components ────────────────────────────────────────
function Pill({ children }) {
  return (
    <div style={{ display:'inline-flex', alignItems:'center', gap:5, background:'#1a1e24',
      border:'1px solid #252b35', borderRadius:3, padding:'2px 8px',
      fontFamily:'monospace', fontSize:10, color:'#4a5568' }}>
      {children}
    </div>
  )
}
function Dot({ status }) {
  const color = status==='Connected' ? '#00e5a0' : status==='Error' ? '#ff6b35' : '#4a5568'
  return <div style={{ width:6, height:6, borderRadius:'50%', background:color, flexShrink:0 }}/>
}
function Field({ label, children }) {
  return (
    <div style={{ display:'flex', flexDirection:'column', gap:3, marginBottom:9 }}>
      <span style={{ fontFamily:'monospace', fontSize:9, letterSpacing:'.1em',
        textTransform:'uppercase', color:'#4a5568' }}>{label}</span>
      {children}
    </div>
  )
}
function StatRow({ label, value, color }) {
  return (
    <div style={{ display:'flex', justifyContent:'space-between', padding:'4px 0', borderBottom:'1px solid #252b35' }}>
      <span style={{ fontFamily:'monospace', fontSize:10, color:'#4a5568' }}>{label}</span>
      <span style={{ fontFamily:'monospace', fontSize:10, color: color||'#c8d4e0' }}>{value}</span>
    </div>
  )
}
function PosVal({ label, value, unit }) {
  return (
    <div style={{ display:'flex', flexDirection:'column', alignItems:'center', gap:2 }}>
      <span style={{ fontFamily:'monospace', fontSize:9, color:'#4a5568', textTransform:'uppercase' }}>{label}</span>
      <span style={{ fontFamily:'monospace', fontSize:20, fontWeight:600, color:'#c8d4e0' }}>{value}</span>
      <span style={{ fontFamily:'monospace', fontSize:9, color:'#4a5568' }}>{unit}</span>
    </div>
  )
}
function RssiBar({ rssi }) {
  const pct   = Math.max(0, Math.min(100, ((rssi + 90) / 50) * 100))
  const color = pct > 60 ? '#00e5a0' : pct > 30 ? '#f6c343' : '#ff6b35'
  return (
    <div style={{ width:40, height:4, background:'#252b35', borderRadius:2, overflow:'hidden' }}>
      <div style={{ width:`${pct}%`, height:'100%', background:color, borderRadius:2, transition:'width .3s' }}/>
    </div>
  )
}

// ── Styles ────────────────────────────────────────────────
const s = {
  root:        { display:'flex', flexDirection:'column', height:'100vh', background:'#0c0e11', color:'#c8d4e0', fontFamily:'sans-serif', fontSize:13, overflow:'hidden' },
  topbar:      { background:'#13161b', borderBottom:'1px solid #252b35', padding:'8px 16px', display:'flex', alignItems:'center', gap:8, flexShrink:0, flexWrap:'wrap' },
  logo:        { fontFamily:'monospace', fontSize:13, fontWeight:600, color:'#00e5a0', letterSpacing:'.08em' },
  val:         { color:'#c8d4e0', marginLeft:4 },
  btnSm:       { background:'#1a1e24', border:'1px solid #252b35', borderRadius:3, color:'#8899aa', fontFamily:'monospace', fontSize:11, padding:'4px 10px', cursor:'pointer' },
  btnSmActive: { background:'#00e5a0', color:'#000', borderColor:'#00e5a0' },
  checkLabel:  { display:'flex', alignItems:'center', gap:4, fontSize:11, color:'#4a5568', cursor:'pointer' },
  workspace:   { display:'flex', flex:1, overflow:'hidden' },
  gridWrap:    { flex:1, overflow:'auto', position:'relative' },
  panel:       { width:290, flexShrink:0, background:'#13161b', borderLeft:'1px solid #252b35', display:'flex', flexDirection:'column', overflow:'hidden' },
  tabs:        { display:'flex', borderBottom:'1px solid #252b35', flexShrink:0 },
  tab:         { flex:1, background:'transparent', border:'none', borderBottom:'2px solid transparent', color:'#4a5568', fontFamily:'monospace', fontSize:9, padding:'8px 4px', cursor:'pointer', letterSpacing:'.05em' },
  tabActive:   { color:'#00e5a0', borderBottomColor:'#00e5a0' },
  panelBody:   { flex:1, overflowY:'auto', padding:14 },
  emptyState:  { display:'flex', flexDirection:'column', alignItems:'center', justifyContent:'center', height:'100%', gap:8, color:'#4a5568', fontFamily:'monospace', fontSize:11, textAlign:'center' },
  input:       { background:'#0c0e11', border:'1px solid #252b35', borderRadius:3, color:'#c8d4e0', fontFamily:'monospace', fontSize:11, padding:'6px 8px', outline:'none', width:'100%', boxSizing:'border-box' },
  row2:        { display:'grid', gridTemplateColumns:'1fr 1fr', gap:8 },
  btnPrimary:  { width:'100%', background:'#00e5a0', color:'#000', border:'none', borderRadius:3, fontFamily:'monospace', fontSize:11, fontWeight:600, padding:8, cursor:'pointer', marginTop:4 },
  btnDanger:   { width:'100%', background:'transparent', color:'#ff6b35', border:'1px solid rgba(255,107,53,.3)', borderRadius:3, fontFamily:'monospace', fontSize:11, padding:6, cursor:'pointer', marginTop:6 },
  toast:       { fontFamily:'monospace', fontSize:10, padding:'6px 8px', borderRadius:3, marginTop:8 },
  toastOk:     { background:'rgba(0,229,160,.08)', border:'1px solid rgba(0,229,160,.25)', color:'#00e5a0' },
  toastErr:    { background:'rgba(255,107,53,.08)', border:'1px solid rgba(255,107,53,.25)', color:'#ff6b35' },
  divider:     { border:'none', borderTop:'1px solid #252b35', margin:'12px 0' },
  legend:      { display:'flex', flexWrap:'wrap', gap:8, padding:'8px 14px', borderTop:'1px solid #252b35' },
  sectionLabel:{ fontFamily:'monospace', fontSize:9, letterSpacing:'.12em', textTransform:'uppercase', color:'#4a5568', marginBottom:8 },
  posCard:     { display:'grid', gridTemplateColumns:'1fr 1fr 1fr', gap:8, background:'#0c0e11', border:'1px solid #252b35', borderRadius:4, padding:12, marginBottom:12, textAlign:'center' },
  nodeRow:     { display:'flex', justifyContent:'space-between', alignItems:'center', padding:'5px 0', borderBottom:'1px solid #252b35', cursor:'pointer' },
  infoBox:     { background:'rgba(59,138,255,.06)', border:'1px solid rgba(59,138,255,.15)', borderRadius:3, padding:'6px 8px', fontFamily:'monospace', fontSize:9, color:'#8899aa', lineHeight:1.6 },
  logBox:      { background:'#0c0e11', border:'1px solid #252b35', borderRadius:3, padding:'6px 8px', height:380, overflowY:'auto', display:'flex', flexDirection:'column', gap:0 },
  liveBanner:  { display:'flex', gap:12, alignItems:'center', background:'rgba(0,229,160,.05)', border:'1px solid rgba(0,229,160,.15)', borderRadius:3, padding:'5px 8px', marginBottom:10, fontFamily:'monospace', fontSize:10 },
}