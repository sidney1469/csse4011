import { useState, useEffect, useRef, useCallback } from 'react'
import axios from 'axios'

const NODE_NAMES = ['4011-A','4011-B','4011-C','4011-D','4011-E','4011-F',
                    '4011-G','4011-H','4011-I','4011-J','4011-K','4011-L','4011-M']
const API    = 'http://localhost:8000'
const GRID_W = 1200
const GRID_H = 900
const CELL   = 60
const TABS   = ['nodes','position','sniffer','config','log']
const TRAJ_HISTORY = 300

const PHY_STR = { 0:'None', 1:'LE 1M', 2:'LE 2M', 3:'LE Coded' }
const ADV_TYPE_STR = {
  0:'ADV_IND', 1:'ADV_DIRECT_IND', 2:'ADV_SCAN_IND',
  3:'ADV_NONCONN_IND', 4:'SCAN_RSP', 5:'EXT_ADV'
}

function metersToGrid(x, y) {
  return { gx: GRID_W / 2 + x * CELL, gy: GRID_H / 2 - y * CELL }
}
function gridToMeters(gx, gy) {
  return {
    x: +((gx - GRID_W / 2) / CELL).toFixed(2),
    y: +((GRID_H / 2 - gy) / CELL).toFixed(2),
  }
}

const EMPTY_FORM = { name:'', mac:'', major:0, minor:0, x:0, y:0, z:0, left:'', right:'' }

export default function App() {
  const [nodes,           setNodes]           = useState({})
  const [liveData,        setLiveData]        = useState({})
  const [liveRawPos,      setLiveRawPos]      = useState(null)
  const [liveFiltPos,     setLiveFiltPos]     = useState(null)
  const [selectedId,      setSelectedId]      = useState(null)
  const [addMode,         setAddMode]         = useState(false)
  const [showRings,       setShowRings]       = useState(true)
  const [showPos,         setShowPos]         = useState(true)
  const [form,            setForm]            = useState(EMPTY_FORM)
  const [wsStatus,        setWsStatus]        = useState('Disconnected')
  const [pktCount,        setPktCount]        = useState(0)
  const [toast,           setToast]           = useState({ type:'', msg:'' })
  const [sending,         setSending]         = useState(false)
  const [activeTab,       setActiveTab]       = useState('nodes')
  const [config,          setConfig]          = useState({ meas_power:-56, path_loss_exp:2.5 })
  const [configSaving,    setConfigSaving]    = useState(false)
  const [log,             setLog]             = useState([])
  const [showRaw,         setShowRaw]         = useState(true)
  const [showFilt,        setShowFilt]        = useState(true)
  const [snifferMode,     setSnifferMode]     = useState(false)
  const [snifferNodes,    setSnifferNodes]    = useState({})
  const [selectedSniffer, setSelectedSniffer] = useState(null)

  const rawHistRef  = useRef([])
  const filtHistRef = useRef([])
  const showRawRef  = useRef(true)
  const showFiltRef = useRef(true)
  useEffect(() => { showRawRef.current  = showRaw  }, [showRaw])
  useEffect(() => { showFiltRef.current = showFilt }, [showFilt])

  const canvasRef     = useRef(null)
  const trajCanvasRef = useRef(null)
  const gridWrapRef   = useRef(null)
  const wsRef         = useRef(null)
  const logRef        = useRef(null)

  const addLog = useCallback((msg, type = 'info') => {
    const ts = new Date().toISOString().slice(11, 23)
    setLog(l => [...l.slice(-299), { ts, msg, type }])
  }, [])

  // ── Grid canvas ───────────────────────────────────────────────────────────
  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    canvas.width = GRID_W; canvas.height = GRID_H
    const ctx = canvas.getContext('2d')
    ctx.clearRect(0, 0, GRID_W, GRID_H)
    ctx.strokeStyle = 'rgba(37,43,53,0.7)'; ctx.lineWidth = 0.5
    for (let x = 0; x <= GRID_W; x += CELL) { ctx.beginPath(); ctx.moveTo(x,0); ctx.lineTo(x,GRID_H); ctx.stroke() }
    for (let y = 0; y <= GRID_H; y += CELL) { ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(GRID_W,y); ctx.stroke() }
    ctx.strokeStyle = 'rgba(55,65,80,1)'; ctx.lineWidth = 1
    ctx.beginPath(); ctx.moveTo(GRID_W/2,0);  ctx.lineTo(GRID_W/2,GRID_H); ctx.stroke()
    ctx.beginPath(); ctx.moveTo(0,GRID_H/2);  ctx.lineTo(GRID_W,GRID_H/2); ctx.stroke()
  }, [])

  // ── Trajectory RAF loop ───────────────────────────────────────────────────
  useEffect(() => {
    let raf
    const draw = () => {
      const canvas = trajCanvasRef.current
      if (!canvas) { raf = requestAnimationFrame(draw); return }
      const W = canvas.width, H = canvas.height
      const ctx = canvas.getContext('2d')
      ctx.clearRect(0, 0, W, H)
      const rh = rawHistRef.current
      const fh = filtHistRef.current
      const pts = [...(showRawRef.current ? rh : []), ...(showFiltRef.current ? fh : [])]
      if (pts.length < 2) { raf = requestAnimationFrame(draw); return }
      let minX=pts[0].x,maxX=pts[0].x,minY=pts[0].y,maxY=pts[0].y
      for (const p of pts) { if(p.x<minX)minX=p.x; if(p.x>maxX)maxX=p.x; if(p.y<minY)minY=p.y; if(p.y>maxY)maxY=p.y }
      const pad=1; minX-=pad; maxX+=pad; minY-=pad; maxY+=pad
      const rX=maxX-minX||1, rY=maxY-minY||1
      const tc=(x,y)=>({ cx:((x-minX)/rX)*(W-40)+20, cy:H-(((y-minY)/rY)*(H-40)+20) })
      ctx.strokeStyle='rgba(37,43,53,0.6)'; ctx.lineWidth=0.5
      for(let gx=Math.ceil(minX);gx<=Math.floor(maxX);gx++){const{cx}=tc(gx,minY);ctx.beginPath();ctx.moveTo(cx,0);ctx.lineTo(cx,H);ctx.stroke()}
      for(let gy=Math.ceil(minY);gy<=Math.floor(maxY);gy++){const{cy}=tc(minX,gy);ctx.beginPath();ctx.moveTo(0,cy);ctx.lineTo(W,cy);ctx.stroke()}
      ctx.fillStyle='rgba(74,85,104,0.9)'; ctx.font='9px monospace'
      for(let gx=Math.ceil(minX);gx<=Math.floor(maxX);gx++){const{cx}=tc(gx,minY);ctx.fillText(gx+'m',cx+2,H-4)}
      for(let gy=Math.ceil(minY);gy<=Math.floor(maxY);gy++){const{cy}=tc(minX,gy);ctx.fillText(gy+'m',2,cy-3)}
      const drawPath=(pts,color,alpha,lw)=>{
        if(pts.length<2)return
        ctx.save(); ctx.strokeStyle=color; ctx.globalAlpha=alpha; ctx.lineWidth=lw; ctx.lineJoin='round'; ctx.lineCap='round'
        ctx.beginPath(); const{cx:x0,cy:y0}=tc(pts[0].x,pts[0].y); ctx.moveTo(x0,y0)
        for(let i=1;i<pts.length;i++){const{cx,cy}=tc(pts[i].x,pts[i].y);ctx.lineTo(cx,cy)}
        ctx.stroke()
        const{cx:lx,cy:ly}=tc(pts[pts.length-1].x,pts[pts.length-1].y)
        ctx.globalAlpha=1; ctx.fillStyle=color; ctx.beginPath(); ctx.arc(lx,ly,5,0,Math.PI*2); ctx.fill()
        ctx.restore()
      }
      if(showRawRef.current)  drawPath(rh,'#3b8aff',0.45,1.2)
      if(showFiltRef.current) drawPath(fh,'#00e5a0',0.9, 2.0)
      raf = requestAnimationFrame(draw)
    }
    raf = requestAnimationFrame(draw)
    return () => cancelAnimationFrame(raf)
  }, [])

  useEffect(() => {
    const w = gridWrapRef.current; if(!w) return
    w.scrollLeft = (GRID_W - w.clientWidth)  / 2
    w.scrollTop  = (GRID_H - w.clientHeight) / 2
  }, [])

  useEffect(() => {
    axios.get(`${API}/config`).then(r => setConfig({ meas_power:r.data.meas_power, path_loss_exp:r.data.path_loss_exp })).catch(()=>{})
    axios.get(`${API}/sniffer`).then(r => setSnifferMode(r.data.sniffer)).catch(()=>{})
  }, [])

  useEffect(() => { if(logRef.current) logRef.current.scrollTop = logRef.current.scrollHeight }, [log])

  // ── WebSocket ─────────────────────────────────────────────────────────────
  useEffect(() => {
    const connect = () => {
      const ws = new WebSocket('ws://localhost:8000/ws')
      wsRef.current = ws
      ws.onopen  = () => { setWsStatus('Connected');    addLog('WebSocket connected','ok') }
      ws.onerror = () => { setWsStatus('Error');        addLog('WebSocket error','err') }
      ws.onclose = () => { setWsStatus('Disconnected'); addLog('WebSocket disconnected — retrying','warn'); setTimeout(connect,3000) }

      ws.onmessage = (e) => {
        let pkt; try { pkt = JSON.parse(e.data) } catch { return }

        // ── beacon_list ────────────────────────────────────────────────
        if (pkt.type === 'beacon_list') {
          addLog(`Received ${pkt.beacons?.length ?? 0} beacons`, 'ok')
          setNodes(prev => {
            const next = { ...prev }
            ;(pkt.beacons || []).forEach(b => {
              const key = b.name.replace(/-/g, '_')
              next[key] = { ...(prev[key]||{}), name:key, mac:b.mac||'', major:b.major??0, minor:b.minor??0, x:b.x??0, y:b.y??0, z:b.z??0, left:b.left||'', right:b.right||'', placed:true }
            })
            return next
          })
          return
        }

        // ── sniffer_node ───────────────────────────────────────────────
        if (pkt.type === 'sniffer_node') {
          setSnifferNodes(prev => ({
            ...prev,
            [pkt.addr]: { ...pkt, last_seen: Date.now() }
          }))
          return
        }

        // ── rssi packet ────────────────────────────────────────────────
        if (pkt.type === 'rssi') {
          setPktCount(c => c + 1)
          setLiveData(prev => {
            const next = { ...prev }
            ;(pkt.nodes || []).forEach(n => {
              const key = n.name.replace(/-/g, '_')
              next[key] = { ...n, name: key }
            })
            return next
          })
          if (pkt.raw_position) {
            const rp = pkt.raw_position
            setLiveRawPos(rp)
            rawHistRef.current = [...rawHistRef.current.slice(-(TRAJ_HISTORY-1)), { x:rp.x, y:rp.y }]
          }
          if (pkt.position) {
            const fp = pkt.position
            setLiveFiltPos(fp)
            filtHistRef.current = [...filtHistRef.current.slice(-(TRAJ_HISTORY-1)), { x:fp.x, y:fp.y }]
          }
        }
      }
    }
    const t = setTimeout(connect, 200)
    return () => { clearTimeout(t); wsRef.current?.close() }
  }, [addLog])

  useEffect(() => {
    if (!selectedId || !nodes[selectedId]) return
    const n = nodes[selectedId]
    setForm({ name:n.name, mac:n.mac||'', major:n.major??0, minor:n.minor??0, x:n.x??0, y:n.y??0, z:n.z??0, left:n.left||'', right:n.right||'' })
    setToast({ type:'', msg:'' })
  }, [selectedId, nodes])

  const handleGridClick = useCallback((e) => {
    if (!addMode) return
    const wrap = gridWrapRef.current
    const rect = wrap.getBoundingClientRect()
    const gx = e.clientX - rect.left + wrap.scrollLeft
    const gy = e.clientY - rect.top  + wrap.scrollTop
    const { x, y } = gridToMeters(gx, gy)
    const used = new Set(Object.keys(nodes))
    const free = NODE_NAMES.find(n => !used.has(n))
    if (!free) { addLog('All 13 nodes placed','warn'); return }
    setNodes(prev => ({ ...prev, [free]:{ name:free, mac:'', major:0, minor:0, x, y, z:0, left:'', right:'', placed:true } }))
    addLog(`Placed ${free} at (${x}, ${y})`)
    setAddMode(false); setSelectedId(free); setActiveTab('nodes')
  }, [addMode, nodes, addLog])

  const handleFormChange = (e) => { const{name,value}=e.target; setForm(f=>({...f,[name]:value})) }

  const removeNode = async () => {
    if (!selectedId) return
    try { await axios.delete(`${API}/remove_node/${selectedId}`); addLog(`Removed ${selectedId}`,'ok') }
    catch (err) { addLog(`Remove failed: ${err.message}`,'err') }
    setNodes(prev=>{const n={...prev};delete n[selectedId];return n})
    setLiveData(prev=>{const n={...prev};delete n[selectedId];return n})
    setSelectedId(null); setForm(EMPTY_FORM)
  }

  const sendNode = async () => {
    if (!form.name) { setToast({type:'err',msg:'Select a name'}); return }
    const payload = { name:form.name, mac:form.mac, major:+form.major, minor:+form.minor, x:+form.x, y:+form.y, z:+form.z, left:form.left||'', right:form.right||'' }
    setNodes(prev=>({...prev,[payload.name]:{...(prev[payload.name]||{}),...payload,placed:true}}))
    setSending(true); setToast({type:'',msg:''})
    try { await axios.post(`${API}/add_node`,payload); setToast({type:'ok',msg:`✓ ${payload.name} sent`}); addLog(`Sent ${payload.name}`,'ok') }
    catch(err) { setToast({type:'err',msg:`✗ ${err.message}`}); addLog(`Failed: ${err.message}`,'err') }
    finally { setSending(false) }
  }

  const toggleSniffer = async () => {
    const next = !snifferMode
    try {
      await axios.post(`${API}/sniffer/${next ? 'on' : 'off'}`)
      setSnifferMode(next)
      if (!next) { setSnifferNodes({}); setSelectedSniffer(null) }
      addLog(`Sniffer mode ${next ? 'ON' : 'OFF'}`, 'ok')
    } catch(err) {
      addLog(`Sniffer toggle failed: ${err.message}`, 'err')
    }
  }

  const viewNode     = async (name) => { try{await axios.get(`${API}/view_node/${name}`);addLog(`Queried ${name}`)}catch(err){addLog(err.message,'err')} }
  const viewAllNodes = async ()     => { try{await axios.get(`${API}/view_nodes`);addLog('Queried all nodes')}catch(err){addLog(err.message,'err')} }
  const saveConfig   = async () => {
    setConfigSaving(true)
    try{await axios.post(`${API}/config`,config);addLog('Config saved','ok')}catch(err){addLog(`Config save failed: ${err.message}`,'err')}
    finally{setConfigSaving(false)}
  }
  const fitView = () => { const w=gridWrapRef.current;if(!w)return;w.scrollLeft=(GRID_W-w.clientWidth)/2;w.scrollTop=(GRID_H-w.clientHeight)/2 }
  const clearTrajectory = () => { rawHistRef.current=[]; filtHistRef.current=[] }

  const live        = selectedId ? (liveData[selectedId]||{}) : {}
  const activeCount = Object.values(liveData).filter(n=>n.rssi&&n.rssi!==0).length
  const gridPos     = liveFiltPos || liveRawPos

  // ── Sniffer inspector rows ────────────────────────────────────────────────
  const snifferInspectorRows = (n) => [
    ['Address',    n.addr],
    ['RSSI',       `${n.rssi} dBm`],
    ['TX Power',   n.tx_power != null ? `${n.tx_power} dBm` : '—'],
    ['Adv Type',   `0x${(n.adv_type||0).toString(16).padStart(2,'0')} (${ADV_TYPE_STR[n.adv_type] || '?'})`],
    ['Adv Props',  `0x${(n.adv_props||0).toString(16).padStart(4,'0')}`],
    ['Primary PHY',   PHY_STR[n.primary_phy]   || n.primary_phy],
    ['Secondary PHY', PHY_STR[n.secondary_phy] || n.secondary_phy],
    ['Interval',   n.interval ? `${(n.interval * 1.25).toFixed(2)} ms` : 'none'],
    n.has_name && n.name && ['Name', n.name],
    n.has_flags  && ['Flags', `0x${(n.flags||0).toString(16).padStart(2,'0')}`],
    n.has_manufacturer_data && ['Company ID', `0x${(n.manufacturer_company_id||0).toString(16).padStart(4,'0')}`],
    n.has_manufacturer_data && n.manufacturer_data?.length > 0 &&
      ['Mfr Data', n.manufacturer_data.map(b=>b.toString(16).padStart(2,'0')).join(' ')],
    n.has_service_data && ['Svc Data Type', `0x${(n.service_data_type||0).toString(16).padStart(2,'0')}`],
    n.has_service_data && n.service_data?.length > 0 &&
      ['Svc Data', n.service_data.map(b=>b.toString(16).padStart(2,'0')).join(' ')],
  ].filter(Boolean)

  return (
    <div style={s.root}>
      {/* ── Topbar ── */}
      <div style={s.topbar}>
        <span style={s.logo}>⬡ BEACON GRID</span>
        <Pill><Dot status={wsStatus}/>{wsStatus}</Pill>
        <Pill>PKT <b style={s.val}>{pktCount}</b></Pill>
        <Pill>NODES <b style={s.val}>{Object.keys(nodes).length}</b></Pill>
        <Pill>ACTIVE <b style={{...s.val,color:activeCount>0?'#00e5a0':'#4a5568'}}>{activeCount}</b></Pill>
        {snifferMode && <Pill>SNIFF <b style={{...s.val,color:'#f6c343'}}>{Object.keys(snifferNodes).length}</b></Pill>}
        <div style={{flex:1}}/>
        <button
          style={{...s.btnSm,...(snifferMode?{background:'#f6c343',color:'#000',borderColor:'#f6c343'}:{})}}
          onClick={()=>{ toggleSniffer(); setActiveTab('sniffer') }}
        >
          {snifferMode ? '◉ Sniffer ON' : '◎ Sniffer'}
        </button>
        <button style={{...s.btnSm,...(addMode?s.btnSmActive:{})}} onClick={()=>setAddMode(v=>!v)}>{addMode?'✕ Cancel':'+ Place'}</button>
        <button style={s.btnSm} onClick={fitView}>Fit</button>
        <button style={s.btnSm} onClick={viewAllNodes}>Query All</button>
        <label style={s.checkLabel}><input type="checkbox" checked={showRings} onChange={e=>setShowRings(e.target.checked)}/> rings</label>
        <label style={s.checkLabel}><input type="checkbox" checked={showPos}   onChange={e=>setShowPos(e.target.checked)}/> pos</label>
      </div>

      <div style={s.workspace}>
        {/* ── Grid ── */}
        <div ref={gridWrapRef} style={{...s.gridWrap,cursor:addMode?'crosshair':'default'}} onClick={handleGridClick}>
          <div style={{position:'relative',width:GRID_W,height:GRID_H}}>
            <canvas ref={canvasRef} style={{position:'absolute',inset:0}}/>

            {showRings && Object.values(nodes).map(node=>{
              const ld=liveData[node.name]; if(!ld||ld.distance<=0)return null
              const{gx,gy}=metersToGrid(node.x,node.y); const px=ld.distance*CELL
              return <div key={node.name+'_ring'} style={{position:'absolute',borderRadius:'50%',border:'1px dashed rgba(59,138,255,0.25)',width:px*2,height:px*2,left:gx-px,top:gy-px,pointerEvents:'none'}}/>
            })}

            {showPos && liveRawPos && (()=>{
              const{gx,gy}=metersToGrid(liveRawPos.x,liveRawPos.y)
              return (
                <div style={{position:'absolute',left:gx,top:gy,transform:'translate(-50%,-50%)',zIndex:18,pointerEvents:'none'}}>
                  <div style={{width:10,height:10,borderRadius:'50%',background:'#3b8aff',border:'2px solid rgba(59,138,255,0.4)',opacity:0.7}}/>
                  <div style={{position:'absolute',top:14,left:'50%',transform:'translateX(-50%)',fontFamily:'monospace',fontSize:8,color:'#3b8aff',whiteSpace:'nowrap',background:'#0c0e11',padding:'1px 3px',borderRadius:2}}>
                    RAW ({liveRawPos.x.toFixed(2)}, {liveRawPos.y.toFixed(2)})
                  </div>
                </div>
              )
            })()}

            {showPos && liveFiltPos && (()=>{
              const{gx,gy}=metersToGrid(liveFiltPos.x,liveFiltPos.y)
              return (
                <div style={{position:'absolute',left:gx,top:gy,transform:'translate(-50%,-50%)',zIndex:20,pointerEvents:'none'}}>
                  <div style={{width:16,height:16,borderRadius:'50%',background:'#ff6b35',border:'2px solid rgba(255,107,53,0.4)',boxShadow:'0 0 0 4px rgba(255,107,53,0.15)',animation:'pulse 1.5s ease-in-out infinite'}}/>
                  <div style={{position:'absolute',top:20,left:'50%',transform:'translateX(-50%)',fontFamily:'monospace',fontSize:8,color:'#ff6b35',whiteSpace:'nowrap',background:'#0c0e11',padding:'1px 3px',borderRadius:2}}>
                    KAL ({liveFiltPos.x.toFixed(2)}, {liveFiltPos.y.toFixed(2)})
                  </div>
                </div>
              )
            })()}

            {Object.values(nodes).map(node=>{
              const ld=liveData[node.name]||{}
              const active=ld.rssi&&ld.rssi!==0
              const selected=node.name===selectedId
              const{gx,gy}=metersToGrid(node.x,node.y)
              return (
                <div key={node.name} onClick={e=>{e.stopPropagation();setSelectedId(node.name);setActiveTab('nodes')}}
                  style={{position:'absolute',left:gx,top:gy,transform:'translate(-50%,-50%)',display:'flex',flexDirection:'column',alignItems:'center',gap:3,cursor:'pointer',opacity:active?1:0.2,transition:'opacity .25s'}}>
                  <div style={{width:38,height:38,borderRadius:'50%',border:`2px solid ${selected?'#3b8aff':active?'#00e5a0':'#252b35'}`,background:selected?'rgba(59,138,255,.12)':active?'rgba(0,229,160,.08)':'#1a1e24',display:'flex',alignItems:'center',justifyContent:'center',fontFamily:'monospace',fontSize:9,fontWeight:600,color:active?'#00e5a0':'#c8d4e0',boxShadow:selected?'0 0 0 2px rgba(59,138,255,.3)':'none',transition:'all .2s'}}>
                    {node.name.replace(/4011[-_]/,'')}
                  </div>
                  <span style={{fontFamily:'monospace',fontSize:9,color:'#4a5568',whiteSpace:'nowrap'}}>{node.name}</span>
                  {active&&<span style={{fontFamily:'monospace',fontSize:8,color:'#3b8aff'}}>{ld.rssi} dBm · {ld.distance}m</span>}
                </div>
              )
            })}
          </div>
        </div>

        {/* ── Side panel ── */}
        <div style={s.panel}>
          <div style={s.tabs}>
            {TABS.map(tab=>(
              <button key={tab} onClick={()=>setActiveTab(tab)} style={{...s.tab,...(activeTab===tab?s.tabActive:{})}}>
                {tab==='nodes'?'◈ Nodes':tab==='position'?'⊕ Pos':tab==='sniffer'?'⊙ Sniff':tab==='config'?'⚙ Cfg':'▤ Log'}
              </button>
            ))}
          </div>

          <div style={s.panelBody}>

            {/* ── NODES ── */}
            {activeTab==='nodes' && (
              !selectedId ? (
                <div style={s.emptyState}><div style={{fontSize:28,opacity:.3}}>◎</div><div>Click a node on the grid<br/>or use + Place to add one</div></div>
              ) : (
                <>
                  <div style={s.sectionLabel}>{selectedId}</div>
                  {(live.rssi&&live.rssi!==0)&&(
                    <div style={s.liveBanner}>
                      <span style={{color:'#00e5a0'}}>● LIVE</span>
                      <span style={{color:'#3b8aff'}}>{live.rssi} dBm</span>
                      <span style={{color:'#00e5a0'}}>{live.distance} m</span>
                    </div>
                  )}
                  <Field label="Name">
                    <select name="name" value={form.name} onChange={handleFormChange} style={s.input}>
                      <option value="">— select —</option>
                      {NODE_NAMES.map(n=><option key={n}>{n}</option>)}
                    </select>
                  </Field>
                  <Field label="MAC Address"><input name="mac" value={form.mac} onChange={handleFormChange} placeholder="XX:XX:XX:XX:XX:XX" style={s.input}/></Field>
                  <div style={s.row2}>
                    <Field label="Major"><input name="major" type="number" min={0} max={65535} value={form.major} onChange={handleFormChange} style={s.input}/></Field>
                    <Field label="Minor"><input name="minor" type="number" min={0} max={65535} value={form.minor} onChange={handleFormChange} style={s.input}/></Field>
                  </div>
                  <div style={s.row2}>
                    <Field label="X (m)"><input name="x" type="number" step={0.1} value={form.x} onChange={handleFormChange} style={s.input}/></Field>
                    <Field label="Y (m)"><input name="y" type="number" step={0.1} value={form.y} onChange={handleFormChange} style={s.input}/></Field>
                  </div>
                  <Field label="Z (m)"><input name="z" type="number" step={0.1} value={form.z} onChange={handleFormChange} style={s.input}/></Field>
                  <div style={s.row2}>
                    <Field label="Left">
                      <select name="left" value={form.left} onChange={handleFormChange} style={s.input}>
                        <option value="">none</option>
                        {NODE_NAMES.filter(n=>n!==form.name).map(n=><option key={n}>{n}</option>)}
                      </select>
                    </Field>
                    <Field label="Right">
                      <select name="right" value={form.right} onChange={handleFormChange} style={s.input}>
                        <option value="">none</option>
                        {NODE_NAMES.filter(n=>n!==form.name).map(n=><option key={n}>{n}</option>)}
                      </select>
                    </Field>
                  </div>
                  <div style={{display:'flex',gap:6,marginTop:4}}>
                    <button onClick={sendNode} disabled={sending} style={{...s.btnPrimary,flex:1}}>{sending?'Sending…':'Send to device'}</button>
                    <button onClick={()=>viewNode(selectedId)} style={{...s.btnSm,fontSize:10}}>Query</button>
                  </div>
                  <button onClick={removeNode} style={s.btnDanger}>Remove node</button>
                  {toast.msg&&<div style={{...s.toast,...(toast.type==='ok'?s.toastOk:s.toastErr)}}>{toast.msg}</div>}
                </>
              )
            )}

            {/* ── POSITION ── */}
            {activeTab==='position' && (
              <>
                <div style={{display:'flex',justifyContent:'space-between',alignItems:'center',marginBottom:8}}>
                  <div style={s.sectionLabel}>Trajectory</div>
                  <button onClick={clearTrajectory} style={{...s.btnSm,fontSize:9}}>Clear</button>
                </div>
                <div style={{display:'flex',gap:6,marginBottom:10}}>
                  <button onClick={()=>setShowRaw(v=>!v)} style={{...s.toggleBtn,borderColor:showRaw?'#3b8aff':'#252b35',color:showRaw?'#3b8aff':'#4a5568',background:showRaw?'rgba(59,138,255,0.08)':'transparent'}}>
                    <span style={{width:8,height:8,borderRadius:'50%',background:showRaw?'#3b8aff':'#4a5568',display:'inline-block',marginRight:5}}/>Raw (LS)
                  </button>
                  <button onClick={()=>setShowFilt(v=>!v)} style={{...s.toggleBtn,borderColor:showFilt?'#00e5a0':'#252b35',color:showFilt?'#00e5a0':'#4a5568',background:showFilt?'rgba(0,229,160,0.08)':'transparent'}}>
                    <span style={{width:8,height:8,borderRadius:'50%',background:showFilt?'#00e5a0':'#4a5568',display:'inline-block',marginRight:5}}/>Kalman
                  </button>
                </div>
                <div style={{background:'#0c0e11',border:'1px solid #252b35',borderRadius:4,overflow:'hidden',marginBottom:12}}>
                  <canvas ref={trajCanvasRef} width={262} height={220} style={{display:'block',width:'100%'}}/>
                </div>
                <div style={s.sectionLabel}>Current position</div>
                <div style={{display:'grid',gridTemplateColumns:'1fr 1fr',gap:8,marginBottom:12}}>
                  <div style={{background:'#0c0e11',border:'1px solid rgba(59,138,255,0.3)',borderRadius:4,padding:'8px',textAlign:'center'}}>
                    <div style={{fontFamily:'monospace',fontSize:8,color:'#3b8aff',marginBottom:4}}>RAW (LS)</div>
                    <div style={{fontFamily:'monospace',fontSize:12,color:'#c8d4e0'}}>
                      {liveRawPos ? `${liveRawPos.x.toFixed(2)}, ${liveRawPos.y.toFixed(2)}` : '—'}
                    </div>
                  </div>
                  <div style={{background:'#0c0e11',border:'1px solid rgba(0,229,160,0.3)',borderRadius:4,padding:'8px',textAlign:'center'}}>
                    <div style={{fontFamily:'monospace',fontSize:8,color:'#00e5a0',marginBottom:4}}>KALMAN</div>
                    <div style={{fontFamily:'monospace',fontSize:12,color:'#c8d4e0'}}>
                      {liveFiltPos ? `${liveFiltPos.x.toFixed(2)}, ${liveFiltPos.y.toFixed(2)}` : '—'}
                    </div>
                  </div>
                </div>
                <hr style={s.divider}/>
                <div style={s.sectionLabel}>Active nodes</div>
                {Object.values(liveData).filter(n=>n.rssi&&n.rssi!==0).sort((a,b)=>b.rssi-a.rssi).map(n=>(
                  <div key={n.name} style={s.nodeRow} onClick={()=>{setSelectedId(n.name);setActiveTab('nodes')}}>
                    <span style={{fontFamily:'monospace',fontSize:10,color:'#c8d4e0'}}>{n.name}</span>
                    <div style={{display:'flex',gap:10,alignItems:'center'}}>
                      <RssiBar rssi={n.rssi}/>
                      <span style={{fontFamily:'monospace',fontSize:10,color:'#3b8aff',minWidth:48,textAlign:'right'}}>{n.rssi} dBm</span>
                      <span style={{fontFamily:'monospace',fontSize:10,color:'#00e5a0',minWidth:40,textAlign:'right'}}>{n.distance}m</span>
                    </div>
                  </div>
                ))}
                {Object.values(liveData).filter(n=>n.rssi&&n.rssi!==0).length===0&&(
                  <div style={{...s.emptyState,height:60}}>No active signals</div>
                )}
              </>
            )}

            {/* ── SNIFFER ── */}
            {activeTab==='sniffer' && (
              <>
                {/* Header row */}
                <div style={{display:'flex',justifyContent:'space-between',alignItems:'center',marginBottom:10}}>
                  <div style={s.sectionLabel}>
                    Sniffer&nbsp;
                    <span style={{color: snifferMode ? '#f6c343' : '#4a5568'}}>
                      {snifferMode ? '● ON' : '○ OFF'}
                    </span>
                  </div>
                  <div style={{display:'flex',gap:6}}>
                    <button
                      onClick={toggleSniffer}
                      style={{...s.btnSm,fontSize:9,...(snifferMode?{background:'#f6c343',color:'#000',borderColor:'#f6c343'}:{})}}
                    >
                      {snifferMode ? 'Turn OFF' : 'Turn ON'}
                    </button>
                    <button onClick={()=>{setSnifferNodes({});setSelectedSniffer(null)}} style={{...s.btnSm,fontSize:9}}>Clear</button>
                  </div>
                </div>

                {/* Node list */}
                <div style={{display:'flex',flexDirection:'column',gap:3,marginBottom:12}}>
                  {Object.values(snifferNodes)
                    .sort((a,b) => b.rssi - a.rssi)
                    .map(node => {
                      const label = node.has_name && node.name ? node.name : node.addr
                      const isSelected = selectedSniffer === node.addr
                      const age = Date.now() - (node.last_seen || 0)
                      const fresh = age < 3000
                      return (
                        <div
                          key={node.addr}
                          onClick={() => setSelectedSniffer(isSelected ? null : node.addr)}
                          style={{
                            background: isSelected ? 'rgba(59,138,255,.1)' : '#0c0e11',
                            border: `1px solid ${isSelected ? '#3b8aff' : fresh ? 'rgba(246,195,67,0.3)' : '#252b35'}`,
                            borderRadius: 3, padding:'6px 8px', cursor:'pointer',
                            transition:'border-color .3s',
                          }}
                        >
                          <div style={{display:'flex',justifyContent:'space-between',alignItems:'center'}}>
                            <span style={{fontFamily:'monospace',fontSize:10,color: fresh ? '#c8d4e0' : '#4a5568',maxWidth:160,overflow:'hidden',textOverflow:'ellipsis',whiteSpace:'nowrap'}}>
                              {label}
                            </span>
                            <div style={{display:'flex',gap:8,alignItems:'center',flexShrink:0}}>
                              <RssiBar rssi={node.rssi}/>
                              <span style={{fontFamily:'monospace',fontSize:9,color:'#3b8aff'}}>{node.rssi}</span>
                            </div>
                          </div>
                          {node.has_name && node.name && (
                            <div style={{fontFamily:'monospace',fontSize:8,color:'#4a5568',marginTop:2,overflow:'hidden',textOverflow:'ellipsis',whiteSpace:'nowrap'}}>
                              {node.addr}
                            </div>
                          )}
                        </div>
                      )
                    })
                  }
                  {Object.keys(snifferNodes).length === 0 && (
                    <div style={{...s.emptyState, height:80}}>
                      {snifferMode ? 'Scanning for nodes…' : 'Enable sniffer mode to scan'}
                    </div>
                  )}
                </div>

                {/* Inspector */}
                {selectedSniffer && snifferNodes[selectedSniffer] && (() => {
                  const n = snifferNodes[selectedSniffer]
                  const rows = snifferInspectorRows(n)
                  return (
                    <div style={{background:'#0c0e11',border:'1px solid #252b35',borderRadius:4,padding:10}}>
                      <div style={{...s.sectionLabel,marginBottom:8}}>Inspector</div>
                      {rows.map(([label, value]) => (
                        <div key={label} style={{display:'flex',justifyContent:'space-between',alignItems:'flex-start',
                          padding:'3px 0',borderBottom:'1px solid #1a1e24',gap:8}}>
                          <span style={{fontFamily:'monospace',fontSize:9,color:'#4a5568',flexShrink:0}}>{label}</span>
                          <span style={{fontFamily:'monospace',fontSize:9,color:'#c8d4e0',
                            textAlign:'right',wordBreak:'break-all'}}>{value}</span>
                        </div>
                      ))}
                      {n.raw?.length > 0 && (
                        <div style={{marginTop:8}}>
                          <div style={{fontFamily:'monospace',fontSize:9,color:'#4a5568',marginBottom:4}}>
                            Raw payload ({n.raw.length} B)
                          </div>
                          <div style={{fontFamily:'monospace',fontSize:8,color:'#4a5568',
                            lineHeight:1.8,wordBreak:'break-all'}}>
                            {n.raw.map((b,i)=>(
                              <span key={i} style={{marginRight:4}}>{b.toString(16).padStart(2,'0')}</span>
                            ))}
                          </div>
                        </div>
                      )}
                    </div>
                  )
                })()}
              </>
            )}

            {/* ── CONFIG ── */}
            {activeTab==='config' && (
              <>
                <div style={s.sectionLabel}>Path loss model</div>
                <Field label="Measured power at 1m (dBm)"><input type="number" step={1} value={config.meas_power} onChange={e=>setConfig(c=>({...c,meas_power:+e.target.value}))} style={s.input}/></Field>
                <Field label="Path loss exponent"><input type="number" step={0.1} min={1} max={6} value={config.path_loss_exp} onChange={e=>setConfig(c=>({...c,path_loss_exp:+e.target.value}))} style={s.input}/></Field>
                <div style={{...s.infoBox,marginBottom:10}}><b>Guide:</b> Free space ≈ 2.0 · Light indoor ≈ 2.5 · Heavy obstruction ≈ 3.5–4.0</div>
                <button onClick={saveConfig} disabled={configSaving} style={s.btnPrimary}>{configSaving?'Saving…':'Save config'}</button>
                <hr style={s.divider}/>
                <div style={s.sectionLabel}>Connection</div>
                <StatRow label="WS status"   value={wsStatus}                  color={wsStatus==='Connected'?'#00e5a0':'#ff6b35'}/>
                <StatRow label="Packets rx"  value={pktCount}                  color="#c8d4e0"/>
                <StatRow label="Nodes known" value={Object.keys(nodes).length} color="#c8d4e0"/>
                <StatRow label="Active"      value={activeCount}               color={activeCount>0?'#00e5a0':'#4a5568'}/>
                <StatRow label="Sniffer"     value={snifferMode?'ON':'OFF'}    color={snifferMode?'#f6c343':'#4a5568'}/>
                <StatRow label="Sniff nodes" value={Object.keys(snifferNodes).length} color="#c8d4e0"/>
              </>
            )}

            {/* ── LOG ── */}
            {activeTab==='log' && (
              <>
                <div style={{display:'flex',justifyContent:'space-between',alignItems:'center',marginBottom:8}}>
                  <div style={s.sectionLabel}>Event log</div>
                  <button onClick={()=>setLog([])} style={{...s.btnSm,fontSize:9}}>Clear</button>
                </div>
                <div ref={logRef} style={s.logBox}>
                  {log.length===0&&<div style={{color:'#4a5568',fontFamily:'monospace',fontSize:10,padding:8}}>No events yet.</div>}
                  {log.map((entry,i)=>(
                    <div key={i} style={{display:'flex',gap:8,padding:'2px 0',borderBottom:'1px solid #252b35'}}>
                      <span style={{fontFamily:'monospace',fontSize:9,color:'#4a5568',flexShrink:0}}>{entry.ts}</span>
                      <span style={{fontFamily:'monospace',fontSize:9,color:entry.type==='ok'?'#00e5a0':entry.type==='err'?'#ff6b35':entry.type==='warn'?'#f6c343':'#8899aa'}}>{entry.msg}</span>
                    </div>
                  ))}
                </div>
              </>
            )}
          </div>

          <div style={s.legend}>
            {[{bg:'rgba(0,229,160,.08)',border:'#00e5a0',label:'Active'},{bg:'rgba(59,138,255,.1)',border:'#3b8aff',label:'Selected'},{bg:'rgba(255,107,53,.15)',border:'#ff6b35',label:'Kalman pos'},{bg:'rgba(59,138,255,.15)',border:'#3b8aff',label:'Raw pos'},{bg:'#1a1e24',border:'#252b35',label:'Inactive',dim:true}].map(({bg,border,label,dim})=>(
              <div key={label} style={{display:'flex',alignItems:'center',gap:5,opacity:dim?0.4:1}}>
                <div style={{width:8,height:8,borderRadius:'50%',background:bg,border:`2px solid ${border}`,flexShrink:0}}/>
                <span style={{fontFamily:'monospace',fontSize:9,color:'#4a5568'}}>{label}</span>
              </div>
            ))}
          </div>
        </div>
      </div>
      <style>{`@keyframes pulse{0%,100%{box-shadow:0 0 0 4px rgba(255,107,53,0.15);}50%{box-shadow:0 0 0 8px rgba(255,107,53,0.05);}}`}</style>
    </div>
  )
}

function Pill({children}){return<div style={{display:'inline-flex',alignItems:'center',gap:5,background:'#1a1e24',border:'1px solid #252b35',borderRadius:3,padding:'2px 8px',fontFamily:'monospace',fontSize:10,color:'#4a5568'}}>{children}</div>}
function Dot({status}){const color=status==='Connected'?'#00e5a0':status==='Error'?'#ff6b35':'#4a5568';return<div style={{width:6,height:6,borderRadius:'50%',background:color,flexShrink:0}}/>}
function Field({label,children}){return<div style={{display:'flex',flexDirection:'column',gap:3,marginBottom:9}}><span style={{fontFamily:'monospace',fontSize:9,letterSpacing:'.1em',textTransform:'uppercase',color:'#4a5568'}}>{label}</span>{children}</div>}
function StatRow({label,value,color}){return<div style={{display:'flex',justifyContent:'space-between',padding:'4px 0',borderBottom:'1px solid #252b35'}}><span style={{fontFamily:'monospace',fontSize:10,color:'#4a5568'}}>{label}</span><span style={{fontFamily:'monospace',fontSize:10,color:color||'#c8d4e0'}}>{value}</span></div>}
function RssiBar({rssi}){const pct=Math.max(0,Math.min(100,((rssi+90)/50)*100));const color=pct>60?'#00e5a0':pct>30?'#f6c343':'#ff6b35';return<div style={{width:40,height:4,background:'#252b35',borderRadius:2,overflow:'hidden'}}><div style={{width:`${pct}%`,height:'100%',background:color,borderRadius:2,transition:'width .3s'}}/></div>}

const s={
  root:{display:'flex',flexDirection:'column',height:'100vh',background:'#0c0e11',color:'#c8d4e0',fontFamily:'sans-serif',fontSize:13,overflow:'hidden'},
  topbar:{background:'#13161b',borderBottom:'1px solid #252b35',padding:'8px 16px',display:'flex',alignItems:'center',gap:8,flexShrink:0,flexWrap:'wrap'},
  logo:{fontFamily:'monospace',fontSize:13,fontWeight:600,color:'#00e5a0',letterSpacing:'.08em'},
  val:{color:'#c8d4e0',marginLeft:4},
  btnSm:{background:'#1a1e24',border:'1px solid #252b35',borderRadius:3,color:'#8899aa',fontFamily:'monospace',fontSize:11,padding:'4px 10px',cursor:'pointer'},
  btnSmActive:{background:'#00e5a0',color:'#000',borderColor:'#00e5a0'},
  checkLabel:{display:'flex',alignItems:'center',gap:4,fontSize:11,color:'#4a5568',cursor:'pointer'},
  workspace:{display:'flex',flex:1,overflow:'hidden'},
  gridWrap:{flex:1,overflow:'auto',position:'relative'},
  panel:{width:290,flexShrink:0,background:'#13161b',borderLeft:'1px solid #252b35',display:'flex',flexDirection:'column',overflow:'hidden'},
  tabs:{display:'flex',borderBottom:'1px solid #252b35',flexShrink:0},
  tab:{flex:1,background:'transparent',border:'none',borderBottom:'2px solid transparent',color:'#4a5568',fontFamily:'monospace',fontSize:9,padding:'8px 2px',cursor:'pointer',letterSpacing:'.03em'},
  tabActive:{color:'#00e5a0',borderBottomColor:'#00e5a0'},
  panelBody:{flex:1,overflowY:'auto',padding:14},
  emptyState:{display:'flex',flexDirection:'column',alignItems:'center',justifyContent:'center',height:'100%',gap:8,color:'#4a5568',fontFamily:'monospace',fontSize:11,textAlign:'center'},
  input:{background:'#0c0e11',border:'1px solid #252b35',borderRadius:3,color:'#c8d4e0',fontFamily:'monospace',fontSize:11,padding:'6px 8px',outline:'none',width:'100%',boxSizing:'border-box'},
  row2:{display:'grid',gridTemplateColumns:'1fr 1fr',gap:8},
  btnPrimary:{width:'100%',background:'#00e5a0',color:'#000',border:'none',borderRadius:3,fontFamily:'monospace',fontSize:11,fontWeight:600,padding:8,cursor:'pointer',marginTop:4},
  btnDanger:{width:'100%',background:'transparent',color:'#ff6b35',border:'1px solid rgba(255,107,53,.3)',borderRadius:3,fontFamily:'monospace',fontSize:11,padding:6,cursor:'pointer',marginTop:6},
  toggleBtn:{flex:1,background:'transparent',border:'1px solid #252b35',borderRadius:3,fontFamily:'monospace',fontSize:10,padding:'5px 8px',cursor:'pointer',display:'flex',alignItems:'center',transition:'all .15s'},
  toast:{fontFamily:'monospace',fontSize:10,padding:'6px 8px',borderRadius:3,marginTop:8},
  toastOk:{background:'rgba(0,229,160,.08)',border:'1px solid rgba(0,229,160,.25)',color:'#00e5a0'},
  toastErr:{background:'rgba(255,107,53,.08)',border:'1px solid rgba(255,107,53,.25)',color:'#ff6b35'},
  divider:{border:'none',borderTop:'1px solid #252b35',margin:'12px 0'},
  legend:{display:'flex',flexWrap:'wrap',gap:8,padding:'8px 14px',borderTop:'1px solid #252b35'},
  sectionLabel:{fontFamily:'monospace',fontSize:9,letterSpacing:'.12em',textTransform:'uppercase',color:'#4a5568',marginBottom:8},
  nodeRow:{display:'flex',justifyContent:'space-between',alignItems:'center',padding:'5px 0',borderBottom:'1px solid #252b35',cursor:'pointer'},
  infoBox:{background:'rgba(59,138,255,.06)',border:'1px solid rgba(59,138,255,.15)',borderRadius:3,padding:'6px 8px',fontFamily:'monospace',fontSize:9,color:'#8899aa',lineHeight:1.6},
  logBox:{background:'#0c0e11',border:'1px solid #252b35',borderRadius:3,padding:'6px 8px',height:380,overflowY:'auto',display:'flex',flexDirection:'column',gap:0},
  liveBanner:{display:'flex',gap:12,alignItems:'center',background:'rgba(0,229,160,.05)',border:'1px solid rgba(0,229,160,.15)',borderRadius:3,padding:'5px 8px',marginBottom:10,fontFamily:'monospace',fontSize:10},
}