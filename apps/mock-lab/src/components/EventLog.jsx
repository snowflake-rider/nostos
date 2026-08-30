import { RadioIcon, SpeakerIcon, TrashIcon } from './Icons.jsx'

function LogIcon({ type }) {
  if (type === 'buzzer') return <SpeakerIcon />
  if (type === 'rgb') return <span className="log-rgb" />
  if (type === 'button') return <span className="log-finger">↘</span>
  if (type === 'sensor') return <span className="log-sensor">S</span>
  return <RadioIcon />
}

export default function EventLog({ events, onClear }) {
  return (
    <aside className="event-log" aria-label="Event log">
      <div className="event-log__header">
        <h2>Event log</h2>
        <button type="button" className="clear-button" onClick={onClear}>
          <TrashIcon />
          Clear log
        </button>
      </div>
      <div className="event-log__body" aria-live="polite">
        {events.length === 0 ? (
          <div className="event-log__empty">
            <RadioIcon size={28} />
            <strong>No events yet</strong>
            <span>Press a board control to begin.</span>
          </div>
        ) : (
          events.map((event, index) => (
            <div className={`log-entry log-entry--${event.type}`} key={event.id}>
              <div className="log-entry__icon"><LogIcon type={event.type} /></div>
              <div>
                <div className="log-entry__time">
                  {event.time}
                  {index === 0 && <span>NEW</span>}
                </div>
                <strong>{event.title}</strong>
                <p>{event.detail}</p>
              </div>
            </div>
          ))
        )}
      </div>
    </aside>
  )
}
