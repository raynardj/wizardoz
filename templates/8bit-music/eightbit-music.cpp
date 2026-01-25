/**
 * 8-Bit Chiptune Synthesizer for ESP32-S3 + PCM5102A DAC
 *
 * Hardware Connections:
 *   ESP32-S3     PCM5102A
 *   ---------    ---------
 *   GPIO4   -->  BCK  (Bit Clock)
 *   GPIO5   -->  LCK  (Word Select / LRCK)
 *   GPIO6   -->  DIN  (Data Input)
 *   3V3     -->  VIN  (Power)
 *   GND     -->  GND  (Ground)
 *   -           SCK  (Leave floating - derived from BCK via PLL)
 *
 * Features:
 *   - 2x Square wave oscillators with variable duty cycle (NES-style)
 *   - Noise channel for percussion
 *   - Arpeggiator with customizable patterns
 *   - ADSR envelope generator
 *   - Note sequencer with tempo control
 *   - Demo song: Classic chiptune melody
 */

#include <Arduino.h>
#include <driver/i2s.h>

// =============================================================================
// I2S Configuration
// =============================================================================

#define I2S_BCLK_PIN 4 // Bit clock -> PCM5102A BCK
#define I2S_LRCK_PIN 5 // Word select -> PCM5102A LCK
#define I2S_DOUT_PIN 6 // Data out -> PCM5102A DIN

#define SAMPLE_RATE 16000  // CD quality sample rate
#define BITS_PER_SAMPLE 16 // 16-bit audio
#define DMA_BUF_COUNT 8    // Number of DMA buffers
#define DMA_BUF_LEN 256    // Samples per buffer

#define I2S_PORT I2S_NUM_0

// =============================================================================
// Synthesizer Configuration
// =============================================================================

#define MASTER_VOLUME 0.3f // Master volume (0.0 - 1.0)
#define NUM_CHANNELS 3     // 2 pulse + 1 noise

// Duty cycle options for square waves (NES-style)
enum DutyCycle
{
    DUTY_12_5 = 0, // 12.5% - thin, reedy sound
    DUTY_25 = 1,   // 25%   - hollow sound
    DUTY_50 = 2,   // 50%   - full square wave
    DUTY_75 = 3    // 75%   - same as 25% inverted
};

// =============================================================================
// Note Frequency Table (Equal Temperament, A4 = 440Hz)
// =============================================================================

// Note definitions for easy song writing
enum Note
{
    NOTE_REST = 0,
    NOTE_C2 = 65,
    NOTE_CS2 = 69,
    NOTE_D2 = 73,
    NOTE_DS2 = 78,
    NOTE_E2 = 82,
    NOTE_F2 = 87,
    NOTE_FS2 = 93,
    NOTE_G2 = 98,
    NOTE_GS2 = 104,
    NOTE_A2 = 110,
    NOTE_AS2 = 117,
    NOTE_B2 = 123,

    NOTE_C3 = 131,
    NOTE_CS3 = 139,
    NOTE_D3 = 147,
    NOTE_DS3 = 156,
    NOTE_E3 = 165,
    NOTE_F3 = 175,
    NOTE_FS3 = 185,
    NOTE_G3 = 196,
    NOTE_GS3 = 208,
    NOTE_A3 = 220,
    NOTE_AS3 = 233,
    NOTE_B3 = 247,

    NOTE_C4 = 262,
    NOTE_CS4 = 277,
    NOTE_D4 = 294,
    NOTE_DS4 = 311,
    NOTE_E4 = 330,
    NOTE_F4 = 349,
    NOTE_FS4 = 370,
    NOTE_G4 = 392,
    NOTE_GS4 = 415,
    NOTE_A4 = 440,
    NOTE_AS4 = 466,
    NOTE_B4 = 494,

    NOTE_C5 = 523,
    NOTE_CS5 = 554,
    NOTE_D5 = 587,
    NOTE_DS5 = 622,
    NOTE_E5 = 659,
    NOTE_F5 = 698,
    NOTE_FS5 = 740,
    NOTE_G5 = 784,
    NOTE_GS5 = 831,
    NOTE_A5 = 880,
    NOTE_AS5 = 932,
    NOTE_B5 = 988,

    NOTE_C6 = 1047,
    NOTE_CS6 = 1109,
    NOTE_D6 = 1175,
    NOTE_DS6 = 1245,
    NOTE_E6 = 1319,
    NOTE_F6 = 1397
};

// =============================================================================
// ADSR Envelope
// =============================================================================

struct Envelope
{
    float attack;  // Attack time in seconds
    float decay;   // Decay time in seconds
    float sustain; // Sustain level (0.0 - 1.0)
    float release; // Release time in seconds

    float level;   // Current level
    float phase;   // Current phase time
    bool gate;     // Note on/off
    bool released; // In release phase

    void trigger()
    {
        phase = 0;
        level = 0;
        gate = true;
        released = false;
    }

    void releaseNote()
    {
        released = true;
        phase = 0;
    }

    float process(float deltaTime)
    {
        if (!gate && level <= 0.001f)
        {
            level = 0;
            return 0;
        }

        phase += deltaTime;

        if (!released)
        {
            // Attack phase
            if (phase < attack)
            {
                level = phase / attack;
            }
            // Decay phase
            else if (phase < attack + decay)
            {
                float decayProgress = (phase - attack) / decay;
                level = 1.0f - (1.0f - sustain) * decayProgress;
            }
            // Sustain phase
            else
            {
                level = sustain;
            }
        }
        else
        {
            // Release phase
            if (phase < release)
            {
                level = sustain * (1.0f - phase / release);
            }
            else
            {
                level = 0;
                gate = false;
            }
        }

        return level;
    }
};

// =============================================================================
// Oscillator Channel
// =============================================================================

struct Oscillator
{
    float frequency;
    float phase;
    float phaseIncrement;
    DutyCycle dutyCycle;
    Envelope envelope;
    bool isNoise;
    uint16_t lfsr; // Linear feedback shift register for noise

    void setFrequency(float freq)
    {
        frequency = freq;
        phaseIncrement = freq / SAMPLE_RATE;
    }

    void noteOn(float freq)
    {
        setFrequency(freq);
        envelope.trigger();
    }

    void noteOff()
    {
        envelope.releaseNote();
    }

    // Generate square wave with variable duty cycle
    float generateSquare()
    {
        phase += phaseIncrement;
        if (phase >= 1.0f)
            phase -= 1.0f;

        float threshold;
        switch (dutyCycle)
        {
        case DUTY_12_5:
            threshold = 0.125f;
            break;
        case DUTY_25:
            threshold = 0.25f;
            break;
        case DUTY_50:
            threshold = 0.5f;
            break;
        case DUTY_75:
            threshold = 0.75f;
            break;
        default:
            threshold = 0.5f;
            break;
        }

        return (phase < threshold) ? 1.0f : -1.0f;
    }

    // Generate noise using LFSR (NES-style)
    float generateNoise()
    {
        // Update LFSR at note frequency rate
        phase += phaseIncrement;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            // NES noise uses XOR of bits 0 and 1 (or 0 and 6 for metallic)
            uint16_t feedback = ((lfsr >> 0) ^ (lfsr >> 1)) & 1;
            lfsr = (lfsr >> 1) | (feedback << 14);
        }

        return (lfsr & 1) ? 1.0f : -1.0f;
    }

    float generate()
    {
        float sample;
        if (isNoise)
        {
            sample = generateNoise();
        }
        else
        {
            sample = generateSquare();
        }

        float env = envelope.process(1.0f / SAMPLE_RATE);
        return sample * env;
    }
};

// =============================================================================
// Arpeggiator
// =============================================================================

struct Arpeggiator
{
    int notes[8];    // Notes in the arpeggio pattern
    int numNotes;    // Number of notes
    int currentNote; // Current note index
    float speed;     // Notes per second
    float timer;     // Time accumulator
    bool enabled;

    void setup(int *notePattern, int count, float notesPerSecond)
    {
        numNotes = min(count, 8);
        for (int i = 0; i < numNotes; i++)
        {
            notes[i] = notePattern[i];
        }
        speed = notesPerSecond;
        currentNote = 0;
        timer = 0;
        enabled = true;
    }

    int update(float deltaTime)
    {
        if (!enabled || numNotes == 0)
            return notes[0];

        timer += deltaTime;
        float noteTime = 1.0f / speed;

        if (timer >= noteTime)
        {
            timer -= noteTime;
            currentNote = (currentNote + 1) % numNotes;
        }

        return notes[currentNote];
    }

    void disable()
    {
        enabled = false;
        currentNote = 0;
    }
};

// =============================================================================
// Song Sequencer
// =============================================================================

struct NoteEvent
{
    int note;        // Note frequency (0 = rest)
    int duration;    // Duration in ticks (1 tick = 1/16th note)
    DutyCycle duty;  // Duty cycle for this note
    bool arpeggio;   // Use arpeggio for this note
    int arpNotes[4]; // Arpeggio notes (semitone offsets)
    int arpCount;    // Number of arp notes
};

// =============================================================================
// Demo Song: Classic 8-bit Adventure Theme
// =============================================================================

// Lead melody (Channel 1)
const NoteEvent melody[] = {
    // Intro phrase
    {NOTE_E4, 2, DUTY_25, false, {}, 0},
    {NOTE_E4, 2, DUTY_25, false, {}, 0},
    {NOTE_REST, 2, DUTY_25, false, {}, 0},
    {NOTE_E4, 2, DUTY_25, false, {}, 0},
    {NOTE_REST, 2, DUTY_25, false, {}, 0},
    {NOTE_C4, 2, DUTY_25, false, {}, 0},
    {NOTE_E4, 4, DUTY_25, false, {}, 0},
    {NOTE_G4, 4, DUTY_25, false, {}, 0},
    {NOTE_REST, 4, DUTY_25, false, {}, 0},
    {NOTE_G3, 4, DUTY_25, false, {}, 0},
    {NOTE_REST, 4, DUTY_25, false, {}, 0},

    // Main theme
    {NOTE_C4, 3, DUTY_25, false, {}, 0},
    {NOTE_REST, 1, DUTY_25, false, {}, 0},
    {NOTE_G3, 3, DUTY_25, false, {}, 0},
    {NOTE_REST, 1, DUTY_25, false, {}, 0},
    {NOTE_E3, 4, DUTY_25, false, {}, 0},
    {NOTE_A3, 3, DUTY_50, false, {}, 0},
    {NOTE_B3, 3, DUTY_50, false, {}, 0},
    {NOTE_AS3, 2, DUTY_50, false, {}, 0},
    {NOTE_A3, 4, DUTY_50, false, {}, 0},

    // Arpeggio section
    {NOTE_G3, 4, DUTY_25, true, {0, 4, 7, 12}, 4},
    {NOTE_E4, 4, DUTY_25, true, {0, 3, 7, 12}, 4},
    {NOTE_G4, 4, DUTY_25, true, {0, 4, 7, 12}, 4},
    {NOTE_A4, 2, DUTY_25, false, {}, 0},
    {NOTE_F4, 2, DUTY_25, false, {}, 0},
    {NOTE_G4, 2, DUTY_25, false, {}, 0},
    {NOTE_REST, 2, DUTY_25, false, {}, 0},
    {NOTE_E4, 4, DUTY_25, false, {}, 0},
    {NOTE_C4, 2, DUTY_25, false, {}, 0},
    {NOTE_D4, 2, DUTY_25, false, {}, 0},
    {NOTE_B3, 4, DUTY_25, false, {}, 0},

    // End marker
    {0, 0, DUTY_50, false, {}, 0}};

// Bass line (Channel 2)
const NoteEvent bassLine[] = {
    // Intro
    {NOTE_REST, 8, DUTY_50, false, {}, 0},
    {NOTE_REST, 8, DUTY_50, false, {}, 0},
    {NOTE_REST, 8, DUTY_50, false, {}, 0},
    {NOTE_REST, 8, DUTY_50, false, {}, 0},

    // Main bass pattern
    {NOTE_C2, 4, DUTY_50, false, {}, 0},
    {NOTE_G2, 4, DUTY_50, false, {}, 0},
    {NOTE_C3, 4, DUTY_50, false, {}, 0},
    {NOTE_G2, 4, DUTY_50, false, {}, 0},
    {NOTE_C2, 4, DUTY_50, false, {}, 0},
    {NOTE_G2, 4, DUTY_50, false, {}, 0},
    {NOTE_C3, 4, DUTY_50, false, {}, 0},
    {NOTE_G2, 4, DUTY_50, false, {}, 0},

    // Arpeggio section bass
    {NOTE_E2, 4, DUTY_50, false, {}, 0},
    {NOTE_E2, 4, DUTY_50, false, {}, 0},
    {NOTE_A2, 4, DUTY_50, false, {}, 0},
    {NOTE_A2, 4, DUTY_50, false, {}, 0},
    {NOTE_D2, 4, DUTY_50, false, {}, 0},
    {NOTE_D2, 4, DUTY_50, false, {}, 0},
    {NOTE_G2, 8, DUTY_50, false, {}, 0},

    // End marker
    {0, 0, DUTY_50, false, {}, 0}};

// Percussion pattern (Noise channel)
const NoteEvent percussion[] = {
    // Basic beat pattern
    {1200, 2, DUTY_50, false, {}, 0}, // Kick (low noise freq)
    {NOTE_REST, 2, DUTY_50, false, {}, 0},
    {8000, 2, DUTY_50, false, {}, 0}, // Hi-hat (high noise freq)
    {NOTE_REST, 2, DUTY_50, false, {}, 0},
    {4000, 2, DUTY_50, false, {}, 0}, // Snare (mid noise freq)
    {NOTE_REST, 2, DUTY_50, false, {}, 0},
    {8000, 2, DUTY_50, false, {}, 0}, // Hi-hat
    {NOTE_REST, 2, DUTY_50, false, {}, 0},

    // End marker (loop point)
    {0, 0, DUTY_50, false, {}, 0}};

// =============================================================================
// Global Variables
// =============================================================================

Oscillator channels[NUM_CHANNELS];
Arpeggiator arpeggiators[NUM_CHANNELS];

// Sequencer state
int melodyIndex = 0;
int bassIndex = 0;
int percIndex = 0;

float melodyTimer = 0;
float bassTimer = 0;
float percTimer = 0;

const float BPM = 140.0f;
const float TICK_DURATION = 60.0f / BPM / 4.0f; // 1/16th note duration

// I2S write buffer
int16_t i2sBuffer[DMA_BUF_LEN * 2]; // Stereo samples

// =============================================================================
// I2S Initialization
// =============================================================================

void setupI2S()
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0};

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRCK_PIN,
        .data_out_num = I2S_DOUT_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE};

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK)
    {
        Serial.printf("I2S driver install failed: %d\n", err);
        return;
    }

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK)
    {
        Serial.printf("I2S set pin failed: %d\n", err);
        return;
    }

    i2s_zero_dma_buffer(I2S_PORT);

    Serial.println("I2S initialized successfully!");
    Serial.printf("  Sample Rate: %d Hz\n", SAMPLE_RATE);
    Serial.printf("  Bits/Sample: %d\n", BITS_PER_SAMPLE);
    Serial.printf("  BCLK Pin: GPIO%d\n", I2S_BCLK_PIN);
    Serial.printf("  LRCK Pin: GPIO%d\n", I2S_LRCK_PIN);
    Serial.printf("  DOUT Pin: GPIO%d\n", I2S_DOUT_PIN);
}

// =============================================================================
// Synthesizer Initialization
// =============================================================================

void setupSynth()
{
    // Initialize pulse channels
    for (int i = 0; i < 2; i++)
    {
        channels[i].phase = 0;
        channels[i].frequency = 0;
        channels[i].phaseIncrement = 0;
        channels[i].dutyCycle = DUTY_25;
        channels[i].isNoise = false;
        channels[i].lfsr = 0x7FFF;

        // Set up envelope (quick attack, medium decay, sustain, short release)
        channels[i].envelope.attack = 0.01f;
        channels[i].envelope.decay = 0.1f;
        channels[i].envelope.sustain = 0.6f;
        channels[i].envelope.release = 0.15f;
        channels[i].envelope.level = 0;
        channels[i].envelope.phase = 0;
        channels[i].envelope.gate = false;
        channels[i].envelope.released = false;
    }

    // Initialize noise channel
    channels[2].phase = 0;
    channels[2].frequency = 1000;
    channels[2].phaseIncrement = 1000.0f / SAMPLE_RATE;
    channels[2].dutyCycle = DUTY_50;
    channels[2].isNoise = true;
    channels[2].lfsr = 0x7FFF;

    // Short percussive envelope for noise
    channels[2].envelope.attack = 0.001f;
    channels[2].envelope.decay = 0.08f;
    channels[2].envelope.sustain = 0.0f;
    channels[2].envelope.release = 0.05f;
    channels[2].envelope.level = 0;
    channels[2].envelope.phase = 0;
    channels[2].envelope.gate = false;
    channels[2].envelope.released = false;

    // Initialize arpeggiators
    for (int i = 0; i < NUM_CHANNELS; i++)
    {
        arpeggiators[i].enabled = false;
        arpeggiators[i].numNotes = 0;
        arpeggiators[i].currentNote = 0;
        arpeggiators[i].timer = 0;
        arpeggiators[i].speed = 20.0f; // 20 notes per second
    }

    Serial.println("Synthesizer initialized!");
    Serial.printf("  Channels: %d (2 pulse + 1 noise)\n", NUM_CHANNELS);
    Serial.printf("  BPM: %.0f\n", BPM);
}

// =============================================================================
// Note to Frequency Helper
// =============================================================================

float noteToFreq(int note, int semitoneOffset = 0)
{
    if (note <= 0)
        return 0;

    // Calculate semitone offset from note frequency
    float baseFreq = (float)note;

    if (semitoneOffset != 0)
    {
        baseFreq *= powf(2.0f, semitoneOffset / 12.0f);
    }

    return baseFreq;
}

// =============================================================================
// Sequencer Update
// =============================================================================

void updateSequencer(float deltaTime)
{
    // Update melody channel
    melodyTimer -= deltaTime;
    if (melodyTimer <= 0)
    {
        const NoteEvent &event = melody[melodyIndex];

        if (event.duration == 0)
        {
            // Loop back to beginning
            melodyIndex = 0;
        }
        else
        {
            channels[0].dutyCycle = event.duty;

            if (event.note > 0)
            {
                if (event.arpeggio && event.arpCount > 0)
                {
                    // Set up arpeggio
                    int arpNotes[8];
                    for (int i = 0; i < event.arpCount; i++)
                    {
                        arpNotes[i] = noteToFreq(event.note, event.arpNotes[i]);
                    }
                    arpeggiators[0].setup(arpNotes, event.arpCount, 15.0f);
                    channels[0].noteOn(arpNotes[0]);
                }
                else
                {
                    arpeggiators[0].disable();
                    channels[0].noteOn(event.note);
                }
            }
            else
            {
                arpeggiators[0].disable();
                channels[0].noteOff();
            }

            melodyTimer = event.duration * TICK_DURATION;
            melodyIndex++;
        }
    }

    // Update arpeggiator for melody
    if (arpeggiators[0].enabled && channels[0].envelope.gate)
    {
        int newFreq = arpeggiators[0].update(deltaTime);
        channels[0].setFrequency(newFreq);
    }

    // Update bass channel
    bassTimer -= deltaTime;
    if (bassTimer <= 0)
    {
        const NoteEvent &event = bassLine[bassIndex];

        if (event.duration == 0)
        {
            bassIndex = 0;
        }
        else
        {
            channels[1].dutyCycle = event.duty;

            if (event.note > 0)
            {
                channels[1].noteOn(event.note);
            }
            else
            {
                channels[1].noteOff();
            }

            bassTimer = event.duration * TICK_DURATION;
            bassIndex++;
        }
    }

    // Update percussion channel
    percTimer -= deltaTime;
    if (percTimer <= 0)
    {
        const NoteEvent &event = percussion[percIndex];

        if (event.duration == 0)
        {
            percIndex = 0;
        }
        else
        {
            if (event.note > 0)
            {
                channels[2].setFrequency(event.note);
                channels[2].envelope.trigger();
            }

            percTimer = event.duration * TICK_DURATION;
            percIndex++;
        }
    }
}

// =============================================================================
// Audio Generation
// =============================================================================

void generateAudio()
{
    float deltaTime = (float)DMA_BUF_LEN / SAMPLE_RATE;

    // Update sequencer
    updateSequencer(deltaTime);

    // Generate samples
    for (int i = 0; i < DMA_BUF_LEN; i++)
    {
        float sample = 0;

        // Mix all channels
        for (int ch = 0; ch < NUM_CHANNELS; ch++)
        {
            sample += channels[ch].generate();
        }

        // Apply master volume and normalize
        sample = sample / NUM_CHANNELS * MASTER_VOLUME;

        // Soft clipping to prevent harsh distortion
        if (sample > 0.8f)
            sample = 0.8f + (sample - 0.8f) * 0.2f;
        if (sample < -0.8f)
            sample = -0.8f + (sample + 0.8f) * 0.2f;

        // Clamp to valid range
        sample = constrain(sample, -1.0f, 1.0f);

        // Convert to 16-bit signed integer
        int16_t sampleInt = (int16_t)(sample * 32767);

        // Write to buffer (stereo: same sample to both channels)
        i2sBuffer[i * 2] = sampleInt;     // Left
        i2sBuffer[i * 2 + 1] = sampleInt; // Right
    }

    // Write to I2S
    size_t bytesWritten;
    i2s_write(I2S_PORT, i2sBuffer, sizeof(i2sBuffer), &bytesWritten, portMAX_DELAY);
}

// =============================================================================
// Setup and Loop
// =============================================================================

void setup()
{
    Serial.begin(115200);

    // Wait for USB CDC
    unsigned long startTime = millis();
    while (!Serial && (millis() - startTime) < 3000)
    {
        delay(10);
    }

    Serial.println();
    Serial.println("╔════════════════════════════════════════════╗");
    Serial.println("║   8-Bit Chiptune Synthesizer               ║");
    Serial.println("║   ESP32-S3 + PCM5102A DAC                  ║");
    Serial.println("╚════════════════════════════════════════════╝");
    Serial.println();

    // Check system info
    Serial.print("CPU Frequency: ");
    Serial.print(getCpuFrequencyMhz());
    Serial.println(" MHz");

    if (psramFound())
    {
        Serial.print("PSRAM: ");
        Serial.print(ESP.getPsramSize() / (1024 * 1024));
        Serial.println(" MB");
    }

    Serial.println();

    // Initialize I2S and synthesizer
    setupI2S();
    setupSynth();

    Serial.println();
    Serial.println("Starting playback...");
    Serial.println("Connect headphones or speakers to the PCM5102A 3.5mm jack");
    Serial.println();
}

void loop()
{
    // Generate and output audio continuously
    generateAudio();
}
