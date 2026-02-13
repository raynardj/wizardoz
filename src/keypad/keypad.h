#pragma once

// =============================================================================
// 4x4 Matrix Keypad — row-column scan
// =============================================================================
//
// Layout per docs/devices/keypad-4x4-matrix.json:
//   Row 1: 1, 2, 3, A
//   Row 2: 4, 5, 6, B
//   Row 3: 7, 8, 9, C
//   Row 4: *, 0, #, D
//
// =============================================================================

/// Initialise keypad GPIOs (rows as outputs, cols as inputs with pull-up).
void keypadInit();

/// Scan the matrix; call every loop iteration.
/// Updates internal state for getKey() and isKeyPressed().
void keypadScan();

/// Return the currently pressed key, or '\0' if none.
/// Debounced; only returns a key after it has been held for DEBOUNCE_MS.
char keypadGetKey();

/// Return true if the given key is currently pressed (debounced).
bool keypadIsKeyPressed(char key);
