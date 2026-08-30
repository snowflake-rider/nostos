---
marp: true
theme: default
size: 16:9
paginate: true
html: true
footer: 'NOSTOS · Bluetooth Setup · 2026-08-31'
---

<style>
:root {
  --bg: #08111f;
  --panel: #101d30;
  --panel-2: #13253d;
  --line: #27415f;
  --text: #e9f2ff;
  --muted: #9bb0ca;
  --blue: #62a8ff;
  --green: #57e6a5;
  --yellow: #ffd166;
  --red: #ff7185;
}

section {
  box-sizing: border-box;
  padding: 48px 58px 54px;
  background:
    radial-gradient(circle at 92% 8%, rgba(98, 168, 255, .14), transparent 28%),
    linear-gradient(145deg, #08111f 0%, #0b1728 100%);
  color: var(--text);
  font-family: -apple-system, BlinkMacSystemFont, "Apple SD Gothic Neo", "Noto Sans KR", sans-serif;
  font-size: 23px;
  line-height: 1.45;
}

section::before {
  content: '';
  position: absolute;
  inset: 0 auto 0 0;
  width: 6px;
  background: linear-gradient(var(--blue), var(--green));
}

h1, h2, h3 { margin: 0; color: var(--text); }
h1 { font-size: 58px; line-height: 1.12; letter-spacing: -2px; }
h2 { font-size: 38px; margin-bottom: 28px; letter-spacing: -1px; }
h2::before { content: '# '; color: var(--green); }
h3 { color: var(--blue); font-size: 24px; margin-bottom: 10px; }
p { margin: 10px 0; }
strong { color: var(--green); }
code {
  padding: .08em .34em;
  border: 1px solid #2b4868;
  border-radius: 6px;
  background: #091523;
  color: #a9d3ff;
  font-family: "SFMono-Regular", Consolas, monospace;
}
ul { margin: 8px 0; padding-left: 1.15em; }
li { margin: 8px 0; }
li::marker { color: var(--green); }
footer { color: #7890ad; font-size: 13px; }
section::after { color: #7890ad; font-size: 14px; }

.lead { display: flex; flex-direction: column; justify-content: center; }
.lead h1 { max-width: 930px; }
.lead .kicker { color: var(--green); font-family: "SFMono-Regular", monospace; font-weight: 700; }
.lead .sub { margin-top: 24px; color: var(--muted); font-size: 25px; }
.snapshot { margin-top: 34px; color: #7890ad; font-size: 17px; }

.grid2 { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
.grid3 { display: grid; grid-template-columns: repeat(3, 1fr); gap: 16px; }
.card {
  padding: 20px 22px;
  border: 1px solid var(--line);
  border-radius: 14px;
  background: linear-gradient(145deg, rgba(19,37,61,.92), rgba(12,26,44,.92));
}
.card p { margin: 5px 0; }
.card ul { font-size: 20px; }
.big { font-size: 32px; font-weight: 800; line-height: 1.2; }
.muted { color: var(--muted); }
.small { font-size: 18px; }
.mono { font-family: "SFMono-Regular", Consolas, monospace; }
.blue { color: var(--blue); }
.green { color: var(--green); }
.yellow { color: var(--yellow); }
.red { color: var(--red); }

.flow {
  display: grid;
  grid-template-columns: 1fr auto 1fr auto 1.15fr auto 1fr;
  gap: 10px;
  align-items: stretch;
  margin: 20px 0 24px;
}
.node {
  display: flex;
  min-height: 120px;
  padding: 16px;
  align-items: center;
  justify-content: center;
  text-align: center;
  border: 1px solid var(--line);
  border-radius: 14px;
  background: var(--panel);
  font-weight: 750;
}
.arrow { display: flex; align-items: center; color: var(--green); font-size: 30px; font-weight: 800; }
.band {
  padding: 13px 18px;
  border-left: 4px solid var(--blue);
  border-radius: 8px;
  background: rgba(98,168,255,.10);
}
.chips { display: flex; flex-wrap: wrap; gap: 10px; }
.chip {
  padding: 6px 12px;
  border: 1px solid var(--line);
  border-radius: 999px;
  background: var(--panel);
  color: #cfe3fb;
  font-family: "SFMono-Regular", Consolas, monospace;
  font-size: 17px;
}
table { width: 100%; border-collapse: collapse; font-size: 20px; }
th { color: var(--blue); background: #10233a; }
th, td { padding: 12px 14px; border: 1px solid var(--line); }
td { background: rgba(16,29,48,.74); }
.status { display: grid; grid-template-columns: 170px 1fr; gap: 10px; align-items: center; }
.badge { padding: 8px 11px; border-radius: 8px; text-align: center; font-weight: 800; font-size: 17px; }
.ok { color: #05291b; background: var(--green); }
.hold { color: #332400; background: var(--yellow); }
.no { color: #3c0c14; background: var(--red); }
</style>

<!-- _class: lead -->
<!-- _paginate: false -->

<div class="kicker">NOSTOS / CURRENT SNAPSHOT</div>

# 현 블루투스 세팅

ESP32-S3 3대의 Bluetooth Mesh + XOSS BLE 연결 구조

<div class="snapshot">기준: 2026-08-31 로컬 working tree · 발표용 요약</div>

---

## 한 장 요약

<div class="grid3">
<div class="card">
  <h3>무선 백본</h3>
  <p class="big">Bluetooth<br>Mesh</p>
  <p class="muted">3개 ESP32-S3 노드</p>
</div>
<div class="card">
  <h3>센서 링크</h3>
  <p class="big">BLE GATT<br>Client</p>
  <p class="muted">XOSS CSC → ESP32-76</p>
</div>
<div class="card">
  <h3>로컬 연결</h3>
  <p class="big">UART<br>115200 8N1</p>
  <p class="muted">ESP32 ↔ paired STM32</p>
</div>
</div>

<div class="band" style="margin-top:24px">
<strong>핵심:</strong> ESP32는 무선 운반, STM32는 센서 상태와 공식 NOSTOS 메시지의 주체입니다.
</div>

---

## 전체 구성

<div class="flow">
  <div class="node"><span><span class="green">XOSS</span><br><span class="small">CSC 속도센서</span></span></div>
  <div class="arrow">→</div>
  <div class="node"><span><span class="blue">ESP32-76</span><br><span class="small">BLE GATT Client</span></span></div>
  <div class="arrow">↔</div>
  <div class="node"><span>STM32-1<br><span class="small">RIDE 저장·발행</span></span></div>
  <div class="arrow">→</div>
  <div class="node"><span><span class="green">Mesh C001</span><br><span class="small">3개 노드 공유</span></span></div>
</div>

<div class="flow" style="grid-template-columns:1fr auto 1fr auto 1fr">
  <div class="node"><span>STM32<br><span class="small">버튼·낙상·환경</span></span></div>
  <div class="arrow">↔</div>
  <div class="node"><span>ESP32-S3<br><span class="small">UART ↔ Mesh Bridge</span></span></div>
  <div class="arrow">↔</div>
  <div class="node"><span>다른 노드<br><span class="small">ESP32 → STM32</span></span></div>
</div>

<p class="small muted">Mesh는 작은 이벤트·상태를 전송합니다. MP3 오디오는 Mesh로 보내지 않습니다.</p>

---

## 블루투스 스택

<div class="grid2">
<div class="card">
  <h3>Platform</h3>
  <ul>
    <li><strong>ESP32-S3</strong> / ESP-IDF 5.5.5</li>
    <li><strong>Bluedroid ON</strong> · NimBLE OFF</li>
    <li>BLE 4.2 기능 ON · BLE 5 기능 OFF</li>
    <li>Bluetooth Classic / A2DP 제외</li>
  </ul>
</div>
<div class="card">
  <h3>Enabled roles</h3>
  <ul>
    <li>Bluetooth Mesh Node</li>
    <li>PB-ADV + PB-GATT Provisioning</li>
    <li>GATT Proxy Server</li>
    <li>GATT Client + Mesh BLE Scan coexistence</li>
  </ul>
</div>
</div>

<div class="chips" style="margin-top:24px">
  <span class="chip">CONFIG_BLE_MESH=y</span>
  <span class="chip">CONFIG_BT_GATTC_ENABLE=y</span>
  <span class="chip">CONFIG_BLE_MESH_SUPPORT_BLE_SCAN=y</span>
</div>

---

## 노드 맵

<table>
  <thead>
    <tr><th>위치</th><th>ESP32</th><th>Mesh Primary</th><th>NOSTOS Source</th><th>추가 역할</th></tr>
  </thead>
  <tbody>
    <tr><td>FRONT</td><td><strong>76</strong></td><td><code>0x0003</code></td><td>source 1</td><td>XOSS BLE owner</td></tr>
    <tr><td>REAR</td><td><strong>D6</strong></td><td><code>0x0005</code></td><td>source 2</td><td>MPU6050 paired STM</td></tr>
    <tr><td>CENTER</td><td><strong>B6</strong></td><td><code>0x0006</code></td><td>source 3</td><td>DHT11 paired STM</td></tr>
  </tbody>
</table>

<div class="band" style="margin-top:24px">
세 노드는 <strong>동일한 v2 이미지</strong>를 사용하고, provisioned primary 주소로 자신의 source를 결정합니다.
</div>

---

## Mesh 모델

<div class="grid2">
<div class="card">
  <h3>제품 데이터</h3>
  <p><strong>Vendor Model</strong> <code>0x02E5 / 0x0001</code></p>
  <p>v2 opcode <code>0x21</code></p>
  <p>Publish / Subscribe: <code>0xC001</code></p>
  <p class="small muted">버튼·낙상·주행·환경 등 NOSTOS 메시지</p>
</div>
<div class="card">
  <h3>진단 제어</h3>
  <p><strong>Generic OnOff</strong> Server + Client</p>
  <p>Client publish: <code>0xC000</code></p>
  <p class="small muted">USB 콘솔의 on/off 확인용. 제품 데이터 경로와 분리.</p>
</div>
</div>

<div class="band" style="margin-top:24px">
<code>event_ready=1</code> 조건: 주소 + NetKey/AppKey + Model Bind + <code>C001</code> publication + TTL/period/retransmit 일치
</div>

---

## Relay 설정

<div class="grid3">
<div class="card">
  <h3>기본 전송</h3>
  <p class="big">TTL 7</p>
  <p class="small">Net transmit<br><code>(2, 20 ms)</code></p>
</div>
<div class="card">
  <h3>Relay</h3>
  <p class="big yellow">지원 ON</p>
  <p class="small">부팅 기본값은 Disabled<br>Provisioner가 노드별 활성화</p>
</div>
<div class="card">
  <h3>기타 Feature</h3>
  <p>GATT Proxy <strong>ON</strong></p>
  <p>Friend / LPN <span class="red">OFF</span></p>
  <p>Beacon <strong>ON</strong></p>
</div>
</div>

<div class="band" style="margin-top:24px">
기본 ADV 출력은 generated config 기준 <strong>+9 dBm</strong>. 콘솔에서 시험용 <code>-24 dBm</code> 전환 가능.
</div>

---

## XOSS 연결

<div class="grid2">
<div class="card">
  <h3>찾기·연결</h3>
  <ul>
    <li>Owner: <strong>ESP32-76 / 0x0003</strong></li>
    <li>이름: <code>XOSS S-26518</code></li>
    <li>CSC Service: <code>0x1816</code></li>
    <li>Measurement Notify: <code>0x2A5B</code></li>
  </ul>
</div>
<div class="card">
  <h3>현재 파라미터</h3>
  <ul>
    <li>바퀴 둘레: <strong>2100 mm</strong></li>
    <li>Stale 판단: <strong>3000 ms</strong></li>
    <li>Reconnect: <strong>2000 ms</strong></li>
    <li>BLE callback → 고정 큐 8칸</li>
  </ul>
</div>
</div>

<div class="band" style="margin-top:24px">
속도·누적거리는 먼저 STM32-1에 <code>SENSOR_LINK_RIDE</code>로 전달됩니다.
</div>

---

## 데이터 소유권

<div class="flow" style="grid-template-columns:1fr auto 1fr auto 1fr auto 1fr">
  <div class="node"><span>XOSS<br><span class="small">CSC Notify</span></span></div>
  <div class="arrow">→</div>
  <div class="node"><span>ESP32-76<br><span class="small">decode</span></span></div>
  <div class="arrow">→</div>
  <div class="node"><span>STM32-1<br><span class="small">save + identity</span></span></div>
  <div class="arrow">→</div>
  <div class="node"><span>Mesh<br><span class="small">NOSTOS_RIDE</span></span></div>
</div>

- STM32가 공식 <code>source / session / sequence</code>를 부여
- ESP32는 같은 센서값을 별도 NOSTOS 패킷으로 중복 발행하지 않음
- Mesh 수신 데이터는 각 ESP32 cache를 거쳐 paired STM32로 전달
- UART v2 framing과 Mesh 암호화·TTL·재조립은 서로 다른 계층

---

## 보존·운영 정책

<div class="grid2">
<div class="card">
  <h3>보존</h3>
  <ul>
    <li>Mesh Settings / provisioning은 NVS에 유지</li>
    <li>앱은 Key index metadata만 별도 저장</li>
    <li>NVS 오류 시 자동 erase 없이 정지</li>
  </ul>
</div>
<div class="card">
  <h3>운영</h3>
  <ul>
    <li>미 provision 상태만 PB-ADV/PB-GATT 광고</li>
    <li>USB 콘솔 <code>factory-reset</code> 비활성</li>
    <li>Relay·키·그룹은 Provisioner에서 변경</li>
  </ul>
</div>
</div>

<div class="band" style="margin-top:24px">
<strong>원칙:</strong> Flash·reset·reprovision 없이 현재 Mesh 키와 노드 주소를 보존합니다.
</div>

---

## 검증 상태

<div class="status">
  <div class="badge ok">CONFIRMED</div><div>기본·resolved config와 활성 코드 경로 대조</div>
  <div class="badge ok">6 / 6 PASS</div><div><code>fw check esp32</code> host 검사 (2026-08-31)</div>
  <div class="badge hold">NOT RUN</div><div>이번 문서 작업에서 ESP32 target build / Flash</div>
  <div class="badge hold">NOT RUN</div><div>세 보드 live status / XOSS 실제 연결 / RF delivery</div>
  <div class="badge no">NOT PROVEN</div><div>3-node multi-hop Relay와 전체 물리 출력 E2E</div>
</div>

<div class="band" style="margin-top:24px">
<code>API accepted</code> 또는 <code>event_ready=1</code>은 peer 수신·실물 동작 성공과 같지 않습니다.
</div>

---

## 발표 결론

<div class="grid3">
<div class="card">
  <h3>현재</h3>
  <p class="big">Mesh + GATT</p>
  <p class="muted">한 ESP32-S3에서 공존</p>
</div>
<div class="card">
  <h3>구조</h3>
  <p class="big">3 Nodes</p>
  <p class="muted">C001 그룹으로 상태 공유</p>
</div>
<div class="card">
  <h3>다음 검증</h3>
  <p class="big">Physical E2E</p>
  <p class="muted">XOSS → STM → Mesh → peer</p>
</div>
</div>

<div class="band" style="margin-top:30px">
다음 성공 기준: <strong>실측 2100 mm 확인 → XOSS Notify → STM32 RIDE → C001 수신 → 원격 출력</strong>
</div>

<p class="small muted" style="margin-top:24px">근거 파일: firmware/esp32/sdkconfig.defaults · sdkconfig · main/{main,mesh_node,xoss_ble,bridge_runtime_v2}.c · firmware/protocol/V2.md · DEVICES.md</p>
