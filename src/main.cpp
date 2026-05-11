#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>
#include <RtAudio.h>
#include <RtMidi.h>

// --- CONSTANTES ---
const unsigned int SAMPLE_RATE = 44100;
unsigned int BUFFER_SIZE = 256;
const double CONTROL_RATE_HZ = 40.0;
const auto CONTROL_PERIOD = std::chrono::milliseconds(static_cast<long long>(1000/CONTROL_RATE_HZ));


// --- Estructura de una voz ---
typedef struct{
    std::atomic<float> phase_acc;
    std::atomic<float> phase_inc;

    std::atomic<uint8_t> note;
    std::atomic<bool> gate;

} higgs_voice;

// --- Variables ---
higgs_voice voices[4];

float volume = 1.f;


// --- CALLBACKs MIDI ---
void mapleCallback(double deltatime, std::vector<unsigned char> *message, void *userData) {
    if (message->size() < 3) return;

    unsigned char status = message->at(0) & 0xF0;

    if (status == 0x90) { // Note On
	voices[0].note = message->at(1);
        float freq = 440.0f * pow(2.0f, (message->at(1) - 69.0f) / 12.0f); // midi -> freq
        voices[0].phase_inc = freq / (float)SAMPLE_RATE;
        voices[0].gate = true;
    } else if (status == 0x80) { // Note Off
	if(voices[0].note == message->at(1))
            voices[0].gate = false;
    }
}

void nanoCallback(double deltatime, std::vector<unsigned char> *message, void *userData) {
    if (message->size() < 3) return;
    if ((message->at(0) & 0xF0) == 0xB0) {
        if (message->at(1) == 2) volume = message->at(2) / 127.f; // Ejemplo: volumen
    }
}

// --- HILO DE AUDIO (Prioridad Crítica) ---
int audioCallback(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
                 double streamTime, RtAudioStreamStatus status, void *userData) {

    float *buffer = (float *)outputBuffer;

    // Carga de datos
    float p = voices[0].phase_acc.load();
    float inc = voices[0].phase_inc.load();
    bool gate = voices[0].gate.load();

    // Osciladores (phase acc)
    for (unsigned int i = 0; i < nBufferFrames; i++) {
        float sample = 0;

	if(gate)
	    sample += p;

	sample *= volume;

        *buffer++ = sample; // L
        *buffer++ = sample; // R

        // Incrementos de fase
	p += inc;
	if(p >= .5f) p -= 1.f;
    }


    voices[0].phase_acc.store(p);

    return 0;
}

// --- HILO DE CONTROL (40 Hz) ---
void controlLoop() {
    while (true) {
        auto start = std::chrono::steady_clock::now();




        std::this_thread::sleep_until(start + CONTROL_PERIOD);
    }
}


// --- Búsqueda de dispositivos MIDI ---
bool openMidiDevice(RtMidiIn *midiIn, std::string targetName, std::string label) {
    unsigned int nPorts = midiIn->getPortCount();
    for (unsigned int i = 0; i < nPorts; i++) {
        std::string portName = midiIn->getPortName(i);
        if (portName.find(targetName) != std::string::npos) {
            midiIn->openPort(i);
            std::cout << "[MIDI] " << label << " conectado: " << portName << std::endl;
            return true;
        }
    }
    std::cout << "[WARNING] No se encontró: " << targetName << std::endl;
    return false;
}


// --- MAIN ---
int main(){
    RtAudio dac;
    RtMidiIn *midiMaple = nullptr;
    RtMidiIn *midiNano = nullptr;

    try {
        midiMaple = new RtMidiIn();
        midiNano = new RtMidiIn();

        // 1. Conectar dispositivos MIDI
        openMidiDevice(midiMaple, "Maple", "Teclado");
        openMidiDevice(midiNano, "nanoKONTROL", "Controlador");

        midiMaple->setCallback(&mapleCallback);
        midiNano->setCallback(&nanoCallback);

        // Ignorar Sysex, Timing y Active Sensing para evitar overhead
        midiMaple->ignoreTypes(true, true, true);
        midiNano->ignoreTypes(true, true, true);

        // 2. Configurar salida de audio
        RtAudio::StreamParameters parameters;
        parameters.deviceId = dac.getDefaultOutputDevice();
        parameters.nChannels = 2;
        parameters.firstChannel = 0;

        dac.openStream(&parameters, NULL, RTAUDIO_FLOAT32, SAMPLE_RATE, &BUFFER_SIZE, &audioCallback);
        dac.startStream();

        std::cout << "\n--- HIGGS SYNTHESIS ENGINE ONLINE ---" << std::endl;
        std::cout << "Buffer size: " << BUFFER_SIZE << " samples" << std::endl;
        std::cout << "Presioná ENTER para apagar el motor." << std::endl;

        // 3. Lanzar hilo de control (40Hz)
        std::thread controlThread(controlLoop);
        controlThread.detach();

        std::cin.get();

        // 4. Cleanup
        if (dac.isStreamOpen()) dac.closeStream();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
	return 1;
    }

    delete midiMaple;
    delete midiNano;
    return 0;
}
