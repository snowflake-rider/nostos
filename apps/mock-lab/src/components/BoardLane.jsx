import { ChevronIcon, RadioIcon, SpeakerIcon } from './Icons.jsx'

const buttonKeys = ['B1', 'B2', 'B3']
const buttonActions = {
  B1: 'SPEED UP',
  B2: 'SPEED DOWN',
  B3: 'STOP + BUZZER',
}

function HardwareButton({ label, active, disabled, onPress }) {
  return (
    <button
      className={`hardware-button${active ? ' is-pressed' : ''}`}
      type="button"
      aria-label={`Press ${label}`}
      aria-pressed={active}
      disabled={disabled}
      onClick={onPress}
    >
      <span className="hardware-button__label">{label}</span>
      <span className="hardware-button__well">
        <span className="hardware-button__cap" />
      </span>
      <span className="hardware-button__action">{buttonActions[label]}</span>
    </button>
  )
}

function RgbOutput({ color, isOn }) {
  return (
    <div className="output-control" aria-label={`RGB LED ${isOn ? 'on' : 'off'}`}>
      <span className="output-label">RGB</span>
      <span
        className={`rgb-led${isOn ? ' is-on' : ''}`}
        style={{ '--rgb-color': color }}
      >
        <span />
      </span>
    </div>
  )
}

function BuzzerOutput({ active }) {
  return (
    <div className="output-control" aria-label={`Buzzer ${active ? 'on' : 'off'}; B3 only`}>
      <span className="output-label">BUZZER</span>
      <span className={`buzzer${active ? ' is-active' : ''}`}>
        {active ? <SpeakerIcon /> : <span className="buzzer__holes" />}
      </span>
    </div>
  )
}

function firmwareOledLines(board) {
  if (board.sensor?.type === 'dht11') {
    return [
      'NOSTOS SENSOR',
      `TEMP ${board.sensor.temperature.toFixed(1)} C`,
      `HUM  ${board.sensor.humidity.toFixed(1)} %`,
      'DHT OK',
    ]
  }
  return ['NOSTOS NODE', 'DHT NOT FITTED']
}

function OledOutput({ board }) {
  const lines = firmwareOledLines(board)
  return (
    <div className="oled-control" aria-label={`SSD1306 display for STM32-${board.id}`}>
      <span className="output-label">SSD1306</span>
      <span className="oled-screen" aria-live="polite">
        {lines.map((line, index) => (
          <span key={`${index}-${line}`}>{line}</span>
        ))}
      </span>
    </div>
  )
}

function Dht11Panel({ sensor, disabled, onChange }) {
  return (
    <div className="sensor-panel sensor-panel--dht" aria-label="DHT11 sensor controls">
      <div className="sensor-panel__header">
        <div>
          <span className="sensor-icon sensor-icon--dht"><i /><i /></span>
          <strong>DHT11</strong>
        </div>
        <span className="sensor-status">PA1 · 1200 ms</span>
      </div>
      <div className="dht-controls">
        <label>
          <span>Temperature <strong>{sensor.temperature.toFixed(1)} °C</strong></span>
          <input
            aria-label="DHT11 temperature"
            type="range"
            min="0"
            max="50"
            step="0.1"
            value={sensor.temperature}
            disabled={disabled}
            onInput={(event) => onChange('temperature', event.currentTarget.value)}
          />
        </label>
        <label>
          <span>Humidity <strong>{sensor.humidity.toFixed(1)} %</strong></span>
          <input
            aria-label="DHT11 humidity"
            type="range"
            min="20"
            max="90"
            step="0.1"
            value={sensor.humidity}
            disabled={disabled}
            onInput={(event) => onChange('humidity', event.currentTarget.value)}
          />
        </label>
      </div>
    </div>
  )
}

function Mpu6050Panel({ sensor, disabled, onSample }) {
  return (
    <div className="sensor-panel sensor-panel--mpu" aria-label="MPU6050 sensor controls">
      <div className="sensor-panel__header">
        <div>
          <span className="sensor-icon sensor-icon--mpu">6</span>
          <strong>MPU6050</strong>
        </div>
        <span className="sensor-status is-calibrated">CALIBRATED</span>
      </div>
      <div className="mpu-readings">
        <span>ACC g <strong>{sensor.acceleration.join(' / ')}</strong></span>
        <span>GYRO °/s <strong>{sensor.rotation.join(' / ')}</strong></span>
      </div>
      <div className="sensor-panel__footer">
        <span>I2C1 PB8/PB9 · not rendered on OLED</span>
        <button type="button" disabled={disabled} onClick={onSample}>Sample motion</button>
      </div>
    </div>
  )
}

function SensorPanel({ board, disabled, actions }) {
  if (board.sensor?.type === 'dht11') {
    return (
      <Dht11Panel
        sensor={board.sensor}
        disabled={disabled}
        onChange={(field, value) => actions.updateDht(board.id, field, value)}
      />
    )
  }
  if (board.sensor?.type === 'mpu6050') {
    return (
      <Mpu6050Panel
        sensor={board.sensor}
        disabled={disabled}
        onSample={() => actions.sampleMpu(board.id)}
      />
    )
  }
  return null
}

function StmBoard({ board, disabled, actions }) {
  return (
    <section className="device-side device-side--stm" aria-labelledby={`stm-${board.id}`}>
      <h2 id={`stm-${board.id}`}>STM32-{board.id}</h2>
      <div className="stm-board">
        <span className="mount-hole mount-hole--tl" />
        <span className="mount-hole mount-hole--tr" />
        <span className="mount-hole mount-hole--bl" />
        <span className="mount-hole mount-hole--br" />
        <span className="usb-port" />
        <span className="board-trace board-trace--one" />
        <span className="board-trace board-trace--two" />
        <div className="hardware-buttons">
          {buttonKeys.map((button) => (
            <HardwareButton
              key={button}
              label={button}
              active={board.lastButton === button && board.transmitting}
              disabled={disabled}
              onPress={() => actions.pressButton(board.id, button)}
            />
          ))}
        </div>
        <div className="board-outputs">
          <RgbOutput color={board.rgb} isOn={board.rgbOn} />
          <BuzzerOutput active={board.buzzer} />
          <OledOutput board={board} />
        </div>
      </div>
      <SensorPanel board={board} disabled={disabled} actions={actions} />
    </section>
  )
}

function Connection({ board }) {
  return (
    <div className={`connection${board.transmitting ? ' is-active' : ''}`} aria-live="polite">
      <span className="connection__label">Link: STM32-{board.id} ↔ ESP32-{board.id}</span>
      <div className="connection__path" aria-hidden="true">
        <span className="radio-node"><RadioIcon /></span>
        <span className="signal-line"><span /></span>
        <span className="packet-node"><ChevronIcon /></span>
        <span className="signal-line"><span /></span>
        <span className="radio-node"><RadioIcon /></span>
      </div>
      <span className="connection__status">
        <span className="connection__status-dot" />
        {board.transmitting ? 'Transmitting…' : 'Idle'}
      </span>
    </div>
  )
}

function EspBoard({ board }) {
  return (
    <section className="device-side device-side--esp" aria-labelledby={`esp-${board.id}`}>
      <h2 id={`esp-${board.id}`}>ESP32-{board.id}</h2>
      <div className={`esp-board${board.transmitting ? ' is-active' : ''}`}>
        <span className="mount-hole mount-hole--tl" />
        <span className="mount-hole mount-hole--tr" />
        <span className="mount-hole mount-hole--bl" />
        <span className="mount-hole mount-hole--br" />
        <span className="esp-usb" />
        <div className="esp-status">
          <span>STATUS</span>
          <i />
        </div>
        <div className="esp-chip">
          <span className="esp-antenna" />
          <strong>ESP32</strong>
        </div>
        <div className="esp-rssi">
          <span>RSSI</span>
          <i className={board.transmitting ? 'on' : ''} />
          <i className={board.transmitting ? 'on' : ''} />
          <i />
        </div>
        <div className="relay-state">
          <span>RELAY<br />STATE</span>
          <strong>{board.transmitting ? 'ON' : 'OFF'}</strong>
        </div>
        <div className="activity-state">
          <span>ACTIVITY</span>
          <i />
        </div>
      </div>
    </section>
  )
}

export default function BoardLane({ board, disabled, actions }) {
  return (
    <article className={`board-lane${board.transmitting ? ' is-active' : ''}${board.sensor ? ' has-sensor' : ''}`}>
      <StmBoard board={board} disabled={disabled} actions={actions} />
      <Connection board={board} />
      <EspBoard board={board} />
    </article>
  )
}
