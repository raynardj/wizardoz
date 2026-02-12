/**
 * Wizardoz Waveform Visualizer
 *
 * Connects to the server WebSocket to receive raw 16-bit PCM audio from an
 * ESP32 and renders a real-time oscilloscope waveform + frequency spectrum
 * on HTML5 Canvas elements.
 */

class WaveVisualizer {
    /**
     * @param {HTMLCanvasElement} waveCanvas  - time-domain waveform canvas
     * @param {HTMLCanvasElement} specCanvas  - frequency spectrum canvas
     * @param {HTMLElement}       levelFill   - level meter fill bar element
     */
    constructor(waveCanvas, specCanvas, levelFill) {
        this.waveCanvas = waveCanvas;
        this.specCanvas = specCanvas;
        this.levelFill  = levelFill;
        this.waveCtx    = waveCanvas.getContext('2d');
        this.specCtx    = specCanvas.getContext('2d');

        this.ws = null;
        this.deviceId = null;
        this.running = false;

        // Audio buffer (ring of recent samples for waveform)
        this.sampleRate = 16000;
        this.bufferSize = 1024;
        this.buffer = new Float32Array(this.bufferSize);
        this.writeIndex = 0;

        // FFT (simple DFT for spectrum — 256 bins)
        this.fftSize = 256;

        // Colours
        this.waveColor   = '#6c63ff';
        this.specColor   = '#3ecf8e';
        this.gridColor   = 'rgba(46, 49, 70, 0.6)';
        this.bgColor     = '#252836';

        this._resizeCanvases();
        window.addEventListener('resize', () => this._resizeCanvases());
    }

    /** Connect to a device's audio WebSocket stream. */
    connect(deviceId) {
        this.disconnect();
        this.deviceId = deviceId;

        const proto = location.protocol === 'https:' ? 'wss' : 'ws';
        const url = `${proto}://${location.host}/ws/visualizer/${encodeURIComponent(deviceId)}`;
        console.log(`[Viz] Connecting to ${url}`);

        this.ws = new WebSocket(url);
        this.ws.binaryType = 'arraybuffer';

        this.ws.onopen = () => {
            console.log('[Viz] WebSocket open');
            this.running = true;
            this._animate();
        };

        this.ws.onmessage = (event) => {
            if (event.data instanceof ArrayBuffer) {
                this._ingestPCM(event.data);
            }
        };

        this.ws.onclose = () => {
            console.log('[Viz] WebSocket closed');
            this.running = false;
        };

        this.ws.onerror = (err) => {
            console.error('[Viz] WebSocket error', err);
        };
    }

    /** Disconnect the WebSocket. */
    disconnect() {
        this.running = false;
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
        this.buffer.fill(0);
        this.writeIndex = 0;
    }

    // -----------------------------------------------------------------------
    // Internal
    // -----------------------------------------------------------------------

    /** Convert raw 16-bit PCM (little-endian) into float buffer. */
    _ingestPCM(arrayBuffer) {
        const samples = new Int16Array(arrayBuffer);
        for (let i = 0; i < samples.length; i++) {
            this.buffer[this.writeIndex] = samples[i] / 32768.0;
            this.writeIndex = (this.writeIndex + 1) % this.bufferSize;
        }
    }

    /** Animation loop. */
    _animate() {
        if (!this.running) return;
        this._drawWaveform();
        this._drawSpectrum();
        this._updateLevel();
        requestAnimationFrame(() => this._animate());
    }

    /** Draw oscilloscope waveform. */
    _drawWaveform() {
        const ctx = this.waveCtx;
        const w = this.waveCanvas.width;
        const h = this.waveCanvas.height;
        const mid = h / 2;

        ctx.fillStyle = this.bgColor;
        ctx.fillRect(0, 0, w, h);

        // Grid lines
        ctx.strokeStyle = this.gridColor;
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(0, mid); ctx.lineTo(w, mid);
        for (let y = mid; y > 0; y -= h / 8) {
            ctx.moveTo(0, y); ctx.lineTo(w, y);
        }
        for (let y = mid; y < h; y += h / 8) {
            ctx.moveTo(0, y); ctx.lineTo(w, y);
        }
        ctx.stroke();

        // Waveform
        ctx.strokeStyle = this.waveColor;
        ctx.lineWidth = 2;
        ctx.beginPath();

        const samplesPerPixel = this.bufferSize / w;
        for (let x = 0; x < w; x++) {
            const idx = Math.floor(
                (this.writeIndex + Math.floor(x * samplesPerPixel)) % this.bufferSize
            );
            const val = this.buffer[idx];
            const y = mid - val * mid * 0.9;
            if (x === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();
    }

    /** Draw frequency spectrum bars (simple DFT magnitude). */
    _drawSpectrum() {
        const ctx = this.specCtx;
        const w = this.specCanvas.width;
        const h = this.specCanvas.height;

        ctx.fillStyle = this.bgColor;
        ctx.fillRect(0, 0, w, h);

        // Compute magnitudes using a basic DFT on the last fftSize samples
        const N = this.fftSize;
        const magnitudes = new Float32Array(N / 2);
        let maxMag = 0.001;

        for (let k = 0; k < N / 2; k++) {
            let re = 0, im = 0;
            for (let n = 0; n < N; n++) {
                const idx = (this.writeIndex - N + n + this.bufferSize) % this.bufferSize;
                const angle = (2 * Math.PI * k * n) / N;
                re += this.buffer[idx] * Math.cos(angle);
                im -= this.buffer[idx] * Math.sin(angle);
            }
            magnitudes[k] = Math.sqrt(re * re + im * im) / N;
            if (magnitudes[k] > maxMag) maxMag = magnitudes[k];
        }

        // Draw bars
        const barCount = N / 2;
        const barWidth = Math.max(1, w / barCount - 1);
        const gap = 1;

        for (let i = 0; i < barCount; i++) {
            const normHeight = (magnitudes[i] / maxMag) * h * 0.9;
            const x = i * (barWidth + gap);
            const barH = Math.max(1, normHeight);

            // Gradient from green to blue-ish
            const ratio = i / barCount;
            const r = Math.floor(62 + ratio * 46);
            const g = Math.floor(207 - ratio * 100);
            const b = Math.floor(142 + ratio * 110);
            ctx.fillStyle = `rgb(${r},${g},${b})`;
            ctx.fillRect(x, h - barH, barWidth, barH);
        }
    }

    /** Update the level meter bar. */
    _updateLevel() {
        // RMS of the last 256 samples
        let sumSq = 0;
        const N = 256;
        for (let i = 0; i < N; i++) {
            const idx = (this.writeIndex - N + i + this.bufferSize) % this.bufferSize;
            sumSq += this.buffer[idx] * this.buffer[idx];
        }
        const rms = Math.sqrt(sumSq / N);
        const pct = Math.min(100, rms * 400);  // scale for visibility
        this.levelFill.style.width = pct + '%';
    }

    /** Resize canvases to match CSS size (retina aware). */
    _resizeCanvases() {
        for (const canvas of [this.waveCanvas, this.specCanvas]) {
            const rect = canvas.getBoundingClientRect();
            const dpr = window.devicePixelRatio || 1;
            canvas.width  = rect.width * dpr;
            canvas.height = rect.height * dpr;
            const ctx = canvas.getContext('2d');
            ctx.scale(dpr, dpr);
            // Reset logical drawing size
            canvas.style.width  = rect.width + 'px';
            canvas.style.height = rect.height + 'px';
        }
    }
}

window.WaveVisualizer = WaveVisualizer;
