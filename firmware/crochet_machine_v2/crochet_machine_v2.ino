// Crochet machine firmware v2 - pattern upload over Wi-Fi (ESP32)
// Libraries: AccelStepper, ESP32Servo. Board: ESP32 Dev Module.
// Runs CH (chain) and PAUSE steps. Other stitch types (SC, HDC, DC, SLST, INC, MR, ROUND, TURN, JOIN)
// are accepted but skipped until row/round hardware is added.

#include <WiFi.h>
#include <WebServer.h>
#include <AccelStepper.h>
#include <ESP32Servo.h>
#include <vector>

const char* WIFI_SSID = "YOUR_WIFI";
const char* WIFI_PASS = "YOUR_PASSWORD";

#define STEP_PIN 26
#define DIR_PIN 27
#define EN_PIN 25
#define HOOK_PIN 18
#define ARM_PIN 19
#define LIMIT_PIN 33

const long Z_DOWN = 600;
const int HOOK_OPEN = 90, HOOK_LATCH = 0, ARM_PARK = 0, ARM_WRAP_END = 180;
const unsigned long HOME_TIMEOUT_MS = 20000;

enum Op { OP_CH, OP_PAUSE, OP_SKIP };
struct Step { Op op; int n; String label; };
enum State { IDLE, RUNNING, PAUSED, FINISHED };

AccelStepper z(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
Servo hook, arm;
WebServer server(80);

std::vector<Step> prog;
State state = IDLE;
int stepIdx = 0, stitchesDone = 0;
bool homed = false;
String msg = "Ready";

void reply(int code, const String& type, const String& body) {
  server.sendHeader("Access-Control-Allow-Origin", "*");   // lets the controller page talk to us
  server.send(code, type, body);
}

bool homeZ() {
  digitalWrite(EN_PIN, LOW);
  z.setSpeed(-400);
  unsigned long t0 = millis();
  while (digitalRead(LIMIT_PIN) == HIGH) {
    z.runSpeed();
    if (millis() - t0 > HOME_TIMEOUT_MS) { digitalWrite(EN_PIN, HIGH); return false; }
  }
  z.setCurrentPosition(0);
  return true;
}

void moveZ(long pos) {
  z.moveTo(pos);
  int n = 0;
  while (z.distanceToGo() != 0) {
    z.run();
    if (++n % 200 == 0) server.handleClient();
  }
}

void stitch() {
  hook.write(HOOK_OPEN); arm.write(ARM_PARK);
  moveZ(Z_DOWN);
  for (int a = ARM_PARK; a <= ARM_WRAP_END; a += 5) { arm.write(a); delay(8); }
  hook.write(HOOK_LATCH); delay(250);
  moveZ(0);
  hook.write(HOOK_OPEN); delay(150);
  arm.write(ARM_PARK);
}

void handleProgram() {
  if (state == RUNNING) { reply(409, "text/plain", "Stop the machine before loading a pattern"); return; }
  String body = server.arg("plain");
  prog.clear();
  int skipped = 0, pos = 0;
  while (pos < (int)body.length()) {
    int nl = body.indexOf('\n', pos);
    if (nl < 0) nl = body.length();
    String line = body.substring(pos, nl);
    pos = nl + 1;
    line.trim();
    if (line.length() == 0) continue;
    int sp = line.indexOf(' ');
    String op = (sp < 0) ? line : line.substring(0, sp);
    String arg = (sp < 0) ? "" : line.substring(sp + 1);
    op.toUpperCase();
    Step s; s.n = arg.toInt(); s.label = line;
    if (op == "CH" && s.n > 0) s.op = OP_CH;
    else if (op == "PAUSE") { s.op = OP_PAUSE; s.label = arg.length() ? arg : "Paused"; }
    else { s.op = OP_SKIP; skipped++; }
    if (prog.size() < 200) prog.push_back(s);
  }
  stepIdx = 0; stitchesDone = 0; state = IDLE; msg = "Pattern loaded";
  reply(200, "text/plain", "Loaded " + String(prog.size()) + " steps, " + String(skipped) + " skipped (need hardware)");
}

void handleStart() {
  if (!homed) { reply(409, "text/plain", "Not homed. Check the limit switch, then press Home"); return; }
  if (server.hasArg("n")) {                      // quick mode: n chain stitches
    prog.clear();
    Step s; s.op = OP_CH; s.n = server.arg("n").toInt(); s.label = "CH " + String(s.n);
    prog.push_back(s);
  }
  if (prog.empty()) { reply(409, "text/plain", "No pattern loaded"); return; }
  stepIdx = 0; stitchesDone = 0; digitalWrite(EN_PIN, LOW);
  state = RUNNING; msg = "Running";
  reply(200, "text/plain", "Started");
}

void handleStop() { state = IDLE; msg = "Stopped"; reply(200, "text/plain", "Stopped"); }
void handleResume() { if (state == PAUSED) { state = RUNNING; stepIdx++; stitchesDone = 0; msg = "Running"; } reply(200, "text/plain", "OK"); }
void handleHome() { state = IDLE; homed = homeZ(); reply(200, "text/plain", homed ? "Homed" : "Homing failed"); }

void handleStatus() {
  String st = state == RUNNING ? "running" : state == PAUSED ? "paused" : state == FINISHED ? "finished" : "idle";
  String m = msg; m.replace("\"", "'");
  reply(200, "application/json",
    "{\"state\":\"" + st + "\",\"homed\":" + (homed ? "true" : "false") +
    ",\"step\":" + String(stepIdx) + ",\"steps\":" + String(prog.size()) +
    ",\"done\":" + String(stitchesDone) + ",\"msg\":\"" + m + "\"}");
}

void setup() {
  Serial.begin(115200);
  pinMode(EN_PIN, OUTPUT); digitalWrite(EN_PIN, HIGH);
  pinMode(LIMIT_PIN, INPUT_PULLUP);
  z.setMaxSpeed(1200); z.setAcceleration(800);
  ESP32PWM::allocateTimer(0); ESP32PWM::allocateTimer(1);
  hook.setPeriodHertz(50); arm.setPeriodHertz(50);
  hook.attach(HOOK_PIN, 500, 2400); arm.attach(ARM_PIN, 500, 2400);
  hook.write(HOOK_OPEN); arm.write(ARM_PARK);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.print("\nMachine IP: "); Serial.println(WiFi.localIP());

  server.on("/program", HTTP_POST, handleProgram);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/resume", handleResume);
  server.on("/home", handleHome);
  server.on("/status", handleStatus);
  server.begin();
  homed = homeZ();
  Serial.println(homed ? "Homed" : "Homing failed - check limit switch");
}

void loop() {
  server.handleClient();
  if (state != RUNNING) return;
  if (stepIdx >= (int)prog.size()) {
    state = FINISHED; msg = "Pattern complete"; digitalWrite(EN_PIN, HIGH); return;
  }
  Step& s = prog[stepIdx];
  if (s.op == OP_CH) {
    if (stitchesDone < s.n) { stitch(); stitchesDone++; }
    else { stepIdx++; stitchesDone = 0; }
  } else if (s.op == OP_PAUSE) {
    state = PAUSED; msg = s.label;
  } else {
    msg = "Skipped: " + s.label; stepIdx++; stitchesDone = 0;
  }
}
