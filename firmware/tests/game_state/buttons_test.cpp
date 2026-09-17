#include "../../smart_desk_esp32/src/hardware/buttons.h"
#include "../../smart_desk_esp32/src/core/app_config.h"
#include <cassert>
#include <cstdio>
namespace {
using namespace AppConfig;
ButtonEvent tick(unsigned long elapsed, bool allow=true) { hostMillis+=elapsed; return Buttons::readButtonEvent(allow); }
ButtonEvent edge(int pin, bool value, bool allow=true) {
  hostPins[pin]=value;
  assert(tick(1,allow)==BUTTON_NONE);
  return tick(BUTTON_DEBOUNCE_MS+1,allow);
}
void reset() { hostMillis=100; Buttons::init(); }
}
void buttonsTests() {
  for (int pin : {BUTTON_LEFT_PIN,BUTTON_RIGHT_PIN}) {
    const auto shortEvent=pin==BUTTON_LEFT_PIN ? BUTTON_LEFT : BUTTON_RIGHT;
    const auto longEvent=pin==BUTTON_LEFT_PIN ? BUTTON_LEFT_LONG : BUTTON_RIGHT_LONG;
    reset(); assert(edge(pin,LOW)==BUTTON_NONE); assert(edge(pin,HIGH)==shortEvent);
    reset(); assert(edge(pin,LOW)==BUTTON_NONE);
    assert(tick(BUTTON_LONG_HOLD_MS-1)==BUTTON_NONE); assert(tick(1)==longEvent);
    assert(tick(1000)==BUTTON_NONE); assert(edge(pin,HIGH)==BUTTON_NONE);
    reset(); assert(edge(pin,LOW,false)==BUTTON_NONE);
    assert(tick(2000,false)==BUTTON_NONE); assert(edge(pin,HIGH,false)==shortEvent);
    reset(); assert(edge(pin,LOW)==BUTTON_NONE); assert(tick(740)==BUTTON_NONE);
    hostPins[pin]=HIGH; assert(tick(10)==BUTTON_NONE); // Release debounce must not emit long.
    assert(tick(BUTTON_DEBOUNCE_MS+1)==shortEvent);
  }
  reset(); assert(edge(BUTTON_OK_PIN,LOW)==BUTTON_OK); assert(tick(2000)==BUTTON_NONE);
  assert(edge(BUTTON_OK_PIN,HIGH)==BUTTON_NONE);
  for (bool releaseEarly : {false,true}) {
    reset(); hostPins[BUTTON_LEFT_PIN]=hostPins[BUTTON_RIGHT_PIN]=LOW;
    assert(tick(1)==BUTTON_NONE); assert(tick(31)==BUTTON_NONE);
    assert(tick(750)==BUTTON_NONE);
    if (!releaseEarly) { assert(tick(250)==BUTTON_MODE_SWITCH); assert(tick(1000)==BUTTON_NONE); }
    assert(edge(BUTTON_LEFT_PIN,HIGH)==BUTTON_NONE);
    assert(tick(1000)==BUTTON_NONE); assert(edge(BUTTON_RIGHT_PIN,HIGH)==(releaseEarly ? BUTTON_BACK : BUTTON_NONE));
    assert(tick(1000)==BUTTON_NONE);
    assert(edge(BUTTON_LEFT_PIN,LOW)==BUTTON_NONE); assert(edge(BUTTON_LEFT_PIN,HIGH)==BUTTON_LEFT);
  }
  for (bool allow : {false,true}) {
    for (bool together : {false,true}) {
      reset(); assert(edge(BUTTON_LEFT_PIN,LOW,allow)==BUTTON_NONE);
      assert(edge(BUTTON_RIGHT_PIN,LOW,allow)==BUTTON_NONE);
      assert(tick(100,allow)==BUTTON_NONE);
      if (together) {
        hostPins[BUTTON_LEFT_PIN]=hostPins[BUTTON_RIGHT_PIN]=HIGH;
        assert(tick(1,allow)==BUTTON_NONE); assert(tick(31,allow)==BUTTON_BACK);
      } else {
        assert(edge(BUTTON_RIGHT_PIN,HIGH,allow)==BUTTON_NONE);
        assert(edge(BUTTON_LEFT_PIN,HIGH,allow)==BUTTON_BACK);
      }
      assert(tick(1000,allow)==BUTTON_NONE);
    }
  }
  // Opposite raw press arrives just before long threshold but isn't debounced yet.
  reset(); assert(edge(BUTTON_LEFT_PIN,LOW)==BUTTON_NONE); assert(tick(740)==BUTTON_NONE);
  hostPins[BUTTON_RIGHT_PIN]=LOW; assert(tick(10)==BUTTON_NONE); assert(tick(31)==BUTTON_NONE);
  assert(tick(1000)==BUTTON_MODE_SWITCH);
  assert(edge(BUTTON_RIGHT_PIN,HIGH)==BUTTON_NONE); assert(edge(BUTTON_LEFT_PIN,HIGH)==BUTTON_NONE);
  // millis rollover uses unsigned elapsed arithmetic.
  reset(); hostMillis=~0UL-100; assert(edge(BUTTON_RIGHT_PIN,LOW)==BUTTON_NONE);
  assert(tick(750)==BUTTON_RIGHT_LONG); assert(edge(BUTTON_RIGHT_PIN,HIGH)==BUTTON_NONE);
  std::puts("PASS D buttons: original shorts/OK/legacy holds, single longs, consumed releases, chord priority/debounce/rollover");
}
