import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import BoardLane from './components/BoardLane.jsx'
import EventLog from './components/EventLog.jsx'
import { ChipIcon, PlayIcon, ResetIcon } from './components/Icons.jsx'

const buttonProfiles = {
  B1: { action: 'SPEED UP', message: 'MSG_SPEED_UP_REQUEST', rgb: '#54d647' },
  B2: { action: 'SPEED DOWN', message: 'MSG_SPEED_DOWN_REQUEST', rgb: '#f3c84b' },
  B3: { action: 'STOP', message: 'MSG_STOP_REQUEST', rgb: '#ff5f55', buzzer: true },
}

const mpuSamples = [
  { acceleration: ['0.02', '0.01', '1.00'], rotation: ['0.4', '-0.2', '0.1'] },
  { acceleration: ['0.18', '-0.05', '0.98'], rotation: ['8.2', '1.4', '-3.1'] },
  { acceleration: ['0.08', '0.21', '0.96'], rotation: ['2.8', '11.6', '0.7'] },
]

function makeBoards() {
  return [
    { id: '01', sensor: null },
    {
      id: '02',
      sensor: { type: 'dht11', temperature: 25.3, humidity: 61.0 },
    },
    {
      id: '03',
      sensor: { type: 'mpu6050', sampleIndex: 0, ...mpuSamples[0] },
    },
  ].map((board) => ({
    ...board,
    rgb: '#cfd7dd',
    rgbOn: false,
    buzzer: false,
    lastButton: null,
    transmitting: false,
    pressToken: 0,
  }))
}

function timestamp() {
  return new Intl.DateTimeFormat('en-GB', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    fractionalSecondDigits: 3,
    hour12: false,
  }).format(new Date())
}

export default function App() {
  const [boards, setBoards] = useState(makeBoards)
  const [events, setEvents] = useState([])
  const [isLive, setIsLive] = useState(true)
  const [scenarioRunning, setScenarioRunning] = useState(false)
  const timers = useRef(new Set())
  const audioContext = useRef(null)
  const eventCounter = useRef(0)
  const pressCounter = useRef(0)

  const schedule = useCallback((callback, delay) => {
    const timer = window.setTimeout(() => {
      timers.current.delete(timer)
      callback()
    }, delay)
    timers.current.add(timer)
    return timer
  }, [])

  useEffect(() => () => {
    timers.current.forEach(window.clearTimeout)
    audioContext.current?.close()
  }, [])

  const addEvent = useCallback((type, title, detail) => {
    eventCounter.current += 1
    setEvents((current) => [{
      id: `${Date.now()}-${eventCounter.current}`,
      type,
      title,
      detail,
      time: timestamp(),
    }, ...current].slice(0, 80))
  }, [])

  const playTone = useCallback((frequency = 620) => {
    const AudioContext = window.AudioContext || window.webkitAudioContext
    if (!AudioContext) return
    audioContext.current ??= new AudioContext()
    const context = audioContext.current
    const oscillator = context.createOscillator()
    const gain = context.createGain()
    oscillator.type = 'square'
    oscillator.frequency.value = frequency
    gain.gain.setValueAtTime(0.035, context.currentTime)
    gain.gain.exponentialRampToValueAtTime(0.0001, context.currentTime + 0.16)
    oscillator.connect(gain)
    gain.connect(context.destination)
    oscillator.start()
    oscillator.stop(context.currentTime + 0.17)
  }, [])

  const updateBoard = useCallback((id, updater) => {
    setBoards((current) => current.map((board) => (
      board.id === id ? updater(board) : board
    )))
  }, [])

  const pressButton = useCallback((id, button) => {
    if (!isLive) return
    const profile = buttonProfiles[button]
    if (!profile) return
    pressCounter.current += 1
    const pressToken = pressCounter.current
    if (profile.buzzer) playTone(760)
    updateBoard(id, (board) => ({
      ...board,
      rgb: profile.rgb,
      rgbOn: true,
      buzzer: Boolean(profile.buzzer),
      lastButton: button,
      transmitting: true,
      pressToken,
    }))
    addEvent('button', `STM32-${id}: ${button} clicked`, `${profile.action} · ${profile.message}`)
    schedule(() => addEvent('radio', `STM32-${id} → ESP32-${id}`, 'Packet sent'), 90)
    schedule(() => addEvent('radio', `ESP32-${id}: Packet received`, 'Relay acknowledged'), 240)
    if (profile.buzzer) {
      schedule(() => addEvent('buzzer', `STM32-${id}: Buzzer ON`, 'B3 / STOP only · Tone 760 Hz'), 340)
    }
    schedule(() => addEvent('rgb', `STM32-${id}: RGB ON`, `${profile.action} · ${profile.rgb.toUpperCase()}`), 410)
    schedule(() => updateBoard(id, (board) => ({
      ...(board.pressToken === pressToken
        ? { ...board, buzzer: false, transmitting: false }
        : board),
    })), 780)
    schedule(() => updateBoard(id, (board) => ({
      ...(board.pressToken === pressToken
        ? { ...board, rgbOn: false }
        : board),
    })), 2000)
  }, [addEvent, isLive, playTone, schedule, updateBoard])

  const updateDht = useCallback((id, field, value) => {
    if (!isLive) return
    updateBoard(id, (current) => ({
      ...current,
      sensor: current.sensor?.type === 'dht11'
        ? { ...current.sensor, [field]: Number(value) }
        : current.sensor,
    }))
  }, [isLive, updateBoard])

  const sampleMpu = useCallback((id) => {
    if (!isLive) return
    updateBoard(id, (current) => {
      if (current.sensor?.type !== 'mpu6050') return current
      const selectedSample = (current.sensor.sampleIndex + 1) % mpuSamples.length
      return {
        ...current,
        sensor: { type: 'mpu6050', sampleIndex: selectedSample, ...mpuSamples[selectedSample] },
      }
    })
    addEvent('sensor', `STM32-${id}: MPU6050 sampled`, 'New motion sample · calibrated')
  }, [addEvent, isLive, updateBoard])

  const resetAll = useCallback(() => {
    timers.current.forEach(window.clearTimeout)
    timers.current.clear()
    setBoards(makeBoards())
    setScenarioRunning(false)
    setIsLive(true)
    setEvents([])
  }, [])

  const runScenario = useCallback(() => {
    if (!isLive || scenarioRunning) return
    setScenarioRunning(true)
    const sequence = [
      ['01', 'B1'],
      ['02', 'B2'],
      ['03', 'B3'],
    ]
    sequence.forEach(([id, button], index) => {
      schedule(() => pressButton(id, button), index * 950)
    })
    schedule(() => setScenarioRunning(false), sequence.length * 950)
  }, [isLive, pressButton, scenarioRunning, schedule])

  const actions = useMemo(() => ({
    pressButton,
    updateDht,
    sampleMpu,
  }), [pressButton, sampleMpu, updateDht])

  return (
    <div className="app-shell">
      <header className="app-header">
        <a className="brand" href="#workbench" aria-label="NOSTOS Mock Lab home">
          <span className="brand__mark"><ChipIcon size={26} /></span>
          <span>NOSTOS Mock Lab</span>
        </a>
        <div className="header-actions">
          <button
            className="primary-action"
            type="button"
            disabled={!isLive || scenarioRunning}
            onClick={runScenario}
          >
            <PlayIcon />
            {scenarioRunning ? 'Running…' : 'Run scenario'}
          </button>
          <button className="secondary-action" type="button" onClick={resetAll}>
            <ResetIcon />
            Reset all
          </button>
          <span className="header-divider" />
          <button
            className={`live-toggle${isLive ? ' is-live' : ''}`}
            type="button"
            aria-pressed={isLive}
            onClick={() => setIsLive((value) => !value)}
          >
            <i />
            {isLive ? 'Live' : 'Paused'}
          </button>
        </div>
      </header>

      <main className="workspace" id="workbench">
        <div className="board-stack">
          {boards.map((board) => (
            <BoardLane
              key={board.id}
              board={board}
              disabled={!isLive}
              actions={actions}
            />
          ))}
        </div>
        <EventLog events={events} onClear={() => setEvents([])} />
      </main>

      <footer className="app-footer">
        <span aria-hidden="true">ⓘ</span>
        B1 = speed up, B2 = speed down, B3 = stop + buzzer. OLED text follows the current display firmware.
      </footer>
    </div>
  )
}
