import { createRequire } from "node:module";
const require = createRequire(import.meta.url);
const pptxgen = require("/Users/kafka/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/pptxgenjs");

const pptx = new pptxgen();
pptx.layout = "LAYOUT_WIDE";
pptx.author = "NOSTOS Team";
pptx.subject = "NOSTOS 개발완료보고서";
pptx.title = "NOSTOS - 개발완료보고서 20P";
pptx.company = "NOSTOS";
pptx.lang = "ko-KR";
pptx.theme = {
  headFontFace: "Arial",
  bodyFontFace: "Arial",
  lang: "ko-KR",
};
pptx.defineSlideMaster({
  title: "NOSTOS_MASTER",
  background: { color: "F8FAFC" },
  objects: [],
  slideNumber: { x: 12.45, y: 7.08, w: 0.45, h: 0.18, color: "5B6472", fontFace: "Arial", fontSize: 9, align: "right" },
});

const C = {
  bg: "F8FAFC", white: "FFFFFF", navy: "071A2B", gray: "5B6472", pale: "E9EEF3",
  cyan: "55C1E7", cyanText: "007A9E", cyanPale: "E7F7FC",
  coral: "FF6B6B", coralText: "C24141", coralPale: "FFF0F0",
  green: "2F855A", greenPale: "EAF7F0", amber: "B7791F", amberPale: "FFF7E6",
};

const W = 13.333, H = 7.5;
const sections = {
  1: "SECTION 01 · 개발 개요", 2: "SECTION 01 · 개발 개요", 3: "SECTION 01 · 개발 개요",
  4: "SECTION 02 · 개발 환경 설명", 5: "SECTION 02 · 개발 환경 설명", 6: "SECTION 02 · 개발 환경 설명",
  7: "SECTION 03 · 개발 프로그램 설명", 8: "SECTION 03 · 개발 프로그램 설명", 9: "SECTION 03 · 개발 프로그램 설명", 10: "SECTION 03 · 개발 프로그램 설명", 11: "SECTION 03 · 개발 프로그램 설명",
  12: "SECTION 04 · 장애 요인과 해결 방안", 13: "SECTION 04 · 장애 요인과 해결 방안",
  14: "SECTION 05 · 개발 결과물의 차별성", 15: "SECTION 05 · 개발 결과물의 차별성", 16: "SECTION 05 · 개발 결과물의 차별성",
  17: "SECTION 06 · 파급력 및 기대 효과", 18: "SECTION 06 · 파급력 및 기대 효과",
  19: "SECTION 07 · 개발 일정 및 업무 분장", 20: "SECTION 07 · 개발 일정 및 업무 분장",
};

function txt(slide, text, x, y, w, h, opts = {}) {
  slide.addText(text, {
    x, y, w, h, margin: opts.margin ?? 0,
    fontFace: opts.fontFace ?? "Arial", fontSize: opts.fontSize ?? 17,
    color: opts.color ?? C.navy, bold: opts.bold ?? false,
    align: opts.align ?? "left", valign: opts.valign ?? "mid",
    breakLine: false, fit: "shrink", paraSpaceAfterPt: opts.paraSpaceAfterPt ?? 0,
    bullet: opts.bullet, isTextBox: true, hyperlink: opts.hyperlink,
  });
}

function line(slide, x, y, w, h = 0, color = C.pale, width = 1.2, end = null, dash = "solid") {
  slide.addShape(pptx.ShapeType.line, {
    x, y, w, h, line: { color, width, dashType: dash, endArrowType: end || undefined },
  });
}

function rect(slide, x, y, w, h, fill = C.white, radius = 0.12, stroke = C.pale, sw = 1) {
  const type = radius ? pptx.ShapeType.roundRect : pptx.ShapeType.rect;
  slide.addShape(type, { x, y, w, h, rectRadius: radius, fill: { color: fill }, line: { color: stroke, width: sw }, radius });
}

function badge(slide, text, x, y, w, fill, color = C.navy) {
  rect(slide, x, y, w, 0.32, fill, 0.12, fill, 0);
  txt(slide, text, x, y + 0.01, w, 0.28, { fontSize: 10.5, bold: true, color, align: "center" });
}

function chrome(slide, n, title, subtitle, footer) {
  slide.background = { color: C.bg };
  if (n > 1) txt(slide, sections[n], 0.55, 0.28, 8.0, 0.25, { fontSize: 10.5, bold: true, color: C.cyanText });
  txt(slide, title, 0.55, n === 1 ? 0.55 : 0.62, 12.15, 0.65, { fontSize: n === 1 ? 31 : 25.5, bold: true, valign: "top" });
  if (subtitle) txt(slide, subtitle, 0.57, n === 1 ? 1.38 : 1.25, 11.8, 0.42, { fontSize: 14.5, color: C.gray, valign: "top" });
  line(slide, 0.55, 6.92, 12.18, 0, C.pale, 1);
  if (footer) txt(slide, footer, 0.55, 6.99, 10.9, 0.24, { fontSize: 9.5, color: C.gray });
  txt(slide, `${String(n).padStart(2, "0")}/20`, 11.72, 6.97, 0.95, 0.25, { fontSize: 11.5, bold: true, color: C.navy, align: "right" });
}

function labelValue(slide, label, value, x, y, w, accent = C.cyanText) {
  txt(slide, label, x, y, w, 0.25, { fontSize: 10.5, bold: true, color: accent });
  txt(slide, value, x, y + 0.28, w, 0.55, { fontSize: 17, bold: true, valign: "top" });
}

function node(slide, x, y, r, label, color = C.navy, fill = C.white) {
  slide.addShape(pptx.ShapeType.ellipse, { x: x-r, y: y-r, w: r*2, h: r*2, fill: { color: fill }, line: { color, width: 2.2 } });
  txt(slide, label, x-r, y-r+0.02, r*2, r*2-0.04, { fontSize: 13, bold: true, color, align: "center" });
}

function addNotes(slide, sources, bridge) {
  slide.addNotes(`발표 연결: ${bridge}\n\n[Sources]\n${sources.map(s => `- ${s}`).join("\n")}`);
}

// N01
{
  const s = pptx.addSlide("NOSTOS_MASTER");
  chrome(s, 1, "NOSTOS", "STM32F411RE · ESP32-S3 Bluetooth Mesh 기반 그룹 라이딩 안전·상태 공유 시스템", "");
  txt(s, "세 라이더, 하나의 안전 상태", 0.6, 2.08, 5.2, 0.55, { fontSize: 25, bold: true });
  txt(s, "로컬 제어는 유지하고, 필요한 상태와 STOP은 그룹 전체로 공유합니다.", 0.6, 2.78, 5.0, 0.8, { fontSize: 17.5, color: C.gray, valign: "top" });
  badge(s, "STM32 DEVICE CONTROL", 0.6, 4.05, 2.25, C.cyanPale, C.cyanText);
  badge(s, "UART · CRC16", 2.98, 4.05, 1.75, C.white, C.navy);
  badge(s, "ESP32-S3 MESH", 4.86, 4.05, 1.95, C.coralPale, C.coralText);
  rect(s, 7.15, 1.85, 5.45, 4.45, C.navy, 0.18, C.navy, 0);
  line(s, 8.25, 3.84, 3.25, 0, C.cyan, 2.4);
  node(s, 8.15, 3.15, 0.55, "N1", C.cyan, C.navy);
  node(s, 10.0, 4.68, 0.55, "N2", C.white, C.navy);
  node(s, 11.6, 2.78, 0.55, "N3", C.coral, C.navy);
  line(s, 8.48, 3.48, 1.15, 0.78, C.cyan, 1.5);
  line(s, 10.45, 4.28, 0.82, -1.0, C.coral, 1.5);
  line(s, 8.69, 3.06, 2.35, -0.19, C.white, 1.5);
  txt(s, "STATE_UPDATE", 8.05, 5.45, 1.65, 0.28, { fontSize: 11, bold: true, color: C.cyan });
  txt(s, "STOP_REQUEST", 9.75, 5.45, 1.65, 0.28, { fontSize: 11, bold: true, color: C.coral });
  txt(s, "STOP_ACK", 11.15, 5.45, 1.1, 0.28, { fontSize: 11, bold: true, color: C.white });
  addNotes(s, ["README.md", "firmware/README.md"], "문제 배경으로 넘어가며 왜 세 노드가 같은 상태를 봐야 하는지 설명합니다.");
}

// N02
{
  const s = pptx.addSlide("NOSTOS_MASTER");
  chrome(s, 2, "그룹 라이딩의 사각지대를 ‘공유 상태’로 줄이다", "거리·소음·시선 분산은 서로 다른 문제가 아니라 같은 정보 공백에서 시작됩니다.", "");
  const rows = [
    ["01", "거리·소음", "앞·뒤 라이더의 상황이 음성·손짓만으로 즉시 전달되지 않음"],
    ["02", "시선 분산", "동료 확인과 휴대전화 조작이 도로 주의를 끊음"],
    ["03", "긴급 전달", "정차·낙상·SOS는 팀 전체에 빠르고 명확하게 도달해야 함"],
  ];
  rows.forEach((r, i) => {
    const y = 2.0 + i*1.28;
    txt(s, r[0], 0.6, y, 0.55, 0.42, { fontSize: 17, bold: true, color: i === 2 ? C.coralText : C.cyanText });
    txt(s, r[1], 1.25, y-0.02, 1.65, 0.42, { fontSize: 19, bold: true });
    txt(s, r[2], 2.95, y-0.02, 4.05, 0.6, { fontSize: 15.5, color: C.gray, valign: "top" });
    if (i < 2) line(s, 0.6, y+0.85, 6.3, 0, C.pale, 1);
  });
  rect(s, 7.45, 1.95, 5.15, 4.55, C.navy, 0.18, C.navy, 0);
  txt(s, "SHARED SAFETY STATE", 7.9, 2.35, 4.2, 0.32, { fontSize: 12, bold: true, color: C.cyan });
  txt(s, "세 명이\n같은 팀 상태를 본다", 7.9, 2.9, 3.9, 1.1, { fontSize: 25, bold: true, color: C.white, valign: "top" });
  line(s, 8.0, 4.4, 3.55, 0, C.cyan, 2.4, "triangle");
  txt(s, "인지", 7.95, 4.7, 0.8, 0.3, { fontSize: 14, bold: true, color: C.white });
  txt(s, "공유", 9.35, 4.7, 0.8, 0.3, { fontSize: 14, bold: true, color: C.white });
  txt(s, "행동", 10.8, 4.7, 0.8, 0.3, { fontSize: 14, bold: true, color: C.white });
  txt(s, "장치 연결이 아니라 판단 근거의 공유", 7.9, 5.55, 4.0, 0.45, { fontSize: 15, bold: true, color: C.coral });
  addNotes(s, ["docs/study/NOSTOS_QA.md", "docs/media/nostos-essence/README.md"], "다음 장에서 이 필요성을 구현 목표와 자료 링크로 구체화합니다.");
}

// N03
{
  const s = pptx.addSlide("NOSTOS_MASTER");
  chrome(s, 3, "로컬 제어를 유지하면서 필요한 상태와 STOP을 공유한다", "목표는 세 노드를 하나의 안전한 주행 그룹으로 묶는 것입니다.", "");
  const steps = [
    ["01", "인지", "속도·온습도·움직임\nPace Up/Down · STOP"],
    ["02", "공유", "STM32 ↔ UART ↔ ESP32-S3\nBluetooth Mesh 공통 메시지"],
    ["03", "행동", "OLED · RGB · Audio · Buzzer\n주행 신호와 긴급 알림"],
  ];
  steps.forEach((r,i)=>{
    const x=0.65+i*3.25;
    badge(s,r[0],x,2.05,0.58,i===2?C.coralPale:C.cyanPale,i===2?C.coralText:C.cyanText);
    txt(s,r[1],x,2.58,2.35,0.42,{fontSize:21,bold:true});
    txt(s,r[2],x,3.15,2.55,1.05,{fontSize:15.5,color:C.gray,valign:"top"});
    if(i<2) line(s,x+2.55,3.05,0.55,0,C.pale,2,"triangle");
  });
  rect(s,10.3,1.98,2.35,3.55,C.navy,0.18,C.navy,0);
  txt(s,"자료 링크",10.65,2.3,1.65,0.36,{fontSize:16,bold:true,color:C.white});
  txt(s,"SOURCE CODE",10.65,3.05,1.6,0.25,{fontSize:10.5,bold:true,color:C.cyan});
  txt(s,"GitHub Repository →",10.65,3.35,1.6,0.42,{fontSize:15,bold:true,color:C.white,hyperlink:{url:"https://github.com/snowflake-rider/nostos"}});
  txt(s,"DEMO VIDEO",10.65,4.12,1.6,0.25,{fontSize:10.5,bold:true,color:C.coral});
  txt(s,"Ride Signals →",10.65,4.42,1.6,0.42,{fontSize:15,bold:true,color:C.white,hyperlink:{url:"https://github.com/snowflake-rider/nostos/blob/main/docs/media/nostos-essence/public/video/nostos-ride-signals.mp4"}});
  txt(s,"운전자의 시선은 도로에, 팀 정보는 모든 노드에",0.7,5.35,8.9,0.5,{fontSize:20,bold:true});
  addNotes(s,["DEVICES.md","firmware/protocol/README.md"],"이 목표를 어떤 MCU 책임 분리로 구현했는지 개발 환경으로 전환합니다.");
}

// N04
{
  const s = pptx.addSlide("NOSTOS_MASTER");
  chrome(s,4,"이중 MCU는 ‘device control’과 ‘Mesh bridge’를 분리한다","shared protocol은 어느 한쪽의 소유물이 아니라 두 MCU가 함께 사용하는 계약입니다.","");
  rect(s,0.65,1.95,4.75,3.85,C.white,0.16,C.cyan,1.6);
  badge(s,"STM32F411RE",0.95,2.22,1.55,C.cyanPale,C.cyanText);
  txt(s,"DEVICE CONTROL",0.95,2.75,3.3,0.42,{fontSize:23,bold:true});
  txt(s,"센서·버튼 입력\nOLED·RGB·Audio·Buzzer 출력\n로컬 안전 감지",0.95,3.45,3.5,1.45,{fontSize:17,color:C.gray,valign:"top"});
  rect(s,7.9,1.95,4.75,3.85,C.navy,0.16,C.navy,0);
  badge(s,"ESP32-S3",8.2,2.22,1.2,C.coralPale,C.coralText);
  txt(s,"MESH BRIDGE",8.2,2.75,3.1,0.42,{fontSize:23,bold:true,color:C.white});
  txt(s,"UART framing·queue\nBluetooth Mesh publish/receive\nSTOP·ACK 우선 정책",8.2,3.45,3.55,1.45,{fontSize:17,color:"D8E4EC",valign:"top"});
  rect(s,5.0,3.05,3.25,1.55,C.cyanPale,0.14,C.cyan,1.2);
  txt(s,"SHARED CONTRACT",5.3,3.25,2.65,0.25,{fontSize:10.5,bold:true,color:C.cyanText,align:"center"});
  txt(s,"nostos_protocol\nCRC16 · UART framing",5.3,3.62,2.65,0.62,{fontFace:"Roboto Mono",fontSize:14.5,bold:true,align:"center"});
  line(s,4.35,3.82,0.65,0,C.cyanText,2,"triangle");
  line(s,8.25,3.82,-0.65,0,C.navy,2,"triangle");
  labelValue(s,"LANGUAGE","Embedded C",0.95,5.95,2.3);
  labelValue(s,"FRAMEWORK","STM32 HAL · ESP-IDF",3.75,5.95,3.0);
  labelValue(s,"BUILD","CMake · fw wrapper",7.35,5.95,3.0);
  addNotes(s,["firmware/protocol/README.md","firmware/protocol/nostos_protocol.h","firmware/stm32/CMakeLists.txt","firmware/esp32/main/CMakeLists.txt"],"책임 분리 이후 세 노드의 공통 구조와 센서 역할을 보여줍니다.");
}

// N05
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,5,"세 노드는 공통 출력 구조와 서로 다른 센서 역할을 공유한다","Node ID는 위치나 센서 종류와 분리되고, 현재 profile만 장치 역할을 정의합니다.","");
  line(s,1.25,4.9,10.8,0,C.navy,2.6);
  const ns=[
    [2.1,"N1","속도 상태","XOSS speed sensor",C.cyan],
    [6.65,"N2","환경 상태","DHT11 temp·humidity",C.white],
    [11.2,"N3","안전 상태","MPU6050 fall candidate",C.coral],
  ];
  ns.forEach(([x,id,role,sensor,col],i)=>{
    line(s,x,4.9,0,-1.25,col,2);
    node(s,x,3.25,0.58,id,col,i===1?C.navy:C.white);
    txt(s,role,x-1.25,2.05,2.5,0.35,{fontSize:19,bold:true,align:"center"});
    txt(s,sensor,x-1.35,2.48,2.7,0.45,{fontSize:14.5,color:C.gray,align:"center"});
  });
  rect(s,1.15,5.35,10.95,0.82,C.navy,0.12,C.navy,0);
  txt(s,"COMMON RAIL",1.45,5.59,1.45,0.28,{fontSize:11,bold:true,color:C.cyan});
  txt(s,"SSD1306  ·  VS1003B  ·  Button ×4  ·  RGB LED  ·  Buzzer",3.1,5.52,8.5,0.38,{fontSize:16,bold:true,color:C.white,align:"center"});
  addNotes(s,["DEVICES.md","PINS.md","docs/adr/0006-rider-node-id-separates-identity-from-capabilities.md"],"하드웨어 구성을 어떤 증거 단계로 검증했는지 다음 장에서 경계를 명확히 합니다.");
}

// N06
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,6,"코드·빌드·실물 동작은 서로 다른 증거다","검증 단계를 분리해야 host PASS를 실제 3-node 동작으로 과장하지 않습니다.","");
  const ladder=[
    ["01","CODE","source inspection",C.green,C.greenPale],
    ["02","HOST CHECK","STM32 12/12 · ESP32 3/3 · Protocol 1/1",C.green,C.greenPale],
    ["03","TARGET BUILD","toolchain configure/build",C.cyanText,C.cyanPale],
    ["04","PHYSICAL COMPONENT","sensor · RF · output per board",C.amber,C.amberPale],
    ["05","PHYSICAL E2E","3-node STOP path",C.coralText,C.coralPale],
  ];
  ladder.forEach((r,i)=>{
    const x=0.75+i*2.5, y=5.65-i*0.72;
    rect(s,x,y,2.15,0.78,r[4],0.12,r[3],1.3);
    txt(s,r[0],x+0.15,y+0.08,0.38,0.24,{fontSize:10.5,bold:true,color:r[3]});
    txt(s,r[1],x+0.55,y+0.08,1.45,0.25,{fontSize:12.5,bold:true,color:r[3]});
    txt(s,r[2],x+0.15,y+0.38,1.85,0.28,{fontSize:10.5,color:C.gray});
    if(i<4) line(s,x+2.15,y+0.38,0.35,-0.72,C.pale,1.6,"triangle");
  });
  badge(s,"VERIFIED",0.8,2.0,1.15,C.greenPale,C.green);
  txt(s,"Host checks는 software behavior evidence",2.1,1.96,4.1,0.38,{fontSize:17,bold:true});
  badge(s,"PENDING",7.0,2.0,1.05,C.coralPale,C.coralText);
  txt(s,"센서·RF·출력의 3-node physical E2E",8.25,1.96,4.3,0.38,{fontSize:17,bold:true});
  addNotes(s,["firmware/tools/fw","firmware/stm32/host-tests/","firmware/esp32/host-tests/","firmware/protocol/tests/"],"이제 검증 경계를 유지한 채 전체 code architecture를 한 장으로 연결합니다.");
}

// N07
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,7,"한 라이더의 입력을 다른 라이더의 application 수락까지 추적한다","정상 상태는 latest-state, 긴급 STOP은 priority, ACK는 peer STM32 acceptance 이후에만 생성됩니다.","");
  const tower=(x,title,accept=false)=>{
    txt(s,title,x,1.84,3.25,0.3,{fontSize:12,bold:true,color:C.gray,align:"center"});
    const blocks=[
      ["DEVICE I/O","Sensor · Button\nOLED · RGB · Audio",C.white,C.navy],
      ["STM32 SERVICES",accept?"message service\nACCEPT gate":"message service\nsafety · display",C.cyanPale,C.navy],
      ["ESP32 BRIDGE","bridge_runtime\npriority/latest-state",C.navy,C.white],
    ];
    blocks.forEach((b,i)=>{
      const y=2.25+i*1.18; rect(s,x,y,3.25,0.92,b[2],0.12,i===2?C.navy:C.pale,1);
      txt(s,b[0],x+0.18,y+0.1,1.25,0.22,{fontSize:10.5,bold:true,color:b[3]});
      txt(s,b[1],x+1.38,y+0.07,1.65,0.62,{fontSize:12.5,color:b[3],align:"right",valign:"mid"});
      if(i<2){ line(s,x+1.62,y+0.92,0,0.26,C.gray,1.2,"triangle"); }
    });
    badge(s,"nostos_protocol · CRC16/UART",x+0.43,3.12,2.38,C.white,C.cyanText);
  };
  tower(0.6,"RIDER A · 발신"); tower(9.48,"RIDER B · 수신",true);
  rect(s,4.15,2.2,4.75,3.55,C.white,0.16,C.pale,1);
  txt(s,"BLUETOOTH MESH",4.55,2.42,3.95,0.28,{fontSize:12,bold:true,color:C.gray,align:"center"});
  node(s,6.52,3.15,0.52,"MESH",C.navy,C.white);
  const lanes=[
    [3.85,"STATE_UPDATE · latest-state",C.cyanText,"triangle"],
    [4.55,"STOP_REQUEST · priority",C.coralText,"triangle"],
    [5.25,"STOP_ACK · STM32 app ACCEPT 이후",C.navy,"triangle"],
  ];
  lanes.forEach((l,i)=>{
    if(i<2) line(s,3.85,l[0],5.55,0,l[2],2,l[3]);
    else line(s,9.4,l[0],-5.55,0,l[2],2,l[3]);
    rect(s,5.15,l[0]-0.18,2.75,0.38,C.bg,0.08,C.bg,0);
    txt(s,l[1],5.15,l[0]-0.14,2.75,0.28,{fontFace:"Roboto Mono",fontSize:11,bold:true,color:l[2],align:"center"});
  });
  txt(s,"Transport receipt ≠ application acceptance",4.55,5.9,3.95,0.35,{fontSize:13.5,bold:true,color:C.coralText,align:"center"});
  addNotes(s,["firmware/protocol/README.md","firmware/esp32/main/bridge_runtime.c","docs/designs/presentation-end-to-end-architecture-slide.md"],"전체 구조를 이해한 뒤, 먼저 물리 입력이 안정된 이벤트로 바뀌는 과정을 좁혀 봅니다.");
}

// N08
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,8,"물리 입력은 한 번의 의도를 안정된 application event로 바꾼다","Pull-up · 30 ms debounce · one-press event가 중복 입력을 줄이고 출력 policy를 결정합니다.","");
  const xs=[0.65,3.25,5.95,8.65,11.05];
  const blocks=[
    ["BUTTON","B1 Up · B2 Down\nB3 STOP · B4 Reset"],
    ["DEBOUNCE","30 ms stable\npress once"],
    ["EVENT","PACE / STOP\nLOCAL RESET"],
    ["POLICY","normal 2 s\nFALL priority"],
    ["OUTPUT","RGB · Audio\nBuzzer · OLED"],
  ];
  blocks.forEach((b,i)=>{
    const accent=i===2?C.coralText:C.cyanText;
    rect(s,xs[i],2.55,i===4?1.65:2.05,1.55,i===2?C.coralPale:C.white,0.14,accent,1.4);
    txt(s,b[0],xs[i]+0.18,2.8,i===4?1.3:1.7,0.28,{fontSize:12,bold:true,color:accent,align:"center"});
    txt(s,b[1],xs[i]+0.16,3.28,i===4?1.33:1.73,0.6,{fontSize:13.5,bold:true,align:"center"});
    if(i<4) line(s,xs[i]+(i===4?1.65:2.05),3.33,0.55,0,C.pale,1.8,"triangle");
  });
  rect(s,0.75,5.05,11.8,0.82,C.navy,0.12,C.navy,0);
  txt(s,"LONG HOLD",1.05,5.29,1.25,0.25,{fontSize:10.5,bold:true,color:C.cyan});
  txt(s,"Calibration 필요 상태에서 Button 1을 3초 hold → 재시도 session",2.55,5.19,9.45,0.4,{fontSize:16,bold:true,color:C.white});
  addNotes(s,["firmware/stm32/MyApp/hw/button.c","firmware/stm32/MyApp/hw/rgb_led.c","firmware/stm32/MyApp/hw/buzzer.c","firmware/stm32/MyApp/service/audio_service.c"],"이 이벤트와 센서 상태가 사용자에게 어떻게 보이는지 OLED dashboard로 이어갑니다.");
}

// N09
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,9,"128×64 화면에 주행·환경·그룹 신호를 통합한다","200 ms update · I²C1 · 0x3C — 작은 화면에서도 우선순위를 먼저 설계했습니다.","");
  rect(s,0.65,1.95,8.05,4.05,C.navy,0.18,C.navy,0);
  rect(s,1.05,2.28,7.25,3.28,"06131F",0.08,C.cyan,1.3);
  txt(s,"NOSTOS",1.35,2.5,2.0,0.38,{fontFace:"Roboto Mono",fontSize:16,bold:true,color:C.cyan});
  txt(s,"22.8 km/h",1.35,2.96,3.1,0.62,{fontFace:"Roboto Mono",fontSize:28,bold:true,color:C.white});
  txt(s,"0.123 km",5.78,3.02,1.95,0.36,{fontFace:"Roboto Mono",fontSize:16,bold:true,color:C.white,align:"right"});
  txt(s,"25.3 °C",1.35,3.85,1.85,0.34,{fontFace:"Roboto Mono",fontSize:15,color:C.white});
  txt(s,"61.0 %",3.35,3.85,1.45,0.34,{fontFace:"Roboto Mono",fontSize:15,color:C.white});
  line(s,1.3,4.35,6.55,0,"21435A",1);
  txt(s,"← Have an amazing ride! Safety first.",1.35,4.52,6.2,0.32,{fontFace:"Roboto Mono",fontSize:12,color:C.cyan});
  txt(s,"N1: ACCELERATE",1.35,4.97,3.4,0.34,{fontFace:"Roboto Mono",fontSize:15,bold:true,color:C.coral});
  txt(s,"READY · UART LINK",5.35,4.97,2.4,0.34,{fontFace:"Roboto Mono",fontSize:12,bold:true,color:C.white,align:"right"});
  txt(s,"SSD1306 DASHBOARD · 2:1 LIVE LAYOUT",1.05,5.69,7.25,0.22,{fontSize:10.5,bold:true,color:"9FB4C4",align:"center"});
  txt(s,"DISPLAY PRIORITY",9.35,2.12,2.75,0.28,{fontSize:11,bold:true,color:C.gray});
  const pr=[
    ["01","FALL","full-screen safety state",C.coralPale,C.coralText],
    ["02","CALIBRATION","ready · retry · failed",C.amberPale,C.amber],
    ["03","DASHBOARD","sensor + group state",C.cyanPale,C.cyanText],
  ];
  pr.forEach((r,i)=>{
    const y=2.6+i*1.05; rect(s,9.35,y,3.25,0.82,r[3],0.12,r[4],1);
    txt(s,r[0],9.58,y+0.11,0.45,0.22,{fontSize:10.5,bold:true,color:r[4]});
    txt(s,r[1],10.15,y+0.08,1.35,0.28,{fontSize:14,bold:true,color:r[4]});
    txt(s,r[2],10.15,y+0.4,2.0,0.22,{fontSize:10.5,color:C.gray});
  });
  txt(s,"우선 상태가 종료되면 dashboard로 복귀",9.35,5.82,3.0,0.38,{fontSize:14.5,bold:true});
  addNotes(s,["firmware/stm32/MyApp/service/display_service.c","firmware/stm32/MyApp/common/app_config.h"],"화면의 FALL 우선 상태를 만드는 낙상 판단의 시간 순서를 설명합니다.");
}

// N10
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,10,"낙상은 단일 기울기가 아니라 시간 순서를 가진 복합 조건이다","충격 후보를 먼저 잡고 자세 변화와 보정 각속도를 관찰한 뒤 countdown을 시작합니다.","");
  const seq=[
    ["01","충격","≥ 1.5 g"],["02","관찰","1–3 s"],["03","자세","> 45°"],["04","회전","< 100°/s"],["05","확정","10 s → FALL"],
  ];
  line(s,1.0,4.05,11.25,0,C.navy,2.2);
  seq.forEach((r,i)=>{
    const x=1.15+i*2.65; node(s,x,4.05,0.33,r[0],i===4?C.coralText:C.cyanText,C.white);
    txt(s,r[1],x-0.75,2.75,1.5,0.38,{fontSize:18,bold:true,align:"center"});
    txt(s,r[2],x-0.85,3.25,1.7,0.32,{fontFace:"Roboto Mono",fontSize:14,bold:true,color:i===4?C.coralText:C.gray,align:"center"});
  });
  rect(s,0.9,5.15,11.45,0.82,C.white,0.12,C.pale,1);
  txt(s,"CALIBRATION BASIS",1.18,5.41,1.7,0.24,{fontSize:10.5,bold:true,color:C.cyanText});
  txt(s,"40 samples · 0.85–1.15 g · ≤ 5°/s · gravity vector + gyro offset",3.0,5.28,8.85,0.36,{fontFace:"Roboto Mono",fontSize:14.5,bold:true});
  addNotes(s,["firmware/stm32/MyApp/service/safety_detector.c"],"판정된 STOP이 다섯 software lane을 지나 ACK로 돌아오는 한 번의 transaction을 보여줍니다.");
}

// N11
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,11,"STOP은 priority로 전달되고 peer STM32 수락 뒤 ACK가 돌아온다","전송 성공이 아니라 application acceptance를 확인하며, 미응답 peer만 retry합니다.","");
  const lanes=["STM32 A","ESP A","MESH","ESP B","STM32 B"];
  const xs=[1.1,3.6,6.1,8.6,11.1];
  lanes.forEach((l,i)=>{
    txt(s,l,xs[i]-0.7,1.92,1.4,0.28,{fontSize:12,bold:true,color:i===4?C.coralText:C.gray,align:"center"});
    line(s,xs[i],2.35,0,3.75,i===2?C.cyan:C.pale,i===2?2:1.2,null,i===2?"dash":"solid");
  });
  line(s,1.1,2.8,10.0,1.55,C.coral,2.6,"triangle");
  rect(s,9.95,4.0,2.35,1.05,C.coralPale,0.12,C.coral,1.4);
  txt(s,"ACCEPTANCE GATE",10.15,4.17,1.95,0.22,{fontSize:10.5,bold:true,color:C.coralText,align:"center"});
  txt(s,"received → app accepts\n→ ACK created",10.13,4.47,1.98,0.45,{fontFace:"Roboto Mono",fontSize:11.5,bold:true,align:"center"});
  line(s,11.1,5.05,-10.0,0.82,C.navy,2.6,"triangle");
  rect(s,4.25,2.58,3.35,0.42,C.bg,0.08,C.bg,0);
  txt(s,"STOP_REQUEST · priority",4.25,2.64,3.35,0.28,{fontFace:"Roboto Mono",fontSize:12,bold:true,color:C.coralText,align:"center"});
  rect(s,4.5,5.55,2.85,0.42,C.bg,0.08,C.bg,0);
  txt(s,"STOP_ACK · 미응답 peer만 retry",4.5,5.61,2.85,0.28,{fontFace:"Roboto Mono",fontSize:11.5,bold:true,color:C.navy,align:"center"});
  txt(s,"4 MESSAGE CONTRACT",0.75,6.22,1.75,0.24,{fontSize:10.5,bold:true,color:C.gray});
  txt(s,"STATE_UPDATE · PACE_REQUEST · STOP_REQUEST · STOP_ACK",2.65,6.12,8.5,0.33,{fontFace:"Roboto Mono",fontSize:13.5,bold:true});
  addNotes(s,["firmware/protocol/README.md","firmware/protocol/nostos_protocol.h","firmware/esp32/main/bridge_runtime.c"],"transaction 이후에는 구현 중 발생한 플랫폼과 queue 장애를 어떤 결정으로 해결했는지 회고합니다.");
}

// N12
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,12,"연결보다 queue 정책과 운영 안정성을 먼저 고정했다","플랫폼 전환을 끝으로 보지 않고, 반복 가능한 운영 규칙으로 바꿨습니다.","");
  const cols=[0.7,4.65,8.6];
  ["CONSTRAINT","DECISION","RESULT"].forEach((h,i)=>txt(s,h,cols[i],1.95,3.1,0.28,{fontSize:11,bold:true,color:i===0?C.coralText:(i===1?C.cyanText:C.green)}));
  const rows=[
    ["Pico 검토와 일정 압박","ESP32-S3 + ESP-IDF 확정","Mesh runtime 기반 구성"],
    ["Normal/STOP 경쟁","latest-state와 priority queue 분리","긴급 의미와 최신 상태 보존"],
    ["현장 재구성 부담","provisioning/group 기준 고정","0xC001 group operation"],
  ];
  rows.forEach((r,i)=>{
    const y=2.45+i*1.25;
    rect(s,cols[0],y,3.05,0.82,C.coralPale,0.1,C.coral,1); txt(s,r[0],cols[0]+0.18,y+0.13,2.68,0.5,{fontSize:15.5,bold:true,align:"center"});
    line(s,3.75,y+0.41,0.75,0,C.pale,1.7,"triangle");
    rect(s,cols[1],y,3.05,0.82,C.cyanPale,0.1,C.cyan,1); txt(s,r[1],cols[1]+0.18,y+0.13,2.68,0.5,{fontSize:15.5,bold:true,align:"center"});
    line(s,7.7,y+0.41,0.75,0,C.pale,1.7,"triangle");
    rect(s,cols[2],y,3.75,0.82,C.greenPale,0.1,C.green,1); txt(s,r[2],cols[2]+0.2,y+0.13,3.35,0.5,{fontSize:15.5,bold:true,align:"center"});
  });
  txt(s,"기간 표기는 code evidence가 아니라 팀 회고 기준",0.75,6.15,6.0,0.34,{fontSize:14,bold:true,color:C.gray});
  addNotes(s,["firmware/esp32/sdkconfig.defaults","firmware/esp32/main/mesh_node.c","firmware/esp32/main/bridge_runtime.c"],"소프트웨어 구조뿐 아니라 현장 통합 문제도 measured와 hypothesis를 분리해 기록했습니다.");
}

// N13
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,13,"현장 통합 문제는 원인과 회피책을 분리해 기록했다","측정하지 않은 원인은 확정하지 않고 HYPOTHESIS로 남겼습니다.","");
  const x=[0.65,4.0,7.05,10.05], w=[3.05,2.7,2.65,2.65];
  ["ISSUE","EVIDENCE TYPE","MITIGATION","RESULT"].forEach((h,i)=>txt(s,h,x[i],1.92,w[i],0.28,{fontSize:11,bold:true,color:i===1?C.coralText:C.gray}));
  const rows=[
    ["접점 튐 · DREQ timing","CODE / HYPOTHESIS","30 ms debounce\ntimeout · SCI read-back","중복 입력·stall 경로 축소"],
    ["이동 중 전원·배선","HYPOTHESIS","전원 분리\n케이블 고정","재현 조건 관리"],
    ["USB hub 동시 Flash","TEAM OBSERVED","한 대씩 순차 Flash\nPort/Serial 지정","대상 혼선 회피"],
  ];
  rows.forEach((r,ri)=>{
    const y=2.38+ri*1.23;
    line(s,0.65,y+0.94,12.05,0,C.pale,1);
    r.forEach((v,i)=>{
      const col=i===1?(v.includes("HYPOTHESIS")?C.coralText:C.cyanText):C.navy;
      txt(s,v,x[i]+0.08,y+0.1,w[i]-0.16,0.68,{fontSize:i===1?13.2:14.5,bold:i===0||i===1,color:col,valign:"top"});
    });
  });
  rect(s,0.75,6.15,11.8,0.46,C.amberPale,0.1,C.amberPale,0);
  txt(s,"원인 확정은 측정 로그가 있을 때만 — 지금은 대응 가능한 경계만 명확히 표시",1.05,6.23,11.2,0.25,{fontSize:14,bold:true,color:C.amber,align:"center"});
  addNotes(s,["firmware/stm32/MyApp/hw/button.c","firmware/stm32/MyApp/hw/vs1003b.c","Team retrospective"],"문제 해결을 넘어 사용자가 실제로 받는 멀티모달 가치로 전환합니다.");
}

// N14
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,14,"raw sensor data를 라이더가 바로 행동할 수 있는 신호로 바꾼다","한 메시지를 상황에 맞는 시각·청각 출력으로 분해해 화면 의존을 낮춥니다.","");
  rect(s,0.7,2.2,3.15,3.65,C.navy,0.16,C.navy,0);
  txt(s,"RIDER INTENT",1.05,2.55,2.45,0.28,{fontSize:11,bold:true,color:C.cyan,align:"center"});
  txt(s,"PACE UP\nPACE DOWN\nSTOP",1.05,3.15,2.45,1.35,{fontFace:"Roboto Mono",fontSize:24,bold:true,color:C.white,align:"center"});
  txt(s,"button · sensor · fall",1.05,5.05,2.45,0.28,{fontSize:13,color:"B8C8D4",align:"center"});
  line(s,3.85,4.03,1.1,0,C.cyanText,2.2,"triangle");
  const outs=[
    ["OLED","수치 · 발신 node · 요청",5.2,2.05,C.cyanPale,C.cyanText],
    ["RGB","Green · Yellow · Red",8.9,2.05,C.white,C.navy],
    ["AUDIO","message → asset → DREQ",5.2,4.35,C.white,C.navy],
    ["BUZZER","FALL priority pattern",8.9,4.35,C.coralPale,C.coralText],
  ];
  outs.forEach(r=>{rect(s,r[2],r[3],3.05,1.55,r[4],0.14,r[5],1.2);txt(s,r[0],r[2]+0.25,r[3]+0.25,2.55,0.3,{fontSize:17,bold:true,color:r[5]});txt(s,r[1],r[2]+0.25,r[3]+0.74,2.55,0.45,{fontSize:14.5,color:C.gray});});
  addNotes(s,["firmware/stm32/MyApp/service/display_service.c","firmware/stm32/MyApp/hw/rgb_led.c","firmware/stm32/MyApp/service/audio_service.c","firmware/stm32/MyApp/hw/buzzer.c"],"센서 차별성은 고정 축이 아니라 실제 장착 자세를 기준으로 삼는 calibration에 있습니다.");
}

// N15
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,15,"장착 자세 calibration이 고정 축 임계값을 대체한다","실제 자전거 장착 상태를 정상 기준으로 저장해 상대 변화로 해석합니다.","");
  txt(s,"BEFORE",0.75,1.98,1.0,0.28,{fontSize:11,bold:true,color:C.coralText});
  rect(s,0.75,2.4,4.8,3.55,C.coralPale,0.16,C.coral,1.2);
  txt(s,"고정 축 threshold",1.1,2.76,3.9,0.42,{fontSize:22,bold:true});
  txt(s,"장착 각도 변화가 곧 오차\n한 축의 순간값에 민감",1.1,3.55,3.7,0.95,{fontSize:17,color:C.gray,valign:"top"});
  line(s,2.05,5.15,1.75,-1.35,C.coralText,2.4,"triangle");
  line(s,2.05,5.15,1.75,0,C.coralText,2.4,"triangle");
  txt(s,"AFTER",7.0,1.98,1.0,0.28,{fontSize:11,bold:true,color:C.cyanText});
  rect(s,7.0,2.4,5.55,3.55,C.cyanPale,0.16,C.cyan,1.2);
  txt(s,"장착 기준 상대 vector",7.35,2.76,4.7,0.42,{fontSize:22,bold:true});
  txt(s,"40 stable samples\ngravity direction normalization\ngyro offset 저장",7.35,3.45,3.05,1.25,{fontFace:"Roboto Mono",fontSize:15.5,bold:true,color:C.gray,valign:"top"});
  node(s,11.05,4.65,0.48,"G",C.cyanText,C.white);
  line(s,11.05,4.65,0,-1.15,C.cyanText,2.3,"triangle");
  line(s,11.05,4.65,0.9,0.5,C.navy,2.3,"triangle");
  addNotes(s,["firmware/stm32/MyApp/service/safety_detector.c"],"다음 장에서는 동일한 의미 보존을 통신 protocol 관점에서 비교합니다.");
}

// N16
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,16,"send-success보다 application acceptance가 더 강한 의미를 준다","latest state · priority STOP · targeted retry가 제한된 MCU 자원에서 message 의미를 보존합니다.","");
  const x=[0.65,4.2,8.15,11.1],w=[3.25,3.6,2.65,1.55];
  ["SEMANTIC","BASIC TRANSPORT","NOSTOS POLICY","WHY IT MATTERS"].forEach((h,i)=>txt(s,h,x[i],1.92,w[i],0.28,{fontSize:10.5,bold:true,color:i===2?C.cyanText:C.gray}));
  const rows=[
    ["STATE_UPDATE","FIFO accumulation","topic latest-state","stale data 제거"],
    ["STOP_REQUEST","normal send queue","priority queue","긴급 의미 선점"],
    ["STOP_ACK","API send success","peer STM32 accepts","application 경계 확인"],
    ["RETRY","broadcast repeat","unmatched peer only","불필요 재전송 축소"],
  ];
  rows.forEach((r,ri)=>{
    const y=2.37+ri*0.93;
    rect(s,0.65,y,12.0,0.72,ri===2?C.coralPale:(ri%2?C.bg:C.white),0.08,ri===2?C.coral:C.pale,ri===2?1.2:0.5);
    r.forEach((v,i)=>txt(s,v,x[i]+0.12,y+0.12,w[i]-0.24,0.45,{fontFace:i<3?"Roboto Mono":"Arial",fontSize:i===0?13.5:(i===3?13:12.5),bold:i===0||i===2,color:i===2?(ri===2?C.coralText:C.cyanText):C.navy}));
  });
  txt(s,"ACK는 물리 출력 완료나 safety certification을 뜻하지 않는다",0.75,6.32,8.5,0.32,{fontSize:14.5,bold:true,color:C.coralText});
  addNotes(s,["firmware/protocol/README.md","firmware/protocol/nostos_protocol.h","firmware/esp32/main/bridge_runtime.c"],"기술적 차별성이 사용자에게 주는 즉시 가치와 적용 가능성을 구분해 설명합니다.");
}

// N17
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,17,"화면 조작 없이 팀 상태를 공유하는 구조는 안전 보조로 확장 가능하다","현재 기능의 즉시 가치와 아직 검증이 필요한 기대 효과를 분리합니다.","");
  rect(s,0.7,2.0,5.25,4.25,C.navy,0.16,C.navy,0);
  txt(s,"IMMEDIATE VALUE",1.05,2.35,2.2,0.28,{fontSize:11,bold:true,color:C.cyan});
  txt(s,"주행 중\n전달 공백 완화",1.05,2.95,3.95,1.1,{fontSize:25,bold:true,color:C.white,valign:"top"});
  txt(s,"버튼을 누르기 어려운 순간에도\n로컬 낙상 판단이 STOP 경로를 시작",1.05,4.55,4.15,0.9,{fontSize:17,color:"D8E4EC",valign:"top"});
  txt(s,"EXPECTED APPLICATION",6.55,2.0,2.3,0.28,{fontSize:11,bold:true,color:C.gray});
  const apps=[
    ["BICYCLE","그룹 라이딩 신호 공유",C.cyanPale,C.cyanText],
    ["PM","소형 모빌리티 안전 보조",C.white,C.navy],
    ["CARE LINK","검증 후 위치·보호자 연락",C.coralPale,C.coralText],
  ];
  apps.forEach((r,i)=>{const y=2.48+i*1.1;rect(s,6.55,y,5.75,0.84,r[2],0.12,r[3],1);txt(s,r[0],6.82,y+0.14,1.45,0.26,{fontSize:12,bold:true,color:r[3]});txt(s,r[1],8.4,y+0.12,3.55,0.5,{fontSize:16,bold:true});});
  txt(s,"시장 규모·판매 수치는 이번 범위에서 사용하지 않음",6.55,5.95,5.45,0.34,{fontSize:14,bold:true,color:C.gray});
  addNotes(s,["docs/study/NOSTOS_QA.md","firmware/protocol/README.md"],"확장 약속 대신 다음에 통과해야 할 evidence gate를 roadmap으로 제시합니다.");
}

// N18
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,18,"확장은 완료 약속이 아니라 evidence gate로 관리한다","physical E2E부터 소형화까지, 앞 단계가 검증되어야 다음 단계가 열립니다.","");
  const gates=[
    ["01","PHYSICAL E2E","3-node STOP path",C.coralPale,C.coralText],
    ["02","RELIABILITY","loss · latency · false positive",C.amberPale,C.amber],
    ["03","USER VALIDATION","ride scenario feedback",C.cyanPale,C.cyanText],
    ["04","MINIATURIZE","housing · battery · mounting",C.greenPale,C.green],
  ];
  gates.forEach((r,i)=>{
    const x=0.72+i*3.12;
    rect(s,x,2.35,2.65,2.55,r[3],0.15,r[4],1.3);
    txt(s,r[0],x+0.25,2.65,0.45,0.28,{fontSize:12,bold:true,color:r[4]});
    txt(s,r[1],x+0.25,3.2,2.15,0.5,{fontFace:"Roboto Mono",fontSize:15.5,bold:true,color:r[4],align:"center"});
    txt(s,r[2],x+0.25,4.05,2.15,0.42,{fontSize:13.5,color:C.gray,align:"center"});
    if(i<3) line(s,x+2.65,3.62,0.47,0,C.pale,1.8,"triangle");
  });
  badge(s,"CURRENT GATE",0.78,5.55,1.55,C.coralPale,C.coralText);
  txt(s,"3-node physical STOP E2E는 아직 PENDING",2.6,5.49,6.0,0.42,{fontSize:18,bold:true});
  addNotes(s,["TODOS.md","evidence taxonomy in this deck"],"로드맵 이후에는 실제 개발 순서를 위험 기술 중심 timeline으로 정리합니다.");
}

// N19
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,19,"위험 기술을 먼저 검증하고 platform → 기능 → 통합 → 검증 순으로 진행했다","정확한 날짜 기록 대신 실제 작업 단계와 주요 의사결정을 기준으로 정리했습니다.","");
  const steps=[
    ["01","기획","요구·역할"],["02","플랫폼","ESP32 결정"],["03","기능","STM32·Mesh"],["04","통합","UART·Protocol"],["05","검증","Host·Demo"],
  ];
  line(s,1.0,4.02,11.2,0,C.navy,2.4);
  steps.forEach((r,i)=>{
    const x=1.12+i*2.72;
    node(s,x,4.02,0.38,r[0],i===1?C.coralText:C.cyanText,C.white);
    txt(s,r[1],x-0.8,2.65,1.6,0.4,{fontSize:20,bold:true,align:"center"});
    txt(s,r[2],x-0.9,3.18,1.8,0.3,{fontSize:14.5,color:C.gray,align:"center"});
    if(i<4) txt(s,"→",x+0.9,3.82,0.55,0.35,{fontSize:18,bold:true,color:C.pale,align:"center"});
  });
  rect(s,1.05,5.25,11.15,0.85,C.navy,0.12,C.navy,0);
  txt(s,"운영 원칙",1.35,5.52,1.2,0.24,{fontSize:11,bold:true,color:C.cyan});
  txt(s,"핵심 통신 조기 검증  ·  기능별 구현 후 공통 protocol 통합  ·  Demo 전 power/flash 점검",2.8,5.38,8.85,0.38,{fontSize:15.5,bold:true,color:C.white,align:"center"});
  addNotes(s,["Team retrospective","STRUCTURE.md","firmware/README.md"],"마지막으로 역할별 산출물이 어떻게 하나의 shared safety state로 결합됐는지 닫습니다.");
}

// N20
{
  const s=pptx.addSlide("NOSTOS_MASTER");
  chrome(s,20,"역할별 산출물이 하나의 shared safety state로 결합됐다","확정된 팀원명이 없는 현재 deck에서는 검증 가능한 역할 범주와 산출물만 표시합니다.","");
  const roles=[
    ["안전 감지","MPU6050 · calibration · fall sequence","STOP candidate 생성"],
    ["사용자 인터페이스","SSD1306 · RGB · Audio · Buzzer","rider action signal"],
    ["무선 통합","UART codec · queue · Bluetooth Mesh","shared group state"],
  ];
  txt(s,"ROLE",0.7,1.96,2.0,0.28,{fontSize:11,bold:true,color:C.gray});
  txt(s,"DELIVERABLE",3.2,1.96,4.5,0.28,{fontSize:11,bold:true,color:C.gray});
  txt(s,"CONTRIBUTION",8.25,1.96,3.6,0.28,{fontSize:11,bold:true,color:C.gray});
  roles.forEach((r,i)=>{
    const y=2.42+i*0.92;
    line(s,0.7,y+0.75,11.6,0,C.pale,1);
    txt(s,r[0],0.78,y+0.12,2.15,0.42,{fontSize:17,bold:true,color:i===2?C.coralText:C.navy});
    txt(s,r[1],3.2,y+0.12,4.4,0.42,{fontFace:"Roboto Mono",fontSize:13.5,bold:true});
    txt(s,r[2],8.25,y+0.12,3.55,0.42,{fontSize:16,bold:true,color:C.cyanText});
  });
  rect(s,0.7,5.55,11.6,0.95,C.navy,0.14,C.navy,0);
  txt(s,"함께 달릴 때, 안전 정보도 함께 움직입니다.",1.05,5.78,7.1,0.38,{fontSize:20,bold:true,color:C.white});
  txt(s,"SOURCE →",9.05,5.86,1.1,0.25,{fontSize:10.5,bold:true,color:C.cyan,hyperlink:{url:"https://github.com/snowflake-rider/nostos"}});
  txt(s,"DEMO →",10.45,5.86,1.05,0.25,{fontSize:10.5,bold:true,color:C.coral,hyperlink:{url:"https://www.youtube.com/watch?v=r-7fjGOm1hw&feature=youtu.be"}});
  addNotes(s,["DEVICES.md","STRUCTURE.md","existing deck role categories"],"질의응답에서는 N07 architecture와 N09 dashboard를 anchor로 되돌아갑니다.");
}

for (const slide of pptx._slides) {
  slide.addShape(pptx.ShapeType.rect, { x: 0, y: 0, w: W, h: H, fill: { color: C.bg, transparency: 100 }, line: { color: C.bg, transparency: 100 }, transparency: 100 });
}

const output = process.argv[2] || "docs/presentations/NOSTOS_개발완료보고서_최종_표지포함20p.pptx";
await pptx.writeFile({ fileName: output });
console.log(output);
