// Autoblow cloud API (developers.autoblow.com/reference/): VacuGlide, VacuPump
// and Autoblow Ultra. The token decides which one; GET /autoblow/connected says.
//
// The UI sets the *desired* device state; a network task on core 0 sends
// whatever differs from what it last sent, one request at a time, and retries
// until it lands. The UI never waits on the network.
#pragma once
#include <stdint.h>

namespace autoblow {

enum class Want : uint8_t { Idle, Speed, Script };

enum class Type : uint8_t { Unknown, VacuGlide, VacuPump, Ultra };

// VacuPump: speed 100 = this much vacuum (kPa below ambient; the pump goes to 60).
static const int VACUPUMP_MAX_KPA = 40;

// Valves, pulses and scripts are VacuGlide-only; the others just take the speed
// (VacuPump: vacuum depth, Ultra: stroke speed).
struct Desired {
  Want want = Want::Idle;
  int speed = 0;        // 1..100, used when want == Speed
  int script = 0;       // 0..29, used when want == Script
  bool scriptPlaying = true;
  bool valvePlus = false;   // stroke-plus = Tighter (air out, more suction)
  bool valveMinus = false;  // stroke-minus = Looser (air in)
  // One-shot vacuum pulse: every new pulseSeq opens the stroke-plus (Tighter) valve for
  // pulseMs, then closes it. Timed by the network task, right where the
  // requests go out, so the valve really is open for pulseMs. Only while
  // want == Speed and neither valve is held; a pulse that fails is skipped.
  uint32_t pulseSeq = 0;
  uint16_t pulseMs = 0;
};

struct Status {
  Type type = Type::Unknown;  // from the last discovery, kept while offline
  bool moving = false;        // the device reports a running mode
  bool wifi = false;
  bool online = false;  // device reachable through the cloud
  char mode[24] = "";   // operationalMode from the device
  int targetSpeed = 0;  // VacuPump: target vacuum as % of VACUPUMP_MAX_KPA
  bool valvePlus = false, valveMinus = false;
  int localScript = 0;
  char error[64] = "";
  uint32_t lost = 0;    // how often the device dropped off (502, or unreachable for graceMs)
  // No answer from the cloud for this long (0 while answers come in). Past a few
  // seconds the UI says "reconnecting"; at graceMs the device counts as lost.
  uint32_t silentMs = 0;
  uint32_t graceMs = 0;
};

void begin();

Desired desired();
void setDesired(const Desired &d);
// Forget what was sent, so the next pass resends everything (used by STOP).
void resync();
// After a loss the task only sends STOP until the UI has seen Status::lost
// change and acknowledged it (so a stale speed can't be resent first).
void ackLost();

Status status();
const char *typeName(Type t);  // "VacuGlide", "VacuPump", "Autoblow Ultra", "Autoblow"

}  // namespace autoblow
