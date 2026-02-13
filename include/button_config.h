#pragma once

// =============================================================================
// Button-to-REST Configuration (compile-time)
// =============================================================================
//
// Compile-time fallback values for response parsing.  The actual backend
// endpoint is configured on the server via the button config UI — the device
// always POSTs to /talkie with an X-Button header and the server forwards
// to the right backend.
//
// =============================================================================

// --- Button "A" (push-to-talk) ------------------------------------------------
#define BUTTON_A_RESPONSE_KEY "text"
#define BUTTON_A_CONTENT_TYPE "audio/wav"
