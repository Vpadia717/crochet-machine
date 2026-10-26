// Automatic chain-stitch crochet machine - ESP32
// Libraries: AccelStepper, ESP32Servo (install via Library Manager)
// Board: ESP32 Dev Module

#include <WiFi.h>
#include <WebServer.h>
#include <AccelStepper.h>
#include <ESP32Servo.h>

const char* WIFI_SSID = "YOUR_WIFI";
const char* WIFI_PASS = "YOUR_PASSWORD";

#define STEP_PIN  26
#define DIR_PIN   27
#define EN_PIN    25
#define HOOK_PIN  18
#define ARM_PIN   19
#define LIMIT_PIN 33

// ---- Calibration values (tune on your machine) ----
const long Z_DOWN = 600;             // steps from home to loop depth (full-step mode)
const int  HOOK_OPEN = 90;           // servo angle, hook open
const int  HOOK_LATCH = 0;           // servo angle, hook latched on yarn
const int  ARM_PARK = 0;             // yarn arm parked
const int  ARM_WRAP_END = 180;       // yarn arm end of sweep
const unsigned long HOME_TIMEOUT_MS = 20000;

AccelStepper z(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
Servo hook, arm;
WebServer server(80);

int target = 0, done = 0;
bool running = false;
bool homed = false;

bool homeZ() {
  digitalWrite(EN_PIN, LOW);
  z.setSpeed(-400);
  unsigned long t0 = millis();
  while (digitalRead(LIMIT_PIN) == HIGH) {
    z.runSpeed();
    if (millis() - t0 > HOME_TIMEOUT_MS) {
      digitalWrite(EN_PIN, HIGH);
      return false;                  // switch never triggered
    }
  }
  z.setCurrentPosition(0);
  return true;
}

void moveZ(long pos) {
  z.moveTo(pos);
  int n = 0;
  while (z.distanceToGo() != 0) {
    z.run();
    if (++n % 200 == 0) server.handleClient();   // keep web server responsive
  }
}

void stitch() {
  hook.write(HOOK_OPEN);
  arm.write(ARM_PARK);
  moveZ(Z_DOWN);                                  // hook into loop
  for (int a = ARM_PARK; a <= ARM_WRAP_END; a += 5) {
    arm.write(a);                                 // wrap yarn
    delay(8);
  }
  hook.write(HOOK_LATCH);                         // latch yarn
  delay(250);
  moveZ(0);                                       // pull through loop
  hook.write(HOOK_OPEN);                          // release
  delay(150);
  arm.write(ARM_PARK);
}

void handleStart() {
  if (!homed) { server.send(409, "text/plain", "Not homed. Check limit switch, then open /home"); return; }
  target = server.hasArg("n") ? server.arg("n").toInt() : 10;
  done = 0;
  digitalWrite(EN_PIN, LOW);
  running = true;
  server.send(200, "text/plain", "Started " + String(target) + " stitches");
}

void handleStop() {
  running = false;
  server.send(200, "text/plain", "Stopped");
}

void handleHome() {
  running = false;
  homed = homeZ();
  server.send(200, "text/plain", homed ? "Homed" : "Homing failed");
}

void handleStatus() {
  server.send(200, "application/json",
    String("{\"running\":") + (running ? "true" : "false") +
    ",\"homed\":" + (homed ? "true" : "false") +
    ",\"done\":" + String(done) +
    ",\"target\":" + String(target) + "}");
}

void handleRoot() {
  server.send(200, "text/html",
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<h2>Crochet Machine</h2>"
    "<input id=n type=number value=20> "
    "<button onclick=\"fetch('/start?n='+n.value)\">Start</button> "
    "<button onclick=\"fetch('/stop')\">Stop</button> "
    "<button onclick=\"fetch('/home')\">Home</button>"
    "<p id=s></p><script>setInterval(async()=>{let r=await (await fetch('/status')).json();"
    "s.innerText=r.done+' / '+r.target+(r.running?' (running)':'')+(r.homed?'':' - not homed');},1000)</script>");
}

void setup() {
  Serial.begin(115200);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);                     // driver disabled until needed
  pinMode(LIMIT_PIN, INPUT_PULLUP);

  z.setMaxSpeed(1200);
  z.setAcceleration(800);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  hook.setPeriodHertz(50);
  arm.setPeriodHertz(50);
  hook.attach(HOOK_PIN, 500, 2400);
  arm.attach(ARM_PIN, 500, 2400);
  hook.write(HOOK_OPEN);
  arm.write(ARM_PARK);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println();
  Serial.print("Open http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/home", handleHome);
  server.on("/status", handleStatus);
  server.begin();

  homed = homeZ();
  Serial.println(homed ? "Homed" : "Homing failed - check limit switch");
}

void loop() {
  server.handleClient();
  if (running && done < target) {
    stitch();
    done++;
  } else if (running) {
    running = false;
    digitalWrite(EN_PIN, HIGH);
  }
}
