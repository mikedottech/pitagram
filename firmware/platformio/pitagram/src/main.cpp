// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Miguel Angel Exposito Sanchez

#include <Arduino.h>
#include "Pitagram.h"
#include "PowerMgr.h"

#include <avr/wdt.h>

void setup() {
  g_power.init();
  g_pitagram.init();
}

void loop() {
  g_power.update();
  g_pitagram.update();
}
